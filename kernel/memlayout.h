// qemu -machine virt is set up like this,
// based on qemu's hw/riscv/virt.c:
//
// 00001000 -- boot ROM, provided by qemu
// 02000000 -- CLINT
// 0C000000 -- PLIC
// 10000000 -- uart0 
// 10001000 -- virtio disk 
// 80000000 -- boot ROM jumps here in machine mode
//             -kernel loads the kernel here
// unused RAM after 80000000.

// the kernel uses physical memory thus:
// 80000000 -- entry.S, then kernel text and data
// end -- start of kernel page allocation area
// PHYSTOP -- end RAM used by the kernel

#define UART0 0x10000000L
#define UART0_IRQ 10

// virtio mmio interface
#define VIRTIO0 0x10001000
#define VIRTIO0_IRQ 1

// virtio mmio interface (for GPU)
#define VIRTIO1 0x10002000
#define VIRTIO1_IRQ 2

// virtio mmio interface (for input)
#define VIRTIO2 0x10003000
#define VIRTIO2_IRQ 3

// core local interruptor (CLINT), which contains the timer.
#define CLINT 0x2000000L
#define CLINT_MTIMECMP(hartid) (CLINT + 0x4000 + 8*(hartid))
#define CLINT_MTIME (CLINT + 0xBFF8) // cycles since boot.

// qemu puts platform-level interrupt controller (PLIC) here.
#define PLIC 0x0c000000L
#define PLIC_PRIORITY (PLIC + 0x0)
#define PLIC_PENDING (PLIC + 0x1000)
#define PLIC_MENABLE(hart) (PLIC + 0x2000 + (hart)*0x100)
#define PLIC_SENABLE(hart) (PLIC + 0x2080 + (hart)*0x100)
#define PLIC_MPRIORITY(hart) (PLIC + 0x200000 + (hart)*0x2000)
#define PLIC_SPRIORITY(hart) (PLIC + 0x201000 + (hart)*0x2000)
#define PLIC_MCLAIM(hart) (PLIC + 0x200004 + (hart)*0x2000)
#define PLIC_SCLAIM(hart) (PLIC + 0x201004 + (hart)*0x2000)

#define PHYSIZE (128*1024*1024 )// 128MB
#define PHYSTATR ( 0x80000000L)
#define PHYSTOP  (PHYSTATR + PHYSIZE)
#define MEMSTOP  (PHYSTOP - 2*PGSIZE)
#define USERRDONLY (PHYSTOP - PGSIZE) // map some kernel data for user read only(e.g. timer ticks)
#define MAXVA (1L << (9 + 9 + 9 + 12 - 1))
#define PGSIZE 4096 // bytes per page
// map the trampoline page to the highest address,
// in both user and kernel space.
#define TRAMPOLINE (MAXVA - PGSIZE)

// User memory layout.
// Address zero first:
//   text
//   original data and bss
//   fixed-size stack
//   anynomous mmap
//   ...
//   USERRDONLY (the same page as in the kernel)
//   TRAPFRAME (p->trapframe, used by the trampoline)
//   TRAMPOLINE (the same page as in the kernel)
#define TRAPFRAME (TRAMPOLINE - PGSIZE)
#define USERRDONLYMAP (TRAPFRAME - PGSIZE)
// anynomous mmap will start from here
// middle of the address space
#define VA_ANYNOMOUS (MAXVA >> 1)

// 2 pages
#define VA_USERSTACK (VA_ANYNOMOUS - 2*PGSIZE)