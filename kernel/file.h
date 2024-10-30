#pragma once
#define DEV_MAX 16
#define CONSOLE 0
#define NOFILE 100
#include "types.h"


struct device {
  const char *name;
  int id;
  void *ptr;
  int (*read) (struct device *dev, int magic , uint64_t buf, int count);
  int (*write)(struct device *dev, int magic , uint64_t buf, int count);
};
typedef struct device device_t;

typedef struct file {
  enum { FD_NONE, FD_DEVICE } type;
  int ref; // reference count
  char readable;
  char writable;
  short major;       // FD_DEVICE

//   struct pipe *pipe; // FD_PIPE
//   struct inode *ip;  // FD_INODE and FD_DEVICE
//   uint off;          // FD_INODE
  
}file_t;