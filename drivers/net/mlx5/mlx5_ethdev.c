/*-
 *   BSD LICENSE
 *
 *   Copyright 2015 6WIND S.A.
 *   Copyright 2015 Mellanox.
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

/* DPDK headers don't like -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <rte_atomic.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include <rte_common.h>
#include <rte_interrupts.h>
#include <rte_alarm.h>
#include <rte_malloc.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-Wpedantic"
#endif

#include "mlx5.h"
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
 * Return private structure associated with an Ethernet device.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   Pointer to private structure.
 */
struct priv *
mlx5_get_priv(struct rte_eth_dev *dev)
{
	struct mlx5_secondary_data *sd;

	if (!mlx5_is_secondary())
		return dev->data->dev_private;
	sd = &mlx5_secondary_data[dev->data->port_id];
	return sd->data.dev_private;
}

/**
 * Check if running as a secondary process.
 *
 * @return
 *   Nonzero if running as a secondary process.
 */
inline int
mlx5_is_secondary(void)
{
	return rte_eal_process_type() != RTE_PROC_PRIMARY;
}

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
		MKSTR(path, "%s/device/net", priv->ctx->device->ibdev_path);

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
		      priv->ctx->device->ibdev_path, name,
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
		      priv->ctx->device->ibdev_path, entry);
		file = fopen(path, "rb");
	} else {
		MKSTR(path, "%s/device/net/%s/%s",
		      priv->ctx->device->ibdev_path, ifname, entry);
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

	MKSTR(path, "%s/device/net/%s/%s", priv->ctx->device->ibdev_path,
	      ifname, entry);

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

	priv->rss_hf = dev->data->dev_conf.rx_adv_conf.rss_conf.rss_hf;
	priv->rxqs = (void *)dev->data->rx_queues;
	priv->txqs = (void *)dev->data->tx_queues;
	if (txqs_n != priv->txqs_n) {
		INFO("%p: TX queues number update: %u -> %u",
		     (void *)dev, priv->txqs_n, txqs_n);
		priv->txqs_n = txqs_n;
	}
	if (rxqs_n > priv->ind_table_max_size) {
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
				     priv->ind_table_max_size :
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

	if (mlx5_is_secondary())
		return -E_RTE_SECONDARY;

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
	struct priv *priv = mlx5_get_priv(dev);
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
	max = ((priv->device_attr.max_cq > priv->device_attr.max_qp) ?
	       priv->device_attr.max_qp : priv->device_attr.max_cq);
	/* If max >= 65535 then max = 0, max_rx_queues is uint16_t. */
	if (max >= 65535)
		max = 65535;
	info->max_rx_queues = max;
	info->max_tx_queues = max;
	info->max_mac_addrs = RTE_DIM(priv->mac);
	info->rx_offload_capa =
		(priv->hw_csum ?
		 (DEV_RX_OFFLOAD_IPV4_CKSUM |
		  DEV_RX_OFFLOAD_UDP_CKSUM |
		  DEV_RX_OFFLOAD_TCP_CKSUM) :
		 0) |
		(priv->hw_vlan_strip ? DEV_RX_OFFLOAD_VLAN_STRIP : 0);
	if (!priv->mps)
		info->tx_offload_capa = DEV_TX_OFFLOAD_VLAN_INSERT;
	if (priv->hw_csum)
		info->tx_offload_capa |=
			(DEV_TX_OFFLOAD_IPV4_CKSUM |
			 DEV_TX_OFFLOAD_UDP_CKSUM |
			 DEV_TX_OFFLOAD_TCP_CKSUM);
	if (priv->tso)
		info->tx_offload_capa |= DEV_TX_OFFLOAD_TCP_TSO;
	if (priv->tunnel_en)
		info->tx_offload_capa |= (DEV_TX_OFFLOAD_OUTER_IPV4_CKSUM |
					  DEV_TX_OFFLOAD_VXLAN_TNL_TSO |
					  DEV_TX_OFFLOAD_GRE_TNL_TSO);
	if (priv_get_ifname(priv, &ifname) == 0)
		info->if_index = if_nametoindex(ifname);
	info->reta_size = priv->reta_idx_n ?
		priv->reta_idx_n : priv->ind_table_max_size;
	info->hash_key_size = ((*priv->rss_conf) ?
			       (*priv->rss_conf)[0]->rss_key_len :
			       0);
	info->speed_capa = priv->link_speed_capa;
	priv_unlock(priv);
}

const uint32_t *
mlx5_dev_supported_ptypes_get(struct rte_eth_dev *dev)
{
	static const uint32_t ptypes[] = {
		/* refers to rxq_cq_to_pkt_type() */
		RTE_PTYPE_L3_IPV4_EXT_UNKNOWN,
		RTE_PTYPE_L3_IPV6_EXT_UNKNOWN,
		RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN,
		RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN,
		RTE_PTYPE_UNKNOWN

	};

	if (dev->rx_pkt_burst == mlx5_rx_burst)
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
	struct priv *priv = mlx5_get_priv(dev);
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
	struct priv *priv = mlx5_get_priv(dev);
	struct ethtool_link_settings edata = {
		.cmd = ETHTOOL_GLINKSETTINGS,
	};
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
	ifr.ifr_data = (void *)&edata;
	if (priv_ifreq(priv, SIOCETHTOOL, &ifr)) {
		DEBUG("ioctl(SIOCETHTOOL, ETHTOOL_GLINKSETTINGS) failed: %s",
		      strerror(errno));
		return -1;
	}
	dev_link.link_speed = edata.speed;
	sc = edata.link_mode_masks[0] |
		((uint64_t)edata.link_mode_masks[1] << 32);
	priv->link_speed_capa = 0;
	if (sc & ETHTOOL_LINK_MODE_Autoneg_BIT)
		priv->link_speed_capa |= ETH_LINK_SPEED_AUTONEG;
	if (sc & (ETHTOOL_LINK_MODE_1000baseT_Full_BIT |
		  ETHTOOL_LINK_MODE_1000baseKX_Full_BIT))
		priv->link_speed_capa |= ETH_LINK_SPEED_1G;
	if (sc & (ETHTOOL_LINK_MODE_10000baseKX4_Full_BIT |
		  ETHTOOL_LINK_MODE_10000baseKR_Full_BIT |
		  ETHTOOL_LINK_MODE_10000baseR_FEC_BIT))
		priv->link_speed_capa |= ETH_LINK_SPEED_10G;
	if (sc & (ETHTOOL_LINK_MODE_20000baseMLD2_Full_BIT |
		  ETHTOOL_LINK_MODE_20000baseKR2_Full_BIT))
		priv->link_speed_capa |= ETH_LINK_SPEED_20G;
	if (sc & (ETHTOOL_LINK_MODE_40000baseKR4_Full_BIT |
		  ETHTOOL_LINK_MODE_40000baseCR4_Full_BIT |
		  ETHTOOL_LINK_MODE_40000baseSR4_Full_BIT |
		  ETHTOOL_LINK_MODE_40000baseLR4_Full_BIT))
		priv->link_speed_capa |= ETH_LINK_SPEED_40G;
	if (sc & (ETHTOOL_LINK_MODE_56000baseKR4_Full_BIT |
		  ETHTOOL_LINK_MODE_56000baseCR4_Full_BIT |
		  ETHTOOL_LINK_MODE_56000baseSR4_Full_BIT |
		  ETHTOOL_LINK_MODE_56000baseLR4_Full_BIT))
		priv->link_speed_capa |= ETH_LINK_SPEED_56G;
	if (sc & (ETHTOOL_LINK_MODE_25000baseCR_Full_BIT |
		  ETHTOOL_LINK_MODE_25000baseKR_Full_BIT |
		  ETHTOOL_LINK_MODE_25000baseSR_Full_BIT))
		priv->link_speed_capa |= ETH_LINK_SPEED_25G;
	if (sc & (ETHTOOL_LINK_MODE_50000baseCR2_Full_BIT |
		  ETHTOOL_LINK_MODE_50000baseKR2_Full_BIT))
		priv->link_speed_capa |= ETH_LINK_SPEED_50G;
	if (sc & (ETHTOOL_LINK_MODE_100000baseKR4_Full_BIT |
		  ETHTOOL_LINK_MODE_100000baseSR4_Full_BIT |
		  ETHTOOL_LINK_MODE_100000baseCR4_Full_BIT |
		  ETHTOOL_LINK_MODE_100000baseLR4_ER4_Full_BIT))
		priv->link_speed_capa |= ETH_LINK_SPEED_100G;
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
	struct utsname utsname;
	int ver[3];

	if (uname(&utsname) == -1 ||
	    sscanf(utsname.release, "%d.%d.%d",
		   &ver[0], &ver[1], &ver[2]) != 3 ||
	    KERNEL_VERSION(ver[0], ver[1], ver[2]) < KERNEL_VERSION(4, 9, 0))
		return mlx5_link_update_unlocked_gset(dev, wait_to_complete);
	return mlx5_link_update_unlocked_gs(dev, wait_to_complete);
}

/**
 * DPDK callback to change the MTU.
 *
 * Setting the MTU affects hardware MRU (packets larger than the MTU cannot be
 * received). Use this as a hint to enable/disable scattered packets support
 * and improve performance when not needed.
 * Since failure is not an option, reconfiguring queues on the fly is not
 * recommended.
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
	int ret = 0;
	unsigned int i;
	uint16_t (*rx_func)(void *, struct rte_mbuf **, uint16_t) =
		mlx5_rx_burst;
	unsigned int max_frame_len;
	int rehash;
	int restart = priv->started;

	if (mlx5_is_secondary())
		return -E_RTE_SECONDARY;

	priv_lock(priv);
	/* Set kernel interface MTU first. */
	if (priv_set_mtu(priv, mtu)) {
		ret = errno;
		WARN("cannot set port %u MTU to %u: %s", priv->port, mtu,
		     strerror(ret));
		goto out;
	} else
		DEBUG("adapter port %u MTU set to %u", priv->port, mtu);
	/* Temporarily replace RX handler with a fake one, assuming it has not
	 * been copied elsewhere. */
	dev->rx_pkt_burst = removed_rx_burst;
	/* Make sure everyone has left mlx5_rx_burst() and uses
	 * removed_rx_burst() instead. */
	rte_wmb();
	usleep(1000);
	/* MTU does not include header and CRC. */
	max_frame_len = ETHER_HDR_LEN + mtu + ETHER_CRC_LEN;
	/* Check if at least one queue is going to need a SGE update. */
	for (i = 0; i != priv->rxqs_n; ++i) {
		struct rxq *rxq = (*priv->rxqs)[i];
		unsigned int mb_len;
		unsigned int size = RTE_PKTMBUF_HEADROOM + max_frame_len;
		unsigned int sges_n;

		if (rxq == NULL)
			continue;
		mb_len = rte_pktmbuf_data_room_size(rxq->mp);
		assert(mb_len >= RTE_PKTMBUF_HEADROOM);
		/*
		 * Determine the number of SGEs needed for a full packet
		 * and round it to the next power of two.
		 */
		sges_n = log2above((size / mb_len) + !!(size % mb_len));
		if (sges_n != rxq->sges_n)
			break;
	}
	/*
	 * If all queues have the right number of SGEs, a simple rehash
	 * of their buffers is enough, otherwise SGE information can only
	 * be updated in a queue by recreating it. All resources that depend
	 * on queues (flows, indirection tables) must be recreated as well in
	 * that case.
	 */
	rehash = (i == priv->rxqs_n);
	if (!rehash) {
		/* Clean up everything as with mlx5_dev_stop(). */
		priv_special_flow_disable_all(priv);
		priv_mac_addrs_disable(priv);
		priv_destroy_hash_rxqs(priv);
		priv_fdir_disable(priv);
		priv_dev_interrupt_handler_uninstall(priv, dev);
	}
recover:
	/* Reconfigure each RX queue. */
	for (i = 0; (i != priv->rxqs_n); ++i) {
		struct rxq *rxq = (*priv->rxqs)[i];
		struct rxq_ctrl *rxq_ctrl =
			container_of(rxq, struct rxq_ctrl, rxq);
		unsigned int mb_len;
		unsigned int tmp;

		if (rxq == NULL)
			continue;
		mb_len = rte_pktmbuf_data_room_size(rxq->mp);
		assert(mb_len >= RTE_PKTMBUF_HEADROOM);
		/* Provide new values to rxq_setup(). */
		dev->data->dev_conf.rxmode.jumbo_frame =
			(max_frame_len > ETHER_MAX_LEN);
		dev->data->dev_conf.rxmode.max_rx_pkt_len = max_frame_len;
		if (rehash)
			ret = rxq_rehash(dev, rxq_ctrl);
		else
			ret = rxq_ctrl_setup(dev, rxq_ctrl, 1 << rxq->elts_n,
					     rxq_ctrl->socket, NULL, rxq->mp);
		if (!ret)
			continue;
		/* Attempt to roll back in case of error. */
		tmp = (mb_len << rxq->sges_n) - RTE_PKTMBUF_HEADROOM;
		if (max_frame_len != tmp) {
			max_frame_len = tmp;
			goto recover;
		}
		/* Double fault, disable RX. */
		break;
	}
	/*
	 * Use a safe RX burst function in case of error, otherwise mimic
	 * mlx5_dev_start().
	 */
	if (ret) {
		ERROR("unable to reconfigure RX queues, RX disabled");
		rx_func = removed_rx_burst;
	} else if (restart &&
		 !rehash &&
		 !priv_create_hash_rxqs(priv) &&
		 !priv_rehash_flows(priv)) {
		if (dev->data->dev_conf.fdir_conf.mode == RTE_FDIR_MODE_NONE)
			priv_fdir_enable(priv);
		priv_dev_interrupt_handler_install(priv, dev);
	}
	priv->mtu = mtu;
	/* Burst functions can now be called again. */
	rte_wmb();
	dev->rx_pkt_burst = rx_func;
out:
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

	if (mlx5_is_secondary())
		return -E_RTE_SECONDARY;

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

	if (mlx5_is_secondary())
		return -E_RTE_SECONDARY;

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
			   "%" SCNx16 ":%" SCNx8 ":%" SCNx8 ".%" SCNx8 "\n",
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
 * Link status handler.
 *
 * @param priv
 *   Pointer to private structure.
 * @param dev
 *   Pointer to the rte_eth_dev structure.
 *
 * @return
 *   Nonzero if the callback process can be called immediately.
 */
static int
priv_dev_link_status_handler(struct priv *priv, struct rte_eth_dev *dev)
{
	struct ibv_async_event event;
	struct rte_eth_link *link = &dev->data->dev_link;
	int ret = 0;

	/* Read all message and acknowledge them. */
	for (;;) {
		if (ibv_get_async_event(priv->ctx, &event))
			break;

		if (event.event_type != IBV_EVENT_PORT_ACTIVE &&
		    event.event_type != IBV_EVENT_PORT_ERR)
			DEBUG("event type %d on port %d not handled",
			      event.event_type, event.element.port_num);
		ibv_ack_async_event(&event);
	}
	mlx5_link_update(dev, 0);
	if (((link->link_speed == 0) && link->link_status) ||
	    ((link->link_speed != 0) && !link->link_status)) {
		if (!priv->pending_alarm) {
			/* Inconsistent status, check again later. */
			priv->pending_alarm = 1;
			rte_eal_alarm_set(MLX5_ALARM_TIMEOUT_US,
					  mlx5_dev_link_status_handler,
					  dev);
		}
	} else {
		ret = 1;
	}
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

	priv_lock(priv);
	assert(priv->pending_alarm == 1);
	priv->pending_alarm = 0;
	ret = priv_dev_link_status_handler(priv, dev);
	priv_unlock(priv);
	if (ret)
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
	int ret;

	priv_lock(priv);
	ret = priv_dev_link_status_handler(priv, dev);
	priv_unlock(priv);
	if (ret)
		_rte_eth_dev_callback_process(dev, RTE_ETH_EVENT_INTR_LSC, NULL);
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
	if (!dev->data->dev_conf.intr_conf.lsc)
		return;
	rte_intr_callback_unregister(&priv->intr_handle,
				     mlx5_dev_interrupt_handler,
				     dev);
	if (priv->pending_alarm)
		rte_eal_alarm_cancel(mlx5_dev_link_status_handler, dev);
	priv->pending_alarm = 0;
	priv->intr_handle.fd = 0;
	priv->intr_handle.type = RTE_INTR_HANDLE_UNKNOWN;
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

	if (!dev->data->dev_conf.intr_conf.lsc)
		return;
	assert(priv->ctx->async_fd > 0);
	flags = fcntl(priv->ctx->async_fd, F_GETFL);
	rc = fcntl(priv->ctx->async_fd, F_SETFL, flags | O_NONBLOCK);
	if (rc < 0) {
		INFO("failed to change file descriptor async event queue");
		dev->data->dev_conf.intr_conf.lsc = 0;
	} else {
		priv->intr_handle.fd = priv->ctx->async_fd;
		priv->intr_handle.type = RTE_INTR_HANDLE_EXT;
		rte_intr_callback_register(&priv->intr_handle,
					   mlx5_dev_interrupt_handler,
					   dev);
	}
}

/**
 * Change the link state (UP / DOWN).
 *
 * @param priv
 *   Pointer to Ethernet device structure.
 * @param up
 *   Nonzero for link up, otherwise link down.
 *
 * @return
 *   0 on success, errno value on failure.
 */
static int
priv_set_link(struct priv *priv, int up)
{
	struct rte_eth_dev *dev = priv->dev;
	int err;

	if (up) {
		err = priv_set_flags(priv, ~IFF_UP, IFF_UP);
		if (err)
			return err;
		priv_select_tx_function(priv);
		priv_select_rx_function(priv);
	} else {
		err = priv_set_flags(priv, ~IFF_UP, ~IFF_UP);
		if (err)
			return err;
		dev->rx_pkt_burst = removed_rx_burst;
		dev->tx_pkt_burst = removed_tx_burst;
	}
	return 0;
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
	err = priv_set_link(priv, 0);
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
	err = priv_set_link(priv, 1);
	priv_unlock(priv);
	return err;
}

/**
 * Configure secondary process queues from a private data pointer (primary
 * or secondary) and update burst callbacks. Can take place only once.
 *
 * All queues must have been previously created by the primary process to
 * avoid undefined behavior.
 *
 * @param priv
 *   Private data pointer from either primary or secondary process.
 *
 * @return
 *   Private data pointer from secondary process, NULL in case of error.
 */
struct priv *
mlx5_secondary_data_setup(struct priv *priv)
{
	unsigned int port_id = 0;
	struct mlx5_secondary_data *sd;
	void **tx_queues;
	void **rx_queues;
	unsigned int nb_tx_queues;
	unsigned int nb_rx_queues;
	unsigned int i;

	/* priv must be valid at this point. */
	assert(priv != NULL);
	/* priv->dev must also be valid but may point to local memory from
	 * another process, possibly with the same address and must not
	 * be dereferenced yet. */
	assert(priv->dev != NULL);
	/* Determine port ID by finding out where priv comes from. */
	while (1) {
		sd = &mlx5_secondary_data[port_id];
		rte_spinlock_lock(&sd->lock);
		/* Primary process? */
		if (sd->primary_priv == priv)
			break;
		/* Secondary process? */
		if (sd->data.dev_private == priv)
			break;
		rte_spinlock_unlock(&sd->lock);
		if (++port_id == RTE_DIM(mlx5_secondary_data))
			port_id = 0;
	}
	/* Switch to secondary private structure. If private data has already
	 * been updated by another thread, there is nothing else to do. */
	priv = sd->data.dev_private;
	if (priv->dev->data == &sd->data)
		goto end;
	/* Sanity checks. Secondary private structure is supposed to point
	 * to local eth_dev, itself still pointing to the shared device data
	 * structure allocated by the primary process. */
	assert(sd->shared_dev_data != &sd->data);
	assert(sd->data.nb_tx_queues == 0);
	assert(sd->data.tx_queues == NULL);
	assert(sd->data.nb_rx_queues == 0);
	assert(sd->data.rx_queues == NULL);
	assert(priv != sd->primary_priv);
	assert(priv->dev->data == sd->shared_dev_data);
	assert(priv->txqs_n == 0);
	assert(priv->txqs == NULL);
	assert(priv->rxqs_n == 0);
	assert(priv->rxqs == NULL);
	nb_tx_queues = sd->shared_dev_data->nb_tx_queues;
	nb_rx_queues = sd->shared_dev_data->nb_rx_queues;
	/* Allocate local storage for queues. */
	tx_queues = rte_zmalloc("secondary ethdev->tx_queues",
				sizeof(sd->data.tx_queues[0]) * nb_tx_queues,
				RTE_CACHE_LINE_SIZE);
	rx_queues = rte_zmalloc("secondary ethdev->rx_queues",
				sizeof(sd->data.rx_queues[0]) * nb_rx_queues,
				RTE_CACHE_LINE_SIZE);
	if (tx_queues == NULL || rx_queues == NULL)
		goto error;
	/* Lock to prevent control operations during setup. */
	priv_lock(priv);
	/* TX queues. */
	for (i = 0; i != nb_tx_queues; ++i) {
		struct txq *primary_txq = (*sd->primary_priv->txqs)[i];
		struct txq_ctrl *primary_txq_ctrl;
		struct txq_ctrl *txq_ctrl;

		if (primary_txq == NULL)
			continue;
		primary_txq_ctrl = container_of(primary_txq,
						struct txq_ctrl, txq);
		txq_ctrl = rte_calloc_socket("TXQ", 1, sizeof(*txq_ctrl) +
					     (1 << primary_txq->elts_n) *
					     sizeof(struct rte_mbuf *), 0,
					     primary_txq_ctrl->socket);
		if (txq_ctrl != NULL) {
			if (txq_ctrl_setup(priv->dev,
					   txq_ctrl,
					   1 << primary_txq->elts_n,
					   primary_txq_ctrl->socket,
					   NULL) == 0) {
				txq_ctrl->txq.stats.idx =
					primary_txq->stats.idx;
				tx_queues[i] = &txq_ctrl->txq;
				continue;
			}
			rte_free(txq_ctrl);
		}
		while (i) {
			txq_ctrl = tx_queues[--i];
			txq_cleanup(txq_ctrl);
			rte_free(txq_ctrl);
		}
		goto error;
	}
	/* RX queues. */
	for (i = 0; i != nb_rx_queues; ++i) {
		struct rxq_ctrl *primary_rxq =
			container_of((*sd->primary_priv->rxqs)[i],
				     struct rxq_ctrl, rxq);

		if (primary_rxq == NULL)
			continue;
		/* Not supported yet. */
		rx_queues[i] = NULL;
	}
	/* Update everything. */
	priv->txqs = (void *)tx_queues;
	priv->txqs_n = nb_tx_queues;
	priv->rxqs = (void *)rx_queues;
	priv->rxqs_n = nb_rx_queues;
	sd->data.rx_queues = rx_queues;
	sd->data.tx_queues = tx_queues;
	sd->data.nb_rx_queues = nb_rx_queues;
	sd->data.nb_tx_queues = nb_tx_queues;
	sd->data.dev_link = sd->shared_dev_data->dev_link;
	sd->data.mtu = sd->shared_dev_data->mtu;
	memcpy(sd->data.rx_queue_state, sd->shared_dev_data->rx_queue_state,
	       sizeof(sd->data.rx_queue_state));
	memcpy(sd->data.tx_queue_state, sd->shared_dev_data->tx_queue_state,
	       sizeof(sd->data.tx_queue_state));
	sd->data.dev_flags = sd->shared_dev_data->dev_flags;
	/* Use local data from now on. */
	rte_mb();
	priv->dev->data = &sd->data;
	rte_mb();
	priv_select_tx_function(priv);
	priv_select_rx_function(priv);
	priv_unlock(priv);
end:
	/* More sanity checks. */
	assert(priv->dev->data == &sd->data);
	rte_spinlock_unlock(&sd->lock);
	return priv;
error:
	priv_unlock(priv);
	rte_free(tx_queues);
	rte_free(rx_queues);
	rte_spinlock_unlock(&sd->lock);
	return NULL;
}

/**
 * Configure the TX function to use.
 *
 * @param priv
 *   Pointer to private structure.
 */
void
priv_select_tx_function(struct priv *priv)
{
	priv->dev->tx_pkt_burst = mlx5_tx_burst;
	/* Select appropriate TX function. */
	if (priv->mps == MLX5_MPW_ENHANCED) {
		priv->dev->tx_pkt_burst =
			mlx5_tx_burst_empw;
		DEBUG("selected Enhanced MPW TX function");
	} else if (priv->mps && priv->txq_inline) {
		priv->dev->tx_pkt_burst = mlx5_tx_burst_mpw_inline;
		DEBUG("selected MPW inline TX function");
	} else if (priv->mps) {
		priv->dev->tx_pkt_burst = mlx5_tx_burst_mpw;
		DEBUG("selected MPW TX function");
	}
}

/**
 * Configure the RX function to use.
 *
 * @param priv
 *   Pointer to private structure.
 */
void
priv_select_rx_function(struct priv *priv)
{
	priv->dev->rx_pkt_burst = mlx5_rx_burst;
}
