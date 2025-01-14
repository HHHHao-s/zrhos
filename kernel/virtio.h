#include "types.h"
//
// virtio device definitions.
// for both the mmio interface, and virtio descriptors.
// only tested with qemu.
//
// the virtio spec:
// https://docs.oasis-open.org/virtio/virtio/v1.1/virtio-v1.1.pdf
//

// virtio mmio control registers, mapped starting at 0x10001000.
// from qemu virtio_mmio.h
#define VIRTIO_MMIO_MAGIC_VALUE		0x000 // 0x74726976
#define VIRTIO_MMIO_VERSION		0x004 // version; should 2
#define VIRTIO_MMIO_DEVICE_ID		0x008 // device type; 1 is net, 2 is disk, 16 is gpu
#define VIRTIO_MMIO_VENDOR_ID		0x00c // 0x554d4551
#define VIRTIO_MMIO_DEVICE_FEATURES	0x010
#define VIRTIO_MMIO_DRIVER_FEATURES	0x020
#define VIRTIO_MMIO_QUEUE_SEL		0x030 // select queue, write-only
#define VIRTIO_MMIO_QUEUE_NUM_MAX	0x034 // max size of current queue, read-only
#define VIRTIO_MMIO_QUEUE_NUM		0x038 // size of current queue, write-only
#define VIRTIO_MMIO_QUEUE_READY		0x044 // ready bit
#define VIRTIO_MMIO_QUEUE_NOTIFY	0x050 // write-only
#define VIRTIO_MMIO_INTERRUPT_STATUS	0x060 // read-only
#define VIRTIO_MMIO_INTERRUPT_ACK	0x064 // write-only
#define VIRTIO_MMIO_STATUS		0x070 // read/write
#define VIRTIO_MMIO_QUEUE_DESC_LOW	0x080 // physical address for descriptor table, write-only
#define VIRTIO_MMIO_QUEUE_DESC_HIGH	0x084
#define VIRTIO_MMIO_DRIVER_DESC_LOW	0x090 // physical address for available ring, write-only
#define VIRTIO_MMIO_DRIVER_DESC_HIGH	0x094
#define VIRTIO_MMIO_DEVICE_DESC_LOW	0x0a0 // physical address for used ring, write-only
#define VIRTIO_MMIO_DEVICE_DESC_HIGH	0x0a4

// status register bits, from qemu virtio_config.h
#define VIRTIO_CONFIG_S_ACKNOWLEDGE	1
#define VIRTIO_CONFIG_S_DRIVER		2
#define VIRTIO_CONFIG_S_DRIVER_OK	4
#define VIRTIO_CONFIG_S_FEATURES_OK	8

// device feature bits
#define VIRTIO_BLK_F_RO              5	/* Disk is read-only */
#define VIRTIO_BLK_F_SCSI            7	/* Supports scsi command passthru */
#define VIRTIO_BLK_F_CONFIG_WCE     11	/* Writeback mode available in config */
#define VIRTIO_BLK_F_MQ             12	/* support more than one vq */

#define VIRTIO_GPU_F_VIRGL          0   /* virgl 3D mode is supported */
#define VIRTIO_GPU_F_EDID           1 /* EDID is supported. */

#define VIRTIO_F_ANY_LAYOUT         27
#define VIRTIO_RING_F_INDIRECT_DESC 28
#define VIRTIO_RING_F_EVENT_IDX     29



// this many virtio descriptors.
// must be a power of two.
#define NUM 8

// a single descriptor, from the spec.
struct virtq_desc {
  uint64_t addr;
  uint32_t len;
  uint16_t flags;
  uint16_t next;
};
#define VRING_DESC_F_NEXT  1 // chained with another descriptor
#define VRING_DESC_F_WRITE 2 // device writes (vs read)

// the (entire) avail ring, from the spec.
struct virtq_avail {
  uint16_t flags; // always zero
  uint16_t idx;   // driver will write ring[idx] next
  uint16_t ring[NUM]; // descriptor numbers of chain heads
  uint16_t unused;
};

// one entry in the "used" ring, with which the
// device tells the driver about completed requests.
struct virtq_used_elem {
  uint32_t id;   // index of start of completed descriptor chain
  uint32_t len;
};

struct virtq_used {
  uint16_t flags; // always zero
  uint16_t idx;   // device increments when it adds a ring[] entry
  struct virtq_used_elem ring[NUM];
};

// these are specific to virtio block devices, e.g. disks,
// described in Section 5.2 of the spec.

#define VIRTIO_BLK_T_IN  0 // read the disk
#define VIRTIO_BLK_T_OUT 1 // write the disk

// the format of the first descriptor in a disk request.
// to be followed by two more descriptors containing
// the block, and a one-byte status.
struct virtio_blk_req {
  uint32_t type; // VIRTIO_BLK_T_IN or ..._OUT
  uint32_t reserved;
  uint64_t sector;
};

// gpu device definitions

#define VIRTIO_GPU_MAX_SCANOUTS  16

struct virtio_gpu_config {
    uint32_t events_read;
    uint32_t events_clear;
    uint32_t num_scanouts;
    uint32_t reserved; 
};

struct virtio_gpu_ctrl_hdr {
    uint32_t type;
    uint32_t flags;
    uint64_t fence_id;
    uint32_t ctx_id;
    uint32_t padding;
};

enum virtio_gpu_ctrl_type {
  VIRTIO_GPU_CMD_GET_DISPLAY_INFO = 0x0100,
  VIRTIO_GPU_CMD_RESOURCE_CREATE_2D,
  VIRTIO_GPU_CMD_RESOURCE_UNREF,
  VIRTIO_GPU_CMD_SET_SCANOUT,
  VIRTIO_GPU_CMD_RESOURCE_FLUSH,
  VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D,
  VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING,
  VIRTIO_GPU_CMD_RESOURCE_DETACH_BACKING,
  VIRTIO_GPU_CMD_GET_CAPSET_INFO,
  VIRTIO_GPU_CMD_GET_CAPSET,
  VIRTIO_GPU_CMD_GET_EDID,
  /* cursor commands */
  VIRTIO_GPU_CMD_UPDATE_CURSOR = 0x0300,
  VIRTIO_GPU_CMD_MOVE_CURSOR,
  /* success responses */
  VIRTIO_GPU_RESP_OK_NODATA = 0x1100,
  VIRTIO_GPU_RESP_OK_DISPLAY_INFO,
  VIRTIO_GPU_RESP_OK_CAPSET_INFO,
  VIRTIO_GPU_RESP_OK_CAPSET,
  VIRTIO_GPU_RESP_OK_EDID,
  /* error responses */
  VIRTIO_GPU_RESP_ERR_UNSPEC = 0x1200,
  VIRTIO_GPU_RESP_ERR_OUT_OF_MEMORY,
  VIRTIO_GPU_RESP_ERR_INVALID_SCANOUT_ID,
  VIRTIO_GPU_RESP_ERR_INVALID_RESOURCE_ID,
  VIRTIO_GPU_RESP_ERR_INVALID_CONTEXT_ID,
  VIRTIO_GPU_RESP_ERR_INVALID_PARAMETER,
  
};

#define VIRTIO_GPU_FLAG_FENCE (1 << 0)

struct virtio_gpu_rect {
  uint32_t x;
  uint32_t y;
  uint32_t width;
  uint32_t height;
};

struct virtio_gpu_resp_display_info {
  struct virtio_gpu_ctrl_hdr hdr;
  struct virtio_gpu_display_one {
    struct virtio_gpu_rect r;
    uint32_t enabled;
    uint32_t flags;
  } pmodes[VIRTIO_GPU_MAX_SCANOUTS];
};

enum virtio_gpu_formats {
  VIRTIO_GPU_FORMAT_B8G8R8A8_UNORM = 1,
  VIRTIO_GPU_FORMAT_B8G8R8X8_UNORM = 2,
  VIRTIO_GPU_FORMAT_A8R8G8B8_UNORM = 3,
  VIRTIO_GPU_FORMAT_X8R8G8B8_UNORM = 4,
  VIRTIO_GPU_FORMAT_R8G8B8A8_UNORM = 67,
  VIRTIO_GPU_FORMAT_X8B8G8R8_UNORM = 68,
  VIRTIO_GPU_FORMAT_A8B8G8R8_UNORM = 121,
  VIRTIO_GPU_FORMAT_R8G8B8X8_UNORM = 134,
};

struct virtio_gpu_resource_create_2d {
  struct virtio_gpu_ctrl_hdr hdr;
  uint32_t resource_id;
  uint32_t format;
  uint32_t width;
  uint32_t height;
};