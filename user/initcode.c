
#include "usyscall.h"
int main(){

    putc('a');
    if(fork()==0){
        // child
        putc('b');
    }else{
        // parent
        putc('c');
    }

}