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

#ifndef RTE_PMD_MLX5_H_
#define RTE_PMD_MLX5_H_

#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include <net/if.h>
#include <netinet/in.h>
#include <linux/if.h>

/* Verbs header. */
/* ISO C doesn't support unnamed structs/unions, disabling -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-pedantic"
#endif
#include <infiniband/verbs.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-pedantic"
#endif

/* DPDK headers don't like -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-pedantic"
#endif
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_spinlock.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-pedantic"
#endif

#include "mlx5_utils.h"
#include "mlx5_rxtx.h"
#include "mlx5_autoconf.h"
#include "mlx5_defs.h"

enum {
	PCI_VENDOR_ID_MELLANOX = 0x15b3,
};

enum {
	PCI_DEVICE_ID_MELLANOX_CONNECTX4 = 0x1013,
	PCI_DEVICE_ID_MELLANOX_CONNECTX4VF = 0x1014,
	PCI_DEVICE_ID_MELLANOX_CONNECTX4LX = 0x1015,
	PCI_DEVICE_ID_MELLANOX_CONNECTX4LXVF = 0x1016,
};

struct priv {
	struct rte_eth_dev *dev; /* Ethernet device. */
	struct ibv_context *ctx; /* Verbs context. */
	struct ibv_device_attr device_attr; /* Device properties. */
	struct ibv_pd *pd; /* Protection Domain. */
	/*
	 * MAC addresses array and configuration bit-field.
	 * An extra entry that cannot be modified by the DPDK is reserved
	 * for broadcast frames (destination MAC address ff:ff:ff:ff:ff:ff).
	 */
	struct ether_addr mac[MLX5_MAX_MAC_ADDRESSES];
	BITFIELD_DECLARE(mac_configured, uint32_t, MLX5_MAX_MAC_ADDRESSES);
	/* Device properties. */
	uint16_t mtu; /* Configured MTU. */
	uint8_t port; /* Physical port number. */
	unsigned int started:1; /* Device started, flows enabled. */
	unsigned int hw_qpg:1; /* QP groups are supported. */
	unsigned int hw_tss:1; /* TSS is supported. */
	unsigned int hw_rss:1; /* RSS is supported. */
	unsigned int hw_csum:1; /* Checksum offload is supported. */
	unsigned int hw_csum_l2tun:1; /* Same for L2 tunnels. */
	unsigned int rss:1; /* RSS is enabled. */
	unsigned int vf:1; /* This is a VF device. */
	unsigned int max_rss_tbl_sz; /* Maximum number of RSS queues. */
	/* RX/TX queues. */
	struct rxq rxq_parent; /* Parent queue when RSS is enabled. */
	unsigned int rxqs_n; /* RX queues array size. */
	unsigned int txqs_n; /* TX queues array size. */
	struct rxq *(*rxqs)[]; /* RX queues. */
	struct txq *(*txqs)[]; /* TX queues. */
	rte_spinlock_t lock; /* Lock for control functions. */
};

/* Work Request ID data type (64 bit). */
typedef union {
	struct {
		uint32_t id;
		uint16_t offset;
	} data;
	uint64_t raw;
} wr_id_t;

/* Compile-time check. */
static inline void wr_id_t_check(void)
{
	wr_id_t check[1 + (2 * -!(sizeof(wr_id_t) == sizeof(uint64_t)))];

	(void)check;
	(void)wr_id_t_check;
}

/**
 * Lock private structure to protect it from concurrent access in the
 * control path.
 *
 * @param priv
 *   Pointer to private structure.
 */
static inline void
priv_lock(struct priv *priv)
{
	rte_spinlock_lock(&priv->lock);
}

/**
 * Unlock private structure.
 *
 * @param priv
 *   Pointer to private structure.
 */
static inline void
priv_unlock(struct priv *priv)
{
	rte_spinlock_unlock(&priv->lock);
}

/* mlx5_ethdev.c */

int priv_get_ifname(const struct priv *, char (*)[IF_NAMESIZE]);
int priv_ifreq(const struct priv *, int req, struct ifreq *);
int priv_get_mtu(struct priv *, uint16_t *);
int priv_set_flags(struct priv *, unsigned int, unsigned int);
int mlx5_dev_configure(struct rte_eth_dev *);
void mlx5_dev_infos_get(struct rte_eth_dev *, struct rte_eth_dev_info *);
int mlx5_dev_set_mtu(struct rte_eth_dev *, uint16_t);
int mlx5_ibv_device_to_pci_addr(const struct ibv_device *,
				struct rte_pci_addr *);

/* mlx5_mac.c */

int priv_get_mac(struct priv *, uint8_t (*)[ETHER_ADDR_LEN]);
void rxq_mac_addrs_del(struct rxq *);
void mlx5_mac_addr_remove(struct rte_eth_dev *, uint32_t);
int rxq_mac_addrs_add(struct rxq *);
int priv_mac_addr_add(struct priv *, unsigned int,
		      const uint8_t (*)[ETHER_ADDR_LEN]);
void mlx5_mac_addr_add(struct rte_eth_dev *, struct ether_addr *, uint32_t,
		       uint32_t);

/* mlx5_stats.c */

void mlx5_stats_get(struct rte_eth_dev *, struct rte_eth_stats *);
void mlx5_stats_reset(struct rte_eth_dev *);

/* mlx5_trigger.c */

int mlx5_dev_start(struct rte_eth_dev *);
void mlx5_dev_stop(struct rte_eth_dev *);

#endif /* RTE_PMD_MLX5_H_ */
