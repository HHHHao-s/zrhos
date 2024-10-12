#include "platform.h"
#include "lock.h"

// lm lock (spinlock, condition variable) module
static int holding(lm_lock_t *lk){
    return lk->locked && lk->cpu == cpuid();
}


void lm_init(){
    printf("lm_init\n");
}

void lm_lockinit(lm_lock_t *lock, char *name){
    lock->locked = 0;
    lock->cpu = -1;
    strcpy(lock->name, name);
}

void lm_lock(lm_lock_t *lk){

    if(holding(lk))
        panic("holding lock");

    // On RISC-V, sync_lock_test_and_set turns into an atomic swap:
    //   a5 = 1
    //   s1 = &lk->locked
    //   amoswap.w.aq a5, a5, (s1)
    while(__sync_lock_test_and_set(&lk->locked, 1) != 0)
        ;

    // Tell the C compiler and the processor to not move loads or stores
    // past this point, to ensure that the critical section's memory
    // references happen strictly after the lock is acquired.
    // On RISC-V, this emits a fence instruction.
    __sync_synchronize();

    // Record info about lock acquisition for holding() and debugging.
    lk->cpu = cpuid();
}

void lm_unlock(lm_lock_t *lk){
    if(!holding(lk))
        panic("unlock");

    lk->cpu = -1;

    // Tell the C compiler and the CPU to not move loads or stores
    // past this point, to ensure that all the stores in the critical
    // section are visible to other CPUs before the lock is released,
    // and that loads in the critical section occur strictly before
    // the lock is released.
    // On RISC-V, this emits a fence instruction.
    __sync_synchronize();

    // Release the lock, equivalent to lk->locked = 0.
    // This code doesn't use a C assignment, since the C standard
    // implies that an assignment might be implemented with
    // multiple store instructions.
    // On RISC-V, sync_lock_release turns into an atomic swap:
    //   s1 = &lk->locked
    //   amoswap.w zero, zero, (s1)
    __sync_lock_release(&lk->locked);
}


