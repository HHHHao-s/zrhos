#include "platform.h"

extern char kernelvec[];

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

    handle_timer_interrupt();

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

    w_sepc(sepc);
    w_sstatus(sstatus);
}

