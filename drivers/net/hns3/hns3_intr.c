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
