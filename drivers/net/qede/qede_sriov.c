/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2020 Marvell.
 * All rights reserved.
 * www.marvell.com
 */

#include "qede_sriov.h"

static void qed_sriov_enable_qid_config(struct ecore_hwfn *hwfn,
					u16 vfid,
					struct ecore_iov_vf_init_params *params)
{
	u16 num_pf_l2_queues, base, i;

	/* Since we have an equal resource distribution per-VF, and we assume
	 * PF has acquired its first queues, we start setting sequentially from
	 * there.
	 */
	num_pf_l2_queues = (u16)FEAT_NUM(hwfn, ECORE_PF_L2_QUE);

	base = num_pf_l2_queues + vfid * params->num_queues;
	params->rel_vf_id = vfid;

	for (i = 0; i < params->num_queues; i++) {
		params->req_rx_queue[i] = base + i;
		params->req_tx_queue[i] = base + i;
	}

	/* PF uses indices 0 for itself; Set vport/RSS afterwards */
	params->vport_id = vfid + 1;
	params->rss_eng_id = vfid + 1;
}

static void qed_sriov_enable(struct ecore_dev *edev, int num)
{
	struct ecore_iov_vf_init_params params;
	struct ecore_hwfn *p_hwfn;
	struct ecore_ptt *p_ptt;
	int i, j, rc;

	if ((u32)num >= RESC_NUM(&edev->hwfns[0], ECORE_VPORT)) {
		DP_NOTICE(edev, false, "Can start at most %d VFs\n",
			  RESC_NUM(&edev->hwfns[0], ECORE_VPORT) - 1);
		return;
	}

	OSAL_MEMSET(&params, 0, sizeof(struct ecore_iov_vf_init_params));

	for_each_hwfn(edev, j) {
		int feat_num;

		p_hwfn = &edev->hwfns[j];
		p_ptt = ecore_ptt_acquire(p_hwfn);
		feat_num = FEAT_NUM(p_hwfn, ECORE_VF_L2_QUE) / num;

		params.num_queues = OSAL_MIN_T(int, feat_num, 16);

		for (i = 0; i < num; i++) {
			if (!ecore_iov_is_valid_vfid(p_hwfn, i, false, true))
				continue;

			qed_sriov_enable_qid_config(p_hwfn, i, &params);

			rc = ecore_iov_init_hw_for_vf(p_hwfn, p_ptt, &params);
			if (rc) {
				DP_ERR(edev, "Failed to enable VF[%d]\n", i);
				ecore_ptt_release(p_hwfn, p_ptt);
				return;
			}
		}

		ecore_ptt_release(p_hwfn, p_ptt);
	}
}

void qed_sriov_configure(struct ecore_dev *edev, int num_vfs_param)
{
	if (!IS_ECORE_SRIOV(edev)) {
		DP_VERBOSE(edev, ECORE_MSG_IOV, "SR-IOV is not supported\n");
		return;
	}

	if (num_vfs_param)
		qed_sriov_enable(edev, num_vfs_param);
}
