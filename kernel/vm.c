#include "platform.h"
#include "memlayout.h"
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

    // allocate and map a kernel stack for each process.


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



