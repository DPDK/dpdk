/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2020 Marvell International Ltd.
 */

#include <rte_cryptodev.h>
#include <rte_eventdev.h>

#include "otx2_cryptodev_hw_access.h"
#include "otx2_cryptodev_qp.h"
#include "otx2_cryptodev_mbox.h"
#include "otx2_evdev.h"

int
otx2_ca_caps_get(const struct rte_eventdev *dev,
		const struct rte_cryptodev *cdev, uint32_t *caps)
{
	RTE_SET_USED(dev);
	RTE_SET_USED(cdev);

	*caps = RTE_EVENT_CRYPTO_ADAPTER_CAP_INTERNAL_PORT_QP_EV_BIND |
		RTE_EVENT_CRYPTO_ADAPTER_CAP_INTERNAL_PORT_OP_NEW |
		RTE_EVENT_CRYPTO_ADAPTER_CAP_INTERNAL_PORT_OP_FWD;

	return 0;
}

int
otx2_ca_qp_add(const struct rte_eventdev *dev, const struct rte_cryptodev *cdev,
		int32_t queue_pair_id, const struct rte_event *event)
{
	struct otx2_sso_evdev *sso_evdev = sso_pmd_priv(dev);
	union otx2_cpt_af_lf_ctl2 af_lf_ctl2;
	struct otx2_cpt_qp *qp;
	int ret;

	qp = cdev->data->queue_pairs[queue_pair_id];

	qp->ca_enable = 1;
	rte_memcpy(&qp->ev, event, sizeof(struct rte_event));

	ret = otx2_cpt_af_reg_read(cdev, OTX2_CPT_AF_LF_CTL2(qp->id),
			qp->blkaddr, &af_lf_ctl2.u);
	if (ret)
		return ret;

	af_lf_ctl2.s.sso_pf_func = otx2_sso_pf_func_get();
	ret = otx2_cpt_af_reg_write(cdev, OTX2_CPT_AF_LF_CTL2(qp->id),
			qp->blkaddr, af_lf_ctl2.u);
	if (ret)
		return ret;

	sso_evdev->rx_offloads |= NIX_RX_OFFLOAD_SECURITY_F;
	sso_fastpath_fns_set((struct rte_eventdev *)(uintptr_t)dev);

	return 0;
}

int
otx2_ca_qp_del(const struct rte_eventdev *dev, const struct rte_cryptodev *cdev,
		int32_t queue_pair_id)
{
	union otx2_cpt_af_lf_ctl2 af_lf_ctl2;
	struct otx2_cpt_qp *qp;
	int ret;

	RTE_SET_USED(dev);

	qp = cdev->data->queue_pairs[queue_pair_id];
	qp->ca_enable = 0;
	memset(&qp->ev, 0, sizeof(struct rte_event));

	ret = otx2_cpt_af_reg_read(cdev, OTX2_CPT_AF_LF_CTL2(qp->id),
			qp->blkaddr, &af_lf_ctl2.u);
	if (ret)
		return ret;

	af_lf_ctl2.s.sso_pf_func = 0;
	ret = otx2_cpt_af_reg_write(cdev, OTX2_CPT_AF_LF_CTL2(qp->id),
			qp->blkaddr, af_lf_ctl2.u);

	return ret;
}
