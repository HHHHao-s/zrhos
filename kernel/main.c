#include "platform.h"
#include "lock.h"
#include "proc.h"
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
        lm_sleeplockinit(&slk, "test");
        started = 1;
        
        __sync_synchronize();
        for(int i=0;i<8;i++){
            task_create(test, 0);
            running++;
        }
    }else{
        while(started == 0);
        __sync_synchronize();
        plic_inithart();
        trap_inithart();

    }
    

    scheduler();


}

