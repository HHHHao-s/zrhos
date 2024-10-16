#include "platform.h"
#include "proc.h"
#include "defs.h"

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

static void init_context(task_t *t, void (*entry)(void)){
  context_t *c = &t->context;
  memset(c, 0, sizeof(*c));
  c->ra = (uint64_t)entry;
  c->sp = (uint64_t)(t->kstack + PGSIZE);
}

static void real_entry(){
  // jump from swtch
  task_t *t = mycpu()->current;
  lm_unlock(&t->lock);
  intr_on();
  t->entry(t->arg);
  exit();
}

// create a new task, return the task pointer, it will run automatically
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

void exit(){
  task_t *t = mycpu()->current;
  lm_lock(&t->lock);
  t->state = DEAD;
  t->id = -1;
  swtch(&t->context, &mycpu()->context);
  // won't return
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