/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2021-2022 NXP
 */

#ifndef _RTE_PMD_DPAA2_QDMA_H_
#define _RTE_PMD_DPAA2_QDMA_H_

#include <rte_compat.h>

/** States if the source addresses is physical. */
#define RTE_DPAA2_QDMA_JOB_SRC_PHY		(1ULL << 30)

/** States if the destination addresses is physical. */
#define RTE_DPAA2_QDMA_JOB_DEST_PHY		(1ULL << 31)

struct rte_dpaa2_qdma_rbp {
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
	/* Virtual Function Active */
	uint32_t vfa:1;
	uint32_t rsv:3;
};

/** Determines a QDMA job */
struct rte_dpaa2_qdma_job {
	/** Source Address from where DMA is (to be) performed */
	uint64_t src;
	/** Destination Address where DMA is (to be) done */
	uint64_t dest;
	/** Length of the DMA operation in bytes. */
	uint32_t len;
	/** See RTE_QDMA_JOB_ flags */
	uint32_t flags;
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

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Enable FD in Ultra Short format on a channel. This API should be
 * called before calling 'rte_dma_vchan_setup()' API.
 *
 * @param dev_id
 *   The identifier of the device.
 * @param vchan
 *   The identifier of virtual DMA channel.
 */
__rte_experimental
void rte_dpaa2_qdma_vchan_fd_us_enable(int16_t dev_id, uint16_t vchan);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Enable internal SG processing on a channel. This API should be
 * called before calling 'rte_dma_vchan_setup()' API.
 *
 * @param dev_id
 *   The identifier of the device.
 * @param vchan
 *   The identifier of virtual DMA channel.
 */
__rte_experimental
void rte_dpaa2_qdma_vchan_internal_sg_enable(int16_t dev_id, uint16_t vchan);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Enable Route-by-port on a channel. This API should be
 * called before calling 'rte_dma_vchan_setup()' API.
 *
 * @param dev_id
 *   The identifier of the device.
 * @param vchan
 *   The identifier of virtual DMA channel.
 * @param rbp_config
 *   Configuration for route-by-port
 */
__rte_experimental
void rte_dpaa2_qdma_vchan_rbp_enable(int16_t dev_id, uint16_t vchan,
		struct rte_dpaa2_qdma_rbp *rbp_config);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Enqueue a copy operation onto the virtual DMA channel for silent mode,
 * when dequeue is not required.
 *
 * This queues up a copy operation to be performed by hardware, if the 'flags'
 * parameter contains RTE_DMA_OP_FLAG_SUBMIT then trigger doorbell to begin
 * this operation, otherwise do not trigger doorbell.
 *
 * @param dev_id
 *   The identifier of the device.
 * @param vchan
 *   The identifier of virtual DMA channel.
 * @param jobs
 *   Jobs to be submitted to QDMA.
 * @param nb_cpls
 *   Number of DMA jobs.
 *
 * @return
 *   - >= 0..Number of enqueued job.
 *   - -ENOSPC: if no space left to enqueue.
 *   - other values < 0 on failure.
 */
__rte_experimental
int rte_dpaa2_qdma_copy_multi(int16_t dev_id, uint16_t vchan,
		struct rte_dpaa2_qdma_job **jobs, uint16_t nb_cpls);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Return the number of operations that have been successfully completed.
 *
 * @param dev_id
 *   The identifier of the device.
 * @param vchan
 *   The identifier of virtual DMA channel.
 * @param jobs
 *   Jobs completed by QDMA.
 * @param nb_cpls
 *   Number of completed DMA jobs.
 *
 * @return
 *   The number of operations that successfully completed. This return value
 *   must be less than or equal to the value of nb_cpls.
 */
__rte_experimental
uint16_t rte_dpaa2_qdma_completed_multi(int16_t dev_id, uint16_t vchan,
		struct rte_dpaa2_qdma_job **jobs, uint16_t nb_cpls);

#endif /* _RTE_PMD_DPAA2_QDMA_H_ */
