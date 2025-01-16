#include "virtio.h"
#include "memlayout.h"
#include "lock.h"
#include "proc.h"
#include "monitor.h"
#include "defs.h"
// the address of virtio mmio register r.
#define R(r) ((volatile uint32_t *)(VIRTIO1 + (r)))
#define VIRTIO_GPU_EVENT_DISPLAY (1 << 0)
#define CTRL_Q 0
#define CURSOR_Q 1




static struct virtio_gpu_dev {
  // a set (not a ring) of DMA descriptors, with which the
  // driver tells the device where to read and write individual
  // disk operations. there are NUM descriptors.
  // most commands consist of a "chain" (a linked list) of a couple of
  // these descriptors.
  struct virtq_desc *desc[2];

  // a ring in which the driver writes descriptor numbers
  // that the driver would like the device to process.  it only
  // includes the head descriptor of each chain. the ring has
  // NUM elements.
  struct virtq_avail *avail[2];

  // a ring in which the device writes descriptor numbers that
  // the device has finished processing (just the head of each chain).
  // there are NUM used ring entries.
  struct virtq_used *used[2];

  // our own book-keeping.
  char free[2][NUM];  // is a descriptor free?
  uint16_t used_idx[2]; // we've looked this far in used[2..NUM].

  // track info about in-flight operations,
  // for use when completion interrupt arrives.
  // indexed by first descriptor index of chain.
  struct {
    char status; // device writes 0 on success
    char pending; // driver sets to 0 when it has seen the status
  } info[2][NUM];

  // gpu control command headers.
  // one-for-one with descriptors, for convenience.
  struct virtio_gpu_ctrl_hdr ops[NUM];
  
  struct virtio_gpu_config config;

  uint32_t width;
  uint32_t height; 
  bgra_t *framebuffer;
  uint64_t pixels;


  uint32_t resource_id;

  lm_lock_t vgpu_lock;
  
} gpu;


// find a free descriptor, mark it non-free, return its index.
static int
alloc_desc(int queue_idx)
{
  for(int i = 0; i < NUM; i++){
    if(gpu.free[queue_idx][i]){
      gpu.free[queue_idx][i] = 0;
      return i;
    }
  }
  return -1;
}

// mark a descriptor as free.
static void
free_desc(int queue_idx,int i)
{
  if(i >= NUM)
    panic("free_desc 1");
  if(gpu.free[queue_idx][i])
    panic("free_desc 2");
  gpu.desc[queue_idx][i].addr = 0;
  gpu.desc[queue_idx][i].len = 0;
  gpu.desc[queue_idx][i].flags = 0;
  gpu.desc[queue_idx][i].next = 0;
  gpu.free[queue_idx][i] = 1;
  wakeup(&gpu.free[queue_idx][0]);
}

// free a chain of descriptors.
static void
free_chain(int queue_idx, int i)
{
  while(1){
    int flag = gpu.desc[queue_idx][i].flags;
    int nxt = gpu.desc[queue_idx][i].next;
    free_desc(queue_idx, i);
    if(flag & VRING_DESC_F_NEXT)
      i = nxt;
    else
      break;
  }
}

// allocate three descriptors (they need not be contiguous).
// disk transfers always use three descriptors.
static int
allocn_desc(int queue_idx, int *idx, int n)
{
  if(n>NUM)
    return -1;

  for(int i = 0; i < n; i++){
    idx[i] = alloc_desc(queue_idx);
    if(idx[i] < 0){
      for(int j = 0; j < i; j++)
        free_desc(queue_idx, idx[j]);
      return -1;
    }
  }
  return 0;
}

static struct virtio_gpu_resp_display_info get_display_info(){

  

  struct virtio_gpu_ctrl_hdr req={
    .type= VIRTIO_GPU_CMD_GET_DISPLAY_INFO ,
    .flags= 0,
    .fence_id = 0,
    .ctx_id = 0,
    .padding = 0
  };


  lm_lock(&gpu.vgpu_lock);
  int idx[2];
  while(1){
    if( allocn_desc(CTRL_Q, idx, 2)!=-1){
      break;
    }
    sleep(&gpu.free[CTRL_Q][0], &gpu.vgpu_lock);
  }
  gpu.desc[CTRL_Q][idx[0]].addr = (uint64_t)(&req);
  gpu.desc[CTRL_Q][idx[0]].len = sizeof(struct virtio_gpu_ctrl_hdr);
  gpu.desc[CTRL_Q][idx[0]].flags = VRING_DESC_F_NEXT;
  gpu.desc[CTRL_Q][idx[0]].next = idx[1];
  
  struct virtio_gpu_resp_display_info resp;

  gpu.desc[CTRL_Q][idx[1]].addr = (uint64_t)(&resp);
  gpu.desc[CTRL_Q][idx[1]].len = sizeof(struct virtio_gpu_resp_display_info);
  gpu.desc[CTRL_Q][idx[1]].flags = VRING_DESC_F_WRITE;
  gpu.desc[CTRL_Q][idx[1]].next = 0;

  gpu.info[CTRL_Q][idx[0]].pending = 1;

  // tell the device the first index in our chain of descriptors.
  gpu.avail[CTRL_Q]->ring[gpu.avail[CTRL_Q]->idx % NUM] = idx[0];

  __sync_synchronize();

  // tell the device another avail ring entry is available. 
  gpu.avail[CTRL_Q]->idx += 1; // not % NUM ...

  __sync_synchronize();

  *R(VIRTIO_MMIO_QUEUE_NOTIFY) = CTRL_Q; // value is queue number

  while(gpu.info[CTRL_Q][idx[0]].pending == 1){
    sleep((void*)(&gpu.info[CTRL_Q][idx[0]].pending), &gpu.vgpu_lock);
  }
  free_chain(CTRL_Q, idx[0]);
  lm_unlock(&gpu.vgpu_lock);



  return resp;

}


static void virtio_gpu_set_scanout(){

  struct virtio_gpu_set_scanout req;
  req.hdr = (struct virtio_gpu_ctrl_hdr){
    .type = VIRTIO_GPU_CMD_SET_SCANOUT,
  };

  req.r = (struct virtio_gpu_rect){
    .x = 0,
    .y = 0,
    .width = gpu.width,
    .height = gpu.height,
  };

  req.scanout_id = 0;
  req.resource_id = gpu.resource_id;

  lm_lock(&gpu.vgpu_lock);

  int idx[2];
  while(1){
    if( allocn_desc(CTRL_Q, idx, 2)!=-1){
      break;
    }
    sleep(&gpu.free[CTRL_Q][0], &gpu.vgpu_lock);
  }

  struct virtio_gpu_ctrl_hdr resp;

  gpu.desc[CTRL_Q][idx[0]].addr = (uint64_t)(&req);
  gpu.desc[CTRL_Q][idx[0]].len = sizeof(struct virtio_gpu_set_scanout);
  gpu.desc[CTRL_Q][idx[0]].flags = VRING_DESC_F_NEXT;
  gpu.desc[CTRL_Q][idx[0]].next = idx[1];

  gpu.desc[CTRL_Q][idx[1]].addr = (uint64_t)(&resp);
  gpu.desc[CTRL_Q][idx[1]].len = sizeof(struct virtio_gpu_ctrl_hdr);
  gpu.desc[CTRL_Q][idx[1]].flags = VRING_DESC_F_WRITE;
  gpu.desc[CTRL_Q][idx[1]].next = 0;

  gpu.info[CTRL_Q][idx[0]].pending = 1;

  // tell the device the first index in our chain of descriptors.
  gpu.avail[CTRL_Q]->ring[gpu.avail[CTRL_Q]->idx % NUM] = idx[0];

  __sync_synchronize();

  // tell the device another avail ring entry is available.

  gpu.avail[CTRL_Q]->idx += 1; // not % NUM ...

  __sync_synchronize();

  *R(VIRTIO_MMIO_QUEUE_NOTIFY) = CTRL_Q; // value is queue number

  while(gpu.info[CTRL_Q][idx[0]].pending == 1){
    sleep((void*)(&gpu.info[CTRL_Q][idx[0]].pending), &gpu.vgpu_lock);
  }

  free_chain(CTRL_Q, idx[0]);
  lm_unlock(&gpu.vgpu_lock);

  panic_on(resp.type != VIRTIO_GPU_RESP_OK_NODATA, "virtio_gpu_set_scanout failed");

  printf("virtio_gpu_set_scanout success\n");



}

void virtio_gpu_resource_flush(){

  struct virtio_gpu_resource_flush req;
  req.hdr = (struct virtio_gpu_ctrl_hdr){
    .type = VIRTIO_GPU_CMD_RESOURCE_FLUSH,
  };

  req.r = (struct virtio_gpu_rect){
    .x = 0,
    .y = 0,
    .width = gpu.width,
    .height = gpu.height,
  };

  req.resource_id = gpu.resource_id;

  lm_lock(&gpu.vgpu_lock);

  int idx[2];
  while(1){
    if( allocn_desc(CTRL_Q, idx, 2)!=-1){
      break;
    }
    sleep(&gpu.free[CTRL_Q][0], &gpu.vgpu_lock);
  }

  struct virtio_gpu_ctrl_hdr resp;

  gpu.desc[CTRL_Q][idx[0]].addr = (uint64_t)(&req);
  gpu.desc[CTRL_Q][idx[0]].len = sizeof(struct virtio_gpu_resource_flush);
  gpu.desc[CTRL_Q][idx[0]].flags = VRING_DESC_F_NEXT;
  gpu.desc[CTRL_Q][idx[0]].next = idx[1];

  gpu.desc[CTRL_Q][idx[1]].addr = (uint64_t)(&resp);
  gpu.desc[CTRL_Q][idx[1]].len = sizeof(struct virtio_gpu_ctrl_hdr);
  gpu.desc[CTRL_Q][idx[1]].flags = VRING_DESC_F_WRITE;
  gpu.desc[CTRL_Q][idx[1]].next = 0;

  gpu.info[CTRL_Q][idx[0]].pending = 1;

  // tell the device the first index in our chain of descriptors.

  gpu.avail[CTRL_Q]->ring[gpu.avail[CTRL_Q]->idx % NUM] = idx[0];

  __sync_synchronize();

  // tell the device another avail ring entry is available.

  gpu.avail[CTRL_Q]->idx += 1; // not % NUM ...

  __sync_synchronize();

  *R(VIRTIO_MMIO_QUEUE_NOTIFY) = CTRL_Q; // value is queue number

  while(gpu.info[CTRL_Q][idx[0]].pending == 1){
    sleep((void*)(&gpu.info[CTRL_Q][idx[0]].pending), &gpu.vgpu_lock);
  }

  free_chain(CTRL_Q, idx[0]);
  lm_unlock(&gpu.vgpu_lock);

  panic_on(resp.type != VIRTIO_GPU_RESP_OK_NODATA, "virtio_gpu_resource_flush failed");

  printf("virtio_gpu_resource_flush success\n");

}


void virtio_gpu_transfer_to_host_2d(){

  struct virtio_gpu_transfer_to_host_2d req;
  req.hdr = (struct virtio_gpu_ctrl_hdr){
    .type = VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D,
  };
  req.r = (struct virtio_gpu_rect){
    .x = 0,
    .y = 0,
    .width = gpu.width,
    .height = gpu.height,
  };
  req.offset = 0;
  req.resource_id = gpu.resource_id;


  lm_lock(&gpu.vgpu_lock);

  int idx[2];
  while(1){
    if( allocn_desc(CTRL_Q, idx, 2)!=-1){
      break;
    }
    sleep(&gpu.free[CTRL_Q][0], &gpu.vgpu_lock);
  }

  struct virtio_gpu_ctrl_hdr resp;

  gpu.desc[CTRL_Q][idx[0]].addr = (uint64_t)(&req);
  gpu.desc[CTRL_Q][idx[0]].len = sizeof(struct virtio_gpu_transfer_to_host_2d);
  gpu.desc[CTRL_Q][idx[0]].flags = VRING_DESC_F_NEXT;
  gpu.desc[CTRL_Q][idx[0]].next = idx[1];

  gpu.desc[CTRL_Q][idx[1]].addr = (uint64_t)(&resp);
  gpu.desc[CTRL_Q][idx[1]].len = sizeof(struct virtio_gpu_ctrl_hdr);
  gpu.desc[CTRL_Q][idx[1]].flags = VRING_DESC_F_WRITE;
  gpu.desc[CTRL_Q][idx[1]].next = 0;

  gpu.info[CTRL_Q][idx[0]].pending = 1;

  // tell the device the first index in our chain of descriptors.

  gpu.avail[CTRL_Q]->ring[gpu.avail[CTRL_Q]->idx % NUM] = idx[0];

  __sync_synchronize();

  // tell the device another avail ring entry is available.

  gpu.avail[CTRL_Q]->idx += 1; // not % NUM ...

  __sync_synchronize();

  *R(VIRTIO_MMIO_QUEUE_NOTIFY) = CTRL_Q; // value is queue number

  while(gpu.info[CTRL_Q][idx[0]].pending == 1){
    sleep((void*)(&gpu.info[CTRL_Q][idx[0]].pending), &gpu.vgpu_lock);
  }

  free_chain(CTRL_Q, idx[0]);
  lm_unlock(&gpu.vgpu_lock);

  panic_on(resp.type != VIRTIO_GPU_RESP_OK_NODATA, "virtio_gpu_transfer_to_host_2d failed");

  printf("virtio_gpu_transfer_to_host_2d success\n");

}


static void virtio_gpu_resource_attach_backing(){

  struct virtio_gpu_resource_attach_backing req;
  req.hdr = (struct virtio_gpu_ctrl_hdr){
    .type = VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING,
  };
  req.resource_id = gpu.resource_id;
  req.nr_entries = 1;

  lm_lock(&gpu.vgpu_lock);

  int idx[3];
  while(1){
    if( allocn_desc(CTRL_Q, idx, 3)!=-1){
      break;
    }
    sleep(&gpu.free[CTRL_Q][0], &gpu.vgpu_lock);
  }
  struct virtio_gpu_mem_entry entry;
  
  mandelbrot((bgra_t*)gpu.framebuffer, gpu.width, gpu.height);

  entry.addr = (uint64_t)gpu.framebuffer;
  entry.length = gpu.pixels * sizeof(bgra_t);


  struct virtio_gpu_ctrl_hdr resp;

  gpu.desc[CTRL_Q][idx[0]].addr = (uint64_t)(&req);
  gpu.desc[CTRL_Q][idx[0]].len = sizeof(struct virtio_gpu_resource_attach_backing);
  gpu.desc[CTRL_Q][idx[0]].flags = VRING_DESC_F_NEXT;
  gpu.desc[CTRL_Q][idx[0]].next = idx[1];

  gpu.desc[CTRL_Q][idx[1]].addr = (uint64_t)(&entry);
  gpu.desc[CTRL_Q][idx[1]].len = sizeof(struct virtio_gpu_mem_entry);
  gpu.desc[CTRL_Q][idx[1]].flags = VRING_DESC_F_NEXT;
  gpu.desc[CTRL_Q][idx[1]].next = idx[2];

  gpu.desc[CTRL_Q][idx[2]].addr = (uint64_t)(&resp);
  gpu.desc[CTRL_Q][idx[2]].len = sizeof(struct virtio_gpu_ctrl_hdr);
  gpu.desc[CTRL_Q][idx[2]].flags = VRING_DESC_F_WRITE;
  gpu.desc[CTRL_Q][idx[2]].next = 0;


  gpu.info[CTRL_Q][idx[0]].pending = 1;

  // tell the device the first index in our chain of descriptors.
  gpu.avail[CTRL_Q]->ring[gpu.avail[CTRL_Q]->idx % NUM] = idx[0];

  __sync_synchronize();

  // tell the device another avail ring entry is available.
  gpu.avail[CTRL_Q]->idx += 1; // not % NUM ...

  __sync_synchronize();

  *R(VIRTIO_MMIO_QUEUE_NOTIFY) = CTRL_Q; // value is queue number

  while(gpu.info[CTRL_Q][idx[0]].pending == 1){
    sleep((void*)(&gpu.info[CTRL_Q][idx[0]].pending), &gpu.vgpu_lock);
  }
  free_chain(CTRL_Q, idx[0]);
  lm_unlock(&gpu.vgpu_lock);

  panic_on(resp.type != VIRTIO_GPU_RESP_OK_NODATA, "virtio_gpu_resource_attach_backing failed");
  printf("virtio_gpu_resource_attach_backing success\n");

}

static void virtio_gpu_resource_create_2d(){
  struct virtio_gpu_resource_create_2d  req;
  req.hdr = (struct virtio_gpu_ctrl_hdr){
    .type = VIRTIO_GPU_CMD_RESOURCE_CREATE_2D,
  };
  req.width = gpu.width;
  req.height = gpu.height;
  req.resource_id = gpu.resource_id;
  req.format = VIRTIO_GPU_FORMAT_B8G8R8A8_UNORM;

  lm_lock(&gpu.vgpu_lock);

  int idx[2];
  while(1){
    if( allocn_desc(CTRL_Q, idx, 2)!=-1){
      break;
    }
    sleep(&gpu.free[CTRL_Q][0], &gpu.vgpu_lock);
  }

  struct virtio_gpu_ctrl_hdr resp;

  gpu.desc[CTRL_Q][idx[0]].addr = (uint64_t)(&req);
  gpu.desc[CTRL_Q][idx[0]].len = sizeof(struct virtio_gpu_resource_create_2d);
  gpu.desc[CTRL_Q][idx[0]].flags = VRING_DESC_F_NEXT;
  gpu.desc[CTRL_Q][idx[0]].next = idx[1];

  gpu.desc[CTRL_Q][idx[1]].addr = (uint64_t)(&resp);
  gpu.desc[CTRL_Q][idx[1]].len = sizeof(struct virtio_gpu_ctrl_hdr);
  gpu.desc[CTRL_Q][idx[1]].flags = VRING_DESC_F_WRITE;
  gpu.desc[CTRL_Q][idx[1]].next = 0;

  gpu.info[CTRL_Q][idx[0]].pending = 1;

  // tell the device the first index in our chain of descriptors.
  gpu.avail[CTRL_Q]->ring[gpu.avail[CTRL_Q]->idx % NUM] = idx[0];

  __sync_synchronize();

  // tell the device another avail ring entry is available.
  gpu.avail[CTRL_Q]->idx += 1; // not % NUM ...

  __sync_synchronize();

  *R(VIRTIO_MMIO_QUEUE_NOTIFY) = CTRL_Q; // value is queue number

  while(gpu.info[CTRL_Q][idx[0]].pending == 1){
    sleep((void*)(&gpu.info[CTRL_Q][idx[0]].pending), &gpu.vgpu_lock);
  }
  free_chain(CTRL_Q, idx[0]);
  lm_unlock(&gpu.vgpu_lock);

  panic_on(resp.type != VIRTIO_GPU_RESP_OK_NODATA, "virtio_gpu_resource_create_2d failed");

}



void
virtio_gpu_init(monitor_t *monitor)
{
  uint32_t status = 0;

  printf("virtio gpu init\n");
  // printf("magic value: %p\n", *R(VIRTIO_MMIO_MAGIC_VALUE));
  // printf("version: %p\n", *R(VIRTIO_MMIO_VERSION));
  // printf("device id: %p\n", *R(VIRTIO_MMIO_DEVICE_ID));
  // printf("vendor id: %p\n", *R(VIRTIO_MMIO_VENDOR_ID));

  lm_lockinit(&gpu.vgpu_lock, "virtio_gpu");

  if(*R(VIRTIO_MMIO_MAGIC_VALUE) != 0x74726976 ||
     *R(VIRTIO_MMIO_VERSION) != 2 ||
     *R(VIRTIO_MMIO_DEVICE_ID) != 16 ||
     *R(VIRTIO_MMIO_VENDOR_ID) != 0x554d4551){
    panic("could not find virtio gpu");
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
  features &= ~(1 << VIRTIO_GPU_F_VIRGL);
  features &= ~(1 << VIRTIO_GPU_F_EDID);
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
    panic("virtio gpu FEATURES_OK unset");

  // Select the queue writing its index (first queue is 0) to QueueSel.
  *R(VIRTIO_MMIO_QUEUE_SEL) = 0;

  // ensure queue 0 is not in use.
  if(*R(VIRTIO_MMIO_QUEUE_READY))
    panic("virtio gpu should not be ready");

  // check maximum queue size.
  uint32_t max = *R(VIRTIO_MMIO_QUEUE_NUM_MAX);
  if(max == 0)
    panic("virtio gpu has no queue 0");
  if(max < NUM)
    panic("virtio gpu max queue too short");

  // allocate and zero queue memory.
  gpu.desc[0] = (struct virtq_desc*)mem_malloc(sizeof(struct virtq_desc)* NUM);
  gpu.avail[0] = (struct virtq_avail*)mem_malloc(sizeof(struct virtq_avail));
  gpu.used[0] = (struct virtq_used*)mem_malloc(sizeof(struct virtq_used));
  memset(gpu.desc[0], 0, sizeof(struct virtq_desc)* NUM);
  memset(gpu.avail[0], 0, sizeof(struct virtq_avail));
  memset(gpu.used[0], 0, sizeof(struct virtq_used));

  // // set queue size.
  *R(VIRTIO_MMIO_QUEUE_NUM) = NUM;

  // // write physical addresses.
  *R(VIRTIO_MMIO_QUEUE_DESC_LOW) = (uint64_t)gpu.desc[0];
  *R(VIRTIO_MMIO_QUEUE_DESC_HIGH) = (uint64_t)gpu.desc[0] >> 32;
  *R(VIRTIO_MMIO_DRIVER_DESC_LOW) = (uint64_t)gpu.avail[0];
  *R(VIRTIO_MMIO_DRIVER_DESC_HIGH) = (uint64_t)gpu.avail[0] >> 32;
  *R(VIRTIO_MMIO_DEVICE_DESC_LOW) = (uint64_t)gpu.used[0];
  *R(VIRTIO_MMIO_DEVICE_DESC_HIGH) = (uint64_t)gpu.used[0] >> 32;

  // // queue is ready.
  *R(VIRTIO_MMIO_QUEUE_READY) = 0x1;

    // Select the queue writing its index (first queue is 0) to QueueSel.
  *R(VIRTIO_MMIO_QUEUE_SEL) = 1;

  // ensure queue 1 is not in use.
  if(*R(VIRTIO_MMIO_QUEUE_READY))
    panic("virtio gpu should not be ready");

  // check maximum queue size.
  max = *R(VIRTIO_MMIO_QUEUE_NUM_MAX);
  if(max == 0)
    panic("virtio gpu has no queue 1");
  if(max < NUM)
    panic("virtio gpu max queue too short");

  // allocate and zero queue memory.
  gpu.desc[1] = (struct virtq_desc*)mem_malloc(sizeof(struct virtq_desc)* NUM);
  gpu.avail[1] = (struct virtq_avail*)mem_malloc(sizeof(struct virtq_avail));
  gpu.used[1] = (struct virtq_used*)mem_malloc(sizeof(struct virtq_used));
  memset(gpu.desc[1], 0, sizeof(struct virtq_desc)* NUM);
  memset(gpu.avail[1], 0, sizeof(struct virtq_avail));
  memset(gpu.used[1], 0, sizeof(struct virtq_used));

  // // set queue size.
  *R(VIRTIO_MMIO_QUEUE_NUM) = NUM;

  // // write physical addresses.
  *R(VIRTIO_MMIO_QUEUE_DESC_LOW) = (uint64_t)gpu.desc[1];
  *R(VIRTIO_MMIO_QUEUE_DESC_HIGH) = (uint64_t)gpu.desc[1] >> 32;
  *R(VIRTIO_MMIO_DRIVER_DESC_LOW) = (uint64_t)gpu.avail[1];
  *R(VIRTIO_MMIO_DRIVER_DESC_HIGH) = (uint64_t)gpu.avail[1] >> 32;
  *R(VIRTIO_MMIO_DEVICE_DESC_LOW) = (uint64_t)gpu.used[1];
  *R(VIRTIO_MMIO_DEVICE_DESC_HIGH) = (uint64_t)gpu.used[1] >> 32;

  // // queue is ready.
  *R(VIRTIO_MMIO_QUEUE_READY) = 0x1;

  // // all NUM descriptors start out unused.
  for(int i = 0; i < NUM; i++){
    gpu.free[0][i] = gpu.free[1][i] = 1;

  }
    
  // // tell device we're completely ready.
  status |= VIRTIO_CONFIG_S_DRIVER_OK;
  *R(VIRTIO_MMIO_STATUS) = status;

  // plic.c and trap.c arrange for interrupts from VIRTIO1_IRQ.

  // query display information

  gpu.resource_id = 1;

  struct virtio_gpu_resp_display_info resp = get_display_info();
  printf("display info: width:%d height:%d\n",resp.pmodes[0].r.width,resp.pmodes[0].r.height);

  gpu.width = resp.pmodes[0].r.width;
  gpu.height = resp.pmodes[0].r.height;
  gpu.framebuffer = mem_malloc(gpu.width * gpu.height * 4);
  gpu.pixels = gpu.width * gpu.height;

  monitor->buf = gpu.framebuffer;
  monitor->buf_len = gpu.pixels*4;
  monitor->width = gpu.width;
  monitor->height = gpu.height;

  virtio_gpu_resource_create_2d();
  virtio_gpu_resource_attach_backing();
  virtio_gpu_set_scanout();
  virtio_gpu_transfer_to_host_2d();
  virtio_gpu_resource_flush();


}



void virtio_gpu_intr(){
    lm_lock(&gpu.vgpu_lock);
    printf("virtio gpu interrupt\n");
    // read the device status register to clear the interrupt.
    *R(VIRTIO_MMIO_INTERRUPT_ACK) = *R(VIRTIO_MMIO_INTERRUPT_STATUS) & 0x3;

    __sync_synchronize();

    while(gpu.used_idx[CTRL_Q] != gpu.used[0]->idx){
        __sync_synchronize();
        int id = gpu.used[0]->ring[gpu.used_idx[CTRL_Q] % NUM].id; 
        gpu.info[CTRL_Q][id].pending = 0;
        wakeup(&gpu.info[CTRL_Q][id].pending);

        gpu.used_idx[CTRL_Q]++;
    }
    lm_unlock(&gpu.vgpu_lock);
}