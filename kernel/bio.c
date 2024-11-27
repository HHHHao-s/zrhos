// buffer cache
#include "types.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"
#include "lock.h"

struct {
  lm_lock_t lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf head;
} bcache;

void
binit(void)
{
  struct buf *b;

  lm_lockinit(&bcache.lock, "bcache");

  // Create linked list of buffers
  bcache.head.prev = &bcache.head;
  bcache.head.next = &bcache.head;
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.head.next;
    b->prev = &bcache.head;
    lm_sem_init(&b->sem, 1); // semaphore lock
    bcache.head.next->prev = b;
    bcache.head.next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint_t dev, uint_t blockno)
{
  struct buf *b;

  lm_lock(&bcache.lock);

  // Is the block already cached?
  for(b = bcache.head.next; b != &bcache.head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      lm_unlock(&bcache.lock);
      lm_P(&b->sem);
      b->holding_task = getpid();
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  for(b = bcache.head.prev; b != &bcache.head; b = b->prev){
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      lm_unlock(&bcache.lock);
      lm_P(&b->sem);
      b->holding_task = getpid();
      return b;
    }
  }
  panic("bget: no buffers");
  return 0;
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint_t dev, uint_t blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(b->holding_task != getpid())
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(b->holding_task != getpid())
    panic("brelse");

  lm_V(&b->sem);

  lm_lock(&bcache.lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.head.next;
    b->prev = &bcache.head;
    bcache.head.next->prev = b;
    bcache.head.next = b;
  }
  
  lm_unlock(&bcache.lock);
}

void
bpin(struct buf *b) {
  lm_lock(&bcache.lock);
  b->refcnt++;
  lm_unlock(&bcache.lock);
}

void
bunpin(struct buf *b) {
  lm_lock(&bcache.lock);
  b->refcnt--;
  lm_unlock(&bcache.lock);
}


