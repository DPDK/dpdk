/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef _HW_MOD_QSL_V7_H_
#define _HW_MOD_QSL_V7_H_

#include <stdint.h>

struct qsl_v7_rcp_s {
	uint32_t discard;
	uint32_t drop;
	uint32_t tbl_lo;
	uint32_t tbl_hi;
	uint32_t tbl_idx;
	uint32_t tbl_msk;
	uint32_t lr;
	uint32_t tsa;
	uint32_t vli;
};

struct qsl_v7_qst_s {
	uint32_t queue;
	uint32_t en;
	uint32_t tx_port;
	uint32_t lre;
	uint32_t tci;
	uint32_t ven;
};

struct qsl_v7_qen_s {
	uint32_t en;
};

struct qsl_v7_unmq_s {
	uint32_t dest_queue;
	uint32_t en;
};

struct hw_mod_qsl_v7_s {
	struct qsl_v7_rcp_s *rcp;
	struct qsl_v7_qst_s *qst;
	struct qsl_v7_qen_s *qen;
	struct qsl_v7_unmq_s *unmq;
};

#endif	/* _HW_MOD_QSL_V7_H_ */
