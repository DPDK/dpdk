/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018 NXP
 */

#ifndef __DPAA2_QDMA_H__
#define __DPAA2_QDMA_H__

/**
 * Represents a QDMA device.
 * A single QDMA device exists which is combination of multiple DPDMAI rawdev's.
 */
struct qdma_device {
	/** total number of hw queues. */
	uint16_t num_hw_queues;
	/**
	 * Maximum number of hw queues to be alocated per core.
	 * This is limited by MAX_HW_QUEUE_PER_CORE
	 */
	uint16_t max_hw_queues_per_core;
	/** Maximum number of VQ's */
	uint16_t max_vqs;
	/** mode of operation - physical(h/w) or virtual */
	uint8_t mode;
	/** Device state - started or stopped */
	uint8_t state;
	/** FLE pool for the device */
	struct rte_mempool *fle_pool;
	/** FLE pool size */
	int fle_pool_count;
	/** A lock to QDMA device whenever required */
	rte_spinlock_t lock;
};

/** Represents a QDMA H/W queue */
struct qdma_hw_queue {
	/** Pointer to Next instance */
	TAILQ_ENTRY(qdma_hw_queue) next;
	/** DPDMAI device to communicate with HW */
	struct dpaa2_dpdmai_dev *dpdmai_dev;
	/** queue ID to communicate with HW */
	uint16_t queue_id;
	/** Associated lcore id */
	uint32_t lcore_id;
	/** Number of users of this hw queue */
	uint32_t num_users;
};

/** Represents a DPDMAI raw device */
struct dpaa2_dpdmai_dev {
	/** Pointer to Next device instance */
	TAILQ_ENTRY(dpaa2_qdma_device) next;
	/** handle to DPDMAI object */
	struct fsl_mc_io dpdmai;
	/** HW ID for DPDMAI object */
	uint32_t dpdmai_id;
	/** Tocken of this device */
	uint16_t token;
	/** Number of queue in this DPDMAI device */
	uint8_t num_queues;
	/** RX queues */
	struct dpaa2_queue rx_queue[DPDMAI_PRIO_NUM];
	/** TX queues */
	struct dpaa2_queue tx_queue[DPDMAI_PRIO_NUM];
};

#endif /* __DPAA2_QDMA_H__ */
