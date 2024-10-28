#include "platform.h"
#include "lock.h"
#include "proc.h"
#include "defs.h"
#include "memlayout.h"
lm_lock_t lk;

volatile static uint64_t sum=0;

volatile static int started=0;

volatile static int running = 0;

semophore_t sem;


void test(void*){

    for(int i=0;i<10000;i++){
        lm_P(&sem);
        sum++;
        lm_V(&sem);
    }
    lm_lock(&lk);
    running--;
    if(running==0){
        printf("sum=%d\n", sum);
    }
    lm_unlock(&lk);

}

void test_init(){

    lm_sem_init(&sem, 1);
    lm_lockinit(&lk, "test");
    lm_lock(&lk);
    for(int i=0;i<8;i++){
        task_create(test, 0);
        running++;
    }
    lm_unlock(&lk);
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
        mmap_init();
        
        started = 1;
        __sync_synchronize();

        kvm_inithart();

        user_init();
        // test_init();
        
    }else{
        while(started == 0);
        __sync_synchronize();
        plic_inithart();
        trap_inithart();
        kvm_inithart();

    }
    

    scheduler();


}

