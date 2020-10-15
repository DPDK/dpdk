/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018-2020 NXP
 */

#ifndef __RTE_PMD_DPAA2_QDMA_H__
#define __RTE_PMD_DPAA2_QDMA_H__

#include <rte_rawdev.h>

/**
 * @file
 *
 * NXP dpaa2 QDMA specific structures.
 *
 */

/** Maximum qdma burst size */
#define RTE_QDMA_BURST_NB_MAX 256

/** Determines the mode of operation */
enum {
	/**
	 * Allocate a H/W queue per VQ i.e. Exclusive hardware queue for a VQ.
	 * This mode will have best performance.
	 */
	RTE_QDMA_MODE_HW,
	/**
	 * A VQ shall not have an exclusive associated H/W queue.
	 * Rather a H/W Queue will be shared by multiple Virtual Queues.
	 * This mode will have intermediate data structures to support
	 * multi VQ to PQ mappings thus having some performance implications.
	 * Note: Even in this mode there is an option to allocate a H/W
	 * queue for a VQ. Please see 'RTE_QDMA_VQ_EXCLUSIVE_PQ' flag.
	 */
	RTE_QDMA_MODE_VIRTUAL
};

/** Determines the format of FD */
enum {
	RTE_QDMA_LONG_FORMAT,
	RTE_QDMA_ULTRASHORT_FORMAT,
};

/**
 * If user has configured a Virtual Queue mode, but for some particular VQ
 * user needs an exclusive H/W queue associated (for better performance
 * on that particular VQ), then user can pass this flag while creating the
 * Virtual Queue. A H/W queue will be allocated corresponding to
 * VQ which uses this flag.
 */
#define RTE_QDMA_VQ_EXCLUSIVE_PQ	(1ULL)

#define RTE_QDMA_VQ_FD_LONG_FORMAT		(1ULL << 1)

#define RTE_QDMA_VQ_FD_SG_FORMAT		(1ULL << 2)

#define RTE_QDMA_VQ_NO_RESPONSE			(1ULL << 3)

/** States if the source addresses is physical. */
#define RTE_QDMA_JOB_SRC_PHY		(1ULL)

/** States if the destination addresses is physical. */
#define RTE_QDMA_JOB_DEST_PHY		(1ULL << 1)

/** Provides QDMA device attributes */
struct rte_qdma_attr {
	/** total number of hw QDMA queues present */
	uint16_t num_hw_queues;
};

/** QDMA device configuration structure */
struct rte_qdma_config {
	/** Number of maximum hw queues to allocate per core. */
	uint16_t max_hw_queues_per_core;
	/** Maximum number of VQ's to be used. */
	uint16_t max_vqs;
	/**
	 * User provides this as input to the driver as a size of the FLE pool.
	 * FLE's (and corresponding source/destination descriptors) are
	 * allocated by the driver at enqueue time to store src/dest and
	 * other data and are freed at the dequeue time. This determines the
	 * maximum number of inflight jobs on the QDMA device. This should
	 * be power of 2.
	 */
	int fle_queue_pool_cnt;
};

struct rte_qdma_rbp {
	uint32_t use_ultrashort:1;
	uint32_t enable:1;
	/**
	 * dportid:
	 * 0000 PCI-Express 1
	 * 0001 PCI-Express 2
	 * 0010 PCI-Express 3
	 * 0011 PCI-Express 4
	 * 0100 PCI-Express 5
	 * 0101 PCI-Express 6
	 */
	uint32_t dportid:4;
	uint32_t dpfid:2;
	uint32_t dvfid:6;
	/*using route by port for destination */
	uint32_t drbp:1;
	/**
	 * sportid:
	 * 0000 PCI-Express 1
	 * 0001 PCI-Express 2
	 * 0010 PCI-Express 3
	 * 0011 PCI-Express 4
	 * 0100 PCI-Express 5
	 * 0101 PCI-Express 6
	 */
	uint32_t sportid:4;
	uint32_t spfid:2;
	uint32_t svfid:6;
	/* using route by port for source */
	uint32_t srbp:1;
	uint32_t rsv:4;
};

/** Provides QDMA device statistics */
struct rte_qdma_vq_stats {
	/** States if this vq has exclusively associated hw queue */
	uint8_t exclusive_hw_queue;
	/** Associated lcore id */
	uint32_t lcore_id;
	/* Total number of enqueues on this VQ */
	uint64_t num_enqueues;
	/* Total number of dequeues from this VQ */
	uint64_t num_dequeues;
	/* total number of pending jobs in this VQ */
	uint64_t num_pending_jobs;
};

/** Determines a QDMA job */
struct rte_qdma_job {
	/** Source Address from where DMA is (to be) performed */
	uint64_t src;
	/** Destination Address where DMA is (to be) done */
	uint64_t dest;
	/** Length of the DMA operation in bytes. */
	uint32_t len;
	/** See RTE_QDMA_JOB_ flags */
	uint32_t flags;
	/**
	 * User can specify a context which will be maintained
	 * on the dequeue operation.
	 */
	uint64_t cnxt;
	/**
	 * Status of the transaction.
	 * This is filled in the dequeue operation by the driver.
	 * upper 8bits acc_err for route by port.
	 * lower 8bits fd error
	 */
	uint16_t status;
	uint16_t vq_id;
	/**
	 * FLE pool element maintained by user, in case no qDMA response.
	 * Note: the address must be allocated from DPDK memory pool.
	 */
	void *usr_elem;
};

struct rte_qdma_enqdeq {
	uint16_t vq_id;
	struct rte_qdma_job **job;
};

struct rte_qdma_queue_config {
	uint32_t lcore_id;
	uint32_t flags;
	struct rte_qdma_rbp *rbp;
};

#define rte_qdma_info rte_rawdev_info
#define rte_qdma_start(id) rte_rawdev_start(id)
#define rte_qdma_reset(id) rte_rawdev_reset(id)
#define rte_qdma_configure(id, cf) rte_rawdev_configure(id, cf)
#define rte_qdma_dequeue_buffers(id, buf, num, ctxt) \
	rte_rawdev_dequeue_buffers(id, buf, num, ctxt)
#define rte_qdma_enqueue_buffers(id, buf, num, ctxt) \
	rte_rawdev_enqueue_buffers(id, buf, num, ctxt)
#define rte_qdma_queue_setup(id, qid, cfg) \
	rte_rawdev_queue_setup(id, qid, cfg)

/*TODO introduce per queue stats API in rawdew */
/**
 * Get a Virtual Queue statistics.
 *
 * @param rawdev
 *   Raw Device.
 * @param vq_id
 *   Virtual Queue ID.
 * @param vq_stats
 *   VQ statistics structure which will be filled in by the driver.
 */
void
rte_qdma_vq_stats(struct rte_rawdev *rawdev,
		uint16_t vq_id,
		struct rte_qdma_vq_stats *vq_stats);

#endif /* __RTE_PMD_DPAA2_QDMA_H__*/
