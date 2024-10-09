#include "platform.h"


// entry.S needs one stack per CPU. 
__attribute__ ((aligned (16))) char stack0[4096 * NCPU];



void start(){

    main();

}