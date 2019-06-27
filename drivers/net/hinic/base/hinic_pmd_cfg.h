/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Huawei Technologies Co., Ltd
 */

#ifndef _HINIC_PMD_CFG_H_
#define _HINIC_PMD_CFG_H_

#define CFG_MAX_CMD_TIMEOUT     8000 /* ms */

#define IS_NIC_TYPE(dev) \
	((dev)->cfg_mgmt->svc_cap.chip_svc_type & CFG_SVC_NIC_BIT0)

struct host_shared_resource_cap {
	u32 host_pctxs; /* Parent Context max 1M, IOE and FCoE max 8K flows */
	u32 host_cctxs; /* Child Context: max 8K */
	u32 host_scqs;  /* shared CQ, chip interface module uses 1 SCQ
			 * TOE/IOE/FCoE each uses 1 SCQ
			 * RoCE/IWARP uses multiple SCQs
			 * So 6 SCQ least
			 */
	u32 host_srqs; /* SRQ number: 256K */
	u32 host_mpts; /* MR number:1M */
};

struct nic_service_cap {
	/* PF resources */
	u16 max_sqs;
	u16 max_rqs;

	/* VF resources, VF obtain them through the MailBox mechanism from
	 * corresponding PF
	 */
	u16 vf_max_sqs;
	u16 vf_max_rqs;

	bool lro_en;    /* LRO feature enable bit */
	u8 lro_sz;      /* LRO context space: n*16B */
	u8 tso_sz;      /* TSO context space: n*16B */
};

/* service type relates define */
enum cfg_svc_type_en {
	CFG_SVC_NIC_BIT0    = (1 << 0),
};

/* device capability */
struct service_cap {
	enum cfg_svc_type_en chip_svc_type;	/* HW supported service type */

	/* Host global resources */
	u16 host_total_function;
	u8 host_oq_id_mask_val;
	u8 host_id;
	u8 ep_id;
	u8 intr_chip_en;
	u8 max_cos_id;	/* PF/VF's max cos id */
	u8 er_id;	/* PF/VF's ER */
	u8 port_id;	/* PF/VF's physical port */
	u8 max_vf;	/* max VF number that PF supported */
	bool sf_en;	/* stateful business status */
	u16 max_sqs;
	u16 max_rqs;

	u32 pf_num;
	u32 pf_id_start;
	u32 vf_num;
	u32 vf_id_start;

	struct host_shared_resource_cap shared_res_cap; /* shared capability */
	struct nic_service_cap      nic_cap;            /* NIC capability */
};

struct cfg_mgmt_info {
	struct hinic_hwdev *hwdev;
	struct service_cap  svc_cap;
};

struct hinic_dev_cap {
	struct hinic_mgmt_msg_head mgmt_msg_head;

	/* Public resource */
	u8 sf_svc_attr;
	u8 host_id;
	u8 sf_en_pf;
	u8 sf_en_vf;

	u8 ep_id;
	u8 intr_type;
	u8 max_cos_id;
	u8 er_id;
	u8 port_id;
	u8 max_vf;
	u16 svc_cap_en;
	u16 host_total_func;
	u8 host_oq_id_mask_val;
	u8 max_vf_cos_id;

	u32 max_conn_num;
	u16 max_stick2cache_num;
	u16 max_bfilter_start_addr;
	u16 bfilter_len;
	u16 hash_bucket_num;
	u8 cfg_file_ver;
	u8 net_port_mode;
	u8 valid_cos_bitmap;	/* every bit indicate cos is valid */
	u8 rsvd1;
	u32 pf_num;
	u32 pf_id_start;
	u32 vf_num;
	u32 vf_id_start;

	/* shared resource */
	u32 host_pctx_num;
	u8 host_sf_en;
	u8 rsvd2[3];
	u32 host_ccxt_num;
	u32 host_scq_num;
	u32 host_srq_num;
	u32 host_mpt_num;

	/* l2nic */
	u16 nic_max_sq;
	u16 nic_max_rq;
	u16 nic_vf_max_sq;
	u16 nic_vf_max_rq;
	u8 nic_lro_en;
	u8 nic_lro_sz;
	u8 nic_tso_sz;
	u8 rsvd3;

	u32 rsvd4[50];
};

/* Obtain service_cap.nic_cap.dev_nic_cap.max_sqs */
u16 hinic_func_max_qnum(void *hwdev);

int init_cfg_mgmt(struct hinic_hwdev *hwdev);

void free_cfg_mgmt(struct hinic_hwdev *hwdev);

int hinic_init_capability(struct hinic_hwdev *hwdev);

bool hinic_support_nic(struct hinic_hwdev *hwdev, struct nic_service_cap *cap);

#endif /* _HINIC_PMD_CFG_H_ */
