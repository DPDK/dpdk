/* SPDX-License-Identifier: BSD-3-Clause
 *
 *   Copyright (c) 2015-2016 Freescale Semiconductor, Inc. All rights reserved.
 *   Copyright 2016-2025 NXP
 *
 */

#ifndef _DPAA2_ETHDEV_H
#define _DPAA2_ETHDEV_H

#include <rte_compat.h>
#include <rte_event_eth_rx_adapter.h>
#include <rte_pmd_dpaa2.h>

#include <bus_fslmc_driver.h>
#include <dpaa2_hw_pvt.h>
#include "dpaa2_tm.h"

#include <mc/fsl_dpni.h>
#include <mc/fsl_mc_sys.h>
#include <mc/fsl_dpmac.h>

#include "base/dpaa2_hw_dpni_annot.h"

#define DPAA2_MIN_RX_BUF_SIZE 512
#define DPAA2_MAX_RX_PKT_LEN  10240 /*WRIOP support*/
#define NET_DPAA2_PMD_DRIVER_NAME net_dpaa2

#define MAX_TCS			DPNI_MAX_TC
#define MAX_RX_QUEUES		128
#define MAX_TX_QUEUES		16
#define MAX_DPNI		8
#define DPAA2_MAX_CHANNELS	16

#define DPAA2_EXTRACT_PARAM_MAX_SIZE \
	RTE_ALIGN(sizeof(struct dpni_ext_set_rx_tc_dist), 256)

#define DPAA2_EXTRACT_ALLOC_KEY_MAX_SIZE 256

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
#define DIST_PARAM_IOVA_SIZE DPAA2_EXTRACT_PARAM_MAX_SIZE

/* Enable TX Congestion control support
 * default is disable
 */
#define DPAA2_TX_CGR_OFF	0x01

/* Disable RX tail drop, default is enable */
#define DPAA2_RX_TAILDROP_OFF	0x04
/* Tx confirmation enabled */
#define DPAA2_TX_CONF_ENABLE	0x08

/* Tx dynamic confirmation enabled,
 * only valid with Tx confirmation enabled.
 */
#define DPAA2_TX_DYNAMIC_CONF_ENABLE	0x10
/* DPDMUX index for DPMAC */
#define DPAA2_DPDMUX_DPMAC_IDX 0

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

/* mac counters */
#define DPAA2_MAC_NUM_STATS            (DPMAC_CNT_EGR_CONTROL_FRAME + 1)
#define DPAA2_MAC_STATS_INDEX_DMA_SIZE (DPAA2_MAC_NUM_STATS * sizeof(uint32_t))
#define DPAA2_MAC_STATS_VALUE_DMA_SIZE (DPAA2_MAC_NUM_STATS * sizeof(uint64_t))

/* Global pool used by driver for SG list TX */
extern struct rte_mempool *dpaa2_tx_sg_pool;
/* Maximum SG segments */
#define DPAA2_MAX_SGS 128
/* SG pool size */
#define DPAA2_POOL_SIZE 2048
/* SG pool cache size */
#define DPAA2_POOL_CACHE_SIZE 256
/* structure to free external and indirect
 * buffers.
 */
struct sw_buf_free {
	/* To which packet this segment belongs */
	uint16_t pkt_id;
	/* The actual segment */
	struct rte_mbuf *seg;
};

/* enable timestamp in mbuf*/
extern bool dpaa2_enable_ts[];
extern uint64_t dpaa2_timestamp_rx_dynflag;
extern int dpaa2_timestamp_dynfield_offset;

/* Externally defined */
extern const struct rte_flow_ops dpaa2_flow_ops;

extern const struct rte_tm_ops dpaa2_tm_ops;

extern bool dpaa2_enable_err_queue;

extern bool dpaa2_print_parser_result;

#define DPAA2_FAPR_SIZE \
	(sizeof(struct dpaa2_annot_hdr) - \
	offsetof(struct dpaa2_annot_hdr, word3))

#define DPAA2_PR_NXTHDR_OFFSET 0

#define DPAA2_FAFE_PSR_OFFSET 2
#define DPAA2_FAFE_PSR_SIZE 2

#define DPAA2_FAF_PSR_OFFSET 4
#define DPAA2_FAF_PSR_SIZE 12

#define DPAA2_FAF_TOTAL_SIZE \
	(DPAA2_FAFE_PSR_SIZE + DPAA2_FAF_PSR_SIZE)

/* Just most popular Frame attribute flags (FAF) here.*/
enum dpaa2_rx_faf_offset {
	/* Set by SP start*/
	FAFE_VXLAN_IN_VLAN_FRAM = 0,
	FAFE_VXLAN_IN_IPV4_FRAM = 1,
	FAFE_VXLAN_IN_IPV6_FRAM = 2,
	FAFE_VXLAN_IN_UDP_FRAM = 3,
	FAFE_VXLAN_IN_TCP_FRAM = 4,

	FAFE_ECPRI_FRAM = 7,
	/* Set by SP end*/

	FAF_GTP_PRIMED_FRAM = 1 + DPAA2_FAFE_PSR_SIZE * 8,
	FAF_PTP_FRAM = 3 + DPAA2_FAFE_PSR_SIZE * 8,
	FAF_VXLAN_FRAM = 4 + DPAA2_FAFE_PSR_SIZE * 8,
	FAF_ETH_FRAM = 10 + DPAA2_FAFE_PSR_SIZE * 8,
	FAF_LLC_SNAP_FRAM = 18 + DPAA2_FAFE_PSR_SIZE * 8,
	FAF_VLAN_FRAM = 21 + DPAA2_FAFE_PSR_SIZE * 8,
	FAF_PPPOE_PPP_FRAM = 25 + DPAA2_FAFE_PSR_SIZE * 8,
	FAF_MPLS_FRAM = 27 + DPAA2_FAFE_PSR_SIZE * 8,
	FAF_ARP_FRAM = 30 + DPAA2_FAFE_PSR_SIZE * 8,
	FAF_IPV4_FRAM = 34 + DPAA2_FAFE_PSR_SIZE * 8,
	FAF_IPV6_FRAM = 42 + DPAA2_FAFE_PSR_SIZE * 8,
	FAF_IP_FRAM = 48 + DPAA2_FAFE_PSR_SIZE * 8,
	FAF_IP_FRAG_FRAM = 50 + DPAA2_FAFE_PSR_SIZE * 8,
	FAF_ICMP_FRAM = 57 + DPAA2_FAFE_PSR_SIZE * 8,
	FAF_IGMP_FRAM = 58 + DPAA2_FAFE_PSR_SIZE * 8,
	FAF_GRE_FRAM = 65 + DPAA2_FAFE_PSR_SIZE * 8,
	FAF_UDP_FRAM = 70 + DPAA2_FAFE_PSR_SIZE * 8,
	FAF_TCP_FRAM = 72 + DPAA2_FAFE_PSR_SIZE * 8,
	FAF_IPSEC_FRAM = 77 + DPAA2_FAFE_PSR_SIZE * 8,
	FAF_IPSEC_ESP_FRAM = 78 + DPAA2_FAFE_PSR_SIZE * 8,
	FAF_IPSEC_AH_FRAM = 79 + DPAA2_FAFE_PSR_SIZE * 8,
	FAF_SCTP_FRAM = 81 + DPAA2_FAFE_PSR_SIZE * 8,
	FAF_DCCP_FRAM = 83 + DPAA2_FAFE_PSR_SIZE * 8,
	FAF_GTP_FRAM = 87 + DPAA2_FAFE_PSR_SIZE * 8,
	FAF_ESP_FRAM = 89 + DPAA2_FAFE_PSR_SIZE * 8,
};

enum dpaa2_ecpri_fafe_type {
	ECPRI_FAFE_TYPE_0 = (8 - FAFE_ECPRI_FRAM),
	ECPRI_FAFE_TYPE_1 = (8 - FAFE_ECPRI_FRAM) | (1 << 1),
	ECPRI_FAFE_TYPE_2 = (8 - FAFE_ECPRI_FRAM) | (2 << 1),
	ECPRI_FAFE_TYPE_3 = (8 - FAFE_ECPRI_FRAM) | (3 << 1),
	ECPRI_FAFE_TYPE_4 = (8 - FAFE_ECPRI_FRAM) | (4 << 1),
	ECPRI_FAFE_TYPE_5 = (8 - FAFE_ECPRI_FRAM) | (5 << 1),
	ECPRI_FAFE_TYPE_6 = (8 - FAFE_ECPRI_FRAM) | (6 << 1),
	ECPRI_FAFE_TYPE_7 = (8 - FAFE_ECPRI_FRAM) | (7 << 1)
};

#define DPAA2_PR_ETH_OFF_OFFSET 19
#define DPAA2_PR_TCI_OFF_OFFSET 21
#define DPAA2_PR_LAST_ETYPE_OFFSET 23
#define DPAA2_PR_L3_OFF_OFFSET 27
#define DPAA2_PR_L4_OFF_OFFSET 30
#define DPAA2_PR_L5_OFF_OFFSET 31
#define DPAA2_PR_NXTHDR_OFF_OFFSET 34

/* Set by SP for vxlan distribution start*/
#define DPAA2_VXLAN_IN_TCI_OFFSET 16

#define DPAA2_VXLAN_IN_DADDR0_OFFSET 20
#define DPAA2_VXLAN_IN_DADDR1_OFFSET 22
#define DPAA2_VXLAN_IN_DADDR2_OFFSET 24
#define DPAA2_VXLAN_IN_DADDR3_OFFSET 25
#define DPAA2_VXLAN_IN_DADDR4_OFFSET 26
#define DPAA2_VXLAN_IN_DADDR5_OFFSET 28

#define DPAA2_VXLAN_IN_SADDR0_OFFSET 29
#define DPAA2_VXLAN_IN_SADDR1_OFFSET 32
#define DPAA2_VXLAN_IN_SADDR2_OFFSET 33
#define DPAA2_VXLAN_IN_SADDR3_OFFSET 35
#define DPAA2_VXLAN_IN_SADDR4_OFFSET 41
#define DPAA2_VXLAN_IN_SADDR5_OFFSET 42

#define DPAA2_VXLAN_VNI_OFFSET 43
#define DPAA2_VXLAN_IN_TYPE_OFFSET 46
/* Set by SP for vxlan distribution end*/

/* ECPRI shares SP context with VXLAN*/
#define DPAA2_ECPRI_MSG_OFFSET DPAA2_VXLAN_VNI_OFFSET

#define DPAA2_ECPRI_MAX_EXTRACT_NB 8

struct ipv4_sd_addr_extract_rule {
	uint32_t ipv4_src;
	uint32_t ipv4_dst;
};

struct ipv6_sd_addr_extract_rule {
	uint8_t ipv6_src[NH_FLD_IPV6_ADDR_SIZE];
	uint8_t ipv6_dst[NH_FLD_IPV6_ADDR_SIZE];
};

struct ipv4_ds_addr_extract_rule {
	uint32_t ipv4_dst;
	uint32_t ipv4_src;
};

struct ipv6_ds_addr_extract_rule {
	uint8_t ipv6_dst[NH_FLD_IPV6_ADDR_SIZE];
	uint8_t ipv6_src[NH_FLD_IPV6_ADDR_SIZE];
};

union ip_addr_extract_rule {
	struct ipv4_sd_addr_extract_rule ipv4_sd_addr;
	struct ipv6_sd_addr_extract_rule ipv6_sd_addr;
	struct ipv4_ds_addr_extract_rule ipv4_ds_addr;
	struct ipv6_ds_addr_extract_rule ipv6_ds_addr;
};

union ip_src_addr_extract_rule {
	uint32_t ipv4_src;
	uint8_t ipv6_src[NH_FLD_IPV6_ADDR_SIZE];
};

union ip_dst_addr_extract_rule {
	uint32_t ipv4_dst;
	uint8_t ipv6_dst[NH_FLD_IPV6_ADDR_SIZE];
};

enum ip_addr_extract_type {
	IP_NONE_ADDR_EXTRACT,
	IP_SRC_EXTRACT,
	IP_DST_EXTRACT,
	IP_SRC_DST_EXTRACT,
	IP_DST_SRC_EXTRACT
};

enum key_prot_type {
	/* HW extracts from standard protocol fields*/
	DPAA2_NET_PROT_KEY,
	/* HW extracts from FAF of PR*/
	DPAA2_FAF_KEY,
	/* HW extracts from PR other than FAF*/
	DPAA2_PR_KEY
};

struct key_prot_field {
	enum key_prot_type type;
	enum net_prot prot;
	uint32_t key_field;
};

struct dpaa2_raw_region {
	uint8_t raw_start;
	uint8_t raw_size;
};

struct dpaa2_key_profile {
	uint8_t num;
	uint8_t key_offset[DPKG_MAX_NUM_OF_EXTRACTS];
	uint8_t key_size[DPKG_MAX_NUM_OF_EXTRACTS];

	enum ip_addr_extract_type ip_addr_type;
	uint8_t ip_addr_extract_pos;
	uint8_t ip_addr_extract_off;

	uint8_t raw_extract_pos;
	uint8_t raw_extract_off;
	uint8_t raw_extract_num;

	uint8_t l4_src_port_present;
	uint8_t l4_src_port_pos;
	uint8_t l4_src_port_offset;
	uint8_t l4_dst_port_present;
	uint8_t l4_dst_port_pos;
	uint8_t l4_dst_port_offset;
	struct key_prot_field prot_field[DPKG_MAX_NUM_OF_EXTRACTS];
	uint16_t key_max_size;
	struct dpaa2_raw_region raw_region;
};

struct dpaa2_key_extract {
	struct dpkg_profile_cfg dpkg;
	struct dpaa2_key_profile key_profile;
};

struct extract_s {
	struct dpaa2_key_extract qos_key_extract;
	struct dpaa2_key_extract tc_key_extract[MAX_TCS];
	uint8_t *qos_extract_param;
	uint8_t *tc_extract_param[MAX_TCS];
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

	uint16_t dpni_ver_major;
	uint16_t dpni_ver_minor;

	enum rte_dpaa2_dev_type ep_dev_type;   /**< Endpoint Device Type */
	uint16_t ep_object_id;                 /**< Endpoint DPAA2 Object ID */
	char ep_name[RTE_DEV_NAME_MAX_LEN];

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
	/* Stores correction offset for one step timestamping */
	uint16_t ptp_correction_offset;
	/* for mac counters */
	uint32_t *cnt_idx_dma_mem;
	uint64_t *cnt_values_dma_mem;
	uint64_t cnt_idx_iova, cnt_values_iova;

	struct dpaa2_dev_flow *curr;
	LIST_HEAD(, dpaa2_dev_flow) flows;
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

void dpaa2_dev_free_eqresp_buf(uint16_t eqresp_ci, struct dpaa2_queue *dpaa2_q);
void dpaa2_flow_clean(struct rte_eth_dev *dev);
uint16_t dpaa2_dev_tx_conf(void *queue)  __rte_unused;

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
int dpaa2_soft_parser_loaded(void);

int dpaa2_dev_recycle_qp_setup(struct rte_dpaa2_device *dpaa2_dev,
	uint16_t qidx, uint64_t cntx,
	eth_rx_burst_t tx_lpbk, eth_tx_burst_t rx_lpbk,
	struct dpaa2_queue **txq,
	struct dpaa2_queue **rxq);

void
dpaa2_dev_mac_setup_stats(struct rte_eth_dev *dev);

#endif /* _DPAA2_ETHDEV_H */
