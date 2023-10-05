/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2023 Broadcom
 * All rights reserved.
 */

#ifndef _BNXT_H_
#define _BNXT_H_

#include <inttypes.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/queue.h>

#include <rte_pci.h>
#include <bus_pci_driver.h>
#include <ethdev_driver.h>
#include <rte_memory.h>
#include <rte_lcore.h>
#include <rte_spinlock.h>
#include <rte_time.h>

#include "bnxt_cpr.h"
#include "bnxt_util.h"

#include "tf_core.h"
#include "bnxt_ulp.h"
#include "bnxt_tf_common.h"
#include "bnxt_vnic.h"

/* Vendor ID */
#define PCI_VENDOR_ID_BROADCOM		0x14E4

/* Device IDs */
#define BROADCOM_DEV_ID_STRATUS_NIC_VF1 0x1606
#define BROADCOM_DEV_ID_STRATUS_NIC_VF2 0x1609
#define BROADCOM_DEV_ID_STRATUS_NIC	0x1614
#define BROADCOM_DEV_ID_57414_VF	0x16c1
#define BROADCOM_DEV_ID_57304_VF	0x16cb
#define BROADCOM_DEV_ID_57417_MF	0x16cc
#define BROADCOM_DEV_ID_NS2		0x16cd
#define BROADCOM_DEV_ID_57406_VF	0x16d3
#define BROADCOM_DEV_ID_57412		0x16d6
#define BROADCOM_DEV_ID_57414		0x16d7
#define BROADCOM_DEV_ID_57416_RJ45	0x16d8
#define BROADCOM_DEV_ID_57417_RJ45	0x16d9
#define BROADCOM_DEV_ID_5741X_VF	0x16dc
#define BROADCOM_DEV_ID_57412_MF	0x16de
#define BROADCOM_DEV_ID_57317_RJ45	0x16e0
#define BROADCOM_DEV_ID_5731X_VF	0x16e1
#define BROADCOM_DEV_ID_57417_SFP	0x16e2
#define BROADCOM_DEV_ID_57416_SFP	0x16e3
#define BROADCOM_DEV_ID_57317_SFP	0x16e4
#define BROADCOM_DEV_ID_57407_MF	0x16ea
#define BROADCOM_DEV_ID_57414_MF	0x16ec
#define BROADCOM_DEV_ID_57416_MF	0x16ee
#define BROADCOM_DEV_ID_57508		0x1750
#define BROADCOM_DEV_ID_57504		0x1751
#define BROADCOM_DEV_ID_57502		0x1752
#define BROADCOM_DEV_ID_57508_MF1	0x1800
#define BROADCOM_DEV_ID_57504_MF1	0x1801
#define BROADCOM_DEV_ID_57502_MF1	0x1802
#define BROADCOM_DEV_ID_57508_MF2	0x1803
#define BROADCOM_DEV_ID_57504_MF2	0x1804
#define BROADCOM_DEV_ID_57502_MF2	0x1805
#define BROADCOM_DEV_ID_57500_VF1	0x1806
#define BROADCOM_DEV_ID_57500_VF2	0x1807
#define BROADCOM_DEV_ID_58802		0xd802
#define BROADCOM_DEV_ID_58804		0xd804
#define BROADCOM_DEV_ID_58808		0x16f0
#define BROADCOM_DEV_ID_58802_VF	0xd800
#define BROADCOM_DEV_ID_58812		0xd812
#define BROADCOM_DEV_ID_58814		0xd814
#define BROADCOM_DEV_ID_58818		0xd818
#define BROADCOM_DEV_ID_58818_VF	0xd82e

#define BROADCOM_DEV_957508_N2100	0x5208
#define BROADCOM_DEV_957414_N225	0x4145

#define BNXT_MAX_MTU		9574
#define BNXT_NUM_VLANS		2
#define BNXT_MAX_PKT_LEN	(BNXT_MAX_MTU + RTE_ETHER_HDR_LEN +\
				 RTE_ETHER_CRC_LEN +\
				 (BNXT_NUM_VLANS * RTE_VLAN_HLEN))
/* FW adds extra 4 bytes for FCS */
#define BNXT_VNIC_MRU(mtu)\
	((mtu) + RTE_ETHER_HDR_LEN + RTE_VLAN_HLEN * BNXT_NUM_VLANS)
#define BNXT_VF_RSV_NUM_RSS_CTX	1
#define BNXT_VF_RSV_NUM_L2_CTX	4
/* TODO: For now, do not support VMDq/RFS on VFs. */
#define BNXT_VF_RSV_NUM_VNIC	1
#define BNXT_MAX_LED		4
#define BNXT_MIN_RING_DESC	16
#define BNXT_MAX_TX_RING_DESC	4096
#define BNXT_MAX_RX_RING_DESC	8192
#define BNXT_DB_SIZE		0x80

#define TPA_MAX_AGGS		64
#define TPA_MAX_AGGS_TH		1024

#define TPA_MAX_NUM_SEGS	32
#define TPA_MAX_SEGS_TH		8 /* 32 segments in 4-segment units */
#define TPA_MAX_SEGS		5 /* 32 segments in log2 units */

#define BNXT_TPA_MAX_AGGS(bp) \
	(BNXT_CHIP_P5(bp) ? TPA_MAX_AGGS_TH : \
			     TPA_MAX_AGGS)

#define BNXT_TPA_MAX_SEGS(bp) \
	(BNXT_CHIP_P5(bp) ? TPA_MAX_SEGS_TH : \
			      TPA_MAX_SEGS)

/*
 * Define the number of async completion rings to be used. Set to zero for
 * configurations in which the maximum number of packet completion rings
 * for packet completions is desired or when async completion handling
 * cannot be interrupt-driven.
 */
#ifdef RTE_EXEC_ENV_FREEBSD
/* In FreeBSD OS, nic_uio driver does not support interrupts */
#define BNXT_NUM_ASYNC_CPR(bp) 0U
#else
#define BNXT_NUM_ASYNC_CPR(bp) 1U
#endif

#define BNXT_MISC_VEC_ID               RTE_INTR_VEC_ZERO_OFFSET
#define BNXT_RX_VEC_START              RTE_INTR_VEC_RXTX_OFFSET

/* Chimp Communication Channel */
#define GRCPF_REG_CHIMP_CHANNEL_OFFSET		0x0
#define GRCPF_REG_CHIMP_COMM_TRIGGER		0x100
/* Kong Communication Channel */
#define GRCPF_REG_KONG_CHANNEL_OFFSET		0xA00
#define GRCPF_REG_KONG_COMM_TRIGGER		0xB00

#define BNXT_INT_LAT_TMR_MIN			75
#define BNXT_INT_LAT_TMR_MAX			150
#define BNXT_NUM_CMPL_AGGR_INT			36
#define BNXT_CMPL_AGGR_DMA_TMR			37
#define BNXT_NUM_CMPL_DMA_AGGR			36
#define BNXT_CMPL_AGGR_DMA_TMR_DURING_INT	50
#define BNXT_NUM_CMPL_DMA_AGGR_DURING_INT	12
#define BNXT_DEVICE_SERIAL_NUM_SIZE		8

#define	BNXT_DEFAULT_VNIC_STATE_MASK			\
	HWRM_ASYNC_EVENT_CMPL_DEFAULT_VNIC_CHANGE_EVENT_DATA1_DEF_VNIC_STATE_MASK
#define	BNXT_DEFAULT_VNIC_STATE_SFT			\
	HWRM_ASYNC_EVENT_CMPL_DEFAULT_VNIC_CHANGE_EVENT_DATA1_DEF_VNIC_STATE_SFT
#define	BNXT_DEFAULT_VNIC_ALLOC				\
	HWRM_ASYNC_EVENT_CMPL_DEFAULT_VNIC_CHANGE_EVENT_DATA1_DEF_VNIC_STATE_DEF_VNIC_ALLOC
#define	BNXT_DEFAULT_VNIC_FREE				\
	HWRM_ASYNC_EVENT_CMPL_DEFAULT_VNIC_CHANGE_EVENT_DATA1_DEF_VNIC_STATE_DEF_VNIC_FREE
#define	BNXT_DEFAULT_VNIC_CHANGE_PF_ID_MASK		\
	HWRM_ASYNC_EVENT_CMPL_DEFAULT_VNIC_CHANGE_EVENT_DATA1_PF_ID_MASK
#define	BNXT_DEFAULT_VNIC_CHANGE_PF_ID_SFT		\
	HWRM_ASYNC_EVENT_CMPL_DEFAULT_VNIC_CHANGE_EVENT_DATA1_PF_ID_SFT
#define	BNXT_DEFAULT_VNIC_CHANGE_VF_ID_MASK		\
	HWRM_ASYNC_EVENT_CMPL_DEFAULT_VNIC_CHANGE_EVENT_DATA1_VF_ID_MASK
#define	BNXT_DEFAULT_VNIC_CHANGE_VF_ID_SFT		\
	HWRM_ASYNC_EVENT_CMPL_DEFAULT_VNIC_CHANGE_EVENT_DATA1_VF_ID_SFT

#define BNXT_EVENT_ERROR_REPORT_TYPE(data1)				\
	(((data1) &							\
	  HWRM_ASYNC_EVENT_CMPL_ERROR_REPORT_BASE_EVENT_DATA1_ERROR_TYPE_MASK)  >>\
	 HWRM_ASYNC_EVENT_CMPL_ERROR_REPORT_BASE_EVENT_DATA1_ERROR_TYPE_SFT)

#define BNXT_HWRM_CMD_TO_FORWARD(cmd)	\
		(bp->pf->vf_req_fwd[(cmd) / 32] |= (1 << ((cmd) % 32)))

#define BNXT_NTOHS              rte_be_to_cpu_16

struct bnxt_led_info {
	uint8_t	     num_leds;
	uint8_t      led_id;
	uint8_t      led_type;
	uint8_t      led_group_id;
	uint8_t      unused;
	uint16_t  led_state_caps;
#define BNXT_LED_ALT_BLINK_CAP(x)       ((x) &  \
	rte_cpu_to_le_16(HWRM_PORT_LED_QCFG_OUTPUT_LED0_STATE_BLINKALT))

	uint16_t  led_color_caps;
};

struct bnxt_led_cfg {
	uint8_t led_id;
	uint8_t led_state;
	uint8_t led_color;
	uint8_t unused;
	uint16_t led_blink_on;
	uint16_t led_blink_off;
	uint8_t led_group_id;
	uint8_t rsvd;
};

#define BNXT_LED_DFLT_ENA                               \
	(HWRM_PORT_LED_CFG_INPUT_ENABLES_LED0_ID |             \
	 HWRM_PORT_LED_CFG_INPUT_ENABLES_LED0_STATE |          \
	 HWRM_PORT_LED_CFG_INPUT_ENABLES_LED0_BLINK_ON |       \
	 HWRM_PORT_LED_CFG_INPUT_ENABLES_LED0_BLINK_OFF |      \
	 HWRM_PORT_LED_CFG_INPUT_ENABLES_LED0_GROUP_ID)

#define BNXT_LED_DFLT_ENA_SHIFT		6

#define BNXT_LED_DFLT_ENABLES(x)                        \
	rte_cpu_to_le_32(BNXT_LED_DFLT_ENA << (BNXT_LED_DFLT_ENA_SHIFT * (x)))

struct bnxt_vlan_table_entry {
	uint16_t		tpid;
	uint16_t		vid;
} __rte_packed;

struct bnxt_vlan_antispoof_table_entry {
	uint16_t		tpid;
	uint16_t		vid;
	uint16_t		mask;
} __rte_packed;

struct bnxt_child_vf_info {
	void			*req_buf;
	struct bnxt_vlan_table_entry	*vlan_table;
	struct bnxt_vlan_antispoof_table_entry	*vlan_as_table;
	STAILQ_HEAD(, bnxt_filter_info)	filter;
	uint32_t		func_cfg_flags;
	uint32_t		l2_rx_mask;
	uint16_t		fid;
	uint16_t		max_tx_rate;
	uint16_t		dflt_vlan;
	uint16_t		vlan_count;
	uint8_t			mac_spoof_en;
	uint8_t			vlan_spoof_en;
	bool			random_mac;
	bool			persist_stats;
};

struct bnxt_parent_info {
#define	BNXT_PF_FID_INVALID	0xFFFF
	uint16_t		fid;
	uint16_t		vnic;
	uint16_t		port_id;
	uint8_t			mac_addr[RTE_ETHER_ADDR_LEN];
};

struct bnxt_pf_info {
#define BNXT_FIRST_PF_FID	1
#define BNXT_MAX_VFS(bp)	((bp)->pf->max_vfs)
#define BNXT_MAX_VF_REPS_P4     64
#define BNXT_MAX_VF_REPS_P5     256
#define BNXT_MAX_VF_REPS(bp) \
				(BNXT_CHIP_P5(bp) ? BNXT_MAX_VF_REPS_P5 : \
				BNXT_MAX_VF_REPS_P4)
#define BNXT_TOTAL_VFS(bp)	((bp)->pf->total_vfs)
#define BNXT_FIRST_VF_FID	128
#define BNXT_PF_RINGS_USED(bp)	bnxt_get_num_queues(bp)
#define BNXT_PF_RINGS_AVAIL(bp)	((bp)->pf->max_cp_rings - \
				 BNXT_PF_RINGS_USED(bp))
	uint16_t		port_id;
	uint16_t		first_vf_id;
	uint16_t		active_vfs;
	uint16_t		max_vfs;
	uint16_t		total_vfs; /* Total VFs possible.
					    * Not necessarily enabled.
					    */
	uint32_t		func_cfg_flags;
	void			*vf_req_buf;
	rte_iova_t		vf_req_buf_dma_addr;
	uint32_t		vf_req_fwd[8];
	uint16_t		total_vnics;
	struct bnxt_child_vf_info	*vf_info;
#define BNXT_EVB_MODE_NONE	0
#define BNXT_EVB_MODE_VEB	1
#define BNXT_EVB_MODE_VEPA	2
	uint8_t			evb_mode;
};

/* Max wait time for link up is 10s and link down is 500ms */
#define BNXT_MAX_LINK_WAIT_CNT	200
#define BNXT_MIN_LINK_WAIT_CNT	10
#define BNXT_LINK_WAIT_INTERVAL	50
struct bnxt_link_info {
	uint32_t		phy_flags;
	uint8_t			mac_type;
	uint8_t			phy_link_status;
	uint8_t			loop_back;
	uint8_t			link_up;
	uint8_t			duplex;
	uint8_t			pause;
	uint8_t			force_pause;
	uint8_t			auto_pause;
	uint8_t			auto_mode;
#define PHY_VER_LEN		3
	uint8_t			phy_ver[PHY_VER_LEN];
	uint16_t		link_speed;
	uint16_t		support_speeds;
	uint16_t		auto_link_speed;
	uint16_t		force_link_speed;
	uint16_t		auto_link_speed_mask;
	uint32_t		preemphasis;
	uint8_t			phy_type;
	uint8_t			media_type;
	uint16_t		support_auto_speeds;
	uint8_t			link_signal_mode;
	uint16_t		force_pam4_link_speed;
	uint16_t		support_pam4_speeds;
	uint16_t		auto_pam4_link_speed_mask;
	uint16_t		support_pam4_auto_speeds;
	uint8_t			req_signal_mode;
	uint8_t			module_status;
};

#define BNXT_COS_QUEUE_COUNT	8
struct bnxt_cos_queue_info {
	uint8_t	id;
	uint8_t	profile;
};

struct rte_flow {
	STAILQ_ENTRY(rte_flow) next;
	struct bnxt_filter_info *filter;
	struct bnxt_vnic_info	*vnic;
};

#define BNXT_PTP_RX_PND_CNT		10
#define BNXT_PTP_FLAGS_PATH_TX		0x0
#define BNXT_PTP_FLAGS_PATH_RX		0x1
#define BNXT_PTP_FLAGS_CURRENT_TIME	0x2
#define BNXT_PTP_CURRENT_TIME_MASK	0xFFFF00000000ULL

struct bnxt_ptp_cfg {
#define BNXT_GRCPF_REG_WINDOW_BASE_OUT  0x400
#define BNXT_GRCPF_REG_SYNC_TIME        0x480
#define BNXT_CYCLECOUNTER_MASK   0xffffffffffffffffULL
	struct rte_timecounter      tc;
	struct rte_timecounter      tx_tstamp_tc;
	struct rte_timecounter      rx_tstamp_tc;
	struct bnxt		*bp;
#define BNXT_MAX_TX_TS	1
	uint16_t			rxctl;
#define BNXT_PTP_MSG_SYNC			BIT(0)
#define BNXT_PTP_MSG_DELAY_REQ			BIT(1)
#define BNXT_PTP_MSG_PDELAY_REQ			BIT(2)
#define BNXT_PTP_MSG_PDELAY_RESP		BIT(3)
#define BNXT_PTP_MSG_FOLLOW_UP			BIT(8)
#define BNXT_PTP_MSG_DELAY_RESP			BIT(9)
#define BNXT_PTP_MSG_PDELAY_RESP_FOLLOW_UP	BIT(10)
#define BNXT_PTP_MSG_ANNOUNCE			BIT(11)
#define BNXT_PTP_MSG_SIGNALING			BIT(12)
#define BNXT_PTP_MSG_MANAGEMENT			BIT(13)
#define BNXT_PTP_MSG_EVENTS		(BNXT_PTP_MSG_SYNC |		\
					 BNXT_PTP_MSG_DELAY_REQ |	\
					 BNXT_PTP_MSG_PDELAY_REQ |	\
					 BNXT_PTP_MSG_PDELAY_RESP)
	uint8_t			tx_tstamp_en:1;
	int			rx_filter;
	uint8_t			filter_all;

#define BNXT_PTP_RX_TS_L	0
#define BNXT_PTP_RX_TS_H	1
#define BNXT_PTP_RX_SEQ		2
#define BNXT_PTP_RX_FIFO	3
#define BNXT_PTP_RX_FIFO_PENDING 0x1
#define BNXT_PTP_RX_FIFO_ADV	4
#define BNXT_PTP_RX_REGS	5

#define BNXT_PTP_TX_TS_L	0
#define BNXT_PTP_TX_TS_H	1
#define BNXT_PTP_TX_SEQ		2
#define BNXT_PTP_TX_FIFO	3
#define BNXT_PTP_TX_FIFO_EMPTY	 0x2
#define BNXT_PTP_TX_REGS	4
	uint32_t			rx_regs[BNXT_PTP_RX_REGS];
	uint32_t			rx_mapped_regs[BNXT_PTP_RX_REGS];
	uint32_t			tx_regs[BNXT_PTP_TX_REGS];
	uint32_t			tx_mapped_regs[BNXT_PTP_TX_REGS];

	/* On P5, the Rx timestamp is present in the Rx completion record */
	uint64_t			rx_timestamp;
	uint64_t			current_time;
	uint64_t			old_time;
	rte_spinlock_t			ptp_lock;
};

struct bnxt_coal {
	uint16_t			num_cmpl_aggr_int;
	uint16_t			num_cmpl_dma_aggr;
	uint16_t			num_cmpl_dma_aggr_during_int;
	uint16_t			int_lat_tmr_max;
	uint16_t			int_lat_tmr_min;
	uint16_t			cmpl_aggr_dma_tmr;
	uint16_t			cmpl_aggr_dma_tmr_during_int;
};

/* 64-bit doorbell */
#define DBR_EPOCH_MASK				0x01000000UL
#define DBR_EPOCH_SFT				24
#define DBR_XID_SFT				32
#define DBR_PATH_L2				(0x1ULL << 56)
#define DBR_VALID				(0x1ULL << 58)
#define DBR_TYPE_SQ				(0x0ULL << 60)
#define DBR_TYPE_SRQ				(0x2ULL << 60)
#define DBR_TYPE_CQ				(0x4ULL << 60)
#define DBR_TYPE_NQ				(0xaULL << 60)
#define DBR_TYPE_NQ_ARM				(0xbULL << 60)

#define DB_PF_OFFSET			0x10000
#define DB_VF_OFFSET			0x4000

#define BNXT_RSS_TBL_SIZE_P5		512U
#define BNXT_RSS_ENTRIES_PER_CTX_P5	64
#define BNXT_MAX_RSS_CTXTS_P5 \
	(BNXT_RSS_TBL_SIZE_P5 / BNXT_RSS_ENTRIES_PER_CTX_P5)

#define BNXT_MAX_QUEUE			8
#define BNXT_MAX_TQM_SP_RINGS		1
#define BNXT_MAX_TQM_FP_LEGACY_RINGS	8
#define BNXT_MAX_TQM_FP_RINGS		9
#define BNXT_MAX_TQM_LEGACY_RINGS	\
	(BNXT_MAX_TQM_SP_RINGS + BNXT_MAX_TQM_FP_LEGACY_RINGS)
#define BNXT_MAX_TQM_RINGS		\
	(BNXT_MAX_TQM_SP_RINGS + BNXT_MAX_TQM_FP_RINGS)
#define BNXT_BACKING_STORE_CFG_LEGACY_LEN	256
#define BNXT_BACKING_STORE_CFG_LEN	\
	sizeof(struct hwrm_func_backing_store_cfg_input)
#define BNXT_PAGE_SHFT 12
#define BNXT_PAGE_SIZE (1 << BNXT_PAGE_SHFT)
#define MAX_CTX_PAGES  (BNXT_PAGE_SIZE / 8)

#define PTU_PTE_VALID             0x1UL
#define PTU_PTE_LAST              0x2UL
#define PTU_PTE_NEXT_TO_LAST      0x4UL

struct bnxt_ring_mem_info {
	int				nr_pages;
	int				page_size;
	uint32_t			flags;
#define BNXT_RMEM_VALID_PTE_FLAG	1
#define BNXT_RMEM_RING_PTE_FLAG		2

	void				**pg_arr;
	rte_iova_t			*dma_arr;
	const struct rte_memzone	*mz;

	uint64_t			*pg_tbl;
	rte_iova_t			pg_tbl_map;
	const struct rte_memzone	*pg_tbl_mz;

	int				vmem_size;
	void				**vmem;
};

struct bnxt_ctx_pg_info {
	uint32_t	entries;
	void		*ctx_pg_arr[MAX_CTX_PAGES];
	rte_iova_t	ctx_dma_arr[MAX_CTX_PAGES];
	struct bnxt_ring_mem_info ring_mem;
};

struct bnxt_ctx_mem_info {
	uint32_t        qp_max_entries;
	uint16_t        qp_min_qp1_entries;
	uint16_t        qp_max_l2_entries;
	uint16_t        qp_entry_size;
	uint16_t        srq_max_l2_entries;
	uint32_t        srq_max_entries;
	uint16_t        srq_entry_size;
	uint16_t        cq_max_l2_entries;
	uint32_t        cq_max_entries;
	uint16_t        cq_entry_size;
	uint16_t        vnic_max_vnic_entries;
	uint16_t        vnic_max_ring_table_entries;
	uint16_t        vnic_entry_size;
	uint32_t        stat_max_entries;
	uint16_t        stat_entry_size;
	uint16_t        tqm_entry_size;
	uint32_t        tqm_min_entries_per_ring;
	uint32_t        tqm_max_entries_per_ring;
	uint32_t        mrav_max_entries;
	uint16_t        mrav_entry_size;
	uint16_t        tim_entry_size;
	uint32_t        tim_max_entries;
	uint8_t         tqm_entries_multiple;
	uint8_t         tqm_fp_rings_count;

	uint32_t        flags;
#define BNXT_CTX_FLAG_INITED    0x01

	struct bnxt_ctx_pg_info qp_mem;
	struct bnxt_ctx_pg_info srq_mem;
	struct bnxt_ctx_pg_info cq_mem;
	struct bnxt_ctx_pg_info vnic_mem;
	struct bnxt_ctx_pg_info stat_mem;
	struct bnxt_ctx_pg_info *tqm_mem[BNXT_MAX_TQM_RINGS];
};

struct bnxt_ctx_mem_buf_info {
	void		*va;
	rte_iova_t	dma;
	uint16_t	ctx_id;
	size_t		size;
};

/* Maximum Firmware Reset bail out value in milliseconds */
#define BNXT_MAX_FW_RESET_TIMEOUT	6000
/* Minimum time required for the firmware readiness in milliseconds */
#define BNXT_MIN_FW_READY_TIMEOUT	2000
/* Frequency for the firmware readiness check in milliseconds */
#define BNXT_FW_READY_WAIT_INTERVAL	100

#define US_PER_MS			1000
#define NS_PER_US			1000

struct bnxt_error_recovery_info {
	/* All units in milliseconds */
	uint32_t	driver_polling_freq;
	uint32_t	primary_func_wait_period;
	uint32_t	normal_func_wait_period;
	uint32_t	primary_func_wait_period_after_reset;
	uint32_t	max_bailout_time_after_reset;
#define BNXT_FW_STATUS_REG		0
#define BNXT_FW_HEARTBEAT_CNT_REG	1
#define BNXT_FW_RECOVERY_CNT_REG	2
#define BNXT_FW_RESET_INPROG_REG	3
#define BNXT_FW_STATUS_REG_CNT		4
	uint32_t	status_regs[BNXT_FW_STATUS_REG_CNT];
	uint32_t	mapped_status_regs[BNXT_FW_STATUS_REG_CNT];
	uint32_t	reset_inprogress_reg_mask;
#define BNXT_NUM_RESET_REG	16
	uint8_t		reg_array_cnt;
	uint32_t	reset_reg[BNXT_NUM_RESET_REG];
	uint32_t	reset_reg_val[BNXT_NUM_RESET_REG];
	uint8_t		delay_after_reset[BNXT_NUM_RESET_REG];
#define BNXT_FLAG_ERROR_RECOVERY_HOST	BIT(0)
#define BNXT_FLAG_ERROR_RECOVERY_CO_CPU	BIT(1)
#define BNXT_FLAG_PRIMARY_FUNC		BIT(2)
#define BNXT_FLAG_RECOVERY_ENABLED	BIT(3)
	uint32_t	flags;

	uint32_t        last_heart_beat;
	uint32_t        last_reset_counter;
};

/* Frequency for the FUNC_DRV_IF_CHANGE retry in milliseconds */
#define BNXT_IF_CHANGE_RETRY_INTERVAL	50
/* Maximum retry count for FUNC_DRV_IF_CHANGE */
#define BNXT_IF_CHANGE_RETRY_COUNT	40

struct bnxt_mark_info {
	uint32_t	mark_id;
	bool		valid;
};

struct bnxt_rep_info {
	struct rte_eth_dev	*vfr_eth_dev;
	pthread_mutex_t		vfr_lock;
	pthread_mutex_t		vfr_start_lock;
	bool			conduit_valid;
};

/* address space location of register */
#define BNXT_FW_STATUS_REG_TYPE_MASK	3
/* register is located in PCIe config space */
#define BNXT_FW_STATUS_REG_TYPE_CFG	0
/* register is located in GRC address space */
#define BNXT_FW_STATUS_REG_TYPE_GRC	1
/* register is located in BAR0  */
#define BNXT_FW_STATUS_REG_TYPE_BAR0	2
/* register is located in BAR1  */
#define BNXT_FW_STATUS_REG_TYPE_BAR1	3

#define BNXT_FW_STATUS_REG_TYPE(reg)	((reg) & BNXT_FW_STATUS_REG_TYPE_MASK)
#define BNXT_FW_STATUS_REG_OFF(reg)	((reg) & ~BNXT_FW_STATUS_REG_TYPE_MASK)

#define BNXT_GRCP_WINDOW_2_BASE		0x2000
#define BNXT_GRCP_WINDOW_3_BASE		0x3000

#define BNXT_GRCP_BASE_MASK		0xfffff000
#define BNXT_GRCP_OFFSET_MASK		0x00000ffc

#define BNXT_FW_STATUS_HEALTHY		0x8000
#define BNXT_FW_STATUS_SHUTDOWN		0x100000

#define BNXT_ETH_RSS_SUPPORT (	\
	RTE_ETH_RSS_IPV4 |		\
	RTE_ETH_RSS_NONFRAG_IPV4_TCP |	\
	RTE_ETH_RSS_NONFRAG_IPV4_UDP |	\
	RTE_ETH_RSS_IPV6 |		\
	RTE_ETH_RSS_NONFRAG_IPV6_TCP |	\
	RTE_ETH_RSS_NONFRAG_IPV6_UDP |	\
	RTE_ETH_RSS_LEVEL_MASK)

#define BNXT_HWRM_SHORT_REQ_LEN		sizeof(struct hwrm_short_input)

struct bnxt_flow_stat_info {
	uint16_t                max_fc;
	uint16_t		flow_count;
	struct bnxt_ctx_mem_buf_info rx_fc_in_tbl;
	struct bnxt_ctx_mem_buf_info rx_fc_out_tbl;
	struct bnxt_ctx_mem_buf_info tx_fc_in_tbl;
	struct bnxt_ctx_mem_buf_info tx_fc_out_tbl;
};

struct bnxt_ring_stats {
	/* Number of transmitted unicast packets */
	uint64_t	tx_ucast_pkts;
	/* Number of transmitted multicast packets */
	uint64_t	tx_mcast_pkts;
	/* Number of transmitted broadcast packets */
	uint64_t	tx_bcast_pkts;
	/* Number of packets discarded in transmit path */
	uint64_t	tx_discard_pkts;
	/* Number of packets in transmit path with error */
	uint64_t	tx_error_pkts;
	/* Number of transmitted bytes for unicast traffic */
	uint64_t	tx_ucast_bytes;
	/* Number of transmitted bytes for multicast traffic */
	uint64_t	tx_mcast_bytes;
	/* Number of transmitted bytes for broadcast traffic */
	uint64_t	tx_bcast_bytes;
	/* Number of received unicast packets */
	uint64_t	rx_ucast_pkts;
	/* Number of received multicast packets */
	uint64_t	rx_mcast_pkts;
	/* Number of received broadcast packets */
	uint64_t	rx_bcast_pkts;
	/* Number of packets discarded in receive path */
	uint64_t	rx_discard_pkts;
	/* Number of packets in receive path with errors */
	uint64_t	rx_error_pkts;
	/* Number of received bytes for unicast traffic */
	uint64_t	rx_ucast_bytes;
	/* Number of received bytes for multicast traffic */
	uint64_t	rx_mcast_bytes;
	/* Number of received bytes for broadcast traffic */
	uint64_t	rx_bcast_bytes;
	/* Number of aggregated unicast packets */
	uint64_t	rx_agg_pkts;
	/* Number of aggregated unicast bytes */
	uint64_t	rx_agg_bytes;
	/* Number of aggregation events */
	uint64_t	rx_agg_events;
	/* Number of aborted aggregations */
	uint64_t	rx_agg_aborts;
};

enum bnxt_session_type {
	BNXT_SESSION_TYPE_REGULAR = 0,
	BNXT_SESSION_TYPE_SHARED_COMMON,
	BNXT_SESSION_TYPE_SHARED_WC,
	BNXT_SESSION_TYPE_LAST
};

struct bnxt {
	void				*bar0;

	struct rte_eth_dev		*eth_dev;
	struct rte_pci_device		*pdev;
	void				*doorbell_base;
	int				legacy_db_size;

	uint32_t		flags;
#define BNXT_FLAG_REGISTERED		BIT(0)
#define BNXT_FLAG_VF			BIT(1)
#define BNXT_FLAG_PORT_STATS		BIT(2)
#define BNXT_FLAG_JUMBO			BIT(3)
#define BNXT_FLAG_SHORT_CMD		BIT(4)
#define BNXT_FLAG_PTP_SUPPORTED		BIT(6)
#define BNXT_FLAG_MULTI_HOST    	BIT(7)
#define BNXT_FLAG_EXT_RX_PORT_STATS	BIT(8)
#define BNXT_FLAG_EXT_TX_PORT_STATS	BIT(9)
#define BNXT_FLAG_KONG_MB_EN		BIT(10)
#define BNXT_FLAG_TRUSTED_VF_EN		BIT(11)
#define BNXT_FLAG_DFLT_VNIC_SET		BIT(12)
#define BNXT_FLAG_CHIP_P5		BIT(13)
#define BNXT_FLAG_STINGRAY		BIT(14)
#define BNXT_FLAG_FW_RESET		BIT(15)
#define BNXT_FLAG_FATAL_ERROR		BIT(16)
#define BNXT_FLAG_IF_CHANGE_HOT_FW_RESET_DONE	BIT(17)
#define BNXT_FLAG_FW_HEALTH_CHECK_SCHEDULED	BIT(18)
#define BNXT_FLAG_EXT_STATS_SUPPORTED		BIT(19)
#define BNXT_FLAG_NEW_RM			BIT(20)
#define BNXT_FLAG_NPAR_PF			BIT(21)
#define BNXT_FLAG_FW_CAP_ONE_STEP_TX_TS		BIT(22)
#define BNXT_FLAG_FC_THREAD			BIT(23)
#define BNXT_FLAG_RX_VECTOR_PKT_MODE		BIT(24)
#define BNXT_FLAG_FLOW_XSTATS_EN		BIT(25)
#define BNXT_FLAG_DFLT_MAC_SET			BIT(26)
#define BNXT_FLAG_GFID_ENABLE			BIT(27)
#define BNXT_PF(bp)		(!((bp)->flags & BNXT_FLAG_VF))
#define BNXT_VF(bp)		((bp)->flags & BNXT_FLAG_VF)
#define BNXT_NPAR(bp)		((bp)->flags & BNXT_FLAG_NPAR_PF)
#define BNXT_MH(bp)		((bp)->flags & BNXT_FLAG_MULTI_HOST)
#define BNXT_SINGLE_PF(bp)	(BNXT_PF(bp) && !BNXT_NPAR(bp) && !BNXT_MH(bp))
#define BNXT_USE_CHIMP_MB	0 //For non-CFA commands, everything uses Chimp.
#define BNXT_USE_KONG(bp)	((bp)->flags & BNXT_FLAG_KONG_MB_EN)
#define BNXT_VF_IS_TRUSTED(bp)	((bp)->flags & BNXT_FLAG_TRUSTED_VF_EN)
#define BNXT_CHIP_P5(bp)	((bp)->flags & BNXT_FLAG_CHIP_P5)
#define BNXT_STINGRAY(bp)	((bp)->flags & BNXT_FLAG_STINGRAY)
#define BNXT_HAS_NQ(bp)		BNXT_CHIP_P5(bp)
#define BNXT_HAS_RING_GRPS(bp)	(!BNXT_CHIP_P5(bp))
#define BNXT_FLOW_XSTATS_EN(bp)	((bp)->flags & BNXT_FLAG_FLOW_XSTATS_EN)
#define BNXT_HAS_DFLT_MAC_SET(bp)	((bp)->flags & BNXT_FLAG_DFLT_MAC_SET)
#define BNXT_GFID_ENABLED(bp)	((bp)->flags & BNXT_FLAG_GFID_ENABLE)

	uint32_t			flags2;
#define BNXT_FLAGS2_PTP_TIMESYNC_ENABLED	BIT(0)
#define BNXT_FLAGS2_PTP_ALARM_SCHEDULED		BIT(1)
#define BNXT_P5_PTP_TIMESYNC_ENABLED(bp)	\
	((bp)->flags2 & BNXT_FLAGS2_PTP_TIMESYNC_ENABLED)
#define BNXT_FLAGS2_TESTPMD_EN			BIT(3)
#define BNXT_TESTPMD_EN(bp)			\
	((bp)->flags2 & BNXT_FLAGS2_TESTPMD_EN)

	uint16_t		chip_num;
#define CHIP_NUM_58818		0xd818
#define BNXT_CHIP_SR2(bp)	((bp)->chip_num == CHIP_NUM_58818)
#define	BNXT_FLAGS2_MULTIROOT_EN		BIT(4)
#define	BNXT_MULTIROOT_EN(bp)			\
	((bp)->flags2 & BNXT_FLAGS2_MULTIROOT_EN)

	uint32_t		fw_cap;
#define BNXT_FW_CAP_HOT_RESET		BIT(0)
#define BNXT_FW_CAP_IF_CHANGE		BIT(1)
#define BNXT_FW_CAP_ERROR_RECOVERY	BIT(2)
#define BNXT_FW_CAP_ERR_RECOVER_RELOAD	BIT(3)
#define BNXT_FW_CAP_HCOMM_FW_STATUS	BIT(4)
#define BNXT_FW_CAP_ADV_FLOW_MGMT	BIT(5)
#define BNXT_FW_CAP_ADV_FLOW_COUNTERS	BIT(6)
#define BNXT_FW_CAP_LINK_ADMIN		BIT(7)
#define BNXT_FW_CAP_TRUFLOW_EN		BIT(8)
#define BNXT_FW_CAP_VLAN_TX_INSERT	BIT(9)
#define BNXT_FW_CAP_RX_ALL_PKT_TS	BIT(10)
#define BNXT_TRUFLOW_EN(bp)	((bp)->fw_cap & BNXT_FW_CAP_TRUFLOW_EN &&\
				 (bp)->app_id != 0xFF)

	pthread_mutex_t         flow_lock;

	uint32_t		vnic_cap_flags;
#define BNXT_VNIC_CAP_COS_CLASSIFY	BIT(0)
#define BNXT_VNIC_CAP_OUTER_RSS		BIT(1)
#define BNXT_VNIC_CAP_RX_CMPL_V2	BIT(2)
#define BNXT_VNIC_CAP_VLAN_RX_STRIP	BIT(3)
#define BNXT_RX_VLAN_STRIP_EN(bp)	((bp)->vnic_cap_flags & BNXT_VNIC_CAP_VLAN_RX_STRIP)
#define BNXT_VNIC_CAP_OUTER_RSS_TRUSTED_VF	BIT(4)
	unsigned int		rx_nr_rings;
	unsigned int		rx_cp_nr_rings;
	unsigned int		rx_num_qs_per_vnic;
	struct bnxt_rx_queue **rx_queues;
	const void		*rx_mem_zone;
	struct rx_port_stats    *hw_rx_port_stats;
	rte_iova_t		hw_rx_port_stats_map;
	struct rx_port_stats_ext    *hw_rx_port_stats_ext;
	rte_iova_t		hw_rx_port_stats_ext_map;
	uint16_t		fw_rx_port_stats_ext_size;

	unsigned int		tx_nr_rings;
	unsigned int		tx_cp_nr_rings;
	struct bnxt_tx_queue **tx_queues;
	const void		*tx_mem_zone;
	struct tx_port_stats    *hw_tx_port_stats;
	rte_iova_t		hw_tx_port_stats_map;
	struct tx_port_stats_ext    *hw_tx_port_stats_ext;
	rte_iova_t		hw_tx_port_stats_ext_map;
	uint16_t		fw_tx_port_stats_ext_size;

	/* Default completion ring */
	struct bnxt_cp_ring_info	*async_cp_ring;
	struct bnxt_cp_ring_info	*rxtx_nq_ring;
	uint32_t		max_ring_grps;
	struct bnxt_ring_grp_info	*grp_info;

	uint16_t			nr_vnics;

	struct bnxt_vnic_info	*vnic_info;
	STAILQ_HEAD(, bnxt_vnic_info)	free_vnic_list;

	struct bnxt_filter_info	*filter_info;
	STAILQ_HEAD(, bnxt_filter_info)	free_filter_list;

	struct bnxt_irq         *irq_tbl;

	uint8_t			mac_addr[RTE_ETHER_ADDR_LEN];

	uint16_t			chimp_cmd_seq;
	uint16_t			kong_cmd_seq;
	void				*hwrm_cmd_resp_addr;
	rte_iova_t			hwrm_cmd_resp_dma_addr;
	void				*hwrm_short_cmd_req_addr;
	rte_iova_t			hwrm_short_cmd_req_dma_addr;
	rte_spinlock_t			hwrm_lock;
	/* synchronize between dev_configure_op and int handler */
	pthread_mutex_t			def_cp_lock;
	/* synchronize between dev_start_op and async evt handler
	 * Locking sequence in async evt handler will be
	 * def_cp_lock
	 * health_check_lock
	 */
	pthread_mutex_t			health_check_lock;
	/* synchronize between dev_stop/dev_close_op and
	 * error recovery thread triggered as part of
	 * HWRM_ASYNC_EVENT_CMPL_EVENT_ID_RESET_NOTIFY
	 */
	pthread_mutex_t			err_recovery_lock;
	uint16_t			max_req_len;
	uint16_t			max_resp_len;
	uint16_t                        hwrm_max_ext_req_len;

	 /* default command timeout value of 500ms */
#define DFLT_HWRM_CMD_TIMEOUT		500000
	 /* short command timeout value of 50ms */
#define SHORT_HWRM_CMD_TIMEOUT		50000
	/* default HWRM request timeout value */
	uint32_t			hwrm_cmd_timeout;

	struct bnxt_link_info		*link_info;
	struct bnxt_cos_queue_info	*rx_cos_queue;
	struct bnxt_cos_queue_info	*tx_cos_queue;
	uint8_t			tx_cosq_id[BNXT_COS_QUEUE_COUNT];
	uint8_t			rx_cosq_cnt;
	uint8_t                 max_tc;
	uint8_t                 max_lltc;
	uint8_t                 max_q;

	uint16_t		fw_fid;
	uint16_t		max_rsscos_ctx;
	uint16_t		max_cp_rings;
	uint16_t		max_tx_rings;
	uint16_t		max_rx_rings;
#define MAX_STINGRAY_RINGS		236U
#define BNXT_MAX_VF_REP_RINGS	8U

	uint16_t		max_nq_rings;
	uint16_t		max_l2_ctx;
	uint16_t		max_rx_em_flows;
	uint16_t		max_vnics;
#define BNXT_MAX_VNICS_COS_CLASSIFY	8
	uint16_t		max_stat_ctx;
	uint16_t		max_tpa_v2;
	uint16_t		first_vf_id;
	uint16_t		vlan;
#define BNXT_OUTER_TPID_MASK	0x0000ffff
#define BNXT_OUTER_TPID_BD_MASK	0xffff0000
#define BNXT_OUTER_TPID_BD_SHFT	16
	uint32_t		outer_tpid_bd;
	struct bnxt_pf_info	*pf;
	struct bnxt_parent_info	*parent;
	uint8_t			port_cnt;
	uint8_t			vxlan_port_cnt;
	uint8_t			geneve_port_cnt;
	uint8_t			ecpri_port_cnt;
	uint16_t		vxlan_port;
	uint16_t		geneve_port;
	uint16_t		ecpri_port;
	uint16_t		vxlan_fw_dst_port_id;
	uint16_t		geneve_fw_dst_port_id;
	uint16_t		ecpri_fw_dst_port_id;
	uint16_t		ecpri_upar_in_use;
	uint32_t		fw_ver;
	uint32_t		hwrm_spec_code;

	struct bnxt_led_info	*leds;
	uint8_t			ieee_1588;
	struct bnxt_ptp_cfg     *ptp_cfg;
	uint8_t			ptp_all_rx_tstamp;
	uint16_t		vf_resv_strategy;
	struct bnxt_ctx_mem_info        *ctx;

	uint16_t		fw_reset_min_msecs;
	uint16_t		fw_reset_max_msecs;
	uint16_t		switch_domain_id;
	uint16_t		num_reps;
	struct bnxt_rep_info	*rep_info;
	uint16_t                *cfa_code_map;
	/* Device Serial Number */
	uint8_t			dsn[BNXT_DEVICE_SERIAL_NUM_SIZE];
	/* Struct to hold adapter error recovery related info */
	struct bnxt_error_recovery_info *recovery_info;
#define BNXT_MARK_TABLE_SZ	(sizeof(struct bnxt_mark_info)  * 64 * 1024)
/* TCAM and EM should be 16-bit only. Other modes not supported. */
#define BNXT_FLOW_ID_MASK	0x0000ffff
	struct bnxt_mark_info	*mark_table;

#define	BNXT_SVIF_INVALID	0xFFFF
	uint16_t		func_svif;
	uint16_t		port_svif;

	struct tf		tfp[BNXT_SESSION_TYPE_LAST];
	struct bnxt_ulp_context	*ulp_ctx;
	struct bnxt_flow_stat_info *flow_stat;
	uint16_t		max_num_kflows;
	uint8_t			app_id;
	uint16_t		tx_cfa_action;
	struct bnxt_ring_stats	*prev_rx_ring_stats;
	struct bnxt_ring_stats	*prev_tx_ring_stats;
	struct bnxt_vnic_queue_db vnic_queue_db;

#define BNXT_MAX_MC_ADDRS	((bp)->max_mcast_addr)
	struct rte_ether_addr	*mcast_addr_list;
	rte_iova_t		mc_list_dma_addr;
	uint32_t		nb_mc_addr;
	uint32_t		max_mcast_addr; /* maximum number of mcast filters supported */

	struct rte_eth_rss_conf	rss_conf; /* RSS configuration. */
	uint16_t		tunnel_disable_flag; /* tunnel stateless offloads status */
};

static
inline uint16_t bnxt_max_rings(struct bnxt *bp)
{
	uint16_t max_tx_rings = bp->max_tx_rings;
	uint16_t max_rx_rings = bp->max_rx_rings;
	uint16_t max_cp_rings = bp->max_cp_rings;
	uint16_t max_rings;

	/* For the sake of symmetry:
	 * max Tx rings == max Rx rings, one stat ctx for each.
	 */
	if (BNXT_STINGRAY(bp)) {
		max_rx_rings = RTE_MIN(RTE_MIN(max_rx_rings / 2U,
					       MAX_STINGRAY_RINGS),
				       bp->max_stat_ctx / 2U);
	} else {
		max_rx_rings = RTE_MIN(max_rx_rings / 2U,
				       bp->max_stat_ctx / 2U);
	}

	/*
	 * RSS table size in P5 is 512.
	 * Cap max Rx rings to the same value for RSS.
	 */
	if (BNXT_CHIP_P5(bp))
		max_rx_rings = RTE_MIN(max_rx_rings, BNXT_RSS_TBL_SIZE_P5);

	max_tx_rings = RTE_MIN(max_tx_rings, max_rx_rings);
	if (max_cp_rings > BNXT_NUM_ASYNC_CPR(bp))
		max_cp_rings -= BNXT_NUM_ASYNC_CPR(bp);
	max_rings = RTE_MIN(max_cp_rings / 2U, max_tx_rings);

	return max_rings;
}

#define BNXT_FC_TIMER	1 /* Timer freq in Sec Flow Counters */

/**
 * Structure to store private data for each VF representor instance
 */
struct bnxt_representor {
	uint16_t		switch_domain_id;
	uint16_t		vf_id;
#define BNXT_REP_IS_PF		BIT(0)
#define BNXT_REP_Q_R2F_VALID		BIT(1)
#define BNXT_REP_Q_F2R_VALID		BIT(2)
#define BNXT_REP_FC_R2F_VALID		BIT(3)
#define BNXT_REP_FC_F2R_VALID		BIT(4)
#define BNXT_REP_BASED_PF_VALID		BIT(5)
	uint32_t		flags;
	uint16_t		fw_fid;
#define	BNXT_DFLT_VNIC_ID_INVALID	0xFFFF
	uint16_t		dflt_vnic_id;
	uint16_t		svif;
	uint16_t		vfr_tx_cfa_action;
	uint8_t			parent_pf_idx; /* Logical PF index */
	uint32_t		dpdk_port_id;
	uint32_t		rep_based_pf;
	uint8_t			rep_q_r2f;
	uint8_t			rep_q_f2r;
	uint8_t			rep_fc_r2f;
	uint8_t			rep_fc_f2r;
	/* Private data store of associated PF/Trusted VF */
	struct rte_eth_dev	*parent_dev;
	uint8_t			mac_addr[RTE_ETHER_ADDR_LEN];
	uint8_t			dflt_mac_addr[RTE_ETHER_ADDR_LEN];
	struct bnxt_rx_queue	**rx_queues;
	unsigned int		rx_nr_rings;
	unsigned int		tx_nr_rings;
	uint64_t                tx_pkts[BNXT_MAX_VF_REP_RINGS];
	uint64_t                tx_bytes[BNXT_MAX_VF_REP_RINGS];
	uint64_t                rx_pkts[BNXT_MAX_VF_REP_RINGS];
	uint64_t                rx_bytes[BNXT_MAX_VF_REP_RINGS];
	uint64_t                rx_drop_pkts[BNXT_MAX_VF_REP_RINGS];
	uint64_t                rx_drop_bytes[BNXT_MAX_VF_REP_RINGS];
};

#define BNXT_REP_PF(vfr_bp)		((vfr_bp)->flags & BNXT_REP_IS_PF)
#define BNXT_REP_BASED_PF(vfr_bp)	\
		((vfr_bp)->flags & BNXT_REP_BASED_PF_VALID)

struct bnxt_vf_rep_tx_queue {
	struct bnxt_tx_queue *txq;
	struct bnxt_representor *bp;
};

#define I2C_DEV_ADDR_A0			0xa0
#define I2C_DEV_ADDR_A2			0xa2
#define SFF_DIAG_SUPPORT_OFFSET		0x5c
#define SFF_MODULE_ID_SFP		0x3
#define SFF_MODULE_ID_QSFP		0xc
#define SFF_MODULE_ID_QSFP_PLUS		0xd
#define SFF_MODULE_ID_QSFP28		0x11
#define SFF8636_FLATMEM_OFFSET		0x2
#define SFF8636_FLATMEM_MASK		0x4
#define SFF8636_OPT_PAGES_OFFSET	0xc3
#define SFF8636_PAGE1_MASK		0x40
#define SFF8636_PAGE2_MASK		0x80
#define BNXT_MAX_PHY_I2C_RESP_SIZE	64

int bnxt_mtu_set_op(struct rte_eth_dev *eth_dev, uint16_t new_mtu);
int bnxt_link_update(struct rte_eth_dev *eth_dev, int wait_to_complete,
		     bool exp_link_status);
int bnxt_rcv_msg_from_vf(struct bnxt *bp, uint16_t vf_id, void *msg);
int is_bnxt_in_error(struct bnxt *bp);

int bnxt_map_fw_health_status_regs(struct bnxt *bp);
uint32_t bnxt_read_fw_status_reg(struct bnxt *bp, uint32_t index);
void bnxt_schedule_fw_health_check(struct bnxt *bp);

bool is_bnxt_supported(struct rte_eth_dev *dev);
bool bnxt_stratus_device(struct bnxt *bp);
void bnxt_print_link_info(struct rte_eth_dev *eth_dev);
uint16_t bnxt_rss_ctxts(const struct bnxt *bp);
uint16_t bnxt_rss_hash_tbl_size(const struct bnxt *bp);
int bnxt_link_update_op(struct rte_eth_dev *eth_dev,
			int wait_to_complete);
int
bnxt_udp_tunnel_port_del_op(struct rte_eth_dev *eth_dev,
			    struct rte_eth_udp_tunnel *udp_tunnel);
int
bnxt_udp_tunnel_port_add_op(struct rte_eth_dev *eth_dev,
			    struct rte_eth_udp_tunnel *udp_tunnel);

extern const struct rte_flow_ops bnxt_flow_ops;
extern const struct rte_flow_ops bnxt_flow_meter_ops;

#define bnxt_acquire_flow_lock(bp) \
	pthread_mutex_lock(&(bp)->flow_lock)

#define bnxt_release_flow_lock(bp) \
	pthread_mutex_unlock(&(bp)->flow_lock)

#define BNXT_VALID_VNIC_OR_RET(bp, vnic_id) do { \
	if ((vnic_id) >= (bp)->max_vnics) { \
		rte_flow_error_set(error, \
				EINVAL, \
				RTE_FLOW_ERROR_TYPE_ATTR_GROUP, \
				NULL, \
				"Group id is invalid!"); \
		rc = -rte_errno; \
		goto ret; \
	} \
} while (0)

#define	BNXT_ETH_DEV_IS_REPRESENTOR(eth_dev)	\
		((eth_dev)->data->dev_flags & RTE_ETH_DEV_REPRESENTOR)

extern int bnxt_logtype_driver;
#define PMD_DRV_LOG_RAW(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, bnxt_logtype_driver, "%s(): " fmt, \
		__func__, ## args)

#define PMD_DRV_LOG(level, fmt, args...) \
	  PMD_DRV_LOG_RAW(level, fmt, ## args)

extern const struct rte_flow_ops bnxt_ulp_rte_flow_ops;
int32_t bnxt_ulp_port_init(struct bnxt *bp);
void bnxt_ulp_port_deinit(struct bnxt *bp);
int32_t bnxt_ulp_create_df_rules(struct bnxt *bp);
void bnxt_ulp_destroy_df_rules(struct bnxt *bp, bool global);
int32_t
bnxt_ulp_create_vfr_default_rules(struct rte_eth_dev *vfr_ethdev);
int32_t
bnxt_ulp_delete_vfr_default_rules(struct bnxt_representor *vfr);
int bnxt_rep_dev_start_op(struct rte_eth_dev *eth_dev);

void bnxt_cancel_fc_thread(struct bnxt *bp);
void bnxt_flow_cnt_alarm_cb(void *arg);
int bnxt_flow_stats_req(struct bnxt *bp);
int bnxt_flow_stats_cnt(struct bnxt *bp);
uint32_t bnxt_get_speed_capabilities(struct bnxt *bp);
int bnxt_flow_ops_get_op(struct rte_eth_dev *dev,
			 const struct rte_flow_ops **ops);
int bnxt_dev_start_op(struct rte_eth_dev *eth_dev);
int bnxt_dev_stop_op(struct rte_eth_dev *eth_dev);
void bnxt_handle_vf_cfg_change(void *arg);
int bnxt_flow_meter_ops_get(struct rte_eth_dev *eth_dev, void *arg);
struct bnxt_vnic_info *bnxt_get_default_vnic(struct bnxt *bp);
struct tf *bnxt_get_tfp_session(struct bnxt *bp, enum bnxt_session_type type);
#endif
