/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2018 Intel Corporation
 */

#ifndef _QAT_SYM_H_
#define _QAT_SYM_H_

#include <rte_cryptodev_pmd.h>
#include <rte_memzone.h>

#include "qat_common.h"
#include "qat_device.h"
#include "qat_crypto_capabilities.h"

/*
 * This macro rounds up a number to a be a multiple of
 * the alignment when the alignment is a power of 2
 */
#define ALIGN_POW2_ROUNDUP(num, align) \
	(((num) + (align) - 1) & ~((align) - 1))
#define QAT_64_BTYE_ALIGN_MASK (~0x3f)

struct qat_sym_session;

struct qat_sym_op_cookie {
	struct qat_sgl qat_sgl_src;
	struct qat_sgl qat_sgl_dst;
	phys_addr_t qat_sgl_src_phys_addr;
	phys_addr_t qat_sgl_dst_phys_addr;
};

int
qat_sym_build_request(void *in_op, uint8_t *out_msg,
		void *op_cookie, enum qat_device_gen qat_dev_gen);

int
qat_sym_process_response(void **op, uint8_t *resp,
		__rte_unused void *op_cookie, enum qat_device_gen qat_dev_gen);

void qat_sym_stats_get(struct rte_cryptodev *dev,
	struct rte_cryptodev_stats *stats);
void qat_sym_stats_reset(struct rte_cryptodev *dev);

int qat_sym_qp_setup(struct rte_cryptodev *dev, uint16_t queue_pair_id,
	const struct rte_cryptodev_qp_conf *rx_conf, int socket_id,
	struct rte_mempool *session_pool);
int qat_sym_qp_release(struct rte_cryptodev *dev,
	uint16_t queue_pair_id);


uint16_t
qat_sym_pmd_enqueue_op_burst(void *qp, struct rte_crypto_op **ops,
		uint16_t nb_ops);

uint16_t
qat_sym_pmd_dequeue_op_burst(void *qp, struct rte_crypto_op **ops,
		uint16_t nb_ops);

#endif /* _QAT_SYM_H_ */
