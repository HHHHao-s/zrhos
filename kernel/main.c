#include "platform.h"
#include "lock.h"
#include "proc.h"
lm_lock_t lk;

volatile static uint64_t sum=0;

volatile static int started=0;

int each[8];

void test(void* arg){
    task_t *t = mytask();
    for(int i=0;i<1000000;i++){
        each[t->id]++;
    }
    printf("task %d done each: %d\n",t->id, each[t->id]);

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

        started = 1;
        __sync_synchronize();
        for(int i=0;i<8;i++){
            task_create(test, 0);
        }
    }else{
        while(started == 0);
        __sync_synchronize();
        plic_inithart();
        trap_inithart();

    }
    

    scheduler();


}

