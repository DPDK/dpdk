/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2019 NXP
 */

#include <sys/queue.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>

#include <ethdev_driver.h>
#include <rte_log.h>
#include <rte_eth_ctrl.h>
#include <rte_malloc.h>
#include <rte_time.h>

#include <rte_fslmc.h>
#include <fsl_dprtc.h>
#include <fsl_dpkg.h>

#include <dpaa2_ethdev.h>
#include <dpaa2_pmd_logs.h>

struct dpaa2_dprtc_dev {
	struct fsl_mc_io dprtc;  /** handle to DPRTC portal object */
	uint16_t token;
	uint32_t dprtc_id; /*HW ID for DPRTC object */
};
static struct dpaa2_dprtc_dev *dprtc_dev;

int dpaa2_timesync_enable(struct rte_eth_dev *dev __rte_unused)
{
	return 0;
}

int dpaa2_timesync_disable(struct rte_eth_dev *dev __rte_unused)
{
	return 0;
}

int dpaa2_timesync_read_time(struct rte_eth_dev *dev,
					struct timespec *timestamp)
{
	uint64_t ns;
	int ret = 0;

	RTE_SET_USED(dev);

	ret = dprtc_get_time(&dprtc_dev->dprtc, CMD_PRI_LOW,
			     dprtc_dev->token, &ns);
	if (ret) {
		DPAA2_PMD_ERR("dprtc_get_time failed ret: %d", ret);
		return ret;
	}

	*timestamp = rte_ns_to_timespec(ns);

	return 0;
}

int dpaa2_timesync_write_time(struct rte_eth_dev *dev,
					const struct timespec *ts)
{
	uint64_t ns;
	int ret = 0;

	RTE_SET_USED(dev);

	ns = rte_timespec_to_ns(ts);

	ret = dprtc_set_time(&dprtc_dev->dprtc, CMD_PRI_LOW,
			     dprtc_dev->token, ns);
	if (ret) {
		DPAA2_PMD_ERR("dprtc_set_time failed ret: %d", ret);
		return ret;
	}

	return 0;
}

int dpaa2_timesync_adjust_time(struct rte_eth_dev *dev, int64_t delta)
{
	uint64_t ns;
	int ret = 0;

	RTE_SET_USED(dev);

	ret = dprtc_get_time(&dprtc_dev->dprtc, CMD_PRI_LOW,
			     dprtc_dev->token, &ns);
	if (ret) {
		DPAA2_PMD_ERR("dprtc_get_time failed ret: %d", ret);
		return ret;
	}

	ns += delta;

	ret = dprtc_set_time(&dprtc_dev->dprtc, CMD_PRI_LOW,
			     dprtc_dev->token, ns);
	if (ret) {
		DPAA2_PMD_ERR("dprtc_set_time failed ret: %d", ret);
		return ret;
	}

	return 0;
}

int dpaa2_timesync_read_tx_timestamp(struct rte_eth_dev *dev,
						struct timespec *timestamp)
{
	struct dpaa2_dev_priv *priv = dev->data->dev_private;

	if (priv->next_tx_conf_queue) {
		while (!priv->tx_timestamp)
			dpaa2_dev_tx_conf(priv->next_tx_conf_queue);
	} else {
		return -1;
	}
	*timestamp = rte_ns_to_timespec(priv->tx_timestamp);

	return 0;
}

int dpaa2_timesync_read_rx_timestamp(struct rte_eth_dev *dev,
						struct timespec *timestamp,
						uint32_t flags __rte_unused)
{
	struct dpaa2_dev_priv *priv = dev->data->dev_private;
	*timestamp = rte_ns_to_timespec(priv->rx_timestamp);
	return 0;
}

#if defined(RTE_LIBRTE_IEEE1588)
static int
dpaa2_create_dprtc_device(int vdev_fd __rte_unused,
			   struct vfio_device_info *obj_info __rte_unused,
			   int dprtc_id)
{
	struct dprtc_attr attr;
	int ret;

	PMD_INIT_FUNC_TRACE();

	/* Allocate DPAA2 dprtc handle */
	dprtc_dev = rte_malloc(NULL, sizeof(struct dpaa2_dprtc_dev), 0);
	if (!dprtc_dev) {
		DPAA2_PMD_ERR("Memory allocation failed for DPRTC Device");
		return -1;
	}

	/* Open the dprtc object */
	dprtc_dev->dprtc.regs = dpaa2_get_mcp_ptr(MC_PORTAL_INDEX);
	ret = dprtc_open(&dprtc_dev->dprtc, CMD_PRI_LOW, dprtc_id,
			  &dprtc_dev->token);
	if (ret) {
		DPAA2_PMD_ERR("Unable to open dprtc object: err(%d)", ret);
		goto init_err;
	}

	ret = dprtc_get_attributes(&dprtc_dev->dprtc, CMD_PRI_LOW,
				    dprtc_dev->token, &attr);
	if (ret) {
		DPAA2_PMD_ERR("Unable to get dprtc attr: err(%d)", ret);
		goto init_err;
	}

	dprtc_dev->dprtc_id = dprtc_id;

	return 0;

init_err:
	rte_free(dprtc_dev);

	return -1;
}

static struct rte_dpaa2_object rte_dpaa2_dprtc_obj = {
	.dev_type = DPAA2_DPRTC,
	.create = dpaa2_create_dprtc_device,
};

RTE_PMD_REGISTER_DPAA2_OBJECT(dprtc, rte_dpaa2_dprtc_obj);
#endif
