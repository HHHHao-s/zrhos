#include "kernel/syscall.h"


.global exit
exit:
    li a7, SYS_exit
    ecall
    ret

.global fork
fork:
    li a7, SYS_fork
    ecall
    ret

.global wait
wait:
    li a7, SYS_wait
    ecall
    ret

.global mmap
mmap:
    li a7, SYS_mmap
    ecall
    ret

.global munmap
munmap:
    li a7, SYS_munmap
    ecall
    ret

.global write
write:
    li a7, SYS_write
    ecall
    ret

.global read
read:
    li a7, SYS_read
    ecall
    ret

.global getpid
getpid:
    li a7, SYS_getpid
    ecall
    ret

.global kill
kill:
    li a7, SYS_kill
    ecall
    ret

.global exec
exec:
    li a7, SYS_exec
    ecall
    ret

.global open
open:
    li a7, SYS_open
    ecall
    ret

.global chdir
chdir:
    li a7, SYS_chdir
    ecall
    ret

.global ioctl
ioctl:
    li a7, SYS_ioctl
    ecall
    ret

.global close
close:
    li a7, SYS_close
    ecall
    ret