#include "defs.h"
#include "proc.h"
#include "memlayout.h"
#include "types.h"
#include "rbtree.h"
#include "mmap.h"

static struct rb_tree ref;



static int compf(const struct rb_node *l, const struct rb_node *r, void *){
    return ((MmapNode_t*)l->data)->addr < ((MmapNode_t*)r->data)->addr;
}

static int ref_cnt_compf(const struct rb_node *l, const struct rb_node *r, void *){
    return ((RefCntNode_t*)l->data)->ref_cnt < ((RefCntNode_t*)r->data)->ref_cnt ;
}

static struct rb_node *ref_cnt_alloc(uint64_t pa, uint64_t ref_cnt){
    struct rb_node *node = (struct rb_node*)mem_malloc(sizeof(struct rb_node));
    RefCntNode_t *data = (RefCntNode_t*)mem_malloc(sizeof(RefCntNode_t));
    *data = (RefCntNode_t){pa, ref_cnt};
    node->data = data;
    node->type = RefCntNode;
    return node;
}

static int ref_cnt_inc(uint64_t pa){
    struct rb_node cmp_node;
    RefCntNode_t cmp_data;
    cmp_data.addr = pa;
    cmp_node.data = (void*)&cmp_data;
    struct rb_node *node = rb_find(&ref, &cmp_node);
    if(node == rb_head(&ref)){
        node = ref_cnt_alloc(pa, 1);
        rb_insert(&ref, node, 0);
    }
    else{
        ((RefCntNode_t*)node->data)->ref_cnt++;
    }
    return 0;
}

static int ref_cnt_dec(uint64_t pa){

    struct rb_node cmp_node;
    RefCntNode_t cmp_data;
    cmp_data.addr = pa;
    cmp_node.data = (void*)&cmp_data;
    struct rb_node *node = rb_find(&ref, &cmp_node);
    if(node == rb_head(&ref)){
        return -1;
    }
    else{
        ((RefCntNode_t*)node->data)->ref_cnt--;
        if(((RefCntNode_t*)node->data)->ref_cnt == 0){
            rb_erase(&ref, node);
            mem_free((void*)((RefCntNode_t*)node->data)->addr);
            mem_free(node->data);
            mem_free(node);
        }
    }
    return 0;

}

static uint64_t ref_cnt_get(uint64_t pa){
    struct rb_node cmp_node;
    RefCntNode_t cmp_data;
    cmp_data.addr = pa;
    cmp_node.data = (void*)&cmp_data;
    struct rb_node *node = rb_find(&ref, &cmp_node);
    if(node == rb_head(&ref)){
        return 0;
    }
    else{
        return ((RefCntNode_t*)node->data)->ref_cnt;
    }
}

static void mmap_dump(struct rb_node *node){
    MmapNode_t *data = (MmapNode_t*)node->data;
    printf("addr: %p, sz: %p, perm: %p, flag: %p\n", data->addr, data->sz, data->perm, data->flag);
    for(uint64_t i = data->addr; i < data->addr + data->sz; i+=PGSIZE){
        printf("addr: %p ref_cnt: %d\n",i, (int)ref_cnt_get(i));
    }
}

void mmap_init(){
    // create a global rbtree for the reference count of the physical page
    rb_init(&ref, 0, ref_cnt_compf, 0);
}

void mmap_free(struct rb_node *node){
    mmap_dump(node);
    if(node->type == MmapNode){
        mem_free(node->data);
        mem_free(node);
    }
    
}

static void mmap_freef(struct rb_node *n){

    MmapNode_t *data = (MmapNode_t*)n->data;
    for(uint64_t i = data->addr; i < data->addr + data->sz; i+=PGSIZE){
        // decrease the reference count of the physical page
        ref_cnt_dec(i);
    }
    mmap_free(n);
}

void mmap_create(task_t *t){
    t->mmap_obj = (struct Mmap*)mem_malloc(sizeof(struct Mmap));
    rb_init(&t->mmap_obj->tree, 0, compf, 0);
}

void mmap_destroy(task_t *t){
    if(t->mmap_obj == 0)
        return;
    rb_clear_free(&t->mmap_obj->tree, mmap_freef);
    mem_free(t->mmap_obj);
    t->mmap_obj = 0;
}

struct rb_node *mmap_alloc(uint64_t addr,
        uint64_t sz,
        uint64_t perm,
        uint64_t flag){
    MmapNode_t* data = (MmapNode_t*)mem_malloc(sizeof(MmapNode_t));
    struct rb_node *node = (struct rb_node*)mem_malloc(sizeof(struct rb_node));
    *data = (MmapNode_t){addr, sz, perm, flag};
    node->data = data;
    node->type = MmapNode;
    return node;
}

struct rb_node *mmap_alloc_copy(MmapNode_t *data){
    return mmap_alloc(data->addr, data->sz, data->perm, data->flag);
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
  t->trapframe->a0 = mmap(t,addr, sz, perm, flag,0 );
  return 0;
}

// it will allocate physical memory for the page
// and increase the reference count of the physical memory
int uvm_mappages(pagetable_t pagetable, uint64_t va, uint64_t sz, int perm){
    uint64_t a, last;
    a = PGROUNDDOWN(va);
    last = PGROUNDDOWN(va + sz - 1);
    for(;;){
        uint64_t pa = (uint64_t)mem_malloc(PGSIZE);
        if(pa == 0)
            return -1;
        if(uvm_map(pagetable, a,pa, PGSIZE, perm, 0) < 0)
            return -1;
        ref_cnt_inc(pa);
        if(a == last)
            break;
        a += PGSIZE;
        
    }
    return 0;
}

// use log to record the mapping of the page
// don't allocate physical memory at the beginning
// when the page is accessed, allocate physical memory in the page fault handler
// can be used by the kernel to allocate memory for the user
// if alloc_phy is 1, allocate physical memory and increase the reference count
// anynonous memory with addr is allowed
uint64_t mmap(task_t *t, uint64_t addr, uint64_t sz, uint64_t perm, uint64_t flag, int alloc_phy){
    if((flag & MAP_PRIVATE) &&( flag & MAP_SHARED)){
        panic("mmap: MAP_PRIVATE and MAP_SHARED can't be used together");
    }
    if((flag & MAP_SHARED) == 0){
        // default map private
        flag |= MAP_PRIVATE;
    }
    addr = PGROUNDDOWN(addr);
    sz = PGROUNDUP(sz);

    Mmap_t *obj = (Mmap_t*)t->mmap_obj;

    if(addr == 0 && ((flag & MAP_ZERO )== 0) && (flag & MAP_ANONYMOUS)){
        addr = VA_ANYNOMOUS+obj->high_level;
    }

    struct rb_node *node = mmap_alloc(addr, sz, perm, flag);

    int out;
    struct rb_node *ret = rb_insert(&obj->tree, node, &out);
  
    if(out != 0 && ret!=rb_head(&obj->tree)){
        if(addr == 0 && ((flag & MAP_ZERO )== 0) && (flag & MAP_ANONYMOUS))
            obj->high_level += sz;
        
    }else{
        panic("mmap failed");
    }
    if(alloc_phy){
        uvm_mappages(t->pagetable, addr, sz, perm);
    }

    return addr;
}

void handle_accessfault(){
    uint64_t stval = r_stval();
    uint64_t scause = r_scause();

    
    printf("access fault stval:%p scause:%d\n", stval, scause);
    panic("access fault");
    
}

// handle the page fault
void handle_pagefault(){

    // stval will contain the faulting address
    uint64_t stval = r_stval();

    uint64_t va = PGROUNDDOWN(stval);

    task_t *t = mytask();

    Mmap_t *obj = t->mmap_obj;

    struct rb_node cmp_node;

    MmapNode_t cmp_data;
    cmp_data.addr = va;
    cmp_node.data = &cmp_data;

    // find the mapping
    struct rb_node *node = rb_ubnd(&obj->tree, &cmp_node);
    if(node == rb_lmst(&obj->tree)){
        // can't find the mapping
        panic("page fault can't find the mapping");
    }
    node = rb_prev(node);
    MmapNode_t *mmap_node = (MmapNode_t*)node->data;
    if(mmap_node->addr <= va && va < mmap_node->addr + mmap_node->sz){
        // found the mapping
        // allocate physical memory
        if(mmap_node->flag & MAP_ANONYMOUS){
            // anonymous memory
            uint64_t pa = (uint64_t)mem_malloc(PGSIZE);
            if(pa == 0){
                panic("page fault can't allocate physical memory");
            }
            // map the physical memory to the virtual address
            if(uvm_map(t->pagetable, va, pa, PGSIZE, mmap_node->perm, 0) < 0){
                panic("page fault can't map physical memory");
            }
            ref_cnt_inc(pa);
        }

        return;
    }else{
        panic("page fault can't find the mapping");
    }

}

void copy_mmap(task_t *t, task_t *nt){
    Mmap_t *obj = t->mmap_obj;
    Mmap_t *nobj = nt->mmap_obj;
    struct rb_node *node = rb_lmst(&obj->tree);
    struct rb_node *head = rb_head(&obj->tree);
    int out;
    while(node != head){
        struct rb_node *n = mmap_alloc_copy((MmapNode_t*)node->data);
        
        rb_insert(&nobj->tree, n, &out);
        if(out == 0)
            panic("copy_mmap: rb_insert");
        
        // do some page table copy
        MmapNode_t *mmap_node = (MmapNode_t*)n->data;

        for(uint64_t va=mmap_node->addr; va<mmap_node->addr+mmap_node->sz; va+=PGSIZE){
            // don't need to allocate physical memory
            pte_t *pte = walk(t->pagetable, va, 0);
            if(pte == 0 || (*pte & PTE_V) == 0){
                // we havn't allocated the page table yet
                // just skip
                continue;
            }
            // map the same physical memory to the new task
            // for the use of copy-on-write
            // disable the write permission
            uint64_t mask = 0xffffffffffffffff;
            // when map private, disable the write permission
            // the page is copy-on-write
            if(mmap_node->flag & MAP_PRIVATE){
                mask &= ~PTE_W;
            }

            uint64_t pa = PTE2PA(*pte);
            // remap the father's memory
            if(uvm_map(t->pagetable, va, pa, PGSIZE, mmap_node->perm & mask, 1) < 0)
                panic("copy_mmap: uvm_map");

            if(uvm_map(nt->pagetable, va, pa, PGSIZE, mmap_node->perm & mask, 0) < 0)
                panic("copy_mmap: uvm_map");

            ref_cnt_inc(pa);
        }

        node = rb_next(node);

    }
    nobj->high_level = obj->high_level;
}