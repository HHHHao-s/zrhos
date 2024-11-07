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
    int fd = open("fs.img", O_RDWR | O_CREAT | O_TRUNC, 0666);
    if(fd < 0){
        cout << "open failed" << endl;
        return -1;
    }
    ftruncate(fd, 2*1024*1024);
    
    write(fd, "hello world", sizeof("hello world"));

    close(fd);
    return 0;



}
