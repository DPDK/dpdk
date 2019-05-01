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
	PCI_DEVICE_ID_MELLANOX_CONNECTX5BFVF = 0xa2d3,
	PCI_DEVICE_ID_MELLANOX_CONNECTX6 = 0x101b,
	PCI_DEVICE_ID_MELLANOX_CONNECTX6VF = 0x101c,
};

/* Request types for IPC. */
enum mlx5_mp_req_type {
	MLX5_MP_REQ_VERBS_CMD_FD = 1,
	MLX5_MP_REQ_CREATE_MR,
	MLX5_MP_REQ_START_RXTX,
	MLX5_MP_REQ_STOP_RXTX,
};

/* Pameters for IPC. */
struct mlx5_mp_param {
	enum mlx5_mp_req_type type;
	int port_id;
	int result;
	RTE_STD_C11
	union {
		uintptr_t addr; /* MLX5_MP_REQ_CREATE_MR */
	} args;
};

/** Request timeout for IPC. */
#define MLX5_MP_REQ_TIMEOUT_SEC 5

/** Key string for IPC. */
#define MLX5_MP_NAME "net_mlx5_mp"

/* Recognized Infiniband device physical port name types. */
enum mlx5_phys_port_name_type {
	MLX5_PHYS_PORT_NAME_TYPE_NOTSET = 0, /* Not set. */
	MLX5_PHYS_PORT_NAME_TYPE_LEGACY, /* before kernel ver < 5.0 */
	MLX5_PHYS_PORT_NAME_TYPE_UPLINK, /* p0, kernel ver >= 5.0 */
	MLX5_PHYS_PORT_NAME_TYPE_PFVF, /* pf0vf0, kernel ver >= 5.0 */
	MLX5_PHYS_PORT_NAME_TYPE_UNKNOWN, /* Unrecognized. */
};

/** Switch information returned by mlx5_nl_switch_info(). */
struct mlx5_switch_info {
	uint32_t master:1; /**< Master device. */
	uint32_t representor:1; /**< Representor device. */
	enum mlx5_phys_port_name_type name_type; /** < Port name type. */
	int32_t pf_num; /**< PF number (valid for pfxvfx format only). */
	int32_t port_name; /**< Representor port name. */
	uint64_t switch_id; /**< Switch identifier. */
};

LIST_HEAD(mlx5_dev_list, mlx5_ibv_shared);

/* Shared data between primary and secondary processes. */
struct mlx5_shared_data {
	rte_spinlock_t lock;
	/* Global spinlock for primary and secondary processes. */
	int init_done; /* Whether primary has done initialization. */
	unsigned int secondary_cnt; /* Number of secondary processes init'd. */
	struct mlx5_dev_list mem_event_cb_list;
	rte_rwlock_t mem_event_rwlock;
};

/* Per-process data structure, not visible to other processes. */
struct mlx5_local_data {
	int init_done; /* Whether a secondary has done initialization. */
};

extern struct mlx5_shared_data *mlx5_shared_data;

struct mlx5_counter_ctrl {
	/* Name of the counter. */
	char dpdk_name[RTE_ETH_XSTATS_NAME_SIZE];
	/* Name of the counter on the device table. */
	char ctr_name[RTE_ETH_XSTATS_NAME_SIZE];
	uint32_t ib:1; /**< Nonzero for IB counters. */
};

struct mlx5_xstats_ctrl {
	/* Number of device stats. */
	uint16_t stats_n;
	/* Number of device stats identified by PMD. */
	uint16_t  mlx5_stats_n;
	/* Index in the device counters table. */
	uint16_t dev_table_idx[MLX5_MAX_XSTATS];
	uint64_t base[MLX5_MAX_XSTATS];
	struct mlx5_counter_ctrl info[MLX5_MAX_XSTATS];
};

struct mlx5_stats_ctrl {
	/* Base for imissed counter. */
	uint64_t imissed_base;
};

/* devx counter object */
struct mlx5_devx_counter_set {
	struct mlx5dv_devx_obj *obj;
	int id; /* Flow counter ID */
};

/* HCA attributes. */
struct mlx5_hca_attr {
	uint32_t eswitch_manager:1;
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
	unsigned int tunnel_en:1;
	/* Whether tunnel stateless offloads are supported. */
	unsigned int mpls_en:1; /* MPLS over GRE/UDP is enabled. */
	unsigned int cqe_comp:1; /* CQE compression is enabled. */
	unsigned int cqe_pad:1; /* CQE padding is enabled. */
	unsigned int tso:1; /* Whether TSO is supported. */
	unsigned int tx_vec_en:1; /* Tx vector is enabled. */
	unsigned int rx_vec_en:1; /* Rx vector is enabled. */
	unsigned int mpw_hdr_dseg:1; /* Enable DSEGs in the title WQEBB. */
	unsigned int mr_ext_memseg_en:1;
	/* Whether memseg should be extended for MR creation. */
	unsigned int l3_vxlan_en:1; /* Enable L3 VXLAN flow creation. */
	unsigned int vf_nl_en:1; /* Enable Netlink requests in VF mode. */
	unsigned int dv_esw_en:1; /* Enable E-Switch DV flow. */
	unsigned int dv_flow_en:1; /* Enable DV flow. */
	unsigned int swp:1; /* Tx generic tunnel checksum and TSO offload. */
	unsigned int devx:1; /* Whether devx interface is available or not. */
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
	int mps; /* Multi-packet send supported mode. */
	unsigned int flow_prio; /* Number of flow priorities. */
	unsigned int tso_max_payload_sz; /* Maximum TCP payload for TSO. */
	unsigned int ind_table_max_size; /* Maximum indirection table size. */
	int txq_inline; /* Maximum packet size for inlining. */
	int txqs_inline; /* Queue number threshold for inlining. */
	int txqs_vec; /* Queue number threshold for vectorized Tx. */
	int inline_max_packet_sz; /* Max packet size for inlining. */
	struct mlx5_hca_attr hca_attr; /* HCA attributes. */
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

LIST_HEAD(mlx5_mr_list, mlx5_mr);

/* Flow drop context necessary due to Verbs API. */
struct mlx5_drop {
	struct mlx5_hrxq *hrxq; /* Hash Rx queue queue. */
	struct mlx5_rxq_ibv *rxq; /* Verbs Rx queue. */
};

struct mlx5_flow_tcf_context;

/* Per port data of shared IB device. */
struct mlx5_ibv_shared_port {
	uint32_t ih_port_id;
	/*
	 * Interrupt handler port_id. Used by shared interrupt
	 * handler to find the corresponding rte_eth device
	 * by IB port index. If value is equal or greater
	 * RTE_MAX_ETHPORTS it means there is no subhandler
	 * installed for specified IB port index.
	 */
};

/* Table structure. */
struct mlx5_flow_tbl_resource {
	void *obj; /**< Pointer to DR table object. */
	rte_atomic32_t refcnt; /**< Reference counter. */
};

#define MLX5_MAX_TABLES 1024
#define MLX5_MAX_TABLES_FDB 32
#define MLX5_GROUP_FACTOR 1

/*
 * Shared Infiniband device context for Master/Representors
 * which belong to same IB device with multiple IB ports.
 **/
struct mlx5_ibv_shared {
	LIST_ENTRY(mlx5_ibv_shared) next;
	uint32_t refcnt;
	uint32_t devx:1; /* Opened with DV. */
	uint32_t max_port; /* Maximal IB device port index. */
	struct ibv_context *ctx; /* Verbs/DV context. */
	struct ibv_pd *pd; /* Protection Domain. */
	char ibdev_name[IBV_SYSFS_NAME_MAX]; /* IB device name. */
	char ibdev_path[IBV_SYSFS_PATH_MAX]; /* IB device path for secondary */
	struct ibv_device_attr_ex device_attr; /* Device properties. */
	struct rte_pci_device *pci_dev; /* Backend PCI device. */
	LIST_ENTRY(mlx5_ibv_shared) mem_event_cb;
	/**< Called by memory event callback. */
	struct {
		uint32_t dev_gen; /* Generation number to flush local caches. */
		rte_rwlock_t rwlock; /* MR Lock. */
		struct mlx5_mr_btree cache; /* Global MR cache table. */
		struct mlx5_mr_list mr_list; /* Registered MR list. */
		struct mlx5_mr_list mr_free_list; /* Freed MR list. */
	} mr;
	/* Shared DV/DR flow data section. */
	pthread_mutex_t dv_mutex; /* DV context mutex. */
	uint32_t dv_refcnt; /* DV/DR data reference counter. */
	void *fdb_domain; /* FDB Direct Rules name space handle. */
	struct mlx5_flow_tbl_resource fdb_tbl[MLX5_MAX_TABLES_FDB];
	/* FDB Direct Rules tables. */
	void *rx_domain; /* RX Direct Rules name space handle. */
	struct mlx5_flow_tbl_resource rx_tbl[MLX5_MAX_TABLES];
	/* RX Direct Rules tables. */
	void *tx_domain; /* TX Direct Rules name space handle. */
	struct mlx5_flow_tbl_resource tx_tbl[MLX5_MAX_TABLES];
	void *esw_drop_action; /* Pointer to DR E-Switch drop action. */
	/* TX Direct Rules tables/ */
	LIST_HEAD(matchers, mlx5_flow_dv_matcher) matchers;
	LIST_HEAD(encap_decap, mlx5_flow_dv_encap_decap_resource) encaps_decaps;
	LIST_HEAD(modify_cmd, mlx5_flow_dv_modify_hdr_resource) modify_cmds;
	LIST_HEAD(tag, mlx5_flow_dv_tag_resource) tags;
	LIST_HEAD(jump, mlx5_flow_dv_jump_tbl_resource) jump_tbl;
	LIST_HEAD(port_id_action_list, mlx5_flow_dv_port_id_action_resource)
		port_id_action_list; /* List of port ID actions. */
	/* Shared interrupt handler section. */
	pthread_mutex_t intr_mutex; /* Interrupt config mutex. */
	uint32_t intr_cnt; /* Interrupt handler reference counter. */
	struct rte_intr_handle intr_handle; /* Interrupt handler for device. */
	struct mlx5_ibv_shared_port port[]; /* per device port data array. */
};

/* Per-process private structure. */
struct mlx5_proc_priv {
	size_t uar_table_sz;
	/* Size of UAR register table. */
	void *uar_table[];
	/* Table of UAR registers for each process. */
};

#define MLX5_PROC_PRIV(port_id) \
	((struct mlx5_proc_priv *)rte_eth_devices[port_id].process_private)

struct mlx5_priv {
	struct rte_eth_dev_data *dev_data;  /* Pointer to device data. */
	struct mlx5_ibv_shared *sh; /* Shared IB device context. */
	uint32_t ibv_port; /* IB device port number. */
	struct ether_addr mac[MLX5_MAX_MAC_ADDRESSES]; /* MAC addresses. */
	BITFIELD_DECLARE(mac_own, uint64_t, MLX5_MAX_MAC_ADDRESSES);
	/* Bit-field of MAC addresses owned by the PMD. */
	uint16_t vlan_filter[MLX5_MAX_VLAN_IDS]; /* VLAN filters table. */
	unsigned int vlan_filter_n; /* Number of configured VLAN filters. */
	/* Device properties. */
	uint16_t mtu; /* Configured MTU. */
	unsigned int isolated:1; /* Whether isolated mode is enabled. */
	unsigned int representor:1; /* Device is a port representor. */
	unsigned int master:1; /* Device is a E-Switch master. */
	unsigned int dr_shared:1; /* DV/DR data is shared. */
	uint16_t domain_id; /* Switch domain identifier. */
	uint16_t vport_id; /* Associated VF vport index (if any). */
	int32_t representor_id; /* Port representor identifier. */
	/* RX/TX queues. */
	unsigned int rxqs_n; /* RX queues array size. */
	unsigned int txqs_n; /* TX queues array size. */
	struct mlx5_rxq_data *(*rxqs)[]; /* RX queues. */
	struct mlx5_txq_data *(*txqs)[]; /* TX queues. */
	struct rte_mempool *mprq_mp; /* Mempool for Multi-Packet RQ. */
	struct rte_eth_rss_conf rss_conf; /* RSS configuration. */
	unsigned int (*reta_idx)[]; /* RETA index table. */
	unsigned int reta_idx_n; /* RETA index size. */
	struct mlx5_drop drop_queue; /* Flow drop queues. */
	struct mlx5_flows flows; /* RTE Flow rules. */
	struct mlx5_flows ctrl_flows; /* Control flow rules. */
	LIST_HEAD(counters, mlx5_flow_counter) flow_counters;
	/* Flow counters. */
	LIST_HEAD(rxq, mlx5_rxq_ctrl) rxqsctrl; /* DPDK Rx queues. */
	LIST_HEAD(rxqibv, mlx5_rxq_ibv) rxqsibv; /* Verbs Rx queues. */
	LIST_HEAD(hrxq, mlx5_hrxq) hrxqs; /* Verbs Hash Rx queues. */
	LIST_HEAD(txq, mlx5_txq_ctrl) txqsctrl; /* DPDK Tx queues. */
	LIST_HEAD(txqibv, mlx5_txq_ibv) txqsibv; /* Verbs Tx queues. */
	/* Verbs Indirection tables. */
	LIST_HEAD(ind_tables, mlx5_ind_table_ibv) ind_tbls;
	/* Pointer to next element. */
	rte_atomic32_t refcnt; /**< Reference counter. */
	struct ibv_flow_action *verbs_action;
	/**< Verbs modify header action object. */
	uint8_t ft_type; /**< Flow table type, Rx or Tx. */
	/* Tags resources cache. */
	uint32_t link_speed_capa; /* Link speed capabilities. */
	struct mlx5_xstats_ctrl xstats_ctrl; /* Extended stats control. */
	struct mlx5_stats_ctrl stats_ctrl; /* Stats control. */
	struct mlx5_dev_config config; /* Device configuration. */
	struct mlx5_verbs_alloc_ctx verbs_alloc_ctx;
	/* Context for Verbs allocator. */
	int nl_socket_rdma; /* Netlink socket (NETLINK_RDMA). */
	int nl_socket_route; /* Netlink socket (NETLINK_ROUTE). */
	uint32_t nl_sn; /* Netlink message sequence number. */
#ifndef RTE_ARCH_64
	rte_spinlock_t uar_lock_cq; /* CQs share a common distinct UAR */
	rte_spinlock_t uar_lock[MLX5_UAR_PAGE_NUM_MAX];
	/* UAR same-page access control required in 32bit implementations. */
#endif
	struct mlx5_flow_tcf_context *tcf_context; /* TC flower context. */
};

#define PORT_ID(priv) ((priv)->dev_data->port_id)
#define ETH_DEV(priv) (&rte_eth_devices[PORT_ID(priv)])

/* mlx5.c */

int mlx5_getenv_int(const char *);
int mlx5_proc_priv_init(struct rte_eth_dev *dev);

/* mlx5_ethdev.c */

int mlx5_get_ifname(const struct rte_eth_dev *dev, char (*ifname)[IF_NAMESIZE]);
int mlx5_get_master_ifname(const char *ibdev_path, char (*ifname)[IF_NAMESIZE]);
unsigned int mlx5_ifindex(const struct rte_eth_dev *dev);
int mlx5_ifreq(const struct rte_eth_dev *dev, int req, struct ifreq *ifr);
int mlx5_get_mtu(struct rte_eth_dev *dev, uint16_t *mtu);
int mlx5_set_flags(struct rte_eth_dev *dev, unsigned int keep,
		   unsigned int flags);
int mlx5_dev_configure(struct rte_eth_dev *dev);
void mlx5_dev_infos_get(struct rte_eth_dev *dev, struct rte_eth_dev_info *info);
int mlx5_fw_version_get(struct rte_eth_dev *dev, char *fw_ver, size_t fw_size);
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
unsigned int mlx5_dev_to_port_id(const struct rte_device *dev,
				 uint16_t *port_list,
				 unsigned int port_list_n);
int mlx5_port_to_eswitch_info(uint16_t port, uint16_t *es_domain_id,
			      uint16_t *es_port_id);
int mlx5_sysfs_switch_info(unsigned int ifindex,
			   struct mlx5_switch_info *info);
void mlx5_sysfs_check_switch_info(bool device_dir,
				  struct mlx5_switch_info *switch_info);
void mlx5_nl_check_switch_info(bool nun_vf_set,
			       struct mlx5_switch_info *switch_info);
void mlx5_translate_port_name(const char *port_name_in,
			      struct mlx5_switch_info *port_info_out);

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

void mlx5_stats_init(struct rte_eth_dev *dev);
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

int mlx5_flow_discover_priorities(struct rte_eth_dev *dev);
void mlx5_flow_print(struct rte_flow *flow);
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

/* mlx5_mp.c */
void mlx5_mp_req_start_rxtx(struct rte_eth_dev *dev);
void mlx5_mp_req_stop_rxtx(struct rte_eth_dev *dev);
int mlx5_mp_req_mr_create(struct rte_eth_dev *dev, uintptr_t addr);
int mlx5_mp_req_verbs_cmd_fd(struct rte_eth_dev *dev);
void mlx5_mp_init_primary(void);
void mlx5_mp_uninit_primary(void);
void mlx5_mp_init_secondary(void);
void mlx5_mp_uninit_secondary(void);

/* mlx5_nl.c */

int mlx5_nl_init(int protocol);
int mlx5_nl_mac_addr_add(struct rte_eth_dev *dev, struct ether_addr *mac,
			 uint32_t index);
int mlx5_nl_mac_addr_remove(struct rte_eth_dev *dev, struct ether_addr *mac,
			    uint32_t index);
void mlx5_nl_mac_addr_sync(struct rte_eth_dev *dev);
void mlx5_nl_mac_addr_flush(struct rte_eth_dev *dev);
int mlx5_nl_promisc(struct rte_eth_dev *dev, int enable);
int mlx5_nl_allmulti(struct rte_eth_dev *dev, int enable);
unsigned int mlx5_nl_portnum(int nl, const char *name);
unsigned int mlx5_nl_ifindex(int nl, const char *name, uint32_t pindex);
int mlx5_nl_switch_info(int nl, unsigned int ifindex,
			struct mlx5_switch_info *info);

/* mlx5_devx_cmds.c */

int mlx5_devx_cmd_flow_counter_alloc(struct ibv_context *ctx,
				     struct mlx5_devx_counter_set *dcx);
int mlx5_devx_cmd_flow_counter_free(struct mlx5dv_devx_obj *obj);
int mlx5_devx_cmd_flow_counter_query(struct mlx5_devx_counter_set *dcx,
				     int clear,
				     uint64_t *pkts, uint64_t *bytes);
int mlx5_devx_cmd_query_hca_attr(struct ibv_context *ctx,
				 struct mlx5_hca_attr *attr);
#endif /* RTE_PMD_MLX5_H_ */
