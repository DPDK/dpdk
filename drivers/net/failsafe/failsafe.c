/*-
 *   BSD LICENSE
 *
 *   Copyright 2017 6WIND S.A.
 *   Copyright 2017 Mellanox.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of 6WIND S.A. nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <rte_alarm.h>
#include <rte_malloc.h>
#include <rte_ethdev.h>
#include <rte_ethdev_vdev.h>
#include <rte_devargs.h>
#include <rte_kvargs.h>
#include <rte_vdev.h>

#include "failsafe_private.h"

const char pmd_failsafe_driver_name[] = FAILSAFE_DRIVER_NAME;
static const struct rte_eth_link eth_link = {
	.link_speed = ETH_SPEED_NUM_10G,
	.link_duplex = ETH_LINK_FULL_DUPLEX,
	.link_status = ETH_LINK_UP,
	.link_autoneg = ETH_LINK_SPEED_AUTONEG,
};

static int
fs_sub_device_alloc(struct rte_eth_dev *dev,
		const char *params)
{
	uint8_t nb_subs;
	int ret;

	ret = failsafe_args_count_subdevice(dev, params);
	if (ret)
		return ret;
	if (PRIV(dev)->subs_tail > FAILSAFE_MAX_ETHPORTS) {
		ERROR("Cannot allocate more than %d ports",
			FAILSAFE_MAX_ETHPORTS);
		return -ENOSPC;
	}
	nb_subs = PRIV(dev)->subs_tail;
	PRIV(dev)->subs = rte_zmalloc(NULL,
			sizeof(struct sub_device) * nb_subs,
			RTE_CACHE_LINE_SIZE);
	if (PRIV(dev)->subs == NULL) {
		ERROR("Could not allocate sub_devices");
		return -ENOMEM;
	}
	return 0;
}

static void
fs_sub_device_free(struct rte_eth_dev *dev)
{
	rte_free(PRIV(dev)->subs);
}

static void fs_hotplug_alarm(void *arg);

int
failsafe_hotplug_alarm_install(struct rte_eth_dev *dev)
{
	int ret;

	if (dev == NULL)
		return -EINVAL;
	if (PRIV(dev)->pending_alarm)
		return 0;
	ret = rte_eal_alarm_set(hotplug_poll * 1000,
				fs_hotplug_alarm,
				dev);
	if (ret) {
		ERROR("Could not set up plug-in event detection");
		return ret;
	}
	PRIV(dev)->pending_alarm = 1;
	return 0;
}

int
failsafe_hotplug_alarm_cancel(struct rte_eth_dev *dev)
{
	int ret = 0;

	if (PRIV(dev)->pending_alarm) {
		rte_errno = 0;
		rte_eal_alarm_cancel(fs_hotplug_alarm, dev);
		if (rte_errno) {
			ERROR("rte_eal_alarm_cancel failed (errno: %s)",
			      strerror(rte_errno));
			ret = -rte_errno;
		} else {
			PRIV(dev)->pending_alarm = 0;
		}
	}
	return ret;
}

static void
fs_hotplug_alarm(void *arg)
{
	struct rte_eth_dev *dev = arg;
	struct sub_device *sdev;
	int ret;
	uint8_t i;

	if (!PRIV(dev)->pending_alarm)
		return;
	PRIV(dev)->pending_alarm = 0;
	FOREACH_SUBDEV(sdev, i, dev)
		if (sdev->state != PRIV(dev)->state)
			break;
	/* if we have non-probed device */
	if (i != PRIV(dev)->subs_tail) {
		ret = failsafe_eth_dev_state_sync(dev);
		if (ret)
			ERROR("Unable to synchronize sub_device state");
	}
	failsafe_dev_remove(dev);
	ret = failsafe_hotplug_alarm_install(dev);
	if (ret)
		ERROR("Unable to set up next alarm");
}

static int
fs_eth_dev_create(struct rte_vdev_device *vdev)
{
	struct rte_eth_dev *dev;
	struct ether_addr *mac;
	struct fs_priv *priv;
	struct sub_device *sdev;
	const char *params;
	unsigned int socket_id;
	uint8_t i;
	int ret;

	dev = NULL;
	priv = NULL;
	socket_id = rte_socket_id();
	INFO("Creating fail-safe device on NUMA socket %u", socket_id);
	params = rte_vdev_device_args(vdev);
	if (params == NULL) {
		ERROR("This PMD requires sub-devices, none provided");
		return -1;
	}
	dev = rte_eth_vdev_allocate(vdev, sizeof(*priv));
	if (dev == NULL) {
		ERROR("Unable to allocate rte_eth_dev");
		return -1;
	}
	priv = PRIV(dev);
	priv->dev = dev;
	dev->dev_ops = &failsafe_ops;
	dev->data->mac_addrs = &PRIV(dev)->mac_addrs[0];
	dev->data->dev_link = eth_link;
	PRIV(dev)->nb_mac_addr = 1;
	TAILQ_INIT(&PRIV(dev)->flow_list);
	dev->rx_pkt_burst = (eth_rx_burst_t)&failsafe_rx_burst;
	dev->tx_pkt_burst = (eth_tx_burst_t)&failsafe_tx_burst;
	ret = fs_sub_device_alloc(dev, params);
	if (ret) {
		ERROR("Could not allocate sub_devices");
		goto free_dev;
	}
	ret = failsafe_args_parse(dev, params);
	if (ret)
		goto free_subs;
	ret = failsafe_eal_init(dev);
	if (ret)
		goto free_args;
	ret = failsafe_hotplug_alarm_install(dev);
	if (ret) {
		ERROR("Could not set up plug-in event detection");
		goto free_args;
	}
	mac = &dev->data->mac_addrs[0];
	if (mac_from_arg) {
		/*
		 * If MAC address was provided as a parameter,
		 * apply to all probed slaves.
		 */
		FOREACH_SUBDEV_STATE(sdev, i, dev, DEV_PROBED) {
			ret = rte_eth_dev_default_mac_addr_set(PORT_ID(sdev),
							       mac);
			if (ret) {
				ERROR("Failed to set default MAC address");
				goto free_args;
			}
		}
	} else {
		/*
		 * Use the ether_addr from first probed
		 * device, either preferred or fallback.
		 */
		FOREACH_SUBDEV(sdev, i, dev)
			if (sdev->state >= DEV_PROBED) {
				ether_addr_copy(&ETH(sdev)->data->mac_addrs[0],
						mac);
				break;
			}
		/*
		 * If no device has been probed and no ether_addr
		 * has been provided on the command line, use a random
		 * valid one.
		 * It will be applied during future slave state syncs to
		 * probed slaves.
		 */
		if (i == priv->subs_tail)
			eth_random_addr(&mac->addr_bytes[0]);
	}
	INFO("MAC address is %02x:%02x:%02x:%02x:%02x:%02x",
		mac->addr_bytes[0], mac->addr_bytes[1],
		mac->addr_bytes[2], mac->addr_bytes[3],
		mac->addr_bytes[4], mac->addr_bytes[5]);
	dev->data->dev_flags |= RTE_ETH_DEV_INTR_LSC;
	return 0;
free_args:
	failsafe_args_free(dev);
free_subs:
	fs_sub_device_free(dev);
free_dev:
	rte_free(PRIV(dev));
	rte_eth_dev_release_port(dev);
	return -1;
}

static int
fs_rte_eth_free(const char *name)
{
	struct rte_eth_dev *dev;
	int ret;

	dev = rte_eth_dev_allocated(name);
	if (dev == NULL)
		return -ENODEV;
	ret = failsafe_eal_uninit(dev);
	if (ret)
		ERROR("Error while uninitializing sub-EAL");
	failsafe_args_free(dev);
	fs_sub_device_free(dev);
	rte_free(PRIV(dev));
	rte_eth_dev_release_port(dev);
	return ret;
}

static int
rte_pmd_failsafe_probe(struct rte_vdev_device *vdev)
{
	const char *name;

	name = rte_vdev_device_name(vdev);
	INFO("Initializing " FAILSAFE_DRIVER_NAME " for %s",
			name);
	return fs_eth_dev_create(vdev);
}

static int
rte_pmd_failsafe_remove(struct rte_vdev_device *vdev)
{
	const char *name;

	name = rte_vdev_device_name(vdev);
	INFO("Uninitializing " FAILSAFE_DRIVER_NAME " for %s", name);
	return fs_rte_eth_free(name);
}

static struct rte_vdev_driver failsafe_drv = {
	.probe = rte_pmd_failsafe_probe,
	.remove = rte_pmd_failsafe_remove,
};

RTE_PMD_REGISTER_VDEV(net_failsafe, failsafe_drv);
RTE_PMD_REGISTER_PARAM_STRING(net_failsafe, PMD_FAILSAFE_PARAM_STRING);
