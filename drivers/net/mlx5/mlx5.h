/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2015 6WIND S.A.
 * Copyright 2015 Mellanox.
 */

#ifndef RTE_PMD_MLX5_H_
#define RTE_PMD_MLX5_H_

#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/queue.h>

/* Verbs header. */
/* ISO C doesn't support unnamed structs/unions, disabling -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <infiniband/verbs.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-Wpedantic"
#endif

#include <rte_pci.h>
#include <rte_ether.h>
#include <rte_ethdev_driver.h>
#include <rte_spinlock.h>
#include <rte_interrupts.h>
#include <rte_errno.h>
#include <rte_flow.h>

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
	PCI_DEVICE_ID_MELLANOX_CONNECTX5 = 0x1017,
	PCI_DEVICE_ID_MELLANOX_CONNECTX5VF = 0x1018,
	PCI_DEVICE_ID_MELLANOX_CONNECTX5EX = 0x1019,
	PCI_DEVICE_ID_MELLANOX_CONNECTX5EXVF = 0x101a,
};

struct mlx5_xstats_ctrl {
	/* Number of device stats. */
	uint16_t stats_n;
	/* Index in the device counters table. */
	uint16_t dev_table_idx[MLX5_MAX_XSTATS];
	uint64_t base[MLX5_MAX_XSTATS];
};

/* Flow list . */
TAILQ_HEAD(mlx5_flows, rte_flow);

/* Default PMD specific parameter value. */
#define MLX5_ARG_UNSET (-1)

/*
 * Device configuration structure.
 *
 * Merged configuration from:
 *
 *  - Device capabilities,
 *  - User device parameters disabled features.
 */
struct mlx5_dev_config {
	unsigned int hw_csum:1; /* Checksum offload is supported. */
	unsigned int hw_csum_l2tun:1; /* Same for L2 tunnels. */
	unsigned int hw_vlan_strip:1; /* VLAN stripping is supported. */
	unsigned int hw_fcs_strip:1; /* FCS stripping is supported. */
	unsigned int hw_padding:1; /* End alignment padding is supported. */
	unsigned int sriov:1; /* This is a VF or PF with VF devices. */
	unsigned int mps:2; /* Multi-packet send supported mode. */
	unsigned int tunnel_en:1; /* Whether tunnel is supported. */
	unsigned int flow_counter_en:1; /* Whether flow counter is supported. */
	unsigned int cqe_comp:1; /* CQE compression is enabled. */
	unsigned int tso:1; /* Whether TSO is supported. */
	unsigned int tx_vec_en:1; /* Tx vector is enabled. */
	unsigned int rx_vec_en:1; /* Rx vector is enabled. */
	unsigned int mpw_hdr_dseg:1; /* Enable DSEGs in the title WQEBB. */
	unsigned int tso_max_payload_sz; /* Maximum TCP payload for TSO. */
	unsigned int ind_table_max_size; /* Maximum indirection table size. */
	int txq_inline; /* Maximum packet size for inlining. */
	int txqs_inline; /* Queue number threshold for inlining. */
	int inline_max_packet_sz; /* Max packet size for inlining. */
};

/**
 * Type of objet being allocated.
 */
enum mlx5_verbs_alloc_type {
	MLX5_VERBS_ALLOC_TYPE_NONE,
	MLX5_VERBS_ALLOC_TYPE_TX_QUEUE,
	MLX5_VERBS_ALLOC_TYPE_RX_QUEUE,
};

/**
 * Verbs allocator needs a context to know in the callback which kind of
 * resources it is allocating.
 */
struct mlx5_verbs_alloc_ctx {
	enum mlx5_verbs_alloc_type type; /* Kind of object being allocated. */
	const void *obj; /* Pointer to the DPDK object. */
};

struct priv {
	struct rte_eth_dev *dev; /* Ethernet device of master process. */
	struct ibv_context *ctx; /* Verbs context. */
	struct ibv_device_attr_ex device_attr; /* Device properties. */
	struct ibv_pd *pd; /* Protection Domain. */
	char ibdev_path[IBV_SYSFS_PATH_MAX]; /* IB device path for secondary */
	struct ether_addr mac[MLX5_MAX_MAC_ADDRESSES]; /* MAC addresses. */
	uint16_t vlan_filter[MLX5_MAX_VLAN_IDS]; /* VLAN filters table. */
	unsigned int vlan_filter_n; /* Number of configured VLAN filters. */
	/* Device properties. */
	uint16_t mtu; /* Configured MTU. */
	uint8_t port; /* Physical port number. */
	unsigned int pending_alarm:1; /* An alarm is pending. */
	unsigned int isolated:1; /* Whether isolated mode is enabled. */
	/* RX/TX queues. */
	unsigned int rxqs_n; /* RX queues array size. */
	unsigned int txqs_n; /* TX queues array size. */
	struct mlx5_rxq_data *(*rxqs)[]; /* RX queues. */
	struct mlx5_txq_data *(*txqs)[]; /* TX queues. */
	struct rte_eth_rss_conf rss_conf; /* RSS configuration. */
	struct rte_intr_handle intr_handle; /* Interrupt handler. */
	unsigned int (*reta_idx)[]; /* RETA index table. */
	unsigned int reta_idx_n; /* RETA index size. */
	struct mlx5_hrxq_drop *flow_drop_queue; /* Flow drop queue. */
	struct mlx5_flows flows; /* RTE Flow rules. */
	struct mlx5_flows ctrl_flows; /* Control flow rules. */
	LIST_HEAD(mr, mlx5_mr) mr; /* Memory region. */
	LIST_HEAD(rxq, mlx5_rxq_ctrl) rxqsctrl; /* DPDK Rx queues. */
	LIST_HEAD(rxqibv, mlx5_rxq_ibv) rxqsibv; /* Verbs Rx queues. */
	LIST_HEAD(hrxq, mlx5_hrxq) hrxqs; /* Verbs Hash Rx queues. */
	LIST_HEAD(txq, mlx5_txq_ctrl) txqsctrl; /* DPDK Tx queues. */
	LIST_HEAD(txqibv, mlx5_txq_ibv) txqsibv; /* Verbs Tx queues. */
	/* Verbs Indirection tables. */
	LIST_HEAD(ind_tables, mlx5_ind_table_ibv) ind_tbls;
	uint32_t link_speed_capa; /* Link speed capabilities. */
	struct mlx5_xstats_ctrl xstats_ctrl; /* Extended stats control. */
	rte_spinlock_t lock; /* Lock for control functions. */
	int primary_socket; /* Unix socket for primary process. */
	void *uar_base; /* Reserved address space for UAR mapping */
	struct rte_intr_handle intr_handle_socket; /* Interrupt handler. */
	struct mlx5_dev_config config; /* Device configuration. */
	struct mlx5_verbs_alloc_ctx verbs_alloc_ctx;
	/* Context for Verbs allocator. */
};

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
 * Try to lock private structure to protect it from concurrent access in the
 * control path.
 *
 * @param priv
 *   Pointer to private structure.
 *
 * @return
 *   1 if the lock is successfully taken; 0 otherwise.
 */
static inline int
priv_trylock(struct priv *priv)
{
	return rte_spinlock_trylock(&priv->lock);
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

/* mlx5.c */

int mlx5_getenv_int(const char *);

/* mlx5_ethdev.c */

struct priv *mlx5_get_priv(struct rte_eth_dev *dev);
int mlx5_is_secondary(void);
int priv_get_ifname(const struct priv *, char (*)[IF_NAMESIZE]);
int priv_ifreq(const struct priv *, int req, struct ifreq *);
int priv_is_ib_cntr(const char *);
int priv_get_cntr_sysfs(struct priv *, const char *, uint64_t *);
int priv_get_num_vfs(struct priv *, uint16_t *);
int priv_get_mtu(struct priv *, uint16_t *);
int priv_set_flags(struct priv *, unsigned int, unsigned int);
int mlx5_dev_configure(struct rte_eth_dev *);
void mlx5_dev_infos_get(struct rte_eth_dev *, struct rte_eth_dev_info *);
const uint32_t *mlx5_dev_supported_ptypes_get(struct rte_eth_dev *dev);
int priv_link_update(struct priv *, int);
int priv_force_link_status_change(struct priv *, int);
int mlx5_link_update(struct rte_eth_dev *, int);
int mlx5_dev_set_mtu(struct rte_eth_dev *, uint16_t);
int mlx5_dev_get_flow_ctrl(struct rte_eth_dev *, struct rte_eth_fc_conf *);
int mlx5_dev_set_flow_ctrl(struct rte_eth_dev *, struct rte_eth_fc_conf *);
int mlx5_ibv_device_to_pci_addr(const struct ibv_device *,
				struct rte_pci_addr *);
void mlx5_dev_link_status_handler(void *);
void mlx5_dev_interrupt_handler(void *);
void priv_dev_interrupt_handler_uninstall(struct priv *, struct rte_eth_dev *);
void priv_dev_interrupt_handler_install(struct priv *, struct rte_eth_dev *);
int mlx5_set_link_down(struct rte_eth_dev *dev);
int mlx5_set_link_up(struct rte_eth_dev *dev);
int mlx5_is_removed(struct rte_eth_dev *dev);
eth_tx_burst_t priv_select_tx_function(struct priv *, struct rte_eth_dev *);
eth_rx_burst_t priv_select_rx_function(struct priv *, struct rte_eth_dev *);

/* mlx5_mac.c */

int priv_get_mac(struct priv *, uint8_t (*)[ETHER_ADDR_LEN]);
void mlx5_mac_addr_remove(struct rte_eth_dev *, uint32_t);
int mlx5_mac_addr_add(struct rte_eth_dev *, struct ether_addr *, uint32_t,
		      uint32_t);
void mlx5_mac_addr_set(struct rte_eth_dev *, struct ether_addr *);

/* mlx5_rss.c */

int mlx5_rss_hash_update(struct rte_eth_dev *, struct rte_eth_rss_conf *);
int mlx5_rss_hash_conf_get(struct rte_eth_dev *, struct rte_eth_rss_conf *);
int priv_rss_reta_index_resize(struct priv *, unsigned int);
int mlx5_dev_rss_reta_query(struct rte_eth_dev *,
			    struct rte_eth_rss_reta_entry64 *, uint16_t);
int mlx5_dev_rss_reta_update(struct rte_eth_dev *,
			     struct rte_eth_rss_reta_entry64 *, uint16_t);

/* mlx5_rxmode.c */

void mlx5_promiscuous_enable(struct rte_eth_dev *);
void mlx5_promiscuous_disable(struct rte_eth_dev *);
void mlx5_allmulticast_enable(struct rte_eth_dev *);
void mlx5_allmulticast_disable(struct rte_eth_dev *);

/* mlx5_stats.c */

void priv_xstats_init(struct priv *);
int mlx5_stats_get(struct rte_eth_dev *, struct rte_eth_stats *);
void mlx5_stats_reset(struct rte_eth_dev *);
int mlx5_xstats_get(struct rte_eth_dev *,
		    struct rte_eth_xstat *, unsigned int);
void mlx5_xstats_reset(struct rte_eth_dev *);
int mlx5_xstats_get_names(struct rte_eth_dev *,
			  struct rte_eth_xstat_name *, unsigned int);

/* mlx5_vlan.c */

int mlx5_vlan_filter_set(struct rte_eth_dev *, uint16_t, int);
int mlx5_vlan_offload_set(struct rte_eth_dev *, int);
void mlx5_vlan_strip_queue_set(struct rte_eth_dev *, uint16_t, int);

/* mlx5_trigger.c */

int mlx5_dev_start(struct rte_eth_dev *);
void mlx5_dev_stop(struct rte_eth_dev *);
int priv_dev_traffic_enable(struct priv *, struct rte_eth_dev *);
int priv_dev_traffic_disable(struct priv *, struct rte_eth_dev *);
int priv_dev_traffic_restart(struct priv *, struct rte_eth_dev *);
int mlx5_traffic_restart(struct rte_eth_dev *);

/* mlx5_flow.c */

int mlx5_dev_filter_ctrl(struct rte_eth_dev *, enum rte_filter_type,
			 enum rte_filter_op, void *);
int mlx5_flow_validate(struct rte_eth_dev *, const struct rte_flow_attr *,
		       const struct rte_flow_item [],
		       const struct rte_flow_action [],
		       struct rte_flow_error *);
struct rte_flow *mlx5_flow_create(struct rte_eth_dev *,
				  const struct rte_flow_attr *,
				  const struct rte_flow_item [],
				  const struct rte_flow_action [],
				  struct rte_flow_error *);
int mlx5_flow_destroy(struct rte_eth_dev *, struct rte_flow *,
		      struct rte_flow_error *);
void priv_flow_flush(struct priv *, struct mlx5_flows *);
int mlx5_flow_flush(struct rte_eth_dev *, struct rte_flow_error *);
int mlx5_flow_query(struct rte_eth_dev *, struct rte_flow *,
		    enum rte_flow_action_type, void *,
		    struct rte_flow_error *);
int mlx5_flow_isolate(struct rte_eth_dev *, int, struct rte_flow_error *);
int priv_flow_start(struct priv *, struct mlx5_flows *);
void priv_flow_stop(struct priv *, struct mlx5_flows *);
int priv_flow_verify(struct priv *);
int mlx5_ctrl_flow_vlan(struct rte_eth_dev *, struct rte_flow_item_eth *,
			struct rte_flow_item_eth *, struct rte_flow_item_vlan *,
			struct rte_flow_item_vlan *);
int mlx5_ctrl_flow(struct rte_eth_dev *, struct rte_flow_item_eth *,
		   struct rte_flow_item_eth *);
int priv_flow_create_drop_queue(struct priv *);
void priv_flow_delete_drop_queue(struct priv *);

/* mlx5_socket.c */

int priv_socket_init(struct priv *priv);
int priv_socket_uninit(struct priv *priv);
void priv_socket_handle(struct priv *priv);
int priv_socket_connect(struct priv *priv);

/* mlx5_mr.c */

struct mlx5_mr *priv_mr_new(struct priv *, struct rte_mempool *);
struct mlx5_mr *priv_mr_get(struct priv *, struct rte_mempool *);
int priv_mr_release(struct priv *, struct mlx5_mr *);
int priv_mr_verify(struct priv *);

#endif /* RTE_PMD_MLX5_H_ */
