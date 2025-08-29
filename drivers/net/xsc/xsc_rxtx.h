/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2025 Yunsilicon Technology Co., Ltd.
 */

#ifndef _XSC_RXTX_H_
#define _XSC_RXTX_H_

#include <rte_byteorder.h>

#define XSC_CQE_OWNER_MASK		0x1
#define XSC_CQE_OWNER_HW		0x2
#define XSC_CQE_OWNER_SW		0x4
#define XSC_CQE_OWNER_ERR		0x8
#define XSC_OPCODE_RAW			0x7

struct __rte_packed_begin xsc_send_wqe_ctrl_seg {
	rte_le32_t	msg_opcode:8;
	rte_le32_t	with_immdt:1;
	rte_le32_t	csum_en:2;
	rte_le32_t	ds_data_num:5;
	rte_le32_t	wqe_id:16;
	rte_le32_t	msg_len;
	union {
		rte_le32_t		opcode_data;
		struct {
			rte_le16_t	has_pph:1;
			rte_le16_t	so_type:1;
			rte_le16_t	so_data_size:14;
			rte_le16_t	rsv1:8;
			rte_le16_t	so_hdr_len:8;
		};
		struct {
			rte_le16_t	desc_id;
			rte_le16_t	is_last_wqe:1;
			rte_le16_t	dst_qp_id:15;
		};
	};
	rte_le32_t	se:1;
	rte_le32_t	ce:1;
	rte_le32_t	rsv2:30;
} __rte_packed_end;

struct __rte_packed_begin xsc_wqe_data_seg {
	union {
		struct {
			uint8_t		in_line:1;
			uint8_t		rsv0:7;
		};
		struct {
			rte_le32_t	rsv1:1;
			rte_le32_t	seg_len:31;
			rte_le32_t	lkey;
			rte_le64_t	va;
		};
		struct {
			uint8_t		rsv2:1;
			uint8_t		len:7;
			uint8_t		in_line_data[15];
		};
	};
} __rte_packed_end;

struct __rte_packed_begin xsc_wqe {
	union {
		struct xsc_send_wqe_ctrl_seg cseg;
		uint32_t ctrl[4];
	};
	union {
		struct xsc_wqe_data_seg dseg[XSC_SEND_WQE_DS];
		uint8_t data[XSC_ESEG_EXTRA_DATA_SIZE];
	};
} __rte_packed_end;

struct __rte_packed_begin xsc_cqe {
	union {
		uint8_t			msg_opcode;
		struct {
			uint8_t		error_code:7;
			uint8_t		is_error:1;
		};
	};
	rte_le16_t	qp_id:15;
	rte_le16_t	rsv:1;
	uint8_t		se:1;
	uint8_t		has_pph:1;
	uint8_t		type:1;
	uint8_t		with_immdt:1;
	uint8_t		csum_err:4;
	rte_le32_t	imm_data;
	rte_le32_t	msg_len;
	rte_le32_t	vni;
	rte_le32_t	tsl;
	rte_le32_t	tsh:16;
	rte_le32_t	wqe_id:16;
	rte_le16_t	rsv2[3];
	rte_le16_t	rsv3:15;
	rte_le16_t	owner:1;
} __rte_packed_end;

struct xsc_cqe_u64 {
	struct xsc_cqe cqe0;
	struct xsc_cqe cqe1;
};

union xsc_cq_doorbell {
	struct {
		uint32_t next_cid:16;
		uint32_t cq_num:15;
		uint32_t cq_sta:1;
	};
	uint32_t cq_data;
};

union xsc_send_doorbell {
	struct {
		uint32_t next_pid:16;
		uint32_t qp_num:15;
		uint32_t rsv:1;
	};
	uint32_t send_data;
};

struct xsc_tx_cq_params {
	uint16_t port_id;
	uint16_t qp_id;
	uint16_t elts_n;
	int socket_id;
};

struct xsc_tx_cq_info {
	void *cq;
	void *cqes;
	uint32_t *cq_db;
	uint32_t cqn;
	uint16_t cqe_s;
	uint16_t cqe_n;
};

struct xsc_tx_qp_params {
	void *cq;
	uint64_t tx_offloads;
	uint16_t port_id;
	uint16_t qp_id;
	uint16_t elts_n;
	int socket_id;
};

struct xsc_tx_qp_info {
	void *qp;
	void *wqes;
	uint32_t *qp_db;
	uint32_t qpn;
	uint16_t tso_en;
	uint16_t wqe_n;
};

union xsc_recv_doorbell {
	struct {
		uint32_t next_pid:13;
		uint32_t qp_num:15;
		uint32_t rsv:4;
	};
	uint32_t recv_data;
};

struct xsc_rx_cq_params {
	uint16_t port_id;
	uint16_t qp_id;
	uint16_t wqe_s;
	int socket_id;
};

struct xsc_rx_cq_info {
	void *cq;
	void *cqes;
	uint32_t *cq_db;
	uint32_t cqn;
	uint16_t cqe_n;
};

static __rte_always_inline int
xsc_check_cqe_own(volatile struct xsc_cqe *cqe, const uint16_t cqe_n, const uint16_t ci)
{
	if (unlikely(((cqe->owner & XSC_CQE_OWNER_MASK) != ((ci >> cqe_n) & XSC_CQE_OWNER_MASK))))
		return XSC_CQE_OWNER_HW;

	rte_io_rmb();
	if (cqe->msg_len <= 0 && cqe->is_error)
		return XSC_CQE_OWNER_ERR;

	return XSC_CQE_OWNER_SW;
}

#endif /* _XSC_RXTX_H_ */
