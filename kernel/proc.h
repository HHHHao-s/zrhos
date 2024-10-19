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

// for sscratch
typedef struct trapframe {
  /*   0 */ uint64_t unused;        // unused (no kernel page table)
  /*   8 */ uint64_t kernel_sp;     // top of process's kernel stack
  /*  16 */ uint64_t kernel_trap;   // usertrap()
  /*  24 */ uint64_t epc;           // saved user program counter
  /*  32 */ uint64_t kernel_hartid; // saved kernel tp
  /*  40 */ uint64_t ra;
  /*  48 */ uint64_t sp;
  /*  56 */ uint64_t gp;
  /*  64 */ uint64_t tp;
  /*  72 */ uint64_t t0;
  /*  80 */ uint64_t t1;
  /*  88 */ uint64_t t2;
  /*  96 */ uint64_t s0;
  /* 104 */ uint64_t s1;
  /* 112 */ uint64_t a0;
  /* 120 */ uint64_t a1;
  /* 128 */ uint64_t a2;
  /* 136 */ uint64_t a3;
  /* 144 */ uint64_t a4;
  /* 152 */ uint64_t a5;
  /* 160 */ uint64_t a6;
  /* 168 */ uint64_t a7;
  /* 176 */ uint64_t s2;
  /* 184 */ uint64_t s3;
  /* 192 */ uint64_t s4;
  /* 200 */ uint64_t s5;
  /* 208 */ uint64_t s6;
  /* 216 */ uint64_t s7;
  /* 224 */ uint64_t s8;
  /* 232 */ uint64_t s9;
  /* 240 */ uint64_t s10;
  /* 248 */ uint64_t s11;
  /* 256 */ uint64_t t3;
  /* 264 */ uint64_t t4;
  /* 272 */ uint64_t t5;
  /* 280 */ uint64_t t6;
}trapframe_t;

typedef struct task
{
    int id;
    enum { RUNNING, RUNNABLE, SLEEPING, DEAD, KILLED } state;
    uintptr_t kstack; // Bottom of kernel stack for this process
    lm_lock_t lock;
    
    context_t context; // Switch here to run process
    void (*entry)(void*); // entry point of this task
    void *arg; // argument passed to entry
    pagetable_t pagetable; // user page table
    trapframe_t trapframe; // data page for uservec.S

    void *chan; // If non-zero, sleeping on chan

}task_t;


typedef struct cpu{

    task_t *current;
    context_t context;
    int noff;
    int intena;


}cpu_t;

