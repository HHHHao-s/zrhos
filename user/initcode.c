#include "kernel/mmap.h"
#include "usyscall.h"
int main(){

    putc('a');

    uint64_t private_region = mmap(0, 4096, PERM_R | PERM_W, MAP_ANONYMOUS | MAP_PRIVATE);

    *(char*)private_region = 'a';// test write

    putc(*(char*)private_region); // test read

    // test copy-on-write
    int pid = fork();
    if(pid == 0){
        // child
        putc(*(char*)private_region); // test read
        *(char*)private_region = 'b';// test write
        putc(*(char*)private_region); // test read
        exit(0);
    }else{
        // parent
        wait(0);
        putc(*(char*)private_region); // test read

    }

}