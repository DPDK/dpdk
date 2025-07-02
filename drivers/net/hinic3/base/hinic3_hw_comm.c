/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Huawei Technologies Co., Ltd
 */

#include <rte_bus_pci.h>
#include <rte_hash.h>
#include <rte_jhash.h>

#include "hinic3_compat.h"
#include "hinic3_cmd.h"
#include "hinic3_cmdq.h"
#include "hinic3_hw_comm.h"
#include "hinic3_hwdev.h"
#include "hinic3_hwif.h"
#include "hinic3_mgmt.h"
#include "hinic3_wq.h"

/* Buffer sizes in hinic3_convert_rx_buf_size must be in ascending order. */
const u32 hinic3_hw_rx_buf_size[] = {
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
hinic3_get_interrupt_cfg(void *dev, struct interrupt_info *info)
{
	struct hinic3_hwdev *hwdev = dev;
	struct hinic3_cmd_msix_config msix_cfg;
	u16 out_size = sizeof(msix_cfg);
	int err;

	if (!hwdev || !info)
		return -EINVAL;

	memset(&msix_cfg, 0, sizeof(msix_cfg));
	msix_cfg.func_id = hinic3_global_func_id(hwdev);
	msix_cfg.msix_index = info->msix_index;
	msix_cfg.opcode = HINIC3_MGMT_CMD_OP_GET;

	err = hinic3_msg_to_mgmt_sync(hwdev,
		HINIC3_MOD_COMM, HINIC3_MGMT_CMD_CFG_MSIX_CTRL_REG,
		&msix_cfg, sizeof(msix_cfg), &msix_cfg, &out_size, 0);
	if (err || !out_size || msix_cfg.status) {
		PMD_DRV_LOG(ERR,
			    "Get interrupt config failed, "
			    "err: %d, status: 0x%x, out size: 0x%x",
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
hinic3_set_interrupt_cfg(void *dev, struct interrupt_info info)
{
	struct hinic3_hwdev *hwdev = dev;
	struct hinic3_cmd_msix_config msix_cfg;
	struct interrupt_info temp_info;
	u16 out_size = sizeof(msix_cfg);
	int err;

	if (!hwdev)
		return -EINVAL;

	temp_info.msix_index = info.msix_index;
	err = hinic3_get_interrupt_cfg(hwdev, &temp_info);
	if (err)
		return -EIO;

	memset(&msix_cfg, 0, sizeof(msix_cfg));
	msix_cfg.func_id = hinic3_global_func_id(hwdev);
	msix_cfg.msix_index = (u16)info.msix_index;
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

	err = hinic3_msg_to_mgmt_sync(hwdev,
		HINIC3_MOD_COMM, HINIC3_MGMT_CMD_CFG_MSIX_CTRL_REG,
		&msix_cfg, sizeof(msix_cfg), &msix_cfg, &out_size, 0);
	if (err || !out_size || msix_cfg.status) {
		PMD_DRV_LOG(ERR,
			    "Set interrupt config failed, "
			    "err: %d, status: 0x%x, out size: 0x%x",
			    err, msix_cfg.status, out_size);
		return -EIO;
	}

	return 0;
}

int
hinic3_set_wq_page_size(void *hwdev, u16 func_idx, u32 page_size)
{
	struct hinic3_cmd_wq_page_size page_size_info;
	u16 out_size = sizeof(page_size_info);
	int err;

	memset(&page_size_info, 0, sizeof(page_size_info));
	page_size_info.func_idx = func_idx;
	page_size_info.page_size = HINIC3_PAGE_SIZE_HW(page_size);
	page_size_info.opcode = HINIC3_MGMT_CMD_OP_SET;

	err = hinic3_msg_to_mgmt_sync(hwdev, HINIC3_MOD_COMM,
				      HINIC3_MGMT_CMD_CFG_PAGESIZE,
				      &page_size_info, sizeof(page_size_info),
				      &page_size_info, &out_size, 0);
	if (err || !out_size || page_size_info.status) {
		PMD_DRV_LOG(ERR,
			    "Set wq page size failed, "
			    "err: %d, status: 0x%x, out_size: 0x%0x",
			    err, page_size_info.status, out_size);
		return -EFAULT;
	}

	return 0;
}

int
hinic3_func_reset(void *hwdev, u64 reset_flag)
{
	struct hinic3_reset func_reset;
	struct hinic3_hwif *hwif = ((struct hinic3_hwdev *)hwdev)->hwif;
	u16 out_size = sizeof(func_reset);
	int err = 0;

	PMD_DRV_LOG(INFO, "Function is reset");

	memset(&func_reset, 0, sizeof(func_reset));
	func_reset.func_id = HINIC3_HWIF_GLOBAL_IDX(hwif);
	func_reset.reset_flag = reset_flag;
	err = hinic3_msg_to_mgmt_sync(hwdev,
		HINIC3_MOD_COMM, HINIC3_MGMT_CMD_FUNC_RESET, &func_reset,
		sizeof(func_reset), &func_reset, &out_size, 0);
	if (err || !out_size || func_reset.status) {
		PMD_DRV_LOG(ERR,
			    "Reset func resources failed, "
			    "err: %d, status: 0x%x, out_size: 0x%x",
			    err, func_reset.status, out_size);
		return -EIO;
	}

	return 0;
}

int
hinic3_set_func_svc_used_state(void *hwdev, u16 svc_type, u8 state)
{
	struct comm_cmd_func_svc_used_state used_state;
	u16 out_size = sizeof(used_state);
	int err;

	if (!hwdev)
		return -EINVAL;

	memset(&used_state, 0, sizeof(used_state));
	used_state.func_id = hinic3_global_func_id(hwdev);
	used_state.svc_type = svc_type;
	used_state.used_state = state;

	err = hinic3_msg_to_mgmt_sync(hwdev,
		HINIC3_MOD_COMM, HINIC3_MGMT_CMD_SET_FUNC_SVC_USED_STATE,
		&used_state, sizeof(used_state), &used_state, &out_size, 0);
	if (err || !out_size || used_state.status) {
		PMD_DRV_LOG(ERR,
			    "Failed to set func service used state, "
			    "err: %d, status: 0x%x, out size: 0x%x",
			    err, used_state.status, out_size);
		return -EIO;
	}

	return 0;
}

int
hinic3_convert_rx_buf_size(u32 rx_buf_sz, u32 *match_sz)
{
	u32 i, num_hw_types, best_match_sz;

	if (unlikely(!match_sz || rx_buf_sz < HINIC3_RX_BUF_SIZE_32B))
		return -EINVAL;

	if (rx_buf_sz >= HINIC3_RX_BUF_SIZE_16K) {
		best_match_sz = HINIC3_RX_BUF_SIZE_16K;
		goto size_matched;
	}

	if (rx_buf_sz >= HINIC3_RX_BUF_SIZE_4K) {
		best_match_sz = ((rx_buf_sz >> RX_BUF_SIZE_1K_LEN)
				 << RX_BUF_SIZE_1K_LEN);
		goto size_matched;
	}

	num_hw_types = sizeof(hinic3_hw_rx_buf_size) /
		       sizeof(hinic3_hw_rx_buf_size[0]);
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

static u16
get_hw_rx_buf_size(u32 rx_buf_sz)
{
	u16 num_hw_types = sizeof(hinic3_hw_rx_buf_size) /
			   sizeof(hinic3_hw_rx_buf_size[0]);
	u16 i;

	for (i = 0; i < num_hw_types; i++) {
		if (hinic3_hw_rx_buf_size[i] == rx_buf_sz)
			return i;
	}

	PMD_DRV_LOG(WARNING, "Chip can't support rx buf size of %d", rx_buf_sz);

	return DEFAULT_RX_BUF_SIZE; /**< Default 2K. */
}

int
hinic3_set_root_ctxt(void *hwdev, u32 rq_depth, u32 sq_depth, u16 rx_buf_sz)
{
	struct hinic3_cmd_root_ctxt root_ctxt;
	u16 out_size = sizeof(root_ctxt);
	int err;

	if (!hwdev)
		return -EINVAL;

	memset(&root_ctxt, 0, sizeof(root_ctxt));
	root_ctxt.func_idx = hinic3_global_func_id(hwdev);
	root_ctxt.set_cmdq_depth = 0;
	root_ctxt.cmdq_depth = 0;
	root_ctxt.lro_en = 1;
	root_ctxt.rq_depth = (u16)ilog2(rq_depth);
	root_ctxt.rx_buf_sz = get_hw_rx_buf_size(rx_buf_sz);
	root_ctxt.sq_depth = (u16)ilog2(sq_depth);

	err = hinic3_msg_to_mgmt_sync(hwdev,
		HINIC3_MOD_COMM, HINIC3_MGMT_CMD_SET_VAT, &root_ctxt,
		sizeof(root_ctxt), &root_ctxt, &out_size, 0);
	if (err || !out_size || root_ctxt.status) {
		PMD_DRV_LOG(ERR,
			    "Set root context failed, "
			    "err: %d, status: 0x%x, out_size: 0x%x",
			    err, root_ctxt.status, out_size);
		return -EFAULT;
	}

	return 0;
}

int
hinic3_clean_root_ctxt(void *hwdev)
{
	struct hinic3_cmd_root_ctxt root_ctxt;
	u16 out_size = sizeof(root_ctxt);
	int err;

	if (!hwdev)
		return -EINVAL;

	memset(&root_ctxt, 0, sizeof(root_ctxt));
	root_ctxt.func_idx = hinic3_global_func_id(hwdev);

	err = hinic3_msg_to_mgmt_sync(hwdev,
		HINIC3_MOD_COMM, HINIC3_MGMT_CMD_SET_VAT, &root_ctxt,
		sizeof(root_ctxt), &root_ctxt, &out_size, 0);
	if (err || !out_size || root_ctxt.status) {
		PMD_DRV_LOG(ERR,
			    "Clean root context failed, "
			    "err: %d, status: 0x%x, out_size: 0x%x",
			    err, root_ctxt.status, out_size);
		return -EFAULT;
	}

	return 0;
}

int
hinic3_set_cmdq_depth(void *hwdev, u16 cmdq_depth)
{
	struct hinic3_cmd_root_ctxt root_ctxt;
	u16 out_size = sizeof(root_ctxt);
	int err;

	memset(&root_ctxt, 0, sizeof(root_ctxt));
	root_ctxt.func_idx = hinic3_global_func_id(hwdev);
	root_ctxt.set_cmdq_depth = 1;
	root_ctxt.cmdq_depth = (u8)ilog2(cmdq_depth);

	err = hinic3_msg_to_mgmt_sync(hwdev,
		HINIC3_MOD_COMM, HINIC3_MGMT_CMD_SET_VAT, &root_ctxt,
		sizeof(root_ctxt), &root_ctxt, &out_size, 0);
	if (err || !out_size || root_ctxt.status) {
		PMD_DRV_LOG(ERR,
			    "Set cmdq depth failed, "
			    "err: %d, status: 0x%x, out_size: 0x%x",
			    err, root_ctxt.status, out_size);
		return -EFAULT;
	}

	return 0;
}

int
hinic3_get_mgmt_version(void *hwdev, char *mgmt_ver, int max_mgmt_len)
{
	struct hinic3_cmd_get_fw_version fw_ver;
	u16 out_size = sizeof(fw_ver);
	int err;

	if (!hwdev || !mgmt_ver)
		return -EINVAL;

	memset(&fw_ver, 0, sizeof(fw_ver));
	fw_ver.fw_type = HINIC3_FW_VER_TYPE_MPU;

	err = hinic3_msg_to_mgmt_sync(hwdev, HINIC3_MOD_COMM,
				      HINIC3_MGMT_CMD_GET_FW_VERSION, &fw_ver,
				      sizeof(fw_ver), &fw_ver, &out_size, 0);
	if (MSG_TO_MGMT_SYNC_RETURN_ERR(err, out_size, fw_ver.status)) {
		PMD_DRV_LOG(ERR,
			    "Get mgmt version failed, "
			    "err: %d, status: 0x%x, out size: 0x%x",
			    err, fw_ver.status, out_size);
		return -EIO;
	}

	snprintf(mgmt_ver, max_mgmt_len, "%s", fw_ver.ver);
	return 0;
}

int
hinic3_get_board_info(void *hwdev, struct hinic3_board_info *info)
{
	struct hinic3_cmd_board_info board_info;
	u16 out_size = sizeof(board_info);
	int err;

	if (!hwdev || !info)
		return -EINVAL;

	memset(&board_info, 0, sizeof(board_info));
	err = hinic3_msg_to_mgmt_sync(hwdev,
		HINIC3_MOD_COMM, HINIC3_MGMT_CMD_GET_BOARD_INFO,
		&board_info, sizeof(board_info), &board_info, &out_size, 0);
	if (err || board_info.status || !out_size) {
		PMD_DRV_LOG(ERR,
			    "Get board info failed, "
			    "err: %d, status: 0x%x, out size: 0x%x",
			    err, board_info.status, out_size);
		return -EFAULT;
	}

	memcpy(info, &board_info.info, sizeof(*info));

	return 0;
}

static int
hinic3_comm_features_nego(void *hwdev, u8 opcode, u64 *s_feature, u16 size)
{
	struct comm_cmd_feature_nego feature_nego;
	u16 out_size = sizeof(feature_nego);
	int err;

	if (!hwdev || !s_feature || size > COMM_MAX_FEATURE_QWORD)
		return -EINVAL;

	memset(&feature_nego, 0, sizeof(feature_nego));
	feature_nego.func_id = hinic3_global_func_id(hwdev);
	feature_nego.opcode = opcode;
	if (opcode == MGMT_MSG_CMD_OP_SET)
		memcpy(feature_nego.s_feature, s_feature, (size * sizeof(u64)));

	err = hinic3_msg_to_mgmt_sync(hwdev, HINIC3_MOD_COMM,
				      HINIC3_MGMT_CMD_FEATURE_NEGO,
				      &feature_nego, sizeof(feature_nego),
				      &feature_nego, &out_size, 0);
	if (err || !out_size || feature_nego.head.status) {
		PMD_DRV_LOG(ERR,
			    "Failed to negotiate feature, "
			    "err: %d, status: 0x%x, out size: 0x%x",
			    err, feature_nego.head.status, out_size);
		return -EINVAL;
	}

	if (opcode == MGMT_MSG_CMD_OP_GET)
		memcpy(s_feature, feature_nego.s_feature, (size * sizeof(u64)));

	return 0;
}

int
hinic3_get_comm_features(void *hwdev, u64 *s_feature, u16 size)
{
	return hinic3_comm_features_nego(hwdev, MGMT_MSG_CMD_OP_GET, s_feature,
					 size);
}

int
hinic3_set_comm_features(void *hwdev, u64 *s_feature, u16 size)
{
	return hinic3_comm_features_nego(hwdev, MGMT_MSG_CMD_OP_SET, s_feature,
					 size);
}
