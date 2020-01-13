/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#ifndef _OTX2_EP_RAWDEV_H_
#define _OTX2_EP_RAWDEV_H_

#include <rte_byteorder.h>
#include <rte_spinlock.h>

/* IQ instruction req types */
#define SDP_REQTYPE_NONE             (0)
#define SDP_REQTYPE_NORESP           (1)
#define SDP_REQTYPE_NORESP_GATHER    (2)

/* Input Request Header format */
struct sdp_instr_irh {
	/* Request ID  */
	uint64_t rid:16;

	/* PCIe port to use for response */
	uint64_t pcie_port:3;

	/* Scatter indicator  1=scatter */
	uint64_t scatter:1;

	/* Size of Expected result OR no. of entries in scatter list */
	uint64_t rlenssz:14;

	/* Desired destination port for result */
	uint64_t dport:6;

	/* Opcode Specific parameters */
	uint64_t param:8;

	/* Opcode for the return packet  */
	uint64_t opcode:16;
};

/* SDP 32B instruction format */
struct sdp_instr_32B {
	/* Pointer where the input data is available. */
	uint64_t dptr;

	/* SDP Instruction Header.  */
	uint64_t ih;

	/** Pointer where the response for a RAW mode packet
	 *  will be written by OCTEON TX2.
	 */
	uint64_t rptr;

	/* Input Request Header. Additional info about the input. */
	uint64_t irh;
};
#define SDP_32B_INSTR_SIZE	(sizeof(sdp_instr_32B))

/* SDP 64B instruction format */
struct sdp_instr_64B {
	/* Pointer where the input data is available. */
	uint64_t dptr;

	/* SDP Instruction Header. */
	uint64_t ih;

	/** Pointer where the response for a RAW mode packet
	 * will be written by OCTEON TX2.
	 */
	uint64_t rptr;

	/* Input Request Header. */
	uint64_t irh;

	/* Additional headers available in a 64-byte instruction. */
	uint64_t exhdr[4];
};
#define SDP_64B_INSTR_SIZE	(sizeof(sdp_instr_64B))

struct sdp_soft_instr {
	/** Input data pointer. It is either pointing directly to input data
	 *  or to a gather list.
	 */
	void *dptr;

	/** Response from OCTEON TX2 comes at this address. It is either
	 *  directlty pointing to output data buffer or to a scatter list.
	 */
	void *rptr;

	/* The instruction header. All input commands have this field. */
	struct sdp_instr_ih ih;

	/* Input request header. */
	struct sdp_instr_irh irh;

	/** The PCI instruction to be sent to OCTEON TX2. This is stored in the
	 *  instr to retrieve the physical address of buffers when instr is
	 *  freed.
	 */
	struct sdp_instr_64B command;

	/** If a gather list was allocated, this ptr points to the buffer used
	 *  for the gather list. The gather list has to be 8B aligned, so this
	 *  value may be different from dptr.
	 */
	void *gather_ptr;

	/* Total data bytes transferred in the gather mode request. */
	uint64_t gather_bytes;

	/** If a scatter list was allocated, this ptr points to the buffer used
	 *  for the scatter list. The scatter list has to be 8B aligned, so
	 *  this value may be different from rptr.
	 */
	void *scatter_ptr;

	/* Total data bytes to be received in the scatter mode request. */
	uint64_t scatter_bytes;

	/* IQ number to which this instruction has to be submitted. */
	uint32_t q_no;

	/* IQ instruction request type. */
	uint32_t reqtype;
};
#define SDP_SOFT_INSTR_SIZE	(sizeof(sdp_soft_instr))

/* SDP IQ request list */
struct sdp_instr_list {
	void *buf;
	uint32_t reqtype;
};
#define SDP_IQREQ_LIST_SIZE	(sizeof(struct sdp_instr_list))

/* Input Queue statistics. Each input queue has four stats fields. */
struct sdp_iq_stats {
	uint64_t instr_posted; /* Instructions posted to this queue. */
	uint64_t instr_processed; /* Instructions processed in this queue. */
	uint64_t instr_dropped; /* Instructions that could not be processed */
};

/* Structure to define the configuration attributes for each Input queue. */
struct sdp_iq_config {
	/* Max number of IQs available */
	uint16_t max_iqs;

	/* Command size - 32 or 64 bytes */
	uint16_t instr_type;

	/* Pending list size, usually set to the sum of the size of all IQs */
	uint32_t pending_list_size;
};

/** The instruction (input) queue.
 *  The input queue is used to post raw (instruction) mode data or packet data
 *  to OCTEON TX2 device from the host. Each IQ of a SDP EP VF device has one
 *  such structure to represent it.
 */
struct sdp_instr_queue {
	/* A spinlock to protect access to the input ring.  */
	rte_spinlock_t lock;
	rte_spinlock_t post_lock;

	struct sdp_device *sdp_dev;
	rte_atomic64_t iq_flush_running;

	uint32_t q_no;
	uint32_t pkt_in_done;

	/* Flag for 64 byte commands. */
	uint32_t iqcmd_64B:1;
	uint32_t rsvd:17;
	uint32_t status:8;

	/* Number of  descriptors in this ring. */
	uint32_t nb_desc;

	/* Input ring index, where the driver should write the next packet */
	uint32_t host_write_index;

	/* Input ring index, where the OCTEON TX2 should read the next packet */
	uint32_t otx_read_index;

	/** This index aids in finding the window in the queue where OCTEON TX2
	 *  has read the commands.
	 */
	uint32_t flush_index;

	/* This keeps track of the instructions pending in this queue. */
	rte_atomic64_t instr_pending;

	uint32_t reset_instr_cnt;

	/* Pointer to the Virtual Base addr of the input ring. */
	uint8_t *base_addr;

	/* This IQ request list */
	struct sdp_instr_list *req_list;

	/* SDP doorbell register for the ring. */
	void *doorbell_reg;

	/* SDP instruction count register for this ring. */
	void *inst_cnt_reg;

	/* Number of instructions pending to be posted to OCTEON TX2. */
	uint32_t fill_cnt;

	/* Statistics for this input queue. */
	struct sdp_iq_stats stats;

	/* DMA mapped base address of the input descriptor ring. */
	uint64_t base_addr_dma;

	/* Memory zone */
	const struct rte_memzone *iq_mz;
};

/* DROQ packet format for application i/f. */
struct sdp_droq_pkt {
	/* DROQ packet data buffer pointer. */
	uint8_t	 *data;

	/* DROQ packet data length */
	uint32_t len;

	uint32_t misc;
};

/** Descriptor format.
 *  The descriptor ring is made of descriptors which have 2 64-bit values:
 *  -# Physical (bus) address of the data buffer.
 *  -# Physical (bus) address of a sdp_droq_info structure.
 *  The device DMA's incoming packets and its information at the address
 *  given by these descriptor fields.
 */
struct sdp_droq_desc {
	/* The buffer pointer */
	uint64_t buffer_ptr;

	/* The Info pointer */
	uint64_t info_ptr;
};
#define SDP_DROQ_DESC_SIZE	(sizeof(struct sdp_droq_desc))

/* Receive Header */
union sdp_rh {
	uint64_t rh64;
};
#define SDP_RH_SIZE (sizeof(union sdp_rh))

/** Information about packet DMA'ed by OCTEON TX2.
 *  The format of the information available at Info Pointer after OCTEON TX2
 *  has posted a packet. Not all descriptors have valid information. Only
 *  the Info field of the first descriptor for a packet has information
 *  about the packet.
 */
struct sdp_droq_info {
	/* The Output Receive Header. */
	union sdp_rh rh;

	/* The Length of the packet. */
	uint64_t length;
};
#define SDP_DROQ_INFO_SIZE	(sizeof(struct sdp_droq_info))

/** Pointer to data buffer.
 *  Driver keeps a pointer to the data buffer that it made available to
 *  the OCTEON TX2 device. Since the descriptor ring keeps physical (bus)
 *  addresses, this field is required for the driver to keep track of
 *  the virtual address pointers.
 */
struct sdp_recv_buffer {
	/* Packet buffer, including meta data. */
	void *buffer;

	/* Data in the packet buffer. */
	/* uint8_t *data; */
};
#define SDP_DROQ_RECVBUF_SIZE	(sizeof(struct sdp_recv_buffer))

/* DROQ statistics. Each output queue has four stats fields. */
struct sdp_droq_stats {
	/* Number of packets received in this queue. */
	uint64_t pkts_received;

	/* Bytes received by this queue. */
	uint64_t bytes_received;

	/* Num of failures of rte_pktmbuf_alloc() */
	uint64_t rx_alloc_failure;
};

/* Structure to define the configuration attributes for each Output queue. */
struct sdp_oq_config {
	/* Max number of OQs available */
	uint16_t max_oqs;

	/* If set, the Output queue uses info-pointer mode. (Default: 1 ) */
	uint16_t info_ptr;

	/** The number of buffers that were consumed during packet processing by
	 *  the driver on this Output queue before the driver attempts to
	 *  replenish the descriptor ring with new buffers.
	 */
	uint32_t refill_threshold;
};

/* The Descriptor Ring Output Queue(DROQ) structure. */
struct sdp_droq {
	/* A spinlock to protect access to this ring. */
	rte_spinlock_t lock;

	struct sdp_device *sdp_dev;
	/* The 8B aligned descriptor ring starts at this address. */
	struct sdp_droq_desc *desc_ring;

	uint32_t q_no;
	uint32_t last_pkt_count;

	/* Driver should read the next packet at this index */
	uint32_t read_idx;

	/* OCTEON TX2 will write the next packet at this index */
	uint32_t write_idx;

	/* At this index, the driver will refill the descriptor's buffer */
	uint32_t refill_idx;

	/* Packets pending to be processed */
	rte_atomic64_t pkts_pending;

	/* Number of descriptors in this ring. */
	uint32_t nb_desc;

	/* The number of descriptors pending to refill. */
	uint32_t refill_count;

	uint32_t refill_threshold;

	/* The 8B aligned info ptrs begin from this address. */
	struct sdp_droq_info *info_list;

	/* receive buffer list contains virtual addresses of the buffers. */
	struct sdp_recv_buffer *recv_buf_list;

	/* The size of each buffer pointed by the buffer pointer. */
	uint32_t buffer_size;

	/** Pointer to the mapped packet credit register.
	 *  Host writes number of info/buffer ptrs available to this register
	 */
	void *pkts_credit_reg;

	/** Pointer to the mapped packet sent register. OCTEON TX2 writes the
	 *  number of packets DMA'ed to host memory in this register.
	 */
	void *pkts_sent_reg;

	/* Statistics for this DROQ. */
	struct sdp_droq_stats stats;

	/* DMA mapped address of the DROQ descriptor ring. */
	size_t desc_ring_dma;

	/* Info_ptr list is allocated at this virtual address. */
	size_t info_base_addr;

	/* DMA mapped address of the info list */
	size_t info_list_dma;

	/* Allocated size of info list. */
	uint32_t info_alloc_size;

	/* Memory zone **/
	const struct rte_memzone *desc_ring_mz;
	const struct rte_memzone *info_mz;
};
#define SDP_DROQ_SIZE		(sizeof(struct sdp_droq))

/* IQ/OQ mask */
struct sdp_io_enable {
	uint64_t iq;
	uint64_t oq;
	uint64_t iq64B;
};

/* Structure to define the configuration. */
struct sdp_config {
	/* Input Queue attributes. */
	struct sdp_iq_config iq;

	/* Output Queue attributes. */
	struct sdp_oq_config oq;

	/* Num of desc for IQ rings */
	uint32_t num_iqdef_descs;

	/* Num of desc for OQ rings */
	uint32_t num_oqdef_descs;

	/* OQ buffer size */
	uint32_t oqdef_buf_size;
};

/* Required functions for each VF device */
struct sdp_fn_list {
	void (*setup_iq_regs)(struct sdp_device *sdpvf, uint32_t q_no);
	void (*setup_oq_regs)(struct sdp_device *sdpvf, uint32_t q_no);

	int (*setup_device_regs)(struct sdp_device *sdpvf);
	uint32_t (*update_iq_read_idx)(struct sdp_instr_queue *iq);

	void (*enable_io_queues)(struct sdp_device *sdpvf);
	void (*disable_io_queues)(struct sdp_device *sdpvf);

	void (*enable_iq)(struct sdp_device *sdpvf, uint32_t q_no);
	void (*disable_iq)(struct sdp_device *sdpvf, uint32_t q_no);

	void (*enable_oq)(struct sdp_device *sdpvf, uint32_t q_no);
	void (*disable_oq)(struct sdp_device *sdpvf, uint32_t q_no);
};

/* SRIOV information */
struct sdp_sriov_info {
	/* Number of rings assigned to VF */
	uint32_t rings_per_vf;

	/* Number of VF devices enabled */
	uint32_t num_vfs;
};


/* Information to be passed from application */
struct sdp_rawdev_info {
	struct rte_mempool *enqdeq_mpool;
	const struct sdp_config *app_conf;
};

/* SDP EP VF device */
struct sdp_device {
	/* PCI device pointer */
	struct rte_pci_device *pci_dev;
	uint16_t chip_id;
	uint16_t pf_num;
	uint16_t vf_num;

	/* This device's PCIe port used for traffic. */
	uint16_t pcie_port;
	uint32_t pkind;

	/* The state of this device */
	rte_atomic64_t status;

	/* Memory mapped h/w address */
	uint8_t *hw_addr;

	struct sdp_fn_list fn_list;

	/* Num IQs */
	uint32_t num_iqs;

	/* The input instruction queues */
	struct sdp_instr_queue *instr_queue[SDP_VF_MAX_IOQS_PER_RAWDEV];

	/* Num OQs */
	uint32_t num_oqs;

	/* The DROQ output queues  */
	struct sdp_droq *droq[SDP_VF_MAX_IOQS_PER_RAWDEV];

	/* IOQ data buffer pool */
	struct rte_mempool *enqdeq_mpool;

	/* IOQ mask */
	struct sdp_io_enable io_qmask;

	/* SR-IOV info */
	struct sdp_sriov_info sriov_info;

	/* Device configuration */
	const struct sdp_config *conf;
};

const struct sdp_config *sdp_get_defconf(struct sdp_device *sdp_dev);
int sdp_setup_iqs(struct sdp_device *sdpvf, uint32_t iq_no);
int sdp_delete_iqs(struct sdp_device *sdpvf, uint32_t iq_no);

int sdp_setup_oqs(struct sdp_device *sdpvf, uint32_t oq_no);
int sdp_delete_oqs(struct sdp_device *sdpvf, uint32_t oq_no);

int sdp_rawdev_enqueue(struct rte_rawdev *dev, struct rte_rawdev_buf **buffers,
		       unsigned int count, rte_rawdev_obj_t context);
int sdp_rawdev_dequeue(struct rte_rawdev *dev, struct rte_rawdev_buf **buffers,
		       unsigned int count, rte_rawdev_obj_t context);

int sdp_rawdev_selftest(uint16_t dev_id);

#endif /* _OTX2_EP_RAWDEV_H_ */
