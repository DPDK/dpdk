/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2020 Marvell International Ltd.
 */

#ifndef _OTX2_CRYPTODEV_QP_H_
#define _OTX2_CRYPTODEV_QP_H_

#include <rte_common.h>
#include <rte_mempool.h>
#include <rte_spinlock.h>

#include "cpt_common.h"

struct otx2_cpt_qp {
	uint32_t id;
	/**< Queue pair id */
	uintptr_t base;
	/**< Base address where BAR is mapped */
	void *lmtline;
	/**< Address of LMTLINE */
	rte_iova_t lf_nq_reg;
	/**< LF enqueue register address */
	struct pending_queue pend_q;
	/**< Pending queue */
	struct rte_mempool *sess_mp;
	/**< Session mempool */
	struct rte_mempool *sess_mp_priv;
	/**< Session private data mempool */
	struct cpt_qp_meta_info meta_info;
	/**< Metabuf info required to support operations on the queue pair */
	rte_iova_t iq_dma_addr;
	/**< Instruction queue address */
};

#endif /* _OTX2_CRYPTODEV_QP_H_ */
