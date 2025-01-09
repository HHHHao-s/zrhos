#include "platform.h"

// number of elements in fixed-size array
#define NELEM(x) (sizeof(x)/sizeof((x)[0]))

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
void exit(int status);
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
int sys_wait();
void procdump();
int either_copyout(int user_dst, uint64_t dst, void *src, uint64_t len);
int either_copyin(void *dst, int user_src, uint64_t src, uint64_t len);
int sys_getpid();
int sys_kill();
int killed();
int getpid();

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
int check_lock(lm_lock_t *lk);


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
int copyinstr(pagetable_t pagetable, char *dst, uint64_t srcva, uint64_t max_len);
int fetchaddr(pagetable_t pagetable, uint64_t va, uint64_t *slot);
int fetchstr(pagetable_t pagetable, uint64_t srcva, char *dst, int max);

// ------------------- syscall.c -------------------

void syscall(void);

// ------------------- mmap.c ----------------------

typedef struct MmapNode MmapNode_t;
typedef struct Mmap Mmap_t;
uint64_t mmap(task_t *t, uint64_t addr, uint64_t sz, uint64_t perm, uint64_t flag, int alloc_phy);
int sys_mmap();
void mmap_destroy(Mmap_t *obj);
void handle_pagefault();
int uvm_mappages(pagetable_t pagetable, uint64_t va, uint64_t sz, int perm);
void handle_accessfault();
void copy_mmap(task_t *t, task_t *nt);
void mmap_init();
int sys_munmap();
uint64_t mmap_by_obj(Mmap_t *obj, pagetable_t pagetable, uint64_t addr, uint64_t sz, uint64_t perm, uint64_t flag, int alloc_phy);
Mmap_t *mmap_create(task_t *t);

// ------------------- file.c -------------------
struct file;

struct file *filealloc(void);
void fileclose(struct file* f);
struct file * filedup(struct file *f);

// ------------------- console.c -------------------

void console_init();
void console_intr(int c);
// extern struct file *console_file;

// ------------------- bio.c -------------------
struct buf;
void binit(void);
struct buf* bread(uint_t dev, uint_t blockno);
void bwrite(struct buf *b);
void brelse(struct buf *b);
void bpin(struct buf *b);
void bunpin(struct buf *b);

// ------------------- virtio_disk.c -------------------

void virtio_disk_init(void);
void virtio_disk_rw(struct buf *b, int write);
void virtio_disk_intr();


// ------------------- fs.c -------------------
typedef struct inode inode_t;
void fs_init(int dev);
uint_t balloc();
inode_t *ialloc(uint_t dev,uint_t type);
int iupdate(inode_t *ip);
void bfree(uint_t dev, uint_t b);
int itrunc(inode_t *ip);
int ilock(inode_t *ip);
int iunlock(inode_t *ip);
int iput(inode_t *ip);
int iunlockput(inode_t *ip);
uint_t bmap(inode_t *ip, uint_t start);
int writei(inode_t *ip, int user_src, uint64_t src, uint_t off, uint_t n);
int readi(inode_t *ip, int user_dst, uint64_t dst, uint_t off, uint_t n);
inode_t *idup(inode_t *ip);
int namecmp(const char *s, const char *t);
struct inode* dirlookup(struct inode *dp, char *name, uint_t *poff);
struct inode* namei(char *path);
struct inode* nameiparent(char *path, char *name);
int dirlink(inode_t *dip, char *name, int inum);
void begin_op();
void end_op();

// ------------------- exec.c -------------------

int
exec(char *path, char **argv);


// ------------------- sysfile.c -------------------

int sys_exec();
int sys_write();
int sys_read();
int sys_open();
int sys_chdir();