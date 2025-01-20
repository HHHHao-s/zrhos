//
// keyboard device
//
#include <stdarg.h>
#include "keyboard.h"
#include "virtio.h"
#include "lock.h"
#include "defs.h"
extern device_t devsw[];

keyboard_t keyboard;


void keyboard_intr(struct virtio_input_event event ){
    
    lm_lock(&keyboard.lock);
    if(event.type!=1){
        lm_unlock(&keyboard.lock);
        return;
    }

    keyboard.buffer[keyboard.head % BUF_SIZE] = (keyboard_event_t){event.type, event.code, event.value};
    keyboard.head = keyboard.head + 1;
    if(keyboard.head - keyboard.tail > BUF_SIZE){
        keyboard.tail = keyboard.head - BUF_SIZE;
    }
    lm_unlock(&keyboard.lock);

}


uint64_t keyboard_read_event(device_t*dev,  int user_dst, uint64_t dst, uint64_t n){

    if(n < sizeof(keyboard_event_t)){
        return -1;
    }

    lm_lock(&keyboard.lock);
    if(keyboard.head == keyboard.tail){
        lm_unlock(&keyboard.lock);
        return 0;
    }

    uint64_t copy_size = n;
    if(sizeof(keyboard_event_t)*(keyboard.head-keyboard.tail) < n){
        copy_size = sizeof(keyboard_event_t)*(keyboard.head-keyboard.tail);
    }
   
    if(either_copyout(user_dst, dst, &keyboard.buffer[keyboard.tail],  copy_size)){
        return -1;
    }
    keyboard.tail = keyboard.head;
    lm_unlock(&keyboard.lock);

    return copy_size;

}

void keyboard_init(){
    
    lm_lockinit(&keyboard.lock, "keyboard");
    keyboard.head = 0;
    keyboard.tail = 0;

    virtio_input_init(); // emulated keyboard input

    device_t *dev = &devsw[KEYBOARD];
    dev->ptr = &keyboard;
    dev->read = keyboard_read_event;
    dev->id = KEYBOARD;
    dev->ioctl = NULL;
    dev->write = NULL;
    dev->name = "keyboard";
    
}