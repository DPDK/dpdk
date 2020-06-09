/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2019 Hisilicon Limited.
 */

#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <inttypes.h>
#include <unistd.h>
#include <rte_bus_pci.h>
#include <rte_byteorder.h>
#include <rte_common.h>
#include <rte_dev.h>
#include <rte_eal.h>
#include <rte_ether.h>
#include <rte_ethdev_driver.h>
#include <rte_ethdev_pci.h>
#include <rte_io.h>
#include <rte_pci.h>

#include "hns3_ethdev.h"
#include "hns3_logs.h"
#include "hns3_rxtx.h"
#include "hns3_regs.h"

#define MAX_SEPARATE_NUM	4
#define SEPARATOR_VALUE		0xFFFFFFFF
#define REG_NUM_PER_LINE	4
#define REG_LEN_PER_LINE	(REG_NUM_PER_LINE * sizeof(uint32_t))

static const uint32_t cmdq_reg_addrs[] = {HNS3_CMDQ_TX_ADDR_L_REG,
					  HNS3_CMDQ_TX_ADDR_H_REG,
					  HNS3_CMDQ_TX_DEPTH_REG,
					  HNS3_CMDQ_TX_TAIL_REG,
					  HNS3_CMDQ_TX_HEAD_REG,
					  HNS3_CMDQ_RX_ADDR_L_REG,
					  HNS3_CMDQ_RX_ADDR_H_REG,
					  HNS3_CMDQ_RX_DEPTH_REG,
					  HNS3_CMDQ_RX_TAIL_REG,
					  HNS3_CMDQ_RX_HEAD_REG,
					  HNS3_VECTOR0_CMDQ_SRC_REG,
					  HNS3_CMDQ_INTR_STS_REG,
					  HNS3_CMDQ_INTR_EN_REG,
					  HNS3_CMDQ_INTR_GEN_REG};

static const uint32_t common_reg_addrs[] = {HNS3_MISC_VECTOR_REG_BASE,
					    HNS3_VECTOR0_OTER_EN_REG,
					    HNS3_MISC_RESET_STS_REG,
					    HNS3_VECTOR0_OTHER_INT_STS_REG,
					    HNS3_GLOBAL_RESET_REG,
					    HNS3_FUN_RST_ING,
					    HNS3_GRO_EN_REG};

static const uint32_t common_vf_reg_addrs[] = {HNS3_MISC_VECTOR_REG_BASE,
					       HNS3_FUN_RST_ING,
					       HNS3_GRO_EN_REG};

static const uint32_t ring_reg_addrs[] = {HNS3_RING_RX_BASEADDR_L_REG,
					  HNS3_RING_RX_BASEADDR_H_REG,
					  HNS3_RING_RX_BD_NUM_REG,
					  HNS3_RING_RX_BD_LEN_REG,
					  HNS3_RING_RX_MERGE_EN_REG,
					  HNS3_RING_RX_TAIL_REG,
					  HNS3_RING_RX_HEAD_REG,
					  HNS3_RING_RX_FBDNUM_REG,
					  HNS3_RING_RX_OFFSET_REG,
					  HNS3_RING_RX_FBD_OFFSET_REG,
					  HNS3_RING_RX_STASH_REG,
					  HNS3_RING_RX_BD_ERR_REG,
					  HNS3_RING_TX_BASEADDR_L_REG,
					  HNS3_RING_TX_BASEADDR_H_REG,
					  HNS3_RING_TX_BD_NUM_REG,
					  HNS3_RING_TX_PRIORITY_REG,
					  HNS3_RING_TX_TC_REG,
					  HNS3_RING_TX_MERGE_EN_REG,
					  HNS3_RING_TX_TAIL_REG,
					  HNS3_RING_TX_HEAD_REG,
					  HNS3_RING_TX_FBDNUM_REG,
					  HNS3_RING_TX_OFFSET_REG,
					  HNS3_RING_TX_EBD_NUM_REG,
					  HNS3_RING_TX_EBD_OFFSET_REG,
					  HNS3_RING_TX_BD_ERR_REG,
					  HNS3_RING_EN_REG};

static const uint32_t tqp_intr_reg_addrs[] = {HNS3_TQP_INTR_CTRL_REG,
					      HNS3_TQP_INTR_GL0_REG,
					      HNS3_TQP_INTR_GL1_REG,
					      HNS3_TQP_INTR_GL2_REG,
					      HNS3_TQP_INTR_RL_REG};

static int
hns3_get_regs_num(struct hns3_hw *hw, uint32_t *regs_num_32_bit,
		  uint32_t *regs_num_64_bit)
{
	struct hns3_cmd_desc desc;
	int ret;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_QUERY_REG_NUM, true);
	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret) {
		hns3_err(hw, "Query register number cmd failed, ret = %d",
			 ret);
		return ret;
	}

	*regs_num_32_bit = rte_le_to_cpu_32(desc.data[0]);
	*regs_num_64_bit = rte_le_to_cpu_32(desc.data[1]);

	return 0;
}

static int
hns3_get_regs_length(struct hns3_hw *hw, uint32_t *length)
{
	struct hns3_adapter *hns = HNS3_DEV_HW_TO_ADAPTER(hw);
	uint32_t cmdq_lines, common_lines, ring_lines, tqp_intr_lines;
	uint32_t regs_num_32_bit, regs_num_64_bit;
	uint32_t len;
	int ret;

	cmdq_lines = sizeof(cmdq_reg_addrs) / REG_LEN_PER_LINE + 1;
	if (hns->is_vf)
		common_lines =
			sizeof(common_vf_reg_addrs) / REG_LEN_PER_LINE + 1;
	else
		common_lines = sizeof(common_reg_addrs) / REG_LEN_PER_LINE + 1;
	ring_lines = sizeof(ring_reg_addrs) / REG_LEN_PER_LINE + 1;
	tqp_intr_lines = sizeof(tqp_intr_reg_addrs) / REG_LEN_PER_LINE + 1;

	len = (cmdq_lines + common_lines + ring_lines * hw->tqps_num +
	      tqp_intr_lines * hw->num_msi) * REG_LEN_PER_LINE;

	if (!hns->is_vf) {
		ret = hns3_get_regs_num(hw, &regs_num_32_bit, &regs_num_64_bit);
		if (ret) {
			hns3_err(hw, "Get register number failed, ret = %d.",
				 ret);
			return -ENOTSUP;
		}
		len += regs_num_32_bit * sizeof(uint32_t) +
		       regs_num_64_bit * sizeof(uint64_t);
	}

	*length = len;
	return 0;
}

static int
hns3_get_32_bit_regs(struct hns3_hw *hw, uint32_t regs_num, void *data)
{
#define HNS3_32_BIT_REG_RTN_DATANUM 8
#define HNS3_32_BIT_DESC_NODATA_LEN 2
	struct hns3_cmd_desc *desc;
	uint32_t *reg_val = data;
	uint32_t *desc_data;
	int cmd_num;
	int i, k, n;
	int ret;

	if (regs_num == 0)
		return 0;

	cmd_num = DIV_ROUND_UP(regs_num + HNS3_32_BIT_DESC_NODATA_LEN,
			       HNS3_32_BIT_REG_RTN_DATANUM);
	desc = rte_zmalloc("hns3-32bit-regs",
			   sizeof(struct hns3_cmd_desc) * cmd_num, 0);
	if (desc == NULL) {
		hns3_err(hw, "Failed to allocate %zx bytes needed to "
			 "store 32bit regs",
			 sizeof(struct hns3_cmd_desc) * cmd_num);
		return -ENOMEM;
	}

	hns3_cmd_setup_basic_desc(&desc[0], HNS3_OPC_QUERY_32_BIT_REG, true);
	ret = hns3_cmd_send(hw, desc, cmd_num);
	if (ret) {
		hns3_err(hw, "Query 32 bit register cmd failed, ret = %d",
			 ret);
		rte_free(desc);
		return ret;
	}

	for (i = 0; i < cmd_num; i++) {
		if (i == 0) {
			desc_data = &desc[i].data[0];
			n = HNS3_32_BIT_REG_RTN_DATANUM -
			    HNS3_32_BIT_DESC_NODATA_LEN;
		} else {
			desc_data = (uint32_t *)(&desc[i]);
			n = HNS3_32_BIT_REG_RTN_DATANUM;
		}
		for (k = 0; k < n; k++) {
			*reg_val++ = rte_le_to_cpu_32(*desc_data++);

			regs_num--;
			if (regs_num == 0)
				break;
		}
	}

	rte_free(desc);
	return 0;
}

static int
hns3_get_64_bit_regs(struct hns3_hw *hw, uint32_t regs_num, void *data)
{
#define HNS3_64_BIT_REG_RTN_DATANUM 4
#define HNS3_64_BIT_DESC_NODATA_LEN 1
	struct hns3_cmd_desc *desc;
	uint64_t *reg_val = data;
	uint64_t *desc_data;
	int cmd_num;
	int i, k, n;
	int ret;

	if (regs_num == 0)
		return 0;

	cmd_num = DIV_ROUND_UP(regs_num + HNS3_64_BIT_DESC_NODATA_LEN,
			       HNS3_64_BIT_REG_RTN_DATANUM);
	desc = rte_zmalloc("hns3-64bit-regs",
			   sizeof(struct hns3_cmd_desc) * cmd_num, 0);
	if (desc == NULL) {
		hns3_err(hw, "Failed to allocate %zx bytes needed to "
			 "store 64bit regs",
			 sizeof(struct hns3_cmd_desc) * cmd_num);
		return -ENOMEM;
	}

	hns3_cmd_setup_basic_desc(&desc[0], HNS3_OPC_QUERY_64_BIT_REG, true);
	ret = hns3_cmd_send(hw, desc, cmd_num);
	if (ret) {
		hns3_err(hw, "Query 64 bit register cmd failed, ret = %d",
			 ret);
		rte_free(desc);
		return ret;
	}

	for (i = 0; i < cmd_num; i++) {
		if (i == 0) {
			desc_data = (uint64_t *)(&desc[i].data[0]);
			n = HNS3_64_BIT_REG_RTN_DATANUM -
			    HNS3_64_BIT_DESC_NODATA_LEN;
		} else {
			desc_data = (uint64_t *)(&desc[i]);
			n = HNS3_64_BIT_REG_RTN_DATANUM;
		}
		for (k = 0; k < n; k++) {
			*reg_val++ = rte_le_to_cpu_64(*desc_data++);

			regs_num--;
			if (!regs_num)
				break;
		}
	}

	rte_free(desc);
	return 0;
}

static void
hns3_direct_access_regs(struct hns3_hw *hw, uint32_t *data)
{
	struct hns3_adapter *hns = HNS3_DEV_HW_TO_ADAPTER(hw);
	uint32_t reg_offset;
	int separator_num;
	int reg_um;
	int i, j;

	/* fetching per-PF registers values from PF PCIe register space */
	reg_um = sizeof(cmdq_reg_addrs) / sizeof(uint32_t);
	separator_num = MAX_SEPARATE_NUM - reg_um % REG_NUM_PER_LINE;
	for (i = 0; i < reg_um; i++)
		*data++ = hns3_read_dev(hw, cmdq_reg_addrs[i]);
	for (i = 0; i < separator_num; i++)
		*data++ = SEPARATOR_VALUE;

	if (hns->is_vf)
		reg_um = sizeof(common_vf_reg_addrs) / sizeof(uint32_t);
	else
		reg_um = sizeof(common_reg_addrs) / sizeof(uint32_t);
	separator_num = MAX_SEPARATE_NUM - reg_um % REG_NUM_PER_LINE;
	for (i = 0; i < reg_um; i++)
		if (hns->is_vf)
			*data++ = hns3_read_dev(hw, common_vf_reg_addrs[i]);
		else
			*data++ = hns3_read_dev(hw, common_reg_addrs[i]);
	for (i = 0; i < separator_num; i++)
		*data++ = SEPARATOR_VALUE;

	reg_um = sizeof(ring_reg_addrs) / sizeof(uint32_t);
	separator_num = MAX_SEPARATE_NUM - reg_um % REG_NUM_PER_LINE;
	for (j = 0; j < hw->tqps_num; j++) {
		reg_offset = HNS3_TQP_REG_OFFSET + HNS3_TQP_REG_SIZE * j;
		for (i = 0; i < reg_um; i++)
			*data++ = hns3_read_dev(hw,
						ring_reg_addrs[i] + reg_offset);
		for (i = 0; i < separator_num; i++)
			*data++ = SEPARATOR_VALUE;
	}

	reg_um = sizeof(tqp_intr_reg_addrs) / sizeof(uint32_t);
	separator_num = MAX_SEPARATE_NUM - reg_um % REG_NUM_PER_LINE;
	for (j = 0; j < hw->num_msi; j++) {
		reg_offset = HNS3_TQP_INTR_REG_SIZE * j;
		for (i = 0; i < reg_um; i++)
			*data++ = hns3_read_dev(hw,
						tqp_intr_reg_addrs[i] +
						reg_offset);
		for (i = 0; i < separator_num; i++)
			*data++ = SEPARATOR_VALUE;
	}
}

int
hns3_get_regs(struct rte_eth_dev *eth_dev, struct rte_dev_reg_info *regs)
{
	struct hns3_adapter *hns = eth_dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;
	uint32_t regs_num_32_bit;
	uint32_t regs_num_64_bit;
	uint32_t length;
	uint32_t *data;
	int ret;

	if (regs == NULL) {
		hns3_err(hw, "the input parameter regs is NULL!");
		return -EINVAL;
	}

	ret = hns3_get_regs_length(hw, &length);
	if (ret)
		return ret;

	data = regs->data;
	if (data == NULL) {
		regs->length = length;
		regs->width = sizeof(uint32_t);
		return 0;
	}

	/* Only full register dump is supported */
	if (regs->length && regs->length != length)
		return -ENOTSUP;

	/* fetching per-PF registers values from PF PCIe register space */
	hns3_direct_access_regs(hw, data);

	if (hns->is_vf)
		return 0;

	ret = hns3_get_regs_num(hw, &regs_num_32_bit, &regs_num_64_bit);
	if (ret) {
		hns3_err(hw, "Get register number failed, ret = %d", ret);
		return ret;
	}

	/* fetching PF common registers values from firmware */
	ret = hns3_get_32_bit_regs(hw, regs_num_32_bit, data);
	if (ret) {
		hns3_err(hw, "Get 32 bit register failed, ret = %d", ret);
		return ret;
	}

	data += regs_num_32_bit;
	ret = hns3_get_64_bit_regs(hw, regs_num_64_bit, data);
	if (ret)
		hns3_err(hw, "Get 64 bit register failed, ret = %d", ret);

	return ret;
}
