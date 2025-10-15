/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#ifndef _ICE_RXTX_H_
#define _ICE_RXTX_H_

#include "../common/rx.h"
#include "../common/tx.h"
#include "ice_ethdev.h"

#define FIELD_GET(_mask, _reg) \
	(__extension__ ({ \
		typeof(_mask) _x = (_mask); \
		(typeof(_x))(((_reg) & (_x)) >> rte_bsf32(_x)); \
	}))
#define FIELD_PREP(_mask, _val) \
	(__extension__ ({ \
		typeof(_mask) _x = (_mask); \
		((typeof(_x))(_val) << rte_bsf32(_x)) & (_x); \
	}))

#define ICE_ALIGN_RING_DESC  32
#define ICE_MIN_RING_DESC    64
#define ICE_MAX_RING_DESC    (8192 - 32)
#define ICE_MAX_RING_DESC_E830	  8096
#define ICE_MAX_NUM_DESC_BY_MAC(hw) ((hw)->phy_model == \
					ICE_PHY_E830 ? \
				    ICE_MAX_RING_DESC_E830 : \
				    ICE_MAX_RING_DESC)
#define ICE_DMA_MEM_ALIGN    4096
#define ICE_RING_BASE_ALIGN  128

#define ICE_TXTIME_TX_DESC_IDX_M	RTE_GENMASK32(12, 0)
#define ICE_TXTIME_STAMP_M		RTE_GENMASK32(31, 13)
#define ICE_REQ_DESC_MULTIPLE	32

#define ICE_RX_MAX_BURST CI_RX_MAX_BURST
#define ICE_TX_MAX_BURST 32

/* Maximal number of segments to split. */
#define ICE_RX_MAX_NSEG CI_RX_MAX_NSEG

#define ICE_CHK_Q_ENA_COUNT        100
#define ICE_CHK_Q_ENA_INTERVAL_US  100

#define ICE_SUPPORT_CHAIN_NUM 5

#define ICE_TD_CMD                      ICE_TX_DESC_CMD_EOP

#define ICE_VPMD_RX_BURST            CI_VPMD_RX_BURST
#define ICE_VPMD_TX_BURST            32
#define ICE_VPMD_RXQ_REARM_THRESH    CI_VPMD_RX_REARM_THRESH
#define ICE_TX_MAX_FREE_BUF_SZ       64
#define ICE_VPMD_DESCS_PER_LOOP      CI_VPMD_DESCS_PER_LOOP
#define ICE_VPMD_DESCS_PER_LOOP_WIDE CI_VPMD_DESCS_PER_LOOP_WIDE

#define ICE_FDIR_PKT_LEN	512

#define ICE_RXDID_COMMS_OVS	22

#define ICE_TX_MIN_PKT_LEN 17

#define ICE_TX_OFFLOAD_MASK (    \
		RTE_MBUF_F_TX_OUTER_IPV6 |	 \
		RTE_MBUF_F_TX_OUTER_IPV4 |	 \
		RTE_MBUF_F_TX_OUTER_IP_CKSUM |  \
		RTE_MBUF_F_TX_VLAN |        \
		RTE_MBUF_F_TX_IPV6 |		 \
		RTE_MBUF_F_TX_IPV4 |		 \
		RTE_MBUF_F_TX_IP_CKSUM |        \
		RTE_MBUF_F_TX_L4_MASK |         \
		RTE_MBUF_F_TX_IEEE1588_TMST |	 \
		RTE_MBUF_F_TX_TCP_SEG |         \
		RTE_MBUF_F_TX_QINQ |        \
		RTE_MBUF_F_TX_TUNNEL_MASK |	 \
		RTE_MBUF_F_TX_UDP_SEG |	 \
		RTE_MBUF_F_TX_OUTER_UDP_CKSUM)

#define ICE_TX_OFFLOAD_NOTSUP_MASK \
		(RTE_MBUF_F_TX_OFFLOAD_MASK ^ ICE_TX_OFFLOAD_MASK)

/* basic scalar path */
#define ICE_RX_SCALAR_OFFLOADS (				\
			RTE_ETH_RX_OFFLOAD_VLAN_STRIP |		\
			RTE_ETH_RX_OFFLOAD_KEEP_CRC |		\
			RTE_ETH_RX_OFFLOAD_SCATTER |		\
			RTE_ETH_RX_OFFLOAD_VLAN_FILTER |	\
			RTE_ETH_RX_OFFLOAD_IPV4_CKSUM |		\
			RTE_ETH_RX_OFFLOAD_UDP_CKSUM |		\
			RTE_ETH_RX_OFFLOAD_TCP_CKSUM |		\
			RTE_ETH_RX_OFFLOAD_QINQ_STRIP |		\
			RTE_ETH_RX_OFFLOAD_OUTER_IPV4_CKSUM |	\
			RTE_ETH_RX_OFFLOAD_VLAN_EXTEND |	\
			RTE_ETH_RX_OFFLOAD_RSS_HASH |		\
			RTE_ETH_RX_OFFLOAD_TIMESTAMP |		\
			RTE_ETH_RX_OFFLOAD_BUFFER_SPLIT)
/* basic vector paths */
#define ICE_RX_VECTOR_OFFLOADS (				\
			RTE_ETH_RX_OFFLOAD_KEEP_CRC |		\
			RTE_ETH_RX_OFFLOAD_SCATTER |		\
			RTE_ETH_RX_OFFLOAD_OUTER_IPV4_CKSUM)
/* vector offload paths */
#define ICE_RX_VECTOR_OFFLOAD_OFFLOADS (	\
		ICE_RX_VECTOR_OFFLOADS |	\
		RTE_ETH_RX_OFFLOAD_CHECKSUM |	\
		RTE_ETH_RX_OFFLOAD_VLAN_STRIP |	\
		RTE_ETH_RX_OFFLOAD_VLAN_FILTER |\
		RTE_ETH_RX_OFFLOAD_RSS_HASH)

/* Max header size can be 2K - 64 bytes */
#define ICE_RX_HDR_BUF_SIZE    (2048 - 64)

/* Max data buffer size must be 16K - 128 bytes */
#define ICE_RX_MAX_DATA_BUF_SIZE	(16 * 1024 - 128)

#define ICE_HEADER_SPLIT_ENA   BIT(0)

#define ICE_TX_MTU_SEG_MAX	8

typedef void (*ice_rxd_to_pkt_fields_t)(struct ci_rx_queue *rxq,
					struct rte_mbuf *mb,
					volatile union ci_rx_flex_desc *rxdp);

enum ice_rx_dtype {
	ICE_RX_DTYPE_NO_SPLIT       = 0,
	ICE_RX_DTYPE_HEADER_SPLIT   = 1,
	ICE_RX_DTYPE_SPLIT_ALWAYS   = 2,
};

/**
 * Structure associated with Tx Time based queue
 */
struct ice_txtime {
	volatile struct ice_ts_desc *ice_ts_ring; /* Tx time ring virtual address */
	uint16_t nb_ts_desc; /* number of Tx Time descriptors */
	uint16_t ts_tail; /* current value of tail register */
	int ts_offset; /* dynamic mbuf Tx timestamp field offset */
	uint64_t ts_flag; /* dynamic mbuf Tx timestamp flag */
	const struct rte_memzone *ts_mz;
};

/* Offload features */
union ice_tx_offload {
	uint64_t data;
	struct {
		uint64_t l2_len:7; /* L2 (MAC) Header Length. */
		uint64_t l3_len:9; /* L3 (IP) Header Length. */
		uint64_t l4_len:8; /* L4 Header Length. */
		uint64_t tso_segsz:16; /* TCP TSO segment size */
		uint64_t outer_l2_len:8; /* outer L2 Header Length */
		uint64_t outer_l3_len:16; /* outer L3 Header Length */
	};
};

/* Rx Flex Descriptor for Comms Package Profile
 * RxDID Profile ID 22 (swap Hash and FlowID)
 * Flex-field 0: Flow ID lower 16-bits
 * Flex-field 1: Flow ID upper 16-bits
 * Flex-field 2: RSS hash lower 16-bits
 * Flex-field 3: RSS hash upper 16-bits
 * Flex-field 4: AUX0
 * Flex-field 5: AUX1
 */
struct ice_32b_rx_flex_desc_comms_ovs {
	/* Qword 0 */
	u8 rxdid;
	u8 mir_id_umb_cast;
	__le16 ptype_flexi_flags0;
	__le16 pkt_len;
	__le16 hdr_len_sph_flex_flags1;

	/* Qword 1 */
	__le16 status_error0;
	__le16 l2tag1;
	__le32 flow_id;

	/* Qword 2 */
	__le16 status_error1;
	u8 flexi_flags2;
	u8 ts_low;
	__le16 l2tag2_1st;
	__le16 l2tag2_2nd;

	/* Qword 3 */
	__le32 rss_hash;
	union {
		struct {
			__le16 aux0;
			__le16 aux1;
		} flex;
		__le32 ts_high;
	} flex_ts;
};

int ice_rx_queue_setup(struct rte_eth_dev *dev,
		       uint16_t queue_idx,
		       uint16_t nb_desc,
		       unsigned int socket_id,
		       const struct rte_eth_rxconf *rx_conf,
		       struct rte_mempool *mp);
int ice_tx_queue_setup(struct rte_eth_dev *dev,
		       uint16_t queue_idx,
		       uint16_t nb_desc,
		       unsigned int socket_id,
		       const struct rte_eth_txconf *tx_conf);
int ice_rx_queue_start(struct rte_eth_dev *dev, uint16_t rx_queue_id);
int ice_rx_queue_stop(struct rte_eth_dev *dev, uint16_t rx_queue_id);
int ice_tx_queue_start(struct rte_eth_dev *dev, uint16_t tx_queue_id);
int ice_tx_queue_stop(struct rte_eth_dev *dev, uint16_t tx_queue_id);
int ice_fdir_rx_queue_start(struct rte_eth_dev *dev, uint16_t rx_queue_id);
int ice_fdir_tx_queue_start(struct rte_eth_dev *dev, uint16_t tx_queue_id);
int ice_fdir_rx_queue_stop(struct rte_eth_dev *dev, uint16_t rx_queue_id);
int ice_fdir_tx_queue_stop(struct rte_eth_dev *dev, uint16_t tx_queue_id);
void ice_rx_queue_release(void *rxq);
void ice_tx_queue_release(void *txq);
void ice_dev_rx_queue_release(struct rte_eth_dev *dev, uint16_t qid);
void ice_dev_tx_queue_release(struct rte_eth_dev *dev, uint16_t qid);
void ice_free_queues(struct rte_eth_dev *dev);
int ice_fdir_setup_tx_resources(struct ice_pf *pf);
int ice_fdir_setup_rx_resources(struct ice_pf *pf);
uint16_t ice_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts,
		       uint16_t nb_pkts);
uint16_t ice_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts,
		       uint16_t nb_pkts);
void ice_set_rx_function(struct rte_eth_dev *dev);
uint16_t ice_prep_pkts(__rte_unused void *tx_queue, struct rte_mbuf **tx_pkts,
		       uint16_t nb_pkts);
void ice_set_tx_function_flag(struct rte_eth_dev *dev,
			      struct ci_tx_queue *txq);
void ice_set_tx_function(struct rte_eth_dev *dev);
int ice_rx_queue_count(void *rx_queue);
void ice_rxq_info_get(struct rte_eth_dev *dev, uint16_t queue_id,
		      struct rte_eth_rxq_info *qinfo);
void ice_txq_info_get(struct rte_eth_dev *dev, uint16_t queue_id,
		      struct rte_eth_txq_info *qinfo);
int ice_rx_burst_mode_get(struct rte_eth_dev *dev, uint16_t queue_id,
			  struct rte_eth_burst_mode *mode);
int ice_tx_burst_mode_get(struct rte_eth_dev *dev, uint16_t queue_id,
			  struct rte_eth_burst_mode *mode);
int ice_rx_descriptor_status(void *rx_queue, uint16_t offset);
int ice_tx_descriptor_status(void *tx_queue, uint16_t offset);
void ice_set_default_ptype_table(struct rte_eth_dev *dev);
const uint32_t *ice_dev_supported_ptypes_get(struct rte_eth_dev *dev,
					     size_t *no_of_elements);
void ice_select_rxd_to_pkt_fields_handler(struct ci_rx_queue *rxq,
					  uint32_t rxdid);

int ice_rx_vec_dev_check(struct rte_eth_dev *dev);
int ice_tx_vec_dev_check(struct rte_eth_dev *dev);
int ice_rxq_vec_setup(struct ci_rx_queue *rxq);
int ice_txq_vec_setup(struct ci_tx_queue *txq);
uint16_t ice_recv_pkts_vec(void *rx_queue, struct rte_mbuf **rx_pkts,
			   uint16_t nb_pkts);
uint16_t ice_recv_scattered_pkts_vec(void *rx_queue, struct rte_mbuf **rx_pkts,
				     uint16_t nb_pkts);
uint16_t ice_xmit_pkts_vec(void *tx_queue, struct rte_mbuf **tx_pkts,
			   uint16_t nb_pkts);
uint16_t ice_recv_pkts_vec_avx2(void *rx_queue, struct rte_mbuf **rx_pkts,
				uint16_t nb_pkts);
uint16_t ice_recv_pkts_vec_avx2_offload(void *rx_queue, struct rte_mbuf **rx_pkts,
					uint16_t nb_pkts);
uint16_t ice_recv_scattered_pkts_vec_avx2(void *rx_queue,
					  struct rte_mbuf **rx_pkts,
					  uint16_t nb_pkts);
uint16_t ice_recv_scattered_pkts_vec_avx2_offload(void *rx_queue,
						  struct rte_mbuf **rx_pkts,
						  uint16_t nb_pkts);
uint16_t ice_xmit_pkts_vec_avx2(void *tx_queue, struct rte_mbuf **tx_pkts,
				uint16_t nb_pkts);
uint16_t ice_xmit_pkts_vec_avx2_offload(void *tx_queue, struct rte_mbuf **tx_pkts,
					uint16_t nb_pkts);
uint16_t ice_recv_pkts_vec_avx512(void *rx_queue, struct rte_mbuf **rx_pkts,
				  uint16_t nb_pkts);
uint16_t ice_recv_pkts_vec_avx512_offload(void *rx_queue,
					  struct rte_mbuf **rx_pkts,
					  uint16_t nb_pkts);
uint16_t ice_recv_scattered_pkts_vec_avx512(void *rx_queue,
					    struct rte_mbuf **rx_pkts,
					    uint16_t nb_pkts);
uint16_t ice_recv_scattered_pkts_vec_avx512_offload(void *rx_queue,
						    struct rte_mbuf **rx_pkts,
						    uint16_t nb_pkts);
uint16_t ice_xmit_pkts_vec_avx512(void *tx_queue, struct rte_mbuf **tx_pkts,
				  uint16_t nb_pkts);
uint16_t ice_xmit_pkts_vec_avx512_offload(void *tx_queue,
					  struct rte_mbuf **tx_pkts,
					  uint16_t nb_pkts);
int ice_fdir_programming(struct ice_pf *pf, struct ice_fltr_desc *fdir_desc);
int ice_tx_done_cleanup(void *txq, uint32_t free_cnt);
int ice_get_monitor_addr(void *rx_queue, struct rte_power_monitor_cond *pmc);
enum rte_vect_max_simd ice_get_max_simd_bitwidth(void);

#define FDIR_PARSING_ENABLE_PER_QUEUE(ad, on) do { \
	int i; \
	for (i = 0; i < (ad)->pf.dev_data->nb_rx_queues; i++) { \
		struct ci_rx_queue *rxq = (ad)->pf.dev_data->rx_queues[i]; \
		if (!rxq) \
			continue; \
		rxq->fdir_enabled = on; \
	} \
	PMD_DRV_LOG(DEBUG, "FDIR processing on RX set to %d", on); \
} while (0)

/* Enable/disable flow director parsing from Rx descriptor in data path. */
static inline
void ice_fdir_rx_parsing_enable(struct ice_adapter *ad, bool on)
{
	if (on) {
		/* Enable flow director parsing from Rx descriptor */
		FDIR_PARSING_ENABLE_PER_QUEUE(ad, on);
		ad->fdir_ref_cnt++;
	} else {
		if (ad->fdir_ref_cnt >= 1) {
			ad->fdir_ref_cnt--;

			if (ad->fdir_ref_cnt == 0)
				FDIR_PARSING_ENABLE_PER_QUEUE(ad, on);
		}
	}
}

#define ICE_TIMESYNC_REG_WRAP_GUARD_BAND  10000

/* Helper function to convert a 32b nanoseconds timestamp to 64b. */
static inline
uint64_t ice_tstamp_convert_32b_64b(struct ice_hw *hw, struct ice_adapter *ad,
				    uint32_t flag, uint32_t in_timestamp)
{
	uint8_t tmr_idx = hw->func_caps.ts_func_info.tmr_index_assoc;
	const uint64_t mask = 0xFFFFFFFF;
	uint32_t hi, lo, lo2, delta;
	uint64_t ns;

	if (flag) {
		lo = ICE_READ_REG(hw, GLTSYN_TIME_L(tmr_idx));
		hi = ICE_READ_REG(hw, GLTSYN_TIME_H(tmr_idx));

		/*
		 * On typical system, the delta between lo and lo2 is ~1000ns,
		 * so 10000 seems a large-enough but not overly-big guard band.
		 */
		if (lo > (UINT32_MAX - ICE_TIMESYNC_REG_WRAP_GUARD_BAND))
			lo2 = ICE_READ_REG(hw, GLTSYN_TIME_L(tmr_idx));
		else
			lo2 = lo;

		if (lo2 < lo) {
			lo = ICE_READ_REG(hw, GLTSYN_TIME_L(tmr_idx));
			hi = ICE_READ_REG(hw, GLTSYN_TIME_H(tmr_idx));
		}

		ad->time_hw = ((uint64_t)hi << 32) | lo;
	}

	delta = (in_timestamp - (uint32_t)(ad->time_hw & mask));
	if (delta > (mask / 2)) {
		delta = ((uint32_t)(ad->time_hw & mask) - in_timestamp);
		ns = ad->time_hw - delta;
	} else {
		ns = ad->time_hw + delta;
	}

	return ns;
}

#endif /* _ICE_RXTX_H_ */
