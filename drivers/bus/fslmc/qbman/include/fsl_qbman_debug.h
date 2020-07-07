/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2015 Freescale Semiconductor, Inc.
 * Copyright 2020 NXP
 */
#ifndef _FSL_QBMAN_DEBUG_H
#define _FSL_QBMAN_DEBUG_H

#include <rte_compat.h>

struct qbman_swp;

struct qbman_fq_query_np_rslt {
uint8_t verb;
	uint8_t rslt;
	uint8_t st1;
	uint8_t st2;
	uint8_t reserved[2];
	uint16_t od1_sfdr;
	uint16_t od2_sfdr;
	uint16_t od3_sfdr;
	uint16_t ra1_sfdr;
	uint16_t ra2_sfdr;
	uint32_t pfdr_hptr;
	uint32_t pfdr_tptr;
	uint32_t frm_cnt;
	uint32_t byte_cnt;
	uint16_t ics_surp;
	uint8_t is;
	uint8_t reserved2[29];
};

__rte_internal
int qbman_fq_query_state(struct qbman_swp *s, uint32_t fqid,
			 struct qbman_fq_query_np_rslt *r);

__rte_internal
uint32_t qbman_fq_state_frame_count(const struct qbman_fq_query_np_rslt *r);

uint32_t qbman_fq_state_byte_count(const struct qbman_fq_query_np_rslt *r);

#endif /* !_FSL_QBMAN_DEBUG_H */
