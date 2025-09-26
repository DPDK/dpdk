/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Huawei Technologies Co., Ltd
 */

#include "hinic3_compat.h"
#include "hinic3_mbox.h"
#include "hinic3_mgmt.h"
#include "hinic3_hw_cfg.h"
#include "hinic3_hwif.h"
#include "hinic3_nic_cfg.h"

static void
parse_pub_res_cap(struct service_cap *cap,
		  struct hinic3_cfg_cmd_dev_cap *dev_cap, enum func_type type)
{
	cap->host_id = dev_cap->host_id;
	cap->ep_id = dev_cap->ep_id;
	cap->er_id = dev_cap->er_id;
	cap->port_id = dev_cap->port_id;

	cap->svc_type = dev_cap->svc_cap_en;
	cap->chip_svc_type = cap->svc_type;

	cap->cos_valid_bitmap = dev_cap->valid_cos_bitmap;
	cap->flexq_en = dev_cap->flexq_en;

	cap->host_total_function = dev_cap->host_total_func;
	cap->max_vf = 0;
	if (type == TYPE_PF || type == TYPE_PPF) {
		cap->max_vf = dev_cap->max_vf;
		cap->pf_num = dev_cap->host_pf_num;
		cap->pf_id_start = dev_cap->pf_id_start;
		cap->vf_num = dev_cap->host_vf_num;
		cap->vf_id_start = dev_cap->vf_id_start;
	}

	PMD_DRV_LOG(DEBUG, "Get public resource capability: ");
	PMD_DRV_LOG(DEBUG,
		    "host_id: 0x%x, ep_id: 0x%x, er_id: 0x%x, port_id: 0x%x",
		    cap->host_id, cap->ep_id, cap->er_id, cap->port_id);
	PMD_DRV_LOG(DEBUG, "host_total_function: 0x%x, max_vf: 0x%x",
		    cap->host_total_function, cap->max_vf);
	PMD_DRV_LOG(DEBUG,
		    "host_pf_num: 0x%x, pf_id_start: 0x%x, host_vf_num: 0x%x, vf_id_start: 0x%x",
		    cap->pf_num, cap->pf_id_start, cap->vf_num, cap->vf_id_start);
}

static void
parse_l2nic_res_cap(struct service_cap *cap,
		    struct hinic3_cfg_cmd_dev_cap *dev_cap)
{
	struct nic_service_cap *nic_cap = &cap->nic_cap;

	nic_cap->max_sqs = dev_cap->nic_max_sq_id + 1;
	nic_cap->max_rqs = dev_cap->nic_max_rq_id + 1;

	PMD_DRV_LOG(DEBUG,
		    "L2nic resource capbility, max_sqs: 0x%x, max_rqs: 0x%x",
		    nic_cap->max_sqs, nic_cap->max_rqs);
}

static void
parse_dev_cap(struct hinic3_hwdev *dev, struct hinic3_cfg_cmd_dev_cap *dev_cap,
	      enum func_type type)
{
	struct service_cap *cap = &dev->cfg_mgmt->svc_cap;

	parse_pub_res_cap(cap, dev_cap, type);

	if (IS_NIC_TYPE(dev))
		parse_l2nic_res_cap(cap, dev_cap);
}

static int
get_cap_from_fw(struct hinic3_hwdev *hwdev, enum func_type type)
{
	struct hinic3_cfg_cmd_dev_cap dev_cap;
	uint16_t out_len = sizeof(dev_cap);
	int err;

	memset(&dev_cap, 0, sizeof(dev_cap));
	dev_cap.func_id = hinic3_global_func_id(hwdev);
	err = hinic3_msg_to_mgmt_sync(hwdev, HINIC3_MOD_CFGM,
				      HINIC3_CFG_CMD_GET_DEV_CAP, &dev_cap,
				      sizeof(dev_cap), &dev_cap, &out_len);
	if (err || dev_cap.status || !out_len) {
		PMD_DRV_LOG(ERR,
			    "Get capability from FW failed, err: %d, status: 0x%x, out size: 0x%x",
			    err, dev_cap.status, out_len);
		return -EFAULT;
	}

	parse_dev_cap(hwdev, &dev_cap, type);
	return 0;
}

static int
get_dev_cap(struct hinic3_hwdev *hwdev)
{
	enum func_type type = HINIC3_FUNC_TYPE(hwdev);

	switch (type) {
	case TYPE_PF:
	case TYPE_PPF:
	case TYPE_VF:
		if (get_cap_from_fw(hwdev, type) != 0)
			return -EFAULT;
		break;
	default:
		PMD_DRV_LOG(ERR, "Unsupported PCIe function type: %d", type);
		return -EINVAL;
	}

	return 0;
}

int
hinic3_cfg_mbx_vf_proc_msg(struct hinic3_hwdev *hwdev, __rte_unused void *pri_handle,
			   struct hinic3_handler_info *handler_info)
{
	if (!hwdev)
		return -EINVAL;

	PMD_DRV_LOG(WARNING, "Unsupported cfg mbox vf event %d to process", handler_info->cmd);

	return 0;
}

int
hinic3_init_cfg_mgmt(struct hinic3_hwdev *hwdev)
{
	struct cfg_mgmt_info *cfg_mgmt = NULL;

	cfg_mgmt = rte_zmalloc("cfg_mgmt", sizeof(*cfg_mgmt),
			       HINIC3_MEM_ALLOC_ALIGN_MIN);
	if (!cfg_mgmt)
		return -ENOMEM;

	memset(cfg_mgmt, 0, sizeof(struct cfg_mgmt_info));
	hwdev->cfg_mgmt = cfg_mgmt;
	cfg_mgmt->hwdev = hwdev;

	return 0;
}

int
hinic3_init_capability(struct hinic3_hwdev *hwdev)
{
	return get_dev_cap(hwdev);
}

void
hinic3_deinit_cfg_mgmt(struct hinic3_hwdev *hwdev)
{
	rte_free(hwdev->cfg_mgmt);
	hwdev->cfg_mgmt = NULL;
}

uint16_t
hinic3_func_max_sqs(struct hinic3_hwdev *hwdev)
{
	if (!hwdev) {
		PMD_DRV_LOG(INFO, "Hwdev is NULL for getting max_sqs");
		return 0;
	}

	return hwdev->cfg_mgmt->svc_cap.nic_cap.max_sqs;
}

uint16_t
hinic3_func_max_rqs(struct hinic3_hwdev *hwdev)
{
	if (!hwdev) {
		PMD_DRV_LOG(INFO, "Hwdev is NULL for getting max_rqs");
		return 0;
	}

	return hwdev->cfg_mgmt->svc_cap.nic_cap.max_rqs;
}

uint8_t
hinic3_physical_port_id(struct hinic3_hwdev *hwdev)
{
	if (!hwdev) {
		PMD_DRV_LOG(INFO, "Hwdev is NULL for getting physical port id");
		return 0;
	}

	return hwdev->cfg_mgmt->svc_cap.port_id;
}
