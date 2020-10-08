/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2020 Marvell International Ltd.
 */

#ifndef _OTX2_CRYPTODEV_OPS_HELPER_H_
#define _OTX2_CRYPTODEV_OPS_HELPER_H_

#include "cpt_pmd_logs.h"

static void
sym_session_clear(int driver_id, struct rte_cryptodev_sym_session *sess)
{
	void *priv = get_sym_session_private_data(sess, driver_id);
	struct rte_mempool *pool;

	if (priv == NULL)
		return;

	memset(priv, 0, cpt_get_session_size());

	pool = rte_mempool_from_obj(priv);

	set_sym_session_private_data(sess, driver_id, NULL);

	rte_mempool_put(pool, priv);
}

static __rte_always_inline uint8_t
otx2_cpt_compcode_get(struct cpt_request_info *req)
{
	volatile struct cpt_res_s_9s *res;
	uint8_t ret;

	res = (volatile struct cpt_res_s_9s *)req->completion_addr;

	if (unlikely(res->compcode == CPT_9X_COMP_E_NOTDONE)) {
		if (rte_get_timer_cycles() < req->time_out)
			return ERR_REQ_PENDING;

		CPT_LOG_DP_ERR("Request timed out");
		return ERR_REQ_TIMEOUT;
	}

	if (likely(res->compcode == CPT_9X_COMP_E_GOOD)) {
		ret = NO_ERR;
		if (unlikely(res->uc_compcode)) {
			ret = res->uc_compcode;
			CPT_LOG_DP_DEBUG("Request failed with microcode error");
			CPT_LOG_DP_DEBUG("MC completion code 0x%x",
					 res->uc_compcode);
		}
	} else {
		CPT_LOG_DP_DEBUG("HW completion code 0x%x", res->compcode);

		ret = res->compcode;
		switch (res->compcode) {
		case CPT_9X_COMP_E_INSTERR:
			CPT_LOG_DP_ERR("Request failed with instruction error");
			break;
		case CPT_9X_COMP_E_FAULT:
			CPT_LOG_DP_ERR("Request failed with DMA fault");
			break;
		case CPT_9X_COMP_E_HWERR:
			CPT_LOG_DP_ERR("Request failed with hardware error");
			break;
		default:
			CPT_LOG_DP_ERR("Request failed with unknown completion code");
		}
	}

	return ret;
}

#endif /* _OTX2_CRYPTODEV_OPS_HELPER_H_ */
