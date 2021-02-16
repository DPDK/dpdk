/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 * Copyright(c) 2018-2019 Pensando Systems, Inc. All rights reserved.
 */

#ifndef _IONIC_LIF_H_
#define _IONIC_LIF_H_

#include <inttypes.h>

#include <rte_ethdev.h>
#include <rte_ether.h>

#include "ionic_osdep.h"
#include "ionic_dev.h"
#include "ionic_rx_filter.h"

#define IONIC_ADMINQ_LENGTH	16	/* must be a power of two */
#define IONIC_NOTIFYQ_LENGTH	64	/* must be a power of two */

#define IONIC_RSS_OFFLOAD_ALL ( \
	IONIC_RSS_TYPE_IPV4 | \
	IONIC_RSS_TYPE_IPV4_TCP | \
	IONIC_RSS_TYPE_IPV4_UDP | \
	IONIC_RSS_TYPE_IPV6 | \
	IONIC_RSS_TYPE_IPV6_TCP | \
	IONIC_RSS_TYPE_IPV6_UDP)

#define IONIC_GET_SG_CNTR_IDX(num_sg_elems)	(num_sg_elems)

struct ionic_tx_stats {
	uint64_t packets;
	uint64_t bytes;
	uint64_t drop;
	uint64_t stop;
	uint64_t no_csum;
	uint64_t tso;
};

struct ionic_rx_stats {
	uint64_t packets;
	uint64_t bytes;
	uint64_t no_cb_arg;
	uint64_t bad_cq_status;
	uint64_t no_room;
	uint64_t bad_len;
	uint64_t mtods;
};

#define IONIC_QCQ_F_INITED	BIT(0)
#define IONIC_QCQ_F_SG		BIT(1)
#define IONIC_QCQ_F_DEFERRED	BIT(4)
#define IONIC_QCQ_F_CSUM_L3	BIT(7)
#define IONIC_QCQ_F_CSUM_UDP	BIT(8)
#define IONIC_QCQ_F_CSUM_TCP	BIT(9)

/* Queue / Completion Queue */
struct ionic_qcq {
	struct ionic_queue q;        /**< Queue */
	struct ionic_cq cq;          /**< Completion Queue */
	struct ionic_lif *lif;       /**< LIF */
	const struct rte_memzone *base_z;
	void *base;
	rte_iova_t base_pa;
};

struct ionic_admin_qcq {
	struct ionic_qcq qcq;
	uint16_t flags;
};

struct ionic_notify_qcq {
	struct ionic_qcq qcq;
	uint16_t flags;

	struct ionic_intr_info intr;
};

struct ionic_rx_qcq {
	/* cacheline0, cacheline1 */
	struct ionic_qcq qcq;

	/* cacheline2 */
	struct rte_mempool *mb_pool;
	uint16_t flags;

	/* cacheline3 (inside stats) */
	struct ionic_rx_stats stats;
};

struct ionic_tx_qcq {
	/* cacheline0, cacheline1 */
	struct ionic_qcq qcq;

	/* cacheline2 */
	uint16_t num_segs_fw;	/* # segs supported by current FW */
	uint16_t flags;

	struct ionic_tx_stats stats;
};

#define IONIC_Q_TO_QCQ(_q)	container_of(_q, struct ionic_qcq, q)
#define IONIC_CQ_TO_QCQ(_cq)	container_of(_cq, struct ionic_qcq, cq)

struct ionic_qtype_info {
	uint8_t  version;
	uint8_t  supported;
	uint64_t features;
	uint16_t desc_sz;
	uint16_t comp_sz;
	uint16_t sg_desc_sz;
	uint16_t max_sg_elems;
	uint16_t sg_desc_stride;
};

#define IONIC_LIF_F_INITED		BIT(0)
#define IONIC_LIF_F_LINK_CHECK_NEEDED	BIT(1)
#define IONIC_LIF_F_UP			BIT(2)
#define IONIC_LIF_F_FW_RESET		BIT(3)

#define IONIC_LIF_NAME_MAX_SZ		(32)

struct ionic_lif {
	struct ionic_adapter *adapter;
	struct rte_eth_dev *eth_dev;
	uint16_t port_id;  /**< Device port identifier */
	uint32_t hw_index;
	uint32_t state;
	uint32_t ntxqcqs;
	uint32_t nrxqcqs;
	rte_spinlock_t adminq_lock;
	rte_spinlock_t adminq_service_lock;
	struct ionic_admin_qcq *adminqcq;
	struct ionic_notify_qcq *notifyqcq;
	struct ionic_tx_qcq **txqcqs;
	struct ionic_rx_qcq **rxqcqs;
	struct ionic_rx_filters rx_filters;
	struct ionic_doorbell __iomem *kern_dbpage;
	uint64_t last_eid;
	uint64_t features;
	uint32_t hw_features;
	uint32_t rx_mode;
	char name[IONIC_LIF_NAME_MAX_SZ];
	uint8_t mac_addr[RTE_ETHER_ADDR_LEN];
	uint16_t rss_types;
	uint8_t rss_hash_key[IONIC_RSS_HASH_KEY_SIZE];
	uint8_t *rss_ind_tbl;
	rte_iova_t rss_ind_tbl_pa;
	const struct rte_memzone *rss_ind_tbl_z;
	uint32_t rss_ind_tbl_nrxqcqs;
	uint32_t info_sz;
	struct ionic_lif_info *info;
	rte_iova_t info_pa;
	const struct rte_memzone *info_z;

	struct ionic_qtype_info qtype_info[IONIC_QTYPE_MAX];
	uint8_t qtype_ver[IONIC_QTYPE_MAX];

	struct rte_eth_stats stats_base;
	struct ionic_lif_stats lif_stats_base;
};

int ionic_lif_identify(struct ionic_adapter *adapter);
int ionic_lifs_size(struct ionic_adapter *ionic);

int ionic_lif_alloc(struct ionic_lif *lif);
void ionic_lif_free(struct ionic_lif *lif);
void ionic_lif_free_queues(struct ionic_lif *lif);

int ionic_lif_init(struct ionic_lif *lif);
void ionic_lif_deinit(struct ionic_lif *lif);

int ionic_lif_start(struct ionic_lif *lif);
void ionic_lif_stop(struct ionic_lif *lif);

void ionic_lif_configure(struct ionic_lif *lif);
void ionic_lif_configure_vlan_offload(struct ionic_lif *lif, int mask);
void ionic_lif_reset(struct ionic_lif *lif);

int ionic_intr_alloc(struct ionic_lif *lif, struct ionic_intr_info *intr);

int ionic_qcq_service(struct ionic_qcq *qcq, int budget, ionic_cq_cb cb,
	void *cb_arg);

int ionic_lif_change_mtu(struct ionic_lif *lif, int new_mtu);

int ionic_dev_add_mac(struct rte_eth_dev *eth_dev,
	struct rte_ether_addr *mac_addr,
	uint32_t index __rte_unused, uint32_t pool __rte_unused);
void ionic_dev_remove_mac(struct rte_eth_dev *eth_dev,
	uint32_t index __rte_unused);
int ionic_dev_set_mac(struct rte_eth_dev *eth_dev,
	struct rte_ether_addr *mac_addr);
int ionic_dev_vlan_filter_set(struct rte_eth_dev *eth_dev, uint16_t vlan_id,
	int on);
int ionic_dev_promiscuous_enable(struct rte_eth_dev *dev);
int ionic_dev_promiscuous_disable(struct rte_eth_dev *dev);
int ionic_dev_allmulticast_enable(struct rte_eth_dev *dev);
int ionic_dev_allmulticast_disable(struct rte_eth_dev *dev);

int ionic_rx_qcq_alloc(struct ionic_lif *lif, uint32_t socket_id,
	uint32_t index, uint16_t nrxq_descs,
	struct ionic_rx_qcq **qcq_out);
int ionic_tx_qcq_alloc(struct ionic_lif *lif, uint32_t socket_id,
	uint32_t index, uint16_t ntxq_descs,
	struct ionic_tx_qcq **qcq_out);
void ionic_qcq_free(struct ionic_qcq *qcq);

int ionic_qcq_enable(struct ionic_qcq *qcq);
int ionic_qcq_disable(struct ionic_qcq *qcq);

int ionic_lif_rxq_init(struct ionic_rx_qcq *rxq);
void ionic_lif_rxq_deinit(struct ionic_rx_qcq *rxq);

int ionic_lif_txq_init(struct ionic_tx_qcq *txq);
void ionic_lif_txq_deinit(struct ionic_tx_qcq *txq);

int ionic_lif_rss_config(struct ionic_lif *lif, const uint16_t types,
	const uint8_t *key, const uint32_t *indir);

int ionic_lif_set_features(struct ionic_lif *lif);

void ionic_lif_get_stats(const struct ionic_lif *lif,
	struct rte_eth_stats *stats);
void ionic_lif_reset_stats(struct ionic_lif *lif);

void ionic_lif_get_hw_stats(struct ionic_lif *lif,
	struct ionic_lif_stats *stats);
void ionic_lif_reset_hw_stats(struct ionic_lif *lif);

int ionic_notifyq_handler(struct ionic_lif *lif, int budget);

#endif /* _IONIC_LIF_H_ */
