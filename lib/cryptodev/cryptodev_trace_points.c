/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Marvell International Ltd.
 */

#include <rte_trace_point_register.h>

#include "cryptodev_trace.h"

RTE_TRACE_POINT_REGISTER(rte_cryptodev_trace_configure,
	lib.cryptodev.configure)

RTE_TRACE_POINT_REGISTER(rte_cryptodev_trace_start,
	lib.cryptodev.start)

RTE_TRACE_POINT_REGISTER(rte_cryptodev_trace_stop,
	lib.cryptodev.stop)

RTE_TRACE_POINT_REGISTER(rte_cryptodev_trace_close,
	lib.cryptodev.close)

RTE_TRACE_POINT_REGISTER(rte_cryptodev_trace_queue_pair_setup,
	lib.cryptodev.queue.pair.setup)

RTE_TRACE_POINT_REGISTER(rte_cryptodev_trace_sym_session_pool_create,
	lib.cryptodev.sym.pool.create)

RTE_TRACE_POINT_REGISTER(rte_cryptodev_trace_asym_session_pool_create,
	lib.cryptodev.asym.pool.create)

RTE_TRACE_POINT_REGISTER(rte_cryptodev_trace_sym_session_create,
	lib.cryptodev.sym.create)

RTE_TRACE_POINT_REGISTER(rte_cryptodev_trace_asym_session_create,
	lib.cryptodev.asym.create)

RTE_TRACE_POINT_REGISTER(rte_cryptodev_trace_sym_session_free,
	lib.cryptodev.sym.free)

RTE_TRACE_POINT_REGISTER(rte_cryptodev_trace_asym_session_free,
	lib.cryptodev.asym.free)

RTE_TRACE_POINT_REGISTER(rte_cryptodev_trace_enqueue_burst,
	lib.cryptodev.enq.burst)

RTE_TRACE_POINT_REGISTER(rte_cryptodev_trace_dequeue_burst,
	lib.cryptodev.deq.burst)

RTE_TRACE_POINT_REGISTER(rte_cryptodev_trace_callback_register,
	lib.cryptodev.callback.register)

RTE_TRACE_POINT_REGISTER(rte_cryptodev_trace_callback_unregister,
	lib.cryptodev.callback.unregister)

RTE_TRACE_POINT_REGISTER(rte_cryptodev_trace_device_count_by_driver,
	lib.cryptodev.device.count.by.driver)

RTE_TRACE_POINT_REGISTER(rte_cryptodev_trace_devices_get,
	lib.cryptodev.devices.get)

RTE_TRACE_POINT_REGISTER(rte_cryptodev_trace_driver_id_get,
	lib.cryptodev.driver.id.get)

RTE_TRACE_POINT_REGISTER(rte_cryptodev_trace_driver_name_get,
	lib.cryptodev.driver.name.get)

RTE_TRACE_POINT_REGISTER(rte_cryptodev_trace_get_aead_algo_enum,
	lib.cryptodev.get.aead.algo.enum)

RTE_TRACE_POINT_REGISTER(rte_cryptodev_trace_get_auth_algo_enum,
	lib.cryptodev.get.auth.algo.enum)

RTE_TRACE_POINT_REGISTER(rte_cryptodev_trace_get_cipher_algo_enum,
	lib.cryptodev.get.cipher.algo.enum)

RTE_TRACE_POINT_REGISTER(rte_cryptodev_trace_get_aead_algo_string,
	lib.cryptodev.get.aead.algo.string)

RTE_TRACE_POINT_REGISTER(rte_cryptodev_trace_get_auth_algo_string,
	lib.cryptodev.get.auth.algo.string)

RTE_TRACE_POINT_REGISTER(rte_cryptodev_trace_get_cipher_algo_string,
	lib.cryptodev.get.cipher.algo.string)

RTE_TRACE_POINT_REGISTER(rte_cryptodev_trace_get_dev_id,
	lib.cryptodev.get.dev.id)

RTE_TRACE_POINT_REGISTER(rte_cryptodev_trace_get_feature_name,
	lib.cryptodev.get.feature.name)

RTE_TRACE_POINT_REGISTER(rte_cryptodev_trace_get_sec_ctx,
	lib.cryptodev.get.sec.ctx)

RTE_TRACE_POINT_REGISTER(rte_cryptodev_trace_info_get,
	lib.cryptodev.info.get)

RTE_TRACE_POINT_REGISTER(rte_cryptodev_trace_is_valid_dev,
	lib.cryptodev.is.valid.dev)

RTE_TRACE_POINT_REGISTER(rte_cryptodev_trace_name_get,
	lib.cryptodev.name.get)

RTE_TRACE_POINT_REGISTER(rte_cryptodev_trace_queue_pair_count,
	lib.cryptodev.queue.pair.count)

RTE_TRACE_POINT_REGISTER(rte_cryptodev_trace_socket_id,
	lib.cryptodev.socket.id)

RTE_TRACE_POINT_REGISTER(rte_cryptodev_trace_stats_get,
	lib.cryptodev.stats.get)

RTE_TRACE_POINT_REGISTER(rte_cryptodev_trace_stats_reset,
	lib.cryptodev.stats.reset)

RTE_TRACE_POINT_REGISTER(rte_cryptodev_trace_sym_capability_check_aead,
	lib.cryptodev.sym.capability.check.aead)

RTE_TRACE_POINT_REGISTER(rte_cryptodev_trace_sym_capability_check_auth,
	lib.cryptodev.sym.capability.check.auth)

RTE_TRACE_POINT_REGISTER(rte_cryptodev_trace_sym_capability_check_cipher,
	lib.cryptodev.sym.capability.check.cipher)

RTE_TRACE_POINT_REGISTER(rte_cryptodev_trace_sym_capability_get,
	lib.cryptodev.sym.capability.get)

RTE_TRACE_POINT_REGISTER(rte_cryptodev_trace_sym_get_private_session_size,
	lib.cryptodev.sym.get.private.session.size)

RTE_TRACE_POINT_REGISTER(rte_cryptodev_trace_asym_capability_get,
	lib.cryptodev.asym.capability.get)

RTE_TRACE_POINT_REGISTER(rte_cryptodev_trace_asym_get_private_session_size,
	lib.cryptodev.asym.get.private.session.size)

RTE_TRACE_POINT_REGISTER(rte_cryptodev_trace_asym_get_xform_enum,
	lib.cryptodev.asym.get.xform.enum)

RTE_TRACE_POINT_REGISTER(rte_cryptodev_trace_asym_get_xform_string,
	lib.cryptodev.asym.get.xform.string)

RTE_TRACE_POINT_REGISTER(rte_cryptodev_trace_asym_xform_capability_check_modlen,
	lib.cryptodev.asym.xform.capability.check.modlen)

RTE_TRACE_POINT_REGISTER(rte_cryptodev_trace_asym_xform_capability_check_optype,
	lib.cryptodev.asym.xform.capability.check.optype)

RTE_TRACE_POINT_REGISTER(rte_cryptodev_trace_sym_cpu_crypto_process,
	lib.cryptodev.sym.cpu.crypto.process)

RTE_TRACE_POINT_REGISTER(rte_cryptodev_trace_sym_session_get_user_data,
	lib.cryptodev.sym.session.get.user.data)

RTE_TRACE_POINT_REGISTER(rte_cryptodev_trace_sym_session_set_user_data,
	lib.cryptodev.sym.session.set.user.data)

RTE_TRACE_POINT_REGISTER(rte_cryptodev_trace_get_qp_status,
	lib.cryptodev.get.qp.status)

RTE_TRACE_POINT_REGISTER(rte_cryptodev_trace_configure_raw_dp_ctx,
	lib.cryptodev.configure.raw.dp.ctx)

RTE_TRACE_POINT_REGISTER(rte_cryptodev_trace_get_raw_dp_ctx_size,
	lib.cryptodev.get.raw.dp.ctx.size)

RTE_TRACE_POINT_REGISTER(rte_cryptodev_trace_add_deq_callback,
	lib.cryptodev.add.deq.callback)

RTE_TRACE_POINT_REGISTER(rte_cryptodev_trace_add_enq_callback,
	lib.cryptodev.add.enq.callback)

RTE_TRACE_POINT_REGISTER(rte_cryptodev_trace_remove_deq_callback,
	lib.cryptodev.remove.deq.callback)

RTE_TRACE_POINT_REGISTER(rte_cryptodev_trace_remove_enq_callback,
	lib.cryptodev.remove.enq.callback)

RTE_TRACE_POINT_REGISTER(rte_cryptodev_trace_asym_session_get_user_data,
	lib.cryptodev.asym.session.get.user.data)

RTE_TRACE_POINT_REGISTER(rte_cryptodev_trace_asym_session_set_user_data,
	lib.cryptodev.asym.session.set.user.data)

RTE_TRACE_POINT_REGISTER(rte_cryptodev_trace_session_event_mdata_set,
	lib.cryptodev.session.event.mdata.set)

RTE_TRACE_POINT_REGISTER(rte_cryptodev_trace_allocate_driver,
	lib.cryptodev.allocate.driver)

RTE_TRACE_POINT_REGISTER(rte_cryptodev_trace_op_pool_create,
	lib.cryptodev.op.pool.create)

RTE_TRACE_POINT_REGISTER(rte_cryptodev_trace_count,
	lib.cryptodev.count)
