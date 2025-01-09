#include "ulib.h"
#include "usyscall.h"
#include "kernel/fctrl.h"
#include "kernel/fs.h"


int main(int argc, char *argv[])
{
    if(argc < 2){
        fprintf(2, "usage: %s [path]\n", argv[0]);
        exit(1);
    }
  
    char *path = argv[1];
    int dir_fd= open(path, O_RDONLY);
    if(dir_fd < 0){

        fprintf(2, "ls: cannot access '%s': No such file or directory\n", path);
        exit(1);
    }
    struct dirent de;
    while(read(dir_fd,(char*) &de, sizeof(de)) == sizeof(de)){
        if(de.inum == 0)
            continue;
        printf("%s\n", de.name);
    }


}