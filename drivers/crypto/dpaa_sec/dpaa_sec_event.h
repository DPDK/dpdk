/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2019 NXP
 *
 */

#ifndef _DPAA_SEC_EVENT_H_
#define _DPAA_SEC_EVENT_H_

__rte_internal
int dpaa_sec_eventq_attach(const struct rte_cryptodev *dev,
		int qp_id,
		uint16_t ch_id,
		const struct rte_event *event);

__rte_internal
int dpaa_sec_eventq_detach(const struct rte_cryptodev *dev,
		int qp_id);

#endif /* _DPAA_SEC_EVENT_H_ */
