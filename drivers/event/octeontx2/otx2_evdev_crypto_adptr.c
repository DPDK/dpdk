/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2020-2021 Marvell.
 */

#include <rte_cryptodev.h>
#include <rte_eventdev.h>

#include "otx2_cryptodev.h"
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
		RTE_EVENT_CRYPTO_ADAPTER_CAP_INTERNAL_PORT_OP_NEW;

	return 0;
}

static int
otx2_ca_qp_sso_link(const struct rte_cryptodev *cdev, struct otx2_cpt_qp *qp,
		    uint16_t sso_pf_func)
{
	union otx2_cpt_af_lf_ctl2 af_lf_ctl2;
	int ret;

	ret = otx2_cpt_af_reg_read(cdev, OTX2_CPT_AF_LF_CTL2(qp->id),
				   &af_lf_ctl2.u);
	if (ret)
		return ret;

	af_lf_ctl2.s.sso_pf_func = sso_pf_func;
	ret = otx2_cpt_af_reg_write(cdev, OTX2_CPT_AF_LF_CTL2(qp->id),
				    af_lf_ctl2.u);
	return ret;
}

static void
otx2_ca_qp_init(struct otx2_cpt_qp *qp, const struct rte_event *event)
{
	if (event) {
		qp->qp_ev_bind = 1;
		rte_memcpy(&qp->ev, event, sizeof(struct rte_event));
	} else {
		qp->qp_ev_bind = 0;
	}
	qp->ca_enable = 1;
}

int
otx2_ca_qp_add(const struct rte_eventdev *dev, const struct rte_cryptodev *cdev,
		int32_t queue_pair_id, const struct rte_event *event)
{
	struct otx2_sso_evdev *sso_evdev = sso_pmd_priv(dev);
	struct otx2_cpt_vf *vf = cdev->data->dev_private;
	uint16_t sso_pf_func = otx2_sso_pf_func_get();
	struct otx2_cpt_qp *qp;
	uint8_t qp_id;
	int ret;

	if (queue_pair_id == -1) {
		for (qp_id = 0; qp_id < vf->nb_queues; qp_id++) {
			qp = cdev->data->queue_pairs[qp_id];
			ret = otx2_ca_qp_sso_link(cdev, qp, sso_pf_func);
			if (ret) {
				uint8_t qp_tmp;
				for (qp_tmp = 0; qp_tmp < qp_id; qp_tmp++)
					otx2_ca_qp_del(dev, cdev, qp_tmp);
				return ret;
			}
			otx2_ca_qp_init(qp, event);
		}
	} else {
		qp = cdev->data->queue_pairs[queue_pair_id];
		ret = otx2_ca_qp_sso_link(cdev, qp, sso_pf_func);
		if (ret)
			return ret;
		otx2_ca_qp_init(qp, event);
	}

	sso_evdev->rx_offloads |= NIX_RX_OFFLOAD_SECURITY_F;
	sso_fastpath_fns_set((struct rte_eventdev *)(uintptr_t)dev);

	/* Update crypto adapter xae count */
	if (queue_pair_id == -1)
		sso_evdev->adptr_xae_cnt +=
			vf->nb_queues * OTX2_CPT_DEFAULT_CMD_QLEN;
	else
		sso_evdev->adptr_xae_cnt += OTX2_CPT_DEFAULT_CMD_QLEN;
	sso_xae_reconfigure((struct rte_eventdev *)(uintptr_t)dev);

	return 0;
}

int
otx2_ca_qp_del(const struct rte_eventdev *dev, const struct rte_cryptodev *cdev,
		int32_t queue_pair_id)
{
	struct otx2_cpt_vf *vf = cdev->data->dev_private;
	struct otx2_cpt_qp *qp;
	uint8_t qp_id;
	int ret;

	RTE_SET_USED(dev);

	ret = 0;
	if (queue_pair_id == -1) {
		for (qp_id = 0; qp_id < vf->nb_queues; qp_id++) {
			qp = cdev->data->queue_pairs[qp_id];
			ret = otx2_ca_qp_sso_link(cdev, qp, 0);
			if (ret)
				return ret;
			qp->ca_enable = 0;
		}
	} else {
		qp = cdev->data->queue_pairs[queue_pair_id];
		ret = otx2_ca_qp_sso_link(cdev, qp, 0);
		if (ret)
			return ret;
		qp->ca_enable = 0;
	}

	return 0;
}
