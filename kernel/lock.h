#pragma once    
struct lm_lock{

    int locked;
    int cpu;
    char name[32];

};

typedef struct lm_lock lm_lock_t;