#include "kernel/syscall.h"
.global putc
putc:
    li a7, SYS_putc
    ecall
    ret

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