/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#ifndef _ICE_RXTX_H_
#define _ICE_RXTX_H_

#include "ice_ethdev.h"

#define ICE_ALIGN_RING_DESC  32
#define ICE_MIN_RING_DESC    64
#define ICE_MAX_RING_DESC    4096
#define ICE_DMA_MEM_ALIGN    4096
#define ICE_RING_BASE_ALIGN  128

#define ICE_RX_MAX_BURST 32
#define ICE_TX_MAX_BURST 32

/* Maximal number of segments to split. */
#define ICE_RX_MAX_NSEG 2

#define ICE_CHK_Q_ENA_COUNT        100
#define ICE_CHK_Q_ENA_INTERVAL_US  100

#ifdef RTE_LIBRTE_ICE_16BYTE_RX_DESC
#define ice_rx_flex_desc ice_16b_rx_flex_desc
#else
#define ice_rx_flex_desc ice_32b_rx_flex_desc
#endif

#define ICE_SUPPORT_CHAIN_NUM 5

#define ICE_TD_CMD                      ICE_TX_DESC_CMD_EOP

#define ICE_VPMD_RX_BURST           32
#define ICE_VPMD_TX_BURST           32
#define ICE_RXQ_REARM_THRESH        64
#define ICE_MAX_RX_BURST            ICE_RXQ_REARM_THRESH
#define ICE_TX_MAX_FREE_BUF_SZ      64
#define ICE_DESCS_PER_LOOP          4

#define ICE_FDIR_PKT_LEN	512

#define ICE_RXDID_COMMS_OVS	22

#define ICE_TX_MIN_PKT_LEN 17

extern uint64_t ice_timestamp_dynflag;
extern int ice_timestamp_dynfield_offset;

/* Max header size can be 2K - 64 bytes */
#define ICE_RX_HDR_BUF_SIZE    (2048 - 64)

#define ICE_HEADER_SPLIT_ENA   BIT(0)

typedef void (*ice_rx_release_mbufs_t)(struct ice_rx_queue *rxq);
typedef void (*ice_tx_release_mbufs_t)(struct ice_tx_queue *txq);
typedef void (*ice_rxd_to_pkt_fields_t)(struct ice_rx_queue *rxq,
					struct rte_mbuf *mb,
					volatile union ice_rx_flex_desc *rxdp);

struct ice_rx_entry {
	struct rte_mbuf *mbuf;
};

enum ice_rx_dtype {
	ICE_RX_DTYPE_NO_SPLIT       = 0,
	ICE_RX_DTYPE_HEADER_SPLIT   = 1,
	ICE_RX_DTYPE_SPLIT_ALWAYS   = 2,
};

struct ice_rx_queue {
	struct rte_mempool *mp; /* mbuf pool to populate RX ring */
	volatile union ice_rx_flex_desc *rx_ring;/* RX ring virtual address */
	rte_iova_t rx_ring_dma; /* RX ring DMA address */
	struct ice_rx_entry *sw_ring; /* address of RX soft ring */
	uint16_t nb_rx_desc; /* number of RX descriptors */
	uint16_t rx_free_thresh; /* max free RX desc to hold */
	uint16_t rx_tail; /* current value of tail */
	uint16_t nb_rx_hold; /* number of held free RX desc */
	struct rte_mbuf *pkt_first_seg; /**< first segment of current packet */
	struct rte_mbuf *pkt_last_seg; /**< last segment of current packet */
	uint16_t rx_nb_avail; /**< number of staged packets ready */
	uint16_t rx_next_avail; /**< index of next staged packets */
	uint16_t rx_free_trigger; /**< triggers rx buffer allocation */
	struct rte_mbuf fake_mbuf; /**< dummy mbuf */
	struct rte_mbuf *rx_stage[ICE_RX_MAX_BURST * 2];

	uint16_t rxrearm_nb;	/**< number of remaining to be re-armed */
	uint16_t rxrearm_start;	/**< the idx we start the re-arming from */
	uint64_t mbuf_initializer; /**< value to init mbufs */

	uint16_t port_id; /* device port ID */
	uint8_t crc_len; /* 0 if CRC stripped, 4 otherwise */
	uint8_t fdir_enabled; /* 0 if FDIR disabled, 1 when enabled */
	uint16_t queue_id; /* RX queue index */
	uint16_t reg_idx; /* RX queue register index */
	uint8_t drop_en; /* if not 0, set register bit */
	volatile uint8_t *qrx_tail; /* register address of tail */
	struct ice_vsi *vsi; /* the VSI this queue belongs to */
	uint16_t rx_buf_len; /* The packet buffer size */
	uint16_t rx_hdr_len; /* The header buffer size */
	uint16_t max_pkt_len; /* Maximum packet length */
	bool q_set; /* indicate if rx queue has been configured */
	bool rx_deferred_start; /* don't start this queue in dev start */
	uint8_t proto_xtr; /* Protocol extraction from flexible descriptor */
	int xtr_field_offs; /*Protocol extraction matedata offset*/
	uint64_t xtr_ol_flag; /* Protocol extraction offload flag */
	uint32_t rxdid; /* Receive Flex Descriptor profile ID */
	ice_rx_release_mbufs_t rx_rel_mbufs;
	uint64_t offloads;
	uint32_t time_high;
	uint32_t hw_register_set;
	const struct rte_memzone *mz;
	uint32_t hw_time_high; /* high 32 bits of timestamp */
	uint32_t hw_time_low; /* low 32 bits of timestamp */
	uint64_t hw_time_update; /* SW time of HW record updating */
	struct rte_eth_rxseg_split rxseg[ICE_RX_MAX_NSEG];
	uint32_t rxseg_nb;
};

struct ice_tx_entry {
	struct rte_mbuf *mbuf;
	uint16_t next_id;
	uint16_t last_id;
};

struct ice_vec_tx_entry {
	struct rte_mbuf *mbuf;
};

struct ice_tx_queue {
	uint16_t nb_tx_desc; /* number of TX descriptors */
	rte_iova_t tx_ring_dma; /* TX ring DMA address */
	volatile struct ice_tx_desc *tx_ring; /* TX ring virtual address */
	struct ice_tx_entry *sw_ring; /* virtual address of SW ring */
	uint16_t tx_tail; /* current value of tail register */
	volatile uint8_t *qtx_tail; /* register address of tail */
	uint16_t nb_tx_used; /* number of TX desc used since RS bit set */
	/* index to last TX descriptor to have been cleaned */
	uint16_t last_desc_cleaned;
	/* Total number of TX descriptors ready to be allocated. */
	uint16_t nb_tx_free;
	/* Start freeing TX buffers if there are less free descriptors than
	 * this value.
	 */
	uint16_t tx_free_thresh;
	/* Number of TX descriptors to use before RS bit is set. */
	uint16_t tx_rs_thresh;
	uint8_t pthresh; /**< Prefetch threshold register. */
	uint8_t hthresh; /**< Host threshold register. */
	uint8_t wthresh; /**< Write-back threshold reg. */
	uint16_t port_id; /* Device port identifier. */
	uint16_t queue_id; /* TX queue index. */
	uint32_t q_teid; /* TX schedule node id. */
	uint16_t reg_idx;
	uint64_t offloads;
	struct ice_vsi *vsi; /* the VSI this queue belongs to */
	uint16_t tx_next_dd;
	uint16_t tx_next_rs;
	bool tx_deferred_start; /* don't start this queue in dev start */
	bool q_set; /* indicate if tx queue has been configured */
	ice_tx_release_mbufs_t tx_rel_mbufs;
	const struct rte_memzone *mz;
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
			      struct ice_tx_queue *txq);
void ice_set_tx_function(struct rte_eth_dev *dev);
uint32_t ice_rx_queue_count(void *rx_queue);
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
const uint32_t *ice_dev_supported_ptypes_get(struct rte_eth_dev *dev);
void ice_select_rxd_to_pkt_fields_handler(struct ice_rx_queue *rxq,
					  uint32_t rxdid);

int ice_rx_vec_dev_check(struct rte_eth_dev *dev);
int ice_tx_vec_dev_check(struct rte_eth_dev *dev);
int ice_rxq_vec_setup(struct ice_rx_queue *rxq);
int ice_txq_vec_setup(struct ice_tx_queue *txq);
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

#define FDIR_PARSING_ENABLE_PER_QUEUE(ad, on) do { \
	int i; \
	for (i = 0; i < (ad)->pf.dev_data->nb_rx_queues; i++) { \
		struct ice_rx_queue *rxq = (ad)->pf.dev_data->rx_queues[i]; \
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
