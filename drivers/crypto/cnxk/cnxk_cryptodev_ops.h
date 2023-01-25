/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef _CNXK_CRYPTODEV_OPS_H_
#define _CNXK_CRYPTODEV_OPS_H_

#include <cryptodev_pmd.h>
#include <rte_event_crypto_adapter.h>

#include "hw/cpt.h"

#include "roc_constants.h"
#include "roc_cpt.h"
#include "roc_cpt_sg.h"
#include "roc_se.h"

#define CNXK_CPT_MIN_HEADROOM_REQ	 32
#define CNXK_CPT_MIN_TAILROOM_REQ	 102

/* Default command timeout in seconds */
#define DEFAULT_COMMAND_TIMEOUT 4

#define MOD_INC(i, l) ((i) == (l - 1) ? (i) = 0 : (i)++)

/* Macros to form words in CPT instruction */
#define CNXK_CPT_INST_W2(tag, tt, grp, rvu_pf_func)                            \
	((tag) | ((uint64_t)(tt) << 32) | ((uint64_t)(grp) << 34) |            \
	 ((uint64_t)(rvu_pf_func) << 48))
#define CNXK_CPT_INST_W3(qord, wqe_ptr)                                        \
	(qord | ((uintptr_t)(wqe_ptr) >> 3) << 3)

struct cpt_qp_meta_info {
	struct rte_mempool *pool;
	int mlen;
};

#define CPT_OP_FLAGS_METABUF	       (1 << 1)
#define CPT_OP_FLAGS_AUTH_VERIFY       (1 << 0)
#define CPT_OP_FLAGS_IPSEC_DIR_INBOUND (1 << 2)
#define CPT_OP_FLAGS_IPSEC_INB_REPLAY  (1 << 3)

struct cpt_inflight_req {
	union cpt_res_s res;
	union {
		struct rte_crypto_op *cop;
		struct rte_event_vector *vec;
	};
	void *mdata;
	uint8_t op_flags;
	void *qp;
} __rte_aligned(ROC_ALIGN);

PLT_STATIC_ASSERT(sizeof(struct cpt_inflight_req) == ROC_CACHE_LINE_SZ);

struct pending_queue {
	/** Array of pending requests */
	struct cpt_inflight_req *req_queue;
	/** Head of the queue to be used for enqueue */
	uint64_t head;
	/** Tail of the queue to be used for dequeue */
	uint64_t tail;
	/** Pending queue mask */
	uint64_t pq_mask;
	/** Timeout to track h/w being unresponsive */
	uint64_t time_out;
};

struct crypto_adpter_info {
	bool enabled;
	/**< Set if queue pair is added to crypto adapter */
	struct rte_mempool *req_mp;
	/**< CPT inflight request mempool */
	uint16_t vector_sz;
	/** Maximum number of cops to combine into single vector */
	struct rte_mempool *vector_mp;
	/** Pool for allocating rte_event_vector */
};

struct cnxk_cpt_qp {
	struct roc_cpt_lf lf;
	/**< Crypto LF */
	struct pending_queue pend_q;
	/**< Pending queue */
	struct roc_cpt_lmtline lmtline;
	/**< Lmtline information */
	struct cpt_qp_meta_info meta_info;
	/**< Metabuf info required to support operations on the queue pair */
	struct crypto_adpter_info ca;
	/**< Crypto adapter related info */
	struct rte_mempool *sess_mp;
	/**< Session mempool */
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

unsigned int cnxk_cpt_sym_session_get_size(struct rte_cryptodev *dev);

int cnxk_cpt_sym_session_configure(struct rte_cryptodev *dev, struct rte_crypto_sym_xform *xform,
				   struct rte_cryptodev_sym_session *sess);

int sym_session_configure(struct roc_cpt *roc_cpt, struct rte_crypto_sym_xform *xform,
			  struct rte_cryptodev_sym_session *sess, bool is_session_less);

void cnxk_cpt_sym_session_clear(struct rte_cryptodev *dev, struct rte_cryptodev_sym_session *sess);

void sym_session_clear(struct rte_cryptodev_sym_session *sess, bool is_session_less);

unsigned int cnxk_ae_session_size_get(struct rte_cryptodev *dev __rte_unused);

void cnxk_ae_session_clear(struct rte_cryptodev *dev,
			   struct rte_cryptodev_asym_session *sess);
int cnxk_ae_session_cfg(struct rte_cryptodev *dev,
			struct rte_crypto_asym_xform *xform,
			struct rte_cryptodev_asym_session *sess);
void cnxk_cpt_dump_on_err(struct cnxk_cpt_qp *qp);
int cnxk_cpt_queue_pair_event_error_query(struct rte_cryptodev *dev, uint16_t qp_id);

static __rte_always_inline void
pending_queue_advance(uint64_t *index, const uint64_t mask)
{
	*index = (*index + 1) & mask;
}

static __rte_always_inline void
pending_queue_retreat(uint64_t *index, const uint64_t mask, uint64_t nb_entry)
{
	*index = (*index - nb_entry) & mask;
}

static __rte_always_inline uint64_t
pending_queue_infl_cnt(uint64_t head, uint64_t tail, const uint64_t mask)
{
	/*
	 * Mask is nb_desc - 1. Add nb_desc to head and mask to account for
	 * cases when tail > head, which happens during wrap around.
	 */
	return ((head + mask + 1) - tail) & mask;
}

static __rte_always_inline uint64_t
pending_queue_free_cnt(uint64_t head, uint64_t tail, const uint64_t mask)
{
	/* mask is nb_desc - 1 */
	return mask - pending_queue_infl_cnt(head, tail, mask);
}

static __rte_always_inline void *
alloc_op_meta(struct roc_se_buf_ptr *buf, int32_t len, struct rte_mempool *cpt_meta_pool,
	      struct cpt_inflight_req *infl_req)
{
	uint8_t *mdata;

	if (unlikely(rte_mempool_get(cpt_meta_pool, (void **)&mdata) < 0))
		return NULL;

	if (likely(buf)) {
		buf->vaddr = mdata;
		buf->size = len;
	}

	infl_req->mdata = mdata;
	infl_req->op_flags |= CPT_OP_FLAGS_METABUF;

	return mdata;
}
#endif /* _CNXK_CRYPTODEV_OPS_H_ */
