/*-
 *   BSD LICENSE
 *
 *   Copyright 2012 6WIND S.A.
 *   Copyright 2012 Mellanox
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

/**
 * @file
 * mlx4 driver initialization.
 */

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Verbs headers do not support -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <infiniband/verbs.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-Wpedantic"
#endif

#include <rte_common.h>
#include <rte_dev.h>
#include <rte_errno.h>
#include <rte_ethdev.h>
#include <rte_ethdev_pci.h>
#include <rte_ether.h>
#include <rte_flow.h>
#include <rte_interrupts.h>
#include <rte_kvargs.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>

#include "mlx4.h"
#include "mlx4_flow.h"
#include "mlx4_rxtx.h"
#include "mlx4_utils.h"

/** Configuration structure for device arguments. */
struct mlx4_conf {
	struct {
		uint32_t present; /**< Bit-field for existing ports. */
		uint32_t enabled; /**< Bit-field for user-enabled ports. */
	} ports;
};

/* Available parameters list. */
const char *pmd_mlx4_init_params[] = {
	MLX4_PMD_PORT_KVARG,
	NULL,
};

/**
 * DPDK callback for Ethernet device configuration.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success, negative errno value otherwise and rte_errno is set.
 */
static int
mlx4_dev_configure(struct rte_eth_dev *dev)
{
	struct priv *priv = dev->data->dev_private;
	struct rte_flow_error error;
	int ret;

	/* Prepare internal flow rules. */
	ret = mlx4_flow_sync(priv, &error);
	if (ret) {
		ERROR("cannot set up internal flow rules (code %d, \"%s\"),"
		      " flow error type %d, cause %p, message: %s",
		      -ret, strerror(-ret), error.type, error.cause,
		      error.message ? error.message : "(unspecified)");
	}
	return ret;
}

/**
 * DPDK callback to start the device.
 *
 * Simulate device start by initializing common RSS resources and attaching
 * all configured flows.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success, negative errno value otherwise and rte_errno is set.
 */
static int
mlx4_dev_start(struct rte_eth_dev *dev)
{
	struct priv *priv = dev->data->dev_private;
	struct rte_flow_error error;
	int ret;

	if (priv->started)
		return 0;
	DEBUG("%p: attaching configured flows to all RX queues", (void *)dev);
	priv->started = 1;
	ret = mlx4_rss_init(priv);
	if (ret) {
		ERROR("%p: cannot initialize RSS resources: %s",
		      (void *)dev, strerror(-ret));
		goto err;
	}
	ret = mlx4_intr_install(priv);
	if (ret) {
		ERROR("%p: interrupt handler installation failed",
		     (void *)dev);
		goto err;
	}
	ret = mlx4_flow_sync(priv, &error);
	if (ret) {
		ERROR("%p: cannot attach flow rules (code %d, \"%s\"),"
		      " flow error type %d, cause %p, message: %s",
		      (void *)dev,
		      -ret, strerror(-ret), error.type, error.cause,
		      error.message ? error.message : "(unspecified)");
		goto err;
	}
	rte_wmb();
	dev->tx_pkt_burst = mlx4_tx_burst;
	dev->rx_pkt_burst = mlx4_rx_burst;
	return 0;
err:
	/* Rollback. */
	priv->started = 0;
	return ret;
}

/**
 * DPDK callback to stop the device.
 *
 * Simulate device stop by detaching all configured flows.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 */
static void
mlx4_dev_stop(struct rte_eth_dev *dev)
{
	struct priv *priv = dev->data->dev_private;

	if (!priv->started)
		return;
	DEBUG("%p: detaching flows from all RX queues", (void *)dev);
	priv->started = 0;
	dev->tx_pkt_burst = mlx4_tx_burst_removed;
	dev->rx_pkt_burst = mlx4_rx_burst_removed;
	rte_wmb();
	mlx4_flow_sync(priv, NULL);
	mlx4_intr_uninstall(priv);
	mlx4_rss_deinit(priv);
}

/**
 * DPDK callback to close the device.
 *
 * Destroy all queues and objects, free memory.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 */
static void
mlx4_dev_close(struct rte_eth_dev *dev)
{
	struct priv *priv = dev->data->dev_private;
	unsigned int i;

	DEBUG("%p: closing device \"%s\"",
	      (void *)dev,
	      ((priv->ctx != NULL) ? priv->ctx->device->name : ""));
	dev->rx_pkt_burst = mlx4_rx_burst_removed;
	dev->tx_pkt_burst = mlx4_tx_burst_removed;
	rte_wmb();
	mlx4_flow_clean(priv);
	for (i = 0; i != dev->data->nb_rx_queues; ++i)
		mlx4_rx_queue_release(dev->data->rx_queues[i]);
	for (i = 0; i != dev->data->nb_tx_queues; ++i)
		mlx4_tx_queue_release(dev->data->tx_queues[i]);
	if (priv->pd != NULL) {
		assert(priv->ctx != NULL);
		claim_zero(ibv_dealloc_pd(priv->pd));
		claim_zero(ibv_close_device(priv->ctx));
	} else
		assert(priv->ctx == NULL);
	mlx4_intr_uninstall(priv);
	memset(priv, 0, sizeof(*priv));
}

static const struct eth_dev_ops mlx4_dev_ops = {
	.dev_configure = mlx4_dev_configure,
	.dev_start = mlx4_dev_start,
	.dev_stop = mlx4_dev_stop,
	.dev_set_link_down = mlx4_dev_set_link_down,
	.dev_set_link_up = mlx4_dev_set_link_up,
	.dev_close = mlx4_dev_close,
	.link_update = mlx4_link_update,
	.promiscuous_enable = mlx4_promiscuous_enable,
	.promiscuous_disable = mlx4_promiscuous_disable,
	.allmulticast_enable = mlx4_allmulticast_enable,
	.allmulticast_disable = mlx4_allmulticast_disable,
	.mac_addr_remove = mlx4_mac_addr_remove,
	.mac_addr_add = mlx4_mac_addr_add,
	.mac_addr_set = mlx4_mac_addr_set,
	.stats_get = mlx4_stats_get,
	.stats_reset = mlx4_stats_reset,
	.dev_infos_get = mlx4_dev_infos_get,
	.dev_supported_ptypes_get = mlx4_dev_supported_ptypes_get,
	.vlan_filter_set = mlx4_vlan_filter_set,
	.rx_queue_setup = mlx4_rx_queue_setup,
	.tx_queue_setup = mlx4_tx_queue_setup,
	.rx_queue_release = mlx4_rx_queue_release,
	.tx_queue_release = mlx4_tx_queue_release,
	.flow_ctrl_get = mlx4_flow_ctrl_get,
	.flow_ctrl_set = mlx4_flow_ctrl_set,
	.mtu_set = mlx4_mtu_set,
	.filter_ctrl = mlx4_filter_ctrl,
	.rx_queue_intr_enable = mlx4_rx_intr_enable,
	.rx_queue_intr_disable = mlx4_rx_intr_disable,
};

/**
 * Get PCI information from struct ibv_device.
 *
 * @param device
 *   Pointer to Ethernet device structure.
 * @param[out] pci_addr
 *   PCI bus address output buffer.
 *
 * @return
 *   0 on success, negative errno value otherwise and rte_errno is set.
 */
static int
mlx4_ibv_device_to_pci_addr(const struct ibv_device *device,
			    struct rte_pci_addr *pci_addr)
{
	FILE *file;
	char line[32];
	MKSTR(path, "%s/device/uevent", device->ibdev_path);

	file = fopen(path, "rb");
	if (file == NULL) {
		rte_errno = errno;
		return -rte_errno;
	}
	while (fgets(line, sizeof(line), file) == line) {
		size_t len = strlen(line);
		int ret;

		/* Truncate long lines. */
		if (len == (sizeof(line) - 1))
			while (line[(len - 1)] != '\n') {
				ret = fgetc(file);
				if (ret == EOF)
					break;
				line[(len - 1)] = ret;
			}
		/* Extract information. */
		if (sscanf(line,
			   "PCI_SLOT_NAME="
			   "%" SCNx32 ":%" SCNx8 ":%" SCNx8 ".%" SCNx8 "\n",
			   &pci_addr->domain,
			   &pci_addr->bus,
			   &pci_addr->devid,
			   &pci_addr->function) == 4) {
			ret = 0;
			break;
		}
	}
	fclose(file);
	return 0;
}

/**
 * Verify and store value for device argument.
 *
 * @param[in] key
 *   Key argument to verify.
 * @param[in] val
 *   Value associated with key.
 * @param[in, out] conf
 *   Shared configuration data.
 *
 * @return
 *   0 on success, negative errno value otherwise and rte_errno is set.
 */
static int
mlx4_arg_parse(const char *key, const char *val, struct mlx4_conf *conf)
{
	unsigned long tmp;

	errno = 0;
	tmp = strtoul(val, NULL, 0);
	if (errno) {
		rte_errno = errno;
		WARN("%s: \"%s\" is not a valid integer", key, val);
		return -rte_errno;
	}
	if (strcmp(MLX4_PMD_PORT_KVARG, key) == 0) {
		uint32_t ports = rte_log2_u32(conf->ports.present);

		if (tmp >= ports) {
			ERROR("port index %lu outside range [0,%" PRIu32 ")",
			      tmp, ports);
			return -EINVAL;
		}
		if (!(conf->ports.present & (1 << tmp))) {
			rte_errno = EINVAL;
			ERROR("invalid port index %lu", tmp);
			return -rte_errno;
		}
		conf->ports.enabled |= 1 << tmp;
	} else {
		rte_errno = EINVAL;
		WARN("%s: unknown parameter", key);
		return -rte_errno;
	}
	return 0;
}

/**
 * Parse device parameters.
 *
 * @param devargs
 *   Device arguments structure.
 *
 * @return
 *   0 on success, negative errno value otherwise and rte_errno is set.
 */
static int
mlx4_args(struct rte_devargs *devargs, struct mlx4_conf *conf)
{
	struct rte_kvargs *kvlist;
	unsigned int arg_count;
	int ret = 0;
	int i;

	if (devargs == NULL)
		return 0;
	kvlist = rte_kvargs_parse(devargs->args, pmd_mlx4_init_params);
	if (kvlist == NULL) {
		rte_errno = EINVAL;
		ERROR("failed to parse kvargs");
		return -rte_errno;
	}
	/* Process parameters. */
	for (i = 0; pmd_mlx4_init_params[i]; ++i) {
		arg_count = rte_kvargs_count(kvlist, MLX4_PMD_PORT_KVARG);
		while (arg_count-- > 0) {
			ret = rte_kvargs_process(kvlist,
						 MLX4_PMD_PORT_KVARG,
						 (int (*)(const char *,
							  const char *,
							  void *))
						 mlx4_arg_parse,
						 conf);
			if (ret != 0)
				goto free_kvlist;
		}
	}
free_kvlist:
	rte_kvargs_free(kvlist);
	return ret;
}

static struct rte_pci_driver mlx4_driver;

/**
 * DPDK callback to register a PCI device.
 *
 * This function creates an Ethernet device for each port of a given
 * PCI device.
 *
 * @param[in] pci_drv
 *   PCI driver structure (mlx4_driver).
 * @param[in] pci_dev
 *   PCI device information.
 *
 * @return
 *   0 on success, negative errno value otherwise and rte_errno is set.
 */
static int
mlx4_pci_probe(struct rte_pci_driver *pci_drv, struct rte_pci_device *pci_dev)
{
	struct ibv_device **list;
	struct ibv_device *ibv_dev;
	int err = 0;
	struct ibv_context *attr_ctx = NULL;
	struct ibv_device_attr device_attr;
	struct mlx4_conf conf = {
		.ports.present = 0,
	};
	unsigned int vf;
	int i;

	(void)pci_drv;
	assert(pci_drv == &mlx4_driver);
	list = ibv_get_device_list(&i);
	if (list == NULL) {
		rte_errno = errno;
		assert(rte_errno);
		if (rte_errno == ENOSYS)
			ERROR("cannot list devices, is ib_uverbs loaded?");
		return -rte_errno;
	}
	assert(i >= 0);
	/*
	 * For each listed device, check related sysfs entry against
	 * the provided PCI ID.
	 */
	while (i != 0) {
		struct rte_pci_addr pci_addr;

		--i;
		DEBUG("checking device \"%s\"", list[i]->name);
		if (mlx4_ibv_device_to_pci_addr(list[i], &pci_addr))
			continue;
		if ((pci_dev->addr.domain != pci_addr.domain) ||
		    (pci_dev->addr.bus != pci_addr.bus) ||
		    (pci_dev->addr.devid != pci_addr.devid) ||
		    (pci_dev->addr.function != pci_addr.function))
			continue;
		vf = (pci_dev->id.device_id ==
		      PCI_DEVICE_ID_MELLANOX_CONNECTX3VF);
		INFO("PCI information matches, using device \"%s\" (VF: %s)",
		     list[i]->name, (vf ? "true" : "false"));
		attr_ctx = ibv_open_device(list[i]);
		err = errno;
		break;
	}
	if (attr_ctx == NULL) {
		ibv_free_device_list(list);
		switch (err) {
		case 0:
			rte_errno = ENODEV;
			ERROR("cannot access device, is mlx4_ib loaded?");
			return -rte_errno;
		case EINVAL:
			rte_errno = EINVAL;
			ERROR("cannot use device, are drivers up to date?");
			return -rte_errno;
		}
		assert(err > 0);
		rte_errno = err;
		return -rte_errno;
	}
	ibv_dev = list[i];
	DEBUG("device opened");
	if (ibv_query_device(attr_ctx, &device_attr)) {
		rte_errno = ENODEV;
		goto error;
	}
	INFO("%u port(s) detected", device_attr.phys_port_cnt);
	conf.ports.present |= (UINT64_C(1) << device_attr.phys_port_cnt) - 1;
	if (mlx4_args(pci_dev->device.devargs, &conf)) {
		ERROR("failed to process device arguments");
		rte_errno = EINVAL;
		goto error;
	}
	/* Use all ports when none are defined */
	if (!conf.ports.enabled)
		conf.ports.enabled = conf.ports.present;
	for (i = 0; i < device_attr.phys_port_cnt; i++) {
		uint32_t port = i + 1; /* ports are indexed from one */
		struct ibv_context *ctx = NULL;
		struct ibv_port_attr port_attr;
		struct ibv_pd *pd = NULL;
		struct priv *priv = NULL;
		struct rte_eth_dev *eth_dev = NULL;
		struct ether_addr mac;

		/* If port is not enabled, skip. */
		if (!(conf.ports.enabled & (1 << i)))
			continue;
		DEBUG("using port %u", port);
		ctx = ibv_open_device(ibv_dev);
		if (ctx == NULL) {
			rte_errno = ENODEV;
			goto port_error;
		}
		/* Check port status. */
		err = ibv_query_port(ctx, port, &port_attr);
		if (err) {
			rte_errno = err;
			ERROR("port query failed: %s", strerror(rte_errno));
			goto port_error;
		}
		if (port_attr.link_layer != IBV_LINK_LAYER_ETHERNET) {
			rte_errno = ENOTSUP;
			ERROR("port %d is not configured in Ethernet mode",
			      port);
			goto port_error;
		}
		if (port_attr.state != IBV_PORT_ACTIVE)
			DEBUG("port %d is not active: \"%s\" (%d)",
			      port, ibv_port_state_str(port_attr.state),
			      port_attr.state);
		/* Make asynchronous FD non-blocking to handle interrupts. */
		if (mlx4_fd_set_non_blocking(ctx->async_fd) < 0) {
			ERROR("cannot make asynchronous FD non-blocking: %s",
			      strerror(rte_errno));
			goto port_error;
		}
		/* Allocate protection domain. */
		pd = ibv_alloc_pd(ctx);
		if (pd == NULL) {
			rte_errno = ENOMEM;
			ERROR("PD allocation failure");
			goto port_error;
		}
		/* from rte_ethdev.c */
		priv = rte_zmalloc("ethdev private structure",
				   sizeof(*priv),
				   RTE_CACHE_LINE_SIZE);
		if (priv == NULL) {
			rte_errno = ENOMEM;
			ERROR("priv allocation failure");
			goto port_error;
		}
		priv->ctx = ctx;
		priv->device_attr = device_attr;
		priv->port = port;
		priv->pd = pd;
		priv->mtu = ETHER_MTU;
		priv->vf = vf;
		priv->hw_csum =	!!(device_attr.device_cap_flags &
				   IBV_DEVICE_RAW_IP_CSUM);
		DEBUG("checksum offloading is %ssupported",
		      (priv->hw_csum ? "" : "not "));
		/* Only ConnectX-3 Pro supports tunneling. */
		priv->hw_csum_l2tun =
			priv->hw_csum &&
			(device_attr.vendor_part_id ==
			 PCI_DEVICE_ID_MELLANOX_CONNECTX3PRO);
		DEBUG("L2 tunnel checksum offloads are %ssupported",
		      (priv->hw_csum_l2tun ? "" : "not "));
		/* Configure the first MAC address by default. */
		if (mlx4_get_mac(priv, &mac.addr_bytes)) {
			ERROR("cannot get MAC address, is mlx4_en loaded?"
			      " (rte_errno: %s)", strerror(rte_errno));
			goto port_error;
		}
		INFO("port %u MAC address is %02x:%02x:%02x:%02x:%02x:%02x",
		     priv->port,
		     mac.addr_bytes[0], mac.addr_bytes[1],
		     mac.addr_bytes[2], mac.addr_bytes[3],
		     mac.addr_bytes[4], mac.addr_bytes[5]);
		/* Register MAC address. */
		priv->mac[0] = mac;
#ifndef NDEBUG
		{
			char ifname[IF_NAMESIZE];

			if (mlx4_get_ifname(priv, &ifname) == 0)
				DEBUG("port %u ifname is \"%s\"",
				      priv->port, ifname);
			else
				DEBUG("port %u ifname is unknown", priv->port);
		}
#endif
		/* Get actual MTU if possible. */
		mlx4_mtu_get(priv, &priv->mtu);
		DEBUG("port %u MTU is %u", priv->port, priv->mtu);
		/* from rte_ethdev.c */
		{
			char name[RTE_ETH_NAME_MAX_LEN];

			snprintf(name, sizeof(name), "%s port %u",
				 ibv_get_device_name(ibv_dev), port);
			eth_dev = rte_eth_dev_allocate(name);
		}
		if (eth_dev == NULL) {
			ERROR("can not allocate rte ethdev");
			rte_errno = ENOMEM;
			goto port_error;
		}
		eth_dev->data->dev_private = priv;
		eth_dev->data->mac_addrs = priv->mac;
		eth_dev->device = &pci_dev->device;
		rte_eth_copy_pci_info(eth_dev, pci_dev);
		eth_dev->device->driver = &mlx4_driver.driver;
		/* Initialize local interrupt handle for current port. */
		priv->intr_handle = (struct rte_intr_handle){
			.fd = -1,
			.type = RTE_INTR_HANDLE_EXT,
		};
		/*
		 * Override ethdev interrupt handle pointer with private
		 * handle instead of that of the parent PCI device used by
		 * default. This prevents it from being shared between all
		 * ports of the same PCI device since each of them is
		 * associated its own Verbs context.
		 *
		 * Rx interrupts in particular require this as the PMD has
		 * no control over the registration of queue interrupts
		 * besides setting up eth_dev->intr_handle, the rest is
		 * handled by rte_intr_rx_ctl().
		 */
		eth_dev->intr_handle = &priv->intr_handle;
		priv->dev = eth_dev;
		eth_dev->dev_ops = &mlx4_dev_ops;
		/* Bring Ethernet device up. */
		DEBUG("forcing Ethernet interface up");
		mlx4_dev_set_link_up(priv->dev);
		/* Update link status once if waiting for LSC. */
		if (eth_dev->data->dev_flags & RTE_ETH_DEV_INTR_LSC)
			mlx4_link_update(eth_dev, 0);
		continue;
port_error:
		rte_free(priv);
		if (pd)
			claim_zero(ibv_dealloc_pd(pd));
		if (ctx)
			claim_zero(ibv_close_device(ctx));
		if (eth_dev)
			rte_eth_dev_release_port(eth_dev);
		break;
	}
	if (i == device_attr.phys_port_cnt)
		return 0;
	/*
	 * XXX if something went wrong in the loop above, there is a resource
	 * leak (ctx, pd, priv, dpdk ethdev) but we can do nothing about it as
	 * long as the dpdk does not provide a way to deallocate a ethdev and a
	 * way to enumerate the registered ethdevs to free the previous ones.
	 */
error:
	if (attr_ctx)
		claim_zero(ibv_close_device(attr_ctx));
	if (list)
		ibv_free_device_list(list);
	assert(rte_errno >= 0);
	return -rte_errno;
}

static const struct rte_pci_id mlx4_pci_id_map[] = {
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_MELLANOX,
			       PCI_DEVICE_ID_MELLANOX_CONNECTX3)
	},
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_MELLANOX,
			       PCI_DEVICE_ID_MELLANOX_CONNECTX3PRO)
	},
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_MELLANOX,
			       PCI_DEVICE_ID_MELLANOX_CONNECTX3VF)
	},
	{
		.vendor_id = 0
	}
};

static struct rte_pci_driver mlx4_driver = {
	.driver = {
		.name = MLX4_DRIVER_NAME
	},
	.id_table = mlx4_pci_id_map,
	.probe = mlx4_pci_probe,
	.drv_flags = RTE_PCI_DRV_INTR_LSC |
		     RTE_PCI_DRV_INTR_RMV,
};

/**
 * Driver initialization routine.
 */
RTE_INIT(rte_mlx4_pmd_init);
static void
rte_mlx4_pmd_init(void)
{
	/*
	 * RDMAV_HUGEPAGES_SAFE tells ibv_fork_init() we intend to use
	 * huge pages. Calling ibv_fork_init() during init allows
	 * applications to use fork() safely for purposes other than
	 * using this PMD, which is not supported in forked processes.
	 */
	setenv("RDMAV_HUGEPAGES_SAFE", "1", 1);
	ibv_fork_init();
	rte_pci_register(&mlx4_driver);
}

RTE_PMD_EXPORT_NAME(net_mlx4, __COUNTER__);
RTE_PMD_REGISTER_PCI_TABLE(net_mlx4, mlx4_pci_id_map);
RTE_PMD_REGISTER_KMOD_DEP(net_mlx4,
	"* ib_uverbs & mlx4_en & mlx4_core & mlx4_ib");
