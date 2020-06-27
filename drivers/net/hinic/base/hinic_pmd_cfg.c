/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Huawei Technologies Co., Ltd
 */

#include "hinic_compat.h"
#include "hinic_pmd_hwdev.h"
#include "hinic_pmd_hwif.h"
#include "hinic_pmd_mgmt.h"
#include "hinic_pmd_eqs.h"
#include "hinic_pmd_cfg.h"
#include "hinic_pmd_mbox.h"

bool hinic_support_nic(struct hinic_hwdev *hwdev, struct nic_service_cap *cap)
{
	if (!IS_NIC_TYPE(hwdev))
		return false;

	if (cap)
		memcpy(cap, &hwdev->cfg_mgmt->svc_cap.nic_cap, sizeof(*cap));

	return true;
}

static void hinic_parse_shared_res_cap(struct service_cap *cap,
					struct hinic_dev_cap *dev_cap,
					__rte_unused enum func_type type)
{
	struct host_shared_resource_cap *shared_cap = &cap->shared_res_cap;

	shared_cap->host_pctxs = dev_cap->host_pctx_num;

	if (dev_cap->host_sf_en)
		cap->sf_en = true;
	else
		cap->sf_en = false;

	shared_cap->host_cctxs = dev_cap->host_ccxt_num;
	shared_cap->host_scqs = dev_cap->host_scq_num;
	shared_cap->host_srqs = dev_cap->host_srq_num;
	shared_cap->host_mpts = dev_cap->host_mpt_num;

	PMD_DRV_LOG(INFO, "Get share resource capability:");
	PMD_DRV_LOG(INFO, "host_pctxs: 0x%x, host_cctxs: 0x%x, host_scqs: 0x%x, host_srqs: 0x%x, host_mpts: 0x%x",
		    shared_cap->host_pctxs, shared_cap->host_cctxs,
		    shared_cap->host_scqs, shared_cap->host_srqs,
		    shared_cap->host_mpts);
}

static void hinic_parse_l2nic_res_cap(struct service_cap *cap,
				struct hinic_dev_cap *dev_cap,
				enum func_type type)
{
	struct nic_service_cap *nic_cap = &cap->nic_cap;

	if (type == TYPE_PF || type == TYPE_PPF) {
		nic_cap->max_sqs = dev_cap->nic_max_sq + 1;
		nic_cap->max_rqs = dev_cap->nic_max_rq + 1;
		nic_cap->vf_max_sqs = dev_cap->nic_vf_max_sq + 1;
		nic_cap->vf_max_rqs = dev_cap->nic_vf_max_rq + 1;
	} else {
		nic_cap->max_sqs = dev_cap->nic_max_sq;
		nic_cap->max_rqs = dev_cap->nic_max_rq;
		nic_cap->vf_max_sqs = 0;
		nic_cap->vf_max_rqs = 0;
	}

	if (dev_cap->nic_lro_en)
		nic_cap->lro_en = true;
	else
		nic_cap->lro_en = false;

	nic_cap->lro_sz = dev_cap->nic_lro_sz;
	nic_cap->tso_sz = dev_cap->nic_tso_sz;

	PMD_DRV_LOG(INFO, "Get l2nic resource capability:");
	PMD_DRV_LOG(INFO, "max_sqs: 0x%x, max_rqs: 0x%x, vf_max_sqs: 0x%x, vf_max_rqs: 0x%x",
		    nic_cap->max_sqs, nic_cap->max_rqs,
		    nic_cap->vf_max_sqs, nic_cap->vf_max_rqs);
}

u16 hinic_func_max_qnum(void *hwdev)
{
	struct hinic_hwdev *dev = hwdev;

	return dev->cfg_mgmt->svc_cap.max_sqs;
}

int init_cfg_mgmt(struct hinic_hwdev *hwdev)
{
	struct cfg_mgmt_info *cfg_mgmt;

	cfg_mgmt = kzalloc(sizeof(*cfg_mgmt), GFP_KERNEL);
	if (!cfg_mgmt)
		return -ENOMEM;

	hwdev->cfg_mgmt = cfg_mgmt;
	cfg_mgmt->hwdev = hwdev;

	return 0;
}

void free_cfg_mgmt(struct hinic_hwdev *hwdev)
{
	kfree(hwdev->cfg_mgmt);
	hwdev->cfg_mgmt = NULL;
}

static void hinic_parse_pub_res_cap(struct service_cap *cap,
			      struct hinic_dev_cap *dev_cap,
			      enum func_type type)
{
	cap->host_id = dev_cap->host_id;
	cap->ep_id = dev_cap->ep_id;
	cap->max_cos_id = dev_cap->max_cos_id;
	cap->er_id = dev_cap->er_id;
	cap->port_id = dev_cap->port_id;

	if (type == TYPE_PF || type == TYPE_PPF) {
		cap->max_vf = dev_cap->max_vf;
		cap->pf_num = dev_cap->pf_num;
		cap->pf_id_start = dev_cap->pf_id_start;
		cap->vf_num = dev_cap->vf_num;
		cap->vf_id_start = dev_cap->vf_id_start;
		cap->max_sqs = dev_cap->nic_max_sq + 1;
		cap->max_rqs = dev_cap->nic_max_rq + 1;
	} else {
		cap->max_vf = 0;
		cap->max_sqs = dev_cap->nic_max_sq;
		cap->max_rqs = dev_cap->nic_max_rq;
	}

	cap->chip_svc_type = dev_cap->svc_cap_en;
	cap->host_total_function = dev_cap->host_total_func;
	cap->host_oq_id_mask_val = dev_cap->host_oq_id_mask_val;

	PMD_DRV_LOG(INFO, "Get public resource capability:");
	PMD_DRV_LOG(INFO, "host_id: 0x%x, ep_id: 0x%x, intr_type: 0x%x, max_cos_id: 0x%x, er_id: 0x%x, port_id: 0x%x",
		    cap->host_id, cap->ep_id, cap->intr_chip_en,
		    cap->max_cos_id, cap->er_id, cap->port_id);
	PMD_DRV_LOG(INFO, "host_total_function: 0x%x, host_oq_id_mask_val: 0x%x, max_vf: 0x%x",
		    cap->host_total_function, cap->host_oq_id_mask_val,
		    cap->max_vf);
	PMD_DRV_LOG(INFO, "chip_svc_type: 0x%x", cap->chip_svc_type);
	PMD_DRV_LOG(INFO, "pf_num: 0x%x, pf_id_start: 0x%x, vf_num: 0x%x, vf_id_start: 0x%x",
		    cap->pf_num, cap->pf_id_start,
		    cap->vf_num, cap->vf_id_start);
}

static void parse_dev_cap(struct hinic_hwdev *dev,
			  struct hinic_dev_cap *dev_cap,
			  enum func_type type)
{
	struct service_cap *cap = &dev->cfg_mgmt->svc_cap;

	/* Public resource */
	hinic_parse_pub_res_cap(cap, dev_cap, type);

	/* PPF managed dynamic resource */
	if (type == TYPE_PPF)
		hinic_parse_shared_res_cap(cap, dev_cap, type);

	/* L2 NIC resource */
	if (IS_NIC_TYPE(dev))
		hinic_parse_l2nic_res_cap(cap, dev_cap, type);
}

static int get_cap_from_fw(struct hinic_hwdev *dev, enum func_type type)
{
	int err;
	u16 in_len, out_len;
	struct hinic_dev_cap dev_cap;

	memset(&dev_cap, 0, sizeof(dev_cap));
	in_len = sizeof(dev_cap);
	out_len = in_len;
	dev_cap.mgmt_msg_head.resp_aeq_num = HINIC_AEQ1;
	err = hinic_msg_to_mgmt_sync(dev, HINIC_MOD_CFGM, HINIC_CFG_NIC_CAP,
				     &dev_cap, in_len, &dev_cap, &out_len, 0);
	if (err || dev_cap.mgmt_msg_head.status || !out_len) {
		PMD_DRV_LOG(ERR, "Get capability from FW failed, err: %d, status: %d, out_len: %d",
			err, dev_cap.mgmt_msg_head.status, out_len);
		return -EIO;
	}

	parse_dev_cap(dev, &dev_cap, type);
	return 0;
}

static int get_cap_from_pf(struct hinic_hwdev *dev, enum func_type type)
{
	int err;
	u16 in_len, out_len;
	struct hinic_dev_cap dev_cap;

	memset(&dev_cap, 0, sizeof(dev_cap));
	in_len = sizeof(dev_cap);
	out_len = in_len;
	err = hinic_mbox_to_pf(dev, HINIC_MOD_CFGM, HINIC_CFG_MBOX_CAP,
			       &dev_cap, in_len, &dev_cap, &out_len,
			       CFG_MAX_CMD_TIMEOUT);
	if (err || dev_cap.mgmt_msg_head.status || !out_len) {
		PMD_DRV_LOG(ERR, "Get capability from PF failed, err: %d, status: %d, out_len: %d",
				err, dev_cap.mgmt_msg_head.status, out_len);
		return -EIO;
	}

	parse_dev_cap(dev, &dev_cap, type);
	return 0;
}

static int get_dev_cap(struct hinic_hwdev *dev)
{
	int err;
	enum func_type type = HINIC_FUNC_TYPE(dev);

	switch (type) {
	case TYPE_PF:
	case TYPE_PPF:
		err = get_cap_from_fw(dev, type);
		if (err) {
			PMD_DRV_LOG(ERR, "Get PF/PPF capability failed");
			return err;
		}
		break;
	case TYPE_VF:
		err = get_cap_from_pf(dev, type);
		if (err) {
			PMD_DRV_LOG(ERR, "Get VF capability failed, err: %d",
					err);
			return err;
		}
		break;
	default:
		PMD_DRV_LOG(ERR, "Unsupported PCI function type");
		return -EINVAL;
	}

	return 0;
}

int hinic_init_capability(struct hinic_hwdev *hwdev)
{
	return get_dev_cap(hwdev);
}
