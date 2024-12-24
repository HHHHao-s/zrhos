#pragma once
// On-disk file system format.
// Both the kernel and user programs use this header file.
#include "types.h"

#define ROOTINO  1   // root i-number
#define BSIZE 1024  // block size

// Disk layout:
// [ boot block | super block | inode blocks |
//                                          free bit map | data blocks]
//
// mkfs computes the super block and builds an initial file system. The
// super block describes the disk layout:
struct superblock {
  uint_t magic;        // Must be FSMAGIC
  uint_t size;         // Size of file system image (blocks)
  uint_t nblocks;      // Number of data blocks
  uint_t ninodes;      // Number of inodes.
  uint_t inodestart;   // Block number of first inode block
  uint_t bmapstart;    // Block number of first free map block
};

#define FSMAGIC 0x0048525a

#define NDIRECT 12
#define NINDIRECT (BSIZE / sizeof(uint_t))
#define MAXFILE (NDIRECT + NINDIRECT)

enum {
  T_NONE = 0, // None
  T_DIR = 1,   // Directory
  T_FILE = 2,  // File
  T_DEVICE = 3 // Device
};

// On-disk inode structure
struct dinode {
  short type;           // File type
  short major;          // Major device number (T_DEVICE only)
  short minor;          // Minor device number (T_DEVICE only)
  short nlink;          // Number of links to inode in file system
  uint_t size;            // Size of file (bytes)
  uint_t addrs[NDIRECT+1];   // Data block addresses, the last one is indirect block
};
// indirect block contains block numbers of data blocks

// Inodes per block.
#define IPB           (BSIZE / sizeof(struct dinode))

// Block containing inode i
#define IBLOCK(i, sb)     ((i) / IPB + sb.inodestart)

// Bitmap bits per block
#define BPB           (BSIZE*8)

// Block of free map containing bit for block b
#define BBLOCK(b, sb) ((b)/BPB + sb.bmapstart)

// Directory is a file containing a sequence of dirent structures.
#define DIRSIZ 14

struct dirent {
  ushort_t inum;
  char name[DIRSIZ];
};

// dirent per block
#define DPB (BSIZE/sizeof(struct dirent))