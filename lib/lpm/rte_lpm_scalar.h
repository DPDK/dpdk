/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 StarFive
 * Copyright(c) 2022 SiFive
 * Copyright(c) 2022 Semihalf
 */

#ifndef _RTE_LPM_SCALAR_H_
#define _RTE_LPM_SCALAR_H_

#include <rte_vect.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline void
rte_lpm_lookupx4(const struct rte_lpm *lpm, xmm_t ip, uint32_t hop[4],
		uint32_t defv)
{
	rte_xmm_t xip = { .x = ip };
	uint32_t nh;
	int ret;

	ret = rte_lpm_lookup(lpm, xip.u32[0], &nh);
	hop[0] = (ret == 0) ? nh : defv;
	ret = rte_lpm_lookup(lpm, xip.u32[1], &nh);
	hop[1] = (ret == 0) ? nh : defv;
	ret = rte_lpm_lookup(lpm, xip.u32[2], &nh);
	hop[2] = (ret == 0) ? nh : defv;
	ret = rte_lpm_lookup(lpm, xip.u32[3], &nh);
	hop[3] = (ret == 0) ? nh : defv;
}

#ifdef __cplusplus
}
#endif

#endif /* _RTE_LPM_SCALAR_H_ */
