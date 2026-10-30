/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2026 NVIDIA Corporation & Affiliates
 */

#include <stdint.h>
#include <time.h>

#include <ethdev_driver.h>
#include <rte_alarm.h>
#include <rte_bitset.h>
#include <rte_interrupts.h>
#include <rte_pause.h>
#include <rte_spinlock.h>
#include <rte_stdatomic.h>

#include "mlx5.h"
#include "mlx5_lsc_intr.h"
#include "mlx5_utils.h"

#define LINK_POLL_DEFAULT_PERIOD_US (UINT64_C(100) * 1000)
#define LINK_POLL_MINIMUM_PERIOD_US LINK_POLL_DEFAULT_PERIOD_US
#define LINK_POLL_MAXIMUM_PERIOD_US (UINT64_C(800) * 1000)

#define POLLED_PORTS_MAX (RTE_MAX_ETHPORTS)

struct mlx5_lsc_intr_ctx {
	RTE_BITSET_DECLARE(polled_ports, POLLED_PORTS_MAX);
	uint64_t period_us;
	bool armed;
	rte_spinlock_t lock;
	RTE_ATOMIC(bool) cb_in_progress;
};

static struct mlx5_lsc_intr_ctx lsc_intr_ctx = {
	.period_us = LINK_POLL_DEFAULT_PERIOD_US,
	.lock = RTE_SPINLOCK_INITIALIZER,
};

static inline bool
lsc_intr_cb_in_progress(struct mlx5_lsc_intr_ctx *ctx)
{
	return rte_atomic_load_explicit(&ctx->cb_in_progress, rte_memory_order_acquire);
}

static inline void
lsc_intr_cb_set_in_progress(struct mlx5_lsc_intr_ctx *ctx, bool val)
{
	rte_atomic_store_explicit(&ctx->cb_in_progress, val, rte_memory_order_release);
}

static struct timespec
timespec_sub(const struct timespec *t0, const struct timespec *t1)
{
	struct timespec ts;

	ts.tv_sec = t0->tv_sec - t1->tv_sec;
	ts.tv_nsec = t0->tv_nsec - t1->tv_nsec;
	if (ts.tv_nsec < 0) {
		ts.tv_sec--;
		ts.tv_nsec += 1000000000L;
	}
	return ts;
}

static uint64_t
adjust_period_and_calc_delay(struct mlx5_lsc_intr_ctx *ctx,
			     struct timespec *after,
			     struct timespec *before)
{
	struct timespec delta = timespec_sub(after, before);
	uint64_t new_period;
	uint64_t delta_us;

	delta_us = delta.tv_sec * 1000000;
	delta_us += delta.tv_nsec / 1000;

	new_period = ctx->period_us;

	if (delta_us > new_period / 2) {
		/* If takes too long, adjust period up. */
		while (new_period < LINK_POLL_MAXIMUM_PERIOD_US && delta_us > new_period / 2)
			new_period = RTE_MIN(new_period * 2, LINK_POLL_MAXIMUM_PERIOD_US);
	} else if (delta_us < new_period / 4) {
		/* If polling is fast, adjust period down. */
		while (new_period > LINK_POLL_MINIMUM_PERIOD_US && delta_us < new_period / 4)
			new_period = RTE_MAX(new_period / 2, LINK_POLL_MINIMUM_PERIOD_US);
	}

	if (new_period != ctx->period_us) {
		DRV_LOG(DEBUG, "adjusting link polling period: %" PRIu64 " us to %" PRIu64 " us",
			ctx->period_us, new_period);
		ctx->period_us = new_period;
	}

	if (ctx->period_us == LINK_POLL_MAXIMUM_PERIOD_US) {
		/*
		 * If at maximum period and querying link still takes too long, then default to
		 * half of the maximum period so other interrupt thread users are not starved.
		 */
		if (delta_us > ctx->period_us / 2)
			return LINK_POLL_MAXIMUM_PERIOD_US / 2;
	}

	MLX5_ASSERT(delta_us <= ctx->period_us / 2);
	return ctx->period_us - delta_us;
}

static void
lsc_intr_disarm(struct mlx5_lsc_intr_ctx *ctx)
{
	rte_spinlock_lock(&ctx->lock);
	ctx->armed = false;
	rte_spinlock_unlock(&ctx->lock);
}

static bool
lsc_intr_dev_add(struct mlx5_lsc_intr_ctx *ctx, struct rte_eth_dev *dev)
{
	uint16_t port_id = dev->data->port_id;
	bool schedule = false;

	rte_spinlock_lock(&ctx->lock);
	rte_bitset_set(ctx->polled_ports, port_id);
	if (!ctx->armed) {
		ctx->armed = true;
		schedule = true;
	}
	rte_spinlock_unlock(&ctx->lock);

	return schedule;
}

static void
lsc_intr_dev_remove(struct mlx5_lsc_intr_ctx *ctx, struct rte_eth_dev *dev, bool disarm)
{
	rte_spinlock_lock(&ctx->lock);
	rte_bitset_clear(ctx->polled_ports, dev->data->port_id);
	if (disarm)
		ctx->armed = false;
	rte_spinlock_unlock(&ctx->lock);
}

static int
lsc_intr_gather_devs(struct mlx5_lsc_intr_ctx *ctx,
		     struct rte_eth_dev *out[POLLED_PORTS_MAX])
{
	ssize_t port_id;
	int ret = 0;

	rte_spinlock_lock(&ctx->lock);
	RTE_BITSET_FOREACH_SET(port_id, ctx->polled_ports, POLLED_PORTS_MAX) {
		out[ret++] = &rte_eth_devices[port_id];
	}
	rte_spinlock_unlock(&ctx->lock);

	return ret;
}

static void
lsc_intr_check_links(struct rte_eth_dev **devs, int devs_nb)
{
	for (int i = 0; i < devs_nb; ++i) {
		struct rte_eth_dev *dev = devs[i];
		int ret;

		/* Check carrier link. If changed, update link parameters. */
		ret = mlx5_link_carrier_get(dev);
		if (ret < 0 ||
		    ret == dev->data->dev_link.link_status ||
		    mlx5_link_update(dev, 0) <= 0)
			devs[i] = NULL;
	}
}

static void
lsc_intr_notify(struct rte_eth_dev **devs, int devs_nb)
{
	for (int i = 0; i < devs_nb; ++i) {
		if (devs[i] == NULL)
			continue;

		rte_eth_dev_callback_process(devs[i], RTE_ETH_EVENT_INTR_LSC, NULL);
	}
}

static bool
lsc_intr_should_reschedule(struct mlx5_lsc_intr_ctx *ctx)
{
	bool should;

	rte_spinlock_lock(&ctx->lock);
	should = rte_bitset_count_set(ctx->polled_ports, POLLED_PORTS_MAX) > 0;
	if (!should)
		ctx->armed = false;
	rte_spinlock_unlock(&ctx->lock);

	return should;
}

static bool
lsc_intr_cb(struct mlx5_lsc_intr_ctx *ctx, uint64_t *delay_us)
{
	static struct rte_eth_dev *to_process[POLLED_PORTS_MAX];
	struct timespec before;
	struct timespec after;
	int to_process_nb = 0;
	bool have_time = true;

	if (clock_gettime(CLOCK_MONOTONIC, &before) < 0)
		have_time = false;

	to_process_nb = lsc_intr_gather_devs(ctx, to_process);
	if (to_process_nb == 0)
		return lsc_intr_should_reschedule(ctx);

	lsc_intr_check_links(to_process, to_process_nb);
	lsc_intr_notify(to_process, to_process_nb);

	if (clock_gettime(CLOCK_MONOTONIC, &after) < 0)
		have_time = false;

	if (have_time) {
		*delay_us = adjust_period_and_calc_delay(ctx, &after, &before);
	} else {
		DRV_LOG(WARNING, "link polling callback failed to calculate polling time");
		*delay_us = ctx->period_us;
	}

	return lsc_intr_should_reschedule(ctx);
}

static void
mlx5_lsc_intr_cb(void *arg)
{
	struct mlx5_lsc_intr_ctx *ctx = arg;
	uint64_t delay_us = ctx->period_us;
	bool reschedule;
	int ret;

	lsc_intr_cb_set_in_progress(ctx, true);
	reschedule = lsc_intr_cb(ctx, &delay_us);
	lsc_intr_cb_set_in_progress(ctx, false);

	/* Reschedule the polling */
	if (reschedule) {
		ret = rte_eal_alarm_set(delay_us, mlx5_lsc_intr_cb, ctx);
		if (ret < 0) {
			DRV_LOG(ERR, "rescheduling link polling callback failed (%s)",
				rte_strerror(-ret));
			lsc_intr_disarm(ctx);
		}
	}
}

int
mlx5_lsc_intr_setup(struct rte_eth_dev *dev)
{
	struct mlx5_lsc_intr_ctx *ctx = &lsc_intr_ctx;
	bool schedule;
	int ret;

	schedule = lsc_intr_dev_add(ctx, dev);
	if (!schedule)
		return 0;

	ret = rte_eal_alarm_set(ctx->period_us, mlx5_lsc_intr_cb, ctx);
	if (ret < 0) {
		DRV_LOG(ERR, "port %u registering link polling callback failed (%s)",
			dev->data->port_id, rte_strerror(-ret));
		lsc_intr_dev_remove(ctx, dev, true);
		return ret;
	}
	DRV_LOG(DEBUG, "registered link polling callback");

	return 0;
}

void
mlx5_lsc_intr_cleanup(struct rte_eth_dev *dev)
{
	struct mlx5_lsc_intr_ctx *ctx = &lsc_intr_ctx;

	lsc_intr_dev_remove(ctx, dev, false);

	/*
	 * If callback is running, then wait until it finishes to prevent
	 * freeing dev while it is running.
	 *
	 * mlx5_lsc_intr_cb will be called only from interrupt thread.
	 * If cleanup was called from the callback, then no need to wait.
	 */
	while (lsc_intr_cb_in_progress(ctx) && !rte_thread_is_intr())
		rte_pause();
}
