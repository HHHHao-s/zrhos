#include "platform.h"
#include "lock.h"
#include "proc.h"
lm_lock_t lk;

volatile static uint64_t sum=0;

volatile static int started=0;



void test(void* arg){

    for(int i=0;i<100000;i++){
        lm_lock(&lk);
        sum++;
        lm_unlock(&lk);
        yield();
    }
    task_t* t = mycpu()->current;
    if(t->id == 0){
        
        for(int i=0;i<10000000;i++){
            ;
        }
        __sync_synchronize();
        printf("sum = %d\n",sum);
    }
    exit();
}

int main(){

    // now is supervisor mode
    
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
    }else{
        while(started == 0);
        __sync_synchronize();
        plic_inithart();
        trap_inithart();

    }

    scheduler();


}

