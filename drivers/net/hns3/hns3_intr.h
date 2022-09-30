/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2021 HiSilicon Limited.
 */

#ifndef HNS3_INTR_H
#define HNS3_INTR_H

#include <stdint.h>

#include "hns3_ethdev.h"

#define HNS3_PPP_MPF_ECC_ERR_INT0_EN		0xFFFFFFFF
#define HNS3_PPP_MPF_ECC_ERR_INT0_EN_MASK	0xFFFFFFFF
#define HNS3_PPP_MPF_ECC_ERR_INT1_EN		0xFFFFFFFF
#define HNS3_PPP_MPF_ECC_ERR_INT1_EN_MASK	0xFFFFFFFF
#define HNS3_PPP_PF_ERR_INT_EN			0x0003
#define HNS3_PPP_PF_ERR_INT_EN_MASK		0x0003
#define HNS3_PPP_MPF_ECC_ERR_INT2_EN		0x003F
#define HNS3_PPP_MPF_ECC_ERR_INT2_EN_MASK	0x003F
#define HNS3_PPP_MPF_ECC_ERR_INT3_EN		0x003F
#define HNS3_PPP_MPF_ECC_ERR_INT3_EN_MASK	0x003F

#define HNS3_MAC_COMMON_ERR_INT_EN		0x107FF
#define HNS3_MAC_COMMON_ERR_INT_EN_MASK		0x107FF
#define HNS3_MAC_TNL_INT_EN			GENMASK(9, 0)
#define HNS3_MAC_TNL_INT_EN_MASK		GENMASK(9, 0)
#define HNS3_MAC_TNL_INT_CLR			GENMASK(9, 0)

#define HNS3_IMP_TCM_ECC_ERR_INT_EN		0xFFFF0000
#define HNS3_IMP_TCM_ECC_ERR_INT_EN_MASK	0xFFFF0000
#define HNS3_IMP_ITCM4_ECC_ERR_INT_EN		0x300
#define HNS3_IMP_ITCM4_ECC_ERR_INT_EN_MASK	0x300
#define HNS3_IMP_RD_POISON_ERR_INT_EN		0x0100
#define HNS3_IMP_RD_POISON_ERR_INT_EN_MASK	0x0100

#define HNS3_CMDQ_NIC_ECC_ERR_INT_EN		0xFFFF
#define HNS3_CMDQ_NIC_ECC_ERR_INT_EN_MASK	0xFFFF

#define HNS3_TQP_ECC_ERR_INT_EN			0x0FFF
#define HNS3_TQP_ECC_ERR_INT_EN_MASK		0x0FFF

#define HNS3_MSIX_SRAM_ECC_ERR_INT_EN		0x0F000000
#define HNS3_MSIX_SRAM_ECC_ERR_INT_EN_MASK	0x0F000000

#define HNS3_PPU_MPF_ABNORMAL_INT0_EN		GENMASK(31, 0)
#define HNS3_PPU_MPF_ABNORMAL_INT0_EN_MASK	GENMASK(31, 0)
#define HNS3_PPU_MPF_ABNORMAL_INT1_EN		GENMASK(31, 0)
#define HNS3_PPU_MPF_ABNORMAL_INT1_EN_MASK	GENMASK(31, 0)
#define HNS3_PPU_MPF_ABNORMAL_INT2_EN		0x3FFF3FFF
#define HNS3_PPU_MPF_ABNORMAL_INT2_EN_MASK	0x3FFF3FFF
#define HNS3_PPU_MPF_ABNORMAL_INT2_EN2		0xB
#define HNS3_PPU_MPF_ABNORMAL_INT2_EN2_MASK	0xB
#define HNS3_PPU_MPF_ABNORMAL_INT3_EN		GENMASK(7, 0)
#define HNS3_PPU_MPF_ABNORMAL_INT3_EN_MASK	GENMASK(23, 16)
#define HNS3_PPU_PF_ABNORMAL_INT_EN		GENMASK(5, 0)
#define HNS3_PPU_PF_ABNORMAL_INT_EN_MASK	GENMASK(5, 0)

#define HNS3_SSU_1BIT_ECC_ERR_INT_EN		GENMASK(31, 0)
#define HNS3_SSU_1BIT_ECC_ERR_INT_EN_MASK	GENMASK(31, 0)
#define HNS3_SSU_MULTI_BIT_ECC_ERR_INT_EN	GENMASK(31, 0)
#define HNS3_SSU_MULTI_BIT_ECC_ERR_INT_EN_MASK	GENMASK(31, 0)
#define HNS3_SSU_BIT32_ECC_ERR_INT_EN		0x0101
#define HNS3_SSU_BIT32_ECC_ERR_INT_EN_MASK	0x0101
#define HNS3_SSU_COMMON_INT_EN			GENMASK(9, 0)
#define HNS3_SSU_COMMON_INT_EN_MASK		GENMASK(9, 0)
#define HNS3_SSU_PORT_BASED_ERR_INT_EN		0x0BFF
#define HNS3_SSU_PORT_BASED_ERR_INT_EN_MASK	0x0BFF0000
#define HNS3_SSU_FIFO_OVERFLOW_ERR_INT_EN	GENMASK(23, 0)
#define HNS3_SSU_FIFO_OVERFLOW_ERR_INT_EN_MASK	GENMASK(23, 0)

#define HNS3_IGU_ERR_INT_ENABLE			0x0000066F
#define HNS3_IGU_ERR_INT_DISABLE		0x00000660
#define HNS3_IGU_ERR_INT_EN_MASK		0x000F
#define HNS3_IGU_TNL_ERR_INT_EN			0x0002AABF
#define HNS3_IGU_TNL_ERR_INT_EN_MASK		0x003F

#define HNS3_NCSI_ERR_INT_EN			0x3

#define HNS3_TM_SCH_ECC_ERR_INT_EN		0x3
#define HNS3_TM_QCN_ERR_INT_TYPE		0x29
#define HNS3_TM_QCN_FIFO_INT_EN			0xFFFF00
#define HNS3_TM_QCN_MEM_ERR_INT_EN		0xFFFFFF

#define HNS3_RESET_PROCESS_MS			200

#define HNS3_DESC_DATA_MAX			8
#define HNS3_REG_NUM_MAX			256
#define HNS3_DESC_NO_DATA_LEN			8
#define HNS3_DESC_DATA_UNIT_SIZE		4

enum hns3_mod_name_list {
	MODULE_NONE,
	MODULE_BIOS_COMMON,
	MODULE_GE,
	MODULE_IGU_EGU,
	MODULE_LGE,
	MODULE_NCSI,
	MODULE_PPP,
	MODULE_QCN,
	MODULE_RCB_RX,
	MODULE_RTC,
	MODULE_SSU,
	MODULE_TM,
	MODULE_RCB_TX,
	MODULE_TXDMA,
	MODULE_MASTER,
	MODULE_ROH_MAC,
};

enum hns3_err_type_list {
	NONE_ERROR,
	FIFO_ERROR,
	MEMORY_ERROR,
	POISION_ERROR,
	MSIX_ECC_ERROR,
	TQP_INT_ECC_ERROR,
	PF_ABNORMAL_INT_ERROR,
	MPF_ABNORMAL_INT_ERROR,
	COMMON_ERROR,
	PORT_ERROR,
	ETS_ERROR,
	NCSI_ERROR,
	GLB_ERROR,
};

struct hns3_hw_mod_name {
	enum hns3_mod_name_list module_name;
	const char *msg;
};

struct hns3_hw_err_type {
	enum hns3_err_type_list error_type;
	const char *msg;
};

struct hns3_sum_err_info {
	uint8_t reset_type; /* the total reset type */
	uint8_t mod_num; /* the modules num encounter error */
	uint8_t rsv[2];
};

struct hns3_mod_err_info {
	uint8_t mod_id; /* the error module id */
	uint8_t err_num; /* the errors num in module */
	uint8_t rsv[2];
};

struct hns3_type_reg_err_info {
	uint8_t type_id; /* the type id of error */
	uint8_t reg_num; /* the related registers num of this error */
	uint8_t rsv[2];
	uint32_t reg[HNS3_REG_NUM_MAX]; /* the registers value */
};

struct hns3_hw_blk {
	const char *name;
	int (*enable_err_intr)(struct hns3_adapter *hns, bool en);
};

struct hns3_hw_error {
	uint32_t int_msk;
	const char *msg;
	enum hns3_reset_level reset_level;
};

struct hns3_hw_error_desc {
	uint8_t desc_offset;
	uint8_t data_offset;
	const char *msg;
	const struct hns3_hw_error *hw_err;
};

int hns3_enable_hw_error_intr(struct hns3_adapter *hns, bool en);
void hns3_handle_msix_error(struct hns3_adapter *hns, uint64_t *levels);
void hns3_handle_ras_error(struct hns3_adapter *hns, uint64_t *levels);
void hns3_config_mac_tnl_int(struct hns3_hw *hw, bool en);
void hns3_handle_error(struct hns3_adapter *hns);

void hns3_intr_unregister(const struct rte_intr_handle *hdl,
			  rte_intr_callback_fn cb_fn, void *cb_arg);
void hns3_notify_reset_ready(struct hns3_hw *hw, bool enable);
int hns3_reset_init(struct hns3_hw *hw);
void hns3_wait_callback(void *param);
void hns3_schedule_reset(struct hns3_adapter *hns);
void hns3_schedule_delayed_reset(struct hns3_adapter *hns);
int hns3_reset_req_hw_reset(struct hns3_adapter *hns);
int hns3_reset_process(struct hns3_adapter *hns,
		       enum hns3_reset_level new_level);
void hns3_reset_abort(struct hns3_adapter *hns);
void hns3_start_report_lse(struct rte_eth_dev *dev);
void hns3_stop_report_lse(struct rte_eth_dev *dev);

#endif /* HNS3_INTR_H */
