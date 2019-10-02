/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2018 Broadcom
 * All rights reserved.
 */

#ifndef _BNXT_H_
#define _BNXT_H_

#include <inttypes.h>
#include <stdbool.h>
#include <sys/queue.h>

#include <rte_pci.h>
#include <rte_bus_pci.h>
#include <rte_ethdev_driver.h>
#include <rte_memory.h>
#include <rte_lcore.h>
#include <rte_spinlock.h>
#include <rte_time.h>

#include "bnxt_cpr.h"
#include "bnxt_util.h"

#define BNXT_MAX_MTU		9574
#define VLAN_TAG_SIZE		4
#define BNXT_NUM_VLANS		2
#define BNXT_MAX_PKT_LEN	(BNXT_MAX_MTU + RTE_ETHER_HDR_LEN +\
				 RTE_ETHER_CRC_LEN +\
				 (BNXT_NUM_VLANS * VLAN_TAG_SIZE))
#define BNXT_VF_RSV_NUM_RSS_CTX	1
#define BNXT_VF_RSV_NUM_L2_CTX	4
/* TODO: For now, do not support VMDq/RFS on VFs. */
#define BNXT_VF_RSV_NUM_VNIC	1
#define BNXT_MAX_LED		4
#define BNXT_MIN_RING_DESC	16
#define BNXT_MAX_TX_RING_DESC	4096
#define BNXT_MAX_RX_RING_DESC	8192
#define BNXT_DB_SIZE		0x80

#ifdef RTE_ARCH_ARM64
#define BNXT_NUM_ASYNC_CPR(bp) (BNXT_STINGRAY(bp) ? 0 : 1)
#else
#define BNXT_NUM_ASYNC_CPR(bp) 1
#endif

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

struct bnxt_led_info {
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

enum bnxt_hw_context {
	HW_CONTEXT_NONE     = 0,
	HW_CONTEXT_IS_RSS   = 1,
	HW_CONTEXT_IS_COS   = 2,
	HW_CONTEXT_IS_LB    = 3,
};

struct bnxt_vlan_table_entry {
	uint16_t		tpid;
	uint16_t		vid;
} __attribute__((packed));

struct bnxt_vlan_antispoof_table_entry {
	uint16_t		tpid;
	uint16_t		vid;
	uint16_t		mask;
} __attribute__((packed));

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

struct bnxt_pf_info {
#define BNXT_FIRST_PF_FID	1
#define BNXT_MAX_VFS(bp)	(bp->pf.max_vfs)
#define BNXT_TOTAL_VFS(bp)	((bp)->pf.total_vfs)
#define BNXT_FIRST_VF_FID	128
#define BNXT_PF_RINGS_USED(bp)	bnxt_get_num_queues(bp)
#define BNXT_PF_RINGS_AVAIL(bp)	(bp->pf.max_cp_rings - BNXT_PF_RINGS_USED(bp))
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

/* Max wait time is 10 * 100ms = 1s */
#define BNXT_LINK_WAIT_CNT	10
#define BNXT_LINK_WAIT_INTERVAL	100
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

#define BNXT_PTP_FLAGS_PATH_TX		0x0
#define BNXT_PTP_FLAGS_PATH_RX		0x1
#define BNXT_PTP_FLAGS_CURRENT_TIME	0x2

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

	/* On Thor, the Rx timestamp is present in the Rx completion record */
	uint64_t			rx_timestamp;
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
#define DBR_XID_SFT				32
#define DBR_PATH_L2				(0x1ULL << 56)
#define DBR_TYPE_SQ				(0x0ULL << 60)
#define DBR_TYPE_SRQ				(0x2ULL << 60)
#define DBR_TYPE_CQ				(0x4ULL << 60)
#define DBR_TYPE_NQ				(0xaULL << 60)
#define DBR_TYPE_NQ_ARM				(0xbULL << 60)

#define BNXT_RSS_TBL_SIZE_THOR		512
#define BNXT_RSS_ENTRIES_PER_CTX_THOR	64
#define BNXT_MAX_RSS_CTXTS_THOR \
	(BNXT_RSS_TBL_SIZE_THOR / BNXT_RSS_ENTRIES_PER_CTX_THOR)

#define BNXT_MAX_TC    8
#define BNXT_MAX_QUEUE 8
#define BNXT_MAX_TC_Q  (BNXT_MAX_TC + 1)
#define BNXT_MAX_Q     (bp->max_q + 1)
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

	uint32_t        flags;
#define BNXT_CTX_FLAG_INITED    0x01

	struct bnxt_ctx_pg_info qp_mem;
	struct bnxt_ctx_pg_info srq_mem;
	struct bnxt_ctx_pg_info cq_mem;
	struct bnxt_ctx_pg_info vnic_mem;
	struct bnxt_ctx_pg_info stat_mem;
	struct bnxt_ctx_pg_info *tqm_mem[BNXT_MAX_TC_Q];
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
	uint32_t	master_func_wait_period;
	uint32_t	normal_func_wait_period;
	uint32_t	master_func_wait_period_after_reset;
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
#define BNXT_FLAG_MASTER_FUNC		BIT(2)
#define BNXT_FLAG_RECOVERY_ENABLED	BIT(3)
	uint32_t	flags;

	uint32_t        last_heart_beat;
	uint32_t        last_reset_counter;
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

#define BNXT_FW_STATUS_SHUTDOWN		0x100000

#define BNXT_HWRM_SHORT_REQ_LEN		sizeof(struct hwrm_short_input)
struct bnxt {
	void				*bar0;

	struct rte_eth_dev		*eth_dev;
	struct rte_eth_rss_conf		rss_conf;
	struct rte_pci_device		*pdev;
	void				*doorbell_base;

	uint32_t		flags;
#define BNXT_FLAG_REGISTERED		BIT(0)
#define BNXT_FLAG_VF			BIT(1)
#define BNXT_FLAG_PORT_STATS		BIT(2)
#define BNXT_FLAG_JUMBO			BIT(3)
#define BNXT_FLAG_SHORT_CMD		BIT(4)
#define BNXT_FLAG_UPDATE_HASH		BIT(5)
#define BNXT_FLAG_PTP_SUPPORTED		BIT(6)
#define BNXT_FLAG_MULTI_HOST    	BIT(7)
#define BNXT_FLAG_EXT_RX_PORT_STATS	BIT(8)
#define BNXT_FLAG_EXT_TX_PORT_STATS	BIT(9)
#define BNXT_FLAG_KONG_MB_EN		BIT(10)
#define BNXT_FLAG_TRUSTED_VF_EN		BIT(11)
#define BNXT_FLAG_DFLT_VNIC_SET		BIT(12)
#define BNXT_FLAG_THOR_CHIP		BIT(13)
#define BNXT_FLAG_STINGRAY		BIT(14)
#define BNXT_FLAG_FW_RESET		BIT(15)
#define BNXT_FLAG_FATAL_ERROR		BIT(16)
#define BNXT_FLAG_FW_CAP_IF_CHANGE		BIT(17)
#define BNXT_FLAG_IF_CHANGE_HOT_FW_RESET_DONE	BIT(18)
#define BNXT_FLAG_FW_CAP_ERROR_RECOVERY		BIT(19)
#define BNXT_FLAG_FW_HEALTH_CHECK_SCHEDULED	BIT(20)
#define BNXT_FLAG_FW_CAP_ERR_RECOVER_RELOAD	BIT(21)
#define BNXT_FLAG_EXT_STATS_SUPPORTED		BIT(22)
#define BNXT_FLAG_NEW_RM			BIT(23)
#define BNXT_FLAG_INIT_DONE			BIT(24)
#define BNXT_FLAG_FW_CAP_ONE_STEP_TX_TS		BIT(25)
#define BNXT_FLAG_ADV_FLOW_MGMT			BIT(26)
#define BNXT_PF(bp)		(!((bp)->flags & BNXT_FLAG_VF))
#define BNXT_VF(bp)		((bp)->flags & BNXT_FLAG_VF)
#define BNXT_NPAR(bp)		((bp)->port_partition_type)
#define BNXT_MH(bp)             ((bp)->flags & BNXT_FLAG_MULTI_HOST)
#define BNXT_SINGLE_PF(bp)      (BNXT_PF(bp) && !BNXT_NPAR(bp) && !BNXT_MH(bp))
#define BNXT_USE_CHIMP_MB	0 //For non-CFA commands, everything uses Chimp.
#define BNXT_USE_KONG(bp)	((bp)->flags & BNXT_FLAG_KONG_MB_EN)
#define BNXT_VF_IS_TRUSTED(bp)	((bp)->flags & BNXT_FLAG_TRUSTED_VF_EN)
#define BNXT_CHIP_THOR(bp)	((bp)->flags & BNXT_FLAG_THOR_CHIP)
#define BNXT_STINGRAY(bp)	((bp)->flags & BNXT_FLAG_STINGRAY)
#define BNXT_HAS_NQ(bp)		BNXT_CHIP_THOR(bp)
#define BNXT_HAS_RING_GRPS(bp)	(!BNXT_CHIP_THOR(bp))

	uint32_t		flow_flags;
#define BNXT_FLOW_FLAG_L2_HDR_SRC_FILTER_EN	BIT(0)

	pthread_mutex_t         flow_lock;
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
	uint32_t		max_ring_grps;
	struct bnxt_ring_grp_info	*grp_info;

	unsigned int		nr_vnics;

#define BNXT_GET_DEFAULT_VNIC(bp)	(&(bp)->vnic_info[0])
	struct bnxt_vnic_info	*vnic_info;
	STAILQ_HEAD(, bnxt_vnic_info)	free_vnic_list;

	struct bnxt_filter_info	*filter_info;
	STAILQ_HEAD(, bnxt_filter_info)	free_filter_list;

	struct bnxt_irq         *irq_tbl;

	uint8_t			mac_addr[RTE_ETHER_ADDR_LEN];

	uint16_t			hwrm_cmd_seq;
	uint16_t			kong_cmd_seq;
	void				*hwrm_cmd_resp_addr;
	rte_iova_t			hwrm_cmd_resp_dma_addr;
	void				*hwrm_short_cmd_req_addr;
	rte_iova_t			hwrm_short_cmd_req_dma_addr;
	rte_spinlock_t			hwrm_lock;
	uint16_t			max_req_len;
	uint16_t			max_resp_len;
	uint16_t                        hwrm_max_ext_req_len;

	struct bnxt_link_info	link_info;
	struct bnxt_cos_queue_info	cos_queue[BNXT_COS_QUEUE_COUNT];
	uint8_t			tx_cosq_id;
	uint8_t                 max_tc;
	uint8_t                 max_lltc;
	uint8_t                 max_q;

	uint16_t		fw_fid;
	uint8_t			dflt_mac_addr[RTE_ETHER_ADDR_LEN];
	uint16_t		max_rsscos_ctx;
	uint16_t		max_cp_rings;
	uint16_t		max_tx_rings;
	uint16_t		max_rx_rings;
	uint16_t		max_nq_rings;
	uint16_t		max_l2_ctx;
	uint16_t		max_rx_em_flows;
	uint16_t		max_vnics;
	uint16_t		max_stat_ctx;
	uint16_t		first_vf_id;
	uint16_t		vlan;
#define BNXT_OUTER_TPID_MASK	0x0000ffff
#define BNXT_OUTER_TPID_BD_MASK	0xffff0000
#define BNXT_OUTER_TPID_BD_SHFT	16
	uint32_t		outer_tpid_bd;
	struct bnxt_pf_info	pf;
	uint8_t			port_partition_type;
	uint8_t			dev_stopped;
	uint8_t			vxlan_port_cnt;
	uint8_t			geneve_port_cnt;
	uint16_t		vxlan_port;
	uint16_t		geneve_port;
	uint16_t		vxlan_fw_dst_port_id;
	uint16_t		geneve_fw_dst_port_id;
	uint32_t		fw_ver;
	uint32_t		hwrm_spec_code;

	struct bnxt_led_info	leds[BNXT_MAX_LED];
	uint8_t			num_leds;
	struct bnxt_ptp_cfg     *ptp_cfg;
	uint16_t		vf_resv_strategy;
	struct bnxt_ctx_mem_info        *ctx;

	uint16_t		fw_reset_min_msecs;
	uint16_t		fw_reset_max_msecs;

	/* Struct to hold adapter error recovery related info */
	struct bnxt_error_recovery_info *recovery_info;
};

int bnxt_link_update_op(struct rte_eth_dev *eth_dev, int wait_to_complete);
int bnxt_rcv_msg_from_vf(struct bnxt *bp, uint16_t vf_id, void *msg);
int is_bnxt_in_error(struct bnxt *bp);
uint16_t bnxt_rss_ctxts(const struct bnxt *bp);

int bnxt_map_fw_health_status_regs(struct bnxt *bp);
uint32_t bnxt_read_fw_status_reg(struct bnxt *bp, uint32_t index);
void bnxt_schedule_fw_health_check(struct bnxt *bp);

bool is_bnxt_supported(struct rte_eth_dev *dev);
bool bnxt_stratus_device(struct bnxt *bp);
extern const struct rte_flow_ops bnxt_flow_ops;
#define bnxt_acquire_flow_lock(bp) \
	pthread_mutex_lock(&(bp)->flow_lock)

#define bnxt_release_flow_lock(bp) \
	pthread_mutex_unlock(&(bp)->flow_lock)

extern int bnxt_logtype_driver;
#define PMD_DRV_LOG_RAW(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, bnxt_logtype_driver, "%s(): " fmt, \
		__func__, ## args)

#define PMD_DRV_LOG(level, fmt, args...) \
	PMD_DRV_LOG_RAW(level, fmt, ## args)
#endif
