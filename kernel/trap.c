#include "platform.h"
#include "proc.h"

extern char kernelvec[];
extern char uservec[];
void trap_init(){
    printf("trap_init\n");
}

void trap_inithart(){
    printf("trap_inithart\n");

    // write kernelvec stvec register
    w_stvec((uint64_t)kernelvec);
}

void handle_external(){
    int irq = plic_claim();
    if(irq == UART0_IRQ){
        printf("uart interrupt\n");
        uart_intr();
    }
    plic_complete(irq);
}

void handle_software(){
    // printf("software interrupt\n");

    // clear software interrupt pending
    w_sip(r_sip() & (~SIE_SSIE));

    task_t *t = mytask();
    if(t != NULL && t->state == RUNNING){
        yield();
    }

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
        // exception
        printf("Exception %p\n", scause);
    }
}

void kerneltrap(){

    // trap from supervisor mode
    if((r_sstatus() & SSTATUS_SPP) == 0)
        panic("kerneltrap: not from supervisor mode");
    
    // interrupts enabled
    if(intr_get() != 0)
        panic("kerneltrap: interrupts enabled");
    uint64_t sepc = r_sepc();
    uint64_t sstatus = r_sstatus();
    // uint64_t scause = r_scause();
    handle_trap();

    w_sepc(sepc);
    w_sstatus(sstatus);
}

// void usertrap(void){
//     printf("usertrap\n");
//     // now satp is zero
//     if( (r_sstatus() & SSTATUS_SPP) != 0)
//         panic("usertrap: not from user mode");
    
//     // send interrupts and exceptions to kernelvec since we're now in the kernel
//     w_stvec((uint64_t)kernelvec);

//     task_t *t = mytask();
//     t->trapframe.epc = r_sepc();
//     if(r_scause() == 8){
//         // system call
//         // if(killed(t))
//         //     exit();
        
//         // sepc points to the ecall instruction
//         // but we want to return to the next instruction
//         t->trapframe.epc += 4;

//         // an interrupt will change sepc, scause, and sstatus
//         // so enable only now that we're done with those registers
//         intr_on();

//         // syscall();
//         printf("syscall\n");
//     }else{
//         handle_trap();
//     }
//     userret();

// }

// void userret(void){


//     task_t *t = mytask();
//     intr_off();
//     w_sepc(t->trapframe.epc);
//     w_stvec((uint64_t)uservec);
    

// }