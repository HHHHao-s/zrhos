#include "memlayout.h"
#include "types.h"
#include "defs.h"
#include "platform.h"



//
// the riscv Platform Level Interrupt Controller (PLIC).
//

void
plic_init(void)
{
  // set desired IRQ priorities non-zero (otherwise disabled).
  *(uint32_t*)(PLIC + UART0_IRQ*4) = 1;
//   *(uint32_t*)(PLIC + VIRTIO0_IRQ*4) = 1;
}

void
plic_inithart(void)
{
  int hart = cpuid();
  
  // set enable bits for this hart's S-mode
  // for the uart and virtio disk.
  *(uint32_t*)PLIC_SENABLE(hart) = (1 << UART0_IRQ) ; // | (1 << VIRTIO0_IRQ);

  // set this hart's S-mode priority threshold to 0.
  *(uint32_t*)PLIC_SPRIORITY(hart) = 0;
}

// ask the PLIC what interrupt we should serve.
int
plic_claim(void)
{
  int hart = cpuid();
  int irq = *(uint32_t*)PLIC_SCLAIM(hart);
  return irq;
}

// tell the PLIC we've served this IRQ.
void
plic_complete(int irq)
{
  int hart = cpuid();
  *(uint32_t*)PLIC_SCLAIM(hart) = irq;
}
