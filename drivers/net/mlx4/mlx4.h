/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2012 6WIND S.A.
 * Copyright 2012 Mellanox Technologies, Ltd
 */

#ifndef RTE_PMD_MLX4_H_
#define RTE_PMD_MLX4_H_

#include <net/if.h>
#include <stdint.h>
#include <sys/queue.h>

/* Verbs headers do not support -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <infiniband/verbs.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-Wpedantic"
#endif

#include <rte_ethdev_driver.h>
#include <rte_ether.h>
#include <rte_interrupts.h>
#include <rte_mempool.h>
#include <rte_spinlock.h>

#ifndef IBV_RX_HASH_INNER
/** This is not necessarily defined by supported RDMA core versions. */
#define IBV_RX_HASH_INNER (1ull << 31)
#endif /* IBV_RX_HASH_INNER */

/** Maximum number of simultaneous MAC addresses. This value is arbitrary. */
#define MLX4_MAX_MAC_ADDRESSES 128

/** Request send completion once in every 64 sends, might be less. */
#define MLX4_PMD_TX_PER_COMP_REQ 64

/** Maximum size for inline data. */
#define MLX4_PMD_MAX_INLINE 0

/** Fixed RSS hash key size in bytes. Cannot be modified. */
#define MLX4_RSS_HASH_KEY_SIZE 40

/**
 * Maximum number of cached Memory Pools (MPs) per TX queue. Each RTE MP
 * from which buffers are to be transmitted will have to be mapped by this
 * driver to their own Memory Region (MR). This is a slow operation.
 *
 * This value is always 1 for RX queues.
 */
#ifndef MLX4_PMD_TX_MP_CACHE
#define MLX4_PMD_TX_MP_CACHE 8
#endif

/** Interrupt alarm timeout value in microseconds. */
#define MLX4_INTR_ALARM_TIMEOUT 100000

/** Port parameter. */
#define MLX4_PMD_PORT_KVARG "port"

enum {
	PCI_VENDOR_ID_MELLANOX = 0x15b3,
};

enum {
	PCI_DEVICE_ID_MELLANOX_CONNECTX3 = 0x1003,
	PCI_DEVICE_ID_MELLANOX_CONNECTX3VF = 0x1004,
	PCI_DEVICE_ID_MELLANOX_CONNECTX3PRO = 0x1007,
};

/** Driver name reported to lower layers and used in log output. */
#define MLX4_DRIVER_NAME "net_mlx4"

struct mlx4_drop;
struct mlx4_rss;
struct rxq;
struct txq;
struct rte_flow;

/** Memory region descriptor. */
struct mlx4_mr {
	LIST_ENTRY(mlx4_mr) next; /**< Next entry in list. */
	uintptr_t start; /**< Base address for memory region. */
	uintptr_t end; /**< End address for memory region. */
	uint32_t lkey; /**< L_Key extracted from @p mr. */
	uint32_t refcnt; /**< Reference count for this object. */
	struct priv *priv; /**< Back pointer to private data. */
	struct ibv_mr *mr; /**< Memory region associated with @p mp. */
	struct rte_mempool *mp; /**< Target memory pool (mempool). */
};

/** Private data structure. */
struct priv {
	struct rte_eth_dev *dev; /**< Ethernet device. */
	struct ibv_context *ctx; /**< Verbs context. */
	struct ibv_device_attr device_attr; /**< Device properties. */
	struct ibv_pd *pd; /**< Protection Domain. */
	/* Device properties. */
	uint16_t mtu; /**< Configured MTU. */
	uint8_t port; /**< Physical port number. */
	uint32_t started:1; /**< Device started, flows enabled. */
	uint32_t vf:1; /**< This is a VF device. */
	uint32_t intr_alarm:1; /**< An interrupt alarm is scheduled. */
	uint32_t isolated:1; /**< Toggle isolated mode. */
	uint32_t hw_csum:1; /**< Checksum offload is supported. */
	uint32_t hw_csum_l2tun:1; /**< Checksum support for L2 tunnels. */
	uint32_t hw_fcs_strip:1; /**< FCS stripping toggling is supported. */
	uint64_t hw_rss_sup; /**< Supported RSS hash fields (Verbs format). */
	struct rte_intr_handle intr_handle; /**< Port interrupt handle. */
	struct mlx4_drop *drop; /**< Shared resources for drop flow rules. */
	LIST_HEAD(, mlx4_rss) rss; /**< Shared targets for Rx flow rules. */
	LIST_HEAD(, rte_flow) flows; /**< Configured flow rule handles. */
	LIST_HEAD(, mlx4_mr) mr; /**< Registered memory regions. */
	rte_spinlock_t mr_lock; /**< Lock for @p mr access. */
	struct ether_addr mac[MLX4_MAX_MAC_ADDRESSES];
	/**< Configured MAC addresses. Unused entries are zeroed. */
};

/* mlx4_ethdev.c */

int mlx4_get_ifname(const struct priv *priv, char (*ifname)[IF_NAMESIZE]);
int mlx4_get_mac(struct priv *priv, uint8_t (*mac)[ETHER_ADDR_LEN]);
int mlx4_mtu_get(struct priv *priv, uint16_t *mtu);
int mlx4_mtu_set(struct rte_eth_dev *dev, uint16_t mtu);
int mlx4_dev_set_link_down(struct rte_eth_dev *dev);
int mlx4_dev_set_link_up(struct rte_eth_dev *dev);
void mlx4_promiscuous_enable(struct rte_eth_dev *dev);
void mlx4_promiscuous_disable(struct rte_eth_dev *dev);
void mlx4_allmulticast_enable(struct rte_eth_dev *dev);
void mlx4_allmulticast_disable(struct rte_eth_dev *dev);
void mlx4_mac_addr_remove(struct rte_eth_dev *dev, uint32_t index);
int mlx4_mac_addr_add(struct rte_eth_dev *dev, struct ether_addr *mac_addr,
		      uint32_t index, uint32_t vmdq);
int mlx4_mac_addr_set(struct rte_eth_dev *dev, struct ether_addr *mac_addr);
int mlx4_vlan_filter_set(struct rte_eth_dev *dev, uint16_t vlan_id, int on);
int mlx4_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *stats);
void mlx4_stats_reset(struct rte_eth_dev *dev);
void mlx4_dev_infos_get(struct rte_eth_dev *dev,
			struct rte_eth_dev_info *info);
int mlx4_link_update(struct rte_eth_dev *dev, int wait_to_complete);
int mlx4_flow_ctrl_get(struct rte_eth_dev *dev,
		       struct rte_eth_fc_conf *fc_conf);
int mlx4_flow_ctrl_set(struct rte_eth_dev *dev,
		       struct rte_eth_fc_conf *fc_conf);
const uint32_t *mlx4_dev_supported_ptypes_get(struct rte_eth_dev *dev);
int mlx4_is_removed(struct rte_eth_dev *dev);

/* mlx4_intr.c */

int mlx4_intr_uninstall(struct priv *priv);
int mlx4_intr_install(struct priv *priv);
int mlx4_rxq_intr_enable(struct priv *priv);
void mlx4_rxq_intr_disable(struct priv *priv);
int mlx4_rx_intr_disable(struct rte_eth_dev *dev, uint16_t idx);
int mlx4_rx_intr_enable(struct rte_eth_dev *dev, uint16_t idx);

/* mlx4_mr.c */

struct mlx4_mr *mlx4_mr_get(struct priv *priv, struct rte_mempool *mp);
void mlx4_mr_put(struct mlx4_mr *mr);
uint32_t mlx4_txq_add_mr(struct txq *txq, struct rte_mempool *mp,
			 uint32_t i);

#endif /* RTE_PMD_MLX4_H_ */
