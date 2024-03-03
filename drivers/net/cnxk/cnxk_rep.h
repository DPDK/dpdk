/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2024 Marvell.
 */
#include <cnxk_eswitch.h>
#include <cnxk_ethdev.h>

#ifndef __CNXK_REP_H__
#define __CNXK_REP_H__

/* Common ethdev ops */
extern struct eth_dev_ops cnxk_rep_dev_ops;

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
	uint16_t hw_func;
	bool is_vf_active;
	bool native_repte;
	struct cnxk_rep_rxq *rxq;
	struct cnxk_rep_txq *txq;
	uint8_t mac_addr[RTE_ETHER_ADDR_LEN];
	uint16_t repte_mtu;
};

static inline struct cnxk_rep_dev *
cnxk_rep_pmd_priv(const struct rte_eth_dev *eth_dev)
{
	return eth_dev->data->dev_private;
}

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
int cnxk_rep_stats_get(struct rte_eth_dev *eth_dev, struct rte_eth_stats *stats);
int cnxk_rep_stats_reset(struct rte_eth_dev *eth_dev);
int cnxk_rep_flow_ops_get(struct rte_eth_dev *ethdev, const struct rte_flow_ops **ops);
int cnxk_rep_state_update(struct cnxk_eswitch_dev *eswitch_dev, uint16_t hw_func, uint16_t *rep_id);

#endif /* __CNXK_REP_H__ */
