/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Huawei Technologies Co., Ltd
 */

#include <rte_bus_pci.h>
#include <rte_vfio.h>

#include "hinic3_compat.h"
#include "hinic3_cmd.h"
#include "hinic3_cmdq.h"
#include "hinic3_csr.h"
#include "hinic3_eqs.h"
#include "hinic3_hw_cfg.h"
#include "hinic3_hw_comm.h"
#include "hinic3_hwdev.h"
#include "hinic3_hwif.h"
#include "hinic3_mbox.h"
#include "hinic3_mgmt.h"
#include "hinic3_wq.h"

enum hinic3_pcie_nosnoop { HINIC3_PCIE_SNOOP = 0, HINIC3_PCIE_NO_SNOOP = 1 };

enum hinic3_pcie_tph {
	HINIC3_PCIE_TPH_DISABLE = 0,
	HINIC3_PCIE_TPH_ENABLE = 1
};

#define HINIC3_DMA_ATTR_INDIR_IDX_SHIFT 0

#define HINIC3_DMA_ATTR_INDIR_IDX_MASK 0x3FF

#define HINIC3_DMA_ATTR_INDIR_IDX_SET(val, member)            \
	(((u32)(val) & HINIC3_DMA_ATTR_INDIR_##member##_MASK) \
	 << HINIC3_DMA_ATTR_INDIR_##member##_SHIFT)

#define HINIC3_DMA_ATTR_INDIR_IDX_CLEAR(val, member)      \
	((val) & (~(HINIC3_DMA_ATTR_INDIR_##member##_MASK \
		    << HINIC3_DMA_ATTR_INDIR_##member##_SHIFT)))

#define HINIC3_DMA_ATTR_ENTRY_ST_SHIFT		0
#define HINIC3_DMA_ATTR_ENTRY_AT_SHIFT		8
#define HINIC3_DMA_ATTR_ENTRY_PH_SHIFT		10
#define HINIC3_DMA_ATTR_ENTRY_NO_SNOOPING_SHIFT 12
#define HINIC3_DMA_ATTR_ENTRY_TPH_EN_SHIFT	13

#define HINIC3_DMA_ATTR_ENTRY_ST_MASK	       0xFF
#define HINIC3_DMA_ATTR_ENTRY_AT_MASK	       0x3
#define HINIC3_DMA_ATTR_ENTRY_PH_MASK	       0x3
#define HINIC3_DMA_ATTR_ENTRY_NO_SNOOPING_MASK 0x1
#define HINIC3_DMA_ATTR_ENTRY_TPH_EN_MASK      0x1

#define HINIC3_DMA_ATTR_ENTRY_SET(val, member)                \
	(((u32)(val) & HINIC3_DMA_ATTR_ENTRY_##member##_MASK) \
	 << HINIC3_DMA_ATTR_ENTRY_##member##_SHIFT)

#define HINIC3_DMA_ATTR_ENTRY_CLEAR(val, member)          \
	((val) & (~(HINIC3_DMA_ATTR_ENTRY_##member##_MASK \
		    << HINIC3_DMA_ATTR_ENTRY_##member##_SHIFT)))

#define HINIC3_PCIE_ST_DISABLE 0
#define HINIC3_PCIE_AT_DISABLE 0
#define HINIC3_PCIE_PH_DISABLE 0

#define PCIE_MSIX_ATTR_ENTRY 0

#define HINIC3_CHIP_PRESENT 1
#define HINIC3_CHIP_ABSENT  0

#define HINIC3_DEFAULT_EQ_MSIX_PENDING_LIMIT	0
#define HINIC3_DEFAULT_EQ_MSIX_COALESCE_TIMER_CFG 0xFF
#define HINIC3_DEFAULT_EQ_MSIX_RESEND_TIMER_CFG	7

typedef void (*mgmt_event_cb)(void *handle, void *buf_in, u16 in_size,
			      void *buf_out, u16 *out_size);

struct mgmt_event_handle {
	u16 cmd;
	mgmt_event_cb proc;
};

bool
hinic3_is_vfio_iommu_enable(const struct rte_eth_dev *rte_dev)
{
	return ((RTE_ETH_DEV_TO_PCI(rte_dev)->kdrv == RTE_PCI_KDRV_VFIO) &&
		(rte_vfio_noiommu_is_enabled() != 1));
}

int
vf_handle_pf_comm_mbox(void *handle, __rte_unused void *pri_handle,
		       __rte_unused u16 cmd, __rte_unused void *buf_in,
		       __rte_unused u16 in_size, __rte_unused void *buf_out,
		       __rte_unused u16 *out_size)
{
	struct hinic3_hwdev *hwdev = handle;

	if (!hwdev)
		return -EINVAL;

	PMD_DRV_LOG(WARNING, "Unsupported pf mbox event %d to process", cmd);

	return 0;
}

static void
fault_event_handler(__rte_unused void *hwdev, __rte_unused void *buf_in,
		    __rte_unused u16 in_size, __rte_unused void *buf_out,
		    __rte_unused u16 *out_size)
{
	PMD_DRV_LOG(WARNING, "Unsupported fault event handler");
}

static void
ffm_event_msg_handler(__rte_unused void *hwdev, void *buf_in, u16 in_size,
		      __rte_unused void *buf_out, u16 *out_size)
{
	struct ffm_intr_info *intr = NULL;

	if (in_size != sizeof(*intr)) {
		PMD_DRV_LOG(ERR,
			    "Invalid fault event report, "
			    "length: %d, should be %zu",
			    in_size, sizeof(*intr));
		return;
	}

	intr = buf_in;

	PMD_DRV_LOG(ERR,
		    "node_id: 0x%x, err_type: 0x%x, err_level: %d, "
		    "err_csr_addr: 0x%08x, err_csr_value: 0x%08x",
		    intr->node_id, intr->err_type, intr->err_level,
		    intr->err_csr_addr, intr->err_csr_value);

	*out_size = sizeof(*intr);
}

static const struct mgmt_event_handle mgmt_event_proc[] = {
	{
		.cmd = HINIC3_MGMT_CMD_FAULT_REPORT,
		.proc = fault_event_handler,
	},

	{
		.cmd = HINIC3_MGMT_CMD_FFM_SET,
		.proc = ffm_event_msg_handler,
	},
};

void
pf_handle_mgmt_comm_event(void *handle, __rte_unused void *pri_handle, u16 cmd,
			  void *buf_in, u16 in_size, void *buf_out,
			  u16 *out_size)
{
	struct hinic3_hwdev *hwdev = handle;
	u32 i, event_num = RTE_DIM(mgmt_event_proc);

	if (!hwdev)
		return;

	for (i = 0; i < event_num; i++) {
		if (cmd == mgmt_event_proc[i].cmd) {
			if (mgmt_event_proc[i].proc)
				mgmt_event_proc[i].proc(handle, buf_in, in_size,
							buf_out, out_size);
			return;
		}
	}

	PMD_DRV_LOG(WARNING, "Unsupported mgmt cpu event %d to process", cmd);
}

static int
set_dma_attr_entry(__rte_unused struct hinic3_hwdev *hwdev,
		   __rte_unused u8 entry_idx, __rte_unused u8 st,
		   __rte_unused u8 at, __rte_unused u8 ph,
		   __rte_unused enum hinic3_pcie_nosnoop no_snooping,
		   __rte_unused enum hinic3_pcie_tph tph_en)
{
	struct hinic3_dma_attr_table attr;
	u16 out_size = sizeof(attr);
	int err;

	memset(&attr, 0, sizeof(attr));
	attr.func_id = hinic3_global_func_id(hwdev);
	attr.entry_idx = entry_idx;
	attr.st = st;
	attr.at = at;
	attr.ph = ph;
	attr.no_snooping = no_snooping;
	attr.tph_en = tph_en;

	err = hinic3_msg_to_mgmt_sync(hwdev, HINIC3_MOD_COMM,
				      HINIC3_MGMT_CMD_SET_DMA_ATTR, &attr,
				      sizeof(attr), &attr, &out_size, 0);
	if (err || !out_size || attr.head.status) {
		PMD_DRV_LOG(ERR,
			    "Set dma attribute failed, err: %d, status: 0x%x, "
			    "out_size: 0x%x",
			    err, attr.head.status, out_size);
		return -EIO;
	}

	return 0;
}

/**
 * Initialize the default dma attributes.
 *
 * @param[in] hwdev
 * Pointer to hardware device structure.
 *
 * 0 on success, non-zero on failure.
 */
static int
dma_attr_table_init(struct hinic3_hwdev *hwdev)
{
	return set_dma_attr_entry(hwdev,
		PCIE_MSIX_ATTR_ENTRY, HINIC3_PCIE_ST_DISABLE,
		HINIC3_PCIE_AT_DISABLE, HINIC3_PCIE_PH_DISABLE,
		HINIC3_PCIE_SNOOP, HINIC3_PCIE_TPH_DISABLE);
}

static int
init_aeqs_msix_attr(struct hinic3_hwdev *hwdev)
{
	struct hinic3_aeqs *aeqs = hwdev->aeqs;
	struct interrupt_info info = {0};
	struct hinic3_eq *eq = NULL;
	u16 q_id;
	int err;

	info.lli_set = 0;
	info.interrupt_coalesce_set = 1;
	info.pending_limt = HINIC3_DEFAULT_EQ_MSIX_PENDING_LIMIT;
	info.coalesce_timer_cfg = HINIC3_DEFAULT_EQ_MSIX_COALESCE_TIMER_CFG;
	info.resend_timer_cfg = HINIC3_DEFAULT_EQ_MSIX_RESEND_TIMER_CFG;

	for (q_id = 0; q_id < aeqs->num_aeqs; q_id++) {
		eq = &aeqs->aeq[q_id];
		info.msix_index = eq->eq_irq.msix_entry_idx;
		err = hinic3_set_interrupt_cfg(hwdev, info);
		if (err) {
			PMD_DRV_LOG(ERR, "Set msix attr for aeq %d failed",
				    q_id);
			return -EFAULT;
		}
	}

	return 0;
}

static int
hinic3_comm_pf_to_mgmt_init(struct hinic3_hwdev *hwdev)
{
	int err;

	/* VF does not support send msg to mgmt directly. */
	if (hinic3_func_type(hwdev) == TYPE_VF)
		return 0;

	err = hinic3_pf_to_mgmt_init(hwdev);
	if (err)
		return err;

	return 0;
}

static void
hinic3_comm_pf_to_mgmt_free(struct hinic3_hwdev *hwdev)
{
	/* VF does not support send msg to mgmt directly. */
	if (hinic3_func_type(hwdev) == TYPE_VF)
		return;

	hinic3_pf_to_mgmt_free(hwdev);
}

static int
hinic3_comm_cmdqs_init(struct hinic3_hwdev *hwdev)
{
	int err;

	err = hinic3_cmdqs_init(hwdev);
	if (err) {
		PMD_DRV_LOG(ERR, "Init cmd queues failed");
		return err;
	}

	err = hinic3_set_cmdq_depth(hwdev, HINIC3_CMDQ_DEPTH);
	if (err) {
		PMD_DRV_LOG(ERR, "Set cmdq depth failed");
		goto set_cmdq_depth_err;
	}

	return 0;

set_cmdq_depth_err:
	hinic3_cmdqs_free(hwdev);

	return err;
}

static void
hinic3_comm_cmdqs_free(struct hinic3_hwdev *hwdev)
{
	hinic3_cmdqs_free(hwdev);
}

static void
hinic3_sync_mgmt_func_state(struct hinic3_hwdev *hwdev)
{
	hinic3_set_pf_status(hwdev->hwif, HINIC3_PF_STATUS_ACTIVE_FLAG);
}

static int
get_func_misc_info(struct hinic3_hwdev *hwdev)
{
	int err;

	err = hinic3_get_board_info(hwdev, &hwdev->board_info);
	if (err) {
		/* For the PF/VF of secondary host, return error. */
		if (hinic3_pcie_itf_id(hwdev))
			return err;

		memset(&hwdev->board_info, 0xff,
		       sizeof(struct hinic3_board_info));
	}

	err = hinic3_get_mgmt_version(hwdev, hwdev->mgmt_ver,
				      HINIC3_MGMT_VERSION_MAX_LEN);
	if (err) {
		PMD_DRV_LOG(ERR, "Get mgmt cpu version failed");
		return err;
	}

	return 0;
}

static int
init_mgmt_channel(struct hinic3_hwdev *hwdev)
{
	int err;

	err = hinic3_aeqs_init(hwdev);
	if (err) {
		PMD_DRV_LOG(ERR, "Init async event queues failed");
		return err;
	}

	err = hinic3_comm_pf_to_mgmt_init(hwdev);
	if (err) {
		PMD_DRV_LOG(ERR, "Init mgmt channel failed");
		goto msg_init_err;
	}

	err = hinic3_func_to_func_init(hwdev);
	if (err) {
		PMD_DRV_LOG(ERR, "Init mailbox channel failed");
		goto func_to_func_init_err;
	}

	return 0;

func_to_func_init_err:
	hinic3_comm_pf_to_mgmt_free(hwdev);

msg_init_err:
	hinic3_aeqs_free(hwdev);

	return err;
}

static void
free_mgmt_channel(struct hinic3_hwdev *hwdev)
{
	hinic3_func_to_func_free(hwdev);
	hinic3_comm_pf_to_mgmt_free(hwdev);
	hinic3_aeqs_free(hwdev);
}

static int
init_cmdqs_channel(struct hinic3_hwdev *hwdev)
{
	int err;

	err = dma_attr_table_init(hwdev);
	if (err) {
		PMD_DRV_LOG(ERR, "Init dma attr table failed");
		goto dma_attr_init_err;
	}

	err = init_aeqs_msix_attr(hwdev);
	if (err)
		goto init_aeqs_msix_err;

	/* Set default wq page_size. */
	hwdev->wq_page_size = HINIC3_DEFAULT_WQ_PAGE_SIZE;
	err = hinic3_set_wq_page_size(hwdev, hinic3_global_func_id(hwdev),
				      hwdev->wq_page_size);
	if (err) {
		PMD_DRV_LOG(ERR, "Set wq page size failed");
		goto init_wq_pg_size_err;
	}

	err = hinic3_comm_cmdqs_init(hwdev);
	if (err) {
		PMD_DRV_LOG(ERR, "Init cmd queues failed");
		goto cmdq_init_err;
	}

	return 0;

cmdq_init_err:
	if (HINIC3_FUNC_TYPE(hwdev) != TYPE_VF)
		hinic3_set_wq_page_size(hwdev, hinic3_global_func_id(hwdev),
					HINIC3_HW_WQ_PAGE_SIZE);
init_wq_pg_size_err:
init_aeqs_msix_err:
dma_attr_init_err:

	return err;
}

static int
hinic3_init_comm_ch(struct hinic3_hwdev *hwdev)
{
	int err;

	err = init_mgmt_channel(hwdev);
	if (err) {
		PMD_DRV_LOG(ERR, "Init mgmt channel failed");
		return err;
	}

	err = get_func_misc_info(hwdev);
	if (err) {
		PMD_DRV_LOG(ERR, "Get function msic information failed");
		goto get_func_info_err;
	}

	err = hinic3_func_reset(hwdev, HINIC3_NIC_RES | HINIC3_COMM_RES);
	if (err) {
		PMD_DRV_LOG(ERR, "Reset function failed");
		goto func_reset_err;
	}

	err = hinic3_set_func_svc_used_state(hwdev, HINIC3_MOD_COMM, 1);
	if (err)
		goto set_used_state_err;

	err = init_cmdqs_channel(hwdev);
	if (err) {
		PMD_DRV_LOG(ERR, "Init cmdq channel failed");
		goto init_cmdqs_channel_err;
	}

	hinic3_sync_mgmt_func_state(hwdev);

	return 0;

init_cmdqs_channel_err:
	hinic3_set_func_svc_used_state(hwdev, HINIC3_MOD_COMM, 0);
set_used_state_err:
func_reset_err:
get_func_info_err:
	free_mgmt_channel(hwdev);

	return err;
}

static void
hinic3_uninit_comm_ch(struct hinic3_hwdev *hwdev)
{
	hinic3_set_pf_status(hwdev->hwif, HINIC3_PF_STATUS_INIT);

	hinic3_comm_cmdqs_free(hwdev);

	if (HINIC3_FUNC_TYPE(hwdev) != TYPE_VF)
		hinic3_set_wq_page_size(hwdev, hinic3_global_func_id(hwdev),
					HINIC3_HW_WQ_PAGE_SIZE);

	hinic3_set_func_svc_used_state(hwdev, HINIC3_MOD_COMM, 0);

	hinic3_func_to_func_free(hwdev);

	hinic3_comm_pf_to_mgmt_free(hwdev);

	hinic3_aeqs_free(hwdev);
}

int
hinic3_init_hwdev(struct hinic3_hwdev *hwdev)
{
	int err;

	hwdev->chip_fault_stats = rte_zmalloc("chip_fault_stats",
					      HINIC3_CHIP_FAULT_SIZE,
					      RTE_CACHE_LINE_SIZE);
	if (!hwdev->chip_fault_stats) {
		PMD_DRV_LOG(ERR, "Alloc memory for chip_fault_stats failed");
		return -ENOMEM;
	}

	err = hinic3_init_hwif(hwdev);
	if (err) {
		PMD_DRV_LOG(ERR, "Initialize hwif failed");
		goto init_hwif_err;
	}

	err = hinic3_init_comm_ch(hwdev);
	if (err) {
		PMD_DRV_LOG(ERR, "Init communication channel failed");
		goto init_comm_ch_err;
	}

	err = hinic3_init_cfg_mgmt(hwdev);
	if (err) {
		PMD_DRV_LOG(ERR, "Init cfg_mgnt failed");
		goto init_cfg_err;
	}

	err = hinic3_init_capability(hwdev);
	if (err) {
		PMD_DRV_LOG(ERR, "Init capability failed");
		goto init_cap_err;
	}

	return 0;

init_cap_err:
	hinic3_deinit_cfg_mgmt(hwdev);
init_cfg_err:
	hinic3_uninit_comm_ch(hwdev);

init_comm_ch_err:
	hinic3_free_hwif(hwdev);

init_hwif_err:
	rte_free(hwdev->chip_fault_stats);

	return -EFAULT;
}

void
hinic3_free_hwdev(struct hinic3_hwdev *hwdev)
{
	hinic3_deinit_cfg_mgmt(hwdev);

	hinic3_uninit_comm_ch(hwdev);

	hinic3_free_hwif(hwdev);

	rte_free(hwdev->chip_fault_stats);
}

#ifndef RTE_VFIO_DMA_MAP_BASE_ADDR
#define RTE_VFIO_DMA_MAP_BASE_ADDR 0
#endif
const struct rte_memzone *
hinic3_dma_zone_reserve(const void *dev, const char *ring_name,
			uint16_t queue_id, size_t size, unsigned int align,
			int socket_id)
{
	return rte_eth_dma_zone_reserve(dev, ring_name, queue_id, size, align,
					socket_id);
}

int
hinic3_memzone_free(const struct rte_memzone *mz)
{
	return rte_memzone_free(mz);
}
