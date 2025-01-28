/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2025 Yunsilicon Technology Co., Ltd.
 */

#ifndef _XSC_RXTX_H_
#define _XSC_RXTX_H_

#include <rte_byteorder.h>

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

struct xsc_tx_cq_params {
	uint16_t port_id;
	uint16_t qp_id;
	uint16_t elts_n;
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
};

struct xsc_tx_qp_info {
	void *qp;
	void *wqes;
	uint32_t *qp_db;
	uint32_t qpn;
	uint16_t tso_en;
	uint16_t wqe_n;
};

struct xsc_rx_cq_params {
	uint16_t port_id;
	uint16_t qp_id;
	uint16_t wqe_s;
};

struct xsc_rx_cq_info {
	void *cq;
	void *cqes;
	uint32_t *cq_db;
	uint32_t cqn;
	uint16_t cqe_n;
};

#endif /* _XSC_RXTX_H_ */
