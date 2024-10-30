#include "file.h"
#include "defs.h"
#include "proc.h"

device_t devsw[DEV_MAX];

file_t ftable[NOFILE];

file_t *filealloc(void){

    for (int i = 0; i < NOFILE; i++)
    {
        if (ftable[i].ref == 0)
        {
            ftable[i].ref = 1;
            return &ftable[i];
        }
    }
    return 0;
}

int sys_write(){
    task_t *t = mytask();
    int fd = t->trapframe->a0;  
    char *buf = (char*)t->trapframe->a1;
    int count = t->trapframe->a2;
    if (fd < 0 || fd >= NOFILE)
    {
        return -1;
    }
    file_t *f = t->ofile[fd];
    if(f==0)
        return -1;
    if (f->type == FD_DEVICE)
    {

        device_t *dev = &devsw[f->major];
        return dev->write(dev, 1, (uint64_t)buf, count);
    }
    return -1;
}   

int sys_read(){
    return 0;
}