#include "syscall.h"
#include "platform.h"
#include "proc.h"
#include "defs.h"

uint64_t sys_putc(){
    task_t *t = mytask();
    char c = t->trapframe->a0;
    uart_putc(c);
    return 0;
}

uint64_t sys_exit(){
    exit();
    return 0;
}

static uint64_t (*syscalls[])(void)={
    [SYS_putc] = sys_putc,
    [SYS_exit] = sys_exit,
};

void syscall(){
    printf("syscall\n");
    task_t *t = mytask();
    if(t == 0)
        return;

    int num = t->trapframe->a7;
    if(num > 0 && num < (sizeof(syscalls)/sizeof(syscalls[0])) && syscalls[num]){
        t->trapframe->a0 = syscalls[num]();
    }else{
        printf("unknown sys call %d\n", num);
    }
}