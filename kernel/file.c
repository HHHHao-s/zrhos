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

void fileclose(file_t* f){
    lm_lock(&ftable.lock);
    f->ref=0;
    lm_unlock(&ftable.lock);
}


