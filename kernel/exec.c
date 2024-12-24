#include "defs.h"
#include "param.h"
#include "fs.h"
#include "file.h"
#include "elf.h"
#include "mmap.h"
#include "proc.h"
#include "memlayout.h"

static int loadseg(pagetable_t, uint64_t , struct inode *, uint_t , uint_t );

int flags2perm(int flags)
{
    int perm = PTE_R;
    if(flags & 0x1)
      perm |= PTE_X;
    if(flags & 0x2)
      perm |= PTE_W;
    return perm;
}

int
exec(char *path, char **argv)
{

  int i=0, off=0;
  uint64_t argc=0, sp=0, ustack[MAXARG], stackbase=0;
  struct elfhdr elf;
  struct inode *ip;
  struct proghdr ph;
  pagetable_t pagetable = 0;
  Mmap_t *mmap_obj=0;
  struct task *t = mytask();

  if((ip = namei(path)) == 0){
    return -1;
  }
  ilock(ip);

  // Check ELF header
  if(readi(ip, 0, (uint64_t)&elf, 0, sizeof(elf)) != sizeof(elf))
    goto bad;

  if(elf.magic != ELF_MAGIC)
    goto bad;

  if((pagetable = user_pagetable(t)) == 0)
    goto bad;

  if((mmap_obj = mmap_create(t)) == 0)
    goto bad;

  // Load program into memory.
  for(i=0, off=elf.phoff; i<elf.phnum; i++, off+=sizeof(ph)){
    if(readi(ip, 0, (uint64_t)&ph, off, sizeof(ph)) != sizeof(ph))
      goto bad;
    if(ph.type != ELF_PROG_LOAD)
      continue;
    if(ph.memsz < ph.filesz)
      goto bad;
    if(ph.vaddr + ph.memsz < ph.vaddr)
      goto bad;
    if(ph.vaddr % PGSIZE != 0)
      goto bad;
    uint64_t maddr = mmap_by_obj(mmap_obj, pagetable, ph.vaddr, ph.memsz, flags2perm(ph.flags), MAP_PRIVATE, 1);
    if(maddr != ph.vaddr)
      goto bad;

    if(loadseg(pagetable, ph.vaddr, ip, ph.off, ph.filesz) < 0)
      goto bad;
  }
  iunlockput(ip);
  ip = 0;

  t = mytask();
  

  // allocate two pages as user stack
  uint64_t addr =  mmap_by_obj(mmap_obj, pagetable, VA_USERSTACK, 2*PGSIZE, PERM_R | PERM_W, MAP_PRIVATE, 1);
  sp = VA_USERSTACK + 2*PGSIZE;
  if(addr == 0)
    goto bad;


  // Push argument strings, prepare rest of stack in ustack.
  for(argc = 0; argv[argc]; argc++) {
    if(argc >= MAXARG)
      goto bad;
    sp -= strlen(argv[argc]) + 1;
    sp -= sp % 16; // riscv sp must be 16-byte aligned
    if(sp < stackbase)
      goto bad;
    if(copyout(pagetable, sp, argv[argc], strlen(argv[argc]) + 1) < 0)
      goto bad;
    ustack[argc] = sp;
  }
  ustack[argc] = 0;

  // push the array of argv[] pointers.
  sp -= (argc+1) * sizeof(uint64_t);
  sp -= sp % 16;
  if(sp < stackbase)
    goto bad;
  if(copyout(pagetable, sp, (char *)ustack, (argc+1)*sizeof(uint64_t)) < 0)
    goto bad;

  // arguments to user main(argc, argv)
  // argc is returned via the system call return
  // value, which goes in a0.
  t->trapframe->a1 = sp;
  t->trapframe->a0 = argc;
    


  // Commit to the user image.
  if(t->pagetable)
    free_pagetable(t->pagetable, 0);
  if(t->mmap_obj)
    mmap_destroy(t->mmap_obj);
  t->pagetable = pagetable;
  t->mmap_obj = mmap_obj;
  memset(t->trapframe, 0, sizeof(t->trapframe));
  t->trapframe->epc = elf.entry;  // initial program counter = main
  t->trapframe->sp = sp; // initial stack pointer


  return 0; 

 bad:
  panic("exec bad");

  if(pagetable)
    free_pagetable(pagetable, 0);
  if(mmap_obj)
    mmap_destroy(mmap_obj);
  if(ip){
    iunlockput(ip);
  }
  return -1;
}

// Load a program segment into pagetable at virtual address va.
// va must be page-aligned
// and the pages from va to va+sz must already be mapped.
// Returns 0 on success, -1 on failure.
static int
loadseg(pagetable_t pagetable, uint64_t va, struct inode *ip, uint_t offset, uint_t sz)
{
  uint_t i, n;
  uint64_t pa;

  for(i = 0; i < sz; i += PGSIZE){
    pa = walkaddr(pagetable, va + i);
    if(pa == 0)
      panic("loadseg: address should exist");
    if(sz - i < PGSIZE)
      n = sz - i;
    else
      n = PGSIZE;
    if(readi(ip, 0, (uint64_t)pa, offset+i, n) != n)
      return -1;
  }
  
  return 0;
}
