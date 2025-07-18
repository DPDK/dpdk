/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2021 HiSilicon Limited.
 */

#ifndef HNS3_MBX_H
#define HNS3_MBX_H

#include <stdint.h>

#include <rte_spinlock.h>

#include "hns3_cmd.h"

enum HNS3_MBX_OPCODE {
	HNS3_MBX_RESET = 0x01,          /* (VF -> PF) assert reset */
	HNS3_MBX_ASSERTING_RESET,       /* (PF -> VF) PF is asserting reset */
	HNS3_MBX_SET_UNICAST,           /* (VF -> PF) set UC addr */
	HNS3_MBX_SET_MULTICAST,         /* (VF -> PF) set MC addr */
	HNS3_MBX_SET_VLAN,              /* (VF -> PF) set VLAN */
	HNS3_MBX_MAP_RING_TO_VECTOR,    /* (VF -> PF) map ring-to-vector */
	HNS3_MBX_UNMAP_RING_TO_VECTOR,  /* (VF -> PF) unamp ring-to-vector */
	HNS3_MBX_SET_PROMISC_MODE,      /* (VF -> PF) set promiscuous mode */
	HNS3_MBX_SET_MACVLAN,           /* (VF -> PF) set unicast filter */
	HNS3_MBX_API_NEGOTIATE,         /* (VF -> PF) negotiate API version */
	HNS3_MBX_GET_QINFO,             /* (VF -> PF) get queue config */
	HNS3_MBX_GET_QDEPTH,            /* (VF -> PF) get queue depth */
	HNS3_MBX_GET_BASIC_INFO,        /* (VF -> PF) get basic info */
	HNS3_MBX_GET_RETA,              /* (VF -> PF) get RETA */
	HNS3_MBX_GET_RSS_KEY,           /* (VF -> PF) get RSS key */
	HNS3_MBX_GET_MAC_ADDR,          /* (VF -> PF) get MAC addr */
	HNS3_MBX_PF_VF_RESP,            /* (PF -> VF) generate response to VF */
	HNS3_MBX_GET_BDNUM,             /* (VF -> PF) get BD num */
	HNS3_MBX_GET_BUFSIZE,           /* (VF -> PF) get buffer size */
	HNS3_MBX_GET_STREAMID,          /* (VF -> PF) get stream id */
	HNS3_MBX_SET_AESTART,           /* (VF -> PF) start ae */
	HNS3_MBX_SET_TSOSTATS,          /* (VF -> PF) get tso stats */
	HNS3_MBX_LINK_STAT_CHANGE,      /* (PF -> VF) link status has changed */
	HNS3_MBX_GET_BASE_CONFIG,       /* (VF -> PF) get config */
	HNS3_MBX_BIND_FUNC_QUEUE,       /* (VF -> PF) bind function and queue */
	HNS3_MBX_GET_LINK_STATUS,       /* (VF -> PF) get link status */
	HNS3_MBX_QUEUE_RESET,           /* (VF -> PF) reset queue */
	HNS3_MBX_KEEP_ALIVE,            /* (VF -> PF) send keep alive cmd */
	HNS3_MBX_SET_ALIVE,             /* (VF -> PF) set alive state */
	HNS3_MBX_SET_MTU,               /* (VF -> PF) set mtu */
	HNS3_MBX_GET_QID_IN_PF,         /* (VF -> PF) get queue id in pf */

	HNS3_MBX_PUSH_VLAN_INFO = 34,   /* (PF -> VF) push port base vlan */

	HNS3_MBX_PUSH_PROMISC_INFO = 36, /* (PF -> VF) push vf promisc info */
	HNS3_MBX_VF_UNINIT,              /* (VF -> PF) vf is unintializing */

	HNS3_MBX_HANDLE_VF_TBL = 38,    /* (VF -> PF) store/clear hw cfg tbl */
	HNS3_MBX_GET_RING_VECTOR_MAP,   /* (VF -> PF) get ring-to-vector map */

	HNS3_MBX_GET_TC = 47,           /* (VF -> PF) get tc info of PF configured */
	HNS3_MBX_SET_TC,                /* (VF -> PF) set tc */

	HNS3_MBX_PUSH_LINK_STATUS = 201, /* (IMP -> PF) get port link status */
};

struct hns3_basic_info {
	uint8_t hw_tc_map;
	uint8_t tc_max;
	uint16_t pf_vf_if_version;
	/* capabilities of VF dependent on PF */
	uint32_t caps;
};

enum hns3_mbx_get_tc_subcode {
	HNS3_MBX_GET_PRIO_MAP = 0, /* query priority to tc map */
	HNS3_MBX_GET_ETS_INFO,     /* query ets info */
};

struct hns3_mbx_tc_prio_map {
	/*
	 * Each four bits correspond to one priority's TC.
	 * Bit0-3 correspond to priority-0's TC, bit4-7 correspond to
	 * priority-1's TC, and so on.
	 */
	uint32_t prio_tc_map;
};

#define HNS3_ETS_SCHED_MODE_INVALID	255
#define HNS3_ETS_DWRR_MAX		100
struct hns3_mbx_tc_ets_info {
	uint8_t sch_mode[HNS3_MAX_TC_NUM]; /* 1~100: DWRR, 0: SP; 255-invalid */
};

#define HNS3_MBX_PRIO_SHIFT	4
#define HNS3_MBX_PRIO_MASK	0xFu
struct __rte_packed_begin hns3_mbx_tc_config {
	/*
	 * Each four bits correspond to one priority's TC.
	 * Bit0-3 correspond to priority-0's TC, bit4-7 correspond to
	 * priority-1's TC, and so on.
	 */
	uint32_t prio_tc_map;
	uint8_t tc_dwrr[HNS3_MAX_TC_NUM];
	uint8_t num_tc;
	/*
	 * Each bit correspond to one TC's scheduling mode, 0 means SP
	 * scheduling mode, 1 means DWRR scheduling mode.
	 * Bit0 corresponds to TC0, bit1 corresponds to TC1, and so on.
	 */
	uint8_t tc_sch_mode;
} __rte_packed_end;

/* below are per-VF mac-vlan subcodes */
enum hns3_mbx_mac_vlan_subcode {
	HNS3_MBX_MAC_VLAN_UC_MODIFY = 0,        /* modify UC mac addr */
	HNS3_MBX_MAC_VLAN_UC_ADD,               /* add a new UC mac addr */
	HNS3_MBX_MAC_VLAN_UC_REMOVE,            /* remove a new UC mac addr */
	HNS3_MBX_MAC_VLAN_MC_MODIFY,            /* modify MC mac addr */
	HNS3_MBX_MAC_VLAN_MC_ADD,               /* add new MC mac addr */
	HNS3_MBX_MAC_VLAN_MC_REMOVE,            /* remove MC mac addr */
};

/* below are per-VF vlan cfg subcodes */
enum hns3_mbx_vlan_cfg_subcode {
	HNS3_MBX_VLAN_FILTER = 0,               /* set vlan filter */
	HNS3_MBX_VLAN_TX_OFF_CFG,               /* set tx side vlan offload */
	HNS3_MBX_VLAN_RX_OFF_CFG,               /* set rx side vlan offload */
	HNS3_MBX_GET_PORT_BASE_VLAN_STATE = 4,  /* get port based vlan state */
	HNS3_MBX_ENABLE_VLAN_FILTER,            /* set vlan filter state */
};

enum hns3_mbx_tbl_cfg_subcode {
	HNS3_MBX_VPORT_LIST_CLEAR = 0,
};

enum hns3_mbx_link_fail_subcode {
	HNS3_MBX_LF_NORMAL = 0,
	HNS3_MBX_LF_REF_CLOCK_LOST,
	HNS3_MBX_LF_XSFP_TX_DISABLE,
	HNS3_MBX_LF_XSFP_ABSENT,
};

#define HNS3_MBX_MAX_RESP_DATA_SIZE	8
#define HNS3_MBX_DEF_TIME_LIMIT_MS	500

struct hns3_mbx_resp_status {
	rte_spinlock_t lock; /* protects against contending sync cmd resp */

	/* The following fields used in the matching scheme for original */
	uint32_t req_msg_data;

	/* The following fields used in the matching scheme for match_id */
	uint16_t match_id;
	bool received_match_resp;

	int resp_status;
	uint8_t additional_info[HNS3_MBX_MAX_RESP_DATA_SIZE];
};

struct hns3_ring_chain_param {
	uint8_t ring_type;
	uint8_t tqp_index;
	uint8_t int_gl_index;
};

struct __rte_packed_begin hns3_mbx_vlan_filter {
	uint8_t is_kill;
	uint16_t vlan_id;
	uint16_t proto;
} __rte_packed_end;

struct __rte_packed_begin hns3_mbx_link_status {
	uint16_t link_status;
	uint32_t speed;
	uint16_t duplex;
	uint8_t flag;
} __rte_packed_end;

#define HNS3_MBX_MSG_MAX_DATA_SIZE	14
#define HNS3_MBX_MAX_RING_CHAIN_PARAM_NUM	4
struct hns3_vf_to_pf_msg {
	uint8_t code;
	union {
		struct {
			uint8_t subcode;
			uint8_t data[HNS3_MBX_MSG_MAX_DATA_SIZE];
		};
		struct {
			uint8_t en_bc;
			uint8_t en_uc;
			uint8_t en_mc;
			uint8_t en_limit_promisc;
		};
		struct {
			uint8_t vector_id;
			uint8_t ring_num;
			struct hns3_ring_chain_param
				ring_param[HNS3_MBX_MAX_RING_CHAIN_PARAM_NUM];
		};
		struct {
			uint8_t link_status;
			uint8_t link_fail_code;
		};
	};
};

struct hns3_pf_to_vf_msg {
	uint16_t code;
	union {
		struct {
			uint16_t vf_mbx_msg_code;
			uint16_t vf_mbx_msg_subcode;
			uint16_t resp_status;
			uint8_t resp_data[HNS3_MBX_MAX_RESP_DATA_SIZE];
		};
		uint16_t promisc_en;
		uint16_t reset_level;
		uint16_t pvid_state;
		uint8_t msg_data[HNS3_MBX_MSG_MAX_DATA_SIZE];
	};
};

struct errno_respcode_map {
	uint16_t resp_code;
	int err_no;
};

#define HNS3_MBX_NEED_RESP_BIT                BIT(0)

struct hns3_mbx_vf_to_pf_cmd {
	uint8_t rsv;
	uint8_t mbx_src_vfid;                   /* Auto filled by IMP */
	uint8_t mbx_need_resp;
	uint8_t rsv1;
	uint8_t msg_len;
	uint8_t rsv2;
	uint16_t match_id;
	struct hns3_vf_to_pf_msg msg;
};

struct hns3_mbx_pf_to_vf_cmd {
	uint8_t dest_vfid;
	uint8_t rsv[3];
	uint8_t msg_len;
	uint8_t rsv1;
	uint16_t match_id;
	struct hns3_pf_to_vf_msg msg;
};

struct hns3_pf_rst_done_cmd {
	uint8_t pf_rst_done;
	uint8_t rsv[23];
};

#define HNS3_PF_RESET_DONE_BIT		BIT(0)

#define hns3_mbx_ring_ptr_move_crq(crq) \
	((crq)->next_to_use = ((crq)->next_to_use + 1) % (crq)->desc_num)

struct hns3_hw;
void hns3pf_handle_mbx_msg(struct hns3_hw *hw);
void hns3vf_handle_mbx_msg(struct hns3_hw *hw);
void hns3vf_mbx_setup(struct hns3_vf_to_pf_msg *req,
		      uint8_t code, uint8_t subcode);
int hns3vf_mbx_send(struct hns3_hw *hw,
		    struct hns3_vf_to_pf_msg *req_msg, bool need_resp,
		    uint8_t *resp_data, uint16_t resp_len);
#endif /* HNS3_MBX_H */
