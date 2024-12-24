#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <iostream>
#include <memory.h>
#include "kernel/fs.h"
#include <unordered_map>
#include <map>
#include <string>

#define ROUNDUP(a, sz)  (((a) + (sz) - 1) & ~((sz) - 1))

using namespace std;

struct buf{
    uint_t blockno;
    uint8_t* data;
};


uint8_t *addr;
map<uint_t, buf> bufmap; // cache


// Disk layout:
// [ boot block | super block | inode blocks |
//                                          free bit map | data blocks]
//
// mkfs computes the super block and builds an initial file system. The
// super block describes the disk layout:
struct superblock sb={
    .magic = FSMAGIC,
    .size = 256*1024*1024, // 256MB
    .nblocks = 256*1024*1024/BSIZE,
    .ninodes = 1024*BSIZE/sizeof(struct dinode), // 1024 block inodes
    .inodestart = 2,
    .bmapstart = 2 + 1024
};



struct buf *bread(uint_t blockno){
    if(bufmap.find(blockno) != bufmap.end()){
        return &bufmap[blockno];
    }
    bufmap.insert({blockno, buf()});
    bufmap[blockno].blockno = blockno;
    bufmap[blockno].data = addr+blockno*BSIZE;
    return &bufmap[blockno];

}

void brelse(struct buf *b){
    ;
}

uint_t get_datastart(){

    uint_t bits = (sb.nblocks)/ (BPB); 
    uint_t Bs = bits*8;
    uint_t blocks = (Bs+BSIZE-1)/BSIZE;
    return sb.bmapstart + blocks;

}

// return the block number of the first free block
uint_t balloc(){
    
    uint_t datastart = get_datastart();
    struct buf *b = NULL;
    for(uint_t i=datastart; i<sb.nblocks; i++){
        if(b==NULL || BBLOCK(i, sb) > b->blockno){
            if(b!=NULL){
                brelse(b);
            }
            b = bread(BBLOCK(i, sb));
        }
        uint_t bi = i % BPB;
        uint_t m = 1 << (bi % 8);
        if((b->data[bi/8] & m) == 0){
            b->data[bi/8] |= m;
            brelse(b);
            return i;
        }
    }
    return 0;
}

struct dinode *ialloc(uint_t type, int *inum = 0){

    struct buf *b = NULL;
    for(uint_t i=1; i<sb.ninodes; i++){
        if(b==NULL || IBLOCK(i, sb) > b->blockno){
            if(b!=NULL){
                brelse(b);
            }
            b = bread(IBLOCK(i, sb));
        }
        dinode *dip = (dinode*)(b->data) + i%IPB;
        if(dip->type == 0){
            memset(dip, 0, sizeof(*dip));
            dip->type = type;
            if(inum != 0){
                *inum = i;
            }
            
            cout << "ialloc: " << i << endl;
            return dip;
        }
    }
    return 0;
}

template<typename T>
void bwrite(uint_t blockno, T *data){
    *((T*)(addr+blockno*BSIZE)) = *data;
}


int dirappend(dinode *dip, dirent ent){
    if(dip->type != T_DIR){
        cout << "not a dir" << endl;
        return 0;
    }
    struct buf *b = NULL;
    int ndirect = 0;
    for(int i=0;i<NDIRECT*DPB;i++){
        if(b == NULL || i>=(ndirect+1)*DPB){
            if(b!=NULL){
                brelse(b);
                ndirect++;
            }           
            if(dip->addrs[ndirect] == 0){
                dip->addrs[ndirect] = balloc();
            }
            b = bread(dip->addrs[ndirect]);
        }
        dirent *d = (dirent*)b->data + i%DPB;
        if(d->inum == 0){
            *d = ent;
            dip->size += sizeof(dirent);
            brelse(b);
            return 0;
        }
        
    }
    if(b)
        brelse(b);
    b = NULL;
    if(dip->addrs[NDIRECT] == 0){
        dip->addrs[NDIRECT] = balloc();
    }
    struct buf *indirect = bread(dip->addrs[NDIRECT]);
    int nindirect = 0;
    for(int i=0;i<NINDIRECT*DPB;i++){
        if(b == NULL || i>=(nindirect+1)*DPB){
            if(b!=NULL){
                brelse(b);
                nindirect++;
            }            
            if(((uint_t*)indirect->data)[nindirect] == 0){
                ((uint_t*)indirect->data)[nindirect] = balloc();
            }
            b = bread(((uint_t*)indirect->data)[nindirect]);

        }
        dirent *d = (dirent*)b->data + i%DPB;
        if(d->inum == 0){
            *d = ent;
            dip->size += sizeof(dirent);
            brelse(b);
            brelse(indirect);
            return 0;
        }
    }
    
    if(b!=NULL){
        brelse(b);
    }
    if(indirect!=NULL){
        brelse(indirect);
    }

    return -1;
}

int main(int argc, char **argv){

    if(argc < 2){
        cout << "usage: mkfs imgname [file1 file2 ...]" << endl;
        return -1;
    }
    string imgname = argv[1];
    int fd = open(imgname.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0666);
    if(fd < 0){
        cout << "open failed" << endl;
        return -1;
    }
    // 256MB
    ftruncate(fd, 256*1024*1024);
    
    addr =(uint8_t*) mmap(NULL, 256*1024*1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    // write superblock
    bwrite(1, &sb);
    uint_t datastart = get_datastart();
    cout << "datastart: " << datastart << endl;
    // write root inode
    int inum = 0;   
    dinode *root_dip = ialloc(T_DIR, &inum);
    root_dip->nlink = 1;
    dirent ent;
    ent.inum = inum; // the inum of root inode if 1 
    strcpy(ent.name, ".");
    dirappend(root_dip, ent);
    strcpy(ent.name, "..");
    dirappend(root_dip, ent);
    

    cout << "root inode done" << endl;
    // write files 
    for(int i=2;i<argc;i++){
        string name = argv[i];
        struct stat st;
        if(stat(name.c_str(), &st) < 0){
            cout << name <<  "stat failed" << endl;
            continue;
        }
        size_t size = st.st_size;
        
        if(size > MAXFILE*BSIZE){
            cout << name << "too large" << endl;
            continue;
        }

        cout << name << " size " << size << endl;

        dinode *dip = ialloc(T_FILE, &inum);
        cout << "ialloc done" << endl;
        dip->nlink = 1;    
        dip->size = size;

        int pblock = 0;
        struct buf *indirect = 0;

        
        FILE *f = fopen(name.c_str(), "rb");
        if(f == NULL){
            cout << name << "open failed" << endl;
            continue;
        }

        while(size > 0){
            uint_t bno = balloc();
            struct buf *b = bread(bno);
            
            size_t n = fread(b->data, 1, BSIZE, f);
            if(n < BSIZE){
                memset(b->data+n, 0, BSIZE-n);
            }
            if(pblock < NDIRECT){
                dip->addrs[pblock] = bno;
            }else{
                if(indirect == 0){
                    int indirect_bno = balloc();
                    dip->addrs[NDIRECT] = indirect_bno;
                    indirect = bread(indirect_bno);
                }
                ((uint_t*)indirect->data)[pblock-NDIRECT] = bno;
            }
            pblock++;
            
            brelse(b);
            size -= n;
            
        }
        brelse(indirect);
        ent.inum = inum;

        name = name.substr(name.find_last_of('_')+1);


        strcpy(ent.name, name.c_str());
        if(dirappend(root_dip, ent)){
            cout << name << "dirappend failed" << endl;
        }
        cout << name << " write done " << endl;
        fclose(f);

    }
    munmap(addr, 256*1024*1024);


    return 0;



}
