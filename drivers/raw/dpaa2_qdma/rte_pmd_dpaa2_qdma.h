/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018-2019 NXP
 */

#ifndef __RTE_PMD_DPAA2_QDMA_H__
#define __RTE_PMD_DPAA2_QDMA_H__

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

/**
 * If user has configured a Virtual Queue mode, but for some particular VQ
 * user needs an exclusive H/W queue associated (for better performance
 * on that particular VQ), then user can pass this flag while creating the
 * Virtual Queue. A H/W queue will be allocated corresponding to
 * VQ which uses this flag.
 */
#define RTE_QDMA_VQ_EXCLUSIVE_PQ	(1ULL)

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
	/** mode of operation - physical(h/w) or virtual */
	uint8_t mode;
	/**
	 * User provides this as input to the driver as a size of the FLE pool.
	 * FLE's (and corresponding source/destination descriptors) are
	 * allocated by the driver at enqueue time to store src/dest and
	 * other data and are freed at the dequeue time. This determines the
	 * maximum number of inflight jobs on the QDMA device. This should
	 * be power of 2.
	 */
	int fle_pool_count;
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
};

/**
 * Initialize the QDMA device.
 *
 * @returns
 *   - 0: Success.
 *   - <0: Error code.
 */
int
rte_qdma_init(void);

/**
 * Get the QDMA attributes.
 *
 * @param qdma_attr
 *   QDMA attributes providing total number of hw queues etc.
 */
void
rte_qdma_attr_get(struct rte_qdma_attr *qdma_attr);

/**
 * Reset the QDMA device. This API will completely reset the QDMA
 * device, bringing it to original state as if only rte_qdma_init() API
 * has been called.
 *
 * @returns
 *   - 0: Success.
 *   - <0: Error code.
 */
int
rte_qdma_reset(void);

/**
 * Configure the QDMA device.
 *
 * @returns
 *   - 0: Success.
 *   - <0: Error code.
 */
int
rte_qdma_configure(struct rte_qdma_config *qdma_config);

/**
 * Start the QDMA device.
 *
 * @returns
 *   - 0: Success.
 *   - <0: Error code.
 */
int
rte_qdma_start(void);

/**
 * Create a Virtual Queue on a particular lcore id.
 * This API can be called from any thread/core. User can create/destroy
 * VQ's at runtime.
 *
 * @param lcore_id
 *   LCORE ID on which this particular queue would be associated with.
 * @param flags
 *  RTE_QDMA_VQ_ flags. See macro definitions.
 *
 * @returns
 *   - >= 0: Virtual queue ID.
 *   - <0: Error code.
 */
int
rte_qdma_vq_create(uint32_t lcore_id, uint32_t flags);

/*create vq for route-by-port*/
int
rte_qdma_vq_create_rbp(uint32_t lcore_id, uint32_t flags,
			struct rte_qdma_rbp *rbp);

/**
 * Enqueue multiple jobs to a Virtual Queue.
 * If the enqueue is successful, the H/W will perform DMA operations
 * on the basis of the QDMA jobs provided.
 *
 * @param vq_id
 *   Virtual Queue ID.
 * @param job
 *   List of QDMA Jobs containing relevant information related to DMA.
 * @param nb_jobs
 *   Number of QDMA jobs provided by the user.
 *
 * @returns
 *   - >=0: Number of jobs successfully submitted
 *   - <0: Error code.
 */
int
rte_qdma_vq_enqueue_multi(uint16_t vq_id,
			  struct rte_qdma_job **job,
			  uint16_t nb_jobs);

/**
 * Enqueue a single job to a Virtual Queue.
 * If the enqueue is successful, the H/W will perform DMA operations
 * on the basis of the QDMA job provided.
 *
 * @param vq_id
 *   Virtual Queue ID.
 * @param job
 *   A QDMA Job containing relevant information related to DMA.
 *
 * @returns
 *   - >=0: Number of jobs successfully submitted
 *   - <0: Error code.
 */
int
rte_qdma_vq_enqueue(uint16_t vq_id,
		    struct rte_qdma_job *job);

/**
 * Dequeue multiple completed jobs from a Virtual Queue.
 * Provides the list of completed jobs capped by nb_jobs.
 *
 * @param vq_id
 *   Virtual Queue ID.
 * @param job
 *   List of QDMA Jobs returned from the API.
 * @param nb_jobs
 *   Number of QDMA jobs requested for dequeue by the user.
 *
 * @returns
 *   - >=0: Number of jobs successfully received
 *   - <0: Error code.
 */
int
rte_qdma_vq_dequeue_multi(uint16_t vq_id,
			  struct rte_qdma_job **job,
			  uint16_t nb_jobs);

/**
 * Dequeue a single completed jobs from a Virtual Queue.
 *
 * @param vq_id
 *   Virtual Queue ID.
 *
 * @returns
 *   - A completed job or NULL if no job is there.
 */
struct rte_qdma_job * __rte_experimental
rte_qdma_vq_dequeue(uint16_t vq_id);

/**
 * Get a Virtual Queue statistics.
 *
 * @param vq_id
 *   Virtual Queue ID.
 * @param vq_stats
 *   VQ statistics structure which will be filled in by the driver.
 */
void
rte_qdma_vq_stats(uint16_t vq_id,
		  struct rte_qdma_vq_stats *vq_stats);

/**
 * Destroy the Virtual Queue specified by vq_id.
 * This API can be called from any thread/core. User can create/destroy
 * VQ's at runtime.
 *
 * @param vq_id
 *   Virtual Queue ID which needs to be uninitialized.
 *
 * @returns
 *   - 0: Success.
 *   - <0: Error code.
 */
int
rte_qdma_vq_destroy(uint16_t vq_id);

/**
 * Destroy the RBP specific Virtual Queue specified by vq_id.
 * This API can be called from any thread/core. User can create/destroy
 * VQ's at runtime.
 *
 * @param vq_id
 *   RBP based Virtual Queue ID which needs to be uninitialized.
 *
 * @returns
 *   - 0: Success.
 *   - <0: Error code.
 */

int
rte_qdma_vq_destroy_rbp(uint16_t vq_id);
/**
 * Stop QDMA device.
 */
void
rte_qdma_stop(void);

/**
 * Destroy the QDMA device.
 */
void
rte_qdma_destroy(void);

#endif /* __RTE_PMD_DPAA2_QDMA_H__*/
