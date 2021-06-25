/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef _CNXK_CRYPTODEV_OPS_H_
#define _CNXK_CRYPTODEV_OPS_H_

#include <rte_cryptodev.h>

#include "roc_api.h"

#define CNXK_CPT_MIN_HEADROOM_REQ 24

struct cpt_qp_meta_info {
	struct rte_mempool *pool;
	int mlen;
};

struct cpt_inflight_req {
	union cpt_res_s res;
	struct rte_crypto_op *cop;
	void *mdata;
	uint8_t op_flags;
} __rte_aligned(16);

struct pending_queue {
	/** Pending requests count */
	uint64_t pending_count;
	/** Array of pending requests */
	struct cpt_inflight_req *req_queue;
	/** Tail of queue to be used for enqueue */
	uint16_t enq_tail;
	/** Head of queue to be used for dequeue */
	uint16_t deq_head;
	/** Timeout to track h/w being unresponsive */
	uint64_t time_out;
};

struct cnxk_cpt_qp {
	struct roc_cpt_lf lf;
	/**< Crypto LF */
	struct pending_queue pend_q;
	/**< Pending queue */
	struct rte_mempool *sess_mp;
	/**< Session mempool */
	struct rte_mempool *sess_mp_priv;
	/**< Session private data mempool */
	struct cpt_qp_meta_info meta_info;
	/**< Metabuf info required to support operations on the queue pair */
	struct roc_cpt_lmtline lmtline;
	/**< Lmtline information */
};

int cnxk_cpt_dev_config(struct rte_cryptodev *dev,
			struct rte_cryptodev_config *conf);

int cnxk_cpt_dev_start(struct rte_cryptodev *dev);

void cnxk_cpt_dev_stop(struct rte_cryptodev *dev);

int cnxk_cpt_dev_close(struct rte_cryptodev *dev);

void cnxk_cpt_dev_info_get(struct rte_cryptodev *dev,
			   struct rte_cryptodev_info *info);

int cnxk_cpt_queue_pair_setup(struct rte_cryptodev *dev, uint16_t qp_id,
			      const struct rte_cryptodev_qp_conf *conf,
			      int socket_id __rte_unused);

int cnxk_cpt_queue_pair_release(struct rte_cryptodev *dev, uint16_t qp_id);

#endif /* _CNXK_CRYPTODEV_OPS_H_ */
