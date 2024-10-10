#include "platform.h"


// entry.S needs one stack per CPU. 
__attribute__ ((aligned (16))) char stack0[4096 * NCPU];



void start(){

    uint64 id= r_mhartid();
    // cpu id写到tp寄存器，cpuid()函数会用到
    w_tp(id);

    main();

}