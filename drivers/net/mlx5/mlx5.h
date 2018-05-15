/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2015 6WIND S.A.
 * Copyright 2015 Mellanox Technologies, Ltd
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
#include <rte_rwlock.h>
#include <rte_interrupts.h>
#include <rte_errno.h>
#include <rte_flow.h>

#include "mlx5_utils.h"
#include "mlx5_mr.h"
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
	PCI_DEVICE_ID_MELLANOX_CONNECTX5BF = 0xa2d2,
};

LIST_HEAD(mlx5_dev_list, priv);

/* Shared memory between primary and secondary processes. */
struct mlx5_shared_data {
	struct mlx5_dev_list mem_event_cb_list;
	rte_rwlock_t mem_event_rwlock;
};

extern struct mlx5_shared_data *mlx5_shared_data;

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
	unsigned int hw_vlan_strip:1; /* VLAN stripping is supported. */
	unsigned int hw_fcs_strip:1; /* FCS stripping is supported. */
	unsigned int hw_padding:1; /* End alignment padding is supported. */
	unsigned int vf:1; /* This is a VF. */
	unsigned int mps:2; /* Multi-packet send supported mode. */
	unsigned int tunnel_en:1;
	/* Whether tunnel stateless offloads are supported. */
	unsigned int mpls_en:1; /* MPLS over GRE/UDP is enabled. */
	unsigned int flow_counter_en:1; /* Whether flow counter is supported. */
	unsigned int cqe_comp:1; /* CQE compression is enabled. */
	unsigned int tso:1; /* Whether TSO is supported. */
	unsigned int tx_vec_en:1; /* Tx vector is enabled. */
	unsigned int rx_vec_en:1; /* Rx vector is enabled. */
	unsigned int mpw_hdr_dseg:1; /* Enable DSEGs in the title WQEBB. */
	unsigned int l3_vxlan_en:1; /* Enable L3 VXLAN flow creation. */
	unsigned int vf_nl_en:1; /* Enable Netlink requests in VF mode. */
	unsigned int swp:1; /* Tx generic tunnel checksum and TSO offload. */
	struct {
		unsigned int enabled:1; /* Whether MPRQ is enabled. */
		unsigned int stride_num_n; /* Number of strides. */
		unsigned int min_stride_size_n; /* Min size of a stride. */
		unsigned int max_stride_size_n; /* Max size of a stride. */
		unsigned int max_memcpy_len;
		/* Maximum packet size to memcpy Rx packets. */
		unsigned int min_rxqs_num;
		/* Rx queue count threshold to enable MPRQ. */
	} mprq; /* Configurations for Multi-Packet RQ. */
	unsigned int max_verbs_prio; /* Number of Verb flow priorities. */
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

/* 8 Verbs priorities. */
#define MLX5_VERBS_FLOW_PRIO_8 8

/**
 * Verbs allocator needs a context to know in the callback which kind of
 * resources it is allocating.
 */
struct mlx5_verbs_alloc_ctx {
	enum mlx5_verbs_alloc_type type; /* Kind of object being allocated. */
	const void *obj; /* Pointer to the DPDK object. */
};

LIST_HEAD(mlx5_mr_list, mlx5_mr);

struct priv {
	LIST_ENTRY(priv) mem_event_cb; /* Called by memory event callback. */
	struct rte_eth_dev_data *dev_data;  /* Pointer to device data. */
	struct ibv_context *ctx; /* Verbs context. */
	struct ibv_device_attr_ex device_attr; /* Device properties. */
	struct ibv_pd *pd; /* Protection Domain. */
	char ibdev_path[IBV_SYSFS_PATH_MAX]; /* IB device path for secondary */
	struct ether_addr mac[MLX5_MAX_MAC_ADDRESSES]; /* MAC addresses. */
	BITFIELD_DECLARE(mac_own, uint64_t, MLX5_MAX_MAC_ADDRESSES);
	/* Bit-field of MAC addresses owned by the PMD. */
	uint16_t vlan_filter[MLX5_MAX_VLAN_IDS]; /* VLAN filters table. */
	unsigned int vlan_filter_n; /* Number of configured VLAN filters. */
	/* Device properties. */
	uint16_t mtu; /* Configured MTU. */
	uint8_t port; /* Physical port number. */
	unsigned int isolated:1; /* Whether isolated mode is enabled. */
	/* RX/TX queues. */
	unsigned int rxqs_n; /* RX queues array size. */
	unsigned int txqs_n; /* TX queues array size. */
	struct mlx5_rxq_data *(*rxqs)[]; /* RX queues. */
	struct mlx5_txq_data *(*txqs)[]; /* TX queues. */
	struct rte_mempool *mprq_mp; /* Mempool for Multi-Packet RQ. */
	struct rte_eth_rss_conf rss_conf; /* RSS configuration. */
	struct rte_intr_handle intr_handle; /* Interrupt handler. */
	unsigned int (*reta_idx)[]; /* RETA index table. */
	unsigned int reta_idx_n; /* RETA index size. */
	struct mlx5_hrxq_drop *flow_drop_queue; /* Flow drop queue. */
	struct mlx5_flows flows; /* RTE Flow rules. */
	struct mlx5_flows ctrl_flows; /* Control flow rules. */
	struct {
		uint32_t dev_gen; /* Generation number to flush local caches. */
		rte_rwlock_t rwlock; /* MR Lock. */
		struct mlx5_mr_btree cache; /* Global MR cache table. */
		struct mlx5_mr_list mr_list; /* Registered MR list. */
		struct mlx5_mr_list mr_free_list; /* Freed MR list. */
	} mr;
	LIST_HEAD(rxq, mlx5_rxq_ctrl) rxqsctrl; /* DPDK Rx queues. */
	LIST_HEAD(rxqibv, mlx5_rxq_ibv) rxqsibv; /* Verbs Rx queues. */
	LIST_HEAD(hrxq, mlx5_hrxq) hrxqs; /* Verbs Hash Rx queues. */
	LIST_HEAD(txq, mlx5_txq_ctrl) txqsctrl; /* DPDK Tx queues. */
	LIST_HEAD(txqibv, mlx5_txq_ibv) txqsibv; /* Verbs Tx queues. */
	/* Verbs Indirection tables. */
	LIST_HEAD(ind_tables, mlx5_ind_table_ibv) ind_tbls;
	uint32_t link_speed_capa; /* Link speed capabilities. */
	struct mlx5_xstats_ctrl xstats_ctrl; /* Extended stats control. */
	int primary_socket; /* Unix socket for primary process. */
	void *uar_base; /* Reserved address space for UAR mapping */
	struct rte_intr_handle intr_handle_socket; /* Interrupt handler. */
	struct mlx5_dev_config config; /* Device configuration. */
	struct mlx5_verbs_alloc_ctx verbs_alloc_ctx;
	/* Context for Verbs allocator. */
	int nl_socket; /* Netlink socket. */
	uint32_t nl_sn; /* Netlink message sequence number. */
};

#define PORT_ID(priv) ((priv)->dev_data->port_id)
#define ETH_DEV(priv) (&rte_eth_devices[PORT_ID(priv)])

/* mlx5.c */

int mlx5_getenv_int(const char *);

/* mlx5_ethdev.c */

int mlx5_get_ifname(const struct rte_eth_dev *dev, char (*ifname)[IF_NAMESIZE]);
int mlx5_ifindex(const struct rte_eth_dev *dev);
int mlx5_ifreq(const struct rte_eth_dev *dev, int req, struct ifreq *ifr);
int mlx5_get_mtu(struct rte_eth_dev *dev, uint16_t *mtu);
int mlx5_set_flags(struct rte_eth_dev *dev, unsigned int keep,
		   unsigned int flags);
int mlx5_dev_configure(struct rte_eth_dev *dev);
void mlx5_dev_infos_get(struct rte_eth_dev *dev, struct rte_eth_dev_info *info);
const uint32_t *mlx5_dev_supported_ptypes_get(struct rte_eth_dev *dev);
int mlx5_link_update(struct rte_eth_dev *dev, int wait_to_complete);
int mlx5_force_link_status_change(struct rte_eth_dev *dev, int status);
int mlx5_dev_set_mtu(struct rte_eth_dev *dev, uint16_t mtu);
int mlx5_dev_get_flow_ctrl(struct rte_eth_dev *dev,
			   struct rte_eth_fc_conf *fc_conf);
int mlx5_dev_set_flow_ctrl(struct rte_eth_dev *dev,
			   struct rte_eth_fc_conf *fc_conf);
int mlx5_ibv_device_to_pci_addr(const struct ibv_device *device,
				struct rte_pci_addr *pci_addr);
void mlx5_dev_link_status_handler(void *arg);
void mlx5_dev_interrupt_handler(void *arg);
void mlx5_dev_interrupt_handler_uninstall(struct rte_eth_dev *dev);
void mlx5_dev_interrupt_handler_install(struct rte_eth_dev *dev);
int mlx5_set_link_down(struct rte_eth_dev *dev);
int mlx5_set_link_up(struct rte_eth_dev *dev);
int mlx5_is_removed(struct rte_eth_dev *dev);
eth_tx_burst_t mlx5_select_tx_function(struct rte_eth_dev *dev);
eth_rx_burst_t mlx5_select_rx_function(struct rte_eth_dev *dev);

/* mlx5_mac.c */

int mlx5_get_mac(struct rte_eth_dev *dev, uint8_t (*mac)[ETHER_ADDR_LEN]);
void mlx5_mac_addr_remove(struct rte_eth_dev *dev, uint32_t index);
int mlx5_mac_addr_add(struct rte_eth_dev *dev, struct ether_addr *mac,
		      uint32_t index, uint32_t vmdq);
int mlx5_mac_addr_set(struct rte_eth_dev *dev, struct ether_addr *mac_addr);
int mlx5_set_mc_addr_list(struct rte_eth_dev *dev,
			  struct ether_addr *mc_addr_set, uint32_t nb_mc_addr);

/* mlx5_rss.c */

int mlx5_rss_hash_update(struct rte_eth_dev *dev,
			 struct rte_eth_rss_conf *rss_conf);
int mlx5_rss_hash_conf_get(struct rte_eth_dev *dev,
			   struct rte_eth_rss_conf *rss_conf);
int mlx5_rss_reta_index_resize(struct rte_eth_dev *dev, unsigned int reta_size);
int mlx5_dev_rss_reta_query(struct rte_eth_dev *dev,
			    struct rte_eth_rss_reta_entry64 *reta_conf,
			    uint16_t reta_size);
int mlx5_dev_rss_reta_update(struct rte_eth_dev *dev,
			     struct rte_eth_rss_reta_entry64 *reta_conf,
			     uint16_t reta_size);

/* mlx5_rxmode.c */

void mlx5_promiscuous_enable(struct rte_eth_dev *dev);
void mlx5_promiscuous_disable(struct rte_eth_dev *dev);
void mlx5_allmulticast_enable(struct rte_eth_dev *dev);
void mlx5_allmulticast_disable(struct rte_eth_dev *dev);

/* mlx5_stats.c */

void mlx5_xstats_init(struct rte_eth_dev *dev);
int mlx5_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *stats);
void mlx5_stats_reset(struct rte_eth_dev *dev);
int mlx5_xstats_get(struct rte_eth_dev *dev, struct rte_eth_xstat *stats,
		    unsigned int n);
void mlx5_xstats_reset(struct rte_eth_dev *dev);
int mlx5_xstats_get_names(struct rte_eth_dev *dev __rte_unused,
			  struct rte_eth_xstat_name *xstats_names,
			  unsigned int n);

/* mlx5_vlan.c */

int mlx5_vlan_filter_set(struct rte_eth_dev *dev, uint16_t vlan_id, int on);
void mlx5_vlan_strip_queue_set(struct rte_eth_dev *dev, uint16_t queue, int on);
int mlx5_vlan_offload_set(struct rte_eth_dev *dev, int mask);

/* mlx5_trigger.c */

int mlx5_dev_start(struct rte_eth_dev *dev);
void mlx5_dev_stop(struct rte_eth_dev *dev);
int mlx5_traffic_enable(struct rte_eth_dev *dev);
void mlx5_traffic_disable(struct rte_eth_dev *dev);
int mlx5_traffic_restart(struct rte_eth_dev *dev);

/* mlx5_flow.c */

unsigned int mlx5_get_max_verbs_prio(struct rte_eth_dev *dev);
int mlx5_flow_validate(struct rte_eth_dev *dev,
		       const struct rte_flow_attr *attr,
		       const struct rte_flow_item items[],
		       const struct rte_flow_action actions[],
		       struct rte_flow_error *error);
struct rte_flow *mlx5_flow_create(struct rte_eth_dev *dev,
				  const struct rte_flow_attr *attr,
				  const struct rte_flow_item items[],
				  const struct rte_flow_action actions[],
				  struct rte_flow_error *error);
int mlx5_flow_destroy(struct rte_eth_dev *dev, struct rte_flow *flow,
		      struct rte_flow_error *error);
void mlx5_flow_list_flush(struct rte_eth_dev *dev, struct mlx5_flows *list);
int mlx5_flow_flush(struct rte_eth_dev *dev, struct rte_flow_error *error);
int mlx5_flow_query(struct rte_eth_dev *dev, struct rte_flow *flow,
		    const struct rte_flow_action *action, void *data,
		    struct rte_flow_error *error);
int mlx5_flow_isolate(struct rte_eth_dev *dev, int enable,
		      struct rte_flow_error *error);
int mlx5_dev_filter_ctrl(struct rte_eth_dev *dev,
			 enum rte_filter_type filter_type,
			 enum rte_filter_op filter_op,
			 void *arg);
int mlx5_flow_start(struct rte_eth_dev *dev, struct mlx5_flows *list);
void mlx5_flow_stop(struct rte_eth_dev *dev, struct mlx5_flows *list);
int mlx5_flow_verify(struct rte_eth_dev *dev);
int mlx5_ctrl_flow_vlan(struct rte_eth_dev *dev,
			struct rte_flow_item_eth *eth_spec,
			struct rte_flow_item_eth *eth_mask,
			struct rte_flow_item_vlan *vlan_spec,
			struct rte_flow_item_vlan *vlan_mask);
int mlx5_ctrl_flow(struct rte_eth_dev *dev,
		   struct rte_flow_item_eth *eth_spec,
		   struct rte_flow_item_eth *eth_mask);
int mlx5_flow_create_drop_queue(struct rte_eth_dev *dev);
void mlx5_flow_delete_drop_queue(struct rte_eth_dev *dev);

/* mlx5_socket.c */

int mlx5_socket_init(struct rte_eth_dev *priv);
void mlx5_socket_uninit(struct rte_eth_dev *priv);
void mlx5_socket_handle(struct rte_eth_dev *priv);
int mlx5_socket_connect(struct rte_eth_dev *priv);

/* mlx5_nl.c */

int mlx5_nl_init(uint32_t nlgroups);
int mlx5_nl_mac_addr_add(struct rte_eth_dev *dev, struct ether_addr *mac,
			 uint32_t index);
int mlx5_nl_mac_addr_remove(struct rte_eth_dev *dev, struct ether_addr *mac,
			    uint32_t index);
void mlx5_nl_mac_addr_sync(struct rte_eth_dev *dev);
void mlx5_nl_mac_addr_flush(struct rte_eth_dev *dev);
int mlx5_nl_promisc(struct rte_eth_dev *dev, int enable);
int mlx5_nl_allmulti(struct rte_eth_dev *dev, int enable);

#endif /* RTE_PMD_MLX5_H_ */
