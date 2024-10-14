#include "platform.h"


// entry.S needs one stack per CPU. 
__attribute__ ((aligned (16))) char stack0[4096 * NCPU];

void mvec(){

    panic("machine mode trap");


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

    uint64_t id= r_mhartid();
    // cpu id写到tp寄存器，cpuid()函数会用到
    w_tp(id);


    // return to supervisor mode and jump to main()
    asm volatile("mret");
}