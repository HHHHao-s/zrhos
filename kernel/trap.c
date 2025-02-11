#include "memlayout.h"
#include "types.h"
#include "defs.h"
#include "platform.h"
#include "proc.h"

extern char kernelvec[];
extern char uservec[];
extern char userret[];
extern char trampoline[];
lm_lock_t tickslock;
volatile uint_t *ticks;

void trap_init(){
    printf("trap_init\n");
    lm_lockinit(&tickslock, "time");
    ticks =(uint_t *) USERRDONLY;
}

void trap_inithart(){
    printf("trap_inithart\n");

    // write kernelvec stvec register
    w_stvec((uint64_t)kernelvec);
}



void
clockintr()
{
    // atomic increment of ticks
  __sync_fetch_and_add(ticks, 1);
  wakeup(&ticks);
}

void handle_external(){
    int irq = plic_claim();

    switch (irq){
    case UART0_IRQ:
        uart_intr();
        break;
    case VIRTIO0_IRQ:

        virtio_disk_intr();
        break;

    case VIRTIO1_IRQ:

        virtio_gpu_intr();
        break;

    case VIRTIO2_IRQ:
    
        virtio_input_intr();
        break;  
    }

    plic_complete(irq);
}

void handle_software(){
    // printf("software interrupt\n");

    // clear software interrupt pending
    uint64_t sepc = r_sepc();
    uint64_t sstatus = r_sstatus();
    w_sip(r_sip() & (~SIE_SSIE));
    if(cpuid() == 0){
        clockintr();
    }

    task_t *t = mytask();
    if(t != NULL && t->state == RUNNING){
        yield();
    }
    // yield may cause some traps to occur
    w_sepc(sepc);
    w_sstatus(sstatus);
}

void handle_trap(){
    uint64_t scause = r_scause();
    if(scause & SCAUSE_INTERRUPT){
        // interrupt
        uint64_t code = scause & (~SCAUSE_INTERRUPT);
        switch (code)
        {
        case 1:
            handle_software();
            break;
        case 9:
            handle_external();
            break;
        default:
            printf("unhandle scause %p\n", scause);
            break;
        }


    }else{
        // printf("Exception %p\n", scause);
        // exception
        switch (scause)
        {
        case 13:
        case 15:
        case 12:{

            // store/AMO or load page fault
            handle_pagefault();
            break;
        }
        case 1:
        case 5:
        case 7:{
            // access fault
            handle_accessfault();
        }
           
            
        
        default:
            panic("handle_trap");
            break;
        }
        
        
    }
}

void kerneltrap(){

    // trap from supervisor mode
    if((r_sstatus() & SSTATUS_SPP) == 0)
        panic("kerneltrap: not from supervisor mode");
    
    // interrupts enabled
    if(intr_get() != 0)
        panic("kerneltrap: interrupts enabled");
    
    // uint64_t scause = r_scause();
    handle_trap();

    
}

void usertrap(void){
    // now satp is kernel page table
    if( (r_sstatus() & SSTATUS_SPP) != 0)
        panic("usertrap: not from user mode");
    
    // send interrupts and exceptions to kernelvec since we're now in the kernel
    w_stvec((uint64_t)kernelvec);

    task_t *t = mytask();
    t->trapframe->epc = r_sepc();
    if(r_scause() == 8){
        // system call
        if(killed())
            exit(0);
        
        // sepc points to the ecall instruction
        // but we want to return to the next instruction
        t->trapframe->epc += 4;

        // an interrupt will change sepc, scause, and sstatus
        // so enable only now that we're done with those registers
        intr_on();

        syscall();
        
    }else{
        // printf("scause %d\n", r_scause());
        handle_trap();
    }

    if(killed()){
        exit(0);
    }

    usertrapret();

}

void usertrapret(void){


    task_t *t = mytask();

    // turn off interrupts until we're back in user space
    intr_off();
    
    // set user vector
    uint64_t va_uservec = TRAMPOLINE + (uservec - trampoline);
    w_stvec(va_uservec);

    // set up trapframe values that uservec will need when
    t->trapframe->kernel_satp = r_satp();
    t->trapframe->kernel_sp = t->kstack + PGSIZE;
    t->trapframe->kernel_trap = (uint64_t)usertrap;
    t->trapframe->kernel_hartid = r_tp();

    // set SPP to 0 for user mode
    // set SPIE to enable interrupts in user mode
    uint64_t x = r_sstatus();
    x &= ~SSTATUS_SPP;
    x |= SSTATUS_SPIE;
    w_sstatus(x);

    uint64_t satp = MAKE_SATP(t->pagetable);

    w_sepc(t->trapframe->epc);

    // jump to userret in trampoline.S at the top of memory
    // which switches to the user page table, restores user registers
    // and switches to user mode with sret
    uint64_t va_userret = TRAMPOLINE + (userret - trampoline);
    ((void (*)(uint64_t))va_userret)(satp);
    

}