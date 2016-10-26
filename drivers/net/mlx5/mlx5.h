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

/* Verbs header. */
/* ISO C doesn't support unnamed structs/unions, disabling -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <infiniband/verbs.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-Wpedantic"
#endif

/* DPDK headers don't like -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_spinlock.h>
#include <rte_interrupts.h>
#include <rte_errno.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-Wpedantic"
#endif

#include "mlx5_utils.h"
#include "mlx5_rxtx.h"
#include "mlx5_autoconf.h"
#include "mlx5_defs.h"

#if !defined(HAVE_VERBS_IBV_EXP_CQ_COMPRESSED_CQE) || \
	!defined(HAVE_VERBS_MLX5_ETH_VLAN_INLINE_HEADER_SIZE)
#error Mellanox OFED >= 3.3 is required, please refer to the documentation.
#endif

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
	uint16_t vlan_filter[MLX5_MAX_VLAN_IDS]; /* VLAN filters table. */
	unsigned int vlan_filter_n; /* Number of configured VLAN filters. */
	/* Device properties. */
	uint16_t mtu; /* Configured MTU. */
	uint8_t port; /* Physical port number. */
	unsigned int started:1; /* Device started, flows enabled. */
	unsigned int promisc_req:1; /* Promiscuous mode requested. */
	unsigned int allmulti_req:1; /* All multicast mode requested. */
	unsigned int hw_csum:1; /* Checksum offload is supported. */
	unsigned int hw_csum_l2tun:1; /* Same for L2 tunnels. */
	unsigned int hw_vlan_strip:1; /* VLAN stripping is supported. */
	unsigned int hw_fcs_strip:1; /* FCS stripping is supported. */
	unsigned int hw_padding:1; /* End alignment padding is supported. */
	unsigned int sriov:1; /* This is a VF or PF with VF devices. */
	unsigned int mps:1; /* Whether multi-packet send is supported. */
	unsigned int cqe_comp:1; /* Whether CQE compression is enabled. */
	unsigned int pending_alarm:1; /* An alarm is pending. */
	unsigned int txq_inline; /* Maximum packet size for inlining. */
	unsigned int txqs_inline; /* Queue number threshold for inlining. */
	/* RX/TX queues. */
	unsigned int rxqs_n; /* RX queues array size. */
	unsigned int txqs_n; /* TX queues array size. */
	struct rxq *(*rxqs)[]; /* RX queues. */
	struct txq *(*txqs)[]; /* TX queues. */
	/* Indirection tables referencing all RX WQs. */
	struct ibv_exp_rwq_ind_table *(*ind_tables)[];
	unsigned int ind_tables_n; /* Number of indirection tables. */
	unsigned int ind_table_max_size; /* Maximum indirection table size. */
	/* Hash RX QPs feeding the indirection table. */
	struct hash_rxq (*hash_rxqs)[];
	unsigned int hash_rxqs_n; /* Hash RX QPs array size. */
	/* RSS configuration array indexed by hash RX queue type. */
	struct rte_eth_rss_conf *(*rss_conf)[];
	uint64_t rss_hf; /* RSS DPDK bit field of active RSS. */
	struct rte_intr_handle intr_handle; /* Interrupt handler. */
	unsigned int (*reta_idx)[]; /* RETA index table. */
	unsigned int reta_idx_n; /* RETA index size. */
	struct fdir_filter_list *fdir_filter_list; /* Flow director rules. */
	struct fdir_queue *fdir_drop_queue; /* Flow director drop queue. */
	uint32_t link_speed_capa; /* Link speed capabilities. */
	rte_spinlock_t lock; /* Lock for control functions. */
};

/* Local storage for secondary process data. */
struct mlx5_secondary_data {
	struct rte_eth_dev_data data; /* Local device data. */
	struct priv *primary_priv; /* Private structure from primary. */
	struct rte_eth_dev_data *shared_dev_data; /* Shared device data. */
	rte_spinlock_t lock; /* Port configuration lock. */
} mlx5_secondary_data[RTE_MAX_ETHPORTS];

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

/* mlx5.c */

int mlx5_getenv_int(const char *);

/* mlx5_ethdev.c */

struct priv *mlx5_get_priv(struct rte_eth_dev *dev);
int mlx5_is_secondary(void);
int priv_get_ifname(const struct priv *, char (*)[IF_NAMESIZE]);
int priv_ifreq(const struct priv *, int req, struct ifreq *);
int priv_get_num_vfs(struct priv *, uint16_t *);
int priv_get_mtu(struct priv *, uint16_t *);
int priv_set_flags(struct priv *, unsigned int, unsigned int);
int mlx5_dev_configure(struct rte_eth_dev *);
void mlx5_dev_infos_get(struct rte_eth_dev *, struct rte_eth_dev_info *);
const uint32_t *mlx5_dev_supported_ptypes_get(struct rte_eth_dev *dev);
int mlx5_link_update_unlocked(struct rte_eth_dev *, int);
int mlx5_link_update(struct rte_eth_dev *, int);
int mlx5_dev_set_mtu(struct rte_eth_dev *, uint16_t);
int mlx5_dev_get_flow_ctrl(struct rte_eth_dev *, struct rte_eth_fc_conf *);
int mlx5_dev_set_flow_ctrl(struct rte_eth_dev *, struct rte_eth_fc_conf *);
int mlx5_ibv_device_to_pci_addr(const struct ibv_device *,
				struct rte_pci_addr *);
void mlx5_dev_link_status_handler(void *);
void mlx5_dev_interrupt_handler(struct rte_intr_handle *, void *);
void priv_dev_interrupt_handler_uninstall(struct priv *, struct rte_eth_dev *);
void priv_dev_interrupt_handler_install(struct priv *, struct rte_eth_dev *);
int mlx5_set_link_down(struct rte_eth_dev *dev);
int mlx5_set_link_up(struct rte_eth_dev *dev);
struct priv *mlx5_secondary_data_setup(struct priv *priv);
void priv_select_tx_function(struct priv *);
void priv_select_rx_function(struct priv *);

/* mlx5_mac.c */

int priv_get_mac(struct priv *, uint8_t (*)[ETHER_ADDR_LEN]);
void hash_rxq_mac_addrs_del(struct hash_rxq *);
void priv_mac_addrs_disable(struct priv *);
void mlx5_mac_addr_remove(struct rte_eth_dev *, uint32_t);
int hash_rxq_mac_addrs_add(struct hash_rxq *);
int priv_mac_addr_add(struct priv *, unsigned int,
		      const uint8_t (*)[ETHER_ADDR_LEN]);
int priv_mac_addrs_enable(struct priv *);
void mlx5_mac_addr_add(struct rte_eth_dev *, struct ether_addr *, uint32_t,
		       uint32_t);
void mlx5_mac_addr_set(struct rte_eth_dev *, struct ether_addr *);

/* mlx5_rss.c */

int rss_hash_rss_conf_new_key(struct priv *, const uint8_t *, unsigned int,
			      uint64_t);
int mlx5_rss_hash_update(struct rte_eth_dev *, struct rte_eth_rss_conf *);
int mlx5_rss_hash_conf_get(struct rte_eth_dev *, struct rte_eth_rss_conf *);
int priv_rss_reta_index_resize(struct priv *, unsigned int);
int mlx5_dev_rss_reta_query(struct rte_eth_dev *,
			    struct rte_eth_rss_reta_entry64 *, uint16_t);
int mlx5_dev_rss_reta_update(struct rte_eth_dev *,
			     struct rte_eth_rss_reta_entry64 *, uint16_t);

/* mlx5_rxmode.c */

int priv_special_flow_enable(struct priv *, enum hash_rxq_flow_type);
void priv_special_flow_disable(struct priv *, enum hash_rxq_flow_type);
int priv_special_flow_enable_all(struct priv *);
void priv_special_flow_disable_all(struct priv *);
void mlx5_promiscuous_enable(struct rte_eth_dev *);
void mlx5_promiscuous_disable(struct rte_eth_dev *);
void mlx5_allmulticast_enable(struct rte_eth_dev *);
void mlx5_allmulticast_disable(struct rte_eth_dev *);

/* mlx5_stats.c */

void mlx5_stats_get(struct rte_eth_dev *, struct rte_eth_stats *);
void mlx5_stats_reset(struct rte_eth_dev *);

/* mlx5_vlan.c */

int mlx5_vlan_filter_set(struct rte_eth_dev *, uint16_t, int);
void mlx5_vlan_offload_set(struct rte_eth_dev *, int);
void mlx5_vlan_strip_queue_set(struct rte_eth_dev *, uint16_t, int);

/* mlx5_trigger.c */

int mlx5_dev_start(struct rte_eth_dev *);
void mlx5_dev_stop(struct rte_eth_dev *);

/* mlx5_fdir.c */

void priv_fdir_queue_destroy(struct priv *, struct fdir_queue *);
int fdir_init_filters_list(struct priv *);
void priv_fdir_delete_filters_list(struct priv *);
void priv_fdir_disable(struct priv *);
void priv_fdir_enable(struct priv *);
int mlx5_dev_filter_ctrl(struct rte_eth_dev *, enum rte_filter_type,
			 enum rte_filter_op, void *);

#endif /* RTE_PMD_MLX5_H_ */
