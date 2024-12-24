#include "fs.h"
#include "platform.h"
#include "types.h"
#include "defs.h"
#include "param.h"
#include "file.h"
#include "proc.h"
#include "buf.h"



struct superblock sb;

uint_t datastart;
struct{
    lm_lock_t lock;
    inode_t inode[NINODE];
}itable;


void readsb(int dev, struct superblock *sb){
    struct buf *bp;
    bp = bread(dev, 1);
    memmove(sb, bp->data, sizeof(*sb));
    brelse(bp);
}


uint_t get_datastart(){

    uint_t bits = (sb.nblocks)/ (BPB); 
    uint_t Bs = bits*8;
    uint_t blocks = (Bs+BSIZE-1)/BSIZE;
    return sb.bmapstart + blocks;

}

// should be called in process
void fs_init(int dev){
    readsb(dev, &sb);
    if(sb.magic != FSMAGIC)
        panic("invalid file system");
    datastart = get_datastart();
    for(int i=0; i<NINODE; i++){
        lm_sem_init(&itable.inode[i].lock, 1);
    }
    lm_lockinit(&itable.lock, "itable");
}


// return the block number of the first free block
uint_t balloc(){
    
    struct buf *b = NULL;
    for(uint_t i=datastart; i<sb.nblocks; i++){
        if(b==NULL || BBLOCK(i, sb) > b->blockno){
            if(b!=NULL){
                brelse(b);
            }
            b = bread(ROOTDEV ,BBLOCK(i, sb));
        }
        uint_t bi = i % BPB;
        uint_t m = 1 << (bi % 8);
        if((b->data[bi/8] & m) == 0){
            b->data[bi/8] |= m;
            bwrite(b);
            brelse(b);
            return i;
        }
    }
    panic("balloc: out of blocks");
    return -1;
}


// Find the inode with number inum on device dev
// and return the in-memory copy. Does not lock
// the inode and does not read it from disk.
static struct inode*
iget(uint_t dev, uint_t inum)
{
  struct inode *ip, *empty;

  lm_lock(&itable.lock);

  // Is the inode already in the table?
  empty = 0;
  for(ip = &itable.inode[0]; ip < &itable.inode[NINODE]; ip++){
    if(ip->ref > 0 && ip->dev == dev && ip->inum == inum){
      ip->ref++;
      lm_unlock(&itable.lock);
      return ip;
    }
    if(empty == 0 && ip->ref == 0)    // Remember empty slot.
      empty = ip;
  }

  // Recycle an inode entry.
  if(empty == 0)
    panic("iget: no inodes");

  ip = empty;
  ip->dev = dev;
  ip->inum = inum;
  ip->ref = 1;
  ip->valid = 0;
  lm_unlock(&itable.lock);

  return ip;
}


// alloc a new inode corresponding to the given type
inode_t *ialloc(uint_t type){

    struct buf *b = NULL;
    for(uint_t i=1; i<sb.ninodes; i++){
        if(b==NULL || IBLOCK(i, sb) > b->blockno){
            if(b!=NULL){
                brelse(b);
            }
            b = bread(ROOTDEV,IBLOCK(i, sb));
        }
        struct dinode *dip = (struct dinode *)(b->data) + i%IPB;
        if(dip->type == 0){
            inode_t *ip = iget(ROOTDEV, i);
            ip->type = type;
            brelse(b);
            return ip;
        }
    }
    return 0;
}

int iupdate(inode_t *ip){

    struct buf *bp = bread(ROOTDEV, IBLOCK(ip->inum, sb));
    struct dinode *dip = (struct dinode *)(bp->data) + ip->inum%IPB;
    dip->type = ip->type;
    dip->major = ip->major;
    dip->minor = ip->minor;
    dip->nlink = ip->nlink;
    dip->size = ip->size;
    memmove(dip->addrs, ip->addrs, sizeof(ip->addrs));
    bwrite(bp);
    brelse(bp);
    return 0;

}

void bfree(uint_t dev, uint_t b){
    struct buf *bp = bread(dev, BBLOCK(b, sb));
    uint_t bi = b % BPB;
    uint_t m = 1 << (bi % 8);
    if((bp->data[bi/8] & m) == 0){
        panic("bfree: freeing free block");
    }
    bp->data[bi/8] &= ~m;
    bwrite(bp);
    brelse(bp);
}

// truncate the inode
int itrunc(inode_t *ip){

    lm_P(&ip->lock);
    for(int i=0;i<NDIRECT;i++){
        if(ip->addrs[i]){
            bfree(ROOTDEV, ip->addrs[i]);
            ip->addrs[i] = 0;
        }
    }
    if(ip->addrs[NDIRECT]){
        struct buf *bp = bread(ROOTDEV, ip->addrs[NDIRECT]);
        uint_t *a = (uint_t*)bp->data;
        for(int j=0;j<NINDIRECT;j++){
            if(a[j]){
                bfree(ROOTDEV, a[j]);
            }
        }
        brelse(bp);
        bfree(ROOTDEV, ip->addrs[NDIRECT]);
        ip->addrs[NDIRECT] = 0;
    }
    ip->size = 0;
    iupdate(ip);
    lm_V(&ip->lock);
    return 0;


}

int ilock(inode_t *ip){

    if(ip == 0 || ip->ref < 1)
        panic("ilock");

    lm_P(&ip->lock);

    if(ip->valid == 0){
        struct buf *bp = bread(ip->dev, IBLOCK(ip->inum, sb));
        struct dinode *dip = (struct dinode *)(bp->data) + ip->inum%IPB;
        ip->type = dip->type;
        ip->major = dip->major;
        ip->minor = dip->minor;
        ip->nlink = dip->nlink;
        ip->size = dip->size;
        memmove(ip->addrs, dip->addrs, sizeof(ip->addrs));
        brelse(bp);
        ip->valid = 1;
        if(ip->type == 0)
            panic("ilock: no type");
    }
    return 0;

}

int iunlock(inode_t *ip){



    lm_V(&ip->lock);

    return 0;

}



// decrease the reference count of the inode
int iput(inode_t *ip){
    

    lm_lock(&itable.lock);
    if(ip->ref == 1 && ip->valid && ip->nlink == 0){
        // inode has no links and no other references: truncate and free.
        lm_P(&ip->lock);

        itrunc(ip);
        ip->type = 0;
        iupdate(ip);
        ip->valid = 0;
        lm_V(&ip->lock);
        lm_unlock(&itable.lock);
        return 0;
    }

    ip->ref--;
    lm_unlock(&itable.lock);
    return 0;
}

int iunlockput(inode_t *ip){

    iunlock(ip);
    iput(ip);
    return 0;

}

// return the no.start bmap
uint_t bmap(inode_t *ip, uint_t start){

    if(start>=MAXFILE)
        return -1;

    uint_t end_block = ip->size/BSIZE+1;

    if(end_block < start){
        // append
        if(end_block<NDIRECT){
            uint_t high = start<NDIRECT?start:NDIRECT;
            for(uint_t i=end_block; i<high; i++){
                uint_t addr = balloc();
                if(addr == 0){
                    return -1;
                }
                ip->addrs[i] = addr;
            }
            if(high >= start){
                return ip->addrs[start];
            }
        }
        // append indirect
        if(ip->addrs[NDIRECT] == 0){
            uint_t addr = balloc();
            if(addr == 0){
                panic("bmap: balloc");
                return -1;
            }
            ip->addrs[NDIRECT] = addr;
        }
        struct buf *bp = bread(ip->dev, ip->addrs[NDIRECT]);
        for(uint_t i=NDIRECT; i<=start; i++){
            if(((uint_t*)bp->data)[i - NDIRECT] != 0)
                continue;
            uint_t addr = balloc();
            if(addr == 0){
                panic("bmap: balloc");
                return -1;
            }
            ((uint_t*)bp->data)[i - NDIRECT] = addr;

        }
        uint_t baddr= ((uint_t*)bp->data)[start - NDIRECT];
        brelse(bp);
        return baddr;

    }else{
        
        if(start<NDIRECT){
            return ip->addrs[start];
        }
        else{    
            struct buf *bp = bread(ip->dev, ip->addrs[NDIRECT]);
            uint_t baddr= ((uint_t*)bp->data)[start - NDIRECT];
            brelse(bp);
            return baddr;
        }
    }
    panic("bmap: out of range");
    return 0;
}

int writei(inode_t *ip, int user_src, uint64_t src, uint_t off, uint_t n){
    uint_t tot, m;
    struct buf *bp;

    if(off > ip->size || off + n < off)
        return -1;
    if(off + n > MAXFILE*BSIZE)
        return -1;

    for(tot=0; tot<n; tot+=m, off+=m, src+=m){
        uint_t addr = bmap(ip, off/BSIZE);
        if(addr == 0)
            break;
        bp = bread(ip->dev, addr);
        m = (n-tot) < (BSIZE - off%BSIZE) ? (n-tot) : (BSIZE - off%BSIZE);
        if(either_copyin(bp->data + (off % BSIZE), user_src, src, m) == -1) {
            brelse(bp);
            break;
        }

        brelse(bp);
    }

    if(off > ip->size)
        ip->size = off;

    // write the i-node back to disk even if the size didn't change
    // because the loop above might have called bmap() and added a new
    // block to ip->addrs[].
    iupdate(ip);

    return tot;
}

int readi(inode_t *ip, int user_dst, uint64_t dst, uint_t off, uint_t n){
    uint_t tot, m;
    struct buf *bp;

    if(off > ip->size || off + n < off || off + n > MAXFILE*BSIZE)
        return -1;


    for(tot=0; tot<n; tot+=m, off+=m, dst+=m){
        uint_t addr = bmap(ip, off/BSIZE);
        if(addr == 0)
            break;
        bp = bread(ip->dev, addr);
        m= (BSIZE - off%BSIZE) <( n - tot) ? (BSIZE - off%BSIZE) : (n - tot);
        if(either_copyout(user_dst, dst, bp->data + (off % BSIZE), m) == -1) {
            brelse(bp);
            break;
        }
        brelse(bp);
    }

    return tot;
}

inode_t *idup(inode_t *ip){
    lm_lock(&itable.lock);
    ip->ref++;
    lm_unlock(&itable.lock);
    return ip;
}


// Paths

// Copy the next path element from path into name.
// Return a pointer to the element following the copied one.
// The returned path has no leading slashes,
// so the caller can check *path=='\0' to see if the name is the last one.
// If no name to remove, return 0.
//
// Examples:
//   skipelem("a/bb/c", name) = "bb/c", setting name = "a"
//   skipelem("///a//bb", name) = "bb", setting name = "a"
//   skipelem("a", name) = "", setting name = "a"
//   skipelem("", name) = skipelem("////", name) = 0
//
static char*
skipelem(char *path, char *name)
{
  char *s;
  int len;

  while(*path == '/')
    path++;
  if(*path == 0)
    return 0;
  s = path;
  while(*path != '/' && *path != 0)
    path++;
  len = path - s;
  if(len >= DIRSIZ)
    memmove(name, s, DIRSIZ);
  else {
    memmove(name, s, len);
    name[len] = 0;
  }
  while(*path == '/')
    path++;
  return path;
}

int namecmp(const char *s, const char *t){
    return strncmp(s, t, DIRSIZ);
}   

// Look for a directory entry in a directory.
// If found, set *poff to byte offset of entry.
struct inode*
dirlookup(struct inode *dp, char *name, uint_t *poff)
{
  uint_t off, inum;
  struct dirent de;

  if(dp->type != T_DIR)
    panic("dirlookup not DIR");

  for(off = 0; off < dp->size; off += sizeof(de)){
    if(readi(dp, 0, (uint64_t)&de, off, sizeof(de)) != sizeof(de))
      panic("dirlookup read");
    if(de.inum == 0)
      continue;
    if(namecmp(name, de.name) == 0){
      // entry matches path element
      if(poff)
        *poff = off;
      inum = de.inum;
      return iget(dp->dev, inum);
    }
  }

  return 0;
}


// Look up and return the inode for a path name.
// If parent != 0, return the inode for the parent and copy the final
// path element into name, which must have room for DIRSIZ bytes.
// Must be called inside a transaction since it calls iput().
static struct inode*
namex(char *path, int nameiparent, char *name)
{
  struct inode *ip, *next;

  if(*path == '/')
    ip = iget(ROOTDEV, ROOTINO);
  else
    ip = idup(mytask()->cwd);

  while((path = skipelem(path, name)) != 0){
    ilock(ip);
    if(ip->type != T_DIR){
      iunlockput(ip);
      return 0;
    }
    if(nameiparent && *path == '\0'){
      // Stop one level early.
      iunlock(ip);
      return ip;
    }
    if((next = dirlookup(ip, name, 0)) == 0){
      iunlockput(ip);
      return 0;
    }
    iunlockput(ip);
    ip = next;
  }
  if(nameiparent){
    iput(ip);
    return 0;
  }
  return ip;
}

struct inode*
namei(char *path)
{
  char name[DIRSIZ];
  return namex(path, 0, name);
}

struct inode*
nameiparent(char *path, char *name)
{
  return namex(path, 1, name);
}
