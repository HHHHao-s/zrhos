//
// monitor device
//
#include <stdarg.h>

#include "types.h"
#include "lock.h"
#include "memlayout.h"
#include "platform.h"
#include "defs.h"
#include "proc.h"
#include "file.h"
#include "monitor.h"

extern device_t devsw[];

monitor_t monitor0;

uint64_t get_monitor_info(device_t*dev,  int user_dst, uint64_t dst, uint64_t n){

    if(n < sizeof(struct monitor_info)){
        return -1;
    }
    monitor_t *this = (monitor_t*)dev->ptr;
    struct monitor_info info;
    info.width = this->width;
    info.height = this->height;
    if(either_copyout(user_dst, dst, &info, sizeof(struct monitor_info)) == -1){
        return -1;
    }

    return sizeof(struct monitor_info);
}


uint64_t
monitor_write(device_t*dev, int user_src, uint64_t src, uint64_t n){
    // write to monitor

    lm_lock(&monitor0.lock);

    monitor_t *this = (monitor_t*)dev->ptr;

    if(n < this->buf_len){
        return -1;
    }

    if(either_copyin(this->buf, user_src, src, this->buf_len) == -1){
        return -1;
    }

    lm_unlock(&monitor0.lock);


    virtio_gpu_transfer_to_host_2d();
    virtio_gpu_resource_flush();


    return this->buf_len;
}

void
monitor_intr(device_t *dev){
    // interrupt handler

}

uint64_t
monitor_ioctl(device_t*dev, int user_src, uint64_t cmd, uint64_t arg){
    // ioctl
    switch (cmd)
    {
    case MONITOR_GET_INFO:
        return get_monitor_info(dev, user_src, arg, sizeof(struct monitor_info));
        break;
    
    default:
        break;
    }


    return 0;
}

void
monitor_init(void){
    // init monitor

    devsw[MONITOR0].name = "monitor0";
    devsw[MONITOR0].id = MONITOR0;
    devsw[MONITOR0].ptr = (void*)&monitor0;
    devsw[MONITOR0].read = NULL;
    devsw[MONITOR0].write = monitor_write;
    devsw[MONITOR0].ioctl = monitor_ioctl;

    lm_lockinit(&monitor0.lock, "monitor0");

    virtio_gpu_init(&monitor0);
    

}