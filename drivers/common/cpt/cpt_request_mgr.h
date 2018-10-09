/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Cavium, Inc
 */

#ifndef _CPT_REQUEST_MGR_H_
#define _CPT_REQUEST_MGR_H_

#include "cpt_mcode_defines.h"

/*
 * This file defines the agreement between the common layer and the individual
 * crypto drivers for OCTEON TX series. Datapath in otx* directory include this
 * file and all these functions are static inlined for better performance.
 *
 */

/*
 * Get the session size
 *
 * This function is used in the data path.
 *
 * @return
 *   - session size
 */
static __rte_always_inline unsigned int
cpt_get_session_size(void)
{
	unsigned int ctx_len = sizeof(struct cpt_ctx);
	return (sizeof(struct cpt_sess_misc) + RTE_ALIGN_CEIL(ctx_len, 8));
}

#endif /* _CPT_REQUEST_MGR_H_ */
