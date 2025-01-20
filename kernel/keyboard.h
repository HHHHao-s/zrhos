#include <stdarg.h>

#include "types.h"
#include "lock.h"
#include "memlayout.h"
#include "platform.h"
#include "proc.h"
#include "file.h"

#define BUF_SIZE 128

typedef struct keyboard_event{

    uint8_t type;
    uint8_t code;
    uint16_t value;

} keyboard_event_t;



typedef struct keyboard{

    lm_lock_t lock;

    keyboard_event_t buffer[BUF_SIZE];

    int head;
    int tail;


}keyboard_t;