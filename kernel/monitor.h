#include "lock.h"
struct monitor_info{

    int width, height;

};

typedef struct monitor{
    lm_lock_t lock;
    
    // device properties
    void* buf;
    size_t buf_len;
    int width, height;
} monitor_t;

enum {
    MONITOR_GET_INFO = 1
};