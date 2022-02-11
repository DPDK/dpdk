/* SPDX-License-Identifier: BSD-3-Clause
 *
 *   Copyright (c) 2015-2016 Freescale Semiconductor, Inc. All rights reserved.
 *   Copyright 2016-2021 NXP
 *
 */

#ifndef _DPAA2_ETHDEV_H
#define _DPAA2_ETHDEV_H

#include <rte_event_eth_rx_adapter.h>
#include <rte_pmd_dpaa2.h>

#include <rte_fslmc.h>
#include <dpaa2_hw_pvt.h>
#include "dpaa2_tm.h"

#include <mc/fsl_dpni.h>
#include <mc/fsl_mc_sys.h>

#define DPAA2_MIN_RX_BUF_SIZE 512
#define DPAA2_MAX_RX_PKT_LEN  10240 /*WRIOP support*/
#define NET_DPAA2_PMD_DRIVER_NAME net_dpaa2

#define MAX_TCS			DPNI_MAX_TC
#define MAX_RX_QUEUES		128
#define MAX_TX_QUEUES		16
#define MAX_DPNI		8
#define DPAA2_MAX_CHANNELS	16

#define DPAA2_RX_DEFAULT_NBDESC 512

#define DPAA2_ETH_MAX_LEN (RTE_ETHER_MTU + \
			   RTE_ETHER_HDR_LEN + RTE_ETHER_CRC_LEN + \
			   VLAN_TAG_SIZE)

/*default tc to be used for ,congestion, distribution etc configuration. */
#define DPAA2_DEF_TC		0

/* Threshold for a Tx queue to *Enter* Congestion state.
 */
#define CONG_ENTER_TX_THRESHOLD   512

/* Threshold for a queue to *Exit* Congestion state.
 */
#define CONG_EXIT_TX_THRESHOLD    480

#define CONG_RETRY_COUNT 18000

/* RX queue tail drop threshold
 * currently considering 64 KB packets
 */
#define CONG_THRESHOLD_RX_BYTES_Q  (64 * 1024)
#define CONG_RX_OAL	128

/* Size of the input SMMU mapped memory required by MC */
#define DIST_PARAM_IOVA_SIZE 256

/* Enable TX Congestion control support
 * default is disable
 */
#define DPAA2_TX_CGR_OFF	0x01

/* Disable RX tail drop, default is enable */
#define DPAA2_RX_TAILDROP_OFF	0x04
/* Tx confirmation enabled */
#define DPAA2_TX_CONF_ENABLE	0x06

/* HW loopback the egress traffic to self ingress*/
#define DPAA2_TX_MAC_LOOPBACK_MODE 0x20

#define DPAA2_TX_SERDES_LOOPBACK_MODE 0x40

#define DPAA2_TX_DPNI_LOOPBACK_MODE 0x80

#define DPAA2_TX_LOOPBACK_MODE \
	(DPAA2_TX_MAC_LOOPBACK_MODE | \
	DPAA2_TX_SERDES_LOOPBACK_MODE | \
	DPAA2_TX_DPNI_LOOPBACK_MODE)

#define DPAA2_RSS_OFFLOAD_ALL ( \
	RTE_ETH_RSS_L2_PAYLOAD | \
	RTE_ETH_RSS_IP | \
	RTE_ETH_RSS_UDP | \
	RTE_ETH_RSS_TCP | \
	RTE_ETH_RSS_SCTP | \
	RTE_ETH_RSS_MPLS | \
	RTE_ETH_RSS_C_VLAN | \
	RTE_ETH_RSS_S_VLAN | \
	RTE_ETH_RSS_ESP | \
	RTE_ETH_RSS_AH | \
	RTE_ETH_RSS_PPPOE)

/* LX2 FRC Parsed values (Little Endian) */
#define DPAA2_PKT_TYPE_ETHER		0x0060
#define DPAA2_PKT_TYPE_IPV4		0x0000
#define DPAA2_PKT_TYPE_IPV6		0x0020
#define DPAA2_PKT_TYPE_IPV4_EXT \
			(0x0001 | DPAA2_PKT_TYPE_IPV4)
#define DPAA2_PKT_TYPE_IPV6_EXT \
			(0x0001 | DPAA2_PKT_TYPE_IPV6)
#define DPAA2_PKT_TYPE_IPV4_TCP \
			(0x000e | DPAA2_PKT_TYPE_IPV4)
#define DPAA2_PKT_TYPE_IPV6_TCP \
			(0x000e | DPAA2_PKT_TYPE_IPV6)
#define DPAA2_PKT_TYPE_IPV4_UDP \
			(0x0010 | DPAA2_PKT_TYPE_IPV4)
#define DPAA2_PKT_TYPE_IPV6_UDP \
			(0x0010 | DPAA2_PKT_TYPE_IPV6)
#define DPAA2_PKT_TYPE_IPV4_SCTP	\
			(0x000f | DPAA2_PKT_TYPE_IPV4)
#define DPAA2_PKT_TYPE_IPV6_SCTP	\
			(0x000f | DPAA2_PKT_TYPE_IPV6)
#define DPAA2_PKT_TYPE_IPV4_ICMP \
			(0x0003 | DPAA2_PKT_TYPE_IPV4_EXT)
#define DPAA2_PKT_TYPE_IPV6_ICMP \
			(0x0003 | DPAA2_PKT_TYPE_IPV6_EXT)
#define DPAA2_PKT_TYPE_VLAN_1		0x0160
#define DPAA2_PKT_TYPE_VLAN_2		0x0260

/* enable timestamp in mbuf*/
extern bool dpaa2_enable_ts[];
extern uint64_t dpaa2_timestamp_rx_dynflag;
extern int dpaa2_timestamp_dynfield_offset;

#define DPAA2_QOS_TABLE_RECONFIGURE	1
#define DPAA2_FS_TABLE_RECONFIGURE	2

#define DPAA2_QOS_TABLE_IPADDR_EXTRACT 4
#define DPAA2_FS_TABLE_IPADDR_EXTRACT 8

#define DPAA2_FLOW_MAX_KEY_SIZE		16

/* Externally defined */
extern const struct rte_flow_ops dpaa2_flow_ops;

extern const struct rte_tm_ops dpaa2_tm_ops;

extern bool dpaa2_enable_err_queue;

#define IP_ADDRESS_OFFSET_INVALID (-1)

struct dpaa2_key_info {
	uint8_t key_offset[DPKG_MAX_NUM_OF_EXTRACTS];
	uint8_t key_size[DPKG_MAX_NUM_OF_EXTRACTS];
	/* Special for IP address. */
	int ipv4_src_offset;
	int ipv4_dst_offset;
	int ipv6_src_offset;
	int ipv6_dst_offset;
	uint8_t key_total_size;
};

struct dpaa2_key_extract {
	struct dpkg_profile_cfg dpkg;
	struct dpaa2_key_info key_info;
};

struct extract_s {
	struct dpaa2_key_extract qos_key_extract;
	struct dpaa2_key_extract tc_key_extract[MAX_TCS];
	uint64_t qos_extract_param;
	uint64_t tc_extract_param[MAX_TCS];
};

struct dpaa2_dev_priv {
	void *hw;
	int32_t hw_id;
	int32_t qdid;
	uint16_t token;
	uint8_t nb_tx_queues;
	uint8_t nb_rx_queues;
	uint32_t options;
	void *rx_vq[MAX_RX_QUEUES];
	void *tx_vq[MAX_TX_QUEUES];
	struct dpaa2_bp_list *bp_list; /**<Attached buffer pool list */
	void *tx_conf_vq[MAX_TX_QUEUES * DPAA2_MAX_CHANNELS];
	void *rx_err_vq;
	uint8_t flags; /*dpaa2 config flags */
	uint8_t max_mac_filters;
	uint8_t max_vlan_filters;
	uint8_t num_rx_tc;
	uint8_t num_tx_tc;
	uint16_t qos_entries;
	uint16_t fs_entries;
	uint8_t dist_queues;
	uint8_t num_channels;
	uint8_t en_ordered;
	uint8_t en_loose_ordered;
	uint8_t max_cgs;
	uint8_t cgid_in_use[MAX_RX_QUEUES];

	struct extract_s extract;

	uint16_t ss_offset;
	uint64_t ss_iova;
	uint64_t ss_param_iova;
	/*stores timestamp of last received packet on dev*/
	uint64_t rx_timestamp;
	/*stores timestamp of last received tx confirmation packet on dev*/
	uint64_t tx_timestamp;
	/* stores pointer to next tx_conf queue that should be processed,
	 * it corresponds to last packet transmitted
	 */
	struct dpaa2_queue *next_tx_conf_queue;

	struct rte_eth_dev *eth_dev; /**< Pointer back to holding ethdev */
	rte_spinlock_t lpbk_qp_lock;

	uint8_t channel_inuse;
	LIST_HEAD(, rte_flow) flows; /**< Configured flow rule handles. */
	LIST_HEAD(nodes, dpaa2_tm_node) nodes;
	LIST_HEAD(shaper_profiles, dpaa2_tm_shaper_profile) shaper_profiles;
};

int dpaa2_distset_to_dpkg_profile_cfg(uint64_t req_dist_set,
				      struct dpkg_profile_cfg *kg_cfg);

int dpaa2_setup_flow_dist(struct rte_eth_dev *eth_dev,
		uint64_t req_dist_set, int tc_index);

int dpaa2_remove_flow_dist(struct rte_eth_dev *eth_dev,
			   uint8_t tc_index);

int dpaa2_attach_bp_list(struct dpaa2_dev_priv *priv,
	struct fsl_mc_io *dpni, void *blist);

__rte_internal
int dpaa2_eth_eventq_attach(const struct rte_eth_dev *dev,
		int eth_rx_queue_id,
		struct dpaa2_dpcon_dev *dpcon,
		const struct rte_event_eth_rx_adapter_queue_conf *queue_conf);

__rte_internal
int dpaa2_eth_eventq_detach(const struct rte_eth_dev *dev,
		int eth_rx_queue_id);

uint16_t dpaa2_dev_rx(void *queue, struct rte_mbuf **bufs, uint16_t nb_pkts);

uint16_t dpaa2_dev_loopback_rx(void *queue, struct rte_mbuf **bufs,
				uint16_t nb_pkts);

uint16_t dpaa2_dev_prefetch_rx(void *queue, struct rte_mbuf **bufs,
			       uint16_t nb_pkts);
void dpaa2_dev_process_parallel_event(struct qbman_swp *swp,
				      const struct qbman_fd *fd,
				      const struct qbman_result *dq,
				      struct dpaa2_queue *rxq,
				      struct rte_event *ev);
void dpaa2_dev_process_atomic_event(struct qbman_swp *swp,
				    const struct qbman_fd *fd,
				    const struct qbman_result *dq,
				    struct dpaa2_queue *rxq,
				    struct rte_event *ev);
void dpaa2_dev_process_ordered_event(struct qbman_swp *swp,
				     const struct qbman_fd *fd,
				     const struct qbman_result *dq,
				     struct dpaa2_queue *rxq,
				     struct rte_event *ev);
uint16_t dpaa2_dev_tx(void *queue, struct rte_mbuf **bufs, uint16_t nb_pkts);
uint16_t dpaa2_dev_tx_ordered(void *queue, struct rte_mbuf **bufs,
			      uint16_t nb_pkts);
__rte_internal
uint16_t dpaa2_dev_tx_multi_txq_ordered(void **queue,
		struct rte_mbuf **bufs, uint16_t nb_pkts);

void dpaa2_dev_free_eqresp_buf(uint16_t eqresp_ci);
void dpaa2_flow_clean(struct rte_eth_dev *dev);
uint16_t dpaa2_dev_tx_conf(void *queue)  __rte_unused;
int dpaa2_dev_is_dpaa2(struct rte_eth_dev *dev);

int dpaa2_timesync_enable(struct rte_eth_dev *dev);
int dpaa2_timesync_disable(struct rte_eth_dev *dev);
int dpaa2_timesync_read_time(struct rte_eth_dev *dev,
					struct timespec *timestamp);
int dpaa2_timesync_write_time(struct rte_eth_dev *dev,
					const struct timespec *timestamp);
int dpaa2_timesync_adjust_time(struct rte_eth_dev *dev, int64_t delta);
int dpaa2_timesync_read_rx_timestamp(struct rte_eth_dev *dev,
						struct timespec *timestamp,
						uint32_t flags __rte_unused);
int dpaa2_timesync_read_tx_timestamp(struct rte_eth_dev *dev,
					  struct timespec *timestamp);

int dpaa2_dev_recycle_config(struct rte_eth_dev *eth_dev);
int dpaa2_dev_recycle_deconfig(struct rte_eth_dev *eth_dev);
int dpaa2_dev_recycle_qp_setup(struct rte_dpaa2_device *dpaa2_dev,
	uint16_t qidx, uint64_t cntx,
	eth_rx_burst_t tx_lpbk, eth_tx_burst_t rx_lpbk,
	struct dpaa2_queue **txq,
	struct dpaa2_queue **rxq);

#endif /* _DPAA2_ETHDEV_H */
