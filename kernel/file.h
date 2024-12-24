#pragma once

#include "types.h"
#include "fs.h"
#include "defs.h"
#include "lock.h"
struct device {
  const char *name;
  int id;
  void *ptr;
  uint64_t (*read) (struct device *dev, int user_src , uint64_t buf, uint64_t count);
  uint64_t (*write)(struct device *dev, int user_src , uint64_t buf, uint64_t count);
};
typedef struct device device_t;

typedef struct file {
  enum { FD_NONE, FD_DEVICE, FD_INODE } type;
  int ref; // reference count
  char readable;
  char writable;
  short major;       // FD_DEVICE

//   struct pipe *pipe; // FD_PIPE
  struct inode *ip;  // FD_INODE and FD_DEVICE
  uint_t off;          // FD_INODE
  
}file_t;


// in-memory copy of an inode
typedef struct inode {
  uint_t dev;           // Device number
  uint_t inum;          // Inode number
  int ref;            // Reference count (protected by itable lock)
  int valid;          // inode has been read from disk? (protected by itable lock)
  
  // copy of disk inode
  semophore_t lock;   // protects everything below here
  short type;         // copy of disk inode
  short major;
  short minor;
  short nlink;
  uint_t size;
  uint_t addrs[NDIRECT+1];
}inode_t;