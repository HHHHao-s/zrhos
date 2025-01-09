#include <stdint.h>
int exit(int);
int fork();
int wait(int*);
uint64_t mmap(uint64_t addr, uint64_t sz, uint64_t perm, uint64_t flag);
int munmap(uint64_t addr, uint64_t sz);
uint64_t write(int fd, char*, uint64_t);
uint64_t read(int fd, char*, uint64_t);
int getpid();
int kill(int);
int exec(const char *pathname, char *const argv[]);
int open(const char*pathname, uint64_t mode);
int chdir(const char*pathname);