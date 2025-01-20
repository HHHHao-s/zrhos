#define NCPU 8  // maximum number of CPUs
#define NTASK 1010 // maximum number of tasks
#define DEV_MAX 16 // maximum number of devices

#define NOFILE 100 // open files per system
#define NBUF 100 // size of disk block cache
#define ROOTDEV       1  // device number of file system root disk
#define NINODE 100 // maximum number of active i-nodes
#define ROOTINO  1   // root i-number
#define MAXARG       32  // max exec arguments
#define MAXPATH      128 // max path name

// ------- device major id -------------
#define CONSOLE 0 // console device node
#define MONITOR0 1 // monitor0 device node
#define KEYBOARD 2 // keyboard device node