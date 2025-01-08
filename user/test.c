#include "ulib.h"

char *c = "hello world";

int main(int argc, char **argv){

    printf("%s from test\n", c);

    for(int i = 0; i < argc; i++){
        printf("%s\n", argv[i]);
    }

    return 0;
}