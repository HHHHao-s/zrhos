#include "syscall.h"
#include "platform.h"
#include "proc.h"
#include "defs.h"



int sys_putc(){
    task_t *t = mytask();
    char c = t->trapframe->a0;
    uart_putc(c);
    return 0;
}



static int (*syscalls[])(void)={
    [SYS_putc] = sys_putc,
    [SYS_exit] = sys_exit,
    [SYS_fork] = sys_fork,
    [SYS_wait] = sys_wait,
    [SYS_mmap] = sys_mmap,
    [SYS_munmap] = sys_munmap,
    [SYS_write] = sys_write,
    [SYS_read] = sys_read,
    [SYS_getpid] = sys_getpid,
    [SYS_kill] = sys_kill,
    [SYS_exec] = sys_exec,
};

void syscall(){
    // printf("syscall\n");
    task_t *t = mytask();
    if(t == 0)
        return;

    int num = t->trapframe->a7;
    if(num > 0 && num < (sizeof(syscalls)/sizeof(syscalls[0])) && syscalls[num]){
        syscalls[num]();
        // printf("syscall %d return %d\n", num, ret);
    }else{
        printf("unknown sys call %d\n", num);
    }
}