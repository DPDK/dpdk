/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018 NXP
 */

#ifndef __DPAA2_QDMA_H__
#define __DPAA2_QDMA_H__

struct qdma_sdd;
struct qdma_io_meta;

#define DPAA2_QDMA_MAX_FLE 3
#define DPAA2_QDMA_MAX_SDD 2

#define DPAA2_DPDMAI_MAX_QUEUES	8

/** FLE pool size: 3 Frame list + 2 source/destination descriptor */
#define QDMA_FLE_POOL_SIZE (sizeof(struct qdma_io_meta) + \
		sizeof(struct qbman_fle) * DPAA2_QDMA_MAX_FLE + \
		sizeof(struct qdma_sdd) * DPAA2_QDMA_MAX_SDD)
/** FLE pool cache size */
#define QDMA_FLE_CACHE_SIZE(_num) (_num/(RTE_MAX_LCORE * 2))

/** Notification by FQD_CTX[fqid] */
#define QDMA_SER_CTX (1 << 8)

/**
 * Source descriptor command read transaction type for RBP=0:
 * coherent copy of cacheable memory
 */
#define DPAA2_SET_SDD_RD_COHERENT(sdd) ((sdd)->cmd = (0xb << 28))
/**
 * Destination descriptor command write transaction type for RBP=0:
 * coherent copy of cacheable memory
 */
#define DPAA2_SET_SDD_WR_COHERENT(sdd) ((sdd)->cmd = (0x6 << 28))

/** Maximum possible H/W Queues on each core */
#define MAX_HW_QUEUE_PER_CORE		64

/**
 * In case of Virtual Queue mode, this specifies the number of
 * dequeue the 'qdma_vq_dequeue/multi' API does from the H/W Queue
 * in case there is no job present on the Virtual Queue ring.
 */
#define QDMA_DEQUEUE_BUDGET		64

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

/** Represents a QDMA virtual queue */
struct qdma_virt_queue {
	/** Status ring of the virtual queue */
	struct rte_ring *status_ring;
	/** Associated hw queue */
	struct qdma_hw_queue *hw_queue;
	/** Associated lcore id */
	uint32_t lcore_id;
	/** States if this vq is in use or not */
	uint8_t in_use;
	/** States if this vq has exclusively associated hw queue */
	uint8_t exclusive_hw_queue;
	/* Total number of enqueues on this VQ */
	uint64_t num_enqueues;
	/* Total number of dequeues from this VQ */
	uint64_t num_dequeues;
};

/** Represents a QDMA per core hw queues allocation in virtual mode */
struct qdma_per_core_info {
	/** list for allocated hw queues */
	struct qdma_hw_queue *hw_queues[MAX_HW_QUEUE_PER_CORE];
	/* Number of hw queues allocated for this core */
	uint16_t num_hw_queues;
};

/** Metadata which is stored with each operation */
struct qdma_io_meta {
	/**
	 * Context which is stored in the FLE pool (just before the FLE).
	 * QDMA job is stored as a this context as a part of metadata.
	 */
	uint64_t cnxt;
	/** VQ ID is stored as a part of metadata of the enqueue command */
	 uint64_t id;
};

/** Source/Destination Descriptor */
struct qdma_sdd {
	uint32_t rsv;
	/** Stride configuration */
	uint32_t stride;
	/** Route-by-port command */
	uint32_t rbpcmd;
	uint32_t cmd;
} __attribute__((__packed__));

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
	struct dpaa2_queue rx_queue[DPAA2_DPDMAI_MAX_QUEUES];
	/** TX queues */
	struct dpaa2_queue tx_queue[DPAA2_DPDMAI_MAX_QUEUES];
};

#endif /* __DPAA2_QDMA_H__ */
