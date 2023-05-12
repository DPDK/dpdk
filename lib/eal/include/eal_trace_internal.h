/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Marvell International Ltd.
 */

#ifndef EAL_TRACE_INTERNAL_H
#define EAL_TRACE_INTERNAL_H

/**
 * @file
 *
 * API for EAL trace support
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <rte_alarm.h>
#include <rte_interrupts.h>
#include <rte_trace_point.h>

#include "eal_interrupts.h"

/* Alarm */
RTE_TRACE_POINT(
	rte_eal_trace_alarm_set,
	RTE_TRACE_POINT_ARGS(uint64_t us, rte_eal_alarm_callback cb_fn,
		void *cb_arg, int rc),
	rte_trace_point_emit_u64(us);
	rte_trace_point_emit_ptr(cb_fn);
	rte_trace_point_emit_ptr(cb_arg);
	rte_trace_point_emit_int(rc);
)

RTE_TRACE_POINT(
	rte_eal_trace_alarm_cancel,
	RTE_TRACE_POINT_ARGS(rte_eal_alarm_callback cb_fn, void *cb_arg,
		int count),
	rte_trace_point_emit_ptr(cb_fn);
	rte_trace_point_emit_ptr(cb_arg);
	rte_trace_point_emit_int(count);
)

/* Interrupt */
RTE_TRACE_POINT(
	rte_eal_trace_intr_callback_register,
	RTE_TRACE_POINT_ARGS(const struct rte_intr_handle *handle,
		rte_intr_callback_fn cb, void *cb_arg, int rc),
	rte_trace_point_emit_int(rc);
	rte_trace_point_emit_int(handle->dev_fd);
	rte_trace_point_emit_int(handle->fd);
	rte_trace_point_emit_int(handle->type);
	rte_trace_point_emit_u32(handle->max_intr);
	rte_trace_point_emit_u32(handle->nb_efd);
	rte_trace_point_emit_ptr(cb);
	rte_trace_point_emit_ptr(cb_arg);
)
RTE_TRACE_POINT(
	rte_eal_trace_intr_callback_unregister,
	RTE_TRACE_POINT_ARGS(const struct rte_intr_handle *handle,
		rte_intr_callback_fn cb, void *cb_arg, int rc),
	rte_trace_point_emit_int(rc);
	rte_trace_point_emit_int(handle->dev_fd);
	rte_trace_point_emit_int(handle->fd);
	rte_trace_point_emit_int(handle->type);
	rte_trace_point_emit_u32(handle->max_intr);
	rte_trace_point_emit_u32(handle->nb_efd);
	rte_trace_point_emit_ptr(cb);
	rte_trace_point_emit_ptr(cb_arg);
)
RTE_TRACE_POINT(
	rte_eal_trace_intr_enable,
	RTE_TRACE_POINT_ARGS(const struct rte_intr_handle *handle, int rc),
	rte_trace_point_emit_int(rc);
	rte_trace_point_emit_int(handle->dev_fd);
	rte_trace_point_emit_int(handle->fd);
	rte_trace_point_emit_int(handle->type);
	rte_trace_point_emit_u32(handle->max_intr);
	rte_trace_point_emit_u32(handle->nb_efd);
)
RTE_TRACE_POINT(
	rte_eal_trace_intr_disable,
	RTE_TRACE_POINT_ARGS(const struct rte_intr_handle *handle, int rc),
	rte_trace_point_emit_int(rc);
	rte_trace_point_emit_int(handle->dev_fd);
	rte_trace_point_emit_int(handle->fd);
	rte_trace_point_emit_int(handle->type);
	rte_trace_point_emit_u32(handle->max_intr);
	rte_trace_point_emit_u32(handle->nb_efd);
)

/* Memory */
RTE_TRACE_POINT(
	rte_eal_trace_mem_zmalloc,
	RTE_TRACE_POINT_ARGS(const char *type, size_t size, unsigned int align,
		int socket, void *ptr),
	rte_trace_point_emit_string(type);
	rte_trace_point_emit_size_t(size);
	rte_trace_point_emit_u32(align);
	rte_trace_point_emit_int(socket);
	rte_trace_point_emit_ptr(ptr);
)

RTE_TRACE_POINT(
	rte_eal_trace_mem_malloc,
	RTE_TRACE_POINT_ARGS(const char *type, size_t size, unsigned int align,
		int socket, void *ptr),
	rte_trace_point_emit_string(type);
	rte_trace_point_emit_size_t(size);
	rte_trace_point_emit_u32(align);
	rte_trace_point_emit_int(socket);
	rte_trace_point_emit_ptr(ptr);
)

RTE_TRACE_POINT(
	rte_eal_trace_mem_realloc,
	RTE_TRACE_POINT_ARGS(size_t size, unsigned int align, int socket,
		void *ptr),
	rte_trace_point_emit_size_t(size);
	rte_trace_point_emit_u32(align);
	rte_trace_point_emit_int(socket);
	rte_trace_point_emit_ptr(ptr);
)

RTE_TRACE_POINT(
	rte_eal_trace_mem_free,
	RTE_TRACE_POINT_ARGS(void *ptr),
	rte_trace_point_emit_ptr(ptr);
)

/* Memzone */
RTE_TRACE_POINT(
	rte_eal_trace_memzone_reserve,
	RTE_TRACE_POINT_ARGS(const char *name, size_t len, int socket_id,
		unsigned int flags, unsigned int align, unsigned int bound,
		const void *mz),
	rte_trace_point_emit_string(name);
	rte_trace_point_emit_size_t(len);
	rte_trace_point_emit_int(socket_id);
	rte_trace_point_emit_u32(flags);
	rte_trace_point_emit_u32(align);
	rte_trace_point_emit_u32(bound);
	rte_trace_point_emit_ptr(mz);
)

RTE_TRACE_POINT(
	rte_eal_trace_memzone_lookup,
	RTE_TRACE_POINT_ARGS(const char *name, const void *memzone),
	rte_trace_point_emit_string(name);
	rte_trace_point_emit_ptr(memzone);
)

RTE_TRACE_POINT(
	rte_eal_trace_memzone_free,
	RTE_TRACE_POINT_ARGS(const char *name, void *addr, int rc),
	rte_trace_point_emit_string(name);
	rte_trace_point_emit_ptr(addr);
	rte_trace_point_emit_int(rc);
)

/* Thread */
RTE_TRACE_POINT(
	rte_eal_trace_thread_remote_launch,
	RTE_TRACE_POINT_ARGS(int (*f)(void *), void *arg,
		unsigned int worker_id, int rc),
	rte_trace_point_emit_ptr(f);
	rte_trace_point_emit_ptr(arg);
	rte_trace_point_emit_u32(worker_id);
	rte_trace_point_emit_int(rc);
)
RTE_TRACE_POINT(
	rte_eal_trace_thread_lcore_ready,
	RTE_TRACE_POINT_ARGS(unsigned int lcore_id, const char *cpuset),
	rte_trace_point_emit_u32(lcore_id);
	rte_trace_point_emit_string(cpuset);
)
RTE_TRACE_POINT(
	rte_eal_trace_thread_lcore_running,
	RTE_TRACE_POINT_ARGS(unsigned int lcore_id, void *f),
	rte_trace_point_emit_u32(lcore_id);
	rte_trace_point_emit_ptr(f);
)
RTE_TRACE_POINT(
	rte_eal_trace_thread_lcore_stopped,
	RTE_TRACE_POINT_ARGS(unsigned int lcore_id),
	rte_trace_point_emit_u32(lcore_id);
)

/* Service */
RTE_TRACE_POINT(
	rte_eal_trace_service_map_lcore,
	RTE_TRACE_POINT_ARGS(unsigned int id, unsigned int lcore_id, unsigned int enabled),
	rte_trace_point_emit_u32(id);
	rte_trace_point_emit_u32(lcore_id);
	rte_trace_point_emit_u32(enabled);
)
RTE_TRACE_POINT(
	rte_eal_trace_service_lcore_state_change,
	RTE_TRACE_POINT_ARGS(unsigned int lcore_id, int lcore_state),
	rte_trace_point_emit_u32(lcore_id);
	rte_trace_point_emit_i32(lcore_state);
)
RTE_TRACE_POINT(
	rte_eal_trace_service_lcore_start,
	RTE_TRACE_POINT_ARGS(unsigned int lcore_id),
	rte_trace_point_emit_u32(lcore_id);
)
RTE_TRACE_POINT(
	rte_eal_trace_service_lcore_stop,
	RTE_TRACE_POINT_ARGS(unsigned int lcore_id),
	rte_trace_point_emit_u32(lcore_id);
)
RTE_TRACE_POINT(
	rte_eal_trace_service_run_begin,
	RTE_TRACE_POINT_ARGS(unsigned int id, unsigned int lcore_id),
	rte_trace_point_emit_u32(id);
	rte_trace_point_emit_u32(lcore_id);
)
RTE_TRACE_POINT(
	rte_eal_trace_service_runstate_set,
	RTE_TRACE_POINT_ARGS(unsigned int id, unsigned int run_state),
	rte_trace_point_emit_u32(id);
	rte_trace_point_emit_u32(run_state);
)
RTE_TRACE_POINT(
	rte_eal_trace_service_run_end,
	RTE_TRACE_POINT_ARGS(unsigned int id, unsigned int lcore_id),
	rte_trace_point_emit_u32(id);
	rte_trace_point_emit_u32(lcore_id);
)
RTE_TRACE_POINT(
	rte_eal_trace_service_component_register,
	RTE_TRACE_POINT_ARGS(int id, const char *service_name),
	rte_trace_point_emit_i32(id);
	rte_trace_point_emit_string(service_name);
)

#ifdef __cplusplus
}
#endif

#endif /* EAL_TRACE_INTERNAL_H */
