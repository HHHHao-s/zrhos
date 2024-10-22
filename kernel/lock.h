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

typedef struct task task_t;

struct task_list{
    task_t *task;
    struct task_list *next;
    struct task_list *prev;
};

typedef struct semophore{
    int value;
    lm_lock_t lk;
    struct task_list *head;
    struct task_list *tail;

} semophore_t;