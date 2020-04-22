/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Marvell International Ltd.
 */

#ifndef _RTE_CRYPTODEV_TRACE_H_
#define _RTE_CRYPTODEV_TRACE_H_

/**
 * @file
 *
 * API for cryptodev trace support
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <rte_trace_point.h>

#include "rte_cryptodev.h"

RTE_TRACE_POINT(
	rte_cryptodev_trace_configure,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id,
		struct rte_cryptodev_config *conf),
	rte_trace_point_emit_u8(dev_id);
	rte_trace_point_emit_u16(conf->nb_queue_pairs);
	rte_trace_point_emit_i64(conf->ff_disable);
)

RTE_TRACE_POINT(
	rte_cryptodev_trace_start,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id, int rc),
	rte_trace_point_emit_u8(dev_id);
	rte_trace_point_emit_int(rc);
)

RTE_TRACE_POINT(
	rte_cryptodev_trace_stop,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id),
	rte_trace_point_emit_u8(dev_id);
)

RTE_TRACE_POINT(
	rte_cryptodev_trace_close,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id, int rc),
	rte_trace_point_emit_u8(dev_id);
	rte_trace_point_emit_int(rc);
)

RTE_TRACE_POINT(
	rte_cryptodev_trace_queue_pair_setup,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id, uint16_t queue_pair_id,
		const struct rte_cryptodev_qp_conf *conf),
	rte_trace_point_emit_u8(dev_id);
	rte_trace_point_emit_u16(queue_pair_id);
	rte_trace_point_emit_u32(conf->nb_descriptors);
	rte_trace_point_emit_ptr(conf->mp_session);
	rte_trace_point_emit_ptr(conf->mp_session_private);
)

RTE_TRACE_POINT(
	rte_cryptodev_trace_sym_session_pool_create,
	RTE_TRACE_POINT_ARGS(const char *name, uint32_t nb_elts,
		uint32_t elt_size, uint32_t cache_size,
		uint16_t user_data_size, void *mempool),
	rte_trace_point_emit_string(name);
	rte_trace_point_emit_u32(nb_elts);
	rte_trace_point_emit_u32(elt_size);
	rte_trace_point_emit_u32(cache_size);
	rte_trace_point_emit_u16(user_data_size);
	rte_trace_point_emit_ptr(mempool);
)

RTE_TRACE_POINT(
	rte_cryptodev_trace_sym_session_create,
	RTE_TRACE_POINT_ARGS(void *mempool,
		struct rte_cryptodev_sym_session *sess),
	rte_trace_point_emit_ptr(mempool);
	rte_trace_point_emit_ptr(sess);
	rte_trace_point_emit_u64(sess->opaque_data);
	rte_trace_point_emit_u16(sess->nb_drivers);
	rte_trace_point_emit_u16(sess->user_data_sz);
)

RTE_TRACE_POINT(
	rte_cryptodev_trace_asym_session_create,
	RTE_TRACE_POINT_ARGS(void *mempool,
		struct rte_cryptodev_asym_session *sess),
	rte_trace_point_emit_ptr(mempool);
	rte_trace_point_emit_ptr(sess);
)

RTE_TRACE_POINT(
	rte_cryptodev_trace_sym_session_free,
	RTE_TRACE_POINT_ARGS(struct rte_cryptodev_sym_session *sess),
	rte_trace_point_emit_ptr(sess);
)

RTE_TRACE_POINT(
	rte_cryptodev_trace_asym_session_free,
	RTE_TRACE_POINT_ARGS(struct rte_cryptodev_asym_session *sess),
	rte_trace_point_emit_ptr(sess);
)

RTE_TRACE_POINT(
	rte_cryptodev_trace_sym_session_init,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id,
		struct rte_cryptodev_sym_session *sess, void *xforms,
		void *mempool),
	rte_trace_point_emit_u8(dev_id);
	rte_trace_point_emit_ptr(sess);
	rte_trace_point_emit_u64(sess->opaque_data);
	rte_trace_point_emit_u16(sess->nb_drivers);
	rte_trace_point_emit_u16(sess->user_data_sz);
	rte_trace_point_emit_ptr(xforms);
	rte_trace_point_emit_ptr(mempool);
)

RTE_TRACE_POINT(
	rte_cryptodev_trace_asym_session_init,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id,
		struct rte_cryptodev_asym_session *sess, void *xforms,
		void *mempool),
	rte_trace_point_emit_u8(dev_id);
	rte_trace_point_emit_ptr(sess);
	rte_trace_point_emit_ptr(xforms);
	rte_trace_point_emit_ptr(mempool);
)

RTE_TRACE_POINT(
	rte_cryptodev_trace_sym_session_clear,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id, void *sess),
	rte_trace_point_emit_u8(dev_id);
	rte_trace_point_emit_ptr(sess);
)

RTE_TRACE_POINT(
	rte_cryptodev_trace_asym_session_clear,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id, void *sess),
	rte_trace_point_emit_u8(dev_id);
	rte_trace_point_emit_ptr(sess);
)

#ifdef __cplusplus
}
#endif

#endif /* _RTE_CRYPTODEV_TRACE_H_ */
