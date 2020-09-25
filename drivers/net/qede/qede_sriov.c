/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2020 Marvell.
 * All rights reserved.
 * www.marvell.com
 */

#include <rte_alarm.h>

#include "base/bcm_osal.h"
#include "base/ecore.h"
#include "base/ecore_sriov.h"
#include "base/ecore_mcp.h"
#include "base/ecore_vf.h"

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

static void qed_handle_vf_msg(struct ecore_hwfn *hwfn)
{
	u64 events[ECORE_VF_ARRAY_LENGTH];
	struct ecore_ptt *ptt;
	int i;

	ptt = ecore_ptt_acquire(hwfn);
	if (!ptt) {
		DP_NOTICE(hwfn, true, "PTT acquire failed\n");
		qed_schedule_iov(hwfn, QED_IOV_WQ_MSG_FLAG);
		return;
	}

	ecore_iov_pf_get_pending_events(hwfn, events);

	ecore_for_each_vf(hwfn, i) {
		/* Skip VFs with no pending messages */
		if (!ECORE_VF_ARRAY_GET_VFID(events, i))
			continue;

		DP_VERBOSE(hwfn, ECORE_MSG_IOV,
			   "Handling VF message from VF 0x%02x [Abs 0x%02x]\n",
			   i, hwfn->p_dev->p_iov_info->first_vf_in_pf + i);

		/* Copy VF's message to PF's request buffer for that VF */
		if (ecore_iov_copy_vf_msg(hwfn, ptt, i))
			continue;

		ecore_iov_process_mbx_req(hwfn, ptt, i);
	}

	ecore_ptt_release(hwfn, ptt);
}

static void qed_handle_bulletin_post(struct ecore_hwfn *hwfn)
{
	struct ecore_ptt *ptt;
	int i;

	ptt = ecore_ptt_acquire(hwfn);
	if (!ptt) {
		DP_NOTICE(hwfn, true, "PTT acquire failed\n");
		qed_schedule_iov(hwfn, QED_IOV_WQ_BULLETIN_UPDATE_FLAG);
		return;
	}

	/* TODO - at the moment update bulletin board of all VFs.
	 * if this proves to costly, we can mark VFs that need their
	 * bulletins updated.
	 */
	ecore_for_each_vf(hwfn, i)
		ecore_iov_post_vf_bulletin(hwfn, i, ptt);

	ecore_ptt_release(hwfn, ptt);
}

void qed_iov_pf_task(void *arg)
{
	struct ecore_hwfn *p_hwfn = arg;
	int rc;

	if (OSAL_GET_BIT(QED_IOV_WQ_MSG_FLAG, &p_hwfn->iov_task_flags)) {
		OSAL_CLEAR_BIT(QED_IOV_WQ_MSG_FLAG, &p_hwfn->iov_task_flags);
		qed_handle_vf_msg(p_hwfn);
	}

	if (OSAL_GET_BIT(QED_IOV_WQ_BULLETIN_UPDATE_FLAG,
			 &p_hwfn->iov_task_flags)) {
		OSAL_CLEAR_BIT(QED_IOV_WQ_BULLETIN_UPDATE_FLAG,
			       &p_hwfn->iov_task_flags);
		qed_handle_bulletin_post(p_hwfn);
	}

	if (OSAL_GET_BIT(QED_IOV_WQ_FLR_FLAG, &p_hwfn->iov_task_flags)) {
		struct ecore_ptt *p_ptt = ecore_ptt_acquire(p_hwfn);

		OSAL_CLEAR_BIT(QED_IOV_WQ_FLR_FLAG, &p_hwfn->iov_task_flags);

		if (!p_ptt) {
			qed_schedule_iov(p_hwfn, QED_IOV_WQ_FLR_FLAG);
			return;
		}

		rc = ecore_iov_vf_flr_cleanup(p_hwfn, p_ptt);
		if (rc)
			qed_schedule_iov(p_hwfn, QED_IOV_WQ_FLR_FLAG);

		ecore_ptt_release(p_hwfn, p_ptt);
	}
}

int qed_schedule_iov(struct ecore_hwfn *p_hwfn, enum qed_iov_wq_flag flag)
{
	DP_VERBOSE(p_hwfn, ECORE_MSG_IOV, "Scheduling iov task [Flag: %d]\n",
		   flag);

	OSAL_SET_BIT(flag, &p_hwfn->iov_task_flags);
	return rte_eal_alarm_set(1, qed_iov_pf_task, p_hwfn);
}

void qed_inform_vf_link_state(struct ecore_hwfn *hwfn)
{
	struct ecore_hwfn *lead_hwfn = ECORE_LEADING_HWFN(hwfn->p_dev);
	struct ecore_mcp_link_capabilities caps;
	struct ecore_mcp_link_params params;
	struct ecore_mcp_link_state link;
	int i;

	if (!hwfn->pf_iov_info)
		return;

	rte_memcpy(&params, ecore_mcp_get_link_params(lead_hwfn),
		   sizeof(params));
	rte_memcpy(&link, ecore_mcp_get_link_state(lead_hwfn), sizeof(link));
	rte_memcpy(&caps, ecore_mcp_get_link_capabilities(lead_hwfn),
		   sizeof(caps));

	/* Update bulletin of all future possible VFs with link configuration */
	for (i = 0; i < hwfn->p_dev->p_iov_info->total_vfs; i++) {
		ecore_iov_set_link(hwfn, i,
				   &params, &link, &caps);
	}

	qed_schedule_iov(hwfn, QED_IOV_WQ_BULLETIN_UPDATE_FLAG);
}
