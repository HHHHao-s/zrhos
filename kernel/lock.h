struct lm_lock{

    int locked;
    int cpu;
    char name[32];

};