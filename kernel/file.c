#include "file.h"
#include "defs.h"
#include "proc.h"
#include "param.h"
device_t devsw[DEV_MAX];

struct{
    lm_lock_t lock;
    file_t files[NOFILE];
}ftable;



file_t *filealloc(void){

    lm_lock(&ftable.lock);

    for (int i = 0; i < NOFILE; i++)
    {
        if (ftable.files[i].ref == 0)
        {
            ftable.files[i].ref = 1;
            lm_unlock(&ftable.lock);
            return &ftable.files[i];
        }
    }

    lm_unlock(&ftable.lock);
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
        t->trapframe->a0= dev->write(dev, 1, (uint64_t)buf, count);
    }
    return -1;
}   

int sys_read(){
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
        t->trapframe->a0= dev->read(dev, 1, (uint64_t)buf, count);
    }
    return -1;
}