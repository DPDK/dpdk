/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2020 Marvell.
 * All rights reserved.
 * www.marvell.com
 */

void qed_sriov_configure(struct ecore_dev *edev, int num_vfs_param);

enum qed_iov_wq_flag {
	QED_IOV_WQ_MSG_FLAG,
	QED_IOV_WQ_SET_UNICAST_FILTER_FLAG,
	QED_IOV_WQ_BULLETIN_UPDATE_FLAG,
	QED_IOV_WQ_STOP_WQ_FLAG,
	QED_IOV_WQ_FLR_FLAG,
	QED_IOV_WQ_TRUST_FLAG,
	QED_IOV_WQ_VF_FORCE_LINK_QUERY_FLAG,
	QED_IOV_WQ_DB_REC_HANDLER,
};

void qed_inform_vf_link_state(struct ecore_hwfn *hwfn);
int qed_schedule_iov(struct ecore_hwfn *p_hwfn, enum qed_iov_wq_flag flag);
void qed_iov_pf_task(void *arg);
