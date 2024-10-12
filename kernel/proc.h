#include <stdint.h>
#include "lock.h"
typedef struct context {
    // Saved registers
    uint64_t ra; // 0 return address
    uint64_t sp; // 8

    // callee-saved
    uint64_t s0; // 16
    uint64_t s1; // 24
    uint64_t s2; // 32
    uint64_t s3; // 40
    uint64_t s4; // 48
    uint64_t s5; // 56
    uint64_t s6; // 64
    uint64_t s7; // 72 
    uint64_t s8; // 80
    uint64_t s9; // 88
    uint64_t s10; // 96
    uint64_t s11; // 104

}context_t;


typedef struct task
{
    int id;
    enum { RUNNING, RUNNABLE, SLEEPING, DEAD } state;
    uintptr_t kstack; // Bottom of kernel stack for this process
    lm_lock_t lock;
    
    context_t context; // Switch here to run process
    void (*entry)(void*); // entry point of this task

    void *arg; // argument passed to entry

}task_t;


typedef struct cpu{

    task_t *current;
    context_t context;

}cpu_t;