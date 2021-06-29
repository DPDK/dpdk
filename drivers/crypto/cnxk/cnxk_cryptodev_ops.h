/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef _CNXK_CRYPTODEV_OPS_H_
#define _CNXK_CRYPTODEV_OPS_H_

#include <rte_cryptodev.h>

#include "roc_api.h"

#define CNXK_CPT_MIN_HEADROOM_REQ 24

/* Default command timeout in seconds */
#define DEFAULT_COMMAND_TIMEOUT 4

#define MOD_INC(i, l) ((i) == (l - 1) ? (i) = 0 : (i)++)

struct cpt_qp_meta_info {
	struct rte_mempool *pool;
	int mlen;
};

enum sym_xform_type {
	CNXK_CPT_CIPHER = 1,
	CNXK_CPT_AUTH,
	CNXK_CPT_AEAD,
	CNXK_CPT_CIPHER_ENC_AUTH_GEN,
	CNXK_CPT_AUTH_VRFY_CIPHER_DEC,
	CNXK_CPT_AUTH_GEN_CIPHER_ENC,
	CNXK_CPT_CIPHER_DEC_AUTH_VRFY
};

#define CPT_OP_FLAGS_METABUF	       (1 << 1)
#define CPT_OP_FLAGS_AUTH_VERIFY       (1 << 0)
#define CPT_OP_FLAGS_IPSEC_DIR_INBOUND (1 << 2)

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

unsigned int cnxk_cpt_sym_session_get_size(struct rte_cryptodev *dev);

int cnxk_cpt_sym_session_configure(struct rte_cryptodev *dev,
				   struct rte_crypto_sym_xform *xform,
				   struct rte_cryptodev_sym_session *sess,
				   struct rte_mempool *pool);

int sym_session_configure(struct roc_cpt *roc_cpt, int driver_id,
			  struct rte_crypto_sym_xform *xform,
			  struct rte_cryptodev_sym_session *sess,
			  struct rte_mempool *pool);

void cnxk_cpt_sym_session_clear(struct rte_cryptodev *dev,
				struct rte_cryptodev_sym_session *sess);

void sym_session_clear(int driver_id, struct rte_cryptodev_sym_session *sess);

unsigned int cnxk_ae_session_size_get(struct rte_cryptodev *dev __rte_unused);

void cnxk_ae_session_clear(struct rte_cryptodev *dev,
			   struct rte_cryptodev_asym_session *sess);
int cnxk_ae_session_cfg(struct rte_cryptodev *dev,
			struct rte_crypto_asym_xform *xform,
			struct rte_cryptodev_asym_session *sess,
			struct rte_mempool *pool);
#endif /* _CNXK_CRYPTODEV_OPS_H_ */
