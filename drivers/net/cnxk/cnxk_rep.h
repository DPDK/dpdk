/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2024 Marvell.
 */

#include <regex.h>

#include <cnxk_eswitch.h>
#include <cnxk_ethdev.h>

#ifndef __CNXK_REP_H__
#define __CNXK_REP_H__

#define CNXK_REP_TX_OFFLOAD_CAPA                                                                   \
	(RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE | RTE_ETH_TX_OFFLOAD_VLAN_INSERT |                      \
	 RTE_ETH_TX_OFFLOAD_MULTI_SEGS)

#define CNXK_REP_RX_OFFLOAD_CAPA                                                                   \
	(RTE_ETH_RX_OFFLOAD_SCATTER | RTE_ETH_RX_OFFLOAD_RSS_HASH | RTE_ETH_RX_OFFLOAD_VLAN_STRIP)

/* Common ethdev ops */
extern struct eth_dev_ops cnxk_rep_dev_ops;

/* Flow ops for representor ports */
extern struct rte_flow_ops cnxk_rep_flow_ops;

struct cnxk_rep_queue_stats {
	uint64_t pkts;
	uint64_t bytes;
};

struct cnxk_rep_rxq {
	/* Parent rep device */
	struct cnxk_rep_dev *rep_dev;
	/* Queue ID */
	uint16_t qid;
	/* No of desc */
	uint16_t nb_desc;
	/* mempool handle */
	struct rte_mempool *mpool;
	/* RX config parameters */
	const struct rte_eth_rxconf *rx_conf;
	/* Per queue TX statistics */
	struct cnxk_rep_queue_stats stats;
};

struct cnxk_rep_txq {
	/* Parent rep device */
	struct cnxk_rep_dev *rep_dev;
	/* Queue ID */
	uint16_t qid;
	/* No of desc */
	uint16_t nb_desc;
	/* TX config parameters */
	const struct rte_eth_txconf *tx_conf;
	/* Per queue TX statistics */
	struct cnxk_rep_queue_stats stats;
};

/* Representor port configurations */
struct cnxk_rep_dev {
	uint16_t port_id;
	uint16_t rep_id;
	uint16_t switch_domain_id;
	struct cnxk_eswitch_dev *parent_dev;
	/* Representee HW func */
	uint16_t hw_func;
	/* No of queues configured at representee */
	uint16_t nb_rxq;
	bool is_vf_active;
	bool native_repte;
	struct cnxk_rep_rxq *rxq;
	struct cnxk_rep_txq *txq;
	uint8_t mac_addr[RTE_ETHER_ADDR_LEN];
	uint16_t repte_mtu;
};

/* Inline functions */
static inline void
cnxk_rep_lock(struct cnxk_rep_dev *rep)
{
	rte_spinlock_lock(&rep->parent_dev->rep_lock);
}

static inline void
cnxk_rep_unlock(struct cnxk_rep_dev *rep)
{
	rte_spinlock_unlock(&rep->parent_dev->rep_lock);
}

static inline struct cnxk_rep_dev *
cnxk_rep_pmd_priv(const struct rte_eth_dev *eth_dev)
{
	return eth_dev->data->dev_private;
}

static __rte_always_inline void
cnxk_rep_pool_buffer_stats(struct rte_mempool *pool)
{
	plt_rep_dbg("        pool %s size %d buffer count in use  %d available %d", pool->name,
		    pool->size, rte_mempool_in_use_count(pool), rte_mempool_avail_count(pool));
}

static inline int
cnxk_ethdev_is_representor(const char *if_name)
{
	regex_t regex;
	int val;

	val = regcomp(&regex, "net_.*_representor_.*", 0);
	val = regexec(&regex, if_name, 0, NULL, 0);
	return (val == 0);
}

/* Prototypes */
int cnxk_rep_dev_probe(struct rte_pci_device *pci_dev, struct cnxk_eswitch_dev *eswitch_dev);
int cnxk_rep_dev_remove(struct cnxk_eswitch_dev *eswitch_dev);
int cnxk_rep_dev_uninit(struct rte_eth_dev *ethdev);
int cnxk_rep_dev_info_get(struct rte_eth_dev *eth_dev, struct rte_eth_dev_info *dev_info);
int cnxk_rep_representor_info_get(struct rte_eth_dev *dev, struct rte_eth_representor_info *info);
int cnxk_rep_dev_configure(struct rte_eth_dev *eth_dev);

int cnxk_rep_link_update(struct rte_eth_dev *eth_dev, int wait_to_compl);
int cnxk_rep_dev_start(struct rte_eth_dev *eth_dev);
int cnxk_rep_rx_queue_setup(struct rte_eth_dev *eth_dev, uint16_t queue_idx, uint16_t nb_desc,
			    unsigned int socket_id, const struct rte_eth_rxconf *rx_conf,
			    struct rte_mempool *mp);
int cnxk_rep_tx_queue_setup(struct rte_eth_dev *eth_dev, uint16_t queue_idx, uint16_t nb_desc,
			    unsigned int socket_id, const struct rte_eth_txconf *tx_conf);
void cnxk_rep_rx_queue_release(struct rte_eth_dev *dev, uint16_t queue_idx);
void cnxk_rep_tx_queue_release(struct rte_eth_dev *dev, uint16_t queue_idx);
int cnxk_rep_dev_stop(struct rte_eth_dev *eth_dev);
int cnxk_rep_dev_close(struct rte_eth_dev *eth_dev);
int cnxk_rep_stats_get(struct rte_eth_dev *eth_dev, struct rte_eth_stats *stats,
		       struct eth_queue_stats *qstats);
int cnxk_rep_stats_reset(struct rte_eth_dev *eth_dev);
int cnxk_rep_flow_ops_get(struct rte_eth_dev *ethdev, const struct rte_flow_ops **ops);
int cnxk_rep_state_update(struct cnxk_eswitch_dev *eswitch_dev, uint32_t state, uint16_t *rep_id);
int cnxk_rep_promiscuous_enable(struct rte_eth_dev *ethdev);
int cnxk_rep_promiscuous_disable(struct rte_eth_dev *ethdev);
int cnxk_rep_mac_addr_set(struct rte_eth_dev *eth_dev, struct rte_ether_addr *addr);
uint16_t cnxk_rep_tx_burst_dummy(void *tx_queue, struct rte_mbuf **tx_pkts, uint16_t nb_pkts);
uint16_t cnxk_rep_rx_burst_dummy(void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t nb_pkts);
void cnxk_rep_tx_queue_stop(struct rte_eth_dev *ethdev, uint16_t queue_id);
void cnxk_rep_rx_queue_stop(struct rte_eth_dev *ethdev, uint16_t queue_id);
int cnxk_rep_xstats_get(struct rte_eth_dev *eth_dev, struct rte_eth_xstat *stats, unsigned int n);
int cnxk_rep_xstats_reset(struct rte_eth_dev *eth_dev);
int cnxk_rep_xstats_get_names(struct rte_eth_dev *eth_dev, struct rte_eth_xstat_name *xstats_names,
			      unsigned int n);
int cnxk_rep_xstats_get_by_id(struct rte_eth_dev *eth_dev, const uint64_t *ids, uint64_t *values,
			      unsigned int n);
int cnxk_rep_xstats_get_names_by_id(struct rte_eth_dev *eth_dev, const uint64_t *ids,
				    struct rte_eth_xstat_name *xstats_names, unsigned int n);
int cnxk_rep_mtu_set(struct rte_eth_dev *eth_dev, uint16_t mtu);

#endif /* __CNXK_REP_H__ */
