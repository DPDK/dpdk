/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2020 Marvell International Ltd.
 */

#ifndef _OTX2_REGEXDEV_H_
#define _OTX2_REGEXDEV_H_

#include <rte_common.h>
#include <rte_regexdev.h>

#include "otx2_dev.h"

#define ree_func_trace otx2_ree_dbg

/* Marvell OCTEON TX2 Regex PMD device name */
#define REGEXDEV_NAME_OCTEONTX2_PMD	regex_octeontx2

#define OTX2_REE_MAX_LFS		36
#define OTX2_REE_MAX_QUEUES_PER_VF	36
#define OTX2_REE_MAX_MATCHES_PER_VF	254

#define OTX2_REE_MAX_PAYLOAD_SIZE	(1 << 14)

#define OTX2_REE_NON_INC_PROG 0
#define OTX2_REE_INC_PROG 1

#define REE_MOD_INC(i, l)   ((i) == (l - 1) ? (i) = 0 : (i)++)


/**
 * Device vf data
 */
struct otx2_ree_vf {
	struct otx2_dev otx2_dev;
	/**< Base class */
	uint16_t max_queues;
	/**< Max queues supported */
	uint8_t nb_queues;
	/**< Number of regex queues attached */
	uint16_t max_matches;
	/**<  Max matches supported*/
	uint16_t lf_msixoff[OTX2_REE_MAX_LFS];
	/**< MSI-X offsets */
	uint8_t block_address;
	/**< REE Block Address */
	uint8_t err_intr_registered:1;
	/**< Are error interrupts registered? */
};

/**
 * Device private data
 */
struct otx2_ree_data {
	uint32_t regexdev_capa;
	uint64_t rule_flags;
	/**< Feature flags exposes HW/SW features for the given device */
	uint16_t max_rules_per_group;
	/**< Maximum rules supported per subset by this device */
	uint16_t max_groups;
	/**< Maximum subset supported by this device */
	void **queue_pairs;
	/**< Array of pointers to queue pairs. */
	uint16_t nb_queue_pairs;
	/**< Number of device queue pairs. */
	struct otx2_ree_vf vf;
	/**< vf data */
	struct rte_regexdev_rule *rules;
	/**< rules to be compiled */
	uint16_t nb_rules;
	/**< number of rules */
} __rte_cache_aligned;

struct otx2_ree_rid {
	uintptr_t rid;
	/** Request id of a ree operation */
	uint64_t user_id;
	/* Client data */
	/**< IOVA address of the pattern to be matched. */
};

struct otx2_ree_pending_queue {
	uint64_t pending_count;
	/** Pending requests count */
	struct otx2_ree_rid *rid_queue;
	/** Array of pending requests */
	uint16_t enq_tail;
	/** Tail of queue to be used for enqueue */
	uint16_t deq_head;
	/** Head of queue to be used for dequeue */
};

struct otx2_ree_qp {
	uint32_t id;
	/**< Queue pair id */
	uintptr_t base;
	/**< Base address where BAR is mapped */
	struct otx2_ree_pending_queue pend_q;
	/**< Pending queue */
	rte_iova_t iq_dma_addr;
	/**< Instruction queue address */
	uint32_t otx2_regexdev_jobid;
	/**< Job ID */
	uint32_t write_offset;
	/**< write offset */
	regexdev_stop_flush_t cb;
	/**< Callback function called during rte_regex_dev_stop()*/
};

#endif /* _OTX2_REGEXDEV_H_ */
