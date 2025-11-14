/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2015 6WIND S.A.
 * Copyright 2015 Mellanox Technologies, Ltd
 */

#include <stddef.h>
#include <inttypes.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <dirent.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <linux/ethtool.h>
#include <linux/sockios.h>
#include <fcntl.h>
#include <stdalign.h>
#include <sys/un.h>
#include <time.h>

#include <ethdev_linux_ethtool.h>
#include <ethdev_driver.h>
#include <bus_pci_driver.h>
#include <rte_mbuf.h>
#include <rte_common.h>
#include <rte_eal_paging.h>
#include <rte_interrupts.h>
#include <rte_malloc.h>
#include <rte_string_fns.h>
#include <rte_rwlock.h>
#include <rte_cycles.h>

#include <mlx5_glue.h>
#include <mlx5_devx_cmds.h>
#include <mlx5_common.h>
#include <mlx5_malloc.h>
#include <mlx5_nl.h>

#include "mlx5.h"
#include "mlx5_rxtx.h"
#include "mlx5_utils.h"

/* Get interface index from SubFunction device name. */
int
mlx5_auxiliary_get_ifindex(const char *sf_name)
{
	char if_name[IF_NAMESIZE] = { 0 };

	if (mlx5_auxiliary_get_child_name(sf_name, "/net",
					  if_name, sizeof(if_name)) != 0)
		return -rte_errno;
	return if_nametoindex(if_name);
}

/**
 * Get interface name from private structure.
 *
 * This is a port representor-aware version of mlx5_get_ifname_sysfs().
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[out] ifname
 *   Interface name output buffer.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_get_ifname(const struct rte_eth_dev *dev, char ifname[MLX5_NAMESIZE])
{
	struct mlx5_priv *priv = dev->data->dev_private;
	unsigned int ifindex;

	MLX5_ASSERT(priv);
	MLX5_ASSERT(priv->sh);
	if (priv->master && priv->sh->bond.ifindex > 0) {
		memcpy(ifname, priv->sh->bond.ifname, MLX5_NAMESIZE);
		return 0;
	}
	ifindex = mlx5_ifindex(dev);
	if (!ifindex) {
		if (!priv->representor)
			return mlx5_get_ifname_sysfs(priv->sh->ibdev_path, ifname);
		rte_errno = ENXIO;
		return -rte_errno;
	}
	if (if_indextoname(ifindex, ifname))
		return 0;
	rte_errno = errno;
	return -rte_errno;
}

/**
 * Perform ifreq ioctl() on associated netdev ifname.
 *
 * @param[in] ifname
 *   Pointer to netdev name.
 * @param req
 *   Request number to pass to ioctl().
 * @param[out] ifr
 *   Interface request structure output buffer.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_ifreq_by_ifname(const char *ifname, int req, struct ifreq *ifr)
{
	int sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
	int ret = 0;

	if (sock == -1) {
		rte_errno = errno;
		return -rte_errno;
	}
	rte_strscpy(ifr->ifr_name, ifname, sizeof(ifr->ifr_name));
	ret = ioctl(sock, req, ifr);
	if (ret == -1) {
		rte_errno = errno;
		goto error;
	}
	close(sock);
	return 0;
error:
	close(sock);
	return -rte_errno;
}

/**
 * Perform ifreq ioctl() on associated Ethernet device.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param req
 *   Request number to pass to ioctl().
 * @param[out] ifr
 *   Interface request structure output buffer.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_ifreq(const struct rte_eth_dev *dev, int req, struct ifreq *ifr)
{
	char ifname[MLX5_NAMESIZE];
	int ret;

	ret = mlx5_get_ifname(dev, ifname);
	if (ret)
		return -rte_errno;
	return mlx5_ifreq_by_ifname(ifname, req, ifr);
}

/**
 * Get device minimum and maximum allowed MTU values.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param[out] min_mtu
 *   Minimum MTU value output buffer.
 * @param[out] max_mtu
 *   Maximum MTU value output buffer.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_os_get_mtu_bounds(struct rte_eth_dev *dev, uint16_t *min_mtu, uint16_t *max_mtu)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	int nl_route;
	int ret;

	nl_route = mlx5_nl_init(NETLINK_ROUTE, 0);
	if  (nl_route < 0)
		return nl_route;

	ret = mlx5_nl_get_mtu_bounds(nl_route, priv->if_index, min_mtu, max_mtu);

	close(nl_route);
	return ret;
}

/**
 * Get device MTU.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param[out] mtu
 *   MTU value output buffer.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_get_mtu(struct rte_eth_dev *dev, uint16_t *mtu)
{
	struct ifreq request;
	int ret = mlx5_ifreq(dev, SIOCGIFMTU, &request);

	if (ret)
		return ret;
	*mtu = request.ifr_mtu;
	return 0;
}

/**
 * Set device MTU.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param mtu
 *   MTU value to set.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_set_mtu(struct rte_eth_dev *dev, uint16_t mtu)
{
	struct ifreq request = { .ifr_mtu = mtu, };

	return mlx5_ifreq(dev, SIOCSIFMTU, &request);
}

/**
 * Set device flags.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param keep
 *   Bitmask for flags that must remain untouched.
 * @param flags
 *   Bitmask for flags to modify.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_set_flags(struct rte_eth_dev *dev, unsigned int keep, unsigned int flags)
{
	struct ifreq request;
	int ret = mlx5_ifreq(dev, SIOCGIFFLAGS, &request);

	if (ret)
		return ret;
	request.ifr_flags &= keep;
	request.ifr_flags |= flags & ~keep;
	return mlx5_ifreq(dev, SIOCSIFFLAGS, &request);
}

/**
 * Get device current raw clock counter
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param[out] time
 *   Current raw clock counter of the device.
 *
 * @return
 *   0 if the clock has correctly been read
 *   The value of errno in case of error
 */
int
mlx5_read_clock(struct rte_eth_dev *dev, uint64_t *clock)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct ibv_context *ctx = priv->sh->cdev->ctx;
	struct ibv_values_ex values;
	int err = 0;

	values.comp_mask = IBV_VALUES_MASK_RAW_CLOCK;
	err = mlx5_glue->query_rt_values_ex(ctx, &values);
	if (err != 0) {
		DRV_LOG(WARNING, "Could not query the clock !");
		return err;
	}
	*clock = values.raw_clock.tv_nsec;
	return 0;
}

/**
 * Retrieve the master device for representor in the same switch domain.
 *
 * @param dev
 *   Pointer to representor Ethernet device structure.
 *
 * @return
 *   Master device structure  on success, NULL otherwise.
 */
static struct rte_eth_dev *
mlx5_find_master_dev(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv;
	uint16_t port_id;
	uint16_t domain_id;

	priv = dev->data->dev_private;
	domain_id = priv->domain_id;
	MLX5_ASSERT(priv->representor);
	MLX5_ETH_FOREACH_DEV(port_id, dev->device) {
		struct mlx5_priv *opriv =
			rte_eth_devices[port_id].data->dev_private;
		if (opriv &&
		    opriv->master &&
		    opriv->domain_id == domain_id &&
		    opriv->sh == priv->sh)
			return &rte_eth_devices[port_id];
	}
	return NULL;
}

/**
 * DPDK callback to retrieve physical link information.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param[out] link
 *   Storage for current link status.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_link_update_unlocked_gset(struct rte_eth_dev *dev,
			       struct rte_eth_link *link)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct ethtool_cmd edata = {
		.cmd = ETHTOOL_GSET /* Deprecated since Linux v4.5. */
	};
	struct ifreq ifr;
	struct rte_eth_link dev_link;
	int link_speed = 0;
	int ret;

	ret = mlx5_ifreq(dev, SIOCGIFFLAGS, &ifr);
	if (ret) {
		DRV_LOG(WARNING, "port %u ioctl(SIOCGIFFLAGS) failed: %s",
			dev->data->port_id, strerror(rte_errno));
		return ret;
	}
	dev_link = (struct rte_eth_link) {
		.link_status = ((ifr.ifr_flags & IFF_UP) &&
				(ifr.ifr_flags & IFF_RUNNING)),
	};
	ifr = (struct ifreq) {
		.ifr_data = (void *)&edata,
	};
	ret = mlx5_ifreq(dev, SIOCETHTOOL, &ifr);
	if (ret) {
		if (ret == -ENOTSUP && priv->representor) {
			struct rte_eth_dev *master;

			/*
			 * For representors we can try to inherit link
			 * settings from the master device. Actually
			 * link settings do not make a lot of sense
			 * for representors due to missing physical
			 * link. The old kernel drivers supported
			 * emulated settings query for representors,
			 * the new ones do not, so we have to add
			 * this code for compatibility issues.
			 */
			master = mlx5_find_master_dev(dev);
			if (master) {
				ifr = (struct ifreq) {
					.ifr_data = (void *)&edata,
				};
				ret = mlx5_ifreq(master, SIOCETHTOOL, &ifr);
			}
		}
		if (ret) {
			DRV_LOG(WARNING,
				"port %u ioctl(SIOCETHTOOL,"
				" ETHTOOL_GSET) failed: %s",
				dev->data->port_id, strerror(rte_errno));
			return ret;
		}
	}
	link_speed = ethtool_cmd_speed(&edata);
	if (link_speed == -1)
		dev_link.link_speed = RTE_ETH_SPEED_NUM_UNKNOWN;
	else
		dev_link.link_speed = link_speed;
	dev_link.link_duplex = ((edata.duplex == DUPLEX_HALF) ?
				RTE_ETH_LINK_HALF_DUPLEX : RTE_ETH_LINK_FULL_DUPLEX);
	dev_link.link_autoneg = !(dev->data->dev_conf.link_speeds &
			RTE_ETH_LINK_SPEED_FIXED);
	*link = dev_link;
	priv->link_speed_capa = rte_eth_link_speed_gset(edata.supported);
	return 0;
}

/**
 * Retrieve physical link information (unlocked version using new ioctl).
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param[out] link
 *   Storage for current link status.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_link_update_unlocked_gs(struct rte_eth_dev *dev,
			     struct rte_eth_link *link)

{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct ethtool_link_settings gcmd = { .cmd = ETHTOOL_GLINKSETTINGS };
	struct ifreq ifr;
	struct rte_eth_link dev_link;
	struct rte_eth_dev *master = NULL;
	int ret;

	ret = mlx5_ifreq(dev, SIOCGIFFLAGS, &ifr);
	if (ret) {
		DRV_LOG(WARNING, "port %u ioctl(SIOCGIFFLAGS) failed: %s",
			dev->data->port_id, strerror(rte_errno));
		return ret;
	}
	dev_link = (struct rte_eth_link) {
		.link_status = ((ifr.ifr_flags & IFF_UP) &&
				(ifr.ifr_flags & IFF_RUNNING)),
	};
	ifr = (struct ifreq) {
		.ifr_data = (void *)&gcmd,
	};
	ret = mlx5_ifreq(dev, SIOCETHTOOL, &ifr);
	if (ret) {
		if (ret == -ENOTSUP && priv->representor) {
			/*
			 * For representors we can try to inherit link
			 * settings from the master device. Actually
			 * link settings do not make a lot of sense
			 * for representors due to missing physical
			 * link. The old kernel drivers supported
			 * emulated settings query for representors,
			 * the new ones do not, so we have to add
			 * this code for compatibility issues.
			 */
			master = mlx5_find_master_dev(dev);
			if (master) {
				ifr = (struct ifreq) {
					.ifr_data = (void *)&gcmd,
				};
				ret = mlx5_ifreq(master, SIOCETHTOOL, &ifr);
			}
		}
		if (ret) {
			DRV_LOG(DEBUG,
				"port %u ioctl(SIOCETHTOOL,"
				" ETHTOOL_GLINKSETTINGS) failed: %s",
				dev->data->port_id, strerror(rte_errno));
			return ret;
		}
	}
	gcmd.link_mode_masks_nwords = -gcmd.link_mode_masks_nwords;

	alignas(struct ethtool_link_settings)
	uint8_t data[offsetof(struct ethtool_link_settings, link_mode_masks) +
		     sizeof(uint32_t) * gcmd.link_mode_masks_nwords * 3];
	struct ethtool_link_settings *ecmd = (void *)data;

	*ecmd = gcmd;
	ifr.ifr_data = (void *)ecmd;
	ret = mlx5_ifreq(master ? master : dev, SIOCETHTOOL, &ifr);
	if (ret) {
		DRV_LOG(DEBUG,
			"port %u ioctl(SIOCETHTOOL,"
			"ETHTOOL_GLINKSETTINGS) failed: %s",
			dev->data->port_id, strerror(rte_errno));
		return ret;
	}

	dev_link.link_speed = (ecmd->speed == UINT32_MAX) ?
				RTE_ETH_SPEED_NUM_UNKNOWN : ecmd->speed;
	dev_link.link_duplex = ((ecmd->duplex == DUPLEX_HALF) ?
				RTE_ETH_LINK_HALF_DUPLEX : RTE_ETH_LINK_FULL_DUPLEX);
	dev_link.link_autoneg = !(dev->data->dev_conf.link_speeds &
				  RTE_ETH_LINK_SPEED_FIXED);
	*link = dev_link;

	priv->link_speed_capa = rte_eth_link_speed_glink(ecmd->link_mode_masks,
			ecmd->link_mode_masks_nwords);

	return 0;
}

/**
 * DPDK callback to retrieve physical link information.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param wait_to_complete
 *   Wait for request completion.
 *
 * @return
 *   0 if link status was not updated, positive if it was, a negative errno
 *   value otherwise and rte_errno is set.
 */
int
mlx5_link_update(struct rte_eth_dev *dev, int wait_to_complete)
{
	int ret;
	struct rte_eth_link dev_link = { 0 };
	time_t start_time = time(NULL);
	int retry = MLX5_GET_LINK_STATUS_RETRY_COUNT;

	do {
		ret = mlx5_link_update_unlocked_gs(dev, &dev_link);
		if (ret == -ENOTSUP)
			ret = mlx5_link_update_unlocked_gset(dev, &dev_link);
		if (ret == 0)
			break;
		/* Handle wait to complete situation. */
		if ((wait_to_complete || retry) && ret == -EAGAIN) {
			if (abs((int)difftime(time(NULL), start_time)) <
			    MLX5_LINK_STATUS_TIMEOUT) {
				usleep(0);
				continue;
			} else {
				rte_errno = EBUSY;
				return -rte_errno;
			}
		} else if (ret < 0) {
			return ret;
		}
	} while (wait_to_complete || retry-- > 0);
	ret = !!memcmp(&dev->data->dev_link, &dev_link,
		       sizeof(struct rte_eth_link));
	dev->data->dev_link = dev_link;
	return ret;
}

/**
 * DPDK callback to get flow control status.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param[out] fc_conf
 *   Flow control output buffer.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_dev_get_flow_ctrl(struct rte_eth_dev *dev, struct rte_eth_fc_conf *fc_conf)
{
	struct ifreq ifr;
	struct ethtool_pauseparam ethpause = {
		.cmd = ETHTOOL_GPAUSEPARAM
	};
	int ret;

	ifr.ifr_data = (void *)&ethpause;
	ret = mlx5_ifreq(dev, SIOCETHTOOL, &ifr);
	if (ret) {
		DRV_LOG(DEBUG,
			"port %u ioctl(SIOCETHTOOL, ETHTOOL_GPAUSEPARAM) failed:"
			" %s",
			dev->data->port_id, strerror(rte_errno));
		return ret;
	}
	fc_conf->autoneg = ethpause.autoneg;
	if (ethpause.rx_pause && ethpause.tx_pause)
		fc_conf->mode = RTE_ETH_FC_FULL;
	else if (ethpause.rx_pause)
		fc_conf->mode = RTE_ETH_FC_RX_PAUSE;
	else if (ethpause.tx_pause)
		fc_conf->mode = RTE_ETH_FC_TX_PAUSE;
	else
		fc_conf->mode = RTE_ETH_FC_NONE;
	return 0;
}

/**
 * DPDK callback to modify flow control parameters.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param[in] fc_conf
 *   Flow control parameters.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_dev_set_flow_ctrl(struct rte_eth_dev *dev, struct rte_eth_fc_conf *fc_conf)
{
	struct ifreq ifr;
	struct ethtool_pauseparam ethpause = {
		.cmd = ETHTOOL_SPAUSEPARAM
	};
	int ret;

	ifr.ifr_data = (void *)&ethpause;
	ethpause.autoneg = fc_conf->autoneg;
	if (((fc_conf->mode & RTE_ETH_FC_FULL) == RTE_ETH_FC_FULL) ||
	    (fc_conf->mode & RTE_ETH_FC_RX_PAUSE))
		ethpause.rx_pause = 1;
	else
		ethpause.rx_pause = 0;

	if (((fc_conf->mode & RTE_ETH_FC_FULL) == RTE_ETH_FC_FULL) ||
	    (fc_conf->mode & RTE_ETH_FC_TX_PAUSE))
		ethpause.tx_pause = 1;
	else
		ethpause.tx_pause = 0;
	ret = mlx5_ifreq(dev, SIOCETHTOOL, &ifr);
	if (ret) {
		DRV_LOG(WARNING,
			"port %u ioctl(SIOCETHTOOL, ETHTOOL_SPAUSEPARAM)"
			" failed: %s",
			dev->data->port_id, strerror(rte_errno));
		return ret;
	}
	return 0;
}

/**
 * Handle asynchronous removal event for entire multiport device.
 *
 * @param sh
 *   Infiniband device shared context.
 */
static void
mlx5_dev_interrupt_device_fatal(struct mlx5_dev_ctx_shared *sh)
{
	uint32_t i;

	for (i = 0; i < sh->max_port; ++i) {
		struct rte_eth_dev *dev;
		struct mlx5_priv *priv;

		if (sh->port[i].ih_port_id >= RTE_MAX_ETHPORTS) {
			/*
			 * Or not existing port either no
			 * handler installed for this port.
			 */
			continue;
		}
		dev = &rte_eth_devices[sh->port[i].ih_port_id];
		MLX5_ASSERT(dev);
		priv = dev->data->dev_private;
		MLX5_ASSERT(priv);
		if (!priv->rmv_notified && dev->data->dev_conf.intr_conf.rmv) {
			/* Notify driver about removal only once. */
			priv->rmv_notified = 1;
			rte_eth_dev_callback_process
				(dev, RTE_ETH_EVENT_INTR_RMV, NULL);
		}
	}
}

static bool
mlx5_dev_nl_ifindex_verify(uint32_t if_index, struct mlx5_priv *priv)
{
	struct mlx5_bond_info *bond = &priv->sh->bond;
	int i;

	if (bond->n_port == 0)
		return (if_index == priv->if_index);

	if (if_index == bond->ifindex)
		return true;
	for (i = 0; i < bond->n_port; i++) {
		if (i >= MLX5_BOND_MAX_PORTS)
			return false;
		if (if_index == bond->ports[i].ifindex)
			return true;
	}

	return false;
}

static void
mlx5_link_update_bond(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_bond_info *bond = &priv->sh->bond;
	struct ifreq ifr = (struct ifreq) {
		.ifr_flags = 0,
	};
	int ret;

	ret = mlx5_ifreq_by_ifname(bond->ifname, SIOCGIFFLAGS, &ifr);
	if (ret) {
		DRV_LOG(WARNING, "ifname %s ioctl(SIOCGIFFLAGS) failed: %s",
			bond->ifname, strerror(rte_errno));
		return;
	}
	dev->data->dev_link.link_status =
		((ifr.ifr_flags & IFF_UP) && (ifr.ifr_flags & IFF_RUNNING));
}

static void
mlx5_dev_interrupt_nl_cb(struct nlmsghdr *hdr, void *cb_arg)
{
	struct mlx5_dev_ctx_shared *sh = cb_arg;
	uint32_t i;
	uint32_t if_index;

	if (mlx5_nl_parse_link_status_update(hdr, &if_index) < 0)
		return;
	for (i = 0; i < sh->max_port; i++) {
		struct mlx5_dev_shared_port *port = &sh->port[i];
		struct rte_eth_dev *dev;
		struct mlx5_priv *priv;

		if (port->nl_ih_port_id >= RTE_MAX_ETHPORTS)
			continue;
		dev = &rte_eth_devices[port->nl_ih_port_id];
		/* Probing may initiate an LSC before configuration is done. */
		if (dev->data->dev_configured &&
		    !dev->data->dev_conf.intr_conf.lsc)
			break;
		priv = dev->data->dev_private;
		if (mlx5_dev_nl_ifindex_verify(if_index, priv)) {
			/* Block logical LSC events. */
			uint16_t prev_status = dev->data->dev_link.link_status;

			if (mlx5_link_update(dev, 0) < 0) {
				DRV_LOG(ERR, "Failed to update link status: %s",
					rte_strerror(rte_errno));
			} else {
				if (priv->sh->bond.n_port)
					mlx5_link_update_bond(dev);
				if (prev_status != dev->data->dev_link.link_status)
					rte_eth_dev_callback_process
						(dev, RTE_ETH_EVENT_INTR_LSC, NULL);
			}
			break;
		}
	}
}

void
mlx5_dev_interrupt_handler_nl(void *arg)
{
	struct mlx5_dev_ctx_shared *sh = arg;
	int nlsk_fd = rte_intr_fd_get(sh->intr_handle_nl);

	if (nlsk_fd < 0)
		return;
	if (mlx5_nl_read_events(nlsk_fd, mlx5_dev_interrupt_nl_cb, sh) < 0)
		DRV_LOG(ERR, "Failed to process Netlink events: %s",
			rte_strerror(rte_errno));
}

/**
 * Handle shared asynchronous events the NIC (removal event
 * and link status change). Supports multiport IB device.
 *
 * @param cb_arg
 *   Callback argument.
 */
void
mlx5_dev_interrupt_handler(void *cb_arg)
{
	struct mlx5_dev_ctx_shared *sh = cb_arg;
	struct ibv_async_event event;

	/* Read all message from the IB device and acknowledge them. */
	for (;;) {
		struct rte_eth_dev *dev;
		uint32_t tmp;

		if (mlx5_glue->get_async_event(sh->cdev->ctx, &event)) {
			if (errno == EIO) {
				DRV_LOG(DEBUG,
					"IBV async event queue closed on: %s",
					sh->ibdev_name);
				mlx5_dev_interrupt_device_fatal(sh);
			}
			break;
		}
		if (event.event_type == IBV_EVENT_DEVICE_FATAL) {
			/*
			 * The DEVICE_FATAL event can be called by kernel
			 * twice - from mlx5 and uverbs layers, and port
			 * index is not applicable. We should notify all
			 * existing ports.
			 */
			mlx5_dev_interrupt_device_fatal(sh);
			mlx5_glue->ack_async_event(&event);
			continue;
		}
		/* Retrieve and check IB port index. */
		tmp = (uint32_t)event.element.port_num;
		MLX5_ASSERT(tmp <= sh->max_port);
		if (!tmp) {
			/* Unsupported device level event. */
			mlx5_glue->ack_async_event(&event);
			DRV_LOG(DEBUG,
				"unsupported common event (type %d)",
				event.event_type);
			continue;
		}
		if (tmp > sh->max_port) {
			/* Invalid IB port index. */
			mlx5_glue->ack_async_event(&event);
			DRV_LOG(DEBUG,
				"cannot handle an event (type %d)"
				"due to invalid IB port index (%u)",
				event.event_type, tmp);
			continue;
		}
		if (sh->port[tmp - 1].ih_port_id >= RTE_MAX_ETHPORTS) {
			/* No handler installed. */
			mlx5_glue->ack_async_event(&event);
			DRV_LOG(DEBUG,
				"cannot handle an event (type %d)"
				"due to no handler installed for port %u",
				event.event_type, tmp);
			continue;
		}
		/* Retrieve ethernet device descriptor. */
		tmp = sh->port[tmp - 1].ih_port_id;
		dev = &rte_eth_devices[tmp];
		MLX5_ASSERT(dev);
		DRV_LOG(DEBUG,
			"port %u cannot handle an unknown event (type %d)",
			dev->data->port_id, event.event_type);
		mlx5_glue->ack_async_event(&event);
	}
}

/**
 * Handle DEVX interrupts from the NIC.
 * This function is probably called from the DPDK host thread.
 *
 * @param cb_arg
 *   Callback argument.
 */
void
mlx5_dev_interrupt_handler_devx(void *cb_arg)
{
#ifndef HAVE_IBV_DEVX_ASYNC
	(void)cb_arg;
	return;
#else
	struct mlx5_dev_ctx_shared *sh = cb_arg;
	union {
		struct mlx5dv_devx_async_cmd_hdr cmd_resp;
		uint8_t buf[MLX5_ST_SZ_BYTES(query_flow_counter_out) +
			    MLX5_ST_SZ_BYTES(traffic_counter) +
			    sizeof(struct mlx5dv_devx_async_cmd_hdr)];
	} out;
	uint8_t *buf = out.buf + sizeof(out.cmd_resp);

	while (!mlx5_glue->devx_get_async_cmd_comp(sh->devx_comp,
						   &out.cmd_resp,
						   sizeof(out.buf)))
		mlx5_flow_async_pool_query_handle
			(sh, (uint64_t)out.cmd_resp.wr_id,
			 mlx5_devx_get_out_command_status(buf));
#endif /* HAVE_IBV_DEVX_ASYNC */
}

/**
 * DPDK callback to bring the link DOWN.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_set_link_down(struct rte_eth_dev *dev)
{
	return mlx5_set_flags(dev, ~IFF_UP, ~IFF_UP);
}

/**
 * DPDK callback to bring the link UP.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_set_link_up(struct rte_eth_dev *dev)
{
	return mlx5_set_flags(dev, ~IFF_UP, IFF_UP);
}

/**
 * Check if mlx5 device was removed.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   1 when device is removed, otherwise 0.
 */
int
mlx5_is_removed(struct rte_eth_dev *dev)
{
	struct ibv_device_attr device_attr;
	struct mlx5_priv *priv = dev->data->dev_private;

	if (mlx5_glue->query_device(priv->sh->cdev->ctx, &device_attr) == EIO)
		return 1;
	return 0;
}

/**
 * Analyze gathered port parameters via sysfs to recognize master
 * and representor devices for E-Switch configuration.
 *
 * @param[in] device_dir
 *   flag of presence of "device" directory under port device key.
 * @param[inout] switch_info
 *   Port information, including port name as a number and port name
 *   type if recognized
 *
 * @return
 *   master and representor flags are set in switch_info according to
 *   recognized parameters (if any).
 */
static void
mlx5_sysfs_check_switch_info(bool device_dir,
			     struct mlx5_switch_info *switch_info)
{
	switch (switch_info->name_type) {
	case MLX5_PHYS_PORT_NAME_TYPE_UNKNOWN:
		/*
		 * Name is not recognized, assume the master,
		 * check the device directory presence.
		 */
		switch_info->master = device_dir;
		break;
	case MLX5_PHYS_PORT_NAME_TYPE_NOTSET:
		/*
		 * Name is not set, this assumes the legacy naming
		 * schema for master, just check if there is
		 * a device directory.
		 */
		switch_info->master = device_dir;
		break;
	case MLX5_PHYS_PORT_NAME_TYPE_UPLINK:
		/* New uplink naming schema recognized. */
		switch_info->master = 1;
		break;
	case MLX5_PHYS_PORT_NAME_TYPE_LEGACY:
		/* Legacy representors naming schema. */
		switch_info->representor = !device_dir;
		break;
	case MLX5_PHYS_PORT_NAME_TYPE_PFHPF:
		/* Fallthrough */
	case MLX5_PHYS_PORT_NAME_TYPE_PFVF:
		/* Fallthrough */
	case MLX5_PHYS_PORT_NAME_TYPE_PFSF:
		/* New representors naming schema. */
		switch_info->representor = 1;
		break;
	default:
		switch_info->master = device_dir;
		break;
	}
}

/**
 * Get switch information associated with network interface.
 *
 * @param ifindex
 *   Network interface index.
 * @param[out] info
 *   Switch information object, populated in case of success.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_sysfs_switch_info(unsigned int ifindex, struct mlx5_switch_info *info)
{
	char ifname[IF_NAMESIZE];
	char *port_name = NULL;
	size_t port_name_size = 0;
	FILE *file;
	struct mlx5_switch_info data = {
		.master = 0,
		.representor = 0,
		.name_type = MLX5_PHYS_PORT_NAME_TYPE_NOTSET,
		.port_name = 0,
		.switch_id = 0,
	};
	DIR *dir;
	bool port_switch_id_set = false;
	bool device_dir = false;
	char c;
	ssize_t line_size;

	if (!if_indextoname(ifindex, ifname)) {
		rte_errno = errno;
		return -rte_errno;
	}

	MKSTR(phys_port_name, "/sys/class/net/%s/phys_port_name",
	      ifname);
	MKSTR(phys_switch_id, "/sys/class/net/%s/phys_switch_id",
	      ifname);
	MKSTR(pci_device, "/sys/class/net/%s/device",
	      ifname);

	file = fopen(phys_port_name, "rb");
	if (file != NULL) {
		char *tail_nl;

		line_size = getline(&port_name, &port_name_size, file);
		if (line_size < 0) {
			free(port_name);
			fclose(file);
			rte_errno = errno;
			return -rte_errno;
		} else if (line_size > 0) {
			/* Remove tailing newline character. */
			tail_nl = strchr(port_name, '\n');
			if (tail_nl)
				*tail_nl = '\0';
			mlx5_translate_port_name(port_name, &data);
		}
		free(port_name);
		fclose(file);
	}
	file = fopen(phys_switch_id, "rb");
	if (file == NULL) {
		rte_errno = errno;
		return -rte_errno;
	}
	port_switch_id_set =
		fscanf(file, "%" SCNx64 "%c", &data.switch_id, &c) == 2 &&
		c == '\n';
	fclose(file);
	dir = opendir(pci_device);
	if (dir != NULL) {
		closedir(dir);
		device_dir = true;
	}
	if (port_switch_id_set) {
		/* We have some E-Switch configuration. */
		mlx5_sysfs_check_switch_info(device_dir, &data);
	}
	*info = data;
	MLX5_ASSERT(!(data.master && data.representor));
	if (data.master && data.representor) {
		DRV_LOG(ERR, "ifindex %u device is recognized as master"
			     " and as representor", ifindex);
		rte_errno = ENODEV;
		return -rte_errno;
	}
	return 0;
}

/**
 * Get bond information associated with network interface.
 *
 * @param pf_ifindex
 *   Network interface index of bond slave interface
 * @param[out] ifindex
 *   Pointer to bond ifindex.
 * @param[out] ifname
 *   Pointer to bond ifname.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_sysfs_bond_info(unsigned int pf_ifindex, unsigned int *ifindex,
		     char *ifname)
{
	char name[IF_NAMESIZE];
	FILE *file;
	unsigned int index;
	int ret;

	if (!if_indextoname(pf_ifindex, name) || !strlen(name)) {
		rte_errno = errno;
		return -rte_errno;
	}
	MKSTR(bond_if, "/sys/class/net/%s/master/ifindex", name);
	/* read bond ifindex */
	file = fopen(bond_if, "rb");
	if (file == NULL) {
		rte_errno = errno;
		return -rte_errno;
	}
	ret = fscanf(file, "%u", &index);
	fclose(file);
	if (ret <= 0) {
		rte_errno = errno;
		return -rte_errno;
	}
	if (ifindex)
		*ifindex = index;

	/* read bond device name from symbol link */
	if (ifname) {
		if (!if_indextoname(index, ifname)) {
			rte_errno = errno;
			return -rte_errno;
		}
	}
	return 0;
}

/**
 * DPDK callback to retrieve plug-in module EEPROM information (type and size).
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param[out] modinfo
 *   Storage for plug-in module EEPROM information.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_get_module_info(struct rte_eth_dev *dev,
		     struct rte_eth_dev_module_info *modinfo)
{
	struct ethtool_modinfo info = {
		.cmd = ETHTOOL_GMODULEINFO,
	};
	struct ifreq ifr = (struct ifreq) {
		.ifr_data = (void *)&info,
	};
	int ret = 0;

	if (!dev) {
		DRV_LOG(WARNING, "missing argument, cannot get module info");
		rte_errno = EINVAL;
		return -rte_errno;
	}
	ret = mlx5_ifreq(dev, SIOCETHTOOL, &ifr);
	if (ret) {
		DRV_LOG(WARNING, "port %u ioctl(SIOCETHTOOL) failed: %s",
			dev->data->port_id, strerror(rte_errno));
		return ret;
	}
	modinfo->type = info.type;
	modinfo->eeprom_len = info.eeprom_len;
	return ret;
}

/**
 * DPDK callback to retrieve plug-in module EEPROM data.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param[out] info
 *   Storage for plug-in module EEPROM data.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int mlx5_get_module_eeprom(struct rte_eth_dev *dev,
			   struct rte_dev_eeprom_info *info)
{
	struct ethtool_eeprom *eeprom;
	struct ifreq ifr;
	int ret = 0;

	if (!dev) {
		DRV_LOG(WARNING, "missing argument, cannot get module eeprom");
		rte_errno = EINVAL;
		return -rte_errno;
	}
	eeprom = mlx5_malloc(MLX5_MEM_ZERO,
			     (sizeof(struct ethtool_eeprom) + info->length), 0,
			     SOCKET_ID_ANY);
	if (!eeprom) {
		DRV_LOG(WARNING, "port %u cannot allocate memory for "
			"eeprom data", dev->data->port_id);
		rte_errno = ENOMEM;
		return -rte_errno;
	}
	eeprom->cmd = ETHTOOL_GMODULEEEPROM;
	eeprom->offset = info->offset;
	eeprom->len = info->length;
	ifr = (struct ifreq) {
		.ifr_data = (void *)eeprom,
	};
	ret = mlx5_ifreq(dev, SIOCETHTOOL, &ifr);
	if (ret)
		DRV_LOG(WARNING, "port %u ioctl(SIOCETHTOOL) failed: %s",
			dev->data->port_id, strerror(rte_errno));
	else
		rte_memcpy(info->data, eeprom->data, info->length);
	mlx5_free(eeprom);
	return ret;
}

/**
 * Read device counters table.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param[in] pf
 *   PF index in case of bonding device, -1 otherwise
 * @param[out] stats
 *   Counters table output buffer.
 *
 * @return
 *   0 on success and stats is filled, negative errno value otherwise and
 *   rte_errno is set.
 */
static int
_mlx5_os_read_dev_counters(struct rte_eth_dev *dev, int pf, uint64_t *stats)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_xstats_ctrl *xstats_ctrl = &priv->xstats_ctrl;
	unsigned int i;
	struct ifreq ifr;
	unsigned int max_stats_n = RTE_MAX(xstats_ctrl->stats_n, xstats_ctrl->stats_n_2nd);
	unsigned int stats_sz = max_stats_n * sizeof(uint64_t);
	unsigned char et_stat_buf[sizeof(struct ethtool_stats) + stats_sz];
	struct ethtool_stats *et_stats = (struct ethtool_stats *)et_stat_buf;
	int ret;
	uint16_t i_idx, o_idx;
	uint32_t total_stats = xstats_n;

	et_stats->cmd = ETHTOOL_GSTATS;
	/* Pass the maximum value, the driver may ignore this. */
	et_stats->n_stats = max_stats_n;
	ifr.ifr_data = (caddr_t)et_stats;
	if (pf >= 0)
		ret = mlx5_ifreq_by_ifname(priv->sh->bond.ports[pf].ifname,
					   SIOCETHTOOL, &ifr);
	else
		ret = mlx5_ifreq(dev, SIOCETHTOOL, &ifr);
	if (ret) {
		DRV_LOG(WARNING,
			"port %u unable to read statistic values from device",
			dev->data->port_id);
		return ret;
	}
	if (pf <= 0) {
		for (i = 0; i != total_stats; i++) {
			i_idx = xstats_ctrl->dev_table_idx[i];
			o_idx = xstats_ctrl->xstats_o_idx[i];
			if (i_idx == UINT16_MAX || xstats_ctrl->info[o_idx].dev)
				continue;
			stats[o_idx] += (uint64_t)et_stats->data[i_idx];
		}
	} else {
		for (i = 0; i != total_stats; i++) {
			i_idx = xstats_ctrl->dev_table_idx_2nd[i];
			o_idx = xstats_ctrl->xstats_o_idx_2nd[i];
			if (i_idx == UINT16_MAX || xstats_ctrl->info[o_idx].dev)
				continue;
			stats[o_idx] += (uint64_t)et_stats->data[i_idx];
		}
	}
	return 0;
}

/*
 * Read device counters.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param bond_master
 *   Indicate if the device is a bond master.
 * @param stats
 *   Counters table output buffer.
 *
 * @return
 *   0 on success and stats is filled, negative errno value otherwise and
 *   rte_errno is set.
 */
int
mlx5_os_read_dev_counters(struct rte_eth_dev *dev, bool bond_master, uint64_t *stats)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_xstats_ctrl *xstats_ctrl = &priv->xstats_ctrl;
	int ret = 0, i;

	memset(stats, 0, sizeof(*stats) * xstats_ctrl->mlx5_stats_n);
	/* Read ifreq counters. */
	if (bond_master) {
		/* Sum xstats from bonding device member ports. */
		for (i = 0; i < priv->sh->bond.n_port; i++) {
			ret = _mlx5_os_read_dev_counters(dev, i, stats);
			if (ret)
				return ret;
		}
	} else {
		ret = _mlx5_os_read_dev_counters(dev, -1, stats);
		if (ret)
			return ret;
	}
	/*
	 * Read IB dev counters.
	 * The counters are unique per IB device but not per netdev IF.
	 * In bonding mode, getting the stats name only from 1 port is enough.
	 */
	for (i = xstats_ctrl->dev_cnt_start; i < xstats_ctrl->mlx5_stats_n; i++) {
		if (!xstats_ctrl->info[i].dev)
			continue;
		/* return last xstats counter if fail to read. */
		if (mlx5_os_read_dev_stat(priv, xstats_ctrl->info[i].ctr_name,
					  &stats[i]) == 0)
			xstats_ctrl->xstats[i] = stats[i];
		else
			stats[i] = xstats_ctrl->xstats[i];
	}
	return ret;
}

/*
 * Query the number of statistics provided by ETHTOOL.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param bond_master
 *   Indicate if the device is a bond master.
 * @param n_stats
 *   Pointer to number of stats to store.
 * @param n_stats_sec
 *   Pointer to number of stats to store for the 2nd port of the bond.
 *
 * @return
 *   0 on success, negative errno value otherwise and rte_errno is set.
 */
int
mlx5_os_get_stats_n(struct rte_eth_dev *dev, bool bond_master,
		    uint16_t *n_stats, uint16_t *n_stats_sec)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct ethtool_drvinfo drvinfo;
	struct ifreq ifr;
	int ret;

	drvinfo.cmd = ETHTOOL_GDRVINFO;
	ifr.ifr_data = (caddr_t)&drvinfo;
	/* Bonding PFs. */
	if (bond_master) {
		ret = mlx5_ifreq_by_ifname(priv->sh->bond.ports[0].ifname,
					   SIOCETHTOOL, &ifr);
		if (ret) {
			DRV_LOG(WARNING, "bonding port %u unable to query number of"
				" statistics for the 1st slave, %d", PORT_ID(priv), ret);
			return ret;
		}
		*n_stats = drvinfo.n_stats;
		ret = mlx5_ifreq_by_ifname(priv->sh->bond.ports[1].ifname,
					   SIOCETHTOOL, &ifr);
		if (ret) {
			DRV_LOG(WARNING, "bonding port %u unable to query number of"
				" statistics for the 2nd slave, %d", PORT_ID(priv), ret);
			return ret;
		}
		*n_stats_sec = drvinfo.n_stats;
	} else {
		ret = mlx5_ifreq(dev, SIOCETHTOOL, &ifr);
		if (ret) {
			DRV_LOG(WARNING, "port %u unable to query number of statistics",
				PORT_ID(priv));
			return ret;
		}
		*n_stats = drvinfo.n_stats;
	}
	return 0;
}

static const struct mlx5_counter_ctrl mlx5_counters_init[] = {
	{
		.dpdk_name = "rx_unicast_bytes",
		.ctr_name = "rx_vport_unicast_bytes",
	},
	{
		.dpdk_name = "rx_multicast_bytes",
		.ctr_name = "rx_vport_multicast_bytes",
	},
	{
		.dpdk_name = "rx_broadcast_bytes",
		.ctr_name = "rx_vport_broadcast_bytes",
	},
	{
		.dpdk_name = "rx_unicast_packets",
		.ctr_name = "rx_vport_unicast_packets",
	},
	{
		.dpdk_name = "rx_multicast_packets",
		.ctr_name = "rx_vport_multicast_packets",
	},
	{
		.dpdk_name = "rx_broadcast_packets",
		.ctr_name = "rx_vport_broadcast_packets",
	},
	{
		.dpdk_name = "tx_unicast_bytes",
		.ctr_name = "tx_vport_unicast_bytes",
	},
	{
		.dpdk_name = "tx_multicast_bytes",
		.ctr_name = "tx_vport_multicast_bytes",
	},
	{
		.dpdk_name = "tx_broadcast_bytes",
		.ctr_name = "tx_vport_broadcast_bytes",
	},
	{
		.dpdk_name = "tx_unicast_packets",
		.ctr_name = "tx_vport_unicast_packets",
	},
	{
		.dpdk_name = "tx_multicast_packets",
		.ctr_name = "tx_vport_multicast_packets",
	},
	{
		.dpdk_name = "tx_broadcast_packets",
		.ctr_name = "tx_vport_broadcast_packets",
	},
	{
		.dpdk_name = "rx_wqe_errors",
		.ctr_name = "rx_wqe_err",
	},
	{
		.dpdk_name = "rx_phy_crc_errors",
		.ctr_name = "rx_crc_errors_phy",
	},
	{
		.dpdk_name = "rx_phy_in_range_len_errors",
		.ctr_name = "rx_in_range_len_errors_phy",
	},
	{
		.dpdk_name = "rx_phy_symbol_errors",
		.ctr_name = "rx_symbol_err_phy",
	},
	{
		.dpdk_name = "tx_phy_errors",
		.ctr_name = "tx_errors_phy",
	},
	{
		.dpdk_name = "rx_out_of_buffer",
		.ctr_name = "out_of_buffer",
		.dev = 1,
	},
	{
		.dpdk_name = "hairpin_out_of_buffer",
		.ctr_name = "hairpin_out_of_buffer",
		.dev = 1,
	},
	{
		.dpdk_name = "dev_internal_queue_oob",
		.ctr_name = "dev_internal_queue_oob",
	},
	{
		.dpdk_name = "tx_phy_packets",
		.ctr_name = "tx_packets_phy",
	},
	{
		.dpdk_name = "rx_phy_packets",
		.ctr_name = "rx_packets_phy",
	},
	{
		.dpdk_name = "tx_phy_discard_packets",
		.ctr_name = "tx_discards_phy",
	},
	{
		.dpdk_name = "rx_phy_discard_packets",
		.ctr_name = "rx_discards_phy",
	},
	{
		.dpdk_name = "rx_prio0_buf_discard_packets",
		.ctr_name = "rx_prio0_buf_discard",
	},
	{
		.dpdk_name = "rx_prio1_buf_discard_packets",
		.ctr_name = "rx_prio1_buf_discard",
	},
	{
		.dpdk_name = "rx_prio2_buf_discard_packets",
		.ctr_name = "rx_prio2_buf_discard",
	},
	{
		.dpdk_name = "rx_prio3_buf_discard_packets",
		.ctr_name = "rx_prio3_buf_discard",
	},
	{
		.dpdk_name = "rx_prio4_buf_discard_packets",
		.ctr_name = "rx_prio4_buf_discard",
	},
	{
		.dpdk_name = "rx_prio5_buf_discard_packets",
		.ctr_name = "rx_prio5_buf_discard",
	},
	{
		.dpdk_name = "rx_prio6_buf_discard_packets",
		.ctr_name = "rx_prio6_buf_discard",
	},
	{
		.dpdk_name = "rx_prio7_buf_discard_packets",
		.ctr_name = "rx_prio7_buf_discard",
	},
	{
		.dpdk_name = "rx_prio0_cong_discard_packets",
		.ctr_name = "rx_prio0_cong_discard",
	},
	{
		.dpdk_name = "rx_prio1_cong_discard_packets",
		.ctr_name = "rx_prio1_cong_discard",
	},
	{
		.dpdk_name = "rx_prio2_cong_discard_packets",
		.ctr_name = "rx_prio2_cong_discard",
	},
	{
		.dpdk_name = "rx_prio3_cong_discard_packets",
		.ctr_name = "rx_prio3_cong_discard",
	},
	{
		.dpdk_name = "rx_prio4_cong_discard_packets",
		.ctr_name = "rx_prio4_cong_discard",
	},
	{
		.dpdk_name = "rx_prio5_cong_discard_packets",
		.ctr_name = "rx_prio5_cong_discard",
	},
	{
		.dpdk_name = "rx_prio6_cong_discard_packets",
		.ctr_name = "rx_prio6_cong_discard",
	},
	{
		.dpdk_name = "rx_prio7_cong_discard_packets",
		.ctr_name = "rx_prio7_cong_discard",
	},
	{
		.dpdk_name = "tx_phy_bytes",
		.ctr_name = "tx_bytes_phy",
	},
	{
		.dpdk_name = "rx_phy_bytes",
		.ctr_name = "rx_bytes_phy",
	},
	/* Representor only */
	{
		.dpdk_name = "rx_vport_packets",
		.ctr_name = "vport_rx_packets",
	},
	{
		.dpdk_name = "rx_vport_bytes",
		.ctr_name = "vport_rx_bytes",
	},
	{
		.dpdk_name = "tx_vport_packets",
		.ctr_name = "vport_tx_packets",
	},
	{
		.dpdk_name = "tx_vport_bytes",
		.ctr_name = "vport_tx_bytes",
	},
	/**
	 * Device counters: These counters are for the
	 * entire PCI device (NIC). These counters are
	 * not counting on a per port/queue basis.
	 */
	{
		.dpdk_name = "rx_pci_signal_integrity",
		.ctr_name = "rx_pci_signal_integrity",
	},
	{
		.dpdk_name = "tx_pci_signal_integrity",
		.ctr_name = "tx_pci_signal_integrity",
	},
	{
		.dpdk_name = "outbound_pci_buffer_overflow",
		.ctr_name = "outbound_pci_buffer_overflow",
	},
	{
		.dpdk_name = "outbound_pci_stalled_rd",
		.ctr_name = "outbound_pci_stalled_rd",
	},
	{
		.dpdk_name = "outbound_pci_stalled_wr",
		.ctr_name = "outbound_pci_stalled_wr",
	},
	{
		.dpdk_name = "outbound_pci_stalled_rd_events",
		.ctr_name = "outbound_pci_stalled_rd_events",
	},
	{
		.dpdk_name = "outbound_pci_stalled_wr_events",
		.ctr_name = "outbound_pci_stalled_wr_events",
	},
	{
		.dpdk_name = "dev_out_of_buffer",
		.ctr_name = "dev_out_of_buffer",
	},
};

const unsigned int xstats_n = RTE_DIM(mlx5_counters_init);

static int
mlx5_os_get_stats_strings(struct rte_eth_dev *dev, bool bond_master,
			  struct ethtool_gstrings *strings,
			  uint32_t stats_n, uint32_t stats_n_2nd)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_xstats_ctrl *xstats_ctrl = &priv->xstats_ctrl;
	struct ifreq ifr;
	int ret;
	uint32_t i, j, idx;

	/* Ensure no out of bounds access before. */
	MLX5_ASSERT(xstats_n <= MLX5_MAX_XSTATS);
	strings->cmd = ETHTOOL_GSTRINGS;
	strings->string_set = ETH_SS_STATS;
	strings->len = stats_n;
	ifr.ifr_data = (caddr_t)strings;
	if (bond_master)
		ret = mlx5_ifreq_by_ifname(priv->sh->bond.ports[0].ifname,
					   SIOCETHTOOL, &ifr);
	else
		ret = mlx5_ifreq(dev, SIOCETHTOOL, &ifr);
	if (ret) {
		DRV_LOG(WARNING, "port %u unable to get statistic names with %d",
			PORT_ID(priv), ret);
		return ret;
	}
	/* Reorganize the orders to reduce the iterations. */
	for (j = 0; j < xstats_n; j++) {
		xstats_ctrl->dev_table_idx[j] = UINT16_MAX;
		for (i = 0; i < stats_n; i++) {
			const char *curr_string =
				(const char *)&strings->data[i * ETH_GSTRING_LEN];

			if (!strcmp(mlx5_counters_init[j].ctr_name, curr_string)) {
				idx = xstats_ctrl->mlx5_stats_n++;
				xstats_ctrl->dev_table_idx[j] = i;
				xstats_ctrl->xstats_o_idx[j] = idx;
				xstats_ctrl->info[idx] = mlx5_counters_init[j];
			}
		}
	}
	if (!bond_master) {
		/* Add dev counters, unique per IB device. */
		xstats_ctrl->dev_cnt_start = xstats_ctrl->mlx5_stats_n;
		for (j = 0; j != xstats_n; j++) {
			if (mlx5_counters_init[j].dev) {
				idx = xstats_ctrl->mlx5_stats_n++;
				xstats_ctrl->info[idx] = mlx5_counters_init[j];
				xstats_ctrl->hw_stats[idx] = 0;
			}
		}
		return 0;
	}

	strings->len = stats_n_2nd;
	ret = mlx5_ifreq_by_ifname(priv->sh->bond.ports[1].ifname,
				   SIOCETHTOOL, &ifr);
	if (ret) {
		DRV_LOG(WARNING, "port %u unable to get statistic names for 2nd slave with %d",
			PORT_ID(priv), ret);
		return ret;
	}
	/* The 2nd slave port may have a different strings set, based on the configuration. */
	for (j = 0; j != xstats_n; j++) {
		xstats_ctrl->dev_table_idx_2nd[j] = UINT16_MAX;
		for (i = 0; i != stats_n_2nd; i++) {
			const char *curr_string =
				(const char *)&strings->data[i * ETH_GSTRING_LEN];

			if (!strcmp(mlx5_counters_init[j].ctr_name, curr_string)) {
				xstats_ctrl->dev_table_idx_2nd[j] = i;
				if (xstats_ctrl->dev_table_idx[j] != UINT16_MAX) {
					/* Already mapped in the 1st slave port. */
					idx = xstats_ctrl->xstats_o_idx[j];
					xstats_ctrl->xstats_o_idx_2nd[j] = idx;
				} else {
					/* Append the new items to the end of the map. */
					idx = xstats_ctrl->mlx5_stats_n++;
					xstats_ctrl->xstats_o_idx_2nd[j] = idx;
					xstats_ctrl->info[idx] = mlx5_counters_init[j];
				}
			}
		}
	}
	/* Dev counters are always at the last now. */
	xstats_ctrl->dev_cnt_start = xstats_ctrl->mlx5_stats_n;
	for (j = 0; j != xstats_n; j++) {
		if (mlx5_counters_init[j].dev) {
			idx = xstats_ctrl->mlx5_stats_n++;
			xstats_ctrl->info[idx] = mlx5_counters_init[j];
			xstats_ctrl->hw_stats[idx] = 0;
		}
	}
	return 0;
}

/**
 * Init the structures to read device counters.
 *
 * @param dev
 *   Pointer to Ethernet device.
 */
void
mlx5_os_stats_init(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_xstats_ctrl *xstats_ctrl = &priv->xstats_ctrl;
	struct mlx5_stats_ctrl *stats_ctrl = &priv->stats_ctrl;
	struct ethtool_gstrings *strings = NULL;
	uint16_t dev_stats_n = 0;
	uint16_t dev_stats_n_2nd = 0;
	unsigned int max_stats_n;
	unsigned int str_sz;
	int ret;
	bool bond_master = (priv->master && priv->pf_bond >= 0);

	/* So that it won't aggregate for each init. */
	xstats_ctrl->mlx5_stats_n = 0;
	ret = mlx5_os_get_stats_n(dev, bond_master, &dev_stats_n, &dev_stats_n_2nd);
	if (ret < 0) {
		DRV_LOG(WARNING, "port %u no extended statistics available",
			dev->data->port_id);
		return;
	}
	max_stats_n = RTE_MAX(dev_stats_n, dev_stats_n_2nd);
	/* Allocate memory to grab stat names and values. */
	str_sz = max_stats_n * ETH_GSTRING_LEN;
	strings = (struct ethtool_gstrings *)
		  mlx5_malloc(0, str_sz + sizeof(struct ethtool_gstrings), 0,
			      SOCKET_ID_ANY);
	if (!strings) {
		DRV_LOG(WARNING, "port %u unable to allocate memory for xstats",
			dev->data->port_id);
		return;
	}
	ret = mlx5_os_get_stats_strings(dev, bond_master, strings,
					dev_stats_n, dev_stats_n_2nd);
	if (ret < 0) {
		DRV_LOG(WARNING, "port %u failed to get the stats strings",
			dev->data->port_id);
		goto free;
	}
	xstats_ctrl->stats_n = dev_stats_n;
	xstats_ctrl->stats_n_2nd = dev_stats_n_2nd;
	/* Copy to base at first time. */
	ret = mlx5_os_read_dev_counters(dev, bond_master, xstats_ctrl->base);
	if (ret)
		DRV_LOG(ERR, "port %u cannot read device counters: %s",
			dev->data->port_id, strerror(rte_errno));
	mlx5_os_read_dev_stat(priv, "out_of_buffer", &stats_ctrl->imissed_base);
	stats_ctrl->imissed = 0;
free:
	mlx5_free(strings);
}

/**
 * Get MAC address by querying netdevice.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[out] mac
 *   MAC address output buffer.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_get_mac(struct rte_eth_dev *dev, uint8_t (*mac)[RTE_ETHER_ADDR_LEN])
{
	struct ifreq request;
	int ret;

	ret = mlx5_ifreq(dev, SIOCGIFHWADDR, &request);
	if (ret)
		return ret;
	memcpy(mac, request.ifr_hwaddr.sa_data, RTE_ETHER_ADDR_LEN);
	return 0;
}

/*
 * Query dropless_rq private flag value provided by ETHTOOL.
 *
 * @param dev
 *   Pointer to Ethernet device.
 *
 * @return
 *   - 0 on success, flag is not set.
 *   - 1 on success, flag is set.
 *   - negative errno value otherwise and rte_errno is set.
 */
int mlx5_get_flag_dropless_rq(struct rte_eth_dev *dev)
{
	struct ethtool_sset_info *sset_info = NULL;
	struct ethtool_drvinfo drvinfo;
	struct ifreq ifr;
	struct ethtool_gstrings *strings = NULL;
	struct ethtool_value flags;
	const int32_t flag_len = sizeof(flags.data) * CHAR_BIT;
	int32_t str_sz;
	int32_t len;
	int32_t i;
	int ret;

	sset_info = mlx5_malloc(0, sizeof(struct ethtool_sset_info) +
			sizeof(uint32_t), 0, SOCKET_ID_ANY);
	if (sset_info == NULL) {
		rte_errno = ENOMEM;
		return -rte_errno;
	}
	sset_info->cmd = ETHTOOL_GSSET_INFO;
	sset_info->reserved = 0;
	sset_info->sset_mask = 1ULL << ETH_SS_PRIV_FLAGS;
	ifr.ifr_data = (caddr_t)&sset_info;
	ret = mlx5_ifreq(dev, SIOCETHTOOL, &ifr);
	if (!ret) {
		const uint32_t *sset_lengths = sset_info->data;

		len = sset_info->sset_mask ? sset_lengths[0] : 0;
	} else if (ret == -EOPNOTSUPP) {
		drvinfo.cmd = ETHTOOL_GDRVINFO;
		ifr.ifr_data = (caddr_t)&drvinfo;
		ret = mlx5_ifreq(dev, SIOCETHTOOL, &ifr);
		if (ret) {
			DRV_LOG(WARNING, "port %u cannot get the driver info",
				dev->data->port_id);
			goto exit;
		}
		len = *(uint32_t *)((char *)&drvinfo +
			offsetof(struct ethtool_drvinfo, n_priv_flags));
	} else {
		DRV_LOG(WARNING, "port %u cannot get the sset info",
			dev->data->port_id);
		goto exit;
	}
	if (!len) {
		DRV_LOG(WARNING, "port %u does not have private flag",
			dev->data->port_id);
		rte_errno = EOPNOTSUPP;
		ret = -rte_errno;
		goto exit;
	} else if (len > flag_len) {
		DRV_LOG(WARNING, "port %u maximal private flags number is %d",
			dev->data->port_id, flag_len);
		len = flag_len;
	}
	str_sz = ETH_GSTRING_LEN * len;
	strings = (struct ethtool_gstrings *)
		  mlx5_malloc(0, str_sz + sizeof(struct ethtool_gstrings), 0,
			      SOCKET_ID_ANY);
	if (!strings) {
		DRV_LOG(WARNING, "port %u unable to allocate memory for"
			" private flags", dev->data->port_id);
		rte_errno = ENOMEM;
		ret = -rte_errno;
		goto exit;
	}
	strings->cmd = ETHTOOL_GSTRINGS;
	strings->string_set = ETH_SS_PRIV_FLAGS;
	strings->len = len;
	ifr.ifr_data = (caddr_t)strings;
	ret = mlx5_ifreq(dev, SIOCETHTOOL, &ifr);
	if (ret) {
		DRV_LOG(WARNING, "port %u unable to get private flags strings",
			dev->data->port_id);
		goto exit;
	}
	for (i = 0; i < len; i++) {
		strings->data[(i + 1) * ETH_GSTRING_LEN - 1] = 0;
		if (!strcmp((const char *)strings->data + i * ETH_GSTRING_LEN,
			     "dropless_rq"))
			break;
	}
	if (i == len) {
		DRV_LOG(WARNING, "port %u does not support dropless_rq",
			dev->data->port_id);
		rte_errno = EOPNOTSUPP;
		ret = -rte_errno;
		goto exit;
	}
	flags.cmd = ETHTOOL_GPFLAGS;
	ifr.ifr_data = (caddr_t)&flags;
	ret = mlx5_ifreq(dev, SIOCETHTOOL, &ifr);
	if (ret) {
		DRV_LOG(WARNING, "port %u unable to get private flags status",
			dev->data->port_id);
		goto exit;
	}
	ret = !!(flags.data & (1U << i));
exit:
	mlx5_free(strings);
	mlx5_free(sset_info);
	return ret;
}

/**
 * Unmaps HCA PCI BAR from the current process address space.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 */
void mlx5_txpp_unmap_hca_bar(struct rte_eth_dev *dev)
{
	struct mlx5_proc_priv *ppriv = dev->process_private;

	if (ppriv && ppriv->hca_bar) {
		rte_mem_unmap(ppriv->hca_bar, MLX5_ST_SZ_BYTES(initial_seg));
		ppriv->hca_bar = NULL;
	}
}

/**
 * Maps HCA PCI BAR to the current process address space.
 * Stores pointer in the process private structure allowing
 * to read internal and real time counter directly from the HW.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success and not NULL pointer to mapped area in process structure.
 *   negative otherwise and NULL pointer
 */
int mlx5_txpp_map_hca_bar(struct rte_eth_dev *dev)
{
	struct mlx5_proc_priv *ppriv = dev->process_private;
	char pci_addr[PCI_PRI_STR_SIZE] = { 0 };
	void *base, *expected = NULL;
	int fd, ret;

	if (!ppriv) {
		rte_errno = ENOMEM;
		return -rte_errno;
	}
	if (ppriv->hca_bar)
		return 0;
	ret = mlx5_dev_to_pci_str(dev->device, pci_addr, sizeof(pci_addr));
	if (ret < 0)
		return -rte_errno;
	/* Open PCI device resource 0 - HCA initialize segment */
	MKSTR(name, "/sys/bus/pci/devices/%s/resource0", pci_addr);
	fd = open(name, O_RDWR | O_SYNC);
	if (fd == -1) {
		rte_errno = ENOTSUP;
		return -ENOTSUP;
	}
	base = rte_mem_map(NULL, MLX5_ST_SZ_BYTES(initial_seg),
			   RTE_PROT_READ, RTE_MAP_SHARED, fd, 0);
	close(fd);
	if (!base) {
		rte_errno = ENOTSUP;
		return -ENOTSUP;
	}
	/* Check there is no concurrent mapping in other thread. */
	if (!rte_atomic_compare_exchange_strong_explicit(&ppriv->hca_bar, &expected,
					 base,
					 rte_memory_order_relaxed, rte_memory_order_relaxed))
		rte_mem_unmap(base, MLX5_ST_SZ_BYTES(initial_seg));
	return 0;
}
