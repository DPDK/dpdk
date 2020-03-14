/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Huawei Technologies Co., Ltd
 */

#ifndef _HINIC_PMD_HWDEV_H_
#define _HINIC_PMD_HWDEV_H_

#include "hinic_pmd_cmd.h"

#define HINIC_PAGE_SIZE_MAX		20

#define HINIC_MGMT_CMD_UNSUPPORTED	0xFF
#define HINIC_PF_SET_VF_ALREADY		0x4

#define MAX_PCIE_DFX_BUF_SIZE		1024

#define HINIC_DEV_BUSY_ACTIVE_FW	0xFE

/* dma pool */
struct dma_pool {
	rte_atomic32_t inuse;
	size_t elem_size;
	size_t align;
	size_t boundary;
	void *hwdev;

	char name[32];
};

enum hinic_res_state {
	HINIC_RES_CLEAN = 0,
	HINIC_RES_ACTIVE = 1,
};

enum hilink_info_print_event {
	HILINK_EVENT_LINK_UP = 1,
	HILINK_EVENT_LINK_DOWN,
	HILINK_EVENT_CABLE_PLUGGED,
	HILINK_EVENT_MAX_TYPE,
};

struct hinic_port_link_status {
	struct hinic_mgmt_msg_head mgmt_msg_head;

	u16	func_id;
	u8	link;
	u8	port_id;
};

enum link_err_status {
	LINK_ERR_MODULE_UNRECOGENIZED,
	LINK_ERR_NUM,
};

struct hinic_cable_plug_event {
	struct hinic_mgmt_msg_head mgmt_msg_head;

	u16	func_id;
	u8	plugged;	/* 0: unplugged, 1: plugged */
	u8	port_id;
};

struct hinic_link_err_event {
	struct hinic_mgmt_msg_head mgmt_msg_head;

	u16	func_id;
	u8	err_type;
	u8	port_id;
};

struct hinic_cons_idx_attr {
	struct hinic_mgmt_msg_head mgmt_msg_head;

	u16	func_idx;
	u8	dma_attr_off;
	u8	pending_limit;
	u8	coalescing_time;
	u8	intr_en;
	u16	intr_idx;
	u32	l2nic_sqn;
	u32	sq_id;
	u64	ci_addr;
};

struct hinic_clear_doorbell {
	struct hinic_mgmt_msg_head mgmt_msg_head;

	u16	func_idx;
	u8	ppf_idx;
	u8	rsvd1;
};

struct hinic_clear_resource {
	struct hinic_mgmt_msg_head mgmt_msg_head;

	u16	func_idx;
	u8	ppf_idx;
	u8	rsvd1;
};

struct hinic_cmd_set_res_state {
	struct hinic_mgmt_msg_head mgmt_msg_head;

	u16	func_idx;
	u8	state;
	u8	rsvd1;
	u32	rsvd2;
};

struct hinic_l2nic_reset {
	struct hinic_mgmt_msg_head mgmt_msg_head;

	u16 func_id;
	u16 rsvd1;
};

struct hinic_page_size {
	struct hinic_mgmt_msg_head mgmt_msg_head;

	u16	func_idx;
	u8	ppf_idx;
	u8	page_size;
	u32	rsvd;
};

struct hinic_msix_config {
	struct hinic_mgmt_msg_head mgmt_msg_head;

	u16	func_id;
	u16	msix_index;
	u8	pending_cnt;
	u8	coalesct_timer_cnt;
	u8	lli_tmier_cnt;
	u8	lli_credit_cnt;
	u8	resend_timer_cnt;
	u8	rsvd1[3];
};

/* defined by chip */
enum hinic_fault_type {
	FAULT_TYPE_CHIP,
	FAULT_TYPE_UCODE,
	FAULT_TYPE_MEM_RD_TIMEOUT,
	FAULT_TYPE_MEM_WR_TIMEOUT,
	FAULT_TYPE_REG_RD_TIMEOUT,
	FAULT_TYPE_REG_WR_TIMEOUT,
	FAULT_TYPE_MAX,
};

/* defined by chip */
enum hinic_fault_err_level {
	/* default err_level=FAULT_LEVEL_FATAL if
	 * type==FAULT_TYPE_MEM_RD_TIMEOUT || FAULT_TYPE_MEM_WR_TIMEOUT ||
	 *	 FAULT_TYPE_REG_RD_TIMEOUT || FAULT_TYPE_REG_WR_TIMEOUT ||
	 *	 FAULT_TYPE_UCODE
	 * other: err_level in event.chip.err_level if type==FAULT_TYPE_CHIP
	 */
	FAULT_LEVEL_FATAL,
	FAULT_LEVEL_SERIOUS_RESET,
	FAULT_LEVEL_SERIOUS_FLR,
	FAULT_LEVEL_GENERAL,
	FAULT_LEVEL_SUGGESTION,
	FAULT_LEVEL_MAX
};

/* defined by chip */
struct hinic_fault_event {
	/* enum hinic_fault_type */
	u8 type;
	u8 rsvd0[3];
	union {
		u32 val[4];
		/* valid only type==FAULT_TYPE_CHIP */
		struct {
			u8 node_id;
			/* enum hinic_fault_err_level */
			u8 err_level;
			u8 err_type;
			u8 rsvd1;
			u32 err_csr_addr;
			u32 err_csr_value;
		/* func_id valid only err_level==FAULT_LEVEL_SERIOUS_FLR */
			u16 func_id;
			u16 rsvd2;
		} chip;

		/* valid only type==FAULT_TYPE_UCODE */
		struct {
			u8 cause_id;
			u8 core_id;
			u8 c_id;
			u8 rsvd3;
			u32 epc;
			u32 rsvd4;
			u32 rsvd5;
		} ucode;

		/* valid only type==FAULT_TYPE_MEM_RD_TIMEOUT ||
		 *		FAULT_TYPE_MEM_WR_TIMEOUT
		 */
		struct {
			u32 err_csr_ctrl;
			u32 err_csr_data;
			u32 ctrl_tab;
			u32 mem_index;
		} mem_timeout;

		/* valid only type==FAULT_TYPE_REG_RD_TIMEOUT ||
		 *		    FAULT_TYPE_REG_WR_TIMEOUT
		 */
		struct {
			u32 err_csr;
			u32 rsvd6;
			u32 rsvd7;
			u32 rsvd8;
		} reg_timeout;
	} event;
};

struct hinic_cmd_fault_event {
	struct hinic_mgmt_msg_head mgmt_msg_head;

	struct hinic_fault_event event;
};

struct hinic_mgmt_watchdog_info {
	struct hinic_mgmt_msg_head mgmt_msg_head;

	u32 curr_time_h;
	u32 curr_time_l;
	u32 task_id;
	u32 rsv;

	u32 reg[13];
	u32 pc;
	u32 lr;
	u32 cpsr;

	u32 stack_top;
	u32 stack_bottom;
	u32 sp;
	u32 curr_used;
	u32 peak_used;
	u32 is_overflow;

	u32 stack_actlen;
	u8 data[1024];
};

struct hinic_pcie_dfx_ntc {
	struct hinic_mgmt_msg_head mgmt_msg_head;

	int len;
	u32 rsvd;
};

struct hinic_pcie_dfx_info {
	struct hinic_mgmt_msg_head mgmt_msg_head;

	u8 host_id;
	u8 last;
	u8 rsvd[2];
	u32 offset;

	u8 data[MAX_PCIE_DFX_BUF_SIZE];
};

struct ffm_intr_info {
	u8 node_id;
	/* error level of the interrupt source */
	u8 err_level;
	/* Classification by interrupt source properties */
	u16 err_type;
	u32 err_csr_addr;
	u32 err_csr_value;
};

struct hinic_board_info {
	u32	board_type;
	u32	port_num;
	u32	port_speed;
	u32	pcie_width;
	u32	host_num;
	u32	pf_num;
	u32	vf_total_num;
	u32	tile_num;
	u32	qcm_num;
	u32	core_num;
	u32	work_mode;
	u32	service_mode;
	u32	pcie_mode;
	u32	cfg_addr;
	u32	boot_sel;
};

struct hinic_comm_board_info {
	struct hinic_mgmt_msg_head mgmt_msg_head;

	struct hinic_board_info info;

	u32	rsvd1[5];
};

struct hi30_ctle_data {
	u8 ctlebst[3];
	u8 ctlecmband[3];
	u8 ctlermband[3];
	u8 ctleza[3];
	u8 ctlesqh[3];
	u8 ctleactgn[3];
	u8 ctlepassgn;
};

struct hi30_ffe_data {
	u8 PRE2;
	u8 PRE1;
	u8 POST1;
	u8 POST2;
	u8 MAIN;
};

enum hilink_fec_type {
	HILINK_FEC_RSFEC,
	HILINK_FEC_BASEFEC,
	HILINK_FEC_NOFEC,
	HILINK_FEC_MAX_TYPE,
};

enum hinic_link_port_type {
	LINK_PORT_FIBRE	= 1,
	LINK_PORT_ELECTRIC,
	LINK_PORT_COPPER,
	LINK_PORT_AOC,
	LINK_PORT_BACKPLANE,
	LINK_PORT_BASET,
	LINK_PORT_MAX_TYPE,
};

enum hilink_fibre_subtype {
	FIBRE_SUBTYPE_SR = 1,
	FIBRE_SUBTYPE_LR,
	FIBRE_SUBTYPE_MAX,
};

struct hinic_link_info {
	u8	vendor_name[16];
	/* port type:
	 * 1 - fiber; 2 - electric; 3 - copper; 4 - AOC; 5 - backplane;
	 * 6 - baseT; 0xffff - unknown
	 *
	 * port subtype:
	 * Only when port_type is fiber:
	 * 1 - SR; 2 - LR
	 */
	u32	port_type;
	u32	port_sub_type;
	u32	cable_length;
	u8	cable_temp;
	u8	cable_max_speed;/* 1(G)/10(G)/25(G)... */
	u8	sfp_type;	/* 0 - qsfp; 1 - sfp */
	u8	rsvd0;
	u32	power[4];	/* uW; if is sfp, only power[2] is valid */

	u8	an_state;	/* 0 - off; 1 - on */
	u8	fec;		/* 0 - RSFEC; 1 - BASEFEC; 2 - NOFEC */
	u16	speed;		/* 1(G)/10(G)/25(G)... */

	u8	cable_absent;	/* 0 - cable present; 1 - cable unpresent */
	u8	alos;		/* 0 - yes; 1 - no */
	u8	rx_los;		/* 0 - yes; 1 - no */
	u8	pma_status;
	u32	pma_dbg_info_reg;	/* pma debug info: */
	u32	pma_signal_ok_reg;	/* signal ok: */

	u32	pcs_err_blk_cnt_reg;	/* error block counter: */
	u32	rf_lf_status_reg;	/* RF/LF status: */
	u8	pcs_link_reg;		/* pcs link: */
	u8	mac_link_reg;		/* mac link: */
	u8	mac_tx_en;
	u8	mac_rx_en;
	u32	pcs_err_cnt;

	u8	lane_used;
	u8	hi30_ffe[5];
	u8	hi30_ctle[19];
	u8	hi30_dfe[14];
	u8	rsvd4;
};

struct hinic_hilink_link_info {
	struct hinic_mgmt_msg_head mgmt_msg_head;

	u16	port_id;
	u8	info_type;	/* 1: link up  2: link down  3 cable plugged */
	u8	rsvd1;

	struct hinic_link_info info;

	u8	rsvd2[780];
};

/* dma os dependency implementation */
struct hinic_os_dep {
	/* kernel dma alloc api */
	rte_atomic32_t dma_alloc_cnt;
	rte_spinlock_t  dma_hash_lock;
	struct rte_hash *dma_addr_hash;
};

struct nic_interrupt_info {
	u32 lli_set;
	u32 interrupt_coalesc_set;
	u16 msix_index;
	u8 lli_credit_limit;
	u8 lli_timer_cfg;
	u8 pending_limt;
	u8 coalesc_timer_cfg;
	u8 resend_timer_cfg;
};

struct hinic_sq_attr {
	u8 dma_attr_off;
	u8 pending_limit;
	u8 coalescing_time;
	u8 intr_en;
	u16 intr_idx;
	u32 l2nic_sqn;
	/* bit[63:2] is addr's high 62bit, bit[0] is valid flag */
	u64 ci_dma_base;
};

struct hinic_hwdev {
	struct rte_pci_device *pcidev_hdl;
	u32 ffm_num;

	/* dma memory allocator */
	struct hinic_os_dep os_dep;
	struct hinic_hwif *hwif;
	struct cfg_mgmt_info *cfg_mgmt;
	struct hinic_aeqs *aeqs;
	struct hinic_mbox_func_to_func *func_to_func;
	struct hinic_msg_pf_to_mgmt *pf_to_mgmt;
	struct hinic_cmdqs *cmdqs;
	struct hinic_nic_io *nic_io;
};

int hinic_osdep_init(struct hinic_hwdev *hwdev);

void hinic_osdep_deinit(struct hinic_hwdev *hwdev);

void dma_free_coherent_volatile(void *hwdev, size_t size,
				volatile void *virt, dma_addr_t phys);

int hinic_get_board_info(void *hwdev, struct hinic_board_info *info);

int hinic_set_ci_table(void *hwdev, u16 q_id, struct hinic_sq_attr *attr);

int hinic_func_rx_tx_flush(struct hinic_hwdev *hwdev);

int hinic_set_interrupt_cfg(struct hinic_hwdev *hwdev,
			    struct nic_interrupt_info interrupt_info);

int init_aeqs_msix_attr(void *hwdev);

void hinic_comm_async_event_handle(struct hinic_hwdev *hwdev, u8 cmd,
				   void *buf_in, u16 in_size,
				   void *buf_out, u16 *out_size);

void hinic_l2nic_async_event_handle(struct hinic_hwdev *hwdev, void *param,
				    u8 cmd, void *buf_in, u16 in_size,
				    void *buf_out, u16 *out_size);

void hinic_hilink_async_event_handle(struct hinic_hwdev *hwdev, u8 cmd,
				     void *buf_in, u16 in_size, void *buf_out,
				     u16 *out_size);

int hinic_init_attr_table(struct hinic_hwdev *hwdev);

int hinic_activate_hwdev_state(struct hinic_hwdev *hwdev);

void hinic_deactivate_hwdev_state(struct hinic_hwdev *hwdev);

int hinic_l2nic_reset(struct hinic_hwdev *hwdev);

int hinic_set_pagesize(void *hwdev, u8 page_size);

void hinic_cpu_to_be32(void *data, u32 len);

void hinic_be32_to_cpu(void *data, u32 len);

#endif /* _HINIC_PMD_HWDEV_H_ */
