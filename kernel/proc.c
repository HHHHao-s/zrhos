#include "platform.h"

int cpuid(){
  int id = r_tp();
  return id;
}