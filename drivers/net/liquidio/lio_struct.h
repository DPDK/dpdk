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

struct lio_device;
struct lio_fn_list {
	int (*setup_device_regs)(struct lio_device *);
};

struct lio_sriov_info {
	/** Number of rings assigned to VF */
	uint32_t rings_per_vf;

	/** Number of VF devices enabled */
	uint32_t num_vfs;
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

	struct lio_sriov_info sriov_info;

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
