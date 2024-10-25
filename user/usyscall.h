#include <stdint.h>
int putc(char);
int exit(int);
int fork();
int wait(int*);
uint64_t mmap(uint64_t addr, uint64_t sz, uint64_t perm, uint64_t flag);