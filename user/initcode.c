#include "kernel/mmap.h"
#include "usyscall.h"
#include "ulib.h"
#include "kernel/fctrl.h"


// test that fork fails gracefully
// the forktest binary also does this, but it runs out of proc entries first.
// inside the bigger usertests binary, we run out of memory first.
void
forktest(char *s)
{
  enum{ N = 1000 };
  int n, pid;

  for(n=0; n<N; n++){
    pid = fork();
    if(pid < 0)
      break;
    if(pid == 0)
      exit(0);
  }

  if (n == 0) {
    printf("%s: no fork at all!\n", s);
    exit(1);
  }

  if(n == N){
    printf("%s: fork claimed to work 1000 times!\n", s);
    exit(1);
  }

  for(; n > 0; n--){
    if(wait(0) < 0){
      printf("%s: wait stopped early\n", s);
      exit(1);
    }
  }

  if(wait(0) != -1){
    printf("%s: wait got too many\n", s);
    exit(1);
  }
}

// try to find races in the reparenting
// code that handles a parent exiting
// when it still has live children.
void
reparent(char *s)
{
  int master_pid = getpid();
  for(int i = 0; i < 200; i++){
    int pid = fork();
    if(pid < 0){
      printf("%s: fork failed\n", s);
      exit(1);
    }
    if(pid){
      // father
      if(wait(0) != pid){
        printf("%s: wait wrong pid %d\n", s, pid);
        exit(1);
      }
    } else {
      // son
      int pid2 = fork();
      if(pid2 < 0){

        kill(master_pid);
        exit(1);
      }
      exit(0);
    }
  }
  exit(0);
}

void consoletest(char *s){
    char buf[128];
    while(1){
        gets(buf, sizeof(buf));
        printf("input: %s\n", buf);
    }
}
// concurrent forks to try to expose locking bugs.
void
forkfork(char *s)
{
  enum { N=2 };
  
  for(int i = 0; i < N; i++){
    int pid = fork();
    if(pid < 0){
      printf("%s: fork failed", s);
      exit(1);
    }
    if(pid == 0){
      for(int j = 0; j < 200; j++){
        int pid1 = fork();
        if(pid1 < 0){
          exit(1);
        }
        if(pid1 == 0){
          exit(0);
        }
        wait(0);
      }
      exit(0);
    }
  }

  int xstatus;
  for(int i = 0; i < N; i++){
    wait(&xstatus);
    if(xstatus != 0) {
      printf("%s: fork in child failed", s);
      exit(1);
    }
  }
  printf("%s: ok\n", s);
}


int main(){
    // printf("hello world\n");
    // consoletest("consoletest");
    // forktest("forktest");
    // reparent("reparent");
    // forkfork("forkfork");
    // char *argv[4] = {"/test", "1", "2", "3"};
    // exec("/test", argv);
    open("/dev/console", O_RDONLY);// stdin
    open("/dev/console", O_WRONLY);// stdout
    open("/dev/console", O_WRONLY);// stderr
    char *argv[1] = {"/sh"};
    exec("/sh", argv);


}