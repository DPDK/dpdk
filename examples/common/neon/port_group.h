/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2018 Intel Corporation.
 * Copyright(c) 2017-2018 Linaro Limited.
 * Copyright(C) 2022 Marvell.
 */

#ifndef PORT_GROUP_H
#define PORT_GROUP_H

#include "pkt_group.h"

/*
 * Group consecutive packets with the same destination port in bursts of 4.
 * Suppose we have array of destination ports:
 * dst_port[] = {a, b, c, d,, e, ... }
 * dp1 should contain: <a, b, c, d>, dp2: <b, c, d, e>.
 * We doing 4 comparisons at once and the result is 4 bit mask.
 * This mask is used as an index into prebuild array of pnum values.
 */
static inline uint16_t *
port_groupx4(uint16_t pn[FWDSTEP + 1], uint16_t *lp, uint16x8_t dp1,
		  uint16x8_t dp2)
{
	union {
		uint16_t u16[FWDSTEP + 1];
		uint64_t u64;
	} __rte_packed *pnum = (void *)pn;

	uint16x8_t mask = {1, 2, 4, 8, 0, 0, 0, 0};
	int32_t v;

	dp1 = vceqq_u16(dp1, dp2);
	dp1 = vandq_u16(dp1, mask);
	v = vaddvq_u16(dp1);

	/* update last port counter. */
	lp[0] += gptbl[v].lpv;
	rte_compiler_barrier();

	/* if dest port value has changed. */
	if (v != GRPMSK) {
		pnum->u64 = gptbl[v].pnum;
		pnum->u16[FWDSTEP] = 1;
		lp = pnum->u16 + gptbl[v].idx;
	}

	return lp;
}

#endif /* PORT_GROUP_H */
