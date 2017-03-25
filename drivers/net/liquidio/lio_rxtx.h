/*
 *   BSD LICENSE
 *
 *   Copyright(c) 2017 Cavium, Inc.. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Cavium, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER(S) OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _LIO_RXTX_H_
#define _LIO_RXTX_H_

#include <stdio.h>
#include <stdint.h>

#include <rte_spinlock.h>
#include <rte_memory.h>

#include "lio_struct.h"

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

static inline void
lio_swap_8B_data(uint64_t *data, uint32_t blocks)
{
	while (blocks) {
		*data = rte_cpu_to_be_64(*data);
		blocks--;
		data++;
	}
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

/** Setup instruction queue zero for the device
 *  @param lio_dev which lio device to setup
 *
 *  @return 0 if success. -1 if fails
 */
int lio_setup_instr_queue0(struct lio_device *lio_dev);
void lio_free_instr_queue0(struct lio_device *lio_dev);
#endif	/* _LIO_RXTX_H_ */
