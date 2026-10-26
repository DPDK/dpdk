/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2026 Broadcom
 * All rights reserved.
 */

#include <errno.h>
#include <limits.h>
#include <stdlib.h>

#include <rte_alarm.h>
#include <rte_malloc.h>

#include "bnxt.h"
#include "bnxt_cpr.h"
#include "bnxt_func_error.h"
#include "bnxt_hwrm.h"

static void bnxt_func_error_poll_cb(void *arg);

int bnxt_func_error_info_alloc(struct bnxt *bp)
{
	bp->func_error_info = rte_zmalloc("bnxt_func_error_info",
					   sizeof(struct bnxt_func_error_info), 0);
	if (bp->func_error_info == NULL)
		return -ENOMEM;

	return 0;
}

void bnxt_func_error_info_free(struct bnxt *bp)
{
	rte_eal_alarm_cancel(bnxt_func_error_recovery_cb, (void *)bp);
	rte_eal_alarm_cancel(bnxt_func_error_poll_cb, (void *)bp);
	rte_free(bp->func_error_info);
	bp->func_error_info = NULL;
}

void bnxt_func_error_clear_pending(struct bnxt *bp)
{
	rte_atomic_store_explicit(&bp->func_error_info->recovery_pending, 0,
				  rte_memory_order_release);
}

static bool bnxt_func_error_acquire_pending(struct bnxt *bp)
{
	unsigned int expected = 0;

	return rte_atomic_compare_exchange_strong_explicit(&bp->func_error_info->recovery_pending,
							    &expected, 1,
							    rte_memory_order_acq_rel,
							    rte_memory_order_acquire);
}

void bnxt_func_error_trigger_recovery(struct bnxt *bp)
{
	if (!bnxt_func_error_acquire_pending(bp))
		return;

	PMD_DRV_LOG_LINE(ERR,
		    "Port %u: HWRM_ERR_CODE_FUNC_RESET_REQUIRED -- function is in "
		    "FUNC_ERROR state, scheduling recovery",
		    bp->eth_dev->data->port_id);

	if (rte_eal_alarm_set(US_PER_MS, bnxt_func_error_recovery_cb, (void *)bp)) {
		PMD_DRV_LOG_LINE(ERR,
			    "Port %u: Failed to schedule FUNC_ERROR recovery",
			    bp->eth_dev->data->port_id);
		bnxt_func_error_clear_pending(bp);
	}
}

void bnxt_func_error_recovery_cb(void *arg)
{
	struct bnxt *bp = arg;
	int rc;

	pthread_mutex_lock(&bp->err_recovery_lock);
	if (bp->flags & BNXT_FLAG_FW_RESET) {
		pthread_mutex_unlock(&bp->err_recovery_lock);
		bnxt_func_error_clear_pending(bp);
		return;
	}
	bp->flags |= BNXT_FLAG_FW_RESET;

	rte_atomic_fetch_add_explicit(&bp->func_error_info->recovery_count, 1,
				      rte_memory_order_relaxed);
	bp->fw_reset_min_msecs = 0;
	bp->fw_reset_max_msecs = BNXT_MAX_FW_RESET_TIMEOUT;

	bnxt_stop_rxtx(bp->eth_dev);

	rc = bnxt_hwrm_func_error_query(bp);
	if (rc)
		PMD_DRV_LOG_LINE(INFO,
			    "Port %u: FUNC_ERROR diagnostic query failed rc:%d "
			    "(no diagnostic data, proceeding with reset anyway)",
			    bp->eth_dev->data->port_id, rc);

	rc = bnxt_hwrm_func_reset(bp);
	if (rc) {
		PMD_DRV_LOG_LINE(ERR,
			    "FUNC_ERROR recovery: HWRM_FUNC_RESET failed");
		bp->flags &= ~BNXT_FLAG_FW_RESET;
		pthread_mutex_unlock(&bp->err_recovery_lock);
		bnxt_func_error_clear_pending(bp);
		return;
	}

	/* HWRM_FUNC_RESET just reclaimed every ring/VNIC/stat-ctx for this
	 * function; bnxt_dev_cleanup()'s own per-resource frees would only
	 * be rejected as "not allocated". Gate them out until cleanup runs.
	 */
	bp->flags |= BNXT_FLAG_PXP_ERROR;
	pthread_mutex_unlock(&bp->err_recovery_lock);

	if (rte_eal_alarm_set(US_PER_MS, bnxt_dev_reset_and_resume,
			      (void *)bp)) {
		PMD_DRV_LOG_LINE(ERR,
			    "Port %u: Failed to schedule FUNC_ERROR reinit -- "
			    "HWRM_FUNC_RESET already reclaimed all resources, "
			    "marking port fatal",
			    bp->eth_dev->data->port_id);
		/* Nothing will ever call bnxt_dev_reset_and_resume() to
		 * rebuild the rings/VNICs HWRM_FUNC_RESET just reclaimed --
		 * clearing PXP_ERROR/FW_RESET here would leave the driver
		 * thinking the port is healthy when it has no resources left.
		 */
		pthread_mutex_lock(&bp->err_recovery_lock);
		bp->flags |= BNXT_FLAG_FATAL_ERROR;
		bp->flags &= ~(BNXT_FLAG_PXP_ERROR | BNXT_FLAG_FW_RESET);
		pthread_mutex_unlock(&bp->err_recovery_lock);
		bnxt_func_error_clear_pending(bp);
	}
}

static void bnxt_func_error_poll_cb(void *arg)
{
	struct bnxt *bp = arg;

	if (!rte_atomic_load_explicit(&bp->func_error_info->support,
				      rte_memory_order_relaxed))
		return;

	if (!rte_atomic_load_explicit(&bp->func_error_info->recovery_pending,
				      rte_memory_order_acquire))
		bnxt_hwrm_func_error_poll(bp);

	/* bnxt_cancel_func_error_poll() clears poll_scheduled before calling
	 * rte_eal_alarm_cancel(), which blocks until this callback (if
	 * already running) returns -- re-check here so a cancel that landed
	 * while this invocation was in flight isn't outrun by this rearm.
	 */
	if (rte_atomic_load_explicit(&bp->func_error_info->poll_scheduled,
				     rte_memory_order_acquire) &&
	    rte_eal_alarm_set(US_PER_S, bnxt_func_error_poll_cb, (void *)bp) != 0)
		rte_atomic_store_explicit(&bp->func_error_info->poll_scheduled, 0,
					  rte_memory_order_release);
}

void bnxt_schedule_func_error_poll(struct bnxt *bp)
{
	if (!rte_atomic_load_explicit(&bp->func_error_info->support,
				      rte_memory_order_relaxed) ||
	    rte_atomic_load_explicit(&bp->func_error_info->poll_scheduled,
				     rte_memory_order_acquire))
		return;

	if (rte_eal_alarm_set(US_PER_S, bnxt_func_error_poll_cb, (void *)bp) != 0)
		return;

	rte_atomic_store_explicit(&bp->func_error_info->poll_scheduled, 1,
				  rte_memory_order_release);
}

void bnxt_cancel_func_error_poll(struct bnxt *bp)
{
	if (!rte_atomic_load_explicit(&bp->func_error_info->poll_scheduled,
				      rte_memory_order_acquire))
		return;

	/* Clear before cancelling: rte_eal_alarm_cancel() blocks until an
	 * in-flight invocation of the callback returns, and that invocation
	 * re-checks this flag right before its own rearm -- clearing first
	 * closes the window where it could rearm after cancel returns.
	 */
	rte_atomic_store_explicit(&bp->func_error_info->poll_scheduled, 0,
				  rte_memory_order_release);
	rte_eal_alarm_cancel(bnxt_func_error_poll_cb, (void *)bp);
}

int bnxt_parse_devarg_func_error_support(const char *key __rte_unused,
					  const char *value, void *opaque_arg)
{
	struct bnxt *bp = opaque_arg;
	unsigned long func_error_support;
	char *end = NULL;

	if (!value || !opaque_arg) {
		PMD_DRV_LOG_LINE(ERR,
			    "Invalid parameter passed to func-error-support "
			    "devargs.");
		return -EINVAL;
	}

	func_error_support = strtoul(value, &end, 10);
	if (end == NULL || *end != '\0' ||
	    (func_error_support == ULONG_MAX && errno == ERANGE)) {
		PMD_DRV_LOG_LINE(ERR,
			    "Invalid parameter passed to func-error-support "
			    "devargs.");
		return -EINVAL;
	}

	if (BNXT_DEVARG_FUNC_ERROR_SUPPORT_INVALID(func_error_support)) {
		PMD_DRV_LOG_LINE(ERR, "Invalid func-error-support(%d) devargs.",
			    (uint8_t)func_error_support);
		return -EINVAL;
	}

	rte_atomic_store_explicit(&bp->func_error_info->support,
				  func_error_support, rte_memory_order_relaxed);
	if (func_error_support)
		PMD_DRV_LOG_LINE(INFO,
			    "func-error-support=1: FUNC_ERROR/PXP_ERROR "
			    "recovery opted in.");

	return 0;
}
