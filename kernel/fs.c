#include "fs.h"
#include "platform.h"
#include "types.h"
#include "defs.h"
#include "param.h"
#include "file.h"
#include "buf.h"
struct superblock sb;

void readsb(int dev, struct superblock *sb){
    struct buf *bp;
    bp = bread(dev, 1);
    memmove(sb, bp->data, sizeof(*sb));
    brelse(bp);
}

void fs_init(int dev){
    readsb(dev, &sb);
    if(sb.magic != FSMAGIC)
        panic("invalid file system");
}




