#include "platform.h"

// ------------------- uart.c ------------------- 
void uart_init(void);
void uart_putc(char c);
int uart_getc(void);
void uart_intr(void);

// ------------------- proc.c -------------------
typedef struct lm_lock lm_lock_t;
typedef struct cpu cpu_t;
typedef struct task task_t;
typedef struct context context_t;

int cpuid();
cpu_t *mycpu(void);
void scheduler(void);
void task_init();
task_t* task_create(void (*entry)(void*), void *arg);
void yield();
void exit();
void force_exit(task_t *t);
void handle_timer_interrupt();
task_t *mytask(void);
void sleep(void *chan, lm_lock_t *lk);
void wakeup(void *chan);
void user_init();
void ret_entry();
int alloc_pid();
pagetable_t user_pagetable(task_t *t);
task_t * utask_create();
int sys_exit();
int sys_fork();
void sched();


// ------------------- main.c -------------------

int main();

// ------------------- klib.c -------------------

#include <stddef.h>
#include <stdarg.h>

// Function declarations
void panic(const char *s);
void panic_on(int cond, const char *s);
int printf(const char *fmt, ...);
int vsprintf(char *out, const char *fmt, va_list ap);
int sprintf(char *out, const char *fmt, ...);
int snprintf(char *out, size_t n, const char *fmt, ...);
int vsnprintf(char *out, size_t n, const char *fmt, va_list ap);
int rand(void);
void srand(unsigned int seed);
int abs(int x);
char* itoa(char *a, int i);
int atoi(const char* nptr);
size_t strlen(const char *s);
char *strcpy(char *dst, const char *src);
char *strncpy(char *dst, const char *src, size_t n);
char *strcat(char *dst, const char *src);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);
void *memset(void *s, int c, size_t n);
void *memmove(void *dst, const void *src, size_t n);
void *memcpy(void *out, const void *in, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);


// ------------------- mem.c -------------------

void mem_init();
void* mem_malloc(size_t size);
void mem_free(void* ptr);

// ------------------- lock.c -------------------
typedef struct lm_sleeplock lm_sleeplock_t;
typedef struct semophore semophore_t;

void lm_init();
void lm_lockinit(lm_lock_t *lock, char *name);
void lm_lock(lm_lock_t *lk);
void lm_unlock(lm_lock_t *lk);
void push_off(void);
void pop_off(void);
int holding(lm_lock_t *lk);
void lm_sleeplockinit(lm_sleeplock_t *lk, char *name);
void lm_sleeplock(lm_sleeplock_t *lk);
void lm_sleepunlock(lm_sleeplock_t *lk);
int lm_holdingsleep(lm_sleeplock_t *lk);
void lm_V(semophore_t *sem);
void lm_P(semophore_t *sem);
void lm_sem_init(semophore_t *sem, int value);

// ------------------- swtch.S -------------------

// switch to the new context
int swtch(context_t *old , context_t *new);

// ------------------- trap.c -------------------

void trap_init(void);
void trap_inithart(void);
void usertrapret(void);
void usertrap(void);


// ------------------- plic.c -------------------

void plic_init(void);
void plic_inithart(void);
int plic_claim(void);
void plic_complete(int irq);


// ------------------- vm.c -------------------

typedef uint64_t pte_t;
typedef uint64_t *pagetable_t; // 512 PTEs
void kvm_init();
void kvm_inithart();
pagetable_t vm_create();
int vm_map(pagetable_t pagetable, uint64_t va, uint64_t pa, uint64_t sz, int perm, int remap);
pte_t* walk(pagetable_t pagetable, uint64_t va, int alloc);
int copyout(pagetable_t pagetable, uint64_t dstva, char *src, uint64_t len);
int copyin(pagetable_t pagetable, char *dst, uint64_t srcva, uint64_t len);
void vmunmap(pagetable_t pagetable, uint64_t va, uint64_t npages, int do_free);
uint64_t walkaddr(pagetable_t pagetable, uint64_t va);
void vm_unmap(pagetable_t pagetable, uint64_t va, uint64_t npages, int do_free);
int uvm_map(pagetable_t pagetable, uint64_t va, uint64_t pa, uint64_t sz, int perm, int remap);
void free_pagetable(pagetable_t pagetable, int do_free);
void copy_pagetable(pagetable_t from , pagetable_t to, int cover);

// ------------------- syscall.c -------------------

void syscall(void);