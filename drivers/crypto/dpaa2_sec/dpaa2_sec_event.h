/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018 NXP
 *
 */

#ifndef _DPAA2_SEC_EVENT_H_
#define _DPAA2_SEC_EVENT_H_

int
dpaa2_sec_eventq_attach(const struct rte_cryptodev *dev,
		int qp_id,
		uint16_t dpcon_id,
		const struct rte_event *event);

int dpaa2_sec_eventq_detach(const struct rte_cryptodev *dev,
		int qp_id);

#endif /* _DPAA2_SEC_EVENT_H_ */
