#include "virtio.h"
#include "memlayout.h"
#include "lock.h"
#include "proc.h"
#include "monitor.h"
#include "defs.h"
// the address of virtio mmio register r.
#define R(r) ((volatile uint32_t *)(VIRTIO2 + (r)))
#define EVENT_Q 0
#define STATUS_Q 1
#define QS 2






static struct virtio_input_dev {
  // a set (not a ring) of DMA descriptors, with which the
  // driver tells the device where to read and write individual
  // disk operations. there are NUM descriptors.
  // most commands consist of a "chain" (a linked list) of a couple of
  // these descriptors.
  struct virtq_desc *desc[QS];

  // a ring in which the driver writes descriptor numbers
  // that the driver would like the device to process.  it only
  // includes the head descriptor of each chain. the ring has
  // NUM elements.
  struct virtq_avail *avail[QS];

  // a ring in which the device writes descriptor numbers that
  // the device has finished processing (just the head of each chain).
  // there are NUM used ring entries.
  struct virtq_used *used[QS];

  uint16_t used_idx[QS]; // we've looked this far in used[2..NUM].

  // buffer for the input events
  struct {
    struct virtio_input_event buffer;
    char pending; // driver sets to 0 when it has seen the status
  } info[QS][NUM];


  lm_lock_t vinput_lock;
  
} input;



static void queue_init(int idx){
  // Select the queue writing its index (first queue is 0) to QueueSel.
  *R(VIRTIO_MMIO_QUEUE_SEL) = idx;

  // ensure queue 0 is not in use.
  if(*R(VIRTIO_MMIO_QUEUE_READY))
    panic("virtio input should not be ready");

  // check maximum queue size.
  uint32_t max = *R(VIRTIO_MMIO_QUEUE_NUM_MAX);
  if(max == 0)
    panic("virtio input has no queue 0");
  if(max < NUM)
    panic("virtio input max queue too short");

  // allocate and zero queue memory.
  input.desc[idx] = (struct virtq_desc*)mem_malloc(sizeof(struct virtq_desc)* NUM);
  input.avail[idx] = (struct virtq_avail*)mem_malloc(sizeof(struct virtq_avail));
  input.used[idx] = (struct virtq_used*)mem_malloc(sizeof(struct virtq_used));
  memset(input.desc[idx], 0, sizeof(struct virtq_desc)* NUM);
  memset(input.avail[idx], 0, sizeof(struct virtq_avail));
  memset(input.used[idx], 0, sizeof(struct virtq_used));

  // // set queue size.
  *R(VIRTIO_MMIO_QUEUE_NUM) = NUM;

  // // write physical addresses.
  *R(VIRTIO_MMIO_QUEUE_DESC_LOW) = (uint64_t)input.desc[idx];
  *R(VIRTIO_MMIO_QUEUE_DESC_HIGH) = (uint64_t)input.desc[idx] >> 32;
  *R(VIRTIO_MMIO_DRIVER_DESC_LOW) = (uint64_t)input.avail[idx];
  *R(VIRTIO_MMIO_DRIVER_DESC_HIGH) = (uint64_t)input.avail[idx] >> 32;
  *R(VIRTIO_MMIO_DEVICE_DESC_LOW) = (uint64_t)input.used[idx];
  *R(VIRTIO_MMIO_DEVICE_DESC_HIGH) = (uint64_t)input.used[idx] >> 32;

  // // queue is ready.
  *R(VIRTIO_MMIO_QUEUE_READY) = 0x1;

  
}


void
virtio_input_init()
{
  uint32_t status = 0;

  printf("virtio input init\n");
  // printf("magic value: %p\n", *R(VIRTIO_MMIO_MAGIC_VALUE));
  // printf("version: %p\n", *R(VIRTIO_MMIO_VERSION));
  // printf("device id: %p\n", *R(VIRTIO_MMIO_DEVICE_ID));
  // printf("vendor id: %p\n", *R(VIRTIO_MMIO_VENDOR_ID));

  lm_lockinit(&input.vinput_lock, "virtio_input");

  if(*R(VIRTIO_MMIO_MAGIC_VALUE) != 0x74726976 ||
     *R(VIRTIO_MMIO_VERSION) != 2 ||
     *R(VIRTIO_MMIO_DEVICE_ID) != 18 ||
     *R(VIRTIO_MMIO_VENDOR_ID) != 0x554d4551){
    panic("could not find virtio input");
  }
  
  // reset device
  *R(VIRTIO_MMIO_STATUS) = status;

  // set ACKNOWLEDGE status bit
  status |= VIRTIO_CONFIG_S_ACKNOWLEDGE;
  *R(VIRTIO_MMIO_STATUS) = status;

  // set DRIVER status bit
  status |= VIRTIO_CONFIG_S_DRIVER;
  *R(VIRTIO_MMIO_STATUS) = status;

  // negotiate features
  uint64_t features = *R(VIRTIO_MMIO_DEVICE_FEATURES);
  features &= ~(1 << VIRTIO_F_ANY_LAYOUT);
  features &= ~(1 << VIRTIO_RING_F_EVENT_IDX);
  features &= ~(1 << VIRTIO_RING_F_INDIRECT_DESC);
  *R(VIRTIO_MMIO_DRIVER_FEATURES) = features;

  // tell device that feature negotiation is complete.
  status |= VIRTIO_CONFIG_S_FEATURES_OK;
  *R(VIRTIO_MMIO_STATUS) = status;

  // re-read status to ensure FEATURES_OK is set.
  status = *R(VIRTIO_MMIO_STATUS);
  if(!(status & VIRTIO_CONFIG_S_FEATURES_OK))
    panic("virtio input FEATURES_OK unset");

  for(int i = 0; i < QS; i++){
    queue_init(i);
    input.used_idx[i] = 0;
  }
  // keep the eventq populated with empty buffers.
  for(int i=0;i<NUM;i++){

    input.desc[EVENT_Q][i].addr = (uint64_t)&input.info[EVENT_Q][i].buffer;
    input.desc[EVENT_Q][i].len = sizeof(input.info[EVENT_Q][i].buffer);
    input.desc[EVENT_Q][i].flags = VRING_DESC_F_WRITE;
    input.desc[EVENT_Q][i].next = 0;

    input.avail[EVENT_Q]->ring[i] = i;
    input.avail[EVENT_Q]->idx += 1;


  }
  
    
  // // tell device we're completely ready.
  status |= VIRTIO_CONFIG_S_DRIVER_OK;
  *R(VIRTIO_MMIO_STATUS) = status;

}




void virtio_input_intr(){
    
  // printf("virtio input interrupt\n");

  lm_lock(&input.vinput_lock);
  // read the device status register to clear the interrupt.
  // *R(VIRTIO_MMIO_INTERRUPT_ACK) = *R(VIRTIO_MMIO_INTERRUPT_STATUS) & 0x3;

  __sync_synchronize();

  while(input.used_idx[EVENT_Q] != input.used[EVENT_Q]->idx){
    __sync_synchronize();
    int id = input.used[EVENT_Q]->ring[input.used_idx[EVENT_Q] % NUM].id; 
    // input.info[EVENT_Q][id].pending = 0;
    input.used_idx[EVENT_Q] += 1;
    
    printf("input event: type %d code %d value %d\n",input.info[EVENT_Q][id].buffer.type,input.info[EVENT_Q][id].buffer.code,input.info[EVENT_Q][id].buffer.value);

    keyboard_intr(input.info[EVENT_Q][id].buffer);

    // wakeup(&input.info[EVENT_Q][id].pending);
  }

  while(input.avail[EVENT_Q]->idx - input.used[EVENT_Q]->idx <NUM){

    input.avail[EVENT_Q]->idx += 1;
  }

  __sync_synchronize();
  
  lm_unlock(&input.vinput_lock);

}