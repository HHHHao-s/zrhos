

// ------------------- uart.c ------------------- 
void uart_init(void);
void uart_putc(char c);
int uart_getc(void);

// ------------------- proc.c -------------------

int cpuid();

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

typedef struct lm_lock lm_lock_t;

void lm_init();
void lm_lockinit(lm_lock_t *lock, char *name);
void lm_lock(lm_lock_t *lk);
void lm_unlock(lm_lock_t *lk);