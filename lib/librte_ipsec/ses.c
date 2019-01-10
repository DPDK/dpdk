/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include <rte_ipsec.h>
#include "sa.h"

static int
session_check(struct rte_ipsec_session *ss)
{
	if (ss == NULL || ss->sa == NULL)
		return -EINVAL;

	if (ss->type == RTE_SECURITY_ACTION_TYPE_NONE) {
		if (ss->crypto.ses == NULL)
			return -EINVAL;
	} else {
		if (ss->security.ses == NULL)
			return -EINVAL;
		if ((ss->type == RTE_SECURITY_ACTION_TYPE_INLINE_CRYPTO ||
				ss->type ==
				RTE_SECURITY_ACTION_TYPE_INLINE_PROTOCOL) &&
				ss->security.ctx == NULL)
			return -EINVAL;
	}

	return 0;
}

int __rte_experimental
rte_ipsec_session_prepare(struct rte_ipsec_session *ss)
{
	int32_t rc;
	struct rte_ipsec_sa_pkt_func fp;

	rc = session_check(ss);
	if (rc != 0)
		return rc;

	rc = ipsec_sa_pkt_func_select(ss, ss->sa, &fp);
	if (rc != 0)
		return rc;

	ss->pkt_func = fp;

	if (ss->type == RTE_SECURITY_ACTION_TYPE_NONE)
		ss->crypto.ses->opaque_data = (uintptr_t)ss;
	else
		ss->security.ses->opaque_data = (uintptr_t)ss;

	return 0;
}
