/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Cavium, Inc
 */

#ifndef _CPT_PMD_OPS_HELPER_H_
#define _CPT_PMD_OPS_HELPER_H_

/*
 * This file defines the agreement between the common layer and the individual
 * crypto drivers for OCTEON TX series. Control path in otx* directory can
 * directly call functions declared here.
 */

/*
 * Get meta length required when operating in direct mode (single buffer
 * in-place)
 *
 * @return
 *   - length
 */

int32_t
cpt_pmd_ops_helper_get_mlen_direct_mode(void);

/*
 * Get size of contiguous meta buffer to be allocated when working in scatter
 * gather mode.
 *
 * @return
 *   - length
 */
int
cpt_pmd_ops_helper_get_mlen_sg_mode(void);
#endif /* _CPT_PMD_OPS_HELPER_H_ */
