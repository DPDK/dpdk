/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2022-2024 NXP
 */

/* System headers */
#include <stdio.h>
#include <inttypes.h>
#include <unistd.h>

#include <rte_ethdev.h>
#include <rte_log.h>
#include <rte_eth_ctrl.h>
#include <rte_malloc.h>
#include <rte_time.h>

#include <dpaa_ethdev.h>
#include <dpaa_rxtx.h>

int
dpaa_timesync_enable(struct rte_eth_dev *dev __rte_unused)
{
	return 0;
}

int
dpaa_timesync_disable(struct rte_eth_dev *dev __rte_unused)
{
	return 0;
}

int
dpaa_timesync_read_time(struct rte_eth_dev *dev,
					struct timespec *timestamp)
{
	uint32_t *tmr_cnt_h, *tmr_cnt_l;
	struct fman_if *fif;
	uint64_t time;

	fif = dev->process_private;

	tmr_cnt_h = &((struct rtc_regs *)fif->fman->time_vir)->tmr_cnt_h;
	tmr_cnt_l = &((struct rtc_regs *)fif->fman->time_vir)->tmr_cnt_l;

	time = (uint64_t)in_be32(tmr_cnt_l);
	time |= ((uint64_t)in_be32(tmr_cnt_h) << 32);

	*timestamp = rte_ns_to_timespec(time);
	return 0;
}

int
dpaa_timesync_write_time(struct rte_eth_dev *dev,
					const struct timespec *ts)
{
	uint32_t *tmr_cnt_h, *tmr_cnt_l;
	struct fman_if *fif;
	uint64_t time;

	fif = dev->process_private;

	tmr_cnt_h = &((struct rtc_regs *)fif->fman->time_vir)->tmr_cnt_h;
	tmr_cnt_l = &((struct rtc_regs *)fif->fman->time_vir)->tmr_cnt_l;

	time = rte_timespec_to_ns(ts);

	out_be32(tmr_cnt_l, (uint32_t)time);
	out_be32(tmr_cnt_h, (uint32_t)(time >> 32));

	return 0;
}

int
dpaa_timesync_adjust_time(struct rte_eth_dev *dev, int64_t delta)
{
	struct timespec ts = {0, 0}, *timestamp = &ts;
	uint64_t ns;

	dpaa_timesync_read_time(dev, timestamp);

	ns = rte_timespec_to_ns(timestamp);
	ns += delta;
	*timestamp = rte_ns_to_timespec(ns);

	dpaa_timesync_write_time(dev, timestamp);

	return 0;
}

int
dpaa_timesync_read_tx_timestamp(struct rte_eth_dev *dev,
						struct timespec *timestamp)
{
	struct dpaa_if *dpaa_intf = dev->data->dev_private;

	if (dpaa_intf->next_tx_conf_queue) {
		while (!dpaa_intf->tx_timestamp)
			dpaa_eth_tx_conf(dpaa_intf->next_tx_conf_queue);
	} else {
		return -1;
	}
	*timestamp = rte_ns_to_timespec(dpaa_intf->tx_timestamp);

	return 0;
}

int
dpaa_timesync_read_rx_timestamp(struct rte_eth_dev *dev,
						struct timespec *timestamp,
						uint32_t flags __rte_unused)
{
	struct dpaa_if *dpaa_intf = dev->data->dev_private;
	*timestamp = rte_ns_to_timespec(dpaa_intf->rx_timestamp);
	return 0;
}
