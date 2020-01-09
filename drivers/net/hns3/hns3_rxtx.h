/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2019 Hisilicon Limited.
 */

#ifndef _HNS3_RXTX_H_
#define _HNS3_RXTX_H_

#define	HNS3_MIN_RING_DESC	32
#define	HNS3_MAX_RING_DESC	32768
#define HNS3_DEFAULT_RING_DESC  1024
#define	HNS3_ALIGN_RING_DESC	32
#define HNS3_RING_BASE_ALIGN	128

#define HNS3_BD_SIZE_512_TYPE			0
#define HNS3_BD_SIZE_1024_TYPE			1
#define HNS3_BD_SIZE_2048_TYPE			2
#define HNS3_BD_SIZE_4096_TYPE			3

#define HNS3_RX_FLAG_VLAN_PRESENT		0x1
#define HNS3_RX_FLAG_L3ID_IPV4			0x0
#define HNS3_RX_FLAG_L3ID_IPV6			0x1
#define HNS3_RX_FLAG_L4ID_UDP			0x0
#define HNS3_RX_FLAG_L4ID_TCP			0x1

#define HNS3_RXD_DMAC_S				0
#define HNS3_RXD_DMAC_M				(0x3 << HNS3_RXD_DMAC_S)
#define HNS3_RXD_VLAN_S				2
#define HNS3_RXD_VLAN_M				(0x3 << HNS3_RXD_VLAN_S)
#define HNS3_RXD_L3ID_S				4
#define HNS3_RXD_L3ID_M				(0xf << HNS3_RXD_L3ID_S)
#define HNS3_RXD_L4ID_S				8
#define HNS3_RXD_L4ID_M				(0xf << HNS3_RXD_L4ID_S)
#define HNS3_RXD_FRAG_B				12
#define HNS3_RXD_STRP_TAGP_S			13
#define HNS3_RXD_STRP_TAGP_M			(0x3 << HNS3_RXD_STRP_TAGP_S)

#define HNS3_RXD_L2E_B				16
#define HNS3_RXD_L3E_B				17
#define HNS3_RXD_L4E_B				18
#define HNS3_RXD_TRUNCAT_B			19
#define HNS3_RXD_HOI_B				20
#define HNS3_RXD_DOI_B				21
#define HNS3_RXD_OL3E_B				22
#define HNS3_RXD_OL4E_B				23
#define HNS3_RXD_GRO_COUNT_S			24
#define HNS3_RXD_GRO_COUNT_M			(0x3f << HNS3_RXD_GRO_COUNT_S)
#define HNS3_RXD_GRO_FIXID_B			30
#define HNS3_RXD_GRO_ECN_B			31

#define HNS3_RXD_ODMAC_S			0
#define HNS3_RXD_ODMAC_M			(0x3 << HNS3_RXD_ODMAC_S)
#define HNS3_RXD_OVLAN_S			2
#define HNS3_RXD_OVLAN_M			(0x3 << HNS3_RXD_OVLAN_S)
#define HNS3_RXD_OL3ID_S			4
#define HNS3_RXD_OL3ID_M			(0xf << HNS3_RXD_OL3ID_S)
#define HNS3_RXD_OL4ID_S			8
#define HNS3_RXD_OL4ID_M			(0xf << HNS3_RXD_OL4ID_S)
#define HNS3_RXD_FBHI_S				12
#define HNS3_RXD_FBHI_M				(0x3 << HNS3_RXD_FBHI_S)
#define HNS3_RXD_FBLI_S				14
#define HNS3_RXD_FBLI_M				(0x3 << HNS3_RXD_FBLI_S)

#define HNS3_RXD_BDTYPE_S			0
#define HNS3_RXD_BDTYPE_M			(0xf << HNS3_RXD_BDTYPE_S)
#define HNS3_RXD_VLD_B				4
#define HNS3_RXD_UDP0_B				5
#define HNS3_RXD_EXTEND_B			7
#define HNS3_RXD_FE_B				8
#define HNS3_RXD_LUM_B				9
#define HNS3_RXD_CRCP_B				10
#define HNS3_RXD_L3L4P_B			11
#define HNS3_RXD_TSIND_S			12
#define HNS3_RXD_TSIND_M			(0x7 << HNS3_RXD_TSIND_S)
#define HNS3_RXD_LKBK_B				15
#define HNS3_RXD_GRO_SIZE_S			16
#define HNS3_RXD_GRO_SIZE_M			(0x3ff << HNS3_RXD_GRO_SIZE_S)

#define HNS3_TXD_L3T_S				0
#define HNS3_TXD_L3T_M				(0x3 << HNS3_TXD_L3T_S)
#define HNS3_TXD_L4T_S				2
#define HNS3_TXD_L4T_M				(0x3 << HNS3_TXD_L4T_S)
#define HNS3_TXD_L3CS_B				4
#define HNS3_TXD_L4CS_B				5
#define HNS3_TXD_VLAN_B				6
#define HNS3_TXD_TSO_B				7

#define HNS3_TXD_L2LEN_S			8
#define HNS3_TXD_L2LEN_M			(0xff << HNS3_TXD_L2LEN_S)
#define HNS3_TXD_L3LEN_S			16
#define HNS3_TXD_L3LEN_M			(0xff << HNS3_TXD_L3LEN_S)
#define HNS3_TXD_L4LEN_S			24
#define HNS3_TXD_L4LEN_M			(0xffUL << HNS3_TXD_L4LEN_S)

#define HNS3_TXD_OL3T_S				0
#define HNS3_TXD_OL3T_M				(0x3 << HNS3_TXD_OL3T_S)
#define HNS3_TXD_OVLAN_B			2
#define HNS3_TXD_MACSEC_B			3
#define HNS3_TXD_TUNTYPE_S			4
#define HNS3_TXD_TUNTYPE_M			(0xf << HNS3_TXD_TUNTYPE_S)

#define HNS3_TXD_BDTYPE_S			0
#define HNS3_TXD_BDTYPE_M			(0xf << HNS3_TXD_BDTYPE_S)
#define HNS3_TXD_FE_B				4
#define HNS3_TXD_SC_S				5
#define HNS3_TXD_SC_M				(0x3 << HNS3_TXD_SC_S)
#define HNS3_TXD_EXTEND_B			7
#define HNS3_TXD_VLD_B				8
#define HNS3_TXD_RI_B				9
#define HNS3_TXD_RA_B				10
#define HNS3_TXD_TSYN_B				11
#define HNS3_TXD_DECTTL_S			12
#define HNS3_TXD_DECTTL_M			(0xf << HNS3_TXD_DECTTL_S)

#define HNS3_TXD_MSS_S				0
#define HNS3_TXD_MSS_M				(0x3fff << HNS3_TXD_MSS_S)

#define HNS3_L2_LEN_UNIT			1UL
#define HNS3_L3_LEN_UNIT			2UL
#define HNS3_L4_LEN_UNIT			2UL

enum hns3_pkt_l2t_type {
	HNS3_L2_TYPE_UNICAST,
	HNS3_L2_TYPE_MULTICAST,
	HNS3_L2_TYPE_BROADCAST,
	HNS3_L2_TYPE_INVALID,
};

enum hns3_pkt_l3t_type {
	HNS3_L3T_NONE,
	HNS3_L3T_IPV6,
	HNS3_L3T_IPV4,
	HNS3_L3T_RESERVED
};

enum hns3_pkt_l4t_type {
	HNS3_L4T_UNKNOWN,
	HNS3_L4T_TCP,
	HNS3_L4T_UDP,
	HNS3_L4T_SCTP
};

enum hns3_pkt_ol3t_type {
	HNS3_OL3T_NONE,
	HNS3_OL3T_IPV6,
	HNS3_OL3T_IPV4_NO_CSUM,
	HNS3_OL3T_IPV4_CSUM
};

enum hns3_pkt_tun_type {
	HNS3_TUN_NONE,
	HNS3_TUN_MAC_IN_UDP,
	HNS3_TUN_NVGRE,
	HNS3_TUN_OTHER
};

/* hardware spec ring buffer format */
struct hns3_desc {
	union {
		uint64_t addr;
		struct {
			uint32_t addr0;
			uint32_t addr1;
		};
	};
	union {
		struct {
			uint16_t vlan_tag;
			uint16_t send_size;
			union {
				/*
				 * L3T | L4T | L3CS | L4CS | VLAN | TSO |
				 * L2_LEN
				 */
				uint32_t type_cs_vlan_tso_len;
				struct {
					uint8_t type_cs_vlan_tso;
					uint8_t l2_len;
					uint8_t l3_len;
					uint8_t l4_len;
				};
			};
			uint16_t outer_vlan_tag;
			uint16_t tv;
			union {
				/* OL3T | OVALAN | MACSEC */
				uint32_t ol_type_vlan_len_msec;
				struct {
					uint8_t ol_type_vlan_msec;
					uint8_t ol2_len;
					uint8_t ol3_len;
					uint8_t ol4_len;
				};
			};

			uint32_t paylen;
			uint16_t tp_fe_sc_vld_ra_ri;
			uint16_t mss;
		} tx;

		struct {
			uint32_t l234_info;
			uint16_t pkt_len;
			uint16_t size;
			uint32_t rss_hash;
			uint16_t fd_id;
			uint16_t vlan_tag;
			union {
				uint32_t ol_info;
				struct {
					uint16_t o_dm_vlan_id_fb;
					uint16_t ot_vlan_tag;
				};
			};
			uint32_t bd_base_info;
		} rx;
	};
} __rte_packed;

struct hns3_entry {
	struct rte_mbuf *mbuf;
};

struct hns3_rx_queue {
	void *io_base;
	struct hns3_adapter *hns;
	struct rte_mempool *mb_pool;
	struct hns3_desc *rx_ring;
	uint64_t rx_ring_phys_addr; /* RX ring DMA address */
	const struct rte_memzone *mz;
	struct hns3_entry *sw_ring;

	struct rte_mbuf *pkt_first_seg;
	struct rte_mbuf *pkt_last_seg;

	uint16_t queue_id;
	uint16_t port_id;
	uint16_t nb_rx_desc;
	uint16_t nb_rx_hold;
	uint16_t rx_tail;
	uint16_t next_to_clean;
	uint16_t next_to_use;
	uint16_t rx_buf_len;
	uint16_t rx_free_thresh;

	bool rx_deferred_start; /* don't start this queue in dev start */
	bool configured;        /* indicate if rx queue has been configured */

	uint64_t non_vld_descs; /* num of non valid rx descriptors */
	uint64_t l2_errors;
	uint64_t pkt_len_errors;
	uint64_t l3_csum_erros;
	uint64_t l4_csum_erros;
	uint64_t ol3_csum_erros;
	uint64_t ol4_csum_erros;
};

struct hns3_tx_queue {
	void *io_base;
	struct hns3_adapter *hns;
	struct hns3_desc *tx_ring;
	uint64_t tx_ring_phys_addr; /* TX ring DMA address */
	const struct rte_memzone *mz;
	struct hns3_entry *sw_ring;

	uint16_t queue_id;
	uint16_t port_id;
	uint16_t nb_tx_desc;
	uint16_t next_to_clean;
	uint16_t next_to_use;
	uint16_t tx_bd_ready;

	bool tx_deferred_start; /* don't start this queue in dev start */
	bool configured;        /* indicate if tx queue has been configured */
};

struct hns3_queue_info {
	const char *type;   /* point to queue memory name */
	const char *ring_name;  /* point to hardware ring name */
	uint16_t idx;
	uint16_t nb_desc;
	unsigned int socket_id;
};

#define HNS3_TX_CKSUM_OFFLOAD_MASK ( \
	PKT_TX_OUTER_IPV6 | \
	PKT_TX_OUTER_IPV4 | \
	PKT_TX_OUTER_IP_CKSUM | \
	PKT_TX_IPV6 | \
	PKT_TX_IPV4 | \
	PKT_TX_IP_CKSUM | \
	PKT_TX_L4_MASK | \
	PKT_TX_TUNNEL_MASK)

enum hns3_cksum_status {
	HNS3_CKSUM_NONE = 0,
	HNS3_L3_CKSUM_ERR = 1,
	HNS3_L4_CKSUM_ERR = 2,
	HNS3_OUTER_L3_CKSUM_ERR = 4,
	HNS3_OUTER_L4_CKSUM_ERR = 8
};

void hns3_dev_rx_queue_release(void *queue);
void hns3_dev_tx_queue_release(void *queue);
void hns3_free_all_queues(struct rte_eth_dev *dev);
int hns3_reset_all_queues(struct hns3_adapter *hns);
int hns3_dev_rx_queue_intr_enable(struct rte_eth_dev *dev, uint16_t queue_id);
int hns3_dev_rx_queue_intr_disable(struct rte_eth_dev *dev, uint16_t queue_id);
int hns3_start_queues(struct hns3_adapter *hns, bool reset_queue);
int hns3_stop_queues(struct hns3_adapter *hns, bool reset_queue);
void hns3_dev_release_mbufs(struct hns3_adapter *hns);
int hns3_rx_queue_setup(struct rte_eth_dev *dev, uint16_t idx, uint16_t nb_desc,
			unsigned int socket, const struct rte_eth_rxconf *conf,
			struct rte_mempool *mp);
int hns3_tx_queue_setup(struct rte_eth_dev *dev, uint16_t idx, uint16_t nb_desc,
			unsigned int socket, const struct rte_eth_txconf *conf);
uint16_t hns3_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts,
			uint16_t nb_pkts);
uint16_t hns3_prep_pkts(__rte_unused void *tx_queue, struct rte_mbuf **tx_pkts,
			uint16_t nb_pkts);
uint16_t hns3_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts,
			uint16_t nb_pkts);
const uint32_t *hns3_dev_supported_ptypes_get(struct rte_eth_dev *dev);
void hns3_set_rxtx_function(struct rte_eth_dev *eth_dev);
void hns3_tqp_intr_enable(struct hns3_hw *hw, uint16_t tpq_int_num, bool en);
int hns3_set_fake_rx_or_tx_queues(struct rte_eth_dev *dev, uint16_t nb_rx_q,
				  uint16_t nb_tx_q);

#endif /* _HNS3_RXTX_H_ */
