#include "types.h"
#include "lock.h"
#define BSIZE 1024  // block size

struct buf {
  int valid;   // has data been read from disk?
  int disk;    // does disk "own" buf?
  uint_t dev;
  uint_t blockno;
  semophore_t sem; // semaphore lock
  uint_t refcnt;
  struct buf *prev; // LRU cache list
  struct buf *next;
  uchar_t data[BSIZE];
  uint_t holding_task; // the task holding this buffer
};

