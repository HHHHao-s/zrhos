#include "platform.h"
#include "proc.h"
#include "defs.h"
#include "memlayout.h"
#include "types.h"

// initcode is the first user program run in user mode
// it does't exist before compile
#include "initcode.inc"

extern char trampoline[], uservec[], userret[];

cpu_t cpus[NCPU];
task_t tasks[NTASK];

inline int cpuid(){
  return r_tp();
}

inline cpu_t *mycpu(void)
{
  return  &cpus[cpuid()];
}

task_t *mytask(void)
{
  push_off();
  task_t *t= mycpu()->current;
  pop_off();
  return t;
}

static void init_context(task_t *t, void (*entry)(void)){
  context_t *c = &t->context;
  memset(c, 0, sizeof(*c));
  c->ra = (uint64_t)entry;
  c->sp = (uint64_t)(t->kstack + PGSIZE);
}

// return to user space
void ret_entry(){
  // jump from swtch
  task_t *t = mycpu()->current;
  lm_unlock(&t->lock);

  usertrapret();

}


void user_init(){

  task_t *t = utask_create();

  // load initcode into memory
  // initcode will begin at address 0 where _start is
  uint64_t high = PGROUNDUP(user_initcode_len);
  for(uint64_t i = 0; i < high; i += PGSIZE){
    uvm_map(t->pagetable, i, (uint64_t)mem_malloc(PGSIZE), PGSIZE,  PTE_R | PTE_W | PTE_X, 0);
  }

  // copy initcode to memory
  copyout(t->pagetable, 0,(char *) user_initcode, user_initcode_len);

  // set the program counter to 0
  t->trapframe->epc = 0;

  // map a stack for user
  uvm_map(t->pagetable, high + PGSIZE, (uint64_t)mem_malloc(PGSIZE), PGSIZE, PTE_R | PTE_W, 0);

  t->trapframe->sp = high + PGSIZE*2;// top of the stack

  t->state = RUNNABLE; 

  lm_unlock(&t->lock);

}



int alloc_pid(){
  static lm_lock_t pid_lock;
  static int nextpid = 1;
  int pid;
  lm_lock(&pid_lock);
  pid = nextpid++;
  lm_unlock(&pid_lock);
  return pid;
}

pagetable_t user_pagetable(task_t *t){
  pagetable_t pagetable = vm_create();

  if(pagetable == 0)
    return 0;

  // map trampoline code
  // kernel use it to return to user space or handle trap
  if(vm_map(pagetable, TRAMPOLINE, (uint64_t)trampoline, PGSIZE, PTE_R | PTE_X, 0) < 0){
    mem_free(pagetable);
    return 0;
  }

  // map trapframe page
  // kernel use it to store trapframe
  if(vm_map(pagetable, TRAPFRAME, (uint64_t)t->trapframe, PGSIZE, PTE_R | PTE_W, 0) < 0){
    vm_unmap(pagetable, TRAMPOLINE, 1, 0);
    mem_free(pagetable);
    return 0;
  }

  return pagetable;
}

// will return a task pointer, with state set to USED and holding the lock
// it will atomically alloc a pid and page table and trapframe
// set the entry to ret_entry
task_t * utask_create(){
  task_t *t = 0;
  for(t = tasks; t < &tasks[NTASK]; t++){
    lm_lock(&t->lock);
    if(t->state == DEAD){
      t->id = alloc_pid();
      t->state = USED;

      init_context(t, ret_entry);
      break;
    }
    lm_unlock(&t->lock);
  }
  if(t == &tasks[NTASK])
    return 0;
  
  
  t->trapframe = (trapframe_t*)mem_malloc(PGSIZE);

  memset(t->trapframe, 0, sizeof(PGSIZE));

  t->pagetable = user_pagetable(t);

  lm_sem_init(&t->sons_sem, 0);

  return t;



}



void scheduler(void)
{
  task_t *t;
  cpu_t *c = mycpu();
  
  c->current = 0;
  for(;;){
    // Avoid deadlock by ensuring that devices can interrupt.
    intr_on();
    // printf("scheduler\n");

    for(t = tasks; t < &tasks[NTASK]; t++) {
      lm_lock(&t->lock);
      if( t->id !=-1 && t->state == RUNNABLE) {
        // Switch to chosen process.  It is the process's job
        // to release its lock and then reacquire it
        // before jumping back to us.
        t->state = RUNNING;
        c->current = t;
        swtch(&c->context, &t->context);

        // Process is done running for now.
        // It should have changed its t->state before coming back.
        c->current = 0;
      }
      lm_unlock(&t->lock);
    }
  }
}

// init every task before using
void task_init(){
  for(int i=0;i<NTASK;i++){
    tasks[i].id = -1;
    tasks[i].state = DEAD;
    tasks[i].kstack = (uintptr_t)mem_malloc(PGSIZE);
    lm_lockinit(&tasks[i].lock,"task");
  }
  for(int i=0;i<NCPU;i++){
    cpus[i].intena = 0;
    cpus[i].noff = 0;
  }
}



static void real_entry(){
  // jump from swtch
  task_t *t = mycpu()->current;
  lm_unlock(&t->lock);
  intr_on();
  t->entry(t->arg);
  exit(0);
}

// create a new task running in kernel, return the task pointer, it will run automatically
task_t* task_create(void (*entry)(void*), void *arg){
  task_t *t;
  for(t = tasks; t < &tasks[NTASK]; t++){
    lm_lock(&t->lock);
    if(t->id == -1){
      t->id = t - tasks;
      t->entry = entry;
      t->arg = arg;
      t->state = RUNNABLE;
      init_context(t, real_entry);
      lm_unlock(&t->lock);
      return t;
    }
    lm_unlock(&t->lock);
  }
  return 0;
}

// Switch to scheduler.  Must hold only p->lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->noff, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  task_t *t = mytask();

  if(!holding(&t->lock))
    panic("sched p->lock");
  if(mycpu()->noff != 1)
    panic("sched locks");
  if(t->state == RUNNING)
    panic("sched running");
  if(intr_get())
    panic("sched interruptible");

  intena = mycpu()->intena;
  swtch(&t->context, &mycpu()->context);
  mycpu()->intena = intena;
}

void yield(){
  task_t *t = mycpu()->current;
  lm_lock(&t->lock);
  t->state = RUNNABLE;
  sched();
  lm_unlock(&t->lock);
}

void free_task(task_t *t){
  if(t->pagetable)
    free_pagetable(t->pagetable, 1);
  t->pagetable = 0;
  if(t->trapframe)
    mem_free(t->trapframe);
  t->trapframe = 0;
  t->id = -1;
  t->state = DEAD;
  t->parent = 0;
}

void reparent(task_t *t){
  for(task_t *p = tasks; p < &tasks[NTASK]; p++){
    if(t==p)
      continue;
    lm_lock(&p->lock);
    if(p->parent == t){
      p->parent = &tasks[0];
      lm_V(&tasks[0].sons_sem);
      lm_unlock(&p->lock);
    }else{
      lm_unlock(&p->lock);
    }
  }
}

void exit(int status){
  task_t *t = mycpu()->current;
  lm_lock(&t->lock);
  t->state = ZOMBIE;
  t->xstatus = status;
  // inform the parent
  if(t->parent){
    lm_V(&t->parent->sons_sem);
  }
  reparent(t);

  swtch(&t->context, &mycpu()->context);
  // won't return
}

int sys_wait(){
  
  task_t *t = mycpu()->current;
  int *status = (int*)t->trapframe->a0;
  lm_P(&t->sons_sem);
  // find the zombie son
  for(task_t *p = tasks; p < &tasks[NTASK]; p++){
    lm_lock(&p->lock);
    if(p->parent == t && p->state == ZOMBIE){
      lm_unlock(&p->lock);
      copyout(t->pagetable, (uint64_t)status, (char *)&t->xstatus, sizeof(t->xstatus));
      t->trapframe->a0 = p->id;
      free_task(p);
      return 0;
    }
    lm_unlock(&p->lock);
  }
  panic("sys_wait: no zombie son");
  return 0;
  
}

void force_exit(task_t *t){
  lm_lock(&t->lock);
  t->state = KILLED;
  if(mycpu()->current == t){
    swtch(&t->context, &mycpu()->context);
    // won't return
  }else{
    lm_unlock(&t->lock);
  }
}

void handle_timer_interrupt(){
  task_t *t = mycpu()->current;
  if(t != NULL && t->state == RUNNING){
    yield();
  }
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, lm_lock_t *lk)
{
  task_t* t = mytask();
  
  // Must acquire p->lock in order to
  // change p->state and then call sched.
  // Once we hold p->lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup locks p->lock),
  // so it's okay to release lk.

  lm_lock(&t->lock);  //DOC: sleeplock1
  lm_unlock(lk);

  // Go to sleep.
  t->chan = chan;
  t->state = SLEEPING;

  sched();

  // Tidy up.
  t->chan = 0;

  // Reacquire original lock.
  lm_unlock(&t->lock);
  lm_lock(lk);
}

// Wake up all processes sleeping on chan.
// Must be called without any p->lock.
void
wakeup(void *chan)
{
  task_t *t;
  task_t *myt = mytask();
  for(t = tasks; t < &tasks[NTASK]; t++) {
    if(t != myt){
      lm_lock(&t->lock);
      if(t->state == SLEEPING && t->chan == chan) {
        t->state = RUNNABLE;
      }
      lm_unlock(&t->lock);
    }
  }
}

int sys_exit(){
    task_t *t = mytask();
    exit(t->trapframe->a0);
    return 0;
}

int sys_fork(){
  task_t *nt = utask_create();

  task_t *t = mytask();

  // copy the user memory
  // don't cover TRAMPOLINE and TRAPFRAME
  copy_pagetable(t->pagetable, nt->pagetable, 0);

  // copy user trapframe
  memmove(nt->trapframe, t->trapframe, sizeof(trapframe_t));


  // set the return value of the child to 0
  nt->trapframe->a0 = 0;

  // set the return value of the parent to the pid of the child
  t->trapframe->a0 = nt->id;

  // set the state of the child to RUNNABLE
  nt->state = RUNNABLE;

  // set the parent
  nt->parent = t;

  // release the lock of the child
  lm_unlock(&nt->lock);
  return 0;

}