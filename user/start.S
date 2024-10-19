#include "kernel/syscall.h"
.global main
_start:

    call main
    li a7 , SYS_exit
    ecall
