#include "defs.h"
#include "fs.h"
#include "file.h"
#include "param.h"
#include "proc.h"


int sys_exec(){

    char path[MAXPATH], *argv[MAXARG];
    int i;
    task_t *t = mytask();
    char *upath = (char*)t->trapframe->a0;
    char **uargv = (char**)t->trapframe->a1;

    pagetable_t pagetable = mytask()->pagetable;


    if(copyinstr(pagetable,  path, (uint64_t)upath, MAXPATH)<0){
        return -1;
    }

    memset(argv, 0, sizeof(argv));
    for(i=0;; i++){
        if(i >= MAXARG){
            goto bad;
        }
        uint64_t uarg;
        if(fetchaddr(pagetable,(uint64_t)&uargv[i], &uarg) < 0){
            goto bad;
        }
        if(uarg == 0){
            argv[i] = 0;
            break;
        }
        argv[i] = mem_malloc(PGSIZE);
        if(argv[i] == 0)
            goto bad;
        // read string from address uarg to argv[i]
        if(fetchstr(pagetable, uarg, argv[i], PGSIZE) < 0)
            goto bad;
    }

    int ret = exec(path, argv);

    // free the memory
    for(i = 0; i < MAXARG && argv[i] != 0; i++)
        mem_free(argv[i]);

    return ret;

bad:
    for(i = 0; i < MAXARG && argv[i] != 0; i++)
        mem_free(argv[i]);

    return -1;

}