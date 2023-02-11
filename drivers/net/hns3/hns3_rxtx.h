/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2021 HiSilicon Limited.
 */

#ifndef HNS3_RXTX_H
#define HNS3_RXTX_H

#include <stdint.h>

#include <ethdev_driver.h>
#include <rte_mbuf_core.h>
#include <rte_ethdev.h>
#include <rte_ethdev_core.h>
#include <rte_io.h>
#include <rte_mempool.h>
#include <rte_memzone.h>

#include "hns3_ethdev.h"

#define	HNS3_MIN_RING_DESC	64
#define	HNS3_MAX_RING_DESC	32768
#define HNS3_DEFAULT_RING_DESC  1024
#define	HNS3_ALIGN_RING_DESC	32
#define HNS3_RING_BASE_ALIGN	128
#define HNS3_BULK_ALLOC_MBUF_NUM	32

#define HNS3_DEFAULT_RX_FREE_THRESH	32
#define HNS3_DEFAULT_TX_FREE_THRESH	32
#define HNS3_DEFAULT_TX_RS_THRESH	32
#define HNS3_TX_FAST_FREE_AHEAD		64

#define HNS3_DEFAULT_RX_BURST		64
#if (HNS3_DEFAULT_RX_BURST > 64)
#error "PMD HNS3: HNS3_DEFAULT_RX_BURST must <= 64\n"
#endif
#define HNS3_DEFAULT_DESCS_PER_LOOP	4
#define HNS3_SVE_DEFAULT_DESCS_PER_LOOP	8
#if (HNS3_DEFAULT_DESCS_PER_LOOP > HNS3_SVE_DEFAULT_DESCS_PER_LOOP)
#define HNS3_VECTOR_RX_OFFSET_TABLE_LEN	HNS3_DEFAULT_DESCS_PER_LOOP
#else
#define HNS3_VECTOR_RX_OFFSET_TABLE_LEN	HNS3_SVE_DEFAULT_DESCS_PER_LOOP
#endif
#define HNS3_DEFAULT_RXQ_REARM_THRESH	64
#define HNS3_UINT8_BIT			8
#define HNS3_UINT16_BIT			16
#define HNS3_UINT32_BIT			32

#define HNS3_512_BD_BUF_SIZE	512
#define HNS3_1K_BD_BUF_SIZE	1024
#define HNS3_2K_BD_BUF_SIZE	2048
#define HNS3_4K_BD_BUF_SIZE	4096

#define HNS3_MIN_BD_BUF_SIZE	HNS3_512_BD_BUF_SIZE
#define HNS3_MAX_BD_BUF_SIZE	HNS3_4K_BD_BUF_SIZE

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
#define HNS3_RXD_TRUNCATE_B			19
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
#define HNS3_RXD_PTYPE_S			4
#define HNS3_RXD_PTYPE_M			(0xff << HNS3_RXD_PTYPE_S)
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

#define HNS3_RXD_TS_VLD_B			14
#define HNS3_RXD_GRO_SIZE_S			16
#define HNS3_RXD_GRO_SIZE_M			(0x3fff << HNS3_RXD_GRO_SIZE_S)

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

#define HNS3_TXD_OL4CS_B			22
#define HNS3_L2_LEN_UNIT			1UL
#define HNS3_L3_LEN_UNIT			2UL
#define HNS3_L4_LEN_UNIT			2UL

#define HNS3_TXD_DEFAULT_BDTYPE		0
#define HNS3_TXD_VLD_CMD		(0x1 << HNS3_TXD_VLD_B)
#define HNS3_TXD_FE_CMD			(0x1 << HNS3_TXD_FE_B)
#define HNS3_TXD_DEFAULT_VLD_FE_BDTYPE		\
		(HNS3_TXD_VLD_CMD | HNS3_TXD_FE_CMD | HNS3_TXD_DEFAULT_BDTYPE)
#define HNS3_TXD_SEND_SIZE_SHIFT	16

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
		uint64_t timestamp;

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

			uint32_t paylen_fd_dop_ol4cs;
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
			union {
				uint32_t bd_base_info;
				struct {
					uint16_t bdtype_vld_udp0;
					uint16_t fe_lum_crcp_l3l4p;
				};
			};
		} rx;
	};
} __rte_packed;

struct hns3_entry {
	struct rte_mbuf *mbuf;
};

struct hns3_rx_basic_stats {
	uint64_t packets;
	uint64_t bytes;
	uint64_t errors;
};

struct hns3_rx_dfx_stats {
	uint64_t l3_csum_errors;
	uint64_t l4_csum_errors;
	uint64_t ol3_csum_errors;
	uint64_t ol4_csum_errors;
};

struct hns3_rx_bd_errors_stats {
	uint64_t l2_errors;
	uint64_t pkt_len_errors;
};

struct hns3_rx_queue {
	volatile void *io_head_reg;
	struct hns3_ptype_table *ptype_tbl;
	struct rte_mempool *mb_pool;
	struct hns3_desc *rx_ring;
	struct hns3_entry *sw_ring;

	uint16_t port_id;
	uint16_t nb_rx_desc;
	/*
	 * threshold for the number of BDs waited to passed to hardware. If the
	 * number exceeds the threshold, driver will pass these BDs to hardware.
	 */
	uint16_t rx_free_thresh;
	uint16_t next_to_use;    /* index of next BD to be polled */
	uint16_t rx_free_hold;   /* num of BDs waited to passed to hardware */
	uint16_t rx_rearm_start; /* index of BD that driver re-arming from */
	uint16_t rx_rearm_nb;    /* number of remaining BDs to be re-armed */

	/* 4 if RTE_ETH_RX_OFFLOAD_KEEP_CRC offload set, 0 otherwise */
	uint8_t crc_len;

	/*
	 * Indicate whether ignore the outer VLAN field in the Rx BD reported
	 * by the Hardware. Because the outer VLAN is the PVID if the PVID is
	 * set for some version of hardware network engine whose vlan mode is
	 * HNS3_SW_SHIFT_AND_DISCARD_MODE, such as kunpeng 920. And this VLAN
	 * should not be transitted to the upper-layer application. For hardware
	 * network engine whose vlan mode is HNS3_HW_SHIFT_AND_DISCARD_MODE,
	 * such as kunpeng 930, PVID will not be reported to the BDs. So, PMD
	 * does not need to perform PVID-related operation in Rx. At this
	 * point, the pvid_sw_discard_en will be false.
	 */
	uint8_t pvid_sw_discard_en:1;
	uint8_t ptype_en:1;          /* indicate if the ptype field enabled */

	uint64_t mbuf_initializer; /* value to init mbufs used with vector rx */
	/* offset_table: used for vector, to solve execute re-order problem */
	uint8_t offset_table[HNS3_VECTOR_RX_OFFSET_TABLE_LEN + 1];

	uint16_t bulk_mbuf_num; /* indicate bulk_mbuf valid nums */

	struct hns3_rx_basic_stats basic_stats;

	struct rte_mbuf *pkt_first_seg;
	struct rte_mbuf *pkt_last_seg;

	struct rte_mbuf *bulk_mbuf[HNS3_BULK_ALLOC_MBUF_NUM];

	/* DFX statistics that driver does not need to discard packets */
	struct hns3_rx_dfx_stats dfx_stats;
	/* Error statistics that driver needs to discard packets */
	struct hns3_rx_bd_errors_stats err_stats;

	struct rte_mbuf fake_mbuf; /* fake mbuf used with vector rx */

	/*
	 * The following fields are not accessed in the I/O path, so they are
	 * placed at the end.
	 */
	void *io_base __rte_cache_aligned;
	struct hns3_adapter *hns;
	uint64_t rx_ring_phys_addr; /* RX ring DMA address */
	const struct rte_memzone *mz;

	uint16_t queue_id;
	uint16_t rx_buf_len;

	bool configured;        /* indicate if rx queue has been configured */
	bool rx_deferred_start; /* don't start this queue in dev start */
	bool enabled;           /* indicate if Rx queue has been enabled */
};

struct hns3_tx_basic_stats {
	uint64_t packets;
	uint64_t bytes;
};

/*
 * The following items are used for the abnormal errors statistics in
 * the Tx datapath. When upper level application calls the
 * rte_eth_tx_burst API function to send multiple packets at a time with
 * burst mode based on hns3 network engine, there are some abnormal
 * conditions that cause the driver to fail to operate the hardware to
 * send packets correctly.
 * Note: When using burst mode to call the rte_eth_tx_burst API function
 * to send multiple packets at a time. When the first abnormal error is
 * detected, add one to the relevant error statistics item, and then
 * exit the loop of sending multiple packets of the function. That is to
 * say, even if there are multiple packets in which abnormal errors may
 * be detected in the burst, the relevant error statistics in the driver
 * will only be increased by one.
 * The detail description of the Tx abnormal errors statistic items as
 * below:
 *  - over_length_pkt_cnt
 *     Total number of greater than HNS3_MAX_FRAME_LEN the driver
 *     supported.
 *
 * - exceed_limit_bd_pkt_cnt
 *     Total number of exceeding the hardware limited bd which process
 *     a packet needed bd numbers.
 *
 * - exceed_limit_bd_reassem_fail
 *     Total number of exceeding the hardware limited bd fail which
 *     process a packet needed bd numbers and reassemble fail.
 *
 * - unsupported_tunnel_pkt_cnt
 *     Total number of unsupported tunnel packet. The unsupported tunnel
 *     type: vxlan_gpe, gtp, ipip and MPLSINUDP, MPLSINUDP is a packet
 *     with MPLS-in-UDP RFC 7510 header.
 *
 * - queue_full_cnt
 *     Total count which the available bd numbers in current bd queue is
 *     less than the bd numbers with the pkt process needed.
 *
 * - pkt_padding_fail_cnt
 *     Total count which the packet length is less than minimum packet
 *     length(struct hns3_tx_queue::min_tx_pkt_len) supported by
 *     hardware in Tx direction and fail to be appended with 0.
 */
struct hns3_tx_dfx_stats {
	uint64_t over_length_pkt_cnt;
	uint64_t exceed_limit_bd_pkt_cnt;
	uint64_t exceed_limit_bd_reassem_fail;
	uint64_t unsupported_tunnel_pkt_cnt;
	uint64_t queue_full_cnt;
	uint64_t pkt_padding_fail_cnt;
};

struct hns3_tx_queue {
	/* The io_tail_reg is write-only if working in tx push mode */
	volatile void *io_tail_reg;
	struct hns3_desc *tx_ring;
	struct hns3_entry *sw_ring;

	uint16_t nb_tx_desc;
	/*
	 * index of next BD whose corresponding rte_mbuf can be released by
	 * driver.
	 */
	uint16_t next_to_clean;
	/* index of next BD to be filled by driver to send packet */
	uint16_t next_to_use;
	/* num of remaining BDs ready to be filled by driver to send packet */
	uint16_t tx_bd_ready;

	/* threshold for free tx buffer if available BDs less than this value */
	uint16_t tx_free_thresh;

	/*
	 * The minimum length of the packet supported by hardware in the Tx
	 * direction.
	 */
	uint8_t min_tx_pkt_len;

	uint8_t max_non_tso_bd_num; /* max BD number of one non-TSO packet */

	/*
	 * tso mode.
	 * value range:
	 *      HNS3_TSO_SW_CAL_PSEUDO_H_CSUM/HNS3_TSO_HW_CAL_PSEUDO_H_CSUM
	 *
	 *  - HNS3_TSO_SW_CAL_PSEUDO_H_CSUM
	 *     In this mode, because of the hardware constraint, network driver
	 *     software need erase the L4 len value of the TCP pseudo header
	 *     and recalculate the TCP pseudo header checksum of packets that
	 *     need TSO.
	 *
	 *  - HNS3_TSO_HW_CAL_PSEUDO_H_CSUM
	 *     In this mode, hardware support recalculate the TCP pseudo header
	 *     checksum of packets that need TSO, so network driver software
	 *     not need to recalculate it.
	 */
	uint16_t tso_mode:1;
	/*
	 * udp checksum mode.
	 * value range:
	 *      HNS3_SPECIAL_PORT_HW_CKSUM_MODE/HNS3_SPECIAL_PORT_SW_CKSUM_MODE
	 *
	 *  - HNS3_SPECIAL_PORT_SW_CKSUM_MODE
	 *     In this mode, HW can not do checksum for special UDP port like
	 *     4789, 4790, 6081 for non-tunnel UDP packets and UDP tunnel
	 *     packets without the RTE_MBUF_F_TX_TUNEL_MASK in the mbuf. So, PMD need
	 *     do the checksum for these packets to avoid a checksum error.
	 *
	 *  - HNS3_SPECIAL_PORT_HW_CKSUM_MODE
	 *     In this mode, HW does not have the preceding problems and can
	 *     directly calculate the checksum of these UDP packets.
	 */
	uint16_t udp_cksum_mode:1;

	uint16_t simple_bd_enable:1;
	uint16_t tx_push_enable:1;    /* check whether the tx push is enabled */
	/*
	 * Indicate whether add the vlan_tci of the mbuf to the inner VLAN field
	 * of Tx BD. Because the outer VLAN will always be the PVID when the
	 * PVID is set and for some version of hardware network engine whose
	 * vlan mode is HNS3_SW_SHIFT_AND_DISCARD_MODE, such as kunpeng 920, the
	 * PVID will overwrite the outer VLAN field of Tx BD. For the hardware
	 * network engine whose vlan mode is HNS3_HW_SHIFT_AND_DISCARD_MODE,
	 * such as kunpeng 930, if the PVID is set, the hardware will shift the
	 * VLAN field automatically. So, PMD does not need to do
	 * PVID-related operations in Tx. And pvid_sw_shift_en will be false at
	 * this point.
	 */
	uint16_t pvid_sw_shift_en:1;
	/* check whether the mbuf fast free offload is enabled */
	uint16_t mbuf_fast_free_en:1;

	/*
	 * For better performance in tx datapath, releasing mbuf in batches is
	 * required.
	 * Only checking the VLD bit of the last descriptor in a batch of the
	 * thresh descriptors does not mean that these descriptors are all sent
	 * by hardware successfully. So we need to check that the VLD bits of
	 * all descriptors are cleared. and then free all mbufs in the batch.
	 * - tx_rs_thresh
	 *   Number of mbufs released at a time.
	 *
	 * - free
	 *   Tx mbuf free array used for preserving temporarily address of mbuf
	 *   released back to mempool, when releasing mbuf in batches.
	 */
	uint16_t tx_rs_thresh;
	struct rte_mbuf **free;

	struct hns3_tx_basic_stats basic_stats;
	struct hns3_tx_dfx_stats dfx_stats;

	/*
	 * The following fields are not accessed in the I/O path, so they are
	 * placed at the end.
	 */
	void *io_base __rte_cache_aligned;
	struct hns3_adapter *hns;
	uint64_t tx_ring_phys_addr; /* TX ring DMA address */
	const struct rte_memzone *mz;

	uint16_t port_id;
	uint16_t queue_id;

	bool configured;        /* indicate if tx queue has been configured */
	bool tx_deferred_start; /* don't start this queue in dev start */
	bool enabled;           /* indicate if Tx queue has been enabled */
};

#define HNS3_GET_TX_QUEUE_PEND_BD_NUM(txq) \
		((txq)->nb_tx_desc - 1 - (txq)->tx_bd_ready)

struct hns3_queue_info {
	const char *type;   /* point to queue memory name */
	const char *ring_name;  /* point to hardware ring name */
	uint16_t idx;
	uint16_t nb_desc;
	unsigned int socket_id;
};

#define HNS3_TX_CKSUM_OFFLOAD_MASK (RTE_MBUF_F_TX_OUTER_UDP_CKSUM | \
	RTE_MBUF_F_TX_OUTER_IP_CKSUM | \
	RTE_MBUF_F_TX_IP_CKSUM | \
	RTE_MBUF_F_TX_TCP_SEG | \
	RTE_MBUF_F_TX_L4_MASK)

enum hns3_cksum_status {
	HNS3_CKSUM_NONE = 0,
	HNS3_L3_CKSUM_ERR = 1,
	HNS3_L4_CKSUM_ERR = 2,
	HNS3_OUTER_L3_CKSUM_ERR = 4,
	HNS3_OUTER_L4_CKSUM_ERR = 8
};

extern uint64_t hns3_timestamp_rx_dynflag;
extern int hns3_timestamp_dynfield_offset;

static inline void
hns3_rx_set_cksum_flag(struct hns3_rx_queue *rxq,
		       struct rte_mbuf *rxm,
		       uint32_t l234_info)
{
#define HNS3_RXD_CKSUM_ERR_MASK	(BIT(HNS3_RXD_L3E_B) | \
				 BIT(HNS3_RXD_L4E_B) | \
				 BIT(HNS3_RXD_OL3E_B) | \
				 BIT(HNS3_RXD_OL4E_B))

	if (likely((l234_info & HNS3_RXD_CKSUM_ERR_MASK) == 0)) {
		rxm->ol_flags |= (RTE_MBUF_F_RX_IP_CKSUM_GOOD | RTE_MBUF_F_RX_L4_CKSUM_GOOD);
		return;
	}

	if (unlikely(l234_info & BIT(HNS3_RXD_L3E_B))) {
		rxm->ol_flags |= RTE_MBUF_F_RX_IP_CKSUM_BAD;
		rxq->dfx_stats.l3_csum_errors++;
	} else {
		rxm->ol_flags |= RTE_MBUF_F_RX_IP_CKSUM_GOOD;
	}

	if (unlikely(l234_info & BIT(HNS3_RXD_L4E_B))) {
		rxm->ol_flags |= RTE_MBUF_F_RX_L4_CKSUM_BAD;
		rxq->dfx_stats.l4_csum_errors++;
	} else {
		rxm->ol_flags |= RTE_MBUF_F_RX_L4_CKSUM_GOOD;
	}

	if (unlikely(l234_info & BIT(HNS3_RXD_OL3E_B)))
		rxq->dfx_stats.ol3_csum_errors++;

	if (unlikely(l234_info & BIT(HNS3_RXD_OL4E_B))) {
		rxm->ol_flags |= RTE_MBUF_F_RX_OUTER_L4_CKSUM_BAD;
		rxq->dfx_stats.ol4_csum_errors++;
	}
}

static inline int
hns3_handle_bdinfo(struct hns3_rx_queue *rxq, struct rte_mbuf *rxm,
		   uint32_t bd_base_info, uint32_t l234_info)
{
#define L2E_TRUNC_ERR_FLAG	(BIT(HNS3_RXD_L2E_B) | \
				 BIT(HNS3_RXD_TRUNCATE_B))

	/*
	 * If packet len bigger than mtu when recv with no-scattered algorithm,
	 * the first n bd will without FE bit, we need process this situation.
	 * Note: we don't need add statistic counter because latest BD which
	 *       with FE bit will mark HNS3_RXD_L2E_B bit.
	 */
	if (unlikely((bd_base_info & BIT(HNS3_RXD_FE_B)) == 0))
		return -EINVAL;

	if (unlikely((l234_info & L2E_TRUNC_ERR_FLAG) || rxm->pkt_len == 0)) {
		if (l234_info & BIT(HNS3_RXD_L2E_B))
			rxq->err_stats.l2_errors++;
		else
			rxq->err_stats.pkt_len_errors++;
		return -EINVAL;
	}

	if (bd_base_info & BIT(HNS3_RXD_L3L4P_B))
		hns3_rx_set_cksum_flag(rxq, rxm, l234_info);

	return 0;
}

static inline uint32_t
hns3_rx_calc_ptype(struct hns3_rx_queue *rxq, const uint32_t l234_info,
		   const uint32_t ol_info)
{
	const struct hns3_ptype_table * const ptype_tbl = rxq->ptype_tbl;
	uint32_t ol3id, ol4id;
	uint32_t l3id, l4id;
	uint32_t ptype;

	if (rxq->ptype_en) {
		ptype = hns3_get_field(ol_info, HNS3_RXD_PTYPE_M,
				       HNS3_RXD_PTYPE_S);
		return ptype_tbl->ptype[ptype];
	}

	ol4id = hns3_get_field(ol_info, HNS3_RXD_OL4ID_M, HNS3_RXD_OL4ID_S);
	ol3id = hns3_get_field(ol_info, HNS3_RXD_OL3ID_M, HNS3_RXD_OL3ID_S);
	l3id = hns3_get_field(l234_info, HNS3_RXD_L3ID_M, HNS3_RXD_L3ID_S);
	l4id = hns3_get_field(l234_info, HNS3_RXD_L4ID_M, HNS3_RXD_L4ID_S);

	if (unlikely(ptype_tbl->ol4table[ol4id]))
		return ptype_tbl->inner_l3table[l3id] |
			ptype_tbl->inner_l4table[l4id] |
			ptype_tbl->ol3table[ol3id] |
			ptype_tbl->ol4table[ol4id];
	else
		return ptype_tbl->l3table[l3id] | ptype_tbl->l4table[l4id];
}

/*
 * If enable using Tx push feature and also device support it, then use quick
 * doorbell (bar45) to inform the hardware.
 *
 * The other cases (such as: device don't support or user don't enable using)
 * then use normal doorbell (bar23) to inform the hardware.
 */
static inline void
hns3_write_txq_tail_reg(struct hns3_tx_queue *txq, uint32_t value)
{
	rte_io_wmb();
	if (txq->tx_push_enable)
		rte_write64_relaxed(rte_cpu_to_le_32(value), txq->io_tail_reg);
	else
		rte_write32_relaxed(rte_cpu_to_le_32(value), txq->io_tail_reg);
}

void hns3_dev_rx_queue_release(struct rte_eth_dev *dev, uint16_t queue_id);
void hns3_dev_tx_queue_release(struct rte_eth_dev *dev, uint16_t queue_id);
void hns3_free_all_queues(struct rte_eth_dev *dev);
int hns3_reset_all_tqps(struct hns3_adapter *hns);
void hns3_dev_all_rx_queue_intr_enable(struct hns3_hw *hw, bool en);
int hns3_dev_rx_queue_intr_enable(struct rte_eth_dev *dev, uint16_t queue_id);
int hns3_dev_rx_queue_intr_disable(struct rte_eth_dev *dev, uint16_t queue_id);
void hns3_enable_all_queues(struct hns3_hw *hw, bool en);
int hns3_init_queues(struct hns3_adapter *hns, bool reset_queue);
void hns3_start_tqps(struct hns3_hw *hw);
void hns3_stop_tqps(struct hns3_hw *hw);
int hns3_rxq_iterate(struct rte_eth_dev *dev,
		 int (*callback)(struct hns3_rx_queue *, void *), void *arg);
void hns3_dev_release_mbufs(struct hns3_adapter *hns);
int hns3_rx_queue_setup(struct rte_eth_dev *dev, uint16_t idx, uint16_t nb_desc,
			unsigned int socket_id,
			const struct rte_eth_rxconf *conf,
			struct rte_mempool *mp);
int hns3_tx_queue_setup(struct rte_eth_dev *dev, uint16_t idx, uint16_t nb_desc,
			unsigned int socket_id,
			const struct rte_eth_txconf *conf);
uint32_t hns3_rx_queue_count(void *rx_queue);
int hns3_dev_rx_queue_start(struct rte_eth_dev *dev, uint16_t rx_queue_id);
int hns3_dev_rx_queue_stop(struct rte_eth_dev *dev, uint16_t rx_queue_id);
int hns3_dev_tx_queue_start(struct rte_eth_dev *dev, uint16_t tx_queue_id);
int hns3_dev_tx_queue_stop(struct rte_eth_dev *dev, uint16_t tx_queue_id);
uint16_t hns3_recv_pkts_simple(void *rx_queue, struct rte_mbuf **rx_pkts,
				uint16_t nb_pkts);
uint16_t hns3_recv_scattered_pkts(void *rx_queue, struct rte_mbuf **rx_pkts,
				  uint16_t nb_pkts);
uint16_t hns3_recv_pkts_vec(void *__restrict rx_queue,
			    struct rte_mbuf **__restrict rx_pkts,
			    uint16_t nb_pkts);
uint16_t hns3_recv_pkts_vec_sve(void *__restrict rx_queue,
				struct rte_mbuf **__restrict rx_pkts,
				uint16_t nb_pkts);
int hns3_rx_burst_mode_get(struct rte_eth_dev *dev,
			   __rte_unused uint16_t queue_id,
			   struct rte_eth_burst_mode *mode);
uint16_t hns3_prep_pkts(__rte_unused void *tx_queue, struct rte_mbuf **tx_pkts,
			uint16_t nb_pkts);
uint16_t hns3_xmit_pkts_simple(void *tx_queue, struct rte_mbuf **tx_pkts,
			       uint16_t nb_pkts);
uint16_t hns3_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts,
			uint16_t nb_pkts);
uint16_t hns3_xmit_pkts_vec(void *tx_queue, struct rte_mbuf **tx_pkts,
							uint16_t nb_pkts);
uint16_t hns3_xmit_pkts_vec_sve(void *tx_queue, struct rte_mbuf **tx_pkts,
				uint16_t nb_pkts);
int hns3_tx_burst_mode_get(struct rte_eth_dev *dev,
			   __rte_unused uint16_t queue_id,
			   struct rte_eth_burst_mode *mode);
const uint32_t *hns3_dev_supported_ptypes_get(struct rte_eth_dev *dev);
void hns3_init_rx_ptype_tble(struct rte_eth_dev *dev);
void hns3_set_rxtx_function(struct rte_eth_dev *eth_dev);
uint32_t hns3_get_tqp_intr_reg_offset(uint16_t tqp_intr_id);
void hns3_set_queue_intr_gl(struct hns3_hw *hw, uint16_t queue_id,
			    uint8_t gl_idx, uint16_t gl_value);
void hns3_set_queue_intr_rl(struct hns3_hw *hw, uint16_t queue_id,
			    uint16_t rl_value);
void hns3_set_queue_intr_ql(struct hns3_hw *hw, uint16_t queue_id,
			    uint16_t ql_value);
int hns3_set_fake_rx_or_tx_queues(struct rte_eth_dev *dev, uint16_t nb_rx_q,
				  uint16_t nb_tx_q);
int hns3_config_gro(struct hns3_hw *hw, bool en);
int hns3_restore_gro_conf(struct hns3_hw *hw);
void hns3_update_all_queues_pvid_proc_en(struct hns3_hw *hw);
void hns3_rx_scattered_reset(struct rte_eth_dev *dev);
void hns3_rx_scattered_calc(struct rte_eth_dev *dev);
int hns3_rx_check_vec_support(struct rte_eth_dev *dev);
int hns3_tx_check_vec_support(struct rte_eth_dev *dev);
void hns3_rxq_vec_setup(struct hns3_rx_queue *rxq);
void hns3_rxq_info_get(struct rte_eth_dev *dev, uint16_t queue_id,
		       struct rte_eth_rxq_info *qinfo);
void hns3_txq_info_get(struct rte_eth_dev *dev, uint16_t queue_id,
		       struct rte_eth_txq_info *qinfo);
uint32_t hns3_get_tqp_reg_offset(uint16_t queue_id);
int hns3_start_all_txqs(struct rte_eth_dev *dev);
int hns3_start_all_rxqs(struct rte_eth_dev *dev);
void hns3_stop_all_txqs(struct rte_eth_dev *dev);
void hns3_restore_tqp_enable_state(struct hns3_hw *hw);
int hns3_tx_done_cleanup(void *txq, uint32_t free_cnt);
void hns3_enable_rxd_adv_layout(struct hns3_hw *hw);
int hns3_dev_rx_descriptor_status(void *rx_queue, uint16_t offset);
int hns3_dev_tx_descriptor_status(void *tx_queue, uint16_t offset);
void hns3_tx_push_init(struct rte_eth_dev *dev);
void hns3_stop_tx_datapath(struct rte_eth_dev *dev);
void hns3_start_tx_datapath(struct rte_eth_dev *dev);
void hns3_stop_rxtx_datapath(struct rte_eth_dev *dev);
void hns3_start_rxtx_datapath(struct rte_eth_dev *dev);

#endif /* HNS3_RXTX_H */
