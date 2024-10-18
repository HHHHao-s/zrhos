#include "platform.h"
#include "lock.h"
#include "proc.h"

// lm lock (spinlock, condition variable) module
int holding(lm_lock_t *lk){
    return lk->locked && lk->cpu == cpuid();
}

// push_off/pop_off are like intr_off()/intr_on() except that they are matched:
// it takes two pop_off()s to undo two push_off()s.  Also, if interrupts
// are initially off, then push_off, pop_off leaves them off.

void
push_off(void)
{
  int old = intr_get();

  intr_off();
  if(mycpu()->noff == 0)
    mycpu()->intena = old;
  mycpu()->noff += 1;
}

void
pop_off(void)
{
  struct cpu *c = mycpu();
  if(intr_get())
    panic("pop_off - interruptible");
  if(c->noff < 1)
    panic("pop_off");
  c->noff -= 1;
  if(c->noff == 0 && c->intena)
    intr_on();
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
    push_off();
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



    // Tell the C compiler and the CPU to not move loads or stores
    // past this point, to ensure that all the stores in the critical
    // section are visible to other CPUs before the lock is released,
    // and that loads in the critical section occur strictly before
    // the lock is released.
    // On RISC-V, this emits a fence instruction.
    lk->cpu = -1;

    __sync_synchronize();

    

    // Release the lock, equivalent to lk->locked = 0.
    // This code doesn't use a C assignment, since the C standard
    // implies that an assignment might be implemented with
    // multiple store instructions.
    // On RISC-V, sync_lock_release turns into an atomic swap:
    //   s1 = &lk->locked
    //   amoswap.w zero, zero, (s1)
    __sync_lock_release(&lk->locked);

        pop_off();
}

void lm_sleeplockinit(lm_sleeplock_t *lk, char *name){
    lm_lockinit(&lk->lk, name);
    lk->locked = 0;
}

void lm_sleeplock(lm_sleeplock_t *lk){
    lm_lock(&lk->lk);
    task_t *t = mytask();
    if(lk->locked == 1 && lk->pid == t->id)
        panic("sleeplock");
    while(lk->locked){
        sleep(lk, &lk->lk);
    }
    lk->locked = 1;
    lk->pid = t->id;
    lm_unlock(&lk->lk);
}

void lm_sleepunlock(lm_sleeplock_t *lk){
    lm_lock(&lk->lk);
    task_t *t = mytask();
    if(lk->locked == 0 || lk->pid != t->id)
        panic("unlock");
    lk->locked = 0;
    lk->pid = -1;
    wakeup(lk);
    lm_unlock(&lk->lk);
}

int lm_holdingsleep(lm_sleeplock_t *lk){
    int r;
    lm_lock(&lk->lk);
    r = lk->locked && lk->pid == mytask()->id;
    lm_unlock(&lk->lk);
    return r;
}