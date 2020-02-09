/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018-2019 NXP
 */

#ifndef __DPAA2_QDMA_H__
#define __DPAA2_QDMA_H__

struct qdma_sdd;
struct rte_qdma_job;

#define DPAA2_QDMA_MAX_FLE 3
#define DPAA2_QDMA_MAX_SDD 2

#define DPAA2_DPDMAI_MAX_QUEUES	8

/** FLE pool size: 3 Frame list + 2 source/destination descriptor */
#define QDMA_FLE_POOL_SIZE (sizeof(struct rte_qdma_job *) + \
		sizeof(struct qbman_fle) * DPAA2_QDMA_MAX_FLE + \
		sizeof(struct qdma_sdd) * DPAA2_QDMA_MAX_SDD)
/** FLE pool cache size */
#define QDMA_FLE_CACHE_SIZE(_num) (_num/(RTE_MAX_LCORE * 2))

/** Notification by FQD_CTX[fqid] */
#define QDMA_SER_CTX (1 << 8)
#define DPAA2_RBP_MEM_RW            0x0
/**
 * Source descriptor command read transaction type for RBP=0:
 * coherent copy of cacheable memory
 */
#define DPAA2_COHERENT_NO_ALLOCATE_CACHE	0xb
#define DPAA2_LX2_COHERENT_NO_ALLOCATE_CACHE	0x7
/**
 * Destination descriptor command write transaction type for RBP=0:
 * coherent copy of cacheable memory
 */
#define DPAA2_COHERENT_ALLOCATE_CACHE		0x6
#define DPAA2_LX2_COHERENT_ALLOCATE_CACHE	0xb

/** Maximum possible H/W Queues on each core */
#define MAX_HW_QUEUE_PER_CORE		64

#define QDMA_RBP_UPPER_ADDRESS_MASK (0xfff0000000000)
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
	/** Route by port */
	struct rte_qdma_rbp rbp;
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

/** Source/Destination Descriptor */
struct qdma_sdd {
	uint32_t rsv;
	/** Stride configuration */
	uint32_t stride;
	/** Route-by-port command */
	union {
		uint32_t rbpcmd;
		struct rbpcmd_st {
			uint32_t vfid:6;
			uint32_t rsv4:2;
			uint32_t pfid:1;
			uint32_t rsv3:7;
			uint32_t attr:3;
			uint32_t rsv2:1;
			uint32_t at:2;
			uint32_t vfa:1;
			uint32_t ca:1;
			uint32_t tc:3;
			uint32_t rsv1:5;
		} rbpcmd_simple;
	};
	union {
		uint32_t cmd;
		struct rcmd_simple {
			uint32_t portid:4;
			uint32_t rsv1:14;
			uint32_t rbp:1;
			uint32_t ssen:1;
			uint32_t rthrotl:4;
			uint32_t sqos:3;
			uint32_t ns:1;
			uint32_t rdtype:4;
		} read_cmd;
		struct wcmd_simple {
			uint32_t portid:4;
			uint32_t rsv3:10;
			uint32_t rsv2:2;
			uint32_t lwc:2;
			uint32_t rbp:1;
			uint32_t dsen:1;
			uint32_t rsv1:4;
			uint32_t dqos:3;
			uint32_t ns:1;
			uint32_t wrttype:4;
		} write_cmd;
	};
} __rte_packed;

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
