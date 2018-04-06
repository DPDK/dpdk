/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2018 Intel Corporation
 */

#ifndef _QAT_CRYPTO_H_
#define _QAT_CRYPTO_H_

#include <rte_cryptodev_pmd.h>
#include <rte_memzone.h>

#include "qat_crypto_capabilities.h"

#define CRYPTODEV_NAME_QAT_SYM_PMD	crypto_qat
/**< Intel QAT Symmetric Crypto PMD device name */

/*
 * This macro rounds up a number to a be a multiple of
 * the alignment when the alignment is a power of 2
 */
#define ALIGN_POW2_ROUNDUP(num, align) \
	(((num) + (align) - 1) & ~((align) - 1))
#define QAT_64_BTYE_ALIGN_MASK (~0x3f)

#define QAT_CSR_HEAD_WRITE_THRESH 32U
/* number of requests to accumulate before writing head CSR */
#define QAT_CSR_TAIL_WRITE_THRESH 32U
/* number of requests to accumulate before writing tail CSR */
#define QAT_CSR_TAIL_FORCE_WRITE_THRESH 256U
/* number of inflights below which no tail write coalescing should occur */

struct qat_session;

enum qat_device_gen {
	QAT_GEN1 = 1,
	QAT_GEN2,
};

/**
 * Structure associated with each queue.
 */
struct qat_queue {
	char		memz_name[RTE_MEMZONE_NAMESIZE];
	void		*base_addr;		/* Base address */
	rte_iova_t	base_phys_addr;		/* Queue physical address */
	uint32_t	head;			/* Shadow copy of the head */
	uint32_t	tail;			/* Shadow copy of the tail */
	uint32_t	modulo;
	uint32_t	msg_size;
	uint16_t	max_inflights;
	uint32_t	queue_size;
	uint8_t		hw_bundle_number;
	uint8_t		hw_queue_number;
	/* HW queue aka ring offset on bundle */
	uint32_t	csr_head;		/* last written head value */
	uint32_t	csr_tail;		/* last written tail value */
	uint16_t	nb_processed_responses;
	/* number of responses processed since last CSR head write */
	uint16_t	nb_pending_requests;
	/* number of requests pending since last CSR tail write */
};

struct qat_qp {
	void			*mmap_bar_addr;
	uint16_t		inflights16;
	struct	qat_queue	tx_q;
	struct	qat_queue	rx_q;
	struct	rte_cryptodev_stats stats;
	struct rte_mempool *op_cookie_pool;
	void **op_cookies;
	uint32_t nb_descriptors;
	enum qat_device_gen qat_dev_gen;
} __rte_cache_aligned;

/** private data structure for each QAT device */
struct qat_pmd_private {
	unsigned max_nb_queue_pairs;
	/**< Max number of queue pairs supported by device */
	unsigned max_nb_sessions;
	/**< Max number of sessions supported by device */
	enum qat_device_gen qat_dev_gen;
	/**< QAT device generation */
	const struct rte_cryptodev_capabilities *qat_dev_capabilities;
};

extern uint8_t cryptodev_qat_driver_id;

int qat_dev_config(struct rte_cryptodev *dev,
		struct rte_cryptodev_config *config);
int qat_dev_start(struct rte_cryptodev *dev);
void qat_dev_stop(struct rte_cryptodev *dev);
int qat_dev_close(struct rte_cryptodev *dev);
void qat_dev_info_get(struct rte_cryptodev *dev,
	struct rte_cryptodev_info *info);

void qat_crypto_sym_stats_get(struct rte_cryptodev *dev,
	struct rte_cryptodev_stats *stats);
void qat_crypto_sym_stats_reset(struct rte_cryptodev *dev);

int qat_crypto_sym_qp_setup(struct rte_cryptodev *dev, uint16_t queue_pair_id,
	const struct rte_cryptodev_qp_conf *rx_conf, int socket_id,
	struct rte_mempool *session_pool);
int qat_crypto_sym_qp_release(struct rte_cryptodev *dev,
	uint16_t queue_pair_id);

int
qat_pmd_session_mempool_create(struct rte_cryptodev *dev,
	unsigned nb_objs, unsigned obj_cache_size, int socket_id);

extern unsigned
qat_crypto_sym_get_session_private_size(struct rte_cryptodev *dev);

extern int
qat_crypto_sym_configure_session(struct rte_cryptodev *dev,
		struct rte_crypto_sym_xform *xform,
		struct rte_cryptodev_sym_session *sess,
		struct rte_mempool *mempool);


int
qat_crypto_set_session_parameters(struct rte_cryptodev *dev,
		struct rte_crypto_sym_xform *xform, void *session_private);

int
qat_crypto_sym_configure_session_aead(struct rte_crypto_sym_xform *xform,
				struct qat_session *session);

int
qat_crypto_sym_configure_session_auth(struct rte_cryptodev *dev,
				struct rte_crypto_sym_xform *xform,
				struct qat_session *session);

int
qat_crypto_sym_configure_session_cipher(struct rte_cryptodev *dev,
		struct rte_crypto_sym_xform *xform,
		struct qat_session *session);


extern void
qat_crypto_sym_clear_session(struct rte_cryptodev *dev,
		struct rte_cryptodev_sym_session *session);

extern uint16_t
qat_pmd_enqueue_op_burst(void *qp, struct rte_crypto_op **ops,
		uint16_t nb_ops);

extern uint16_t
qat_pmd_dequeue_op_burst(void *qp, struct rte_crypto_op **ops,
		uint16_t nb_ops);

#endif /* _QAT_CRYPTO_H_ */
