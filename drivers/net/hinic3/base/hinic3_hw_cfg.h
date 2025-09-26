/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Huawei Technologies Co., Ltd
 */

#ifndef _HINIC3_HW_CFG_H_
#define _HINIC3_HW_CFG_H_

#include "hinic3_hwdev.h"

/* Number of PFs and VFs. */
#define HOST_PF_NUM	   4
#define HOST_VF_NUM	   0
#define HOST_OQID_MASK_VAL 2

enum intr_type { INTR_TYPE_MSIX, INTR_TYPE_MSI, INTR_TYPE_INT, INTR_TYPE_NONE };

/* Service type relates define. */
enum cfg_svc_type_en { CFG_SVC_NIC_BIT0 = 1 };

struct nic_service_cap {
	uint16_t max_sqs;
	uint16_t max_rqs;
};

/* Device capability. */
struct service_cap {
	enum cfg_svc_type_en svc_type;	    /**< User input service type. */
	enum cfg_svc_type_en chip_svc_type; /**< HW supported service type. */

	uint8_t host_id;
	uint8_t ep_id;
	uint8_t er_id;   /**< PF/VF's ER. */
	uint8_t port_id; /**< PF/VF's physical port. */

	uint16_t host_total_function;
	uint8_t pf_num;
	uint8_t pf_id_start;
	uint16_t vf_num; /**< Max numbers of vf in current host. */
	uint16_t vf_id_start;

	uint8_t flexq_en;
	uint8_t cos_valid_bitmap;
	uint16_t max_vf; /**< Max VF number that PF supported. */

	struct nic_service_cap nic_cap; /**< NIC capability. */
};

struct cfg_mgmt_info {
	void *hwdev;
	struct service_cap svc_cap;
};

enum hinic3_cfg_cmd {
	HINIC3_CFG_CMD_GET_DEV_CAP = 0,
};

struct hinic3_cfg_cmd_dev_cap {
	uint8_t status;
	uint8_t version;
	uint8_t rsvd0[6];

	uint16_t func_id;
	uint16_t rsvd1;

	/* Public resource. */
	uint8_t host_id;
	uint8_t ep_id;
	uint8_t er_id;
	uint8_t port_id;

	uint16_t host_total_func;
	uint8_t host_pf_num;
	uint8_t pf_id_start;
	uint16_t host_vf_num;
	uint16_t vf_id_start;
	uint32_t rsvd_host;

	uint16_t svc_cap_en;
	uint16_t max_vf;
	uint8_t flexq_en;
	uint8_t valid_cos_bitmap;
	/* Reserved for func_valid_cos_bitmap. */
	uint16_t rsvd_cos;

	uint32_t rsvd[11];

	/* l2nic */
	uint16_t nic_max_sq_id;
	uint16_t nic_max_rq_id;
	uint32_t rsvd_nic[3];

	uint32_t rsvd_glb[60];
};

#define IS_NIC_TYPE(dev) \
	(((uint32_t)(dev)->cfg_mgmt->svc_cap.chip_svc_type) & CFG_SVC_NIC_BIT0)

int hinic3_init_cfg_mgmt(struct hinic3_hwdev *hwdev);
int hinic3_init_capability(struct hinic3_hwdev *hwdev);
void hinic3_deinit_cfg_mgmt(struct hinic3_hwdev *hwdev);

uint16_t hinic3_func_max_sqs(struct hinic3_hwdev *hwdev);
uint16_t hinic3_func_max_rqs(struct hinic3_hwdev *hwdev);

uint8_t hinic3_physical_port_id(struct hinic3_hwdev *hwdev);

int hinic3_cfg_mbx_vf_proc_msg(struct hinic3_hwdev *hwdev, void *pri_handle,
			       struct hinic3_handler_info *handler_info);

#endif /* _HINIC3_HW_CFG_H_ */
