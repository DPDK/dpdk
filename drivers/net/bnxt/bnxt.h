/*-
 *   BSD LICENSE
 *
 *   Copyright(c) Broadcom Limited.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Broadcom Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _BNXT_H_
#define _BNXT_H_

#include <inttypes.h>
#include <stdbool.h>
#include <sys/queue.h>

#include <rte_pci.h>
#include <rte_ethdev.h>
#include <rte_memory.h>
#include <rte_lcore.h>
#include <rte_spinlock.h>

#include "bnxt_cpr.h"

#define BNXT_MAX_MTU		9500
#define VLAN_TAG_SIZE		4
#define BNXT_MAX_LED		4

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
#define BNXT_FIRST_VF_FID	128
#define BNXT_PF_RINGS_USED(bp)	bnxt_get_num_queues(bp)
#define BNXT_PF_RINGS_AVAIL(bp)	(bp->pf.max_cp_rings - BNXT_PF_RINGS_USED(bp))
	uint8_t			port_id;
	uint16_t		first_vf_id;
	uint16_t		active_vfs;
	uint16_t		max_vfs;
	uint32_t		func_cfg_flags;
	void			*vf_req_buf;
	phys_addr_t		vf_req_buf_dma_addr;
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
	uint16_t		auto_link_speed_mask;
	uint32_t		preemphasis;
};

#define BNXT_COS_QUEUE_COUNT	8
struct bnxt_cos_queue_info {
	uint8_t	id;
	uint8_t	profile;
};

#define BNXT_HWRM_SHORT_REQ_LEN		sizeof(struct hwrm_short_input)
struct bnxt {
	void				*bar0;

	struct rte_eth_dev		*eth_dev;
	struct rte_pci_device		*pdev;

	uint32_t		flags;
#define BNXT_FLAG_REGISTERED	(1 << 0)
#define BNXT_FLAG_VF		(1 << 1)
#define BNXT_FLAG_PORT_STATS	(1 << 2)
#define BNXT_FLAG_JUMBO		(1 << 3)
#define BNXT_FLAG_SHORT_CMD	(1 << 4)
#define BNXT_PF(bp)		(!((bp)->flags & BNXT_FLAG_VF))
#define BNXT_VF(bp)		((bp)->flags & BNXT_FLAG_VF)
#define BNXT_NPAR_ENABLED(bp)	((bp)->port_partition_type)
#define BNXT_NPAR_PF(bp)	(BNXT_PF(bp) && BNXT_NPAR_ENABLED(bp))

	unsigned int		rx_nr_rings;
	unsigned int		rx_cp_nr_rings;
	struct bnxt_rx_queue **rx_queues;
	const void		*rx_mem_zone;
	struct rx_port_stats    *hw_rx_port_stats;
	phys_addr_t		hw_rx_port_stats_map;

	unsigned int		tx_nr_rings;
	unsigned int		tx_cp_nr_rings;
	struct bnxt_tx_queue **tx_queues;
	const void		*tx_mem_zone;
	struct tx_port_stats    *hw_tx_port_stats;
	phys_addr_t		hw_tx_port_stats_map;

	/* Default completion ring */
	struct bnxt_cp_ring_info	*def_cp_ring;
	uint32_t		max_ring_grps;
	struct bnxt_ring_grp_info	*grp_info;

	unsigned int		nr_vnics;

	struct bnxt_vnic_info	*vnic_info;
	STAILQ_HEAD(, bnxt_vnic_info)	free_vnic_list;

	struct bnxt_filter_info	*filter_info;
	STAILQ_HEAD(, bnxt_filter_info)	free_filter_list;

	/* VNIC pointer for flow filter (VMDq) pools */
#define MAX_FF_POOLS	ETH_64_POOLS
	STAILQ_HEAD(, bnxt_vnic_info)	ff_pool[MAX_FF_POOLS];

	struct bnxt_irq         *irq_tbl;

#define MAX_NUM_MAC_ADDR	32
	uint8_t			mac_addr[ETHER_ADDR_LEN];

	uint16_t			hwrm_cmd_seq;
	void				*hwrm_cmd_resp_addr;
	phys_addr_t			hwrm_cmd_resp_dma_addr;
	void				*hwrm_short_cmd_req_addr;
	phys_addr_t			hwrm_short_cmd_req_dma_addr;
	rte_spinlock_t			hwrm_lock;
	uint16_t			max_req_len;
	uint16_t			max_resp_len;

	struct bnxt_link_info	link_info;
	struct bnxt_cos_queue_info	cos_queue[BNXT_COS_QUEUE_COUNT];

	uint16_t		fw_fid;
	uint8_t			dflt_mac_addr[ETHER_ADDR_LEN];
	uint16_t		max_rsscos_ctx;
	uint16_t		max_cp_rings;
	uint16_t		max_tx_rings;
	uint16_t		max_rx_rings;
	uint16_t		max_l2_ctx;
	uint16_t		max_vnics;
	uint16_t		max_stat_ctx;
	uint16_t		vlan;
	struct bnxt_pf_info		pf;
	uint8_t			port_partition_type;
	uint8_t			dev_stopped;
	uint8_t			vxlan_port_cnt;
	uint8_t			geneve_port_cnt;
	uint16_t		vxlan_port;
	uint16_t		geneve_port;
	uint16_t		vxlan_fw_dst_port_id;
	uint16_t		geneve_fw_dst_port_id;
	uint32_t		fw_ver;
	rte_atomic64_t		rx_mbuf_alloc_fail;

	struct bnxt_led_info	leds[BNXT_MAX_LED];
	uint8_t			num_leds;
};

int bnxt_link_update_op(struct rte_eth_dev *eth_dev, int wait_to_complete);
int bnxt_rcv_msg_from_vf(struct bnxt *bp, uint16_t vf_id, void *msg);

#define RX_PROD_AGG_BD_TYPE_RX_PROD_AGG		0x6

bool is_bnxt_supported(struct rte_eth_dev *dev);
#endif
