#include "defs.h"
#include "fs.h"
#include "file.h"
#include "param.h"
#include "proc.h"

uint64_t arguint64(int pos){
    task_t *t = mytask();
    switch (pos)
    {
    case 0:
        return t->trapframe->a0;
    case 1:
        return t->trapframe->a1;

    case 2:
        return t->trapframe->a2;
    case 3:
        return t->trapframe->a3;
    case 4:
        return t->trapframe->a4;

    case 5:
        return t->trapframe->a5;
    case 6:
        return t->trapframe->a6;
    default:
        break;
    }
    return 0;

}

int argstr(int pos, char *buf, uint64_t n){
    int rn= 0;
    char *ubuf = (char*)arguint64(pos);
    pagetable_t pagetable = mytask()->pagetable;
    if((rn= copyinstr(pagetable,  buf, (uint64_t)ubuf, n))<0){
        return -1;
    }
    return rn;

}

int sys_exec(){

    char path[MAXPATH], *argv[MAXARG];
    int i;
    task_t *t = mytask();
    char **uargv = (char**)t->trapframe->a1;

    if(argstr(0, path, MAXPATH)<0){
        return -1;
    }

    pagetable_t pagetable = mytask()->pagetable;

    memset(argv, 0, sizeof(argv));
    for(i=0;; i++){
        if(i >= MAXARG){
            goto bad;
        }
        uint64_t uarg;
        if(fetchaddr(pagetable,(uint64_t)&uargv[i], &uarg) < 0){
            goto bad;
        }
        if(uarg == 0){
            argv[i] = 0;
            break;
        }
        argv[i] = mem_malloc(PGSIZE);
        if(argv[i] == 0)
            goto bad;
        // read string from address uarg to argv[i]
        if(fetchstr(pagetable, uarg, argv[i], PGSIZE) < 0)
            goto bad;
    }

    int ret = exec(path, argv);

    // free the memory
    for(i = 0; i < MAXARG && argv[i] != 0; i++)
        mem_free(argv[i]);

    return ret;

bad:
    for(i = 0; i < MAXARG && argv[i] != 0; i++)
        mem_free(argv[i]);

    return -1;

}


int sys_chdir(){

    char path[MAXPATH];
    struct inode *ip;
    task_t *t = mytask();
    char *upath = (char*)t->trapframe->a0;

    pagetable_t pagetable = t->pagetable;

    if(copyinstr(pagetable, path, (uint64_t)upath, MAXPATH) < 0){
        return -1;
    }
    begin_op();
    if((ip = namei(path)) == 0){
        end_op();
        return -1;
    }
    ilock(ip);
    if(ip->type != T_DIR){
        iunlockput(ip);
        end_op();
        return -1;
    }
    iunlock(ip);
    iput(t->cwd);

    t->cwd = ip;
    end_op();
    return 0;

}


static struct inode*
create(char *path, short type, short major, short minor)
{
  struct inode *ip, *dp;
  char name[DIRSIZ];

  if((dp = nameiparent(path, name)) == 0)
    return 0;

  ilock(dp);

  if((ip = dirlookup(dp, name, 0)) != 0){
    iunlockput(dp);
    ilock(ip);
    if(type == T_FILE && (ip->type == T_FILE || ip->type == T_DEVICE))
      return ip;
    iunlockput(ip);
    return 0;
  }

  if((ip = ialloc(dp->dev, type)) == 0){
    iunlockput(dp);
    return 0;
  }

  ilock(ip);
  ip->major = major;
  ip->minor = minor;
  ip->nlink = 1;
  iupdate(ip);

  if(type == T_DIR){  // Create . and .. entries.
    // No ip->nlink++ for ".": avoid cyclic ref count.
    if(dirlink(ip, ".", ip->inum) < 0 || dirlink(ip, "..", dp->inum) < 0)
      goto fail;
  }

  if(dirlink(dp, name, ip->inum) < 0)
    goto fail;

  if(type == T_DIR){
    // now that success is guaranteed:
    dp->nlink++;  // for ".."
    iupdate(dp);
  }

  iunlockput(dp);

  return ip;

 fail:
  // something went wrong. de-allocate ip.
  ip->nlink = 0;
  iupdate(ip);
  iunlockput(ip);
  iunlockput(dp);
  return 0;
}

// Allocate a file descriptor for the given file.
// Takes over file reference from caller on success.
static int
fdalloc(struct file *f)
{
  int fd;
  task_t *t = mytask();

  for(fd = 0; fd < NOFILE; fd++){
    if(t->ofile[fd] == 0){
      t->ofile[fd] = f;
      return fd;
    }
  }
  return -1;
}

int
sys_open(void)
{
    char path[MAXPATH];
    int fd;
    uint64_t omode;
    struct file *f;
    struct inode *ip;
    int n;


    omode = arguint64(1);
    if((n = argstr(0, path, MAXPATH)) < 0)
        return -1;
    begin_op();
    if(omode & O_CREATE){
        ip = create(path, T_FILE, 0, 0);
        if(ip == 0){
            end_op();
            return -1;
        }
    } else {
        if((ip = namei(path)) == 0){
            end_op();
            return -1;
        }
        ilock(ip);
        if(ip->type == T_DIR && omode != O_RDONLY){
            iunlockput(ip);
            end_op();
            return -1;
        }
    }

    if(ip->type == T_DEVICE && (ip->major < 0 || ip->major >= DEV_MAX)){
        iunlockput(ip);
        end_op();
        return -1;
    }

    if((f = filealloc()) == 0 || (fd = fdalloc(f)) < 0){
        if(f)
            fileclose(f);
        iunlockput(ip);
        end_op();
        return -1;
    }

    if(ip->type == T_DEVICE){
        f->type = FD_DEVICE;
        f->major = ip->major;
    } else {
        f->type = FD_INODE;
        f->off = 0;
    }
    f->ip = ip;
    f->readable = !(omode & O_WRONLY);
    f->writable = (omode & O_WRONLY) || (omode & O_RDWR);

    if((omode & O_TRUNC) && ip->type == T_FILE){
        itrunc(ip);
    }

    iunlock(ip);
    end_op();

    mytask()->trapframe->a0 = fd;

    return 0;
}

int sys_write(){
    extern device_t devsw[];
    task_t *t = mytask();
    int fd = t->trapframe->a0;  
    char *buf = (char*)t->trapframe->a1;
    int count = t->trapframe->a2;
    if (fd < 0 || fd >= NOFILE)
    {
        return -1;
    }
    file_t *f = t->ofile[fd];
    if(f==0)
        return -1;
    if (f->type == FD_DEVICE)
    {

        device_t *dev = &devsw[f->major];
        t->trapframe->a0= dev->write(dev, 1, (uint64_t)buf, count);
    }else if(f->type==FD_INODE){
        begin_op();
        ilock(f->ip);
        if(f->writable == 0){
            iunlock(f->ip);
            return -1;
        }
        int n = writei(f->ip, 1, (uint64_t)buf, f->off, count);
        if(n > 0)
            f->off += n;
        iunlock(f->ip);
        end_op();
        t->trapframe->a0 = n;

    }
    return -1;
}   

int sys_read(){
    extern device_t devsw[];
    task_t *t = mytask();
    int fd = t->trapframe->a0;  
    char *buf = (char*)t->trapframe->a1;
    int count = t->trapframe->a2;
    if (fd < 0 || fd >= NOFILE)
    {
        return -1;
    }
    file_t *f = t->ofile[fd];
    if(f==0)
        return -1;
    if (f->type == FD_DEVICE)
    {

        device_t *dev = &devsw[f->major];
        t->trapframe->a0= dev->read(dev, 1, (uint64_t)buf, count);
    }else if(f->type == FD_INODE){

        ilock(f->ip);
        if(f->readable == 0){
            iunlock(f->ip);
            return -1;
        }
        int n = readi(f->ip, 1, (uint64_t)buf, f->off, count);
        if(n > 0)
            f->off += n;
        iunlock(f->ip);
        t->trapframe->a0 = n;

    }
    return -1;
}