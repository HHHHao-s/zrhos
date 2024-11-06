#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <iostream>
#include "kernel/fs.h"


using namespace std;

int main(){
    cout << "hello world" << endl;
    int fd = open("../fs.img", O_RDWR | O_CREAT | O_TRUNC, 0666);
    if(fd < 0){
        cout << "open failed" << endl;
        return -1;
    }
    ftruncate(fd, 2*1024*1024);
    
    // 2MB
    uint8_t *addr = (uint8_t *)mmap(0, 2*1024*1024, PROT_READ | PROT_WRITE, MAP_SHARED , fd, 0);
    
    // write a super block
    struct superblock sb={
        .magic = FSMAGIC,
        .size = 2*1024*1024,
        .nblocks = 2*1024*1024/BSIZE,
        .ninodes = 200,
        .nlog = 16,
        .logstart = 2,
        .inodestart = 18,
        .bmapstart = 34,
    };
    *(struct superblock*)(addr) = sb;



    

    munmap(addr, 2*1024*1024);   

    close(fd);
    return 0;



}
