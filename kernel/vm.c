#include "platform.h"
#include "memlayout.h"
#include "types.h"
#include "defs.h"
#include "error.h"  

// virtual memory module 
// Sv39
// A 64-bit virtual address is split into five fields:
//   39..63 -- must be zero.
//   30..38 -- 9 bits of level-2 index.
//   21..29 -- 9 bits of level-1 index.
//   12..20 -- 9 bits of level-0 index.
//    0..11 -- 12 bits of byte offset within the page.

pagetable_t kernel_pagetable;
extern char etext[];  // kernel.ld sets this to the end of the kernel code.
extern char trampoline[];  // trampoline.S

void kvm_init(){
    
    kernel_pagetable = vm_create();

    // uart registers
    vm_map(kernel_pagetable, UART0, UART0, PGSIZE, PTE_R | PTE_W, 0);
    // virtio mmio disk interface
    // vm_map(kernel_pagetable, VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W, 0);

    // PLIC
    vm_map(kernel_pagetable, PLIC, PLIC, 0x400000, PTE_R | PTE_W, 0);

    // map kernel text executable and read-only.
    vm_map(kernel_pagetable, PHYSTATR, PHYSTATR, (uint64_t)etext-PHYSTATR, PTE_R | PTE_X, 0);

    // map kernel data and the physical RAM we'll make use of.
    vm_map(kernel_pagetable, (uint64_t)etext, (uint64_t)etext, PHYSTOP-(uint64_t)etext, PTE_R | PTE_W, 0);

    // map the trampoline for trap entry/exit to
    // the highest virtual address in the kernel.
    vm_map(kernel_pagetable, TRAMPOLINE, (uint64_t)trampoline, PGSIZE, PTE_R | PTE_X, 0);


}

void kvm_inithart(){
    // wait for any previous writes to the page table memory to finish.
  sfence_vma();

  w_satp(MAKE_SATP(kernel_pagetable));

  // flush stale entries from the TLB.
  sfence_vma();
}


// create an empty kernel page table
pagetable_t vm_create(){
    pagetable_t pagetable;
    pagetable = (pagetable_t)mem_malloc(PGSIZE);
    if(pagetable == 0)
        return 0;
    memset(pagetable, 0, PGSIZE);
    return pagetable;
}



int uvm_map(pagetable_t pagetable, uint64_t va, uint64_t pa, uint64_t sz, int perm, int remap){

    return vm_map(pagetable, va, pa, sz, perm | PTE_U, remap);
}

// can indicate the permission of the page (e.g. user, read, write, execute)
int vm_map(pagetable_t pagetable, uint64_t va, uint64_t pa, uint64_t sz, int perm, int remap){
    
    uint64_t last = PGROUNDDOWN(va + sz - 1);
    for(;;){
        pte_t *pte = walk(pagetable, va, 1);
        if(pte == 0)
            return -1;
        if((*pte & PTE_V) && !remap)
            panic("vm_map: remap");
        *pte = PA2PTE(pa) | perm | PTE_V;
        if(va == last)
            break;
        va += PGSIZE;
        pa += PGSIZE;
    }
    return 0;

}

// Remove npages of mappings starting from va. va must be
// page-aligned. The mappings must exist.
// Optionally free the physical memory.
void
vm_unmap(pagetable_t pagetable, uint64_t va, uint64_t npages, int do_free)
{
  uint64_t a;
  pte_t *pte;

  if((va % PGSIZE) != 0)
    panic("uvmunmap: not aligned");

  for(a = va; a < va + npages*PGSIZE; a += PGSIZE){
    if((pte = walk(pagetable, a, 0)) == 0)
      panic("uvmunmap: walk");
    if((*pte & PTE_V) == 0)
      panic("uvmunmap: not mapped");
    if(PTE_FLAGS(*pte) == PTE_V)
      panic("uvmunmap: not a leaf");
    if(do_free){
      uint64_t pa = PTE2PA(*pte);
      mem_free((void*)pa);
    }
    *pte = 0;
  }
}

// Look up a virtual address, return the physical address,
// or 0 if not mapped.
// Can only be used to look up user pages.
uint64_t
walkaddr(pagetable_t pagetable, uint64_t va)
{
  pte_t *pte;
  uint64_t pa;

  if(va >= MAXVA)
    return 0;

  pte = walk(pagetable, va, 0);
  if(pte == 0)
    return 0;
  if((*pte & PTE_V) == 0)
    return 0;
  if((*pte & PTE_U) == 0)
    return 0;
  pa = PTE2PA(*pte);
  return pa;
}

pte_t* walk(pagetable_t pagetable, uint64_t va, int alloc){
    // the ending 10 bits of the virtual address are the flags
    // the ppn is the physical page number
    // the pte contains the ppn and the flags
    // (ppn >> 10) << 12 is the physical address
    for(int level = 2; level > 0; level--) {
        pte_t *pte = &pagetable[PX(level, va)];
        if(*pte & PTE_V) {
            pagetable = (pagetable_t)PTE2PA(*pte);
        } else {
            if(!alloc || (pagetable = (pagetable_t)mem_malloc(PGSIZE)) == 0)
                return 0;
            memset(pagetable, 0, PGSIZE);
            *pte = PA2PTE(pagetable) | PTE_V;
        }
    }
    return &pagetable[PX(0, va)];

}

int is_pagetable(pte_t pte){
    return (pte & PTE_V) && (pte & (PTE_R|PTE_W|PTE_X)) == 0;
}

// free all memory referenced by page table
void free_pagetable(pagetable_t pagetable, int do_free){
    for(int i=0;i<512;i++){
        pte_t pte = pagetable[i];
        if(is_pagetable(pte)){
            // this PTE points to a lower-level page table.
            uint64_t child = PTE2PA(pte);
            free_pagetable((pagetable_t)child, do_free);
            mem_free((void*)child);
            pagetable[i] = 0;
        } else if(pte & PTE_V){
            uint64_t pa = PTE2PA(pte);
            if(do_free)
                mem_free((void*)pa);
        }
    }
}



// copy pagetable from one to another
// if cover and the PTE is already mapped, free the old page
void copy_pagetable(pagetable_t from , pagetable_t to, int cover){
  for(int i=0;i<512;i++){
    pte_t pte = from[i];
    pte_t tpte = to[i];
    if((pte & PTE_V) == 0 && (tpte & PTE_V) != 0 && cover){
      // free the old page
      free_pagetable((pagetable_t)PTE2PA(tpte), 1);
      continue;
    }
    if(is_pagetable(pte)){
      // this PTE points to a lower-level page table.
      uint64_t child = PTE2PA(pte);
      if(is_pagetable(tpte)){
        // already allocated
        copy_pagetable((pagetable_t)child, (pagetable_t)PTE2PA(tpte), cover);
      } else{
        // allocate a new page table
        pagetable_t tchild = (pagetable_t)mem_malloc(PGSIZE);
        memset((void*)tchild, 0, PGSIZE);
        to[i] = PA2PTE(tchild) | PTE_V;
        copy_pagetable((pagetable_t)child, (pagetable_t)tchild, cover);
      }
    } else if(pte & PTE_V){
      // this PTE points to a page
      uint64_t pa = PTE2PA(pte);
      uint64_t flags = PTE_FLAGS(pte);
      if(tpte & PTE_V){
        if(cover){
          // conver the old page
          uint64_t mem = PTE2PA(tpte);
          memmove((void*)mem, (void*)pa, PGSIZE);
          to[i] = PA2PTE(mem) | flags | PTE_V;
        }
        
      }else{
        // allocate a new page
        uint64_t mem = (uint64_t)mem_malloc(PGSIZE);
        memmove((void*)mem, (void*)pa, PGSIZE);
        to[i] = PA2PTE(mem) | flags | PTE_V;

      }
    }

  }
}

// Copy from kernel to user.
// Copy len bytes from src to virtual address dstva in a given page table.
// Return 0 on success, -1 on error.
int
copyout(pagetable_t pagetable, uint64_t dstva, char *src, uint64_t len)
{
  uint64_t n, va0, pa0;

  while(len > 0){
    va0 = PGROUNDDOWN(dstva);
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0)
      return -1;
    n = PGSIZE - (dstva - va0);
    if(n > len)
      n = len;
    memmove((void *)(pa0 + (dstva - va0)), src, n);

    len -= n;
    src += n;
    dstva = va0 + PGSIZE;
  }
  return 0;
}

// Copy from user to kernel.
// Copy len bytes to dst from virtual address srcva in a given page table.
// Return 0 on success, -1 on error.
int
copyin(pagetable_t pagetable, char *dst, uint64_t srcva, uint64_t len)
{
  uint64_t n, va0, pa0;

  while(len > 0){
    va0 = PGROUNDDOWN(srcva);
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0)
      return -1;
    n = PGSIZE - (srcva - va0);
    if(n > len)
      n = len;
    memmove(dst, (void *)(pa0 + (srcva - va0)), n);

    len -= n;
    dst += n;
    srcva = va0 + PGSIZE;
  }
  return 0;
}

