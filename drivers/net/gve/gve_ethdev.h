/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2022 Intel Corporation
 */

#ifndef _GVE_ETHDEV_H_
#define _GVE_ETHDEV_H_

#include <ethdev_driver.h>
#include <ethdev_pci.h>
#include <rte_ether.h>
#include <rte_pci.h>

#include "base/gve.h"

/* TODO: this is a workaround to ensure that Tx complq is enough */
#define DQO_TX_MULTIPLIER 4

#define GVE_DEFAULT_RX_FREE_THRESH   64
#define GVE_DEFAULT_TX_FREE_THRESH   32
#define GVE_DEFAULT_TX_RS_THRESH     32
#define GVE_TX_MAX_FREE_SZ          512

#define GVE_RX_BUF_ALIGN_DQO        128
#define GVE_RX_MIN_BUF_SIZE_DQO    1024
#define GVE_RX_MAX_BUF_SIZE_DQO    ((16 * 1024) - GVE_RX_BUF_ALIGN_DQO)

#define GVE_RX_BUF_ALIGN_GQI       2048
#define GVE_RX_MIN_BUF_SIZE_GQI    2048
#define GVE_RX_MAX_BUF_SIZE_GQI    4096

#define GVE_TX_CKSUM_OFFLOAD_MASK (		\
		RTE_MBUF_F_TX_L4_MASK  |	\
		RTE_MBUF_F_TX_TCP_SEG)

/* A list of pages registered with the device during setup and used by a queue
 * as buffers
 */
struct gve_queue_page_list {
	uint32_t id; /* unique id */
	uint32_t num_entries;
	dma_addr_t *page_buses; /* the dma addrs of the pages */
	const struct rte_memzone *mz;
};

/* A TX desc ring entry */
union gve_tx_desc {
	struct gve_tx_pkt_desc pkt; /* first desc for a packet */
	struct gve_tx_seg_desc seg; /* subsequent descs for a packet */
};

/* Tx desc for DQO format */
union gve_tx_desc_dqo {
	struct gve_tx_pkt_desc_dqo pkt;
	struct gve_tx_tso_context_desc_dqo tso_ctx;
	struct gve_tx_general_context_desc_dqo general_ctx;
};

/* Offload features */
union gve_tx_offload {
	uint64_t data;
	struct {
		uint64_t l2_len:7; /* L2 (MAC) Header Length. */
		uint64_t l3_len:9; /* L3 (IP) Header Length. */
		uint64_t l4_len:8; /* L4 Header Length. */
		uint64_t tso_segsz:16; /* TCP TSO segment size */
		/* uint64_t unused : 24; */
	};
};

struct gve_tx_iovec {
	uint32_t iov_base; /* offset in fifo */
	uint32_t iov_len;
};

struct gve_tx_stats {
	uint64_t packets;
	uint64_t bytes;
	uint64_t errors;
};

struct gve_rx_stats {
	uint64_t packets;
	uint64_t bytes;
	uint64_t errors;
	uint64_t no_mbufs;
	uint64_t no_mbufs_bulk;
};

struct gve_xstats_name_offset {
	char name[RTE_ETH_XSTATS_NAME_SIZE];
	unsigned int offset;
};

struct gve_tx_queue {
	volatile union gve_tx_desc *tx_desc_ring;
	const struct rte_memzone *mz;
	uint64_t tx_ring_phys_addr;
	struct rte_mbuf **sw_ring;
	volatile rte_be32_t *qtx_tail;
	volatile rte_be32_t *qtx_head;

	uint32_t tx_tail;
	uint16_t nb_tx_desc;
	uint16_t nb_free;
	uint16_t nb_used;
	uint32_t next_to_clean;
	uint16_t free_thresh;
	uint16_t rs_thresh;

	/* Only valid for DQO_QPL queue format */
	uint16_t sw_tail;
	uint16_t sw_ntc;
	uint16_t sw_nb_free;
	uint32_t fifo_size;
	uint32_t fifo_head;
	uint32_t fifo_avail;
	uint64_t fifo_base;
	struct gve_queue_page_list *qpl;
	struct gve_tx_iovec *iov_ring;

	/* stats items */
	struct gve_tx_stats stats;

	uint16_t port_id;
	uint16_t queue_id;

	uint16_t ntfy_id;
	volatile rte_be32_t *ntfy_addr;

	struct gve_priv *hw;
	const struct rte_memzone *qres_mz;
	struct gve_queue_resources *qres;

	/* newly added for DQO */
	volatile union gve_tx_desc_dqo *tx_ring;
	struct gve_tx_compl_desc *compl_ring;
	const struct rte_memzone *compl_ring_mz;
	uint64_t compl_ring_phys_addr;
	uint32_t complq_tail;
	uint16_t sw_size;
	uint8_t cur_gen_bit;
	uint32_t last_desc_cleaned;
	void **txqs;
	uint16_t re_cnt;

	/* Only valid for DQO_RDA queue format */
	struct gve_tx_queue *complq;

	uint8_t is_gqi_qpl;
};

struct gve_rx_ctx {
	struct rte_mbuf *mbuf_head;
	struct rte_mbuf *mbuf_tail;
	uint16_t total_frags;
	bool drop_pkt;
};

struct gve_rx_queue {
	volatile struct gve_rx_desc *rx_desc_ring;
	volatile union gve_rx_data_slot *rx_data_ring;
	const struct rte_memzone *mz;
	const struct rte_memzone *data_mz;
	uint64_t rx_ring_phys_addr;
	struct rte_mbuf **sw_ring;
	struct rte_mempool *mpool;
	struct gve_rx_ctx ctx;

	uint16_t rx_tail;
	uint16_t nb_rx_desc;
	uint16_t expected_seqno; /* the next expected seqno */
	uint16_t free_thresh;
	uint16_t nb_rx_hold;
	uint32_t next_avail;
	uint32_t nb_avail;

	volatile rte_be32_t *qrx_tail;
	volatile rte_be32_t *ntfy_addr;

	/* only valid for GQI_QPL queue format */
	struct gve_queue_page_list *qpl;

	/* stats items */
	struct gve_rx_stats stats;

	struct gve_priv *hw;
	const struct rte_memzone *qres_mz;
	struct gve_queue_resources *qres;

	uint16_t port_id;
	uint16_t queue_id;
	uint16_t ntfy_id;
	uint16_t rx_buf_len;

	/* newly added for DQO */
	volatile struct gve_rx_desc_dqo *rx_ring;
	struct gve_rx_compl_desc_dqo *compl_ring;
	const struct rte_memzone *compl_ring_mz;
	uint64_t compl_ring_phys_addr;
	uint8_t cur_gen_bit;
	uint16_t bufq_tail;

	/* Only valid for DQO_RDA queue format */
	struct gve_rx_queue *bufq;

	uint8_t is_gqi_qpl;
};

struct gve_priv {
	struct gve_irq_db *irq_dbs; /* array of num_ntfy_blks */
	const struct rte_memzone *irq_dbs_mz;
	uint32_t mgmt_msix_idx;
	rte_be32_t *cnt_array; /* array of num_event_counters */
	const struct rte_memzone *cnt_array_mz;

	uint16_t num_event_counters;
	uint16_t tx_desc_cnt; /* txq size */
	uint16_t rx_desc_cnt; /* rxq size */
	uint16_t tx_pages_per_qpl; /* tx buffer length */
	uint16_t rx_data_slot_cnt; /* rx buffer length */

	/* Only valid for DQO_RDA queue format */
	uint16_t tx_compq_size; /* tx completion queue size */
	uint16_t rx_bufq_size; /* rx buff queue size */

	uint64_t max_registered_pages;
	uint64_t num_registered_pages; /* num pages registered with NIC */
	uint16_t default_num_queues; /* default num queues to set up */
	enum gve_queue_format queue_format; /* see enum gve_queue_format */
	uint8_t enable_rsc;

	uint16_t max_nb_txq;
	uint16_t max_nb_rxq;
	uint32_t num_ntfy_blks; /* spilt between TX and RX so must be even */

	struct gve_registers __iomem *reg_bar0; /* see gve_register.h */
	rte_be32_t __iomem *db_bar2; /* "array" of doorbells */
	struct rte_pci_device *pci_dev;

	/* Admin queue - see gve_adminq.h*/
	union gve_adminq_command *adminq;
	struct gve_dma_mem adminq_dma_mem;
	uint32_t adminq_mask; /* masks prod_cnt to adminq size */
	uint32_t adminq_prod_cnt; /* free-running count of AQ cmds executed */
	uint32_t adminq_cmd_fail; /* free-running count of AQ cmds failed */
	uint32_t adminq_timeouts; /* free-running count of AQ cmds timeouts */
	/* free-running count of per AQ cmd executed */
	uint32_t adminq_describe_device_cnt;
	uint32_t adminq_cfg_device_resources_cnt;
	uint32_t adminq_register_page_list_cnt;
	uint32_t adminq_unregister_page_list_cnt;
	uint32_t adminq_create_tx_queue_cnt;
	uint32_t adminq_create_rx_queue_cnt;
	uint32_t adminq_destroy_tx_queue_cnt;
	uint32_t adminq_destroy_rx_queue_cnt;
	uint32_t adminq_dcfg_device_resources_cnt;
	uint32_t adminq_set_driver_parameter_cnt;
	uint32_t adminq_report_stats_cnt;
	uint32_t adminq_report_link_speed_cnt;
	uint32_t adminq_get_ptype_map_cnt;
	uint32_t adminq_verify_driver_compatibility_cnt;
	volatile uint32_t state_flags;

	/* Gvnic device link speed from hypervisor. */
	uint64_t link_speed;

	uint16_t max_mtu;
	struct rte_ether_addr dev_addr; /* mac address */

	struct gve_queue_page_list *qpl;

	struct gve_tx_queue **txqs;
	struct gve_rx_queue **rxqs;
};

static inline bool
gve_is_gqi(struct gve_priv *priv)
{
	return priv->queue_format == GVE_GQI_RDA_FORMAT ||
		priv->queue_format == GVE_GQI_QPL_FORMAT;
}

static inline bool
gve_get_admin_queue_ok(struct gve_priv *priv)
{
	return !!rte_bit_relaxed_get32(GVE_PRIV_FLAGS_ADMIN_QUEUE_OK,
				       &priv->state_flags);
}

static inline void
gve_set_admin_queue_ok(struct gve_priv *priv)
{
	rte_bit_relaxed_set32(GVE_PRIV_FLAGS_ADMIN_QUEUE_OK,
			      &priv->state_flags);
}

static inline void
gve_clear_admin_queue_ok(struct gve_priv *priv)
{
	rte_bit_relaxed_clear32(GVE_PRIV_FLAGS_ADMIN_QUEUE_OK,
				&priv->state_flags);
}

static inline bool
gve_get_device_resources_ok(struct gve_priv *priv)
{
	return !!rte_bit_relaxed_get32(GVE_PRIV_FLAGS_DEVICE_RESOURCES_OK,
				       &priv->state_flags);
}

static inline void
gve_set_device_resources_ok(struct gve_priv *priv)
{
	rte_bit_relaxed_set32(GVE_PRIV_FLAGS_DEVICE_RESOURCES_OK,
			      &priv->state_flags);
}

static inline void
gve_clear_device_resources_ok(struct gve_priv *priv)
{
	rte_bit_relaxed_clear32(GVE_PRIV_FLAGS_DEVICE_RESOURCES_OK,
				&priv->state_flags);
}

static inline bool
gve_get_device_rings_ok(struct gve_priv *priv)
{
	return !!rte_bit_relaxed_get32(GVE_PRIV_FLAGS_DEVICE_RINGS_OK,
				       &priv->state_flags);
}

static inline void
gve_set_device_rings_ok(struct gve_priv *priv)
{
	rte_bit_relaxed_set32(GVE_PRIV_FLAGS_DEVICE_RINGS_OK,
			      &priv->state_flags);
}

static inline void
gve_clear_device_rings_ok(struct gve_priv *priv)
{
	rte_bit_relaxed_clear32(GVE_PRIV_FLAGS_DEVICE_RINGS_OK,
				&priv->state_flags);
}

int
gve_rx_queue_setup(struct rte_eth_dev *dev, uint16_t queue_id, uint16_t nb_desc,
		   unsigned int socket_id, const struct rte_eth_rxconf *conf,
		   struct rte_mempool *pool);
int
gve_tx_queue_setup(struct rte_eth_dev *dev, uint16_t queue_id, uint16_t nb_desc,
		   unsigned int socket_id, const struct rte_eth_txconf *conf);

void
gve_tx_queue_release(struct rte_eth_dev *dev, uint16_t qid);

void
gve_rx_queue_release(struct rte_eth_dev *dev, uint16_t qid);

int
gve_tx_queue_start(struct rte_eth_dev *dev, uint16_t tx_queue_id);

int
gve_rx_queue_start(struct rte_eth_dev *dev, uint16_t rx_queue_id);

int
gve_tx_queue_stop(struct rte_eth_dev *dev, uint16_t tx_queue_id);

int
gve_rx_queue_stop(struct rte_eth_dev *dev, uint16_t rx_queue_id);

void
gve_stop_tx_queues(struct rte_eth_dev *dev);

void
gve_stop_rx_queues(struct rte_eth_dev *dev);

uint16_t
gve_rx_burst(void *rxq, struct rte_mbuf **rx_pkts, uint16_t nb_pkts);

uint16_t
gve_tx_burst(void *txq, struct rte_mbuf **tx_pkts, uint16_t nb_pkts);

void
gve_set_rx_function(struct rte_eth_dev *dev);

void
gve_set_tx_function(struct rte_eth_dev *dev);

/* Below functions are used for DQO */

int
gve_rx_queue_setup_dqo(struct rte_eth_dev *dev, uint16_t queue_id,
		       uint16_t nb_desc, unsigned int socket_id,
		       const struct rte_eth_rxconf *conf,
		       struct rte_mempool *pool);
int
gve_tx_queue_setup_dqo(struct rte_eth_dev *dev, uint16_t queue_id,
		       uint16_t nb_desc, unsigned int socket_id,
		       const struct rte_eth_txconf *conf);

void
gve_tx_queue_release_dqo(struct rte_eth_dev *dev, uint16_t qid);

void
gve_rx_queue_release_dqo(struct rte_eth_dev *dev, uint16_t qid);

int
gve_rx_queue_start_dqo(struct rte_eth_dev *dev, uint16_t rx_queue_id);

int
gve_tx_queue_start_dqo(struct rte_eth_dev *dev, uint16_t tx_queue_id);

int
gve_rx_queue_stop_dqo(struct rte_eth_dev *dev, uint16_t rx_queue_id);

int
gve_tx_queue_stop_dqo(struct rte_eth_dev *dev, uint16_t tx_queue_id);

void
gve_stop_tx_queues_dqo(struct rte_eth_dev *dev);

void
gve_stop_rx_queues_dqo(struct rte_eth_dev *dev);

uint16_t
gve_rx_burst_dqo(void *rxq, struct rte_mbuf **rx_pkts, uint16_t nb_pkts);

uint16_t
gve_tx_burst_dqo(void *txq, struct rte_mbuf **tx_pkts, uint16_t nb_pkts);

void
gve_set_rx_function_dqo(struct rte_eth_dev *dev);

void
gve_set_tx_function_dqo(struct rte_eth_dev *dev);

#endif /* _GVE_ETHDEV_H_ */
