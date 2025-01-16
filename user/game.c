#include "ulib.h"
#include "usyscall.h"
#include "kernel/fctrl.h"
#include "kernel/fs.h"
#include "kernel/monitor.h"

struct monitor_info info;
uint8_t *buf;

int main(){

    int monitor_fd = open("/dev/monitor0", O_RDWR);
    if(monitor_fd < 0){
        fprintf(2, "cannot open monitor\n");
        exit(1);
    }

    if(ioctl(monitor_fd, MONITOR_GET_INFO,(uint64_t) &info) < 0){
        fprintf(2, "cannot get monitor info\n");
        exit(1);
    }

    printf("width: %d, height: %d\n", info.width, info.height);

    buf = (uint8_t*)malloc(info.width*info.height*4);

    close(monitor_fd);

    free(buf);
}