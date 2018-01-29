/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2015 6WIND S.A.
 * Copyright 2015 Mellanox.
 */

#define _GNU_SOURCE

#include <stddef.h>
#include <assert.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <dirent.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/utsname.h>
#include <netinet/in.h>
#include <linux/ethtool.h>
#include <linux/sockios.h>
#include <linux/version.h>
#include <fcntl.h>
#include <stdalign.h>
#include <sys/un.h>

#include <rte_atomic.h>
#include <rte_ethdev_driver.h>
#include <rte_bus_pci.h>
#include <rte_mbuf.h>
#include <rte_common.h>
#include <rte_interrupts.h>
#include <rte_alarm.h>
#include <rte_malloc.h>

#include "mlx5.h"
#include "mlx5_glue.h"
#include "mlx5_rxtx.h"
#include "mlx5_utils.h"

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

/**
 * Get interface name from private structure.
 *
 * @param[in] priv
 *   Pointer to private structure.
 * @param[out] ifname
 *   Interface name output buffer.
 *
 * @return
 *   0 on success, -1 on failure and errno is set.
 */
int
priv_get_ifname(const struct priv *priv, char (*ifname)[IF_NAMESIZE])
{
	DIR *dir;
	struct dirent *dent;
	unsigned int dev_type = 0;
	unsigned int dev_port_prev = ~0u;
	char match[IF_NAMESIZE] = "";

	{
		MKSTR(path, "%s/device/net", priv->ibdev_path);

		dir = opendir(path);
		if (dir == NULL)
			return -1;
	}
	while ((dent = readdir(dir)) != NULL) {
		char *name = dent->d_name;
		FILE *file;
		unsigned int dev_port;
		int r;

		if ((name[0] == '.') &&
		    ((name[1] == '\0') ||
		     ((name[1] == '.') && (name[2] == '\0'))))
			continue;

		MKSTR(path, "%s/device/net/%s/%s",
		      priv->ibdev_path, name,
		      (dev_type ? "dev_id" : "dev_port"));

		file = fopen(path, "rb");
		if (file == NULL) {
			if (errno != ENOENT)
				continue;
			/*
			 * Switch to dev_id when dev_port does not exist as
			 * is the case with Linux kernel versions < 3.15.
			 */
try_dev_id:
			match[0] = '\0';
			if (dev_type)
				break;
			dev_type = 1;
			dev_port_prev = ~0u;
			rewinddir(dir);
			continue;
		}
		r = fscanf(file, (dev_type ? "%x" : "%u"), &dev_port);
		fclose(file);
		if (r != 1)
			continue;
		/*
		 * Switch to dev_id when dev_port returns the same value for
		 * all ports. May happen when using a MOFED release older than
		 * 3.0 with a Linux kernel >= 3.15.
		 */
		if (dev_port == dev_port_prev)
			goto try_dev_id;
		dev_port_prev = dev_port;
		if (dev_port == (priv->port - 1u))
			snprintf(match, sizeof(match), "%s", name);
	}
	closedir(dir);
	if (match[0] == '\0')
		return -1;
	strncpy(*ifname, match, sizeof(*ifname));
	return 0;
}

/**
 * Check if the counter is located on ib counters file.
 *
 * @param[in] cntr
 *   Counter name.
 *
 * @return
 *   1 if counter is located on ib counters file , 0 otherwise.
 */
int
priv_is_ib_cntr(const char *cntr)
{
	if (!strcmp(cntr, "out_of_buffer"))
		return 1;
	return 0;
}

/**
 * Read from sysfs entry.
 *
 * @param[in] priv
 *   Pointer to private structure.
 * @param[in] entry
 *   Entry name relative to sysfs path.
 * @param[out] buf
 *   Data output buffer.
 * @param size
 *   Buffer size.
 *
 * @return
 *   0 on success, -1 on failure and errno is set.
 */
static int
priv_sysfs_read(const struct priv *priv, const char *entry,
		char *buf, size_t size)
{
	char ifname[IF_NAMESIZE];
	FILE *file;
	int ret;
	int err;

	if (priv_get_ifname(priv, &ifname))
		return -1;

	if (priv_is_ib_cntr(entry)) {
		MKSTR(path, "%s/ports/1/hw_counters/%s",
		      priv->ibdev_path, entry);
		file = fopen(path, "rb");
	} else {
		MKSTR(path, "%s/device/net/%s/%s",
		      priv->ibdev_path, ifname, entry);
		file = fopen(path, "rb");
	}
	if (file == NULL)
		return -1;
	ret = fread(buf, 1, size, file);
	err = errno;
	if (((size_t)ret < size) && (ferror(file)))
		ret = -1;
	else
		ret = size;
	fclose(file);
	errno = err;
	return ret;
}

/**
 * Write to sysfs entry.
 *
 * @param[in] priv
 *   Pointer to private structure.
 * @param[in] entry
 *   Entry name relative to sysfs path.
 * @param[in] buf
 *   Data buffer.
 * @param size
 *   Buffer size.
 *
 * @return
 *   0 on success, -1 on failure and errno is set.
 */
static int
priv_sysfs_write(const struct priv *priv, const char *entry,
		 char *buf, size_t size)
{
	char ifname[IF_NAMESIZE];
	FILE *file;
	int ret;
	int err;

	if (priv_get_ifname(priv, &ifname))
		return -1;

	MKSTR(path, "%s/device/net/%s/%s", priv->ibdev_path, ifname, entry);

	file = fopen(path, "wb");
	if (file == NULL)
		return -1;
	ret = fwrite(buf, 1, size, file);
	err = errno;
	if (((size_t)ret < size) || (ferror(file)))
		ret = -1;
	else
		ret = size;
	fclose(file);
	errno = err;
	return ret;
}

/**
 * Get unsigned long sysfs property.
 *
 * @param priv
 *   Pointer to private structure.
 * @param[in] name
 *   Entry name relative to sysfs path.
 * @param[out] value
 *   Value output buffer.
 *
 * @return
 *   0 on success, -1 on failure and errno is set.
 */
static int
priv_get_sysfs_ulong(struct priv *priv, const char *name, unsigned long *value)
{
	int ret;
	unsigned long value_ret;
	char value_str[32];

	ret = priv_sysfs_read(priv, name, value_str, (sizeof(value_str) - 1));
	if (ret == -1) {
		DEBUG("cannot read %s value from sysfs: %s",
		      name, strerror(errno));
		return -1;
	}
	value_str[ret] = '\0';
	errno = 0;
	value_ret = strtoul(value_str, NULL, 0);
	if (errno) {
		DEBUG("invalid %s value `%s': %s", name, value_str,
		      strerror(errno));
		return -1;
	}
	*value = value_ret;
	return 0;
}

/**
 * Set unsigned long sysfs property.
 *
 * @param priv
 *   Pointer to private structure.
 * @param[in] name
 *   Entry name relative to sysfs path.
 * @param value
 *   Value to set.
 *
 * @return
 *   0 on success, -1 on failure and errno is set.
 */
static int
priv_set_sysfs_ulong(struct priv *priv, const char *name, unsigned long value)
{
	int ret;
	MKSTR(value_str, "%lu", value);

	ret = priv_sysfs_write(priv, name, value_str, (sizeof(value_str) - 1));
	if (ret == -1) {
		DEBUG("cannot write %s `%s' (%lu) to sysfs: %s",
		      name, value_str, value, strerror(errno));
		return -1;
	}
	return 0;
}

/**
 * Perform ifreq ioctl() on associated Ethernet device.
 *
 * @param[in] priv
 *   Pointer to private structure.
 * @param req
 *   Request number to pass to ioctl().
 * @param[out] ifr
 *   Interface request structure output buffer.
 *
 * @return
 *   0 on success, -1 on failure and errno is set.
 */
int
priv_ifreq(const struct priv *priv, int req, struct ifreq *ifr)
{
	int sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
	int ret = -1;

	if (sock == -1)
		return ret;
	if (priv_get_ifname(priv, &ifr->ifr_name) == 0)
		ret = ioctl(sock, req, ifr);
	close(sock);
	return ret;
}

/**
 * Return the number of active VFs for the current device.
 *
 * @param[in] priv
 *   Pointer to private structure.
 * @param[out] num_vfs
 *   Number of active VFs.
 *
 * @return
 *   0 on success, -1 on failure and errno is set.
 */
int
priv_get_num_vfs(struct priv *priv, uint16_t *num_vfs)
{
	/* The sysfs entry name depends on the operating system. */
	const char **name = (const char *[]){
		"device/sriov_numvfs",
		"device/mlx5_num_vfs",
		NULL,
	};
	int ret;

	do {
		unsigned long ulong_num_vfs;

		ret = priv_get_sysfs_ulong(priv, *name, &ulong_num_vfs);
		if (!ret)
			*num_vfs = ulong_num_vfs;
	} while (*(++name) && ret);
	return ret;
}

/**
 * Get device MTU.
 *
 * @param priv
 *   Pointer to private structure.
 * @param[out] mtu
 *   MTU value output buffer.
 *
 * @return
 *   0 on success, -1 on failure and errno is set.
 */
int
priv_get_mtu(struct priv *priv, uint16_t *mtu)
{
	unsigned long ulong_mtu;

	if (priv_get_sysfs_ulong(priv, "mtu", &ulong_mtu) == -1)
		return -1;
	*mtu = ulong_mtu;
	return 0;
}

/**
 * Read device counter from sysfs.
 *
 * @param priv
 *   Pointer to private structure.
 * @param name
 *   Counter name.
 * @param[out] cntr
 *   Counter output buffer.
 *
 * @return
 *   0 on success, -1 on failure and errno is set.
 */
int
priv_get_cntr_sysfs(struct priv *priv, const char *name, uint64_t *cntr)
{
	unsigned long ulong_ctr;

	if (priv_get_sysfs_ulong(priv, name, &ulong_ctr) == -1)
		return -1;
	*cntr = ulong_ctr;
	return 0;
}

/**
 * Set device MTU.
 *
 * @param priv
 *   Pointer to private structure.
 * @param mtu
 *   MTU value to set.
 *
 * @return
 *   0 on success, -1 on failure and errno is set.
 */
static int
priv_set_mtu(struct priv *priv, uint16_t mtu)
{
	uint16_t new_mtu;

	if (priv_set_sysfs_ulong(priv, "mtu", mtu) ||
	    priv_get_mtu(priv, &new_mtu))
		return -1;
	if (new_mtu == mtu)
		return 0;
	errno = EINVAL;
	return -1;
}

/**
 * Set device flags.
 *
 * @param priv
 *   Pointer to private structure.
 * @param keep
 *   Bitmask for flags that must remain untouched.
 * @param flags
 *   Bitmask for flags to modify.
 *
 * @return
 *   0 on success, -1 on failure and errno is set.
 */
int
priv_set_flags(struct priv *priv, unsigned int keep, unsigned int flags)
{
	unsigned long tmp;

	if (priv_get_sysfs_ulong(priv, "flags", &tmp) == -1)
		return -1;
	tmp &= keep;
	tmp |= (flags & (~keep));
	return priv_set_sysfs_ulong(priv, "flags", tmp);
}

/**
 * Ethernet device configuration.
 *
 * Prepare the driver for a given number of TX and RX queues.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success, errno value on failure.
 */
static int
dev_configure(struct rte_eth_dev *dev)
{
	struct priv *priv = dev->data->dev_private;
	unsigned int rxqs_n = dev->data->nb_rx_queues;
	unsigned int txqs_n = dev->data->nb_tx_queues;
	unsigned int i;
	unsigned int j;
	unsigned int reta_idx_n;
	const uint8_t use_app_rss_key =
		!!dev->data->dev_conf.rx_adv_conf.rss_conf.rss_key;
	uint64_t supp_tx_offloads = mlx5_priv_get_tx_port_offloads(priv);
	uint64_t tx_offloads = dev->data->dev_conf.txmode.offloads;
	uint64_t supp_rx_offloads =
		(mlx5_priv_get_rx_port_offloads(priv) |
		 mlx5_priv_get_rx_queue_offloads(priv));
	uint64_t rx_offloads = dev->data->dev_conf.rxmode.offloads;

	if ((tx_offloads & supp_tx_offloads) != tx_offloads) {
		ERROR("Some Tx offloads are not supported "
		      "requested 0x%" PRIx64 " supported 0x%" PRIx64,
		      tx_offloads, supp_tx_offloads);
		return ENOTSUP;
	}
	if ((rx_offloads & supp_rx_offloads) != rx_offloads) {
		ERROR("Some Rx offloads are not supported "
		      "requested 0x%" PRIx64 " supported 0x%" PRIx64,
		      rx_offloads, supp_rx_offloads);
		return ENOTSUP;
	}
	if (use_app_rss_key &&
	    (dev->data->dev_conf.rx_adv_conf.rss_conf.rss_key_len !=
	     rss_hash_default_key_len)) {
		/* MLX5 RSS only support 40bytes key. */
		return EINVAL;
	}
	priv->rss_conf.rss_key =
		rte_realloc(priv->rss_conf.rss_key,
			    rss_hash_default_key_len, 0);
	if (!priv->rss_conf.rss_key) {
		ERROR("cannot allocate RSS hash key memory (%u)", rxqs_n);
		return ENOMEM;
	}
	memcpy(priv->rss_conf.rss_key,
	       use_app_rss_key ?
	       dev->data->dev_conf.rx_adv_conf.rss_conf.rss_key :
	       rss_hash_default_key,
	       rss_hash_default_key_len);
	priv->rss_conf.rss_key_len = rss_hash_default_key_len;
	priv->rss_conf.rss_hf = dev->data->dev_conf.rx_adv_conf.rss_conf.rss_hf;
	priv->rxqs = (void *)dev->data->rx_queues;
	priv->txqs = (void *)dev->data->tx_queues;
	if (txqs_n != priv->txqs_n) {
		INFO("%p: TX queues number update: %u -> %u",
		     (void *)dev, priv->txqs_n, txqs_n);
		priv->txqs_n = txqs_n;
	}
	if (rxqs_n > priv->config.ind_table_max_size) {
		ERROR("cannot handle this many RX queues (%u)", rxqs_n);
		return EINVAL;
	}
	if (rxqs_n == priv->rxqs_n)
		return 0;
	INFO("%p: RX queues number update: %u -> %u",
	     (void *)dev, priv->rxqs_n, rxqs_n);
	priv->rxqs_n = rxqs_n;
	/* If the requested number of RX queues is not a power of two, use the
	 * maximum indirection table size for better balancing.
	 * The result is always rounded to the next power of two. */
	reta_idx_n = (1 << log2above((rxqs_n & (rxqs_n - 1)) ?
				     priv->config.ind_table_max_size :
				     rxqs_n));
	if (priv_rss_reta_index_resize(priv, reta_idx_n))
		return ENOMEM;
	/* When the number of RX queues is not a power of two, the remaining
	 * table entries are padded with reused WQs and hashes are not spread
	 * uniformly. */
	for (i = 0, j = 0; (i != reta_idx_n); ++i) {
		(*priv->reta_idx)[i] = j;
		if (++j == rxqs_n)
			j = 0;
	}
	return 0;
}

/**
 * DPDK callback for Ethernet device configuration.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success, negative errno value on failure.
 */
int
mlx5_dev_configure(struct rte_eth_dev *dev)
{
	struct priv *priv = dev->data->dev_private;
	int ret;

	priv_lock(priv);
	ret = dev_configure(dev);
	assert(ret >= 0);
	priv_unlock(priv);
	return -ret;
}

/**
 * DPDK callback to get information about the device.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param[out] info
 *   Info structure output buffer.
 */
void
mlx5_dev_infos_get(struct rte_eth_dev *dev, struct rte_eth_dev_info *info)
{
	struct priv *priv = dev->data->dev_private;
	struct mlx5_dev_config *config = &priv->config;
	unsigned int max;
	char ifname[IF_NAMESIZE];

	info->pci_dev = RTE_ETH_DEV_TO_PCI(dev);

	priv_lock(priv);
	/* FIXME: we should ask the device for these values. */
	info->min_rx_bufsize = 32;
	info->max_rx_pktlen = 65536;
	/*
	 * Since we need one CQ per QP, the limit is the minimum number
	 * between the two values.
	 */
	max = RTE_MIN(priv->device_attr.orig_attr.max_cq,
		      priv->device_attr.orig_attr.max_qp);
	/* If max >= 65535 then max = 0, max_rx_queues is uint16_t. */
	if (max >= 65535)
		max = 65535;
	info->max_rx_queues = max;
	info->max_tx_queues = max;
	info->max_mac_addrs = RTE_DIM(priv->mac);
	info->rx_queue_offload_capa =
		mlx5_priv_get_rx_queue_offloads(priv);
	info->rx_offload_capa = (mlx5_priv_get_rx_port_offloads(priv) |
				 info->rx_queue_offload_capa);
	info->tx_offload_capa = mlx5_priv_get_tx_port_offloads(priv);
	if (priv_get_ifname(priv, &ifname) == 0)
		info->if_index = if_nametoindex(ifname);
	info->reta_size = priv->reta_idx_n ?
		priv->reta_idx_n : config->ind_table_max_size;
	info->hash_key_size = priv->rss_conf.rss_key_len;
	info->speed_capa = priv->link_speed_capa;
	info->flow_type_rss_offloads = ~MLX5_RSS_HF_MASK;
	priv_unlock(priv);
}

const uint32_t *
mlx5_dev_supported_ptypes_get(struct rte_eth_dev *dev)
{
	static const uint32_t ptypes[] = {
		/* refers to rxq_cq_to_pkt_type() */
		RTE_PTYPE_L2_ETHER,
		RTE_PTYPE_L3_IPV4_EXT_UNKNOWN,
		RTE_PTYPE_L3_IPV6_EXT_UNKNOWN,
		RTE_PTYPE_L4_NONFRAG,
		RTE_PTYPE_L4_FRAG,
		RTE_PTYPE_L4_TCP,
		RTE_PTYPE_L4_UDP,
		RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN,
		RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN,
		RTE_PTYPE_INNER_L4_NONFRAG,
		RTE_PTYPE_INNER_L4_FRAG,
		RTE_PTYPE_INNER_L4_TCP,
		RTE_PTYPE_INNER_L4_UDP,
		RTE_PTYPE_UNKNOWN
	};

	if (dev->rx_pkt_burst == mlx5_rx_burst ||
	    dev->rx_pkt_burst == mlx5_rx_burst_vec)
		return ptypes;
	return NULL;
}

/**
 * DPDK callback to retrieve physical link information.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param wait_to_complete
 *   Wait for request completion (ignored).
 */
static int
mlx5_link_update_unlocked_gset(struct rte_eth_dev *dev, int wait_to_complete)
{
	struct priv *priv = dev->data->dev_private;
	struct ethtool_cmd edata = {
		.cmd = ETHTOOL_GSET /* Deprecated since Linux v4.5. */
	};
	struct ifreq ifr;
	struct rte_eth_link dev_link;
	int link_speed = 0;

	/* priv_lock() is not taken to allow concurrent calls. */

	(void)wait_to_complete;
	if (priv_ifreq(priv, SIOCGIFFLAGS, &ifr)) {
		WARN("ioctl(SIOCGIFFLAGS) failed: %s", strerror(errno));
		return -1;
	}
	memset(&dev_link, 0, sizeof(dev_link));
	dev_link.link_status = ((ifr.ifr_flags & IFF_UP) &&
				(ifr.ifr_flags & IFF_RUNNING));
	ifr.ifr_data = (void *)&edata;
	if (priv_ifreq(priv, SIOCETHTOOL, &ifr)) {
		WARN("ioctl(SIOCETHTOOL, ETHTOOL_GSET) failed: %s",
		     strerror(errno));
		return -1;
	}
	link_speed = ethtool_cmd_speed(&edata);
	if (link_speed == -1)
		dev_link.link_speed = 0;
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
	if (memcmp(&dev_link, &dev->data->dev_link, sizeof(dev_link))) {
		/* Link status changed. */
		dev->data->dev_link = dev_link;
		return 0;
	}
	/* Link status is still the same. */
	return -1;
}

/**
 * Retrieve physical link information (unlocked version using new ioctl).
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param wait_to_complete
 *   Wait for request completion (ignored).
 */
static int
mlx5_link_update_unlocked_gs(struct rte_eth_dev *dev, int wait_to_complete)
{
	struct priv *priv = dev->data->dev_private;
	struct ethtool_link_settings gcmd = { .cmd = ETHTOOL_GLINKSETTINGS };
	struct ifreq ifr;
	struct rte_eth_link dev_link;
	uint64_t sc;

	(void)wait_to_complete;
	if (priv_ifreq(priv, SIOCGIFFLAGS, &ifr)) {
		WARN("ioctl(SIOCGIFFLAGS) failed: %s", strerror(errno));
		return -1;
	}
	memset(&dev_link, 0, sizeof(dev_link));
	dev_link.link_status = ((ifr.ifr_flags & IFF_UP) &&
				(ifr.ifr_flags & IFF_RUNNING));
	ifr.ifr_data = (void *)&gcmd;
	if (priv_ifreq(priv, SIOCETHTOOL, &ifr)) {
		DEBUG("ioctl(SIOCETHTOOL, ETHTOOL_GLINKSETTINGS) failed: %s",
		      strerror(errno));
		return -1;
	}
	gcmd.link_mode_masks_nwords = -gcmd.link_mode_masks_nwords;

	alignas(struct ethtool_link_settings)
	uint8_t data[offsetof(struct ethtool_link_settings, link_mode_masks) +
		     sizeof(uint32_t) * gcmd.link_mode_masks_nwords * 3];
	struct ethtool_link_settings *ecmd = (void *)data;

	*ecmd = gcmd;
	ifr.ifr_data = (void *)ecmd;
	if (priv_ifreq(priv, SIOCETHTOOL, &ifr)) {
		DEBUG("ioctl(SIOCETHTOOL, ETHTOOL_GLINKSETTINGS) failed: %s",
		      strerror(errno));
		return -1;
	}
	dev_link.link_speed = ecmd->speed;
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
	dev_link.link_duplex = ((ecmd->duplex == DUPLEX_HALF) ?
				ETH_LINK_HALF_DUPLEX : ETH_LINK_FULL_DUPLEX);
	dev_link.link_autoneg = !(dev->data->dev_conf.link_speeds &
				  ETH_LINK_SPEED_FIXED);
	if (memcmp(&dev_link, &dev->data->dev_link, sizeof(dev_link))) {
		/* Link status changed. */
		dev->data->dev_link = dev_link;
		return 0;
	}
	/* Link status is still the same. */
	return -1;
}

/**
 * Enable receiving and transmitting traffic.
 *
 * @param priv
 *   Pointer to private structure.
 */
static void
priv_link_start(struct priv *priv)
{
	struct rte_eth_dev *dev = priv->dev;
	int err;

	dev->tx_pkt_burst = priv_select_tx_function(priv, dev);
	dev->rx_pkt_burst = priv_select_rx_function(priv, dev);
	err = priv_dev_traffic_enable(priv, dev);
	if (err)
		ERROR("%p: error occurred while configuring control flows: %s",
		      (void *)priv, strerror(err));
	err = priv_flow_start(priv, &priv->flows);
	if (err)
		ERROR("%p: error occurred while configuring flows: %s",
		      (void *)priv, strerror(err));
}

/**
 * Disable receiving and transmitting traffic.
 *
 * @param priv
 *   Pointer to private structure.
 */
static void
priv_link_stop(struct priv *priv)
{
	struct rte_eth_dev *dev = priv->dev;

	priv_flow_stop(priv, &priv->flows);
	priv_dev_traffic_disable(priv, dev);
	dev->rx_pkt_burst = removed_rx_burst;
	dev->tx_pkt_burst = removed_tx_burst;
}

/**
 * Retrieve physical link information and update rx/tx_pkt_burst callbacks
 * accordingly.
 *
 * @param priv
 *   Pointer to private structure.
 * @param wait_to_complete
 *   Wait for request completion (ignored).
 */
int
priv_link_update(struct priv *priv, int wait_to_complete)
{
	struct rte_eth_dev *dev = priv->dev;
	struct utsname utsname;
	int ver[3];
	int ret;
	struct rte_eth_link dev_link = dev->data->dev_link;

	if (uname(&utsname) == -1 ||
	    sscanf(utsname.release, "%d.%d.%d",
		   &ver[0], &ver[1], &ver[2]) != 3 ||
	    KERNEL_VERSION(ver[0], ver[1], ver[2]) < KERNEL_VERSION(4, 9, 0))
		ret = mlx5_link_update_unlocked_gset(dev, wait_to_complete);
	else
		ret = mlx5_link_update_unlocked_gs(dev, wait_to_complete);
	/* If lsc interrupt is disabled, should always be ready for traffic. */
	if (!dev->data->dev_conf.intr_conf.lsc) {
		priv_link_start(priv);
		return ret;
	}
	/* Re-select burst callbacks only if link status has been changed. */
	if (!ret && dev_link.link_status != dev->data->dev_link.link_status) {
		if (dev->data->dev_link.link_status == ETH_LINK_UP)
			priv_link_start(priv);
		else
			priv_link_stop(priv);
	}
	return ret;
}

/**
 * Querying the link status till it changes to the desired state.
 * Number of query attempts is bounded by MLX5_MAX_LINK_QUERY_ATTEMPTS.
 *
 * @param priv
 *   Pointer to private structure.
 * @param status
 *   Link desired status.
 *
 * @return
 *   0 on success, negative errno value on failure.
 */
int
priv_force_link_status_change(struct priv *priv, int status)
{
	int try = 0;

	while (try < MLX5_MAX_LINK_QUERY_ATTEMPTS) {
		priv_link_update(priv, 0);
		if (priv->dev->data->dev_link.link_status == status)
			return 0;
		try++;
		sleep(1);
	}
	return -EAGAIN;
}

/**
 * DPDK callback to retrieve physical link information.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param wait_to_complete
 *   Wait for request completion (ignored).
 */
int
mlx5_link_update(struct rte_eth_dev *dev, int wait_to_complete)
{
	struct priv *priv = dev->data->dev_private;
	int ret;

	priv_lock(priv);
	ret = priv_link_update(priv, wait_to_complete);
	priv_unlock(priv);
	return ret;
}

/**
 * DPDK callback to change the MTU.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param in_mtu
 *   New MTU.
 *
 * @return
 *   0 on success, negative errno value on failure.
 */
int
mlx5_dev_set_mtu(struct rte_eth_dev *dev, uint16_t mtu)
{
	struct priv *priv = dev->data->dev_private;
	uint16_t kern_mtu;
	int ret = 0;

	priv_lock(priv);
	ret = priv_get_mtu(priv, &kern_mtu);
	if (ret)
		goto out;
	/* Set kernel interface MTU first. */
	ret = priv_set_mtu(priv, mtu);
	if (ret)
		goto out;
	ret = priv_get_mtu(priv, &kern_mtu);
	if (ret)
		goto out;
	if (kern_mtu == mtu) {
		priv->mtu = mtu;
		DEBUG("adapter port %u MTU set to %u", priv->port, mtu);
	}
	priv_unlock(priv);
	return 0;
out:
	ret = errno;
	WARN("cannot set port %u MTU to %u: %s", priv->port, mtu,
	     strerror(ret));
	priv_unlock(priv);
	assert(ret >= 0);
	return -ret;
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
 *   0 on success, negative errno value on failure.
 */
int
mlx5_dev_get_flow_ctrl(struct rte_eth_dev *dev, struct rte_eth_fc_conf *fc_conf)
{
	struct priv *priv = dev->data->dev_private;
	struct ifreq ifr;
	struct ethtool_pauseparam ethpause = {
		.cmd = ETHTOOL_GPAUSEPARAM
	};
	int ret;

	ifr.ifr_data = (void *)&ethpause;
	priv_lock(priv);
	if (priv_ifreq(priv, SIOCETHTOOL, &ifr)) {
		ret = errno;
		WARN("ioctl(SIOCETHTOOL, ETHTOOL_GPAUSEPARAM)"
		     " failed: %s",
		     strerror(ret));
		goto out;
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
	ret = 0;

out:
	priv_unlock(priv);
	assert(ret >= 0);
	return -ret;
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
 *   0 on success, negative errno value on failure.
 */
int
mlx5_dev_set_flow_ctrl(struct rte_eth_dev *dev, struct rte_eth_fc_conf *fc_conf)
{
	struct priv *priv = dev->data->dev_private;
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

	priv_lock(priv);
	if (priv_ifreq(priv, SIOCETHTOOL, &ifr)) {
		ret = errno;
		WARN("ioctl(SIOCETHTOOL, ETHTOOL_SPAUSEPARAM)"
		     " failed: %s",
		     strerror(ret));
		goto out;
	}
	ret = 0;

out:
	priv_unlock(priv);
	assert(ret >= 0);
	return -ret;
}

/**
 * Get PCI information from struct ibv_device.
 *
 * @param device
 *   Pointer to Ethernet device structure.
 * @param[out] pci_addr
 *   PCI bus address output buffer.
 *
 * @return
 *   0 on success, -1 on failure and errno is set.
 */
int
mlx5_ibv_device_to_pci_addr(const struct ibv_device *device,
			    struct rte_pci_addr *pci_addr)
{
	FILE *file;
	char line[32];
	MKSTR(path, "%s/device/uevent", device->ibdev_path);

	file = fopen(path, "rb");
	if (file == NULL)
		return -1;
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
 * Update the link status.
 *
 * @param priv
 *   Pointer to private structure.
 *
 * @return
 *   Zero if the callback process can be called immediately.
 */
static int
priv_link_status_update(struct priv *priv)
{
	struct rte_eth_link *link = &priv->dev->data->dev_link;

	priv_link_update(priv, 0);
	if (((link->link_speed == 0) && link->link_status) ||
		((link->link_speed != 0) && !link->link_status)) {
		/*
		 * Inconsistent status. Event likely occurred before the
		 * kernel netdevice exposes the new status.
		 */
		if (!priv->pending_alarm) {
			priv->pending_alarm = 1;
			rte_eal_alarm_set(MLX5_ALARM_TIMEOUT_US,
					  mlx5_dev_link_status_handler,
					  priv->dev);
		}
		return 1;
	} else if (unlikely(priv->pending_alarm)) {
		/* Link interrupt occurred while alarm is already scheduled. */
		priv->pending_alarm = 0;
		rte_eal_alarm_cancel(mlx5_dev_link_status_handler, priv->dev);
	}
	return 0;
}

/**
 * Device status handler.
 *
 * @param priv
 *   Pointer to private structure.
 * @param events
 *   Pointer to event flags holder.
 *
 * @return
 *   Events bitmap of callback process which can be called immediately.
 */
static uint32_t
priv_dev_status_handler(struct priv *priv)
{
	struct ibv_async_event event;
	uint32_t ret = 0;

	/* Read all message and acknowledge them. */
	for (;;) {
		if (mlx5_glue->get_async_event(priv->ctx, &event))
			break;
		if ((event.event_type == IBV_EVENT_PORT_ACTIVE ||
			event.event_type == IBV_EVENT_PORT_ERR) &&
			(priv->dev->data->dev_conf.intr_conf.lsc == 1))
			ret |= (1 << RTE_ETH_EVENT_INTR_LSC);
		else if (event.event_type == IBV_EVENT_DEVICE_FATAL &&
			priv->dev->data->dev_conf.intr_conf.rmv == 1)
			ret |= (1 << RTE_ETH_EVENT_INTR_RMV);
		else
			DEBUG("event type %d on port %d not handled",
			      event.event_type, event.element.port_num);
		mlx5_glue->ack_async_event(&event);
	}
	if (ret & (1 << RTE_ETH_EVENT_INTR_LSC))
		if (priv_link_status_update(priv))
			ret &= ~(1 << RTE_ETH_EVENT_INTR_LSC);
	return ret;
}

/**
 * Handle delayed link status event.
 *
 * @param arg
 *   Registered argument.
 */
void
mlx5_dev_link_status_handler(void *arg)
{
	struct rte_eth_dev *dev = arg;
	struct priv *priv = dev->data->dev_private;
	int ret;

	while (!priv_trylock(priv)) {
		/* Alarm is being canceled. */
		if (priv->pending_alarm == 0)
			return;
		rte_pause();
	}
	priv->pending_alarm = 0;
	ret = priv_link_status_update(priv);
	priv_unlock(priv);
	if (!ret)
		_rte_eth_dev_callback_process(dev, RTE_ETH_EVENT_INTR_LSC, NULL);
}

/**
 * Handle interrupts from the NIC.
 *
 * @param[in] intr_handle
 *   Interrupt handler.
 * @param cb_arg
 *   Callback argument.
 */
void
mlx5_dev_interrupt_handler(void *cb_arg)
{
	struct rte_eth_dev *dev = cb_arg;
	struct priv *priv = dev->data->dev_private;
	uint32_t events;

	priv_lock(priv);
	events = priv_dev_status_handler(priv);
	priv_unlock(priv);
	if (events & (1 << RTE_ETH_EVENT_INTR_LSC))
		_rte_eth_dev_callback_process(dev, RTE_ETH_EVENT_INTR_LSC, NULL);
	if (events & (1 << RTE_ETH_EVENT_INTR_RMV))
		_rte_eth_dev_callback_process(dev, RTE_ETH_EVENT_INTR_RMV, NULL);
}

/**
 * Handle interrupts from the socket.
 *
 * @param cb_arg
 *   Callback argument.
 */
static void
mlx5_dev_handler_socket(void *cb_arg)
{
	struct rte_eth_dev *dev = cb_arg;
	struct priv *priv = dev->data->dev_private;

	priv_lock(priv);
	priv_socket_handle(priv);
	priv_unlock(priv);
}

/**
 * Uninstall interrupt handler.
 *
 * @param priv
 *   Pointer to private structure.
 * @param dev
 *   Pointer to the rte_eth_dev structure.
 */
void
priv_dev_interrupt_handler_uninstall(struct priv *priv, struct rte_eth_dev *dev)
{
	if (dev->data->dev_conf.intr_conf.lsc ||
	    dev->data->dev_conf.intr_conf.rmv)
		rte_intr_callback_unregister(&priv->intr_handle,
					     mlx5_dev_interrupt_handler, dev);
	if (priv->primary_socket)
		rte_intr_callback_unregister(&priv->intr_handle_socket,
					     mlx5_dev_handler_socket, dev);
	if (priv->pending_alarm) {
		priv->pending_alarm = 0;
		rte_eal_alarm_cancel(mlx5_dev_link_status_handler, dev);
	}
	priv->intr_handle.fd = 0;
	priv->intr_handle.type = RTE_INTR_HANDLE_UNKNOWN;
	priv->intr_handle_socket.fd = 0;
	priv->intr_handle_socket.type = RTE_INTR_HANDLE_UNKNOWN;
}

/**
 * Install interrupt handler.
 *
 * @param priv
 *   Pointer to private structure.
 * @param dev
 *   Pointer to the rte_eth_dev structure.
 */
void
priv_dev_interrupt_handler_install(struct priv *priv, struct rte_eth_dev *dev)
{
	int rc, flags;

	assert(priv->ctx->async_fd > 0);
	flags = fcntl(priv->ctx->async_fd, F_GETFL);
	rc = fcntl(priv->ctx->async_fd, F_SETFL, flags | O_NONBLOCK);
	if (rc < 0) {
		INFO("failed to change file descriptor async event queue");
		dev->data->dev_conf.intr_conf.lsc = 0;
		dev->data->dev_conf.intr_conf.rmv = 0;
	}
	if (dev->data->dev_conf.intr_conf.lsc ||
	    dev->data->dev_conf.intr_conf.rmv) {
		priv->intr_handle.fd = priv->ctx->async_fd;
		priv->intr_handle.type = RTE_INTR_HANDLE_EXT;
		rte_intr_callback_register(&priv->intr_handle,
					   mlx5_dev_interrupt_handler, dev);
	}

	rc = priv_socket_init(priv);
	if (!rc && priv->primary_socket) {
		priv->intr_handle_socket.fd = priv->primary_socket;
		priv->intr_handle_socket.type = RTE_INTR_HANDLE_EXT;
		rte_intr_callback_register(&priv->intr_handle_socket,
					   mlx5_dev_handler_socket, dev);
	}
}

/**
 * Change the link state (UP / DOWN).
 *
 * @param priv
 *   Pointer to private data structure.
 * @param up
 *   Nonzero for link up, otherwise link down.
 *
 * @return
 *   0 on success, errno value on failure.
 */
static int
priv_dev_set_link(struct priv *priv, int up)
{
	return priv_set_flags(priv, ~IFF_UP, up ? IFF_UP : ~IFF_UP);
}

/**
 * DPDK callback to bring the link DOWN.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success, errno value on failure.
 */
int
mlx5_set_link_down(struct rte_eth_dev *dev)
{
	struct priv *priv = dev->data->dev_private;
	int err;

	priv_lock(priv);
	err = priv_dev_set_link(priv, 0);
	priv_unlock(priv);
	return err;
}

/**
 * DPDK callback to bring the link UP.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success, errno value on failure.
 */
int
mlx5_set_link_up(struct rte_eth_dev *dev)
{
	struct priv *priv = dev->data->dev_private;
	int err;

	priv_lock(priv);
	err = priv_dev_set_link(priv, 1);
	priv_unlock(priv);
	return err;
}

/**
 * Configure the TX function to use.
 *
 * @param priv
 *   Pointer to private data structure.
 * @param dev
 *   Pointer to rte_eth_dev structure.
 *
 * @return
 *   Pointer to selected Tx burst function.
 */
eth_tx_burst_t
priv_select_tx_function(struct priv *priv, struct rte_eth_dev *dev)
{
	eth_tx_burst_t tx_pkt_burst = mlx5_tx_burst;
	struct mlx5_dev_config *config = &priv->config;
	uint64_t tx_offloads = dev->data->dev_conf.txmode.offloads;
	int tso = !!(tx_offloads & (DEV_TX_OFFLOAD_TCP_TSO |
				    DEV_TX_OFFLOAD_VXLAN_TNL_TSO |
				    DEV_TX_OFFLOAD_GRE_TNL_TSO));
	int vlan_insert = !!(tx_offloads & DEV_TX_OFFLOAD_VLAN_INSERT);

	assert(priv != NULL);
	/* Select appropriate TX function. */
	if (vlan_insert || tso)
		return tx_pkt_burst;
	if (config->mps == MLX5_MPW_ENHANCED) {
		if (priv_check_vec_tx_support(priv, dev) > 0) {
			if (priv_check_raw_vec_tx_support(priv, dev) > 0)
				tx_pkt_burst = mlx5_tx_burst_raw_vec;
			else
				tx_pkt_burst = mlx5_tx_burst_vec;
			DEBUG("selected Enhanced MPW TX vectorized function");
		} else {
			tx_pkt_burst = mlx5_tx_burst_empw;
			DEBUG("selected Enhanced MPW TX function");
		}
	} else if (config->mps && (config->txq_inline > 0)) {
		tx_pkt_burst = mlx5_tx_burst_mpw_inline;
		DEBUG("selected MPW inline TX function");
	} else if (config->mps) {
		tx_pkt_burst = mlx5_tx_burst_mpw;
		DEBUG("selected MPW TX function");
	}
	return tx_pkt_burst;
}

/**
 * Configure the RX function to use.
 *
 * @param priv
 *   Pointer to private data structure.
 * @param dev
 *   Pointer to rte_eth_dev structure.
 *
 * @return
 *   Pointer to selected Rx burst function.
 */
eth_rx_burst_t
priv_select_rx_function(struct priv *priv, __rte_unused struct rte_eth_dev *dev)
{
	eth_rx_burst_t rx_pkt_burst = mlx5_rx_burst;

	assert(priv != NULL);
	if (priv_check_vec_rx_support(priv) > 0) {
		rx_pkt_burst = mlx5_rx_burst_vec;
		DEBUG("selected RX vectorized function");
	}
	return rx_pkt_burst;
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
	struct priv *priv = dev->data->dev_private;

	if (mlx5_glue->query_device(priv->ctx, &device_attr) == EIO)
		return 1;
	return 0;
}
