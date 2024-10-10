#include "platform.h"

lm_lock_t lk;

uint64_t sum;

int main(){
    if(cpuid() == 0){
        uart_init();
        lm_init();
        mem_init();
    }else{
        ;
    }

}

