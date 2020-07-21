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

#include <rte_atomic.h>
#include <rte_ethdev_driver.h>
#include <rte_bus_pci.h>
#include <rte_mbuf.h>
#include <rte_common.h>
#include <rte_interrupts.h>
#include <rte_malloc.h>
#include <rte_string_fns.h>
#include <rte_rwlock.h>
#include <rte_cycles.h>

#include <mlx5_glue.h>
#include <mlx5_devx_cmds.h>
#include <mlx5_common.h>
#include <mlx5_malloc.h>

#include "mlx5.h"
#include "mlx5_rxtx.h"
#include "mlx5_utils.h"

/* Supported speed values found in /usr/include/linux/ethtool.h */
#ifndef HAVE_SUPPORTED_40000baseKR4_Full
#define SUPPORTED_40000baseKR4_Full (1 << 23)
#endif
#ifndef HAVE_SUPPORTED_40000baseCR4_Full
#define SUPPORTED_40000baseCR4_Full (1 << 24)
#endif
#ifndef HAVE_SUPPORTED_40000baseSR4_Full
#define SUPPORTED_40000baseSR4_Full (1 << 25)
#endif
#ifndef HAVE_SUPPORTED_40000baseLR4_Full
#define SUPPORTED_40000baseLR4_Full (1 << 26)
#endif
#ifndef HAVE_SUPPORTED_56000baseKR4_Full
#define SUPPORTED_56000baseKR4_Full (1 << 27)
#endif
#ifndef HAVE_SUPPORTED_56000baseCR4_Full
#define SUPPORTED_56000baseCR4_Full (1 << 28)
#endif
#ifndef HAVE_SUPPORTED_56000baseSR4_Full
#define SUPPORTED_56000baseSR4_Full (1 << 29)
#endif
#ifndef HAVE_SUPPORTED_56000baseLR4_Full
#define SUPPORTED_56000baseLR4_Full (1 << 30)
#endif

/* Add defines in case the running kernel is not the same as user headers. */
#ifndef ETHTOOL_GLINKSETTINGS
struct ethtool_link_settings {
	uint32_t cmd;
	uint32_t speed;
	uint8_t duplex;
	uint8_t port;
	uint8_t phy_address;
	uint8_t autoneg;
	uint8_t mdio_support;
	uint8_t eth_to_mdix;
	uint8_t eth_tp_mdix_ctrl;
	int8_t link_mode_masks_nwords;
	uint32_t reserved[8];
	uint32_t link_mode_masks[];
};

/* The kernel values can be found in /include/uapi/linux/ethtool.h */
#define ETHTOOL_GLINKSETTINGS 0x0000004c
#define ETHTOOL_LINK_MODE_1000baseT_Full_BIT 5
#define ETHTOOL_LINK_MODE_Autoneg_BIT 6
#define ETHTOOL_LINK_MODE_1000baseKX_Full_BIT 17
#define ETHTOOL_LINK_MODE_10000baseKX4_Full_BIT 18
#define ETHTOOL_LINK_MODE_10000baseKR_Full_BIT 19
#define ETHTOOL_LINK_MODE_10000baseR_FEC_BIT 20
#define ETHTOOL_LINK_MODE_20000baseMLD2_Full_BIT 21
#define ETHTOOL_LINK_MODE_20000baseKR2_Full_BIT 22
#define ETHTOOL_LINK_MODE_40000baseKR4_Full_BIT 23
#define ETHTOOL_LINK_MODE_40000baseCR4_Full_BIT 24
#define ETHTOOL_LINK_MODE_40000baseSR4_Full_BIT 25
#define ETHTOOL_LINK_MODE_40000baseLR4_Full_BIT 26
#define ETHTOOL_LINK_MODE_56000baseKR4_Full_BIT 27
#define ETHTOOL_LINK_MODE_56000baseCR4_Full_BIT 28
#define ETHTOOL_LINK_MODE_56000baseSR4_Full_BIT 29
#define ETHTOOL_LINK_MODE_56000baseLR4_Full_BIT 30
#endif
#ifndef HAVE_ETHTOOL_LINK_MODE_25G
#define ETHTOOL_LINK_MODE_25000baseCR_Full_BIT 31
#define ETHTOOL_LINK_MODE_25000baseKR_Full_BIT 32
#define ETHTOOL_LINK_MODE_25000baseSR_Full_BIT 33
#endif
#ifndef HAVE_ETHTOOL_LINK_MODE_50G
#define ETHTOOL_LINK_MODE_50000baseCR2_Full_BIT 34
#define ETHTOOL_LINK_MODE_50000baseKR2_Full_BIT 35
#endif
#ifndef HAVE_ETHTOOL_LINK_MODE_100G
#define ETHTOOL_LINK_MODE_100000baseKR4_Full_BIT 36
#define ETHTOOL_LINK_MODE_100000baseSR4_Full_BIT 37
#define ETHTOOL_LINK_MODE_100000baseCR4_Full_BIT 38
#define ETHTOOL_LINK_MODE_100000baseLR4_ER4_Full_BIT 39
#endif
#ifndef HAVE_ETHTOOL_LINK_MODE_200G
#define ETHTOOL_LINK_MODE_200000baseKR4_Full_BIT 62
#define ETHTOOL_LINK_MODE_200000baseSR4_Full_BIT 63
#define ETHTOOL_LINK_MODE_200000baseLR4_ER4_FR4_Full_BIT 0 /* 64 - 64 */
#define ETHTOOL_LINK_MODE_200000baseDR4_Full_BIT 1 /* 65 - 64 */
#define ETHTOOL_LINK_MODE_200000baseCR4_Full_BIT 2 /* 66 - 64 */
#endif


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
mlx5_get_ifname(const struct rte_eth_dev *dev, char (*ifname)[IF_NAMESIZE])
{
	struct mlx5_priv *priv = dev->data->dev_private;
	unsigned int ifindex;

	MLX5_ASSERT(priv);
	MLX5_ASSERT(priv->sh);
	ifindex = mlx5_ifindex(dev);
	if (!ifindex) {
		if (!priv->representor)
			return mlx5_get_ifname_sysfs(priv->sh->ibdev_path,
						     *ifname);
		rte_errno = ENXIO;
		return -rte_errno;
	}
	if (if_indextoname(ifindex, &(*ifname)[0]))
		return 0;
	rte_errno = errno;
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
	int sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
	int ret = 0;

	if (sock == -1) {
		rte_errno = errno;
		return -rte_errno;
	}
	ret = mlx5_get_ifname(dev, &ifr->ifr_name);
	if (ret)
		goto error;
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
	struct ibv_context *ctx = priv->sh->ctx;
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
	MLX5_ETH_FOREACH_DEV(port_id, priv->pci_dev) {
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
		dev_link.link_speed = ETH_SPEED_NUM_NONE;
	else
		dev_link.link_speed = link_speed;
	priv->link_speed_capa = 0;
	if (edata.supported & SUPPORTED_Autoneg)
		priv->link_speed_capa |= ETH_LINK_SPEED_AUTONEG;
	if (edata.supported & (SUPPORTED_1000baseT_Full |
			       SUPPORTED_1000baseKX_Full))
		priv->link_speed_capa |= ETH_LINK_SPEED_1G;
	if (edata.supported & SUPPORTED_10000baseKR_Full)
		priv->link_speed_capa |= ETH_LINK_SPEED_10G;
	if (edata.supported & (SUPPORTED_40000baseKR4_Full |
			       SUPPORTED_40000baseCR4_Full |
			       SUPPORTED_40000baseSR4_Full |
			       SUPPORTED_40000baseLR4_Full))
		priv->link_speed_capa |= ETH_LINK_SPEED_40G;
	dev_link.link_duplex = ((edata.duplex == DUPLEX_HALF) ?
				ETH_LINK_HALF_DUPLEX : ETH_LINK_FULL_DUPLEX);
	dev_link.link_autoneg = !(dev->data->dev_conf.link_speeds &
			ETH_LINK_SPEED_FIXED);
	if (((dev_link.link_speed && !dev_link.link_status) ||
	     (!dev_link.link_speed && dev_link.link_status))) {
		rte_errno = EAGAIN;
		return -rte_errno;
	}
	*link = dev_link;
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
	uint64_t sc;
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
	dev_link.link_speed = (ecmd->speed == UINT32_MAX) ? ETH_SPEED_NUM_NONE :
							    ecmd->speed;
	sc = ecmd->link_mode_masks[0] |
		((uint64_t)ecmd->link_mode_masks[1] << 32);
	priv->link_speed_capa = 0;
	if (sc & MLX5_BITSHIFT(ETHTOOL_LINK_MODE_Autoneg_BIT))
		priv->link_speed_capa |= ETH_LINK_SPEED_AUTONEG;
	if (sc & (MLX5_BITSHIFT(ETHTOOL_LINK_MODE_1000baseT_Full_BIT) |
		  MLX5_BITSHIFT(ETHTOOL_LINK_MODE_1000baseKX_Full_BIT)))
		priv->link_speed_capa |= ETH_LINK_SPEED_1G;
	if (sc & (MLX5_BITSHIFT(ETHTOOL_LINK_MODE_10000baseKX4_Full_BIT) |
		  MLX5_BITSHIFT(ETHTOOL_LINK_MODE_10000baseKR_Full_BIT) |
		  MLX5_BITSHIFT(ETHTOOL_LINK_MODE_10000baseR_FEC_BIT)))
		priv->link_speed_capa |= ETH_LINK_SPEED_10G;
	if (sc & (MLX5_BITSHIFT(ETHTOOL_LINK_MODE_20000baseMLD2_Full_BIT) |
		  MLX5_BITSHIFT(ETHTOOL_LINK_MODE_20000baseKR2_Full_BIT)))
		priv->link_speed_capa |= ETH_LINK_SPEED_20G;
	if (sc & (MLX5_BITSHIFT(ETHTOOL_LINK_MODE_40000baseKR4_Full_BIT) |
		  MLX5_BITSHIFT(ETHTOOL_LINK_MODE_40000baseCR4_Full_BIT) |
		  MLX5_BITSHIFT(ETHTOOL_LINK_MODE_40000baseSR4_Full_BIT) |
		  MLX5_BITSHIFT(ETHTOOL_LINK_MODE_40000baseLR4_Full_BIT)))
		priv->link_speed_capa |= ETH_LINK_SPEED_40G;
	if (sc & (MLX5_BITSHIFT(ETHTOOL_LINK_MODE_56000baseKR4_Full_BIT) |
		  MLX5_BITSHIFT(ETHTOOL_LINK_MODE_56000baseCR4_Full_BIT) |
		  MLX5_BITSHIFT(ETHTOOL_LINK_MODE_56000baseSR4_Full_BIT) |
		  MLX5_BITSHIFT(ETHTOOL_LINK_MODE_56000baseLR4_Full_BIT)))
		priv->link_speed_capa |= ETH_LINK_SPEED_56G;
	if (sc & (MLX5_BITSHIFT(ETHTOOL_LINK_MODE_25000baseCR_Full_BIT) |
		  MLX5_BITSHIFT(ETHTOOL_LINK_MODE_25000baseKR_Full_BIT) |
		  MLX5_BITSHIFT(ETHTOOL_LINK_MODE_25000baseSR_Full_BIT)))
		priv->link_speed_capa |= ETH_LINK_SPEED_25G;
	if (sc & (MLX5_BITSHIFT(ETHTOOL_LINK_MODE_50000baseCR2_Full_BIT) |
		  MLX5_BITSHIFT(ETHTOOL_LINK_MODE_50000baseKR2_Full_BIT)))
		priv->link_speed_capa |= ETH_LINK_SPEED_50G;
	if (sc & (MLX5_BITSHIFT(ETHTOOL_LINK_MODE_100000baseKR4_Full_BIT) |
		  MLX5_BITSHIFT(ETHTOOL_LINK_MODE_100000baseSR4_Full_BIT) |
		  MLX5_BITSHIFT(ETHTOOL_LINK_MODE_100000baseCR4_Full_BIT) |
		  MLX5_BITSHIFT(ETHTOOL_LINK_MODE_100000baseLR4_ER4_Full_BIT)))
		priv->link_speed_capa |= ETH_LINK_SPEED_100G;
	if (sc & (MLX5_BITSHIFT(ETHTOOL_LINK_MODE_200000baseKR4_Full_BIT) |
		  MLX5_BITSHIFT(ETHTOOL_LINK_MODE_200000baseSR4_Full_BIT)))
		priv->link_speed_capa |= ETH_LINK_SPEED_200G;

	sc = ecmd->link_mode_masks[2] |
		((uint64_t)ecmd->link_mode_masks[3] << 32);
	if (sc & (MLX5_BITSHIFT(ETHTOOL_LINK_MODE_200000baseCR4_Full_BIT) |
		  MLX5_BITSHIFT
		       (ETHTOOL_LINK_MODE_200000baseLR4_ER4_FR4_Full_BIT) |
		  MLX5_BITSHIFT(ETHTOOL_LINK_MODE_200000baseDR4_Full_BIT)))
		priv->link_speed_capa |= ETH_LINK_SPEED_200G;
	dev_link.link_duplex = ((ecmd->duplex == DUPLEX_HALF) ?
				ETH_LINK_HALF_DUPLEX : ETH_LINK_FULL_DUPLEX);
	dev_link.link_autoneg = !(dev->data->dev_conf.link_speeds &
				  ETH_LINK_SPEED_FIXED);
	if (((dev_link.link_speed && !dev_link.link_status) ||
	     (!dev_link.link_speed && dev_link.link_status))) {
		rte_errno = EAGAIN;
		return -rte_errno;
	}
	*link = dev_link;
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
	struct rte_eth_link dev_link;
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
		DRV_LOG(WARNING,
			"port %u ioctl(SIOCETHTOOL, ETHTOOL_GPAUSEPARAM) failed:"
			" %s",
			dev->data->port_id, strerror(rte_errno));
		return ret;
	}
	fc_conf->autoneg = ethpause.autoneg;
	if (ethpause.rx_pause && ethpause.tx_pause)
		fc_conf->mode = RTE_FC_FULL;
	else if (ethpause.rx_pause)
		fc_conf->mode = RTE_FC_RX_PAUSE;
	else if (ethpause.tx_pause)
		fc_conf->mode = RTE_FC_TX_PAUSE;
	else
		fc_conf->mode = RTE_FC_NONE;
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
	if (((fc_conf->mode & RTE_FC_FULL) == RTE_FC_FULL) ||
	    (fc_conf->mode & RTE_FC_RX_PAUSE))
		ethpause.rx_pause = 1;
	else
		ethpause.rx_pause = 0;

	if (((fc_conf->mode & RTE_FC_FULL) == RTE_FC_FULL) ||
	    (fc_conf->mode & RTE_FC_TX_PAUSE))
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

		if (sh->port[i].ih_port_id >= RTE_MAX_ETHPORTS) {
			/*
			 * Or not existing port either no
			 * handler installed for this port.
			 */
			continue;
		}
		dev = &rte_eth_devices[sh->port[i].ih_port_id];
		MLX5_ASSERT(dev);
		if (dev->data->dev_conf.intr_conf.rmv)
			_rte_eth_dev_callback_process
				(dev, RTE_ETH_EVENT_INTR_RMV, NULL);
	}
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

		if (mlx5_glue->get_async_event(sh->ctx, &event))
			break;
		/* Retrieve and check IB port index. */
		tmp = (uint32_t)event.element.port_num;
		if (!tmp && event.event_type == IBV_EVENT_DEVICE_FATAL) {
			/*
			 * The DEVICE_FATAL event is called once for
			 * entire device without port specifying.
			 * We should notify all existing ports.
			 */
			mlx5_glue->ack_async_event(&event);
			mlx5_dev_interrupt_device_fatal(sh);
			continue;
		}
		MLX5_ASSERT(tmp && (tmp <= sh->max_port));
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
		if ((event.event_type == IBV_EVENT_PORT_ACTIVE ||
		     event.event_type == IBV_EVENT_PORT_ERR) &&
			dev->data->dev_conf.intr_conf.lsc) {
			mlx5_glue->ack_async_event(&event);
			if (mlx5_link_update(dev, 0) == -EAGAIN) {
				usleep(0);
				continue;
			}
			_rte_eth_dev_callback_process
				(dev, RTE_ETH_EVENT_INTR_LSC, NULL);
			continue;
		}
		DRV_LOG(DEBUG,
			"port %u cannot handle an unknown event (type %d)",
			dev->data->port_id, event.event_type);
		mlx5_glue->ack_async_event(&event);
	}
}

/*
 * Unregister callback handler safely. The handler may be active
 * while we are trying to unregister it, in this case code -EAGAIN
 * is returned by rte_intr_callback_unregister(). This routine checks
 * the return code and tries to unregister handler again.
 *
 * @param handle
 *   interrupt handle
 * @param cb_fn
 *   pointer to callback routine
 * @cb_arg
 *   opaque callback parameter
 */
void
mlx5_intr_callback_unregister(const struct rte_intr_handle *handle,
			      rte_intr_callback_fn cb_fn, void *cb_arg)
{
	/*
	 * Try to reduce timeout management overhead by not calling
	 * the timer related routines on the first iteration. If the
	 * unregistering succeeds on first call there will be no
	 * timer calls at all.
	 */
	uint64_t twait = 0;
	uint64_t start = 0;

	do {
		int ret;

		ret = rte_intr_callback_unregister(handle, cb_fn, cb_arg);
		if (ret >= 0)
			return;
		if (ret != -EAGAIN) {
			DRV_LOG(INFO, "failed to unregister interrupt"
				      " handler (error: %d)", ret);
			MLX5_ASSERT(false);
			return;
		}
		if (twait) {
			struct timespec onems;

			/* Wait one millisecond and try again. */
			onems.tv_sec = 0;
			onems.tv_nsec = NS_PER_S / MS_PER_S;
			nanosleep(&onems, 0);
			/* Check whether one second elapsed. */
			if ((rte_get_timer_cycles() - start) <= twait)
				continue;
		} else {
			/*
			 * We get the amount of timer ticks for one second.
			 * If this amount elapsed it means we spent one
			 * second in waiting. This branch is executed once
			 * on first iteration.
			 */
			twait = rte_get_timer_hz();
			MLX5_ASSERT(twait);
		}
		/*
		 * Timeout elapsed, show message (once a second) and retry.
		 * We have no other acceptable option here, if we ignore
		 * the unregistering return code the handler will not
		 * be unregistered, fd will be closed and we may get the
		 * crush. Hanging and messaging in the loop seems not to be
		 * the worst choice.
		 */
		DRV_LOG(INFO, "Retrying to unregister interrupt handler");
		start = rte_get_timer_cycles();
	} while (true);
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

	if (mlx5_glue->query_device(priv->sh->ctx, &device_attr) == EIO)
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
		/* New representors naming schema. */
		switch_info->representor = 1;
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
	char port_name[IF_NAMESIZE];
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
	int ret;

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
		ret = fscanf(file, "%" RTE_STR(IF_NAMESIZE) "s", port_name);
		fclose(file);
		if (ret == 1)
			mlx5_translate_port_name(port_name, &data);
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

	if (!dev || !modinfo) {
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

	if (!dev || !info) {
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
 * @param[out] stats
 *   Counters table output buffer.
 *
 * @return
 *   0 on success and stats is filled, negative errno value otherwise and
 *   rte_errno is set.
 */
int
mlx5_os_read_dev_counters(struct rte_eth_dev *dev, uint64_t *stats)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_xstats_ctrl *xstats_ctrl = &priv->xstats_ctrl;
	unsigned int i;
	struct ifreq ifr;
	unsigned int stats_sz = xstats_ctrl->stats_n * sizeof(uint64_t);
	unsigned char et_stat_buf[sizeof(struct ethtool_stats) + stats_sz];
	struct ethtool_stats *et_stats = (struct ethtool_stats *)et_stat_buf;
	int ret;

	et_stats->cmd = ETHTOOL_GSTATS;
	et_stats->n_stats = xstats_ctrl->stats_n;
	ifr.ifr_data = (caddr_t)et_stats;
	ret = mlx5_ifreq(dev, SIOCETHTOOL, &ifr);
	if (ret) {
		DRV_LOG(WARNING,
			"port %u unable to read statistic values from device",
			dev->data->port_id);
		return ret;
	}
	for (i = 0; i != xstats_ctrl->mlx5_stats_n; ++i) {
		if (xstats_ctrl->info[i].dev) {
			ret = mlx5_os_read_dev_stat(priv,
					    xstats_ctrl->info[i].ctr_name,
					    &stats[i]);
			/* return last xstats counter if fail to read. */
			if (ret == 0)
				xstats_ctrl->xstats[i] = stats[i];
			else
				stats[i] = xstats_ctrl->xstats[i];
		} else {
			stats[i] = (uint64_t)
				et_stats->data[xstats_ctrl->dev_table_idx[i]];
		}
	}
	return 0;
}

/**
 * Query the number of statistics provided by ETHTOOL.
 *
 * @param dev
 *   Pointer to Ethernet device.
 *
 * @return
 *   Number of statistics on success, negative errno value otherwise and
 *   rte_errno is set.
 */
int
mlx5_os_get_stats_n(struct rte_eth_dev *dev)
{
	struct ethtool_drvinfo drvinfo;
	struct ifreq ifr;
	int ret;

	drvinfo.cmd = ETHTOOL_GDRVINFO;
	ifr.ifr_data = (caddr_t)&drvinfo;
	ret = mlx5_ifreq(dev, SIOCETHTOOL, &ifr);
	if (ret) {
		DRV_LOG(WARNING, "port %u unable to query number of statistics",
			dev->data->port_id);
		return ret;
	}
	return drvinfo.n_stats;
}

static const struct mlx5_counter_ctrl mlx5_counters_init[] = {
	{
		.dpdk_name = "rx_port_unicast_bytes",
		.ctr_name = "rx_vport_unicast_bytes",
	},
	{
		.dpdk_name = "rx_port_multicast_bytes",
		.ctr_name = "rx_vport_multicast_bytes",
	},
	{
		.dpdk_name = "rx_port_broadcast_bytes",
		.ctr_name = "rx_vport_broadcast_bytes",
	},
	{
		.dpdk_name = "rx_port_unicast_packets",
		.ctr_name = "rx_vport_unicast_packets",
	},
	{
		.dpdk_name = "rx_port_multicast_packets",
		.ctr_name = "rx_vport_multicast_packets",
	},
	{
		.dpdk_name = "rx_port_broadcast_packets",
		.ctr_name = "rx_vport_broadcast_packets",
	},
	{
		.dpdk_name = "tx_port_unicast_bytes",
		.ctr_name = "tx_vport_unicast_bytes",
	},
	{
		.dpdk_name = "tx_port_multicast_bytes",
		.ctr_name = "tx_vport_multicast_bytes",
	},
	{
		.dpdk_name = "tx_port_broadcast_bytes",
		.ctr_name = "tx_vport_broadcast_bytes",
	},
	{
		.dpdk_name = "tx_port_unicast_packets",
		.ctr_name = "tx_vport_unicast_packets",
	},
	{
		.dpdk_name = "tx_port_multicast_packets",
		.ctr_name = "tx_vport_multicast_packets",
	},
	{
		.dpdk_name = "tx_port_broadcast_packets",
		.ctr_name = "tx_vport_broadcast_packets",
	},
	{
		.dpdk_name = "rx_wqe_err",
		.ctr_name = "rx_wqe_err",
	},
	{
		.dpdk_name = "rx_crc_errors_phy",
		.ctr_name = "rx_crc_errors_phy",
	},
	{
		.dpdk_name = "rx_in_range_len_errors_phy",
		.ctr_name = "rx_in_range_len_errors_phy",
	},
	{
		.dpdk_name = "rx_symbol_err_phy",
		.ctr_name = "rx_symbol_err_phy",
	},
	{
		.dpdk_name = "tx_errors_phy",
		.ctr_name = "tx_errors_phy",
	},
	{
		.dpdk_name = "rx_out_of_buffer",
		.ctr_name = "out_of_buffer",
		.dev = 1,
	},
	{
		.dpdk_name = "tx_packets_phy",
		.ctr_name = "tx_packets_phy",
	},
	{
		.dpdk_name = "rx_packets_phy",
		.ctr_name = "rx_packets_phy",
	},
	{
		.dpdk_name = "tx_discards_phy",
		.ctr_name = "tx_discards_phy",
	},
	{
		.dpdk_name = "rx_discards_phy",
		.ctr_name = "rx_discards_phy",
	},
	{
		.dpdk_name = "tx_bytes_phy",
		.ctr_name = "tx_bytes_phy",
	},
	{
		.dpdk_name = "rx_bytes_phy",
		.ctr_name = "rx_bytes_phy",
	},
	/* Representor only */
	{
		.dpdk_name = "rx_packets",
		.ctr_name = "vport_rx_packets",
	},
	{
		.dpdk_name = "rx_bytes",
		.ctr_name = "vport_rx_bytes",
	},
	{
		.dpdk_name = "tx_packets",
		.ctr_name = "vport_tx_packets",
	},
	{
		.dpdk_name = "tx_bytes",
		.ctr_name = "vport_tx_bytes",
	},
};

static const unsigned int xstats_n = RTE_DIM(mlx5_counters_init);

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
	unsigned int i;
	unsigned int j;
	struct ifreq ifr;
	struct ethtool_gstrings *strings = NULL;
	unsigned int dev_stats_n;
	unsigned int str_sz;
	int ret;

	/* So that it won't aggregate for each init. */
	xstats_ctrl->mlx5_stats_n = 0;
	ret = mlx5_os_get_stats_n(dev);
	if (ret < 0) {
		DRV_LOG(WARNING, "port %u no extended statistics available",
			dev->data->port_id);
		return;
	}
	dev_stats_n = ret;
	/* Allocate memory to grab stat names and values. */
	str_sz = dev_stats_n * ETH_GSTRING_LEN;
	strings = (struct ethtool_gstrings *)
		  mlx5_malloc(0, str_sz + sizeof(struct ethtool_gstrings), 0,
			      SOCKET_ID_ANY);
	if (!strings) {
		DRV_LOG(WARNING, "port %u unable to allocate memory for xstats",
		     dev->data->port_id);
		return;
	}
	strings->cmd = ETHTOOL_GSTRINGS;
	strings->string_set = ETH_SS_STATS;
	strings->len = dev_stats_n;
	ifr.ifr_data = (caddr_t)strings;
	ret = mlx5_ifreq(dev, SIOCETHTOOL, &ifr);
	if (ret) {
		DRV_LOG(WARNING, "port %u unable to get statistic names",
			dev->data->port_id);
		goto free;
	}
	for (i = 0; i != dev_stats_n; ++i) {
		const char *curr_string = (const char *)
			&strings->data[i * ETH_GSTRING_LEN];

		for (j = 0; j != xstats_n; ++j) {
			if (!strcmp(mlx5_counters_init[j].ctr_name,
				    curr_string)) {
				unsigned int idx = xstats_ctrl->mlx5_stats_n++;

				xstats_ctrl->dev_table_idx[idx] = i;
				xstats_ctrl->info[idx] = mlx5_counters_init[j];
				break;
			}
		}
	}
	/* Add dev counters. */
	for (i = 0; i != xstats_n; ++i) {
		if (mlx5_counters_init[i].dev) {
			unsigned int idx = xstats_ctrl->mlx5_stats_n++;

			xstats_ctrl->info[idx] = mlx5_counters_init[i];
			xstats_ctrl->hw_stats[idx] = 0;
		}
	}
	MLX5_ASSERT(xstats_ctrl->mlx5_stats_n <= MLX5_MAX_XSTATS);
	xstats_ctrl->stats_n = dev_stats_n;
	/* Copy to base at first time. */
	ret = mlx5_os_read_dev_counters(dev, xstats_ctrl->base);
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

