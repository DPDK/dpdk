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

#ifndef _LIO_STRUCT_H_
#define _LIO_STRUCT_H_

#include <stdio.h>
#include <stdint.h>
#include <sys/queue.h>

#include <rte_spinlock.h>
#include <rte_atomic.h>

#include "lio_hw_defs.h"

struct lio_stailq_node {
	STAILQ_ENTRY(lio_stailq_node) entries;
};

STAILQ_HEAD(lio_stailq_head, lio_stailq_node);

struct lio_version {
	uint16_t major;
	uint16_t minor;
	uint16_t micro;
	uint16_t reserved;
};

/** The txpciq info passed to host from the firmware */
union octeon_txpciq {
	uint64_t txpciq64;

	struct {
#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
		uint64_t q_no : 8;
		uint64_t port : 8;
		uint64_t pkind : 6;
		uint64_t use_qpg : 1;
		uint64_t qpg : 11;
		uint64_t aura_num : 10;
		uint64_t reserved : 20;
#else
		uint64_t reserved : 20;
		uint64_t aura_num : 10;
		uint64_t qpg : 11;
		uint64_t use_qpg : 1;
		uint64_t pkind : 6;
		uint64_t port : 8;
		uint64_t q_no : 8;
#endif
	} s;
};

/** The instruction (input) queue.
 *  The input queue is used to post raw (instruction) mode data or packet
 *  data to Octeon device from the host. Each input queue for
 *  a LIO device has one such structure to represent it.
 */
struct lio_instr_queue {
	/** A spinlock to protect access to the input ring.  */
	rte_spinlock_t lock;

	rte_spinlock_t post_lock;

	struct lio_device *lio_dev;

	uint32_t pkt_in_done;

	rte_atomic64_t iq_flush_running;

	/** Flag that indicates if the queue uses 64 byte commands. */
	uint32_t iqcmd_64B:1;

	/** Queue info. */
	union octeon_txpciq txpciq;

	uint32_t rsvd:17;

	uint32_t status:8;

	/** Maximum no. of instructions in this queue. */
	uint32_t max_count;

	/** Index in input ring where the driver should write the next packet */
	uint32_t host_write_index;

	/** Index in input ring where Octeon is expected to read the next
	 *  packet.
	 */
	uint32_t lio_read_index;

	/** This index aids in finding the window in the queue where Octeon
	 *  has read the commands.
	 */
	uint32_t flush_index;

	/** This field keeps track of the instructions pending in this queue. */
	rte_atomic64_t instr_pending;

	/** Pointer to the Virtual Base addr of the input ring. */
	uint8_t *base_addr;

	struct lio_request_list *request_list;

	/** Octeon doorbell register for the ring. */
	void *doorbell_reg;

	/** Octeon instruction count register for this ring. */
	void *inst_cnt_reg;

	/** Number of instructions pending to be posted to Octeon. */
	uint32_t fill_cnt;

	/** DMA mapped base address of the input descriptor ring. */
	uint64_t base_addr_dma;

	/** Application context */
	void *app_ctx;

	/* network stack queue index */
	int q_index;

	/* Memory zone */
	const struct rte_memzone *iq_mz;
};

struct lio_io_enable {
	uint64_t iq;
	uint64_t oq;
	uint64_t iq64B;
};

struct lio_fn_list {
	void (*setup_iq_regs)(struct lio_device *, uint32_t);

	int (*setup_mbox)(struct lio_device *);
	void (*free_mbox)(struct lio_device *);

	int (*setup_device_regs)(struct lio_device *);
};

struct lio_pf_vf_hs_word {
#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN
	/** PKIND value assigned for the DPI interface */
	uint64_t pkind : 8;

	/** OCTEON core clock multiplier */
	uint64_t core_tics_per_us : 16;

	/** OCTEON coprocessor clock multiplier */
	uint64_t coproc_tics_per_us : 16;

	/** app that currently running on OCTEON */
	uint64_t app_mode : 8;

	/** RESERVED */
	uint64_t reserved : 16;

#elif RTE_BYTE_ORDER == RTE_BIG_ENDIAN

	/** RESERVED */
	uint64_t reserved : 16;

	/** app that currently running on OCTEON */
	uint64_t app_mode : 8;

	/** OCTEON coprocessor clock multiplier */
	uint64_t coproc_tics_per_us : 16;

	/** OCTEON core clock multiplier */
	uint64_t core_tics_per_us : 16;

	/** PKIND value assigned for the DPI interface */
	uint64_t pkind : 8;
#endif
};

struct lio_sriov_info {
	/** Number of rings assigned to VF */
	uint32_t rings_per_vf;

	/** Number of VF devices enabled */
	uint32_t num_vfs;
};

/* Head of a response list */
struct lio_response_list {
	/** List structure to add delete pending entries to */
	struct lio_stailq_head head;

	/** A lock for this response list */
	rte_spinlock_t lock;

	rte_atomic64_t pending_req_count;
};

/* Structure to define the configuration attributes for each Input queue. */
struct lio_iq_config {
	/* Max number of IQs available */
	uint8_t max_iqs;

	/** Pending list size (usually set to the sum of the size of all Input
	 *  queues)
	 */
	uint32_t pending_list_size;

	/** Command size - 32 or 64 bytes */
	uint32_t instr_type;
};

/* Structure to define the configuration attributes for each Output queue. */
struct lio_oq_config {
	/* Max number of OQs available */
	uint8_t max_oqs;

	/** If set, the Output queue uses info-pointer mode. (Default: 1 ) */
	uint32_t info_ptr;

	/** The number of buffers that were consumed during packet processing by
	 *  the driver on this Output queue before the driver attempts to
	 *  replenish the descriptor ring with new buffers.
	 */
	uint32_t refill_threshold;
};

/* Structure to define the configuration. */
struct lio_config {
	uint16_t card_type;
	const char *card_name;

	/** Input Queue attributes. */
	struct lio_iq_config iq;

	/** Output Queue attributes. */
	struct lio_oq_config oq;

	int num_nic_ports;

	int num_def_tx_descs;

	/* Num of desc for rx rings */
	int num_def_rx_descs;

	int def_rx_buf_size;
};

/* -----------------------  THE LIO DEVICE  --------------------------- */
/** The lio device.
 *  Each lio device has this structure to represent all its
 *  components.
 */
struct lio_device {
	/** PCI device pointer */
	struct rte_pci_device *pci_dev;

	/** Octeon Chip type */
	uint16_t chip_id;
	uint16_t pf_num;
	uint16_t vf_num;

	uint8_t *hw_addr;

	struct lio_fn_list fn_list;

	uint32_t num_iqs;

	/* The pool containing pre allocated buffers used for soft commands */
	struct rte_mempool *sc_buf_pool;

	/** The input instruction queues */
	struct lio_instr_queue *instr_queue[LIO_MAX_POSSIBLE_INSTR_QUEUES];

	/** The singly-linked tail queues of instruction response */
	struct lio_response_list response_list;

	struct lio_io_enable io_qmask;

	struct lio_sriov_info sriov_info;

	struct lio_pf_vf_hs_word pfvf_hsword;

	/** Mail Box details of each lio queue. */
	struct lio_mbox **mbox;

	char dev_string[LIO_DEVICE_NAME_LEN]; /* Device print string */

	const struct lio_config *default_config;

	struct rte_eth_dev      *eth_dev;

	uint8_t max_rx_queues;
	uint8_t max_tx_queues;
	uint8_t nb_rx_queues;
	uint8_t nb_tx_queues;
	uint8_t port_configured;
	uint8_t port_id;
};
#endif /* _LIO_STRUCT_H_ */
