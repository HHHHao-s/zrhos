#include "platform.h"

int main(){
    if(cpuid() == 0){
        uart_init();
        printf("Hello, world!\n");
        panic("main");
    }else{
        ;
    }
    

}

