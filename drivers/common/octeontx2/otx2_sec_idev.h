/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Marvell International Ltd.
 */

#ifndef _OTX2_SEC_IDEV_H_
#define _OTX2_SEC_IDEV_H_

#include <rte_ethdev.h>

#define OTX2_MAX_CPT_QP_PER_PORT 64
#define OTX2_MAX_INLINE_PORTS 64

struct otx2_cpt_qp;

struct otx2_sec_idev_cfg {
	struct {
		struct otx2_cpt_qp *qp;
		rte_atomic16_t ref_cnt;
	} tx_cpt[OTX2_MAX_CPT_QP_PER_PORT];

	uint16_t tx_cpt_idx;
	rte_spinlock_t tx_cpt_lock;
};

__rte_internal
uint8_t otx2_eth_dev_is_sec_capable(struct rte_eth_dev *eth_dev);

__rte_internal
int otx2_sec_idev_cfg_init(int port_id);

__rte_internal
int otx2_sec_idev_tx_cpt_qp_add(uint16_t port_id, struct otx2_cpt_qp *qp);

__rte_internal
int otx2_sec_idev_tx_cpt_qp_remove(struct otx2_cpt_qp *qp);

__rte_internal
int otx2_sec_idev_tx_cpt_qp_put(struct otx2_cpt_qp *qp);

__rte_internal
int otx2_sec_idev_tx_cpt_qp_get(uint16_t port_id, struct otx2_cpt_qp **qp);

#endif /* _OTX2_SEC_IDEV_H_ */
