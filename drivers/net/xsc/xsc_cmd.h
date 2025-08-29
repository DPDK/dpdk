/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2025 Yunsilicon Technology Co., Ltd.
 */

#ifndef _XSC_CMD_H_
#define _XSC_CMD_H_

#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <net/if.h>

#define XSC_BOARD_SN_LEN		32
#define XSC_CMD_QUERY_HCA_CAP_V1	1

enum xsc_cmd_opcode {
	XSC_CMD_OP_QUERY_HCA_CAP		= 0x100,
	XSC_CMD_OP_CREATE_CQ			= 0x400,
	XSC_CMD_OP_DESTROY_CQ			= 0x401,
	XSC_CMD_OP_CREATE_QP			= 0x500,
	XSC_CMD_OP_DESTROY_QP			= 0x501,
	XSC_CMD_OP_RTR2RTS_QP			= 0x504,
	XSC_CMD_OP_QP_2RST			= 0x50A,
	XSC_CMD_OP_CREATE_MULTI_QP		= 0x515,
	XSC_CMD_OP_ACCESS_REG			= 0x805,
	XSC_CMD_OP_MODIFY_NIC_HCA		= 0x812,
	XSC_CMD_OP_MODIFY_RAW_QP		= 0x81f,
	XSC_CMD_OP_QUERY_EVENT_TYPE		= 0x831,
	XSC_CMD_OP_QUERY_LINK_INFO		= 0x832,
	XSC_CMD_OP_ENABLE_MSIX			= 0x850,
	XSC_CMD_OP_EXEC_NP			= 0x900,
	XSC_CMD_OP_SET_MTU			= 0x1100,
	XSC_CMD_OP_QUERY_ETH_MAC		= 0X1101,
	XSC_CMD_OP_SET_PORT_ADMIN_STATUS	= 0x1801,
	XSC_CMD_OP_MAX
};

enum xsc_cmd_status {
	XSC_CMD_SUCC = 0,
	XSC_CMD_FAIL,
	XSC_CMD_TIMEOUT,
};

struct xsc_cmd_inbox_hdr {
	rte_be16_t opcode;
	uint8_t rsvd[4];
	rte_be16_t ver;
};

struct xsc_cmd_outbox_hdr {
	uint8_t status;
	uint8_t rsvd[5];
	rte_be16_t ver;
};

struct xsc_cmd_fw_version {
	uint8_t major;
	uint8_t minor;
	rte_be16_t patch;
	rte_be32_t tweak;
	uint8_t extra_flag;
	uint8_t rsv[7];
};

struct xsc_cmd_hca_cap {
	uint8_t rsvd1[12];
	uint8_t send_seg_num;
	uint8_t send_wqe_shift;
	uint8_t recv_seg_num;
	uint8_t recv_wqe_shift;
	uint8_t log_max_srq_sz;
	uint8_t log_max_qp_sz;
	uint8_t log_max_mtt;
	uint8_t log_max_qp;
	uint8_t log_max_strq_sz;
	uint8_t log_max_srqs;
	uint8_t rsvd2[2];
	uint8_t log_max_tso;
	uint8_t log_max_cq_sz;
	uint8_t rsvd3;
	uint8_t log_max_cq;
	uint8_t log_max_eq_sz;
	uint8_t log_max_mkey;
	uint8_t log_max_msix;
	uint8_t log_max_eq;
	uint8_t max_indirection;
	uint8_t log_max_mrw_sz;
	uint8_t log_max_bsf_list_sz;
	uint8_t log_max_klm_list_sz;
	uint8_t rsvd4;
	uint8_t log_max_ra_req_dc;
	uint8_t rsvd5;
	uint8_t log_max_ra_res_dc;
	uint8_t rsvd6;
	uint8_t log_max_ra_req_qp;
	uint8_t log_max_qp_depth;
	uint8_t log_max_ra_res_qp;
	rte_be16_t max_vfs;
	rte_be16_t raweth_qp_id_end;
	rte_be16_t raw_tpe_qp_num;
	rte_be16_t max_qp_count;
	rte_be16_t raweth_qp_id_base;
	uint8_t rsvd7;
	uint8_t local_ca_ack_delay;
	uint8_t max_num_eqs;
	uint8_t num_ports;
	uint8_t log_max_msg;
	uint8_t mac_port;
	rte_be16_t raweth_rss_qp_id_base;
	rte_be16_t stat_rate_support;
	uint8_t rsvd8[2];
	rte_be64_t flags;
	uint8_t rsvd9;
	uint8_t uar_sz;
	uint8_t rsvd10;
	uint8_t log_pg_sz;
	rte_be16_t bf_log_bf_reg_size;
	rte_be16_t msix_base;
	rte_be16_t msix_num;
	rte_be16_t max_desc_sz_sq;
	uint8_t rsvd11[2];
	rte_be16_t max_desc_sz_rq;
	uint8_t rsvd12[2];
	rte_be16_t max_desc_sz_sq_dc;
	uint8_t rsvd13[4];
	rte_be16_t max_qp_mcg;
	uint8_t rsvd14;
	uint8_t log_max_mcg;
	uint8_t rsvd15;
	uint8_t log_max_pd;
	uint8_t rsvd16;
	uint8_t log_max_xrcd;
	uint8_t rsvd17[40];
	rte_be32_t uar_page_sz;
	uint8_t rsvd18[8];
	rte_be32_t hw_feature_flag;
	rte_be16_t pf0_vf_funcid_base;
	rte_be16_t pf0_vf_funcid_top;
	rte_be16_t pf1_vf_funcid_base;
	rte_be16_t pf1_vf_funcid_top;
	rte_be16_t pcie0_pf_funcid_base;
	rte_be16_t pcie0_pf_funcid_top;
	rte_be16_t pcie1_pf_funcid_base;
	rte_be16_t pcie1_pf_funcid_top;
	uint8_t log_msx_atomic_size_qp;
	uint8_t pcie_host;
	uint8_t rsvd19;
	uint8_t log_msx_atomic_size_dc;
	uint8_t board_sn[XSC_BOARD_SN_LEN];
	uint8_t max_tc;
	uint8_t mac_bit;
	rte_be16_t funcid_to_logic_port;
	uint8_t rsvd20[6];
	uint8_t nif_port_num;
	uint8_t reg_mr_via_cmdq;
	rte_be32_t hca_core_clock;
	rte_be32_t max_rwq_indirection_tables;
	rte_be32_t max_rwq_indirection_table_size;
	rte_be32_t chip_ver_h;
	rte_be32_t chip_ver_m;
	rte_be32_t chip_ver_l;
	rte_be32_t hotfix_num;
	rte_be32_t feature_flag;
	rte_be32_t rx_pkt_len_max;
	rte_be32_t glb_func_id;
	rte_be64_t tx_db;
	rte_be64_t rx_db;
	rte_be64_t complete_db;
	rte_be64_t complete_reg;
	rte_be64_t event_db;
	rte_be32_t qp_rate_limit_min;
	rte_be32_t qp_rate_limit_max;
	struct xsc_cmd_fw_version fw_ver;
	uint8_t lag_logic_port_ofst;
	rte_be64_t max_mr_size;
	rte_be16_t max_cmd_in_len;
	rte_be16_t max_cmd_out_len;
};

struct xsc_cmd_query_hca_cap_mbox_in {
	struct xsc_cmd_inbox_hdr hdr;
	rte_be16_t cpu_num;
	uint8_t rsvd[6];
};

struct xsc_cmd_query_hca_cap_mbox_out {
	struct xsc_cmd_outbox_hdr hdr;
	uint8_t rsvd[8];
	struct xsc_cmd_hca_cap hca_cap;
};

struct xsc_cmd_cq_context {
	uint16_t eqn;
	uint16_t pa_num;
	uint16_t glb_func_id;
	uint8_t log_cq_sz;
	uint8_t cq_type;
};

struct xsc_cmd_create_cq_mbox_in {
	struct xsc_cmd_inbox_hdr hdr;
	struct xsc_cmd_cq_context ctx;
	uint64_t pas[];
};

struct xsc_cmd_create_cq_mbox_out {
	struct xsc_cmd_outbox_hdr hdr;
	uint32_t cqn;
	uint8_t rsvd[4];
};

struct xsc_cmd_destroy_cq_mbox_in {
	struct xsc_cmd_inbox_hdr hdr;
	uint32_t cqn;
	uint8_t rsvd[4];
};

struct xsc_cmd_destroy_cq_mbox_out {
	struct xsc_cmd_outbox_hdr hdr;
	uint8_t rsvd[8];
};

struct xsc_cmd_create_qp_request {
	rte_be16_t input_qpn;
	rte_be16_t pa_num;
	uint8_t qp_type;
	uint8_t log_sq_sz;
	uint8_t log_rq_sz;
	uint8_t dma_direct;
	rte_be32_t pdn;
	rte_be16_t cqn_send;
	rte_be16_t cqn_recv;
	rte_be16_t glb_funcid;
	uint8_t page_shift;
	uint8_t rsvd;
	rte_be64_t pas[];
};

struct xsc_cmd_create_qp_mbox_in {
	struct xsc_cmd_inbox_hdr hdr;
	struct xsc_cmd_create_qp_request req;
};

struct xsc_cmd_create_qp_mbox_out {
	struct xsc_cmd_outbox_hdr hdr;
	uint32_t qpn;
	uint8_t rsvd[4];
};

struct xsc_cmd_create_multiqp_mbox_in {
	struct xsc_cmd_inbox_hdr  hdr;
	rte_be16_t qp_num;
	uint8_t qp_type;
	uint8_t rsvd;
	rte_be32_t req_len;
	uint8_t data[];
};

struct xsc_cmd_create_multiqp_mbox_out {
	struct xsc_cmd_outbox_hdr hdr;
	rte_be32_t qpn_base;
};

struct xsc_cmd_destroy_qp_mbox_in {
	struct xsc_cmd_inbox_hdr hdr;
	rte_be32_t qpn;
	uint8_t rsvd[4];
};

struct xsc_cmd_destroy_qp_mbox_out {
	struct xsc_cmd_outbox_hdr hdr;
	uint8_t rsvd[8];
};

struct xsc_cmd_qp_context {
	rte_be32_t remote_qpn;
	rte_be32_t cqn_send;
	rte_be32_t cqn_recv;
	rte_be32_t next_send_psn;
	rte_be32_t next_recv_psn;
	rte_be32_t pdn;
	rte_be16_t src_udp_port;
	rte_be16_t path_id;
	uint8_t mtu_mode;
	uint8_t lag_sel;
	uint8_t lag_sel_en;
	uint8_t retry_cnt;
	uint8_t rnr_retry;
	uint8_t dscp;
	uint8_t state;
	uint8_t hop_limit;
	uint8_t dmac[6];
	uint8_t smac[6];
	rte_be32_t dip[4];
	rte_be32_t sip[4];
	rte_be16_t ip_type;
	rte_be16_t grp_id;
	uint8_t vlan_valid;
	uint8_t dci_cfi_prio_sl;
	rte_be16_t vlan_id;
	uint8_t qp_out_port;
	uint8_t pcie_no;
	rte_be16_t lag_id;
	rte_be16_t func_id;
	rte_be16_t rsvd;
};

struct xsc_cmd_modify_qp_mbox_in {
	struct xsc_cmd_inbox_hdr hdr;
	rte_be32_t qpn;
	struct xsc_cmd_qp_context ctx;
	uint8_t no_need_wait;
};

struct xsc_cmd_modify_qp_mbox_out {
	struct xsc_cmd_outbox_hdr hdr;
	uint8_t rsvd[8];
};

struct xsc_cmd_modify_raw_qp_request {
	uint16_t qpn;
	uint16_t lag_id;
	uint16_t func_id;
	uint8_t dma_direct;
	uint8_t prio;
	uint8_t qp_out_port;
	uint8_t rsvd[7];
};

struct xsc_cmd_modify_raw_qp_mbox_in {
	struct xsc_cmd_inbox_hdr hdr;
	uint8_t pcie_no;
	uint8_t rsv[7];
	struct xsc_cmd_modify_raw_qp_request req;
};

struct xsc_cmd_modify_raw_qp_mbox_out {
	struct xsc_cmd_outbox_hdr hdr;
	uint8_t rsvd[8];
};

struct xsc_cmd_set_mtu_mbox_in {
	struct xsc_cmd_inbox_hdr hdr;
	rte_be16_t mtu;
	rte_be16_t rx_buf_sz_min;
	uint8_t mac_port;
	uint8_t rsvd;
};

struct xsc_cmd_set_mtu_mbox_out {
	struct xsc_cmd_outbox_hdr hdr;
};

struct xsc_cmd_query_eth_mac_mbox_in {
	struct xsc_cmd_inbox_hdr  hdr;
	uint8_t index;
};

struct xsc_cmd_query_eth_mac_mbox_out {
	struct xsc_cmd_outbox_hdr hdr;
	uint8_t mac[6];
};

struct xsc_cmd_nic_attr {
	rte_be16_t caps;
	rte_be16_t caps_mask;
	uint8_t mac_addr[6];
};

struct xsc_cmd_rss_modify_attr {
	uint8_t caps_mask;
	uint8_t rss_en;
	rte_be16_t rqn_base;
	rte_be16_t rqn_num;
	uint8_t hfunc;
	rte_be32_t hash_tmpl;
	uint8_t hash_key[52];
};

struct xsc_cmd_modify_nic_hca_mbox_in {
	struct xsc_cmd_inbox_hdr hdr;
	struct xsc_cmd_nic_attr nic;
	struct xsc_cmd_rss_modify_attr rss;
};

struct xsc_cmd_modify_nic_hca_mbox_out {
	struct xsc_cmd_outbox_hdr hdr;
	uint8_t rsvd[4];
};

struct xsc_cmd_access_reg_mbox_in {
	struct xsc_cmd_inbox_hdr hdr;
	uint8_t rsvd0[2];
	rte_be16_t register_id;
	rte_be32_t arg;
	rte_be32_t data[];
};

struct xsc_cmd_access_reg_mbox_out {
	struct xsc_cmd_outbox_hdr hdr;
	uint8_t rsvd[8];
	rte_be32_t data[];
};

struct xsc_cmd_set_port_admin_status_mbox_in {
	struct xsc_cmd_inbox_hdr hdr;
	uint16_t admin_status;
};

struct xsc_cmd_set_port_admin_status_mbox_out {
	struct xsc_cmd_outbox_hdr  hdr;
	uint32_t status;
};

struct xsc_cmd_query_linkinfo_mbox_in {
	struct xsc_cmd_inbox_hdr hdr;
};

struct xsc_cmd_linkinfo {
	uint8_t status; /*link status: 0-down, 1-up */
	uint8_t port;
	uint8_t duplex;
	uint8_t autoneg;
	rte_be32_t linkspeed;
	rte_be64_t supported;
	rte_be64_t advertising;
	rte_be64_t supported_fec;
	rte_be64_t advertised_fec;
	rte_be64_t supported_speed[2];
	rte_be64_t advertising_speed[2];
};

struct xsc_cmd_query_linkinfo_mbox_out {
	struct xsc_cmd_outbox_hdr hdr;
	struct xsc_cmd_linkinfo ctx;
};

struct xsc_cmd_modify_linkinfo_mbox_in {
	struct xsc_cmd_inbox_hdr hdr;
	struct xsc_cmd_linkinfo ctx;
};

struct xsc_cmd_modify_linkinfo_mbox_out {
	struct xsc_cmd_outbox_hdr hdr;
	uint32_t status;
};

struct xsc_cmd_event_resp {
	uint8_t event_type;
};

struct xsc_cmd_event_query_type_mbox_in {
	struct xsc_cmd_inbox_hdr hdr;
	uint8_t rsvd[2];
};

struct xsc_cmd_event_query_type_mbox_out {
	struct xsc_cmd_outbox_hdr hdr;
	struct xsc_cmd_event_resp ctx;
};

struct xsc_cmd_msix_table_info_mbox_in {
	struct xsc_cmd_inbox_hdr hdr;
	uint16_t index;
	uint8_t rsvd[6];
};

struct xsc_cmd_msix_table_info_mbox_out {
	struct xsc_cmd_outbox_hdr hdr;
	uint32_t addr_lo;
	uint32_t addr_hi;
	uint32_t data;
};

#endif /* _XSC_CMD_H_ */
