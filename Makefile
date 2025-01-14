K=kernel
U=user

OBJS = \
  $K/entry.o \
  $K/start.o \
  $K/main.o \
  $K/uart.o \
  $K/proc.o \
  $K/klib.o \
  $K/mem.o \
  $K/lock.o \
  $K/swtch.o \
  $K/plic.o \
  $K/trap.o \
  $K/kernelvec.o \
  $K/vm.o \
  $K/trampoline.o \
  $K/syscall.o \
  $K/mmap.o \
  $K/rbtree.o \
  $K/console.o \
  $K/file.o \
  $K/fs.o \
  $K/bio.o \
  $K/virtio_disk.o \
  $K/exec.o \
  $K/sysfile.o \
  $K/virtio_gpu.o \


ifndef TOOLPREFIX
TOOLPREFIX := $(shell if riscv64-unknown-elf-objdump -i 2>&1 | grep 'elf64-big' >/dev/null 2>&1; \
	then echo 'riscv64-unknown-elf-'; \
	elif riscv64-linux-gnu-objdump -i 2>&1 | grep 'elf64-big' >/dev/null 2>&1; \
	then echo 'riscv64-linux-gnu-'; \
	elif riscv64-unknown-linux-gnu-objdump -i 2>&1 | grep 'elf64-big' >/dev/null 2>&1; \
	then echo 'riscv64-unknown-linux-gnu-'; \
	else echo "***" 1>&2; \
	echo "*** Error: Couldn't find a riscv64 version of GCC/binutils." 1>&2; \
	echo "*** To turn off this error, run 'gmake TOOLPREFIX= ...'." 1>&2; \
	echo "***" 1>&2; exit 1; fi)
endif

QEMU = qemu-system-riscv64

CC = $(TOOLPREFIX)gcc
AS = $(TOOLPREFIX)gas
LD = $(TOOLPREFIX)ld
OBJCOPY = $(TOOLPREFIX)objcopy
OBJDUMP = $(TOOLPREFIX)objdump

CFLAGS = -Wall -Werror -O -fno-omit-frame-pointer -ggdb -gdwarf-2


CFLAGS += $(XCFLAGS)
CFLAGS += -MD
CFLAGS += -mcmodel=medany
CFLAGS += -ffreestanding -fno-common -nostdlib -mno-relax
CFLAGS += -I.
CFLAGS += $(shell $(CC) -fno-stack-protector -E -x c /dev/null >/dev/null 2>&1 && echo -fno-stack-protector)


# Disable PIE when possible (for Ubuntu 16.10 toolchain)
ifneq ($(shell $(CC) -dumpspecs 2>/dev/null | grep -e '[^f]no-pie'),)
CFLAGS += -fno-pie -no-pie
endif
ifneq ($(shell $(CC) -dumpspecs 2>/dev/null | grep -e '[^f]nopie'),)
CFLAGS += -fno-pie -nopie
endif

LDFLAGS = -z max-page-size=4096

$K/kernel:  $(OBJS) $K/kernel.ld mkfs/mkfs
	$(LD) $(LDFLAGS) -T $K/kernel.ld -o $K/kernel $(OBJS) $(OBJS_KCSAN)
	$(OBJDUMP) -S $K/kernel > $K/kernel.asm
	$(OBJDUMP) -t $K/kernel | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $K/kernel.sym


$K/%.o: $K/%.c $K/initcode.inc
	$(CC) $(CFLAGS) $(EXTRAFLAG) -c -o $@ $<

UOBJS = \
  $U/ulib.o \
  $U/start.o \
  $U/usyscall.o \
  $U/initcode.o \

ULIB = $U/ulib.o $U/usyscall.o

$K/initcode.inc:  $(UOBJS)
	$(LD) $(LDFLAGS) -T $U/initcode.ld -o $U/initcode $^
	$(OBJDUMP) -S $U/initcode > $U/initcode.asm
	$(OBJCOPY) -S -O binary $U/initcode
	xxd -i $U/initcode  > $@

$U/%.o: $U/%.c
	$(CC) $(CFLAGS) -I. -c -o $@ $<

$U/%.o: $U/%.S
	$(CC) $(CFLAGS) -I. -c -o $@ $<



UPROGS =\
	$U/_test \
	$U/_sh \
	$U/_ls \


mkfs/mkfs: mkfs/mkfs.cpp $(UPROGS)
	g++ -Wall -g -I. -fsanitize=address -o mkfs/mkfs mkfs/mkfs.cpp
	mkfs/mkfs fs.img $(UPROGS)

_%: %.o $(ULIB)
	$(LD) $(LDFLAGS) -T $U/user.ld -o $@ $^
	$(OBJDUMP) -S $@ > $*.asm
	$(OBJDUMP) -t $@ | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $*.sym


clean: 
	rm -f *.tex *.dvi *.idx *.aux *.log *.ind *.ilg \
	*/*.o */*.d */*.asm */*.sym \
	$U/initcode $U/initcode.out $K/kernel fs.img \
	mkfs/mkfs .gdbinit \
        $U/usys.S \
	$(UPROGS) \
	ph barrier \
	$K/initcode.inc \






ifndef CPUS
CPUS := 1
endif

FWDPORT = $(shell expr `id -u` % 5000 + 25999)

QEMUOPTS = -machine virt -bios none -kernel $K/kernel -m 128M -smp $(CPUS) -nographic

# Disable the legacy mode for Virtio-MMIO devices to ensure the use of modern mode.
# Define a disk drive fs.img with a raw format and assign a unique identifier x0.
# Connect a Virtio block device to the Virtio-MMIO bus and connect the disk drive x0 to this device.
QEMUOPTS += -global virtio-mmio.force-legacy=false
QEMUOPTS += -drive file=fs.img,if=none,format=raw,id=x0
QEMUOPTS += -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0

# Add VirtIO GPU device and enable SDL display
QEMUOPTS += -device virtio-gpu-device,bus=virtio-mmio-bus.1
QEMUOPTS += -display sdl,gl=on

# Optional: Set display resolution
QEMUOPTS += -global virtio-gpu-pci.xres=1024 -global virtio-gpu-pci.yres=768

qemu: $K/kernel
	$(QEMU) $(QEMUOPTS)

qemu-gdb: $K/kernel
	@echo "*** Now run 'gdb' in another window." 1>&2
	$(QEMU) $(QEMUOPTS) -S -s

