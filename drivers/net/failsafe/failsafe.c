/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2017 6WIND S.A.
 * Copyright 2017 Mellanox Technologies, Ltd
 */

#include <stdbool.h>

#include <rte_alarm.h>
#include <rte_malloc.h>
#include <rte_ethdev_driver.h>
#include <rte_ethdev_vdev.h>
#include <rte_devargs.h>
#include <rte_kvargs.h>
#include <rte_bus_vdev.h>

#include "failsafe_private.h"

int failsafe_logtype;

const char pmd_failsafe_driver_name[] = FAILSAFE_DRIVER_NAME;
static const struct rte_eth_link eth_link = {
	.link_speed = ETH_SPEED_NUM_10G,
	.link_duplex = ETH_LINK_FULL_DUPLEX,
	.link_status = ETH_LINK_UP,
	.link_autoneg = ETH_LINK_AUTONEG,
};

static int
fs_sub_device_alloc(struct rte_eth_dev *dev,
		const char *params)
{
	uint8_t nb_subs;
	int ret;
	int i;
	struct sub_device *sdev;
	uint8_t sdev_iterator;

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
	/* Initiate static sub devices linked list. */
	for (i = 1; i < nb_subs; i++)
		PRIV(dev)->subs[i - 1].next = PRIV(dev)->subs + i;
	PRIV(dev)->subs[i - 1].next = PRIV(dev)->subs;

	FOREACH_SUBDEV(sdev, sdev_iterator, dev) {
		sdev->sdev_port_id = RTE_MAX_ETHPORTS;
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
	ret = rte_eal_alarm_set(failsafe_hotplug_poll * 1000,
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

	rte_errno = 0;
	rte_eal_alarm_cancel(fs_hotplug_alarm, dev);
	if (rte_errno) {
		ERROR("rte_eal_alarm_cancel failed (errno: %s)",
		      strerror(rte_errno));
		ret = -rte_errno;
	} else {
		PRIV(dev)->pending_alarm = 0;
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
		if (fs_lock(dev, 1) != 0)
			goto reinstall;
		ret = failsafe_eth_dev_state_sync(dev);
		fs_unlock(dev, 1);
		if (ret)
			ERROR("Unable to synchronize sub_device state");
	}
	failsafe_dev_remove(dev);
reinstall:
	ret = failsafe_hotplug_alarm_install(dev);
	if (ret)
		ERROR("Unable to set up next alarm");
}

static int
fs_mutex_init(struct fs_priv *priv)
{
	int ret;
	pthread_mutexattr_t attr;

	ret = pthread_mutexattr_init(&attr);
	if (ret) {
		ERROR("Cannot initiate mutex attributes - %s", strerror(ret));
		return ret;
	}
	/* Allow mutex relocks for the thread holding the mutex. */
	ret = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	if (ret) {
		ERROR("Cannot set mutex type - %s", strerror(ret));
		return ret;
	}
	ret = pthread_mutex_init(&priv->hotplug_mutex, &attr);
	if (ret) {
		ERROR("Cannot initiate mutex - %s", strerror(ret));
		return ret;
	}
	return 0;
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
	priv->data = dev->data;
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
	ret = rte_eth_dev_owner_new(&priv->my_owner.id);
	if (ret) {
		ERROR("Failed to get unique owner identifier");
		goto free_args;
	}
	snprintf(priv->my_owner.name, sizeof(priv->my_owner.name),
		 FAILSAFE_OWNER_NAME);
	DEBUG("Failsafe port %u owner info: %s_%016"PRIX64, dev->data->port_id,
	      priv->my_owner.name, priv->my_owner.id);
	ret = rte_eth_dev_callback_register(RTE_ETH_ALL, RTE_ETH_EVENT_NEW,
					    failsafe_eth_new_event_callback,
					    dev);
	if (ret) {
		ERROR("Failed to register NEW callback");
		goto free_args;
	}
	ret = failsafe_eal_init(dev);
	if (ret)
		goto unregister_new_callback;
	ret = fs_mutex_init(priv);
	if (ret)
		goto unregister_new_callback;
	ret = failsafe_hotplug_alarm_install(dev);
	if (ret) {
		ERROR("Could not set up plug-in event detection");
		goto unregister_new_callback;
	}
	mac = &dev->data->mac_addrs[0];
	if (failsafe_mac_from_arg) {
		/*
		 * If MAC address was provided as a parameter,
		 * apply to all probed slaves.
		 */
		FOREACH_SUBDEV_STATE(sdev, i, dev, DEV_PROBED) {
			ret = rte_eth_dev_default_mac_addr_set(PORT_ID(sdev),
							       mac);
			if (ret) {
				ERROR("Failed to set default MAC address");
				goto cancel_alarm;
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
	PRIV(dev)->intr_handle = (struct rte_intr_handle){
		.fd = -1,
		.type = RTE_INTR_HANDLE_EXT,
	};
	rte_eth_dev_probing_finish(dev);
	return 0;
cancel_alarm:
	failsafe_hotplug_alarm_cancel(dev);
unregister_new_callback:
	rte_eth_dev_callback_unregister(RTE_ETH_ALL, RTE_ETH_EVENT_NEW,
					failsafe_eth_new_event_callback, dev);
free_args:
	failsafe_args_free(dev);
free_subs:
	fs_sub_device_free(dev);
free_dev:
	/* mac_addrs must not be freed alone because part of dev_private */
	dev->data->mac_addrs = NULL;
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
	rte_eth_dev_callback_unregister(RTE_ETH_ALL, RTE_ETH_EVENT_NEW,
					failsafe_eth_new_event_callback, dev);
	ret = failsafe_eal_uninit(dev);
	if (ret)
		ERROR("Error while uninitializing sub-EAL");
	failsafe_args_free(dev);
	fs_sub_device_free(dev);
	ret = pthread_mutex_destroy(&PRIV(dev)->hotplug_mutex);
	if (ret)
		ERROR("Error while destroying hotplug mutex");
	rte_free(PRIV(dev)->mcast_addrs);
	/* mac_addrs must not be freed alone because part of dev_private */
	dev->data->mac_addrs = NULL;
	rte_eth_dev_release_port(dev);
	return ret;
}

static bool
devargs_already_listed(struct rte_devargs *devargs)
{
	struct rte_devargs *list_da;

	RTE_EAL_DEVARGS_FOREACH(devargs->bus->name, list_da) {
		if (strcmp(list_da->name, devargs->name) == 0)
			/* devargs already in the list */
			return true;
	}
	return false;
}

static int
rte_pmd_failsafe_probe(struct rte_vdev_device *vdev)
{
	const char *name;
	struct rte_eth_dev *eth_dev;
	struct sub_device  *sdev;
	struct rte_devargs devargs;
	uint8_t i;
	int ret;

	name = rte_vdev_device_name(vdev);
	INFO("Initializing " FAILSAFE_DRIVER_NAME " for %s",
			name);

	if (rte_eal_process_type() == RTE_PROC_SECONDARY &&
	    strlen(rte_vdev_device_args(vdev)) == 0) {
		eth_dev = rte_eth_dev_attach_secondary(name);
		if (!eth_dev) {
			ERROR("Failed to probe %s", name);
			return -1;
		}
		eth_dev->dev_ops = &failsafe_ops;
		eth_dev->device = &vdev->device;
		eth_dev->rx_pkt_burst = (eth_rx_burst_t)&failsafe_rx_burst;
		eth_dev->tx_pkt_burst = (eth_tx_burst_t)&failsafe_tx_burst;
		/*
		 * Failsafe will attempt to probe all of its sub-devices.
		 * Any failure in sub-devices is not a fatal error.
		 * A sub-device can be plugged later.
		 */
		FOREACH_SUBDEV(sdev, i, eth_dev) {
			/* rebuild devargs to be able to get the bus name. */
			ret = rte_devargs_parse(&devargs,
						sdev->devargs.name);
			if (ret != 0) {
				ERROR("Failed to parse devargs %s",
					devargs.name);
				continue;
			}
			if (!devargs_already_listed(&devargs)) {
				ret = rte_dev_probe(devargs.name);
				if (ret != 0) {
					ERROR("Failed to probe devargs %s",
					      devargs.name);
					continue;
				}
			}
		}
		rte_eth_dev_probing_finish(eth_dev);
		return 0;
	}

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

RTE_INIT(failsafe_init_log)
{
	failsafe_logtype = rte_log_register("pmd.net.failsafe");
	if (failsafe_logtype >= 0)
		rte_log_set_level(failsafe_logtype, RTE_LOG_NOTICE);
}
