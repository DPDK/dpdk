/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Marvell International Ltd.
 */

#ifndef CRYPTODEV_TRACE_H
#define CRYPTODEV_TRACE_H

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
	RTE_TRACE_POINT_ARGS(uint8_t dev_id, void *sess, void *xforms,
		void *mempool),
	rte_trace_point_emit_u8(dev_id);
	rte_trace_point_emit_ptr(sess);
	rte_trace_point_emit_ptr(xforms);
	rte_trace_point_emit_ptr(mempool);
)

RTE_TRACE_POINT(
	rte_cryptodev_trace_asym_session_pool_create,
	RTE_TRACE_POINT_ARGS(const char *name, uint32_t nb_elts,
		uint16_t user_data_size, uint32_t cache_size, void *mempool),
	rte_trace_point_emit_string(name);
	rte_trace_point_emit_u32(nb_elts);
	rte_trace_point_emit_u16(user_data_size);
	rte_trace_point_emit_u32(cache_size);
	rte_trace_point_emit_ptr(mempool);
)

RTE_TRACE_POINT(
	rte_cryptodev_trace_asym_session_create,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id, void *xforms, void *mempool,
			void *sess),
	rte_trace_point_emit_u8(dev_id);
	rte_trace_point_emit_ptr(xforms);
	rte_trace_point_emit_ptr(mempool);
	rte_trace_point_emit_ptr(sess);
)

RTE_TRACE_POINT(
	rte_cryptodev_trace_sym_session_free,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id, void *sess),
	rte_trace_point_emit_u8(dev_id);
	rte_trace_point_emit_ptr(sess);
)

RTE_TRACE_POINT(
	rte_cryptodev_trace_asym_session_free,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id, void *sess),
	rte_trace_point_emit_u8(dev_id);
	rte_trace_point_emit_ptr(sess);
)

RTE_TRACE_POINT(
	rte_cryptodev_trace_callback_register,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id,
		enum rte_cryptodev_event_type event, const void *cb_fn),
	rte_trace_point_emit_u8(dev_id);
	rte_trace_point_emit_int(event);
	rte_trace_point_emit_ptr(cb_fn);
)

RTE_TRACE_POINT(
	rte_cryptodev_trace_callback_unregister,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id,
		enum rte_cryptodev_event_type event, const void *cb_fn),
	rte_trace_point_emit_u8(dev_id);
	rte_trace_point_emit_int(event);
	rte_trace_point_emit_ptr(cb_fn);
)

RTE_TRACE_POINT(
	rte_cryptodev_trace_device_count_by_driver,
	RTE_TRACE_POINT_ARGS(uint8_t driver_id, uint8_t dev_count),
	rte_trace_point_emit_u8(driver_id);
	rte_trace_point_emit_u8(dev_count);
)

RTE_TRACE_POINT(
	rte_cryptodev_trace_devices_get,
	RTE_TRACE_POINT_ARGS(const char *driver_name, uint8_t count),
	rte_trace_point_emit_string(driver_name);
	rte_trace_point_emit_u8(count);
)

RTE_TRACE_POINT(
	rte_cryptodev_trace_driver_id_get,
	RTE_TRACE_POINT_ARGS(const char *name, int driver_id),
	rte_trace_point_emit_string(name);
	rte_trace_point_emit_int(driver_id);
)

RTE_TRACE_POINT(
	rte_cryptodev_trace_driver_name_get,
	RTE_TRACE_POINT_ARGS(uint8_t driver_id, const char *name),
	rte_trace_point_emit_u8(driver_id);
	rte_trace_point_emit_string(name);
)

RTE_TRACE_POINT(
	rte_cryptodev_trace_get_aead_algo_enum,
	RTE_TRACE_POINT_ARGS(const char *algo_string,
		enum rte_crypto_aead_algorithm algo_enum, int ret),
	rte_trace_point_emit_string(algo_string);
	rte_trace_point_emit_int(algo_enum);
	rte_trace_point_emit_int(ret);
)

RTE_TRACE_POINT(
	rte_cryptodev_trace_get_auth_algo_enum,
	RTE_TRACE_POINT_ARGS(const char *algo_string,
		enum rte_crypto_auth_algorithm algo_enum, int ret),
	rte_trace_point_emit_string(algo_string);
	rte_trace_point_emit_int(algo_enum);
	rte_trace_point_emit_int(ret);
)

RTE_TRACE_POINT(
	rte_cryptodev_trace_get_cipher_algo_enum,
	RTE_TRACE_POINT_ARGS(const char *algo_string,
		enum rte_crypto_cipher_algorithm algo_enum, int ret),
	rte_trace_point_emit_string(algo_string);
	rte_trace_point_emit_int(algo_enum);
	rte_trace_point_emit_int(ret);
)

RTE_TRACE_POINT(
	rte_cryptodev_trace_get_aead_algo_string,
	RTE_TRACE_POINT_ARGS(enum rte_crypto_aead_algorithm algo_enum,
		const char *algo_string),
	rte_trace_point_emit_int(algo_enum);
	rte_trace_point_emit_string(algo_string);
)

RTE_TRACE_POINT(
	rte_cryptodev_trace_get_auth_algo_string,
	RTE_TRACE_POINT_ARGS(enum rte_crypto_auth_algorithm algo_enum,
		const char *algo_string),
	rte_trace_point_emit_int(algo_enum);
	rte_trace_point_emit_string(algo_string);
)

RTE_TRACE_POINT(
	rte_cryptodev_trace_get_cipher_algo_string,
	RTE_TRACE_POINT_ARGS(enum rte_crypto_cipher_algorithm algo_enum,
		const char *algo_string),
	rte_trace_point_emit_int(algo_enum);
	rte_trace_point_emit_string(algo_string);
)

RTE_TRACE_POINT(
	rte_cryptodev_trace_get_dev_id,
	RTE_TRACE_POINT_ARGS(const char *name, int ret),
	rte_trace_point_emit_string(name);
	rte_trace_point_emit_int(ret);
)

RTE_TRACE_POINT(
	rte_cryptodev_trace_get_feature_name,
	RTE_TRACE_POINT_ARGS(uint64_t flag),
	rte_trace_point_emit_u64(flag);
)

RTE_TRACE_POINT(
	rte_cryptodev_trace_get_sec_ctx,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id, const void *sec_ctx),
	rte_trace_point_emit_u8(dev_id);
	rte_trace_point_emit_ptr(sec_ctx);
)

RTE_TRACE_POINT(
	rte_cryptodev_trace_info_get,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id, const char *driver_name),
	rte_trace_point_emit_u8(dev_id);
	rte_trace_point_emit_string(driver_name);
)

RTE_TRACE_POINT(
	rte_cryptodev_trace_is_valid_dev,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id, unsigned int ret),
	rte_trace_point_emit_u8(dev_id);
	rte_trace_point_emit_u32(ret);
)

RTE_TRACE_POINT(
	rte_cryptodev_trace_name_get,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id, const char *name),
	rte_trace_point_emit_u8(dev_id);
	rte_trace_point_emit_string(name);
)

RTE_TRACE_POINT(
	rte_cryptodev_trace_queue_pair_count,
	RTE_TRACE_POINT_ARGS(const void *dev, const char *name,
		uint8_t socket_id, uint8_t dev_id, uint16_t nb_queue_pairs),
	rte_trace_point_emit_ptr(dev);
	rte_trace_point_emit_string(name);
	rte_trace_point_emit_u8(socket_id);
	rte_trace_point_emit_u8(dev_id);
	rte_trace_point_emit_u16(nb_queue_pairs);
)

RTE_TRACE_POINT(
	rte_cryptodev_trace_socket_id,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id, const char *name, int socket_id),
	rte_trace_point_emit_u8(dev_id);
	rte_trace_point_emit_string(name);
	rte_trace_point_emit_int(socket_id);
)

RTE_TRACE_POINT(
	rte_cryptodev_trace_stats_get,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id,
		const struct rte_cryptodev_stats *stats),
	rte_trace_point_emit_u8(dev_id);
	rte_trace_point_emit_u64(stats->enqueued_count);
	rte_trace_point_emit_u64(stats->dequeued_count);
	rte_trace_point_emit_u64(stats->enqueue_err_count);
	rte_trace_point_emit_u64(stats->dequeue_err_count);
)

RTE_TRACE_POINT(
	rte_cryptodev_trace_stats_reset,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id),
	rte_trace_point_emit_u8(dev_id);
)

RTE_TRACE_POINT(
	rte_cryptodev_trace_sym_capability_check_aead,
	RTE_TRACE_POINT_ARGS(
		const struct rte_cryptodev_symmetric_capability *capability,
		uint16_t key_size, uint16_t digest_size, uint16_t aad_size,
		uint16_t iv_size, int ret),
	rte_trace_point_emit_ptr(capability);
	rte_trace_point_emit_int(capability->xform_type);
	rte_trace_point_emit_u16(key_size);
	rte_trace_point_emit_u16(digest_size);
	rte_trace_point_emit_u16(aad_size);
	rte_trace_point_emit_u16(iv_size);
	rte_trace_point_emit_int(ret);
)

RTE_TRACE_POINT(
	rte_cryptodev_trace_sym_capability_check_auth,
	RTE_TRACE_POINT_ARGS(
		const struct rte_cryptodev_symmetric_capability *capability,
		uint16_t key_size, uint16_t digest_size, uint16_t iv_size,
		int ret),
	rte_trace_point_emit_ptr(capability);
	rte_trace_point_emit_int(capability->xform_type);
	rte_trace_point_emit_u16(key_size);
	rte_trace_point_emit_u16(digest_size);
	rte_trace_point_emit_u16(iv_size);
	rte_trace_point_emit_int(ret);
)

RTE_TRACE_POINT(
	rte_cryptodev_trace_sym_capability_check_cipher,
	RTE_TRACE_POINT_ARGS(
		const struct rte_cryptodev_symmetric_capability *capability,
		uint16_t key_size, uint16_t iv_size, int ret),
	rte_trace_point_emit_ptr(capability);
	rte_trace_point_emit_int(capability->xform_type);
	rte_trace_point_emit_u16(key_size);
	rte_trace_point_emit_u16(iv_size);
	rte_trace_point_emit_int(ret);
)

RTE_TRACE_POINT(
	rte_cryptodev_trace_sym_capability_get,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id, const char *driver_name,
		uint8_t driver_id, int idx_type, const void *sym_capability),
	rte_trace_point_emit_u8(dev_id);
	rte_trace_point_emit_string(driver_name);
	rte_trace_point_emit_u8(driver_id);
	rte_trace_point_emit_int(idx_type);
	rte_trace_point_emit_ptr(sym_capability);
)

RTE_TRACE_POINT(
	rte_cryptodev_trace_sym_get_private_session_size,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id, uint32_t priv_sess_size),
	rte_trace_point_emit_u8(dev_id);
	rte_trace_point_emit_u32(priv_sess_size);
)

RTE_TRACE_POINT(
	rte_cryptodev_trace_asym_capability_get,
	RTE_TRACE_POINT_ARGS(const char *driver_name, uint8_t driver_id,
		int idx_type, const void *asym_cap),
	rte_trace_point_emit_string(driver_name);
	rte_trace_point_emit_u8(driver_id);
	rte_trace_point_emit_int(idx_type);
	rte_trace_point_emit_ptr(asym_cap);
)

RTE_TRACE_POINT(
	rte_cryptodev_trace_asym_get_private_session_size,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id, uint32_t priv_sess_size),
	rte_trace_point_emit_u8(dev_id);
	rte_trace_point_emit_u32(priv_sess_size);
)

RTE_TRACE_POINT(
	rte_cryptodev_trace_asym_get_xform_enum,
	RTE_TRACE_POINT_ARGS(const char *xform_string,
		enum rte_crypto_asym_xform_type xform_enum, int ret),
	rte_trace_point_emit_string(xform_string);
	rte_trace_point_emit_int(xform_enum);
	rte_trace_point_emit_int(ret);
)

RTE_TRACE_POINT(
	rte_cryptodev_trace_asym_get_xform_string,
	RTE_TRACE_POINT_ARGS(enum rte_crypto_asym_xform_type xform_enum,
		const char *xform_string),
	rte_trace_point_emit_int(xform_enum);
	rte_trace_point_emit_string(xform_string);
)

RTE_TRACE_POINT(
	rte_cryptodev_trace_asym_xform_capability_check_modlen,
	RTE_TRACE_POINT_ARGS(const void *capability, uint16_t modlen, int ret),
	rte_trace_point_emit_ptr(capability);
	rte_trace_point_emit_u16(modlen);
	rte_trace_point_emit_int(ret);
)

RTE_TRACE_POINT(
	rte_cryptodev_trace_sym_cpu_crypto_process,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id, const void *sess),
	rte_trace_point_emit_u8(dev_id);
	rte_trace_point_emit_ptr(sess);
)

RTE_TRACE_POINT(
	rte_cryptodev_trace_sym_session_get_user_data,
	RTE_TRACE_POINT_ARGS(const void *sess, const void *data),
	rte_trace_point_emit_ptr(sess);
	rte_trace_point_emit_ptr(data);
)

RTE_TRACE_POINT(
	rte_cryptodev_trace_sym_session_set_user_data,
	RTE_TRACE_POINT_ARGS(const void *sess, const void *data, uint16_t size),
	rte_trace_point_emit_ptr(sess);
	rte_trace_point_emit_ptr(data);
	rte_trace_point_emit_u16(size);
)

RTE_TRACE_POINT(
	rte_cryptodev_trace_get_qp_status,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id, uint16_t queue_pair_id, int ret),
	rte_trace_point_emit_u8(dev_id);
	rte_trace_point_emit_u16(queue_pair_id);
	rte_trace_point_emit_int(ret);
)

RTE_TRACE_POINT(
	rte_cryptodev_trace_configure_raw_dp_ctx,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id, uint16_t qp_id, int sess_type),
	rte_trace_point_emit_u8(dev_id);
	rte_trace_point_emit_u16(qp_id);
	rte_trace_point_emit_int(sess_type);
)

RTE_TRACE_POINT(
	rte_cryptodev_trace_get_raw_dp_ctx_size,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id),
	rte_trace_point_emit_u8(dev_id);
)

RTE_TRACE_POINT(
	rte_cryptodev_trace_add_deq_callback,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id, uint16_t qp_id, const void *cb_fn),
	rte_trace_point_emit_u8(dev_id);
	rte_trace_point_emit_u16(qp_id);
	rte_trace_point_emit_ptr(cb_fn);
)

RTE_TRACE_POINT(
	rte_cryptodev_trace_add_enq_callback,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id, uint16_t qp_id, const void *cb_fn),
	rte_trace_point_emit_u8(dev_id);
	rte_trace_point_emit_u16(qp_id);
	rte_trace_point_emit_ptr(cb_fn);
)

RTE_TRACE_POINT(
	rte_cryptodev_trace_remove_deq_callback,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id, uint16_t qp_id, const void *fn),
	rte_trace_point_emit_u8(dev_id);
	rte_trace_point_emit_u16(qp_id);
	rte_trace_point_emit_ptr(fn);
)

RTE_TRACE_POINT(
	rte_cryptodev_trace_remove_enq_callback,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id, uint16_t qp_id, const void *fn),
	rte_trace_point_emit_u8(dev_id);
	rte_trace_point_emit_u16(qp_id);
	rte_trace_point_emit_ptr(fn);
)

RTE_TRACE_POINT(
	rte_cryptodev_trace_asym_session_get_user_data,
	RTE_TRACE_POINT_ARGS(const void *sess, const void *data),
	rte_trace_point_emit_ptr(sess);
	rte_trace_point_emit_ptr(data);
)

RTE_TRACE_POINT(
	rte_cryptodev_trace_asym_session_set_user_data,
	RTE_TRACE_POINT_ARGS(const void *sess, const void *data, uint16_t size),
	rte_trace_point_emit_ptr(sess);
	rte_trace_point_emit_ptr(data);
	rte_trace_point_emit_u16(size);
)

RTE_TRACE_POINT(
	rte_cryptodev_trace_session_event_mdata_set,
	RTE_TRACE_POINT_ARGS(uint8_t dev_id, const void *sess, int op_type,
		int sess_type, const void *ev_mdata, uint16_t size),
	rte_trace_point_emit_u8(dev_id);
	rte_trace_point_emit_ptr(sess);
	rte_trace_point_emit_int(op_type);
	rte_trace_point_emit_int(sess_type);
	rte_trace_point_emit_ptr(ev_mdata);
	rte_trace_point_emit_u16(size);
)

RTE_TRACE_POINT(
	rte_cryptodev_trace_allocate_driver,
	RTE_TRACE_POINT_ARGS(const char *name),
	rte_trace_point_emit_string(name);
)

RTE_TRACE_POINT(
	rte_cryptodev_trace_op_pool_create,
	RTE_TRACE_POINT_ARGS(const char *name, int socket_id, int type,
		uint32_t nb_elts, const void *mp),
	rte_trace_point_emit_string(name);
	rte_trace_point_emit_int(socket_id);
	rte_trace_point_emit_int(type);
	rte_trace_point_emit_u32(nb_elts);
	rte_trace_point_emit_ptr(mp);
)

RTE_TRACE_POINT(
	rte_cryptodev_trace_asym_xform_capability_check_optype,
	RTE_TRACE_POINT_ARGS(uint32_t op_types,
		enum rte_crypto_asym_op_type op_type, int ret),
	rte_trace_point_emit_u32(op_types);
	rte_trace_point_emit_int(op_type);
	rte_trace_point_emit_int(ret);
)

RTE_TRACE_POINT(
	rte_cryptodev_trace_count,
	RTE_TRACE_POINT_ARGS(uint8_t nb_devs),
	rte_trace_point_emit_u8(nb_devs);
)

#ifdef __cplusplus
}
#endif

#endif /* CRYPTODEV_TRACE_H */
