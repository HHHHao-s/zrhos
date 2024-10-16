#include "platform.h"


// entry.S needs one stack per CPU. 
__attribute__ ((aligned (16))) char stack0[4096 * NCPU];

extern char timervec[];

// a scratch area per CPU for machine-mode timer interrupts.
uint64_t timer_scratch[NCPU][5];

void mvec(){
    panic("machine mode trap");
}

void timer_init(){

    // each CPU has a separate source of timer interrupts.
  int id = r_mhartid();

  // ask the CLINT for a timer interrupt.
  int interval = 1000000; // cycles; about 1/10th second in qemu.
  *(uint64_t*)CLINT_MTIMECMP(id) = *(uint64_t*)CLINT_MTIME + interval;

  // prepare information in scratch[] for timervec.
  // scratch[0..2] : space for timervec to save registers.
  // scratch[3] : address of CLINT MTIMECMP register.
  // scratch[4] : desired interval (in cycles) between timer interrupts.
  uint64_t *scratch = &timer_scratch[id][0];
  scratch[3] = CLINT_MTIMECMP(id);
  scratch[4] = interval;
  w_mscratch((uint64_t)scratch);

  // set the machine-mode trap handler.
  w_mtvec((uint64_t)timervec);

  // enable machine-mode interrupts.
  w_mstatus(r_mstatus() | MSTATUS_MIE);

  // enable machine-mode timer interrupts.
  w_mie(r_mie() | MIE_MTIE);

}



void start(){

    // set M Previous Privilege mode to Supervisor, for mret.
    uint64_t x = r_mstatus();
    x&= ~MSTATUS_MPP_MASK;
    x|= MSTATUS_MPP_S;
    w_mstatus(x);

    // write main to mepc register
    w_mepc((uint64_t)main);

    // write 0 to satp register to disable paging
    w_satp(0);

    // write 0xffff to medeleg and mideleg to delegate all interrupts and exceptions to supervisor mode
    w_medeleg(0xffff);
    w_mideleg(0xffff);

    w_sie(r_sie() | SIE_SEIE | SIE_STIE | SIE_SSIE);

    // configure Physical Memory Protection to give supervisor mode
    // access to all of physical memory.
    w_pmpaddr0(0x3fffffffffffffull);
    w_pmpcfg0(0xf);

    timer_init();

    uint64_t id= r_mhartid();
    // cpu id写到tp寄存器，cpuid()函数会用到
    w_tp(id);


    // return to supervisor mode and jump to main()
    asm volatile("mret");
}