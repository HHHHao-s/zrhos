#include "kernel/mmap.h"
#include "usyscall.h"
int main(){

    putc('a');

    uint64_t private_region = mmap(0, 4096, PERM_R | PERM_W, MAP_ANONYMOUS | MAP_PRIVATE);

    *(char*)private_region = 'a';// test write

    putc(*(char*)private_region); // test read

    

}