/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2021 HiSilicon Limited.
 */

#include <ethdev_pci.h>
#include <rte_io.h>

#include "hns3_ethdev.h"
#include "hns3_logs.h"
#include "hns3_rxtx.h"
#include "hns3_regs.h"

#define MAX_SEPARATE_NUM	4
#define SEPARATOR_VALUE		0xFFFFFFFF
#define REG_NUM_PER_LINE	4
#define REG_LEN_PER_LINE	(REG_NUM_PER_LINE * sizeof(uint32_t))

static int hns3_get_dfx_reg_line(struct hns3_hw *hw, uint32_t *lines);

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
					  HNS3_RING_RX_EN_REG,
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
					  HNS3_RING_TX_EN_REG,
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

static const uint32_t hns3_dfx_reg_opcode_list[] = {
	HNS3_OPC_DFX_BIOS_COMMON_REG,
	HNS3_OPC_DFX_SSU_REG_0,
	HNS3_OPC_DFX_SSU_REG_1,
	HNS3_OPC_DFX_IGU_EGU_REG,
	HNS3_OPC_DFX_RPU_REG_0,
	HNS3_OPC_DFX_RPU_REG_1,
	HNS3_OPC_DFX_NCSI_REG,
	HNS3_OPC_DFX_RTC_REG,
	HNS3_OPC_DFX_PPP_REG,
	HNS3_OPC_DFX_RCB_REG,
	HNS3_OPC_DFX_TQP_REG,
	HNS3_OPC_DFX_SSU_REG_2
};

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
	uint32_t dfx_reg_lines;
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
	      tqp_intr_lines * hw->num_msi) * REG_NUM_PER_LINE;

	if (!hns->is_vf) {
		ret = hns3_get_regs_num(hw, &regs_num_32_bit, &regs_num_64_bit);
		if (ret) {
			hns3_err(hw, "fail to get the number of registers, "
				 "ret = %d.", ret);
			return ret;
		}
		dfx_reg_lines = regs_num_32_bit * sizeof(uint32_t) /
					REG_LEN_PER_LINE + 1;
		dfx_reg_lines += regs_num_64_bit * sizeof(uint64_t) /
					REG_LEN_PER_LINE + 1;

		ret = hns3_get_dfx_reg_line(hw, &dfx_reg_lines);
		if (ret) {
			hns3_err(hw, "fail to get the number of dfx registers, "
				 "ret = %d.", ret);
			return ret;
		}
		len += dfx_reg_lines * REG_NUM_PER_LINE;
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

static int
hns3_insert_reg_separator(int reg_num, uint32_t *data)
{
	int separator_num;
	int i;

	separator_num = MAX_SEPARATE_NUM - reg_num % REG_NUM_PER_LINE;
	for (i = 0; i < separator_num; i++)
		*data++ = SEPARATOR_VALUE;
	return separator_num;
}

static int
hns3_direct_access_regs(struct hns3_hw *hw, uint32_t *data)
{
	struct hns3_adapter *hns = HNS3_DEV_HW_TO_ADAPTER(hw);
	uint32_t *origin_data_ptr = data;
	uint32_t reg_offset;
	size_t reg_num;
	uint16_t j;
	size_t i;

	/* fetching per-PF registers values from PF PCIe register space */
	reg_num = sizeof(cmdq_reg_addrs) / sizeof(uint32_t);
	for (i = 0; i < reg_num; i++)
		*data++ = hns3_read_dev(hw, cmdq_reg_addrs[i]);
	data += hns3_insert_reg_separator(reg_num, data);

	if (hns->is_vf)
		reg_num = sizeof(common_vf_reg_addrs) / sizeof(uint32_t);
	else
		reg_num = sizeof(common_reg_addrs) / sizeof(uint32_t);
	for (i = 0; i < reg_num; i++)
		if (hns->is_vf)
			*data++ = hns3_read_dev(hw, common_vf_reg_addrs[i]);
		else
			*data++ = hns3_read_dev(hw, common_reg_addrs[i]);
	data += hns3_insert_reg_separator(reg_num, data);

	reg_num = sizeof(ring_reg_addrs) / sizeof(uint32_t);
	for (j = 0; j < hw->tqps_num; j++) {
		reg_offset = hns3_get_tqp_reg_offset(j);
		for (i = 0; i < reg_num; i++)
			*data++ = hns3_read_dev(hw,
						ring_reg_addrs[i] + reg_offset);
		data += hns3_insert_reg_separator(reg_num, data);
	}

	reg_num = sizeof(tqp_intr_reg_addrs) / sizeof(uint32_t);
	for (j = 0; j < hw->intr_tqps_num; j++) {
		reg_offset = hns3_get_tqp_intr_reg_offset(j);
		for (i = 0; i < reg_num; i++)
			*data++ = hns3_read_dev(hw, tqp_intr_reg_addrs[i] +
						reg_offset);
		data += hns3_insert_reg_separator(reg_num, data);
	}
	return data - origin_data_ptr;
}

static int
hns3_get_dfx_reg_bd_num(struct hns3_hw *hw, uint32_t *bd_num_list,
			uint32_t list_size)
{
#define HNS3_GET_DFX_REG_BD_NUM_SIZE	4
	struct hns3_cmd_desc desc[HNS3_GET_DFX_REG_BD_NUM_SIZE];
	uint32_t index, desc_index;
	uint32_t bd_num;
	uint32_t i;
	int ret;

	for (i = 0; i < HNS3_GET_DFX_REG_BD_NUM_SIZE - 1; i++) {
		hns3_cmd_setup_basic_desc(&desc[i], HNS3_OPC_DFX_BD_NUM, true);
		desc[i].flag |= rte_cpu_to_le_16(HNS3_CMD_FLAG_NEXT);
	}
	/* The last BD does not need a next flag */
	hns3_cmd_setup_basic_desc(&desc[i], HNS3_OPC_DFX_BD_NUM, true);

	ret = hns3_cmd_send(hw, desc, HNS3_GET_DFX_REG_BD_NUM_SIZE);
	if (ret) {
		hns3_err(hw, "fail to get dfx bd num, ret = %d.\n", ret);
		return ret;
	}

	/* The first data in the first BD is a reserved field */
	for (i = 1; i <= list_size; i++) {
		desc_index = i / HNS3_CMD_DESC_DATA_NUM;
		index = i % HNS3_CMD_DESC_DATA_NUM;
		bd_num = rte_le_to_cpu_32(desc[desc_index].data[index]);
		bd_num_list[i - 1] = bd_num;
	}

	return 0;
}

static int
hns3_dfx_reg_cmd_send(struct hns3_hw *hw, struct hns3_cmd_desc *desc,
			int bd_num, uint32_t opcode)
{
	int ret;
	int i;

	for (i = 0; i < bd_num - 1; i++) {
		hns3_cmd_setup_basic_desc(&desc[i], opcode, true);
		desc[i].flag |= rte_cpu_to_le_16(HNS3_CMD_FLAG_NEXT);
	}
	/* The last BD does not need a next flag */
	hns3_cmd_setup_basic_desc(&desc[i], opcode, true);

	ret = hns3_cmd_send(hw, desc, bd_num);
	if (ret)
		hns3_err(hw, "fail to query dfx registers, opcode = 0x%04X, "
			 "ret = %d.\n", opcode, ret);

	return ret;
}

static int
hns3_dfx_reg_fetch_data(struct hns3_cmd_desc *desc, int bd_num, uint32_t *reg)
{
	int desc_index;
	int reg_num;
	int index;
	int i;

	reg_num = bd_num * HNS3_CMD_DESC_DATA_NUM;
	for (i = 0; i < reg_num; i++) {
		desc_index = i / HNS3_CMD_DESC_DATA_NUM;
		index = i % HNS3_CMD_DESC_DATA_NUM;
		*reg++ = desc[desc_index].data[index];
	}
	reg_num += hns3_insert_reg_separator(reg_num, reg);

	return reg_num;
}

static int
hns3_get_dfx_reg_line(struct hns3_hw *hw, uint32_t *lines)
{
	int opcode_num = RTE_DIM(hns3_dfx_reg_opcode_list);
	uint32_t bd_num_list[opcode_num];
	uint32_t bd_num, data_len;
	int ret;
	int i;

	ret = hns3_get_dfx_reg_bd_num(hw, bd_num_list, opcode_num);
	if (ret)
		return ret;

	for (i = 0; i < opcode_num; i++) {
		bd_num = bd_num_list[i];
		data_len = bd_num * HNS3_CMD_DESC_DATA_NUM * sizeof(uint32_t);
		*lines += data_len / REG_LEN_PER_LINE + 1;
	}

	return 0;
}

static int
hns3_get_dfx_regs(struct hns3_hw *hw, void **data)
{
	int opcode_num = RTE_DIM(hns3_dfx_reg_opcode_list);
	uint32_t max_bd_num, bd_num, opcode;
	uint32_t bd_num_list[opcode_num];
	struct hns3_cmd_desc *cmd_descs;
	uint32_t *reg_val = (uint32_t *)*data;
	int ret;
	int i;

	ret = hns3_get_dfx_reg_bd_num(hw, bd_num_list, opcode_num);
	if (ret)
		return ret;

	max_bd_num = 0;
	for (i = 0; i < opcode_num; i++)
		max_bd_num = RTE_MAX(bd_num_list[i], max_bd_num);

	cmd_descs = rte_zmalloc(NULL, sizeof(*cmd_descs) * max_bd_num, 0);
	if (cmd_descs == NULL)
		return -ENOMEM;

	for (i = 0; i < opcode_num; i++) {
		opcode = hns3_dfx_reg_opcode_list[i];
		bd_num = bd_num_list[i];
		if (bd_num == 0)
			continue;
		ret = hns3_dfx_reg_cmd_send(hw, cmd_descs, bd_num, opcode);
		if (ret)
			break;
		reg_val += hns3_dfx_reg_fetch_data(cmd_descs, bd_num, reg_val);
	}
	rte_free(cmd_descs);
	*data = (void *)reg_val;

	return ret;
}

int
hns3_get_regs(struct rte_eth_dev *eth_dev, struct rte_dev_reg_info *regs)
{
#define HNS3_64_BIT_REG_SIZE (sizeof(uint64_t) / sizeof(uint32_t))
	struct hns3_adapter *hns = eth_dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;
	uint32_t regs_num_32_bit;
	uint32_t regs_num_64_bit;
	uint32_t length;
	uint32_t *data;
	int ret;

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

	regs->version = hw->fw_version;

	/* fetching per-PF registers values from PF PCIe register space */
	data += hns3_direct_access_regs(hw, data);

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
	data += hns3_insert_reg_separator(regs_num_32_bit, data);

	ret = hns3_get_64_bit_regs(hw, regs_num_64_bit, data);
	if (ret) {
		hns3_err(hw, "Get 64 bit register failed, ret = %d", ret);
		return ret;
	}
	data += regs_num_64_bit * HNS3_64_BIT_REG_SIZE;
	data += hns3_insert_reg_separator(regs_num_64_bit *
					  HNS3_64_BIT_REG_SIZE, data);

	return  hns3_get_dfx_regs(hw, (void **)&data);
}
