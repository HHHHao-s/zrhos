#pragma once    
typedef struct lm_lock{

    int locked;
    int cpu;
    char name[32];

}lm_lock_t;

typedef struct lm_sleeplock{
    int locked;
    lm_lock_t lk;
    int pid;
}lm_sleeplock_t;

