/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Huawei Technologies Co., Ltd
 */

#include "hinic3_compat.h"
#include "hinic3_cmd.h"
#include "hinic3_cmdq.h"
#include "hinic3_hw_comm.h"
#include "hinic3_hwdev.h"
#include "hinic3_hwif.h"
#include "hinic3_mgmt.h"
#include "hinic3_wq.h"
#include "hinic3_nic_cfg.h"

/* Buffer sizes in hinic3_convert_rx_buf_size must be in ascending order. */
const uint32_t hinic3_hw_rx_buf_size[] = {
	HINIC3_RX_BUF_SIZE_32B,
	HINIC3_RX_BUF_SIZE_64B,
	HINIC3_RX_BUF_SIZE_96B,
	HINIC3_RX_BUF_SIZE_128B,
	HINIC3_RX_BUF_SIZE_192B,
	HINIC3_RX_BUF_SIZE_256B,
	HINIC3_RX_BUF_SIZE_384B,
	HINIC3_RX_BUF_SIZE_512B,
	HINIC3_RX_BUF_SIZE_768B,
	HINIC3_RX_BUF_SIZE_1K,
	HINIC3_RX_BUF_SIZE_1_5K,
	HINIC3_RX_BUF_SIZE_2K,
	HINIC3_RX_BUF_SIZE_3K,
	HINIC3_RX_BUF_SIZE_4K,
	HINIC3_RX_BUF_SIZE_8K,
	HINIC3_RX_BUF_SIZE_16K,
};

int
hinic3_get_interrupt_cfg(struct hinic3_hwdev *hwdev, struct interrupt_info *info)
{
	struct hinic3_cmd_msix_config msix_cfg;
	uint16_t out_size = sizeof(msix_cfg);
	int err;

	if (!hwdev || !info)
		return -EINVAL;

	memset(&msix_cfg, 0, sizeof(msix_cfg));
	msix_cfg.func_id = hinic3_global_func_id(hwdev);
	msix_cfg.msix_index = info->msix_index;
	msix_cfg.opcode = HINIC3_MGMT_CMD_OP_GET;

	err = hinic3_msg_to_mgmt_sync(hwdev, HINIC3_MOD_COMM,
				      HINIC3_MGMT_CMD_CFG_MSIX_CTRL_REG,
				      &msix_cfg, sizeof(msix_cfg),
				      &msix_cfg, &out_size);
	if (err || !out_size || msix_cfg.status) {
		PMD_DRV_LOG(ERR,
			    "Get interrupt config failed, err: %d, status: 0x%x, out size: 0x%x",
			    err, msix_cfg.status, out_size);
		return -EINVAL;
	}

	info->lli_credit_limit = msix_cfg.lli_credit_cnt;
	info->lli_timer_cfg = msix_cfg.lli_tmier_cnt;
	info->pending_limt = msix_cfg.pending_cnt;
	info->coalesce_timer_cfg = msix_cfg.coalesce_timer_cnt;
	info->resend_timer_cfg = msix_cfg.resend_timer_cnt;

	return 0;
}

int
hinic3_set_interrupt_cfg(struct hinic3_hwdev *hwdev, struct interrupt_info info)
{
	struct hinic3_cmd_msix_config msix_cfg;
	struct interrupt_info temp_info;
	uint16_t out_size = sizeof(msix_cfg);
	int err;

	if (!hwdev)
		return -EINVAL;

	temp_info.msix_index = info.msix_index;
	err = hinic3_get_interrupt_cfg(hwdev, &temp_info);
	if (err)
		return -EIO;

	memset(&msix_cfg, 0, sizeof(msix_cfg));
	msix_cfg.func_id = hinic3_global_func_id(hwdev);
	msix_cfg.msix_index = info.msix_index;
	msix_cfg.opcode = HINIC3_MGMT_CMD_OP_SET;

	msix_cfg.lli_credit_cnt = temp_info.lli_credit_limit;
	msix_cfg.lli_tmier_cnt = temp_info.lli_timer_cfg;
	msix_cfg.pending_cnt = temp_info.pending_limt;
	msix_cfg.coalesce_timer_cnt = temp_info.coalesce_timer_cfg;
	msix_cfg.resend_timer_cnt = temp_info.resend_timer_cfg;

	if (info.lli_set) {
		msix_cfg.lli_credit_cnt = info.lli_credit_limit;
		msix_cfg.lli_tmier_cnt = info.lli_timer_cfg;
	}

	if (info.interrupt_coalesce_set) {
		msix_cfg.pending_cnt = info.pending_limt;
		msix_cfg.coalesce_timer_cnt = info.coalesce_timer_cfg;
		msix_cfg.resend_timer_cnt = info.resend_timer_cfg;
	}

	err = hinic3_msg_to_mgmt_sync(hwdev, HINIC3_MOD_COMM,
				      HINIC3_MGMT_CMD_CFG_MSIX_CTRL_REG,
				      &msix_cfg, sizeof(msix_cfg),
				      &msix_cfg, &out_size);
	if (err || !out_size || msix_cfg.status) {
		PMD_DRV_LOG(ERR,
			    "Set interrupt config failed, err: %d, status: 0x%x, out size: 0x%x",
			    err, msix_cfg.status, out_size);
		return -EIO;
	}

	return 0;
}

int
hinic3_set_wq_page_size(struct hinic3_hwdev *hwdev, uint16_t func_idx, uint32_t page_size)
{
	struct hinic3_cmd_wq_page_size page_size_info;
	uint16_t out_size = sizeof(page_size_info);
	int err;

	memset(&page_size_info, 0, sizeof(page_size_info));
	page_size_info.func_idx = func_idx;
	page_size_info.page_size = HINIC3_PAGE_SIZE_HW(page_size);
	page_size_info.opcode = HINIC3_MGMT_CMD_OP_SET;

	err = hinic3_msg_to_mgmt_sync(hwdev, HINIC3_MOD_COMM,
				      HINIC3_MGMT_CMD_CFG_PAGESIZE,
				      &page_size_info, sizeof(page_size_info),
				      &page_size_info, &out_size);
	if (err || !out_size || page_size_info.status) {
		PMD_DRV_LOG(ERR,
			    "Set wq page size failed, err: %d, status: 0x%x, out_size: 0x%0x",
			    err, page_size_info.status, out_size);
		return -EFAULT;
	}

	return 0;
}

int
hinic3_func_reset(struct hinic3_hwdev *hwdev, uint64_t reset_flag)
{
	struct hinic3_reset func_reset;
	struct hinic3_hwif *hwif = hwdev->hwif;
	uint16_t out_size = sizeof(func_reset);
	int err = 0;

	PMD_DRV_LOG(DEBUG, "Function is reset");

	memset(&func_reset, 0, sizeof(func_reset));
	func_reset.func_id = HINIC3_HWIF_GLOBAL_IDX(hwif);
	func_reset.reset_flag = reset_flag;
	err = hinic3_msg_to_mgmt_sync(hwdev, HINIC3_MOD_COMM,
				      HINIC3_MGMT_CMD_FUNC_RESET,
				      &func_reset, sizeof(func_reset),
				      &func_reset, &out_size);
	if (err || !out_size || func_reset.status) {
		PMD_DRV_LOG(ERR,
			    "Reset func resources failed, err: %d, status: 0x%x, out_size: 0x%x",
			    err, func_reset.status, out_size);
		return -EIO;
	}

	return 0;
}

int
hinic3_set_func_svc_used_state(struct hinic3_hwdev *hwdev, uint16_t svc_type, uint8_t state)
{
	struct comm_cmd_func_svc_used_state used_state;
	uint16_t out_size = sizeof(used_state);
	int err;

	if (!hwdev)
		return -EINVAL;

	memset(&used_state, 0, sizeof(used_state));
	used_state.func_id = hinic3_global_func_id(hwdev);
	used_state.svc_type = svc_type;
	used_state.used_state = state;

	err = hinic3_msg_to_mgmt_sync(hwdev, HINIC3_MOD_COMM,
				      HINIC3_MGMT_CMD_SET_FUNC_SVC_USED_STATE,
				      &used_state, sizeof(used_state),
				      &used_state, &out_size);
	if (err || !out_size || used_state.status) {
		PMD_DRV_LOG(ERR,
			    "Failed to set func service used state, err: %d, status: 0x%x, out size: 0x%x",
			    err, used_state.status, out_size);
		return -EIO;
	}

	return 0;
}

int
hinic3_convert_rx_buf_size(uint32_t rx_buf_sz, uint32_t *match_sz)
{
	uint32_t i, num_hw_types, best_match_sz;

	if (unlikely(!match_sz || rx_buf_sz < HINIC3_RX_BUF_SIZE_32B))
		return -EINVAL;

	if (rx_buf_sz >= HINIC3_RX_BUF_SIZE_16K) {
		best_match_sz = HINIC3_RX_BUF_SIZE_16K;
		goto size_matched;
	}

	if (rx_buf_sz >= HINIC3_RX_BUF_SIZE_4K) {
		best_match_sz =
			((rx_buf_sz >> RX_BUF_SIZE_1K_LEN) << RX_BUF_SIZE_1K_LEN);
		goto size_matched;
	}

	num_hw_types = RTE_DIM(hinic3_hw_rx_buf_size);
	best_match_sz = hinic3_hw_rx_buf_size[0];
	for (i = 0; i < num_hw_types; i++) {
		if (rx_buf_sz == hinic3_hw_rx_buf_size[i]) {
			best_match_sz = hinic3_hw_rx_buf_size[i];
			break;
		} else if (rx_buf_sz < hinic3_hw_rx_buf_size[i]) {
			break;
		}
		best_match_sz = hinic3_hw_rx_buf_size[i];
	}

size_matched:
	*match_sz = best_match_sz;

	return 0;
}

static uint16_t
get_hw_rx_buf_size(uint32_t rx_buf_sz)
{
	uint16_t num_hw_types = RTE_DIM(hinic3_hw_rx_buf_size);
	uint16_t i;

	for (i = 0; i < num_hw_types; i++) {
		if (hinic3_hw_rx_buf_size[i] == rx_buf_sz)
			return i;
	}

	PMD_DRV_LOG(WARNING, "Chip can't support rx buf size of %d", rx_buf_sz);

	return DEFAULT_RX_BUF_SIZE; /**< Default 2K. */
}

int
hinic3_set_root_ctxt(struct hinic3_hwdev *hwdev, uint32_t rq_depth,
		     uint32_t sq_depth, uint16_t rx_buf_sz)
{
	struct hinic3_cmd_root_ctxt root_ctxt;
	uint16_t out_size = sizeof(root_ctxt);
	int err;

	if (!hwdev)
		return -EINVAL;

	memset(&root_ctxt, 0, sizeof(root_ctxt));
	root_ctxt.func_idx = hinic3_global_func_id(hwdev);
	root_ctxt.set_cmdq_depth = 0;
	root_ctxt.cmdq_depth = 0;
	root_ctxt.lro_en = 1;
	root_ctxt.rq_depth = (uint16_t)rte_log2_u32(rq_depth);
	root_ctxt.rx_buf_sz = get_hw_rx_buf_size(rx_buf_sz);
	root_ctxt.sq_depth = (uint16_t)rte_log2_u32(sq_depth);

	err = hinic3_msg_to_mgmt_sync(hwdev, HINIC3_MOD_COMM,
				      HINIC3_MGMT_CMD_SET_VAT,
				      &root_ctxt, sizeof(root_ctxt),
				      &root_ctxt, &out_size);
	if (err || !out_size || root_ctxt.status) {
		PMD_DRV_LOG(ERR,
			    "Set root context failed, err: %d, status: 0x%x, out_size: 0x%x",
			    err, root_ctxt.status, out_size);
		return -EFAULT;
	}

	return 0;
}

int
hinic3_clean_root_ctxt(struct hinic3_hwdev *hwdev)
{
	struct hinic3_cmd_root_ctxt root_ctxt;
	uint16_t out_size = sizeof(root_ctxt);
	int err;

	if (!hwdev)
		return -EINVAL;

	memset(&root_ctxt, 0, sizeof(root_ctxt));
	root_ctxt.func_idx = hinic3_global_func_id(hwdev);

	err = hinic3_msg_to_mgmt_sync(hwdev, HINIC3_MOD_COMM,
				      HINIC3_MGMT_CMD_SET_VAT,
				      &root_ctxt, sizeof(root_ctxt),
				      &root_ctxt, &out_size);
	if (err || !out_size || root_ctxt.status) {
		PMD_DRV_LOG(ERR,
			    "Clean root context failed, err: %d, status: 0x%x, out_size: 0x%x",
			    err, root_ctxt.status, out_size);
		return -EFAULT;
	}

	return 0;
}

int
hinic3_set_cmdq_depth(struct hinic3_hwdev *hwdev, uint16_t cmdq_depth)
{
	struct hinic3_cmd_root_ctxt root_ctxt;
	uint16_t out_size = sizeof(root_ctxt);
	int err;

	memset(&root_ctxt, 0, sizeof(root_ctxt));
	root_ctxt.func_idx = hinic3_global_func_id(hwdev);
	root_ctxt.set_cmdq_depth = 1;
	root_ctxt.cmdq_depth = (uint8_t)rte_log2_u32(cmdq_depth);

	err = hinic3_msg_to_mgmt_sync(hwdev, HINIC3_MOD_COMM,
				      HINIC3_MGMT_CMD_SET_VAT,
				      &root_ctxt, sizeof(root_ctxt),
				      &root_ctxt, &out_size);
	if (err || !out_size || root_ctxt.status) {
		PMD_DRV_LOG(ERR,
			    "Set cmdq depth failed, err: %d, status: 0x%x, out_size: 0x%x",
			    err, root_ctxt.status, out_size);
		return -EFAULT;
	}

	return 0;
}

int
hinic3_get_mgmt_version(struct hinic3_hwdev *hwdev, char *mgmt_ver, int max_mgmt_len)
{
	struct hinic3_cmd_get_fw_version fw_ver;
	uint16_t out_size = sizeof(fw_ver);
	int err;

	if (!hwdev || !mgmt_ver)
		return -EINVAL;

	memset(&fw_ver, 0, sizeof(fw_ver));
	fw_ver.fw_type = HINIC3_FW_VER_TYPE_MPU;

	err = hinic3_msg_to_mgmt_sync(hwdev, HINIC3_MOD_COMM,
				      HINIC3_MGMT_CMD_GET_FW_VERSION,
				      &fw_ver, sizeof(fw_ver), &fw_ver, &out_size);
	if (MSG_TO_MGMT_SYNC_RETURN_ERR(err, out_size, fw_ver.status)) {
		PMD_DRV_LOG(ERR,
			    "Get mgmt version failed, err: %d, status: 0x%x, out size: 0x%x",
			    err, fw_ver.status, out_size);
		return -EIO;
	}

	strlcpy(mgmt_ver, (char *)fw_ver.ver, max_mgmt_len);
	return 0;
}

int
hinic3_get_board_info(struct hinic3_hwdev *hwdev, struct hinic3_board_info *info)
{
	struct hinic3_cmd_board_info board_info;
	uint16_t out_size = sizeof(board_info);
	int err;

	if (!hwdev || !info)
		return -EINVAL;

	memset(&board_info, 0, sizeof(board_info));
	err = hinic3_msg_to_mgmt_sync(hwdev, HINIC3_MOD_COMM,
				      HINIC3_MGMT_CMD_GET_BOARD_INFO,
				      &board_info, sizeof(board_info),
				      &board_info, &out_size);
	if (err || board_info.status || !out_size) {
		PMD_DRV_LOG(ERR,
			    "Get board info failed, err: %d, status: 0x%x, out size: 0x%x",
			    err, board_info.status, out_size);
		return -EFAULT;
	}

	*info = board_info.info;

	return 0;
}

static int
hinic3_comm_features_nego(struct hinic3_hwdev *hwdev,
			  uint8_t opcode, uint64_t *s_feature, uint16_t size)
{
	struct comm_cmd_feature_nego feature_nego;
	uint16_t out_size = sizeof(feature_nego);
	int err;

	if (!hwdev || !s_feature || size > COMM_MAX_FEATURE_QWORD)
		return -EINVAL;

	memset(&feature_nego, 0, sizeof(feature_nego));
	feature_nego.func_id = hinic3_global_func_id(hwdev);
	feature_nego.opcode = opcode;
	if (opcode == MGMT_MSG_CMD_OP_SET)
		memcpy(feature_nego.s_feature, s_feature, (size * sizeof(uint64_t)));

	err = hinic3_msg_to_mgmt_sync(hwdev, HINIC3_MOD_COMM,
				      HINIC3_MGMT_CMD_FEATURE_NEGO,
				      &feature_nego, sizeof(feature_nego),
				      &feature_nego, &out_size);
	if (err || !out_size || feature_nego.head.status) {
		PMD_DRV_LOG(ERR,
			    "Failed to negotiate feature, err: %d, status: 0x%x, out size: 0x%x",
			    err, feature_nego.head.status, out_size);
		return -EINVAL;
	}

	if (opcode == MGMT_MSG_CMD_OP_GET)
		memcpy(s_feature, feature_nego.s_feature, (size * sizeof(uint64_t)));

	return 0;
}

int
hinic3_get_comm_features(struct hinic3_hwdev *hwdev, uint64_t *s_feature, uint16_t size)
{
	return hinic3_comm_features_nego(hwdev, MGMT_MSG_CMD_OP_GET, s_feature, size);
}

int
hinic3_set_comm_features(struct hinic3_hwdev *hwdev, uint64_t *s_feature, uint16_t size)
{
	return hinic3_comm_features_nego(hwdev, MGMT_MSG_CMD_OP_SET, s_feature, size);
}
