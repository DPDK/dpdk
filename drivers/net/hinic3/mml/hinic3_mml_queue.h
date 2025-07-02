/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * Description   : hinic3 mml for queue
 */

#ifndef _HINIC3_MML_QUEUE
#define _HINIC3_MML_QUEUE

#define OBJ_Q_INFO   0
#define OBJ_WQE_INFO 1
#define OBJ_CQE_INFO 2

/* TX. */
struct nic_tx_ctrl_section {
	union {
		struct {
			unsigned int len : 18;
			unsigned int r : 1;
			unsigned int bdsl : 8;
			unsigned int tss : 1;
			unsigned int df : 1;
			unsigned int dn : 1;
			unsigned int ec : 1;
			unsigned int o : 1;
		} ctrl_sec;
		unsigned int ctrl_format;
	};
	union {
		struct {
			unsigned int pkt_type : 2;
			unsigned int payload_offset : 8;
			unsigned int ufo : 1;
			unsigned int tso : 1;
			unsigned int tcp_udp_cs : 1;
			unsigned int mss : 14;
			unsigned int sctp : 1;
			unsigned int uc : 1;
			unsigned int pri : 3;
		} qsf;
		unsigned int queue_info;
	};
};

struct nic_tx_task_section {
	unsigned int dw0;
	unsigned int dw1;

	/* dw2. */
	union {
		struct {
			/*
			 * When TX direct, output bond id;
			 * when RX direct, output function id.
			 */
			unsigned int vport_id : 12;
			unsigned int vport_type : 4;
			unsigned int traffic_type : 6;
			/*
			 * Only used in TX direct, ctrl pkt(LACP\LLDP) output
			 * port id.
			 */
			unsigned int secondary_port_id : 2;
			unsigned int rsvd0 : 6;
			unsigned int crypto_en : 1;
			unsigned int pkt_type : 1;
		} bs2;
		unsigned int dw2;
	};

	unsigned int dw3;
};

struct nic_tx_sge {
	union {
		struct {
			unsigned int length : 31; /**< SGE length. */
			unsigned int rsvd : 1;
		} bs0;
		unsigned int dw0;
	};

	union {
		struct {
			/* Key or unused. */
			unsigned int key : 30;
			/* 0:normal, 1:pointer to next SGE. */
			unsigned int extension : 1;
			/* 0:list, 1:last. */
			unsigned int list : 1;
		} bs1;
		unsigned int dw1;
	};

	unsigned int dma_addr_high;
	unsigned int dma_addr_low;
};

struct nic_tx_wqe_desc {
	struct nic_tx_ctrl_section control;
	struct nic_tx_task_section task;
	unsigned int bd0_hi_addr;
	unsigned int bd0_lo_addr;
};

/* RX. */
typedef struct tag_l2nic_rx_cqe {
	union {
		struct {
			unsigned int checksum_err : 16;
			unsigned int lro_num : 8;
			unsigned int rsvd0 : 1;
			unsigned int spec_flags : 3;
			unsigned int flush : 1;
			unsigned int decry_pkt : 1;
			unsigned int bp_en : 1;
			unsigned int rx_done : 1;
		} bs;
		unsigned int value;
	} dw0;

	union {
		struct {
			unsigned int vlan : 16;
			unsigned int length : 16;
		} bs;
		unsigned int value;
	} dw1;

	union {
		struct {
			unsigned int pkt_types : 12;
			unsigned int rsvd1 : 7;
			unsigned int umbcast : 2;
			unsigned int vlan_offload_en : 1;
			unsigned int rsvd0 : 2;
			unsigned int rss_type : 8;
		} bs;
		unsigned int value;
	} dw2;

	union {
		struct {
			unsigned int rss_hash_value;
		} bs;
		unsigned int value;
	} dw3;

	/* dw4~dw7 field for nic/ovs multipexing. */
	union {
		struct { /**< For nic. */
			unsigned int tx_ts_seq : 16;
			unsigned int msg_1588_offset : 8;
			unsigned int msg_1588_type : 4;
			unsigned int rsvd : 1;
			unsigned int if_rx_ts : 1;
			unsigned int if_tx_ts : 1;
			unsigned int if_1588 : 1;
		} bs;

		struct { /**< For ovs. */
			unsigned int reserved;
		} ovs_bs;

		struct {
			unsigned int xid;
		} crypt_bs;

		unsigned int value;
	} dw4;

	union {
		struct { /**< For nic. */
			unsigned int msg_1588_ts;
		} bs;

		struct { /**< For ovs. */
			unsigned int traffic_from : 16;
			unsigned int traffic_type : 6;
			unsigned int rsvd0 : 2;
			unsigned int l4_type : 3;
			unsigned int l3_type : 3;
			unsigned int mac_type : 2;
		} ovs_bs;

		struct { /**< For crypt. */
			unsigned int esp_next_head : 8;
			unsigned int decrypt_status : 8;
			unsigned int rsvd : 16;
		} crypt_bs;

		unsigned int value;
	} dw5;

	union {
		struct { /**< For nic. */
			unsigned int lro_ts;
		} bs;

		struct { /**< For ovs. */
			unsigned int reserved;
		} ovs_bs;

		unsigned int value;
	} dw6;

	union {
		struct { /**< For nic. */
			/* Data len of the first or middle pkt size. */
			unsigned int first_len : 13;
			/* Data len of the last pkt size. */
			unsigned int last_len : 13;
			/* The number of packet. */
			unsigned int pkt_num : 5;
			/* Only this bit = 1, other dw fields is valid. */
			unsigned int super_cqe_en : 1;
		} bs;

		struct { /**< For ovs. */
			unsigned int localtag;
		} ovs_bs;

		unsigned int value;
	} dw7;
} l2nic_rx_cqe_s;

struct nic_rq_bd_sec {
	unsigned int pkt_buf_addr_high; /**< Packet buffer address high. */
	unsigned int pkt_buf_addr_low;	/**< Packet buffer address low. */
	unsigned int len;
};

typedef struct _nic_rq_wqe {
	/* RX buffer SGE. Notes, buf_desc.len limit in bit 0~13. */
	struct nic_rq_bd_sec buf_desc;
	/* Reserved field 0 for 16B align. */
	unsigned int rsvd0;
	/*
	 * CQE buffer SGE. Notes, cqe_sect.len is in unit of 16B and limit in
	 * bit 0~4.
	 */
	struct nic_rq_bd_sec cqe_sect;
	/* Reserved field 1 for unused. */
	unsigned int rsvd1;
} nic_rq_wqe;

/* CMD. */
struct cmd_show_q_st {
	struct tool_target target;

	int qobj;
	int q_id;
	int wqe_id;
	int direction;
};

#endif /* _HINIC3_MML_QUEUE */
