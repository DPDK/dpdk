/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018 NXP
 *
 */

#ifndef _DPAA2_SEC_EVENT_H_
#define _DPAA2_SEC_EVENT_H_

__rte_internal
int dpaa2_sec_eventq_attach(const struct rte_cryptodev *dev,
		int qp_id,
		struct dpaa2_dpcon_dev *dpcon,
		const struct rte_event *event);

__rte_internal
int dpaa2_sec_eventq_detach(const struct rte_cryptodev *dev,
		int qp_id);

#endif /* _DPAA2_SEC_EVENT_H_ */
