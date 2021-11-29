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

#include <ethdev_driver.h>
#include <rte_ether.h>
#include <rte_interrupts.h>
#include <rte_mempool.h>
#include <rte_rwlock.h>

#include "mlx4_mr.h"

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

/** Interrupt alarm timeout value in microseconds. */
#define MLX4_INTR_ALARM_TIMEOUT 100000

/* Maximum packet headers size (L2+L3+L4) for TSO. */
#define MLX4_MAX_TSO_HEADER 192

/** Port parameter. */
#define MLX4_PMD_PORT_KVARG "port"

/** Enable extending memsegs when creating a MR. */
#define MLX4_MR_EXT_MEMSEG_EN_KVARG "mr_ext_memseg_en"

enum {
	PCI_VENDOR_ID_MELLANOX = 0x15b3,
};

enum {
	PCI_DEVICE_ID_MELLANOX_CONNECTX3 = 0x1003,
	PCI_DEVICE_ID_MELLANOX_CONNECTX3VF = 0x1004,
	PCI_DEVICE_ID_MELLANOX_CONNECTX3PRO = 0x1007,
};

/* Request types for IPC. */
enum mlx4_mp_req_type {
	MLX4_MP_REQ_VERBS_CMD_FD = 1,
	MLX4_MP_REQ_CREATE_MR,
	MLX4_MP_REQ_START_RXTX,
	MLX4_MP_REQ_STOP_RXTX,
};

/* Parameters for IPC. */
struct mlx4_mp_param {
	enum mlx4_mp_req_type type;
	int port_id;
	int result;
	RTE_STD_C11
	union {
		uintptr_t addr; /* MLX4_MP_REQ_CREATE_MR */
	} args;
};

/** Request timeout for IPC. */
#define MLX4_MP_REQ_TIMEOUT_SEC 5

/** Key string for IPC. */
#define MLX4_MP_NAME "net_mlx4_mp"

/** Driver name reported to lower layers and used in log output. */
#define MLX4_DRIVER_NAME "net_mlx4"

struct mlx4_drop;
struct mlx4_rss;
struct rxq;
struct txq;
struct rte_flow;

/**
 * Type of object being allocated.
 */
enum mlx4_verbs_alloc_type {
	MLX4_VERBS_ALLOC_TYPE_NONE,
	MLX4_VERBS_ALLOC_TYPE_TX_QUEUE,
	MLX4_VERBS_ALLOC_TYPE_RX_QUEUE,
};

/**
 * Verbs allocator needs a context to know in the callback which kind of
 * resources it is allocating.
 */
struct mlx4_verbs_alloc_ctx {
	int enabled;
	enum mlx4_verbs_alloc_type type; /* Kind of object being allocated. */
	const void *obj; /* Pointer to the DPDK object. */
};

LIST_HEAD(mlx4_dev_list, mlx4_priv);
LIST_HEAD(mlx4_mr_list, mlx4_mr);

/* Shared data between primary and secondary processes. */
struct mlx4_shared_data {
	rte_spinlock_t lock;
	/* Global spinlock for primary and secondary processes. */
	int init_done; /* Whether primary has done initialization. */
	unsigned int secondary_cnt; /* Number of secondary processes init'd. */
	struct mlx4_dev_list mem_event_cb_list;
	rte_rwlock_t mem_event_rwlock;
};

/* Per-process data structure, not visible to other processes. */
struct mlx4_local_data {
	int init_done; /* Whether a secondary has done initialization. */
};

extern struct mlx4_shared_data *mlx4_shared_data;

/* Per-process private structure. */
struct mlx4_proc_priv {
	size_t uar_table_sz;
	/* Size of UAR register table. */
	void *uar_table[];
	/* Table of UAR registers for each process. */
};

#define MLX4_PROC_PRIV(port_id) \
	((struct mlx4_proc_priv *)rte_eth_devices[port_id].process_private)

/** Private data structure. */
struct mlx4_priv {
	LIST_ENTRY(mlx4_priv) mem_event_cb;
	/**< Called by memory event callback. */
	struct rte_eth_dev_data *dev_data;  /* Pointer to device data. */
	struct ibv_context *ctx; /**< Verbs context. */
	struct ibv_device_attr device_attr; /**< Device properties. */
	struct ibv_pd *pd; /**< Protection Domain. */
	/* Device properties. */
	unsigned int if_index;	/**< Associated network device index */
	uint16_t mtu; /**< Configured MTU. */
	uint8_t port; /**< Physical port number. */
	uint32_t started:1; /**< Device started, flows enabled. */
	uint32_t vf:1; /**< This is a VF device. */
	uint32_t intr_alarm:1; /**< An interrupt alarm is scheduled. */
	uint32_t isolated:1; /**< Toggle isolated mode. */
	uint32_t rss_init:1; /**< Common RSS context is initialized. */
	uint32_t hw_csum:1; /**< Checksum offload is supported. */
	uint32_t hw_csum_l2tun:1; /**< Checksum support for L2 tunnels. */
	uint32_t hw_fcs_strip:1; /**< FCS stripping toggling is supported. */
	uint32_t tso:1; /**< Transmit segmentation offload is supported. */
	uint32_t mr_ext_memseg_en:1;
	/** Whether memseg should be extended for MR creation. */
	uint32_t tso_max_payload_sz; /**< Max supported TSO payload size. */
	uint32_t hw_rss_max_qps; /**< Max Rx Queues supported by RSS. */
	uint64_t hw_rss_sup; /**< Supported RSS hash fields (Verbs format). */
	struct rte_intr_handle *intr_handle; /**< Port interrupt handle. */
	struct mlx4_drop *drop; /**< Shared resources for drop flow rules. */
	struct {
		uint32_t dev_gen; /* Generation number to flush local caches. */
		rte_rwlock_t rwlock; /* MR Lock. */
		struct mlx4_mr_btree cache; /* Global MR cache table. */
		struct mlx4_mr_list mr_list; /* Registered MR list. */
		struct mlx4_mr_list mr_free_list; /* Freed MR list. */
	} mr;
	LIST_HEAD(, mlx4_rss) rss; /**< Shared targets for Rx flow rules. */
	LIST_HEAD(, rte_flow) flows; /**< Configured flow rule handles. */
	struct rte_ether_addr mac[MLX4_MAX_MAC_ADDRESSES];
	/**< Configured MAC addresses. Unused entries are zeroed. */
	uint32_t mac_mc; /**< Number of trailing multicast entries in mac[]. */
	struct mlx4_verbs_alloc_ctx verbs_alloc_ctx;
	/**< Context for Verbs allocator. */
};

#define PORT_ID(priv) ((priv)->dev_data->port_id)
#define ETH_DEV(priv) (&rte_eth_devices[PORT_ID(priv)])

int mlx4_proc_priv_init(struct rte_eth_dev *dev);
void mlx4_proc_priv_uninit(struct rte_eth_dev *dev);


/* mlx4_ethdev.c */

int mlx4_get_ifname(const struct mlx4_priv *priv, char (*ifname)[IF_NAMESIZE]);
int mlx4_get_mac(struct mlx4_priv *priv, uint8_t (*mac)[RTE_ETHER_ADDR_LEN]);
int mlx4_mtu_get(struct mlx4_priv *priv, uint16_t *mtu);
int mlx4_mtu_set(struct rte_eth_dev *dev, uint16_t mtu);
int mlx4_dev_set_link_down(struct rte_eth_dev *dev);
int mlx4_dev_set_link_up(struct rte_eth_dev *dev);
int mlx4_promiscuous_enable(struct rte_eth_dev *dev);
int mlx4_promiscuous_disable(struct rte_eth_dev *dev);
int mlx4_allmulticast_enable(struct rte_eth_dev *dev);
int mlx4_allmulticast_disable(struct rte_eth_dev *dev);
void mlx4_mac_addr_remove(struct rte_eth_dev *dev, uint32_t index);
int mlx4_mac_addr_add(struct rte_eth_dev *dev, struct rte_ether_addr *mac_addr,
		      uint32_t index, uint32_t vmdq);
int mlx4_mac_addr_set(struct rte_eth_dev *dev, struct rte_ether_addr *mac_addr);
int mlx4_set_mc_addr_list(struct rte_eth_dev *dev, struct rte_ether_addr *list,
			  uint32_t num);
int mlx4_vlan_filter_set(struct rte_eth_dev *dev, uint16_t vlan_id, int on);
int mlx4_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *stats);
int mlx4_stats_reset(struct rte_eth_dev *dev);
int mlx4_fw_version_get(struct rte_eth_dev *dev, char *fw_ver, size_t fw_size);
int mlx4_dev_infos_get(struct rte_eth_dev *dev,
		       struct rte_eth_dev_info *info);
int mlx4_link_update(struct rte_eth_dev *dev, int wait_to_complete);
int mlx4_flow_ctrl_get(struct rte_eth_dev *dev,
		       struct rte_eth_fc_conf *fc_conf);
int mlx4_flow_ctrl_set(struct rte_eth_dev *dev,
		       struct rte_eth_fc_conf *fc_conf);
const uint32_t *mlx4_dev_supported_ptypes_get(struct rte_eth_dev *dev);
int mlx4_is_removed(struct rte_eth_dev *dev);

/* mlx4_intr.c */

int mlx4_intr_uninstall(struct mlx4_priv *priv);
int mlx4_intr_install(struct mlx4_priv *priv);
int mlx4_rxq_intr_enable(struct mlx4_priv *priv);
void mlx4_rxq_intr_disable(struct mlx4_priv *priv);
int mlx4_rx_intr_disable(struct rte_eth_dev *dev, uint16_t idx);
int mlx4_rx_intr_enable(struct rte_eth_dev *dev, uint16_t idx);

/* mlx4_mp.c */
void mlx4_mp_req_start_rxtx(struct rte_eth_dev *dev);
void mlx4_mp_req_stop_rxtx(struct rte_eth_dev *dev);
int mlx4_mp_req_mr_create(struct rte_eth_dev *dev, uintptr_t addr);
int mlx4_mp_req_verbs_cmd_fd(struct rte_eth_dev *dev);
int mlx4_mp_init_primary(void);
void mlx4_mp_uninit_primary(void);
int mlx4_mp_init_secondary(void);
void mlx4_mp_uninit_secondary(void);

#endif /* RTE_PMD_MLX4_H_ */
