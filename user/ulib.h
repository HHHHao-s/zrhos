#include <stdarg.h>
#include "kernel/types.h"
char* strcpy(char *s, const char *t);
int strcmp(const char *p, const char *q);
uint64_t strlen(const char *s);
void* memset(void *dst, int c, uint64_t n);
char* strchr(const char *s, char c);
char* gets(char *buf, int max);
int atoi(const char *s);
void* memmove(void *vdst, const void *vsrc, int n);
int memcmp(const void *s1, const void *s2, uint64_t n);
void* memcpy(void *dst, const void *src, uint64_t n);
void vprintf(int fd, const char *fmt, va_list ap);
void fprintf(int fd, const char *fmt, ...);
void printf(const char *fmt, ...);