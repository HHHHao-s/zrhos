#include "platform.h"
#include "lock.h"
#include "proc.h"
#include "defs.h"
#include "memlayout.h"
lm_lock_t lk;

volatile static uint64_t sum=0;

volatile static int started=0;

volatile static int running = 0;

lm_sleeplock_t slk;

void test(void* arg){

    for(int i=0;i<10000;i++){
        lm_sleeplock(&slk);
        sum++;
        lm_sleepunlock(&slk);
    }
    
    lm_lock(&lk);
    running--;
    lm_unlock(&lk);

    if(running == 0){
        printf("sum=%d\n", sum);
    }

}

int main(){

    // now in supervisor mode
    
    if(intr_get()){
        panic("intr_get");
    }

    if(cpuid() == 0){
        uart_init();
        lm_init();
        mem_init();
        task_init();
        plic_init();
        plic_inithart();
        trap_init();
        trap_inithart();
        kvm_init();
        
        started = 1;
        __sync_synchronize();

        kvm_inithart();

        user_init();
        
    }else{
        while(started == 0);
        __sync_synchronize();
        plic_inithart();
        trap_inithart();
        kvm_inithart();

    }
    

    scheduler();


}

