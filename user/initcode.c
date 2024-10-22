
#include "usyscall.h"
int main(){

    putc('a');

    for(int i=0;i<8;i++){

        if(fork()==0){
            // child
            putc('b'+i);
            exit(0);
        }else{
            // parent
            putc('c'+i);
        }

    }

    for(int i=0;i<8;i++){
        int status;
        int pid=wait(&status);
        putc('0'+pid);
    }

}