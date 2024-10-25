#include "defs.h"
#include "proc.h"
#include "memlayout.h"
#include "types.h"
#include "rbtree.h"
#include "mmap.h"

static int compf(const struct rb_node *l, const struct rb_node *r, void *){
    return l->val.addr < r->val.addr;
}

static void freef(struct rb_node *n){
    mem_free(n);
}

void mmap_init(task_t *t){
    t->mmap_obj = (struct Mmap*)mem_malloc(sizeof(struct Mmap));
    rb_init(&t->mmap_obj->tree, 0, compf, 0);
}

void mmap_destroy(task_t *t){
    rb_clear_free(&t->mmap_obj->tree, freef);
    mem_free(t->mmap_obj);
    t->mmap_obj = 0;
}

// map an anonymous memory region with given permission and size
// round up the size to PGSIZE
// if addr is 0, the kernel will choose a random virtual address
int sys_mmap(){

  task_t *t = mytask();
  uint64_t addr = t->trapframe->a0;
  uint64_t sz = t->trapframe->a1;
  uint64_t perm = t->trapframe->a2;
  uint64_t flag = t->trapframe->a3;
  t->trapframe->a0 = mmap(t,addr, sz, perm, flag);
  return 0;
}

// use log to record the mapping of the page
// don't allocate physical memory at the beginning
// when the page is accessed, allocate physical memory in the page fault handler
uint64_t mmap(task_t *t, uint64_t addr, uint64_t sz, uint64_t perm, uint64_t flag){
  
  if( addr % PGSIZE != 0 || sz % PGSIZE != 0){
    return -1;
  }

    Mmap_t *obj = (Mmap_t*)t->mmap_obj;

    if(addr == 0 && (flag & MAP_ANONYMOUS)){
        addr = VA_ANYNOMOUS+obj->high_level;
    }

    struct rb_node *node = (struct rb_node*)mem_malloc(sizeof(struct rb_node));
    node->val.addr = addr;
    node->val.sz = sz;
    node->val.perm = perm;
    node->val.flag = flag;
    int out;
    struct rb_node *ret = rb_insert(&obj->tree, node, &out);
  
    if(ret != 0){
        obj->high_level += sz;
        return addr;
    }else{
        panic("mmap failed");
    }

    return addr;

  

}

void handle_pagefault(){

    // stval will contain the faulting address
    uint64_t stval = r_stval();

    uint64_t va = PGROUNDDOWN(stval);

    task_t *t = mytask();

    Mmap_t *obj = t->mmap_obj;

    struct rb_node cmp_node;
    cmp_node.val.addr = va;

    // find the mapping
    struct rb_node *node = rb_ubnd(&obj->tree, &cmp_node);
    if(node == rb_lmst(&obj->tree)){
        // can't find the mapping
        panic("page fault can't find the mapping");
    }
    node = rb_prev(node);
    if(node->val.addr <= va && va < node->val.addr + node->val.sz){
        // found the mapping
        // allocate physical memory
        if(node->val.flag & MAP_ANONYMOUS){
            // anonymous memory
            uint64_t pa = (uint64_t)mem_malloc(PGSIZE);
            if(pa == 0){
                panic("page fault can't allocate physical memory");
            }
            // map the physical memory to the virtual address
            if(uvm_map(t->pagetable, va, pa, PGSIZE, node->val.perm, 0) < 0){
                panic("page fault can't map physical memory");
            }
        }
        return;
    }else{
        panic("page fault can't find the mapping");
    }

}