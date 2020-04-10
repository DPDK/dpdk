/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2019 Hisilicon Limited.
 */

#include <stdbool.h>
#include <rte_atomic.h>
#include <rte_alarm.h>
#include <rte_cycles.h>
#include <rte_ethdev.h>
#include <rte_io.h>
#include <rte_malloc.h>
#include <rte_pci.h>
#include <rte_bus_pci.h>

#include "hns3_ethdev.h"
#include "hns3_logs.h"
#include "hns3_intr.h"
#include "hns3_regs.h"
#include "hns3_rxtx.h"

#define SWITCH_CONTEXT_US	10

/* offset in MSIX bd */
#define MAC_ERROR_OFFSET	1
#define PPP_PF_ERROR_OFFSET	2
#define PPU_PF_ERROR_OFFSET	3
#define RCB_ERROR_OFFSET	5
#define RCB_ERROR_STATUS_OFFSET	2

#define HNS3_CHECK_MERGE_CNT(val)			\
	do {						\
		if (val)				\
			hw->reset.stats.merge_cnt++;	\
	} while (0)

static const char *reset_string[HNS3_MAX_RESET] = {
	"none",	"vf_func", "vf_pf_func", "vf_full", "flr",
	"vf_global", "pf_func", "global", "IMP",
};

const struct hns3_hw_error mac_afifo_tnl_int[] = {
	{ .int_msk = BIT(0), .msg = "egu_cge_afifo_ecc_1bit_err",
	  .reset_level = HNS3_NONE_RESET },
	{ .int_msk = BIT(1), .msg = "egu_cge_afifo_ecc_mbit_err",
	  .reset_level = HNS3_GLOBAL_RESET },
	{ .int_msk = BIT(2), .msg = "egu_lge_afifo_ecc_1bit_err",
	  .reset_level = HNS3_NONE_RESET },
	{ .int_msk = BIT(3), .msg = "egu_lge_afifo_ecc_mbit_err",
	  .reset_level = HNS3_GLOBAL_RESET },
	{ .int_msk = BIT(4), .msg = "cge_igu_afifo_ecc_1bit_err",
	  .reset_level = HNS3_NONE_RESET },
	{ .int_msk = BIT(5), .msg = "cge_igu_afifo_ecc_mbit_err",
	  .reset_level = HNS3_GLOBAL_RESET },
	{ .int_msk = BIT(6), .msg = "lge_igu_afifo_ecc_1bit_err",
	  .reset_level = HNS3_NONE_RESET },
	{ .int_msk = BIT(7), .msg = "lge_igu_afifo_ecc_mbit_err",
	  .reset_level = HNS3_GLOBAL_RESET },
	{ .int_msk = BIT(8), .msg = "cge_igu_afifo_overflow_err",
	  .reset_level = HNS3_GLOBAL_RESET },
	{ .int_msk = BIT(9), .msg = "lge_igu_afifo_overflow_err",
	  .reset_level = HNS3_GLOBAL_RESET },
	{ .int_msk = BIT(10), .msg = "egu_cge_afifo_underrun_err",
	  .reset_level = HNS3_GLOBAL_RESET },
	{ .int_msk = BIT(11), .msg = "egu_lge_afifo_underrun_err",
	  .reset_level = HNS3_GLOBAL_RESET },
	{ .int_msk = BIT(12), .msg = "egu_ge_afifo_underrun_err",
	  .reset_level = HNS3_GLOBAL_RESET },
	{ .int_msk = BIT(13), .msg = "ge_igu_afifo_overflow_err",
	  .reset_level = HNS3_GLOBAL_RESET },
	{ .int_msk = 0, .msg = NULL,
	  .reset_level = HNS3_NONE_RESET}
};

const struct hns3_hw_error ppu_mpf_abnormal_int_st2[] = {
	{ .int_msk = BIT(13), .msg = "rpu_rx_pkt_bit32_ecc_mbit_err",
	  .reset_level = HNS3_GLOBAL_RESET },
	{ .int_msk = BIT(14), .msg = "rpu_rx_pkt_bit33_ecc_mbit_err",
	  .reset_level = HNS3_GLOBAL_RESET },
	{ .int_msk = BIT(15), .msg = "rpu_rx_pkt_bit34_ecc_mbit_err",
	  .reset_level = HNS3_GLOBAL_RESET },
	{ .int_msk = BIT(16), .msg = "rpu_rx_pkt_bit35_ecc_mbit_err",
	  .reset_level = HNS3_GLOBAL_RESET },
	{ .int_msk = BIT(17), .msg = "rcb_tx_ring_ecc_mbit_err",
	  .reset_level = HNS3_GLOBAL_RESET },
	{ .int_msk = BIT(18), .msg = "rcb_rx_ring_ecc_mbit_err",
	  .reset_level = HNS3_GLOBAL_RESET },
	{ .int_msk = BIT(19), .msg = "rcb_tx_fbd_ecc_mbit_err",
	  .reset_level = HNS3_GLOBAL_RESET },
	{ .int_msk = BIT(20), .msg = "rcb_rx_ebd_ecc_mbit_err",
	  .reset_level = HNS3_GLOBAL_RESET },
	{ .int_msk = BIT(21), .msg = "rcb_tso_info_ecc_mbit_err",
	  .reset_level = HNS3_GLOBAL_RESET },
	{ .int_msk = BIT(22), .msg = "rcb_tx_int_info_ecc_mbit_err",
	  .reset_level = HNS3_GLOBAL_RESET },
	{ .int_msk = BIT(23), .msg = "rcb_rx_int_info_ecc_mbit_err",
	  .reset_level = HNS3_GLOBAL_RESET },
	{ .int_msk = BIT(24), .msg = "tpu_tx_pkt_0_ecc_mbit_err",
	  .reset_level = HNS3_GLOBAL_RESET },
	{ .int_msk = BIT(25), .msg = "tpu_tx_pkt_1_ecc_mbit_err",
	  .reset_level = HNS3_GLOBAL_RESET },
	{ .int_msk = BIT(26), .msg = "rd_bus_err",
	  .reset_level = HNS3_GLOBAL_RESET },
	{ .int_msk = BIT(27), .msg = "wr_bus_err",
	  .reset_level = HNS3_GLOBAL_RESET },
	{ .int_msk = BIT(28), .msg = "reg_search_miss",
	  .reset_level = HNS3_GLOBAL_RESET },
	{ .int_msk = BIT(29), .msg = "rx_q_search_miss",
	  .reset_level = HNS3_NONE_RESET },
	{ .int_msk = BIT(30), .msg = "ooo_ecc_err_detect",
	  .reset_level = HNS3_NONE_RESET },
	{ .int_msk = BIT(31), .msg = "ooo_ecc_err_multpl",
	  .reset_level = HNS3_GLOBAL_RESET },
	{ .int_msk = 0, .msg = NULL,
	  .reset_level = HNS3_NONE_RESET}
};

const struct hns3_hw_error ssu_port_based_pf_int[] = {
	{ .int_msk = BIT(0), .msg = "roc_pkt_without_key_port",
	  .reset_level = HNS3_GLOBAL_RESET },
	{ .int_msk = BIT(9), .msg = "low_water_line_err_port",
	  .reset_level = HNS3_NONE_RESET },
	{ .int_msk = BIT(10), .msg = "hi_water_line_err_port",
	  .reset_level = HNS3_GLOBAL_RESET },
	{ .int_msk = 0, .msg = NULL,
	  .reset_level = HNS3_NONE_RESET}
};

const struct hns3_hw_error ppp_pf_abnormal_int[] = {
	{ .int_msk = BIT(0), .msg = "tx_vlan_tag_err",
	  .reset_level = HNS3_NONE_RESET },
	{ .int_msk = BIT(1), .msg = "rss_list_tc_unassigned_queue_err",
	  .reset_level = HNS3_NONE_RESET },
	{ .int_msk = 0, .msg = NULL,
	  .reset_level = HNS3_NONE_RESET}
};

const struct hns3_hw_error ppu_pf_abnormal_int[] = {
	{ .int_msk = BIT(0), .msg = "over_8bd_no_fe",
	  .reset_level = HNS3_FUNC_RESET },
	{ .int_msk = BIT(1), .msg = "tso_mss_cmp_min_err",
	  .reset_level = HNS3_NONE_RESET },
	{ .int_msk = BIT(2), .msg = "tso_mss_cmp_max_err",
	  .reset_level = HNS3_NONE_RESET },
	{ .int_msk = BIT(3), .msg = "tx_rd_fbd_poison",
	  .reset_level = HNS3_FUNC_RESET },
	{ .int_msk = BIT(4), .msg = "rx_rd_ebd_poison",
	  .reset_level = HNS3_FUNC_RESET },
	{ .int_msk = BIT(5), .msg = "buf_wait_timeout",
	  .reset_level = HNS3_NONE_RESET },
	{ .int_msk = 0, .msg = NULL,
	  .reset_level = HNS3_NONE_RESET}
};

static int
config_ppp_err_intr(struct hns3_adapter *hns, uint32_t cmd, bool en)
{
	struct hns3_hw *hw = &hns->hw;
	struct hns3_cmd_desc desc[2];
	int ret;

	/* configure PPP error interrupts */
	hns3_cmd_setup_basic_desc(&desc[0], cmd, false);
	desc[0].flag |= rte_cpu_to_le_16(HNS3_CMD_FLAG_NEXT);
	hns3_cmd_setup_basic_desc(&desc[1], cmd, false);

	if (cmd == HNS3_PPP_CMD0_INT_CMD) {
		if (en) {
			desc[0].data[0] =
				rte_cpu_to_le_32(HNS3_PPP_MPF_ECC_ERR_INT0_EN);
			desc[0].data[1] =
				rte_cpu_to_le_32(HNS3_PPP_MPF_ECC_ERR_INT1_EN);
			desc[0].data[4] =
				rte_cpu_to_le_32(HNS3_PPP_PF_ERR_INT_EN);
		}

		desc[1].data[0] =
			rte_cpu_to_le_32(HNS3_PPP_MPF_ECC_ERR_INT0_EN_MASK);
		desc[1].data[1] =
			rte_cpu_to_le_32(HNS3_PPP_MPF_ECC_ERR_INT1_EN_MASK);
		desc[1].data[2] =
			rte_cpu_to_le_32(HNS3_PPP_PF_ERR_INT_EN_MASK);
	} else if (cmd == HNS3_PPP_CMD1_INT_CMD) {
		if (en) {
			desc[0].data[0] =
				rte_cpu_to_le_32(HNS3_PPP_MPF_ECC_ERR_INT2_EN);
			desc[0].data[1] =
				rte_cpu_to_le_32(HNS3_PPP_MPF_ECC_ERR_INT3_EN);
		}

		desc[1].data[0] =
			rte_cpu_to_le_32(HNS3_PPP_MPF_ECC_ERR_INT2_EN_MASK);
		desc[1].data[1] =
			rte_cpu_to_le_32(HNS3_PPP_MPF_ECC_ERR_INT3_EN_MASK);
	}

	ret = hns3_cmd_send(hw, &desc[0], 2);
	if (ret)
		hns3_err(hw, "fail to configure PPP error int: %d", ret);

	return ret;
}

static int
enable_ppp_err_intr(struct hns3_adapter *hns, bool en)
{
	int ret;

	ret = config_ppp_err_intr(hns, HNS3_PPP_CMD0_INT_CMD, en);
	if (ret)
		return ret;

	return config_ppp_err_intr(hns, HNS3_PPP_CMD1_INT_CMD, en);
}

static int
enable_ssu_err_intr(struct hns3_adapter *hns, bool en)
{
	struct hns3_hw *hw = &hns->hw;
	struct hns3_cmd_desc desc[2];
	int ret;

	/* configure SSU ecc error interrupts */
	hns3_cmd_setup_basic_desc(&desc[0], HNS3_SSU_ECC_INT_CMD, false);
	desc[0].flag |= rte_cpu_to_le_16(HNS3_CMD_FLAG_NEXT);
	hns3_cmd_setup_basic_desc(&desc[1], HNS3_SSU_ECC_INT_CMD, false);
	if (en) {
		desc[0].data[0] =
			rte_cpu_to_le_32(HNS3_SSU_1BIT_ECC_ERR_INT_EN);
		desc[0].data[1] =
			rte_cpu_to_le_32(HNS3_SSU_MULTI_BIT_ECC_ERR_INT_EN);
		desc[0].data[4] =
			rte_cpu_to_le_32(HNS3_SSU_BIT32_ECC_ERR_INT_EN);
	}

	desc[1].data[0] = rte_cpu_to_le_32(HNS3_SSU_1BIT_ECC_ERR_INT_EN_MASK);
	desc[1].data[1] =
		rte_cpu_to_le_32(HNS3_SSU_MULTI_BIT_ECC_ERR_INT_EN_MASK);
	desc[1].data[2] = rte_cpu_to_le_32(HNS3_SSU_BIT32_ECC_ERR_INT_EN_MASK);

	ret = hns3_cmd_send(hw, &desc[0], 2);
	if (ret) {
		hns3_err(hw, "fail to configure SSU ECC error interrupt: %d",
			 ret);
		return ret;
	}

	/* configure SSU common error interrupts */
	hns3_cmd_setup_basic_desc(&desc[0], HNS3_SSU_COMMON_INT_CMD, false);
	desc[0].flag |= rte_cpu_to_le_16(HNS3_CMD_FLAG_NEXT);
	hns3_cmd_setup_basic_desc(&desc[1], HNS3_SSU_COMMON_INT_CMD, false);

	if (en) {
		desc[0].data[0] = rte_cpu_to_le_32(HNS3_SSU_COMMON_INT_EN);
		desc[0].data[1] =
			rte_cpu_to_le_32(HNS3_SSU_PORT_BASED_ERR_INT_EN);
		desc[0].data[2] =
			rte_cpu_to_le_32(HNS3_SSU_FIFO_OVERFLOW_ERR_INT_EN);
	}

	desc[1].data[0] = rte_cpu_to_le_32(HNS3_SSU_COMMON_INT_EN_MASK |
					   HNS3_SSU_PORT_BASED_ERR_INT_EN_MASK);
	desc[1].data[1] =
		rte_cpu_to_le_32(HNS3_SSU_FIFO_OVERFLOW_ERR_INT_EN_MASK);

	ret = hns3_cmd_send(hw, &desc[0], 2);
	if (ret)
		hns3_err(hw, "fail to configure SSU COMMON error intr: %d",
			 ret);

	return ret;
}

static int
config_ppu_err_intrs(struct hns3_adapter *hns, uint32_t cmd, bool en)
{
	struct hns3_hw *hw = &hns->hw;
	struct hns3_cmd_desc desc[2];
	int num = 1;

	/* configure PPU error interrupts */
	switch (cmd) {
	case HNS3_PPU_MPF_ECC_INT_CMD:
		hns3_cmd_setup_basic_desc(&desc[0], cmd, false);
		desc[0].flag |= HNS3_CMD_FLAG_NEXT;
		hns3_cmd_setup_basic_desc(&desc[1], cmd, false);
		if (en) {
			desc[0].data[0] = HNS3_PPU_MPF_ABNORMAL_INT0_EN;
			desc[0].data[1] = HNS3_PPU_MPF_ABNORMAL_INT1_EN;
			desc[1].data[3] = HNS3_PPU_MPF_ABNORMAL_INT3_EN;
			desc[1].data[4] = HNS3_PPU_MPF_ABNORMAL_INT2_EN;
		}

		desc[1].data[0] = HNS3_PPU_MPF_ABNORMAL_INT0_EN_MASK;
		desc[1].data[1] = HNS3_PPU_MPF_ABNORMAL_INT1_EN_MASK;
		desc[1].data[2] = HNS3_PPU_MPF_ABNORMAL_INT2_EN_MASK;
		desc[1].data[3] |= HNS3_PPU_MPF_ABNORMAL_INT3_EN_MASK;
		num = 2;
		break;
	case HNS3_PPU_MPF_OTHER_INT_CMD:
		hns3_cmd_setup_basic_desc(&desc[0], cmd, false);
		if (en)
			desc[0].data[0] = HNS3_PPU_MPF_ABNORMAL_INT2_EN2;

		desc[0].data[2] = HNS3_PPU_MPF_ABNORMAL_INT2_EN2_MASK;
		break;
	case HNS3_PPU_PF_OTHER_INT_CMD:
		hns3_cmd_setup_basic_desc(&desc[0], cmd, false);
		if (en)
			desc[0].data[0] = HNS3_PPU_PF_ABNORMAL_INT_EN;

		desc[0].data[2] = HNS3_PPU_PF_ABNORMAL_INT_EN_MASK;
		break;
	default:
		hns3_err(hw,
			 "Invalid cmd(%u) to configure PPU error interrupts.",
			 cmd);
		return -EINVAL;
	}

	return hns3_cmd_send(hw, &desc[0], num);
}

static int
enable_ppu_err_intr(struct hns3_adapter *hns, bool en)
{
	struct hns3_hw *hw = &hns->hw;
	int ret;

	ret = config_ppu_err_intrs(hns, HNS3_PPU_MPF_ECC_INT_CMD, en);
	if (ret) {
		hns3_err(hw, "fail to configure PPU MPF ECC error intr: %d",
			 ret);
		return ret;
	}

	ret = config_ppu_err_intrs(hns, HNS3_PPU_MPF_OTHER_INT_CMD, en);
	if (ret) {
		hns3_err(hw, "fail to configure PPU MPF other intr: %d",
			 ret);
		return ret;
	}

	ret = config_ppu_err_intrs(hns, HNS3_PPU_PF_OTHER_INT_CMD, en);
	if (ret)
		hns3_err(hw, "fail to configure PPU PF error interrupts: %d",
			 ret);
	return ret;
}

static int
enable_mac_err_intr(struct hns3_adapter *hns, bool en)
{
	struct hns3_hw *hw = &hns->hw;
	struct hns3_cmd_desc desc;
	int ret;

	/* configure MAC common error interrupts */
	hns3_cmd_setup_basic_desc(&desc, HNS3_MAC_COMMON_INT_EN, false);
	if (en)
		desc.data[0] = rte_cpu_to_le_32(HNS3_MAC_COMMON_ERR_INT_EN);

	desc.data[1] = rte_cpu_to_le_32(HNS3_MAC_COMMON_ERR_INT_EN_MASK);

	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret)
		hns3_err(hw, "fail to configure MAC COMMON error intr: %d",
			 ret);

	return ret;
}

static const struct hns3_hw_blk hw_blk[] = {
	{
		.name = "PPP",
		.enable_err_intr = enable_ppp_err_intr,
	},
	{
		.name = "SSU",
		.enable_err_intr = enable_ssu_err_intr,
	},
	{
		.name = "PPU",
		.enable_err_intr = enable_ppu_err_intr,
	},
	{
		.name = "MAC",
		.enable_err_intr = enable_mac_err_intr,
	},
	{
		.name = NULL,
		.enable_err_intr = NULL,
	}
};

int
hns3_enable_hw_error_intr(struct hns3_adapter *hns, bool en)
{
	const struct hns3_hw_blk *module = hw_blk;
	int ret = 0;

	while (module->enable_err_intr) {
		ret = module->enable_err_intr(hns, en);
		if (ret)
			return ret;

		module++;
	}

	return ret;
}

static enum hns3_reset_level
hns3_find_highest_level(struct hns3_adapter *hns, const char *reg,
			const struct hns3_hw_error *err, uint32_t err_sts)
{
	enum hns3_reset_level reset_level = HNS3_FUNC_RESET;
	struct hns3_hw *hw = &hns->hw;
	bool need_reset = false;

	while (err->msg) {
		if (err->int_msk & err_sts) {
			hns3_warn(hw, "%s %s found [error status=0x%x]",
				  reg, err->msg, err_sts);
			if (err->reset_level != HNS3_NONE_RESET &&
			    err->reset_level >= reset_level) {
				reset_level = err->reset_level;
				need_reset = true;
			}
		}
		err++;
	}
	if (need_reset)
		return reset_level;
	else
		return HNS3_NONE_RESET;
}

static int
query_num_bds_in_msix(struct hns3_hw *hw, struct hns3_cmd_desc *desc_bd)
{
	int ret;

	hns3_cmd_setup_basic_desc(desc_bd, HNS3_QUERY_MSIX_INT_STS_BD_NUM,
				  true);
	ret = hns3_cmd_send(hw, desc_bd, 1);
	if (ret)
		hns3_err(hw, "query num bds in msix failed: %d", ret);

	return ret;
}

static int
query_all_mpf_msix_err(struct hns3_hw *hw, struct hns3_cmd_desc *desc,
		       uint32_t mpf_bd_num)
{
	int ret;

	hns3_cmd_setup_basic_desc(desc, HNS3_QUERY_CLEAR_ALL_MPF_MSIX_INT,
				  true);
	desc[0].flag |= rte_cpu_to_le_16(HNS3_CMD_FLAG_NEXT);

	ret = hns3_cmd_send(hw, &desc[0], mpf_bd_num);
	if (ret)
		hns3_err(hw, "query all mpf msix err failed: %d", ret);

	return ret;
}

static int
clear_all_mpf_msix_err(struct hns3_hw *hw, struct hns3_cmd_desc *desc,
		       uint32_t mpf_bd_num)
{
	int ret;

	hns3_cmd_reuse_desc(desc, false);
	desc[0].flag |= rte_cpu_to_le_16(HNS3_CMD_FLAG_NEXT);

	ret = hns3_cmd_send(hw, desc, mpf_bd_num);
	if (ret)
		hns3_err(hw, "clear all mpf msix err failed: %d", ret);

	return ret;
}

static int
query_all_pf_msix_err(struct hns3_hw *hw, struct hns3_cmd_desc *desc,
		      uint32_t pf_bd_num)
{
	int ret;

	hns3_cmd_setup_basic_desc(desc, HNS3_QUERY_CLEAR_ALL_PF_MSIX_INT, true);
	desc[0].flag |= rte_cpu_to_le_16(HNS3_CMD_FLAG_NEXT);

	ret = hns3_cmd_send(hw, desc, pf_bd_num);
	if (ret)
		hns3_err(hw, "query all pf msix int cmd failed: %d", ret);

	return ret;
}

static int
clear_all_pf_msix_err(struct hns3_hw *hw, struct hns3_cmd_desc *desc,
		      uint32_t pf_bd_num)
{
	int ret;

	hns3_cmd_reuse_desc(desc, false);
	desc[0].flag |= rte_cpu_to_le_16(HNS3_CMD_FLAG_NEXT);

	ret = hns3_cmd_send(hw, desc, pf_bd_num);
	if (ret)
		hns3_err(hw, "clear all pf msix err failed: %d", ret);

	return ret;
}

void
hns3_intr_unregister(const struct rte_intr_handle *hdl,
		     rte_intr_callback_fn cb_fn, void *cb_arg)
{
	int retry_cnt = 0;
	int ret;

	do {
		ret = rte_intr_callback_unregister(hdl, cb_fn, cb_arg);
		if (ret >= 0) {
			break;
		} else if (ret != -EAGAIN) {
			PMD_INIT_LOG(ERR, "Failed to unregister intr: %d", ret);
			break;
		}
		rte_delay_ms(HNS3_INTR_UNREG_FAIL_DELAY_MS);
	} while (retry_cnt++ < HNS3_INTR_UNREG_FAIL_RETRY_CNT);
}

void
hns3_handle_msix_error(struct hns3_adapter *hns, uint64_t *levels)
{
	uint32_t mpf_bd_num, pf_bd_num, bd_num;
	enum hns3_reset_level req_level;
	struct hns3_hw *hw = &hns->hw;
	struct hns3_pf *pf = &hns->pf;
	struct hns3_cmd_desc desc_bd;
	struct hns3_cmd_desc *desc;
	uint32_t *desc_data;
	uint32_t status;
	int ret;

	/* query the number of bds for the MSIx int status */
	ret = query_num_bds_in_msix(hw, &desc_bd);
	if (ret) {
		hns3_err(hw, "fail to query msix int status bd num: %d", ret);
		return;
	}

	mpf_bd_num = rte_le_to_cpu_32(desc_bd.data[0]);
	pf_bd_num = rte_le_to_cpu_32(desc_bd.data[1]);
	bd_num = max_t(uint32_t, mpf_bd_num, pf_bd_num);
	if (bd_num < RCB_ERROR_OFFSET) {
		hns3_err(hw, "bd_num is less than RCB_ERROR_OFFSET: %u",
			 bd_num);
		return;
	}

	desc = rte_zmalloc(NULL, bd_num * sizeof(struct hns3_cmd_desc), 0);
	if (desc == NULL) {
		hns3_err(hw, "fail to zmalloc desc");
		return;
	}

	/* query all main PF MSIx errors */
	ret = query_all_mpf_msix_err(hw, &desc[0], mpf_bd_num);
	if (ret) {
		hns3_err(hw, "query all mpf msix int cmd failed: %d", ret);
		goto out;
	}

	/* log MAC errors */
	desc_data = (uint32_t *)&desc[MAC_ERROR_OFFSET];
	status = rte_le_to_cpu_32(*desc_data);
	if (status) {
		req_level = hns3_find_highest_level(hns, "MAC_AFIFO_TNL_INT_R",
						    mac_afifo_tnl_int,
						    status);
		hns3_atomic_set_bit(req_level, levels);
		pf->abn_int_stats.mac_afifo_tnl_intr_cnt++;
	}

	/* log PPU(RCB) errors */
	desc_data = (uint32_t *)&desc[RCB_ERROR_OFFSET];
	status = rte_le_to_cpu_32(*(desc_data + RCB_ERROR_STATUS_OFFSET)) &
			HNS3_PPU_MPF_INT_ST2_MSIX_MASK;
	if (status) {
		req_level = hns3_find_highest_level(hns,
						    "PPU_MPF_ABNORMAL_INT_ST2",
						    ppu_mpf_abnormal_int_st2,
						    status);
		hns3_atomic_set_bit(req_level, levels);
		pf->abn_int_stats.ppu_mpf_abnormal_intr_st2_cnt++;
	}

	/* clear all main PF MSIx errors */
	ret = clear_all_mpf_msix_err(hw, desc, mpf_bd_num);
	if (ret) {
		hns3_err(hw, "clear all mpf msix int cmd failed: %d", ret);
		goto out;
	}

	/* query all PF MSIx errors */
	memset(desc, 0, bd_num * sizeof(struct hns3_cmd_desc));
	ret = query_all_pf_msix_err(hw, &desc[0], pf_bd_num);
	if (ret) {
		hns3_err(hw, "query all pf msix int cmd failed (%d)", ret);
		goto out;
	}

	/* log SSU PF errors */
	status = rte_le_to_cpu_32(desc[0].data[0]) &
		 HNS3_SSU_PORT_INT_MSIX_MASK;
	if (status) {
		req_level = hns3_find_highest_level(hns,
						    "SSU_PORT_BASED_ERR_INT",
						    ssu_port_based_pf_int,
						    status);
		hns3_atomic_set_bit(req_level, levels);
		pf->abn_int_stats.ssu_port_based_pf_intr_cnt++;
	}

	/* log PPP PF errors */
	desc_data = (uint32_t *)&desc[PPP_PF_ERROR_OFFSET];
	status = rte_le_to_cpu_32(*desc_data);
	if (status) {
		req_level = hns3_find_highest_level(hns,
						    "PPP_PF_ABNORMAL_INT_ST0",
						    ppp_pf_abnormal_int,
						    status);
		hns3_atomic_set_bit(req_level, levels);
		pf->abn_int_stats.ppp_pf_abnormal_intr_cnt++;
	}

	/* log PPU(RCB) PF errors */
	desc_data = (uint32_t *)&desc[PPU_PF_ERROR_OFFSET];
	status = rte_le_to_cpu_32(*desc_data) & HNS3_PPU_PF_INT_MSIX_MASK;
	if (status) {
		req_level = hns3_find_highest_level(hns,
						    "PPU_PF_ABNORMAL_INT_ST",
						    ppu_pf_abnormal_int,
						    status);
		hns3_atomic_set_bit(req_level, levels);
		pf->abn_int_stats.ppu_pf_abnormal_intr_cnt++;
	}

	/* clear all PF MSIx errors */
	ret = clear_all_pf_msix_err(hw, desc, pf_bd_num);
	if (ret)
		hns3_err(hw, "clear all pf msix int cmd failed: %d", ret);
out:
	rte_free(desc);
}

int
hns3_reset_init(struct hns3_hw *hw)
{
	rte_spinlock_init(&hw->lock);
	hw->reset.level = HNS3_NONE_RESET;
	hw->reset.stage = RESET_STAGE_NONE;
	hw->reset.request = 0;
	hw->reset.pending = 0;
	rte_atomic16_init(&hw->reset.resetting);
	rte_atomic16_init(&hw->reset.disable_cmd);
	hw->reset.wait_data = rte_zmalloc("wait_data",
					  sizeof(struct hns3_wait_data), 0);
	if (!hw->reset.wait_data) {
		PMD_INIT_LOG(ERR, "Failed to allocate memory for wait_data");
		return -ENOMEM;
	}
	return 0;
}

void
hns3_schedule_reset(struct hns3_adapter *hns)
{
	struct hns3_hw *hw = &hns->hw;

	/* Reschedule the reset process after successful initialization */
	if (hw->adapter_state == HNS3_NIC_UNINITIALIZED) {
		rte_atomic16_set(&hns->hw.reset.schedule, SCHEDULE_PENDING);
		return;
	}

	if (hw->adapter_state >= HNS3_NIC_CLOSED)
		return;

	/* Schedule restart alarm if it is not scheduled yet */
	if (rte_atomic16_read(&hns->hw.reset.schedule) == SCHEDULE_REQUESTED)
		return;
	if (rte_atomic16_read(&hns->hw.reset.schedule) == SCHEDULE_DEFERRED)
		rte_eal_alarm_cancel(hw->reset.ops->reset_service, hns);
	rte_atomic16_set(&hns->hw.reset.schedule, SCHEDULE_REQUESTED);

	rte_eal_alarm_set(SWITCH_CONTEXT_US, hw->reset.ops->reset_service, hns);
}

void
hns3_schedule_delayed_reset(struct hns3_adapter *hns)
{
#define DEFERRED_SCHED_US (3 * MSEC_PER_SEC * USEC_PER_MSEC)
	struct hns3_hw *hw = &hns->hw;

	/* Do nothing if it is uninited or closed */
	if (hw->adapter_state == HNS3_NIC_UNINITIALIZED ||
	    hw->adapter_state >= HNS3_NIC_CLOSED) {
		return;
	}

	if (rte_atomic16_read(&hns->hw.reset.schedule) != SCHEDULE_NONE)
		return;
	rte_atomic16_set(&hns->hw.reset.schedule, SCHEDULE_DEFERRED);
	rte_eal_alarm_set(DEFERRED_SCHED_US, hw->reset.ops->reset_service, hns);
}

void
hns3_wait_callback(void *param)
{
	struct hns3_wait_data *data = (struct hns3_wait_data *)param;
	struct hns3_adapter *hns = data->hns;
	struct hns3_hw *hw = &hns->hw;
	uint64_t msec;
	bool done;

	data->count--;
	if (data->check_completion) {
		/*
		 * Check if the current time exceeds the deadline
		 * or a pending reset coming, or reset during close.
		 */
		msec = get_timeofday_ms();
		if (msec > data->end_ms || is_reset_pending(hns) ||
		    hw->adapter_state == HNS3_NIC_CLOSING) {
			done = false;
			data->count = 0;
		} else
			done = data->check_completion(hw);
	} else
		done = true;

	if (!done && data->count > 0) {
		rte_eal_alarm_set(data->interval, hns3_wait_callback, data);
		return;
	}
	if (done)
		data->result = HNS3_WAIT_SUCCESS;
	else {
		hns3_err(hw, "%s wait timeout at stage %d",
			 reset_string[hw->reset.level], hw->reset.stage);
		data->result = HNS3_WAIT_TIMEOUT;
	}
	hns3_schedule_reset(hns);
}

void
hns3_notify_reset_ready(struct hns3_hw *hw, bool enable)
{
	uint32_t reg_val;

	reg_val = hns3_read_dev(hw, HNS3_CMDQ_TX_DEPTH_REG);
	if (enable)
		reg_val |= HNS3_NIC_SW_RST_RDY;
	else
		reg_val &= ~HNS3_NIC_SW_RST_RDY;

	hns3_write_dev(hw, HNS3_CMDQ_TX_DEPTH_REG, reg_val);
}

int
hns3_reset_req_hw_reset(struct hns3_adapter *hns)
{
	struct hns3_hw *hw = &hns->hw;

	if (hw->reset.wait_data->result == HNS3_WAIT_UNKNOWN) {
		hw->reset.wait_data->hns = hns;
		hw->reset.wait_data->check_completion = NULL;
		hw->reset.wait_data->interval = HNS3_RESET_SYNC_US;
		hw->reset.wait_data->count = 1;
		hw->reset.wait_data->result = HNS3_WAIT_REQUEST;
		rte_eal_alarm_set(hw->reset.wait_data->interval,
				  hns3_wait_callback, hw->reset.wait_data);
		return -EAGAIN;
	} else if (hw->reset.wait_data->result == HNS3_WAIT_REQUEST)
		return -EAGAIN;

	/* inform hardware that preparatory work is done */
	hns3_notify_reset_ready(hw, true);
	return 0;
}

static void
hns3_clear_reset_level(struct hns3_hw *hw, uint64_t *levels)
{
	uint64_t merge_cnt = hw->reset.stats.merge_cnt;
	int64_t tmp;

	switch (hw->reset.level) {
	case HNS3_IMP_RESET:
		hns3_atomic_clear_bit(HNS3_IMP_RESET, levels);
		tmp = hns3_test_and_clear_bit(HNS3_GLOBAL_RESET, levels);
		HNS3_CHECK_MERGE_CNT(tmp);
		tmp = hns3_test_and_clear_bit(HNS3_FUNC_RESET, levels);
		HNS3_CHECK_MERGE_CNT(tmp);
		break;
	case HNS3_GLOBAL_RESET:
		hns3_atomic_clear_bit(HNS3_GLOBAL_RESET, levels);
		tmp = hns3_test_and_clear_bit(HNS3_FUNC_RESET, levels);
		HNS3_CHECK_MERGE_CNT(tmp);
		break;
	case HNS3_FUNC_RESET:
		hns3_atomic_clear_bit(HNS3_FUNC_RESET, levels);
		break;
	case HNS3_VF_RESET:
		hns3_atomic_clear_bit(HNS3_VF_RESET, levels);
		tmp = hns3_test_and_clear_bit(HNS3_VF_PF_FUNC_RESET, levels);
		HNS3_CHECK_MERGE_CNT(tmp);
		tmp = hns3_test_and_clear_bit(HNS3_VF_FUNC_RESET, levels);
		HNS3_CHECK_MERGE_CNT(tmp);
		break;
	case HNS3_VF_FULL_RESET:
		hns3_atomic_clear_bit(HNS3_VF_FULL_RESET, levels);
		tmp = hns3_test_and_clear_bit(HNS3_VF_FUNC_RESET, levels);
		HNS3_CHECK_MERGE_CNT(tmp);
		break;
	case HNS3_VF_PF_FUNC_RESET:
		hns3_atomic_clear_bit(HNS3_VF_PF_FUNC_RESET, levels);
		tmp = hns3_test_and_clear_bit(HNS3_VF_FUNC_RESET, levels);
		HNS3_CHECK_MERGE_CNT(tmp);
		break;
	case HNS3_VF_FUNC_RESET:
		hns3_atomic_clear_bit(HNS3_VF_FUNC_RESET, levels);
		break;
	case HNS3_FLR_RESET:
		hns3_atomic_clear_bit(HNS3_FLR_RESET, levels);
		break;
	case HNS3_NONE_RESET:
	default:
		return;
	};
	if (merge_cnt != hw->reset.stats.merge_cnt)
		hns3_warn(hw,
			  "No need to do low-level reset after %s reset. "
			  "merge cnt: %" PRIx64 " total merge cnt: %" PRIx64,
			  reset_string[hw->reset.level],
			  hw->reset.stats.merge_cnt - merge_cnt,
			  hw->reset.stats.merge_cnt);
}

static bool
hns3_reset_err_handle(struct hns3_adapter *hns)
{
#define MAX_RESET_FAIL_CNT 5

	struct hns3_hw *hw = &hns->hw;

	if (hw->adapter_state == HNS3_NIC_CLOSING)
		goto reset_fail;

	if (is_reset_pending(hns)) {
		hw->reset.attempts = 0;
		hw->reset.stats.fail_cnt++;
		hns3_warn(hw, "%s reset fail because new Reset is pending "
			      "attempts:%" PRIx64,
			  reset_string[hw->reset.level],
			  hw->reset.stats.fail_cnt);
		hw->reset.level = HNS3_NONE_RESET;
		return true;
	}

	hw->reset.attempts++;
	if (hw->reset.attempts < MAX_RESET_FAIL_CNT) {
		hns3_atomic_set_bit(hw->reset.level, &hw->reset.pending);
		hns3_warn(hw, "%s retry to reset attempts: %d",
			  reset_string[hw->reset.level],
			  hw->reset.attempts);
		return true;
	}

	if (rte_atomic16_read(&hw->reset.disable_cmd))
		hns3_cmd_init(hw);
reset_fail:
	hw->reset.attempts = 0;
	hw->reset.stats.fail_cnt++;
	hns3_warn(hw, "%s reset fail fail_cnt:%" PRIx64 " success_cnt:%" PRIx64
		  " global_cnt:%" PRIx64 " imp_cnt:%" PRIx64
		  " request_cnt:%" PRIx64 " exec_cnt:%" PRIx64
		  " merge_cnt:%" PRIx64 "adapter_state:%d",
		  reset_string[hw->reset.level], hw->reset.stats.fail_cnt,
		  hw->reset.stats.success_cnt, hw->reset.stats.global_cnt,
		  hw->reset.stats.imp_cnt, hw->reset.stats.request_cnt,
		  hw->reset.stats.exec_cnt, hw->reset.stats.merge_cnt,
		  hw->adapter_state);

	/* IMP no longer waiting the ready flag */
	hns3_notify_reset_ready(hw, true);
	return false;
}

static int
hns3_reset_pre(struct hns3_adapter *hns)
{
	struct hns3_hw *hw = &hns->hw;
	struct timeval tv;
	int ret;

	if (hw->reset.stage == RESET_STAGE_NONE) {
		rte_atomic16_set(&hns->hw.reset.resetting, 1);
		hw->reset.stage = RESET_STAGE_DOWN;
		ret = hw->reset.ops->stop_service(hns);
		gettimeofday(&tv, NULL);
		if (ret) {
			hns3_warn(hw, "Reset step1 down fail=%d time=%ld.%.6ld",
				  ret, tv.tv_sec, tv.tv_usec);
			return ret;
		}
		hns3_warn(hw, "Reset step1 down success time=%ld.%.6ld",
			  tv.tv_sec, tv.tv_usec);
		hw->reset.stage = RESET_STAGE_PREWAIT;
	}
	if (hw->reset.stage == RESET_STAGE_PREWAIT) {
		ret = hw->reset.ops->prepare_reset(hns);
		gettimeofday(&tv, NULL);
		if (ret) {
			hns3_warn(hw,
				  "Reset step2 prepare wait fail=%d time=%ld.%.6ld",
				  ret, tv.tv_sec, tv.tv_usec);
			return ret;
		}
		hns3_warn(hw, "Reset step2 prepare wait success time=%ld.%.6ld",
			  tv.tv_sec, tv.tv_usec);
		hw->reset.stage = RESET_STAGE_REQ_HW_RESET;
		hw->reset.wait_data->result = HNS3_WAIT_UNKNOWN;
	}
	return 0;
}

static int
hns3_reset_post(struct hns3_adapter *hns)
{
#define TIMEOUT_RETRIES_CNT	5
	struct hns3_hw *hw = &hns->hw;
	struct timeval tv_delta;
	struct timeval tv;
	int ret = 0;

	if (hw->adapter_state == HNS3_NIC_CLOSING) {
		hns3_warn(hw, "Don't do reset_post during closing, just uninit cmd");
		hns3_cmd_uninit(hw);
		return -EPERM;
	}

	if (hw->reset.stage == RESET_STAGE_DEV_INIT) {
		rte_spinlock_lock(&hw->lock);
		if (hw->reset.mbuf_deferred_free) {
			hns3_dev_release_mbufs(hns);
			hw->reset.mbuf_deferred_free = false;
		}
		ret = hw->reset.ops->reinit_dev(hns);
		rte_spinlock_unlock(&hw->lock);
		gettimeofday(&tv, NULL);
		if (ret) {
			hns3_warn(hw, "Reset step5 devinit fail=%d retries=%d",
				  ret, hw->reset.retries);
			goto err;
		}
		hns3_warn(hw, "Reset step5 devinit success time=%ld.%.6ld",
			  tv.tv_sec, tv.tv_usec);
		hw->reset.retries = 0;
		hw->reset.stage = RESET_STAGE_RESTORE;
		rte_eal_alarm_set(SWITCH_CONTEXT_US,
				  hw->reset.ops->reset_service, hns);
		return -EAGAIN;
	}
	if (hw->reset.stage == RESET_STAGE_RESTORE) {
		rte_spinlock_lock(&hw->lock);
		ret = hw->reset.ops->restore_conf(hns);
		rte_spinlock_unlock(&hw->lock);
		gettimeofday(&tv, NULL);
		if (ret) {
			hns3_warn(hw,
				  "Reset step6 restore fail=%d retries=%d",
				  ret, hw->reset.retries);
			goto err;
		}
		hns3_warn(hw, "Reset step6 restore success time=%ld.%.6ld",
			  tv.tv_sec, tv.tv_usec);
		hw->reset.retries = 0;
		hw->reset.stage = RESET_STAGE_DONE;
	}
	if (hw->reset.stage == RESET_STAGE_DONE) {
		/* IMP will wait ready flag before reset */
		hns3_notify_reset_ready(hw, false);
		hns3_clear_reset_level(hw, &hw->reset.pending);
		rte_atomic16_clear(&hns->hw.reset.resetting);
		hw->reset.attempts = 0;
		hw->reset.stats.success_cnt++;
		hw->reset.stage = RESET_STAGE_NONE;
		rte_spinlock_lock(&hw->lock);
		hw->reset.ops->start_service(hns);
		rte_spinlock_unlock(&hw->lock);
		gettimeofday(&tv, NULL);
		timersub(&tv, &hw->reset.start_time, &tv_delta);
		hns3_warn(hw, "%s reset done fail_cnt:%" PRIx64
			  " success_cnt:%" PRIx64 " global_cnt:%" PRIx64
			  " imp_cnt:%" PRIx64 " request_cnt:%" PRIx64
			  " exec_cnt:%" PRIx64 " merge_cnt:%" PRIx64,
			  reset_string[hw->reset.level],
			  hw->reset.stats.fail_cnt, hw->reset.stats.success_cnt,
			  hw->reset.stats.global_cnt, hw->reset.stats.imp_cnt,
			  hw->reset.stats.request_cnt, hw->reset.stats.exec_cnt,
			  hw->reset.stats.merge_cnt);
		hns3_warn(hw,
			  "%s reset done delta %ld ms time=%ld.%.6ld",
			  reset_string[hw->reset.level],
			  tv_delta.tv_sec * MSEC_PER_SEC +
			  tv_delta.tv_usec / USEC_PER_MSEC,
			  tv.tv_sec, tv.tv_usec);
		hw->reset.level = HNS3_NONE_RESET;
	}
	return 0;

err:
	if (ret == -ETIME) {
		hw->reset.retries++;
		if (hw->reset.retries < TIMEOUT_RETRIES_CNT) {
			rte_eal_alarm_set(HNS3_RESET_SYNC_US,
					  hw->reset.ops->reset_service, hns);
			return -EAGAIN;
		}
	}
	hw->reset.retries = 0;
	return -EIO;
}

/*
 * There are three scenarios as follows:
 * When the reset is not in progress, the reset process starts.
 * During the reset process, if the reset level has not changed,
 * the reset process continues; otherwise, the reset process is aborted.
 *	hw->reset.level   new_level          action
 *	HNS3_NONE_RESET	 HNS3_XXXX_RESET    start reset
 *	HNS3_XXXX_RESET  HNS3_XXXX_RESET    continue reset
 *	HNS3_LOW_RESET   HNS3_HIGH_RESET    abort
 */
int
hns3_reset_process(struct hns3_adapter *hns, enum hns3_reset_level new_level)
{
	struct hns3_hw *hw = &hns->hw;
	struct timeval tv_delta;
	struct timeval tv;
	int ret;

	if (hw->reset.level == HNS3_NONE_RESET) {
		hw->reset.level = new_level;
		hw->reset.stats.exec_cnt++;
		gettimeofday(&hw->reset.start_time, NULL);
		hns3_warn(hw, "Start %s reset time=%ld.%.6ld",
			  reset_string[hw->reset.level],
			  hw->reset.start_time.tv_sec,
			  hw->reset.start_time.tv_usec);
	}

	if (is_reset_pending(hns)) {
		gettimeofday(&tv, NULL);
		hns3_warn(hw,
			  "%s reset is aborted by high level time=%ld.%.6ld",
			  reset_string[hw->reset.level], tv.tv_sec, tv.tv_usec);
		if (hw->reset.wait_data->result == HNS3_WAIT_REQUEST)
			rte_eal_alarm_cancel(hns3_wait_callback,
					     hw->reset.wait_data);
		ret = -EBUSY;
		goto err;
	}

	ret = hns3_reset_pre(hns);
	if (ret)
		goto err;

	if (hw->reset.stage == RESET_STAGE_REQ_HW_RESET) {
		ret = hns3_reset_req_hw_reset(hns);
		if (ret == -EAGAIN)
			return ret;
		gettimeofday(&tv, NULL);
		hns3_warn(hw,
			  "Reset step3 request IMP reset success time=%ld.%.6ld",
			  tv.tv_sec, tv.tv_usec);
		hw->reset.stage = RESET_STAGE_WAIT;
		hw->reset.wait_data->result = HNS3_WAIT_UNKNOWN;
	}
	if (hw->reset.stage == RESET_STAGE_WAIT) {
		ret = hw->reset.ops->wait_hardware_ready(hns);
		if (ret)
			goto retry;
		gettimeofday(&tv, NULL);
		hns3_warn(hw, "Reset step4 reset wait success time=%ld.%.6ld",
			  tv.tv_sec, tv.tv_usec);
		hw->reset.stage = RESET_STAGE_DEV_INIT;
	}

	ret = hns3_reset_post(hns);
	if (ret)
		goto retry;

	return 0;
retry:
	if (ret == -EAGAIN)
		return ret;
err:
	hns3_clear_reset_level(hw, &hw->reset.pending);
	if (hns3_reset_err_handle(hns)) {
		hw->reset.stage = RESET_STAGE_PREWAIT;
		hns3_schedule_reset(hns);
	} else {
		rte_spinlock_lock(&hw->lock);
		if (hw->reset.mbuf_deferred_free) {
			hns3_dev_release_mbufs(hns);
			hw->reset.mbuf_deferred_free = false;
		}
		rte_spinlock_unlock(&hw->lock);
		rte_atomic16_clear(&hns->hw.reset.resetting);
		hw->reset.stage = RESET_STAGE_NONE;
		gettimeofday(&tv, NULL);
		timersub(&tv, &hw->reset.start_time, &tv_delta);
		hns3_warn(hw, "%s reset fail delta %ld ms time=%ld.%.6ld",
			  reset_string[hw->reset.level],
			  tv_delta.tv_sec * MSEC_PER_SEC +
			  tv_delta.tv_usec / USEC_PER_MSEC,
			  tv.tv_sec, tv.tv_usec);
		hw->reset.level = HNS3_NONE_RESET;
	}

	return -EIO;
}

/*
 * The reset process can only be terminated after handshake with IMP(step3),
 * so that IMP can complete the reset process normally.
 */
void
hns3_reset_abort(struct hns3_adapter *hns)
{
	struct hns3_hw *hw = &hns->hw;
	struct timeval tv;
	int i;

	for (i = 0; i < HNS3_QUIT_RESET_CNT; i++) {
		if (hw->reset.level == HNS3_NONE_RESET)
			break;
		rte_delay_ms(HNS3_QUIT_RESET_DELAY_MS);
	}

	/* IMP no longer waiting the ready flag */
	hns3_notify_reset_ready(hw, true);

	rte_eal_alarm_cancel(hw->reset.ops->reset_service, hns);
	rte_eal_alarm_cancel(hns3_wait_callback, hw->reset.wait_data);

	if (hw->reset.level != HNS3_NONE_RESET) {
		gettimeofday(&tv, NULL);
		hns3_err(hw, "Failed to terminate reset: %s time=%ld.%.6ld",
			 reset_string[hw->reset.level], tv.tv_sec, tv.tv_usec);
	}
}
