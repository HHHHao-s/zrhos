#include "kernel/syscall.h"


.global main
.section .text
.global _start
_start:

    call main
    li a7 , SYS_exit
    ecall
