/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Cavium, Inc
 */

#ifndef _LIO_RXTX_H_
#define _LIO_RXTX_H_

#include <stdio.h>
#include <stdint.h>

#include <rte_spinlock.h>
#include <rte_memory.h>

#include "lio_struct.h"

#ifndef ROUNDUP4
#define ROUNDUP4(val) (((val) + 3) & 0xfffffffc)
#endif

#define LIO_STQUEUE_FIRST_ENTRY(ptr, type, elem)	\
	(type *)((char *)((ptr)->stqh_first) - offsetof(type, elem))

#define lio_check_timeout(cur_time, chk_time) ((cur_time) > (chk_time))

#define lio_uptime		\
	(size_t)(rte_get_timer_cycles() / rte_get_timer_hz())

/** Descriptor format.
 *  The descriptor ring is made of descriptors which have 2 64-bit values:
 *  -# Physical (bus) address of the data buffer.
 *  -# Physical (bus) address of a lio_droq_info structure.
 *  The device DMA's incoming packets and its information at the address
 *  given by these descriptor fields.
 */
struct lio_droq_desc {
	/** The buffer pointer */
	uint64_t buffer_ptr;

	/** The Info pointer */
	uint64_t info_ptr;
};

#define LIO_DROQ_DESC_SIZE	(sizeof(struct lio_droq_desc))

/** Information about packet DMA'ed by Octeon.
 *  The format of the information available at Info Pointer after Octeon
 *  has posted a packet. Not all descriptors have valid information. Only
 *  the Info field of the first descriptor for a packet has information
 *  about the packet.
 */
struct lio_droq_info {
	/** The Output Receive Header. */
	union octeon_rh rh;

	/** The Length of the packet. */
	uint64_t length;
};

#define LIO_DROQ_INFO_SIZE	(sizeof(struct lio_droq_info))

/** Pointer to data buffer.
 *  Driver keeps a pointer to the data buffer that it made available to
 *  the Octeon device. Since the descriptor ring keeps physical (bus)
 *  addresses, this field is required for the driver to keep track of
 *  the virtual address pointers.
 */
struct lio_recv_buffer {
	/** Packet buffer, including meta data. */
	void *buffer;

	/** Data in the packet buffer. */
	uint8_t *data;

};

#define LIO_DROQ_RECVBUF_SIZE	(sizeof(struct lio_recv_buffer))

#define LIO_DROQ_SIZE		(sizeof(struct lio_droq))

#define LIO_IQ_SEND_OK		0
#define LIO_IQ_SEND_STOP	1
#define LIO_IQ_SEND_FAILED	-1

/* conditions */
#define LIO_REQTYPE_NONE		0
#define LIO_REQTYPE_NORESP_NET		1
#define LIO_REQTYPE_NORESP_NET_SG	2
#define LIO_REQTYPE_SOFT_COMMAND	3

struct lio_request_list {
	uint32_t reqtype;
	void *buf;
};

/*----------------------  INSTRUCTION FORMAT ----------------------------*/

struct lio_instr3_64B {
	/** Pointer where the input data is available. */
	uint64_t dptr;

	/** Instruction Header. */
	uint64_t ih3;

	/** Instruction Header. */
	uint64_t pki_ih3;

	/** Input Request Header. */
	uint64_t irh;

	/** opcode/subcode specific parameters */
	uint64_t ossp[2];

	/** Return Data Parameters */
	uint64_t rdp;

	/** Pointer where the response for a RAW mode packet will be written
	 *  by Octeon.
	 */
	uint64_t rptr;

};

union lio_instr_64B {
	struct lio_instr3_64B cmd3;
};

/** The size of each buffer in soft command buffer pool */
#define LIO_SOFT_COMMAND_BUFFER_SIZE	1536

/** Maximum number of buffers to allocate into soft command buffer pool */
#define LIO_MAX_SOFT_COMMAND_BUFFERS	255

struct lio_soft_command {
	/** Soft command buffer info. */
	struct lio_stailq_node node;
	uint64_t dma_addr;
	uint32_t size;

	/** Command and return status */
	union lio_instr_64B cmd;

#define LIO_COMPLETION_WORD_INIT	0xffffffffffffffffULL
	uint64_t *status_word;

	/** Data buffer info */
	void *virtdptr;
	uint64_t dmadptr;
	uint32_t datasize;

	/** Return buffer info */
	void *virtrptr;
	uint64_t dmarptr;
	uint32_t rdatasize;

	/** Context buffer info */
	void *ctxptr;
	uint32_t ctxsize;

	/** Time out and callback */
	size_t wait_time;
	size_t timeout;
	uint32_t iq_no;
	void (*callback)(uint32_t, void *);
	void *callback_arg;
	struct rte_mbuf *mbuf;
};

struct lio_iq_post_status {
	int status;
	int index;
};

/*   wqe
 *  ---------------  0
 * |  wqe  word0-3 |
 *  ---------------  32
 * |    PCI IH     |
 *  ---------------  40
 * |     RPTR      |
 *  ---------------  48
 * |    PCI IRH    |
 *  ---------------  56
 * |    OCTEON_CMD |
 *  ---------------  64
 * | Addtl 8-BData |
 * |               |
 *  ---------------
 */

union octeon_cmd {
	uint64_t cmd64;

	struct	{
#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
		uint64_t cmd : 5;

		uint64_t more : 6; /* How many udd words follow the command */

		uint64_t reserved : 29;

		uint64_t param1 : 16;

		uint64_t param2 : 8;

#elif RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN

		uint64_t param2 : 8;

		uint64_t param1 : 16;

		uint64_t reserved : 29;

		uint64_t more : 6;

		uint64_t cmd : 5;

#endif
	} s;
};

#define OCTEON_CMD_SIZE (sizeof(union octeon_cmd))

/* Maximum number of 8-byte words can be
 * sent in a NIC control message.
 */
#define LIO_MAX_NCTRL_UDD	32

/* Structure of control information passed by driver to the BASE
 * layer when sending control commands to Octeon device software.
 */
struct lio_ctrl_pkt {
	/** Command to be passed to the Octeon device software. */
	union octeon_cmd ncmd;

	/** Send buffer */
	void *data;
	uint64_t dmadata;

	/** Response buffer */
	void *rdata;
	uint64_t dmardata;

	/** Additional data that may be needed by some commands. */
	uint64_t udd[LIO_MAX_NCTRL_UDD];

	/** Input queue to use to send this command. */
	uint64_t iq_no;

	/** Time to wait for Octeon software to respond to this control command.
	 *  If wait_time is 0, BASE assumes no response is expected.
	 */
	size_t wait_time;

	struct lio_dev_ctrl_cmd *ctrl_cmd;
};

/** Structure of data information passed by driver to the BASE
 *  layer when forwarding data to Octeon device software.
 */
struct lio_data_pkt {
	/** Pointer to information maintained by NIC module for this packet. The
	 *  BASE layer passes this as-is to the driver.
	 */
	void *buf;

	/** Type of buffer passed in "buf" above. */
	uint32_t reqtype;

	/** Total data bytes to be transferred in this command. */
	uint32_t datasize;

	/** Command to be passed to the Octeon device software. */
	union lio_instr_64B cmd;

	/** Input queue to use to send this command. */
	uint32_t q_no;
};

/** Structure passed by driver to BASE layer to prepare a command to send
 *  network data to Octeon.
 */
union lio_cmd_setup {
	struct {
		uint32_t iq_no : 8;
		uint32_t gather : 1;
		uint32_t timestamp : 1;
		uint32_t ip_csum : 1;
		uint32_t transport_csum : 1;
		uint32_t tnl_csum : 1;
		uint32_t rsvd : 19;

		union {
			uint32_t datasize;
			uint32_t gatherptrs;
		} u;
	} s;

	uint64_t cmd_setup64;
};

/* Instruction Header */
struct octeon_instr_ih3 {
#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN

	/** Reserved3 */
	uint64_t reserved3 : 1;

	/** Gather indicator 1=gather*/
	uint64_t gather : 1;

	/** Data length OR no. of entries in gather list */
	uint64_t dlengsz : 14;

	/** Front Data size */
	uint64_t fsz : 6;

	/** Reserved2 */
	uint64_t reserved2 : 4;

	/** PKI port kind - PKIND */
	uint64_t pkind : 6;

	/** Reserved1 */
	uint64_t reserved1 : 32;

#elif RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN
	/** Reserved1 */
	uint64_t reserved1 : 32;

	/** PKI port kind - PKIND */
	uint64_t pkind : 6;

	/** Reserved2 */
	uint64_t reserved2 : 4;

	/** Front Data size */
	uint64_t fsz : 6;

	/** Data length OR no. of entries in gather list */
	uint64_t dlengsz : 14;

	/** Gather indicator 1=gather*/
	uint64_t gather : 1;

	/** Reserved3 */
	uint64_t reserved3 : 1;

#endif
};

/* PKI Instruction Header(PKI IH) */
struct octeon_instr_pki_ih3 {
#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN

	/** Wider bit */
	uint64_t w : 1;

	/** Raw mode indicator 1 = RAW */
	uint64_t raw : 1;

	/** Use Tag */
	uint64_t utag : 1;

	/** Use QPG */
	uint64_t uqpg : 1;

	/** Reserved2 */
	uint64_t reserved2 : 1;

	/** Parse Mode */
	uint64_t pm : 3;

	/** Skip Length */
	uint64_t sl : 8;

	/** Use Tag Type */
	uint64_t utt : 1;

	/** Tag type */
	uint64_t tagtype : 2;

	/** Reserved1 */
	uint64_t reserved1 : 2;

	/** QPG Value */
	uint64_t qpg : 11;

	/** Tag Value */
	uint64_t tag : 32;

#elif RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN

	/** Tag Value */
	uint64_t tag : 32;

	/** QPG Value */
	uint64_t qpg : 11;

	/** Reserved1 */
	uint64_t reserved1 : 2;

	/** Tag type */
	uint64_t tagtype : 2;

	/** Use Tag Type */
	uint64_t utt : 1;

	/** Skip Length */
	uint64_t sl : 8;

	/** Parse Mode */
	uint64_t pm : 3;

	/** Reserved2 */
	uint64_t reserved2 : 1;

	/** Use QPG */
	uint64_t uqpg : 1;

	/** Use Tag */
	uint64_t utag : 1;

	/** Raw mode indicator 1 = RAW */
	uint64_t raw : 1;

	/** Wider bit */
	uint64_t w : 1;
#endif
};

/** Input Request Header */
struct octeon_instr_irh {
#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
	uint64_t opcode : 4;
	uint64_t rflag : 1;
	uint64_t subcode : 7;
	uint64_t vlan : 12;
	uint64_t priority : 3;
	uint64_t reserved : 5;
	uint64_t ossp : 32; /* opcode/subcode specific parameters */
#elif RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN
	uint64_t ossp : 32; /* opcode/subcode specific parameters */
	uint64_t reserved : 5;
	uint64_t priority : 3;
	uint64_t vlan : 12;
	uint64_t subcode : 7;
	uint64_t rflag : 1;
	uint64_t opcode : 4;
#endif
};

/* pkiih3 + irh + ossp[0] + ossp[1] + rdp + rptr = 40 bytes */
#define OCTEON_SOFT_CMD_RESP_IH3	(40 + 8)
/* pki_h3 + irh + ossp[0] + ossp[1] = 32 bytes */
#define OCTEON_PCI_CMD_O3		(24 + 8)

/** Return Data Parameters */
struct octeon_instr_rdp {
#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
	uint64_t reserved : 49;
	uint64_t pcie_port : 3;
	uint64_t rlen : 12;
#elif RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN
	uint64_t rlen : 12;
	uint64_t pcie_port : 3;
	uint64_t reserved : 49;
#endif
};

union octeon_packet_params {
	uint32_t pkt_params32;
	struct {
#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
		uint32_t reserved : 24;
		uint32_t ip_csum : 1; /* Perform IP header checksum(s) */
		/* Perform Outer transport header checksum */
		uint32_t transport_csum : 1;
		/* Find tunnel, and perform transport csum. */
		uint32_t tnl_csum : 1;
		uint32_t tsflag : 1;   /* Timestamp this packet */
		uint32_t ipsec_ops : 4; /* IPsec operation */
#else
		uint32_t ipsec_ops : 4;
		uint32_t tsflag : 1;
		uint32_t tnl_csum : 1;
		uint32_t transport_csum : 1;
		uint32_t ip_csum : 1;
		uint32_t reserved : 7;
#endif
	} s;
};

/** Utility function to prepare a 64B NIC instruction based on a setup command
 * @param cmd - pointer to instruction to be filled in.
 * @param setup - pointer to the setup structure
 * @param q_no - which queue for back pressure
 *
 * Assumes the cmd instruction is pre-allocated, but no fields are filled in.
 */
static inline void
lio_prepare_pci_cmd(struct lio_device *lio_dev,
		    union lio_instr_64B *cmd,
		    union lio_cmd_setup *setup,
		    uint32_t tag)
{
	union octeon_packet_params packet_params;
	struct octeon_instr_pki_ih3 *pki_ih3;
	struct octeon_instr_irh *irh;
	struct octeon_instr_ih3 *ih3;
	int port;

	memset(cmd, 0, sizeof(union lio_instr_64B));

	ih3 = (struct octeon_instr_ih3 *)&cmd->cmd3.ih3;
	pki_ih3 = (struct octeon_instr_pki_ih3 *)&cmd->cmd3.pki_ih3;

	/* assume that rflag is cleared so therefore front data will only have
	 * irh and ossp[1] and ossp[2] for a total of 24 bytes
	 */
	ih3->pkind = lio_dev->instr_queue[setup->s.iq_no]->txpciq.s.pkind;
	/* PKI IH */
	ih3->fsz = OCTEON_PCI_CMD_O3;

	if (!setup->s.gather) {
		ih3->dlengsz = setup->s.u.datasize;
	} else {
		ih3->gather = 1;
		ih3->dlengsz = setup->s.u.gatherptrs;
	}

	pki_ih3->w = 1;
	pki_ih3->raw = 0;
	pki_ih3->utag = 0;
	pki_ih3->utt = 1;
	pki_ih3->uqpg = lio_dev->instr_queue[setup->s.iq_no]->txpciq.s.use_qpg;

	port = (int)lio_dev->instr_queue[setup->s.iq_no]->txpciq.s.port;

	if (tag)
		pki_ih3->tag = tag;
	else
		pki_ih3->tag = LIO_DATA(port);

	pki_ih3->tagtype = OCTEON_ORDERED_TAG;
	pki_ih3->qpg = lio_dev->instr_queue[setup->s.iq_no]->txpciq.s.qpg;
	pki_ih3->pm = 0x0; /* parse from L2 */
	pki_ih3->sl = 32;  /* sl will be sizeof(pki_ih3) + irh + ossp0 + ossp1*/

	irh = (struct octeon_instr_irh *)&cmd->cmd3.irh;

	irh->opcode = LIO_OPCODE;
	irh->subcode = LIO_OPCODE_NW_DATA;

	packet_params.pkt_params32 = 0;
	packet_params.s.ip_csum = setup->s.ip_csum;
	packet_params.s.transport_csum = setup->s.transport_csum;
	packet_params.s.tnl_csum = setup->s.tnl_csum;
	packet_params.s.tsflag = setup->s.timestamp;

	irh->ossp = packet_params.pkt_params32;
}

int lio_setup_sc_buffer_pool(struct lio_device *lio_dev);
void lio_free_sc_buffer_pool(struct lio_device *lio_dev);

struct lio_soft_command *
lio_alloc_soft_command(struct lio_device *lio_dev,
		       uint32_t datasize, uint32_t rdatasize,
		       uint32_t ctxsize);
void lio_prepare_soft_command(struct lio_device *lio_dev,
			      struct lio_soft_command *sc,
			      uint8_t opcode, uint8_t subcode,
			      uint32_t irh_ossp, uint64_t ossp0,
			      uint64_t ossp1);
int lio_send_soft_command(struct lio_device *lio_dev,
			  struct lio_soft_command *sc);
void lio_free_soft_command(struct lio_soft_command *sc);

/** Send control packet to the device
 *  @param lio_dev - lio device pointer
 *  @param nctrl   - control structure with command, timeout, and callback info
 *
 *  @returns IQ_FAILED if it failed to add to the input queue. IQ_STOP if it the
 *  queue should be stopped, and LIO_IQ_SEND_OK if it sent okay.
 */
int lio_send_ctrl_pkt(struct lio_device *lio_dev,
		      struct lio_ctrl_pkt *ctrl_pkt);

/** Maximum ordered requests to process in every invocation of
 *  lio_process_ordered_list(). The function will continue to process requests
 *  as long as it can find one that has finished processing. If it keeps
 *  finding requests that have completed, the function can run for ever. The
 *  value defined here sets an upper limit on the number of requests it can
 *  process before it returns control to the poll thread.
 */
#define LIO_MAX_ORD_REQS_TO_PROCESS	4096

/** Error codes used in Octeon Host-Core communication.
 *
 *   31		16 15		0
 *   ----------------------------
 * |		|		|
 *   ----------------------------
 *   Error codes are 32-bit wide. The upper 16-bits, called Major Error Number,
 *   are reserved to identify the group to which the error code belongs. The
 *   lower 16-bits, called Minor Error Number, carry the actual code.
 *
 *   So error codes are (MAJOR NUMBER << 16)| MINOR_NUMBER.
 */
/** Status for a request.
 *  If the request is successfully queued, the driver will return
 *  a LIO_REQUEST_PENDING status. LIO_REQUEST_TIMEOUT is only returned by
 *  the driver if the response for request failed to arrive before a
 *  time-out period or if the request processing * got interrupted due to
 *  a signal respectively.
 */
enum {
	/** A value of 0x00000000 indicates no error i.e. success */
	LIO_REQUEST_DONE	= 0x00000000,
	/** (Major number: 0x0000; Minor Number: 0x0001) */
	LIO_REQUEST_PENDING	= 0x00000001,
	LIO_REQUEST_TIMEOUT	= 0x00000003,

};

/*------ Error codes used by firmware (bits 15..0 set by firmware */
#define LIO_FIRMWARE_MAJOR_ERROR_CODE	 0x0001
#define LIO_FIRMWARE_STATUS_CODE(status) \
	((LIO_FIRMWARE_MAJOR_ERROR_CODE << 16) | (status))

/** Initialize the response lists. The number of response lists to create is
 *  given by count.
 *  @param lio_dev - the lio device structure.
 */
void lio_setup_response_list(struct lio_device *lio_dev);

/** Check the status of first entry in the ordered list. If the instruction at
 *  that entry finished processing or has timed-out, the entry is cleaned.
 *  @param lio_dev - the lio device structure.
 *  @return 1 if the ordered list is empty, 0 otherwise.
 */
int lio_process_ordered_list(struct lio_device *lio_dev);

#define LIO_INCR_INSTRQUEUE_PKT_COUNT(lio_dev, iq_no, field, count)	\
	(((lio_dev)->instr_queue[iq_no]->stats.field) += count)

static inline void
lio_swap_8B_data(uint64_t *data, uint32_t blocks)
{
	while (blocks) {
		*data = rte_cpu_to_be_64(*data);
		blocks--;
		data++;
	}
}

static inline uint64_t
lio_map_ring(void *buf)
{
	rte_iova_t dma_addr;

	dma_addr = rte_mbuf_data_iova_default(((struct rte_mbuf *)buf));

	return (uint64_t)dma_addr;
}

static inline uint64_t
lio_map_ring_info(struct lio_droq *droq, uint32_t i)
{
	rte_iova_t dma_addr;

	dma_addr = droq->info_list_dma + (i * LIO_DROQ_INFO_SIZE);

	return (uint64_t)dma_addr;
}

static inline int
lio_opcode_slow_path(union octeon_rh *rh)
{
	uint16_t subcode1, subcode2;

	subcode1 = LIO_OPCODE_SUBCODE(rh->r.opcode, rh->r.subcode);
	subcode2 = LIO_OPCODE_SUBCODE(LIO_OPCODE, LIO_OPCODE_NW_DATA);

	return subcode2 != subcode1;
}

static inline void
lio_add_sg_size(struct lio_sg_entry *sg_entry,
		uint16_t size, uint32_t pos)
{
#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
	sg_entry->u.size[pos] = size;
#elif RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN
	sg_entry->u.size[3 - pos] = size;
#endif
}

/* Macro to increment index.
 * Index is incremented by count; if the sum exceeds
 * max, index is wrapped-around to the start.
 */
static inline uint32_t
lio_incr_index(uint32_t index, uint32_t count, uint32_t max)
{
	if ((index + count) >= max)
		index = index + count - max;
	else
		index += count;

	return index;
}

int lio_setup_droq(struct lio_device *lio_dev, int q_no, int num_descs,
		   int desc_size, struct rte_mempool *mpool,
		   unsigned int socket_id);
uint16_t lio_dev_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts,
			   uint16_t budget);
void lio_delete_droq_queue(struct lio_device *lio_dev, int oq_no);

void lio_delete_sglist(struct lio_instr_queue *txq);
int lio_setup_sglists(struct lio_device *lio_dev, int iq_no,
		      int fw_mapped_iq, int num_descs, unsigned int socket_id);
uint16_t lio_dev_xmit_pkts(void *tx_queue, struct rte_mbuf **pkts,
			   uint16_t nb_pkts);
int lio_wait_for_instr_fetch(struct lio_device *lio_dev);
int lio_setup_iq(struct lio_device *lio_dev, int q_index,
		 union octeon_txpciq iq_no, uint32_t num_descs, void *app_ctx,
		 unsigned int socket_id);
int lio_flush_iq(struct lio_device *lio_dev, struct lio_instr_queue *iq);
void lio_delete_instruction_queue(struct lio_device *lio_dev, int iq_no);
/** Setup instruction queue zero for the device
 *  @param lio_dev which lio device to setup
 *
 *  @return 0 if success. -1 if fails
 */
int lio_setup_instr_queue0(struct lio_device *lio_dev);
void lio_free_instr_queue0(struct lio_device *lio_dev);
void lio_dev_clear_queues(struct rte_eth_dev *eth_dev);
#endif	/* _LIO_RXTX_H_ */
