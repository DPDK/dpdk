/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2020 Intel Corporation
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include <rte_malloc.h>
#include <rte_errno.h>

#include "base/dlb2_regs.h"
#include "base/dlb2_hw_types.h"
#include "base/dlb2_resource.h"
#include "base/dlb2_osdep.h"
#include "dlb2_main.h"
#include "../dlb2_user.h"
#include "../dlb2_priv.h"
#include "../dlb2_iface.h"
#include "../dlb2_inline_fns.h"

#define PF_ID_ZERO 0	/* PF ONLY! */
#define NO_OWNER_VF 0	/* PF ONLY! */
#define NOT_VF_REQ false /* PF ONLY! */
#define DLB2_PCI_PASID_CAP_OFFSET        0x148   /* PASID capability offset */

static int
dlb2_pf_init_driver_state(struct dlb2_dev *dlb2_dev)
{
	rte_spinlock_init(&dlb2_dev->resource_mutex);

	return 0;
}

static void dlb2_pf_enable_pm(struct dlb2_dev *dlb2_dev)
{
	int version;
	version = DLB2_HW_DEVICE_FROM_PCI_ID(dlb2_dev->pdev);

	dlb2_clr_pmcsr_disable(&dlb2_dev->hw, version);
}

#define DLB2_READY_RETRY_LIMIT 1000
static int dlb2_pf_wait_for_device_ready(struct dlb2_dev *dlb2_dev,
					 int dlb_version)
{
	u32 retries = 0;

	/* Allow at least 1s for the device to become active after power-on */
	for (retries = 0; retries < DLB2_READY_RETRY_LIMIT; retries++) {
		u32 idle_val;
		u32 idle_dlb_func_idle;
		u32 pm_st_val;
		u32 pm_st_pmsm;
		u32 addr;

		addr = DLB2_CM_CFG_PM_STATUS(dlb_version);
		pm_st_val = DLB2_CSR_RD(&dlb2_dev->hw, addr);
		addr = DLB2_CM_CFG_DIAGNOSTIC_IDLE_STATUS(dlb_version);
		idle_val = DLB2_CSR_RD(&dlb2_dev->hw, addr);
		idle_dlb_func_idle = idle_val &
			DLB2_CM_CFG_DIAGNOSTIC_IDLE_STATUS_DLB_FUNC_IDLE;
		pm_st_pmsm = pm_st_val & DLB2_CM_CFG_PM_STATUS_PMSM;
		if (pm_st_pmsm && idle_dlb_func_idle)
			break;

		rte_delay_ms(1);
	};

	if (retries == DLB2_READY_RETRY_LIMIT) {
		DLB2_LOG_ERR("[%s()] wait for device ready timed out",
		       __func__);
		return -1;
	}

	return 0;
}

struct dlb2_dev *
dlb2_probe(struct rte_pci_device *pdev, const void *probe_args)
{
	struct dlb2_dev *dlb2_dev;
	int ret = 0;
	int dlb_version = 0;

	DLB2_INFO(dlb2_dev, "probe\n");

	dlb2_dev = rte_malloc("DLB2_PF", sizeof(struct dlb2_dev),
			      RTE_CACHE_LINE_SIZE);

	if (dlb2_dev == NULL) {
		ret = -ENOMEM;
		goto dlb2_dev_malloc_fail;
	}

	dlb_version = DLB2_HW_DEVICE_FROM_PCI_ID(pdev);

	/* PCI Bus driver has already mapped bar space into process.
	 * Save off our IO register and FUNC addresses.
	 */

	/* BAR 0 */
	if (pdev->mem_resource[0].addr == NULL) {
		DLB2_ERR(dlb2_dev, "probe: BAR 0 addr (csr_kva) is NULL\n");
		ret = -EINVAL;
		goto pci_mmap_bad_addr;
	}
	dlb2_dev->hw.func_kva = (void *)(uintptr_t)pdev->mem_resource[0].addr;
	dlb2_dev->hw.func_phys_addr = pdev->mem_resource[0].phys_addr;

	DLB2_INFO(dlb2_dev, "DLB2 FUNC VA=%p, PA=%p, len=%p\n",
		  (void *)dlb2_dev->hw.func_kva,
		  (void *)dlb2_dev->hw.func_phys_addr,
		  (void *)(pdev->mem_resource[0].len));

	/* BAR 2 */
	if (pdev->mem_resource[2].addr == NULL) {
		DLB2_ERR(dlb2_dev, "probe: BAR 2 addr (func_kva) is NULL\n");
		ret = -EINVAL;
		goto pci_mmap_bad_addr;
	}
	dlb2_dev->hw.csr_kva = (void *)(uintptr_t)pdev->mem_resource[2].addr;
	dlb2_dev->hw.csr_phys_addr = pdev->mem_resource[2].phys_addr;

	DLB2_INFO(dlb2_dev, "DLB2 CSR VA=%p, PA=%p, len=%p\n",
		  (void *)dlb2_dev->hw.csr_kva,
		  (void *)dlb2_dev->hw.csr_phys_addr,
		  (void *)(pdev->mem_resource[2].len));

	dlb2_dev->pdev = pdev;

	/* PM enable must be done before any other MMIO accesses, and this
	 * setting is persistent across device reset.
	 */
	dlb2_pf_enable_pm(dlb2_dev);

	ret = dlb2_pf_wait_for_device_ready(dlb2_dev, dlb_version);
	if (ret)
		goto wait_for_device_ready_fail;

	ret = dlb2_resource_probe(&dlb2_dev->hw, probe_args);
	if (ret)
		goto resource_probe_fail;

	ret = dlb2_pf_reset(dlb2_dev);
	if (ret)
		goto dlb2_reset_fail;

	ret = dlb2_pf_init_driver_state(dlb2_dev);
	if (ret)
		goto init_driver_state_fail;

	ret = dlb2_resource_init(&dlb2_dev->hw, dlb_version, probe_args);
	if (ret)
		goto resource_init_fail;

	return dlb2_dev;

resource_init_fail:
	dlb2_resource_free(&dlb2_dev->hw);
init_driver_state_fail:
dlb2_reset_fail:
pci_mmap_bad_addr:
resource_probe_fail:
wait_for_device_ready_fail:
	rte_free(dlb2_dev);
dlb2_dev_malloc_fail:
	rte_errno = ret;
	return NULL;
}

int
dlb2_pf_reset(struct dlb2_dev *dlb2_dev)
{
	int ret = 0;
	int i = 0;
	uint32_t dword[16];
	uint16_t cmd;
	off_t off;

	uint16_t dev_ctl_word;
	uint16_t dev_ctl2_word;
	uint16_t lnk_word;
	uint16_t lnk_word2;
	uint16_t slt_word;
	uint16_t slt_word2;
	uint16_t rt_ctl_word;
	uint32_t pri_reqs_dword;
	uint16_t pri_ctrl_word;

	off_t pcie_cap_offset;
	int pri_cap_offset;
	off_t msix_cap_offset;
	int err_cap_offset;
	int acs_cap_offset;
	int wait_count;

	uint16_t devsta_busy_word;
	uint16_t devctl_word;

	struct rte_pci_device *pdev = dlb2_dev->pdev;

	/* Save PCI config state */

	for (i = 0; i < 16; i++) {
		if (rte_pci_read_config(pdev, &dword[i], 4, i * 4) != 4)
			return ret;
	}

	pcie_cap_offset = rte_pci_find_capability(pdev, RTE_PCI_CAP_ID_EXP);

	if (pcie_cap_offset < 0) {
		DLB2_LOG_ERR("[%s()] failed to find the pcie capability",
		       __func__);
		return pcie_cap_offset;
	}

	off = pcie_cap_offset + RTE_PCI_EXP_DEVCTL;
	if (rte_pci_read_config(pdev, &dev_ctl_word, 2, off) != 2)
		dev_ctl_word = 0;

	off = pcie_cap_offset + RTE_PCI_EXP_LNKCTL;
	if (rte_pci_read_config(pdev, &lnk_word, 2, off) != 2)
		lnk_word = 0;

	off = pcie_cap_offset + RTE_PCI_EXP_SLTCTL;
	if (rte_pci_read_config(pdev, &slt_word, 2, off) != 2)
		slt_word = 0;

	off = pcie_cap_offset + RTE_PCI_EXP_RTCTL;
	if (rte_pci_read_config(pdev, &rt_ctl_word, 2, off) != 2)
		rt_ctl_word = 0;

	off = pcie_cap_offset + RTE_PCI_EXP_DEVCTL2;
	if (rte_pci_read_config(pdev, &dev_ctl2_word, 2, off) != 2)
		dev_ctl2_word = 0;

	off = pcie_cap_offset + RTE_PCI_EXP_LNKCTL2;
	if (rte_pci_read_config(pdev, &lnk_word2, 2, off) != 2)
		lnk_word2 = 0;

	off = pcie_cap_offset + RTE_PCI_EXP_SLTCTL2;
	if (rte_pci_read_config(pdev, &slt_word2, 2, off) != 2)
		slt_word2 = 0;

	off = RTE_PCI_EXT_CAP_ID_PRI;
	pri_cap_offset = rte_pci_find_ext_capability(pdev, off);

	if (pri_cap_offset >= 0) {
		off = pri_cap_offset + RTE_PCI_PRI_ALLOC_REQ;
		if (rte_pci_read_config(pdev, &pri_reqs_dword, 4, off) != 4)
			pri_reqs_dword = 0;
	}

	/* clear the PCI command register before issuing the FLR */

	off = RTE_PCI_COMMAND;
	cmd = 0;
	if (rte_pci_write_config(pdev, &cmd, 2, off) != 2) {
		DLB2_LOG_ERR("[%s()] failed to write the pci command",
		       __func__);
		return ret;
	}

	/* issue the FLR */
	for (wait_count = 0; wait_count < 4; wait_count++) {
		int sleep_time;

		off = pcie_cap_offset + RTE_PCI_EXP_DEVSTA;
		ret = rte_pci_read_config(pdev, &devsta_busy_word, 2, off);
		if (ret != 2) {
			DLB2_LOG_ERR("[%s()] failed to read the pci device status",
			       __func__);
			return ret;
		}

		if (!(devsta_busy_word & RTE_PCI_EXP_DEVSTA_TRPND))
			break;

		sleep_time = (1 << (wait_count)) * 100;
		rte_delay_ms(sleep_time);
	}

	if (wait_count == 4) {
		DLB2_LOG_ERR("[%s()] wait for pci pending transactions timed out",
		       __func__);
		return -1;
	}

	off = pcie_cap_offset + RTE_PCI_EXP_DEVCTL;
	ret = rte_pci_read_config(pdev, &devctl_word, 2, off);
	if (ret != 2) {
		DLB2_LOG_ERR("[%s()] failed to read the pcie device control",
		       __func__);
		return ret;
	}

	devctl_word |= RTE_PCI_EXP_DEVCTL_BCR_FLR;

	ret = rte_pci_write_config(pdev, &devctl_word, 2, off);
	if (ret != 2) {
		DLB2_LOG_ERR("[%s()] failed to write the pcie device control",
		       __func__);
		return ret;
	}

	rte_delay_ms(100);

	/* Restore PCI config state */

	if (pcie_cap_offset >= 0) {
		off = pcie_cap_offset + RTE_PCI_EXP_DEVCTL;
		ret = rte_pci_write_config(pdev, &dev_ctl_word, 2, off);
		if (ret != 2) {
			DLB2_LOG_ERR("[%s()] failed to write the pcie device control at offset %d",
				__func__, (int)off);
			return ret;
		}

		off = pcie_cap_offset + RTE_PCI_EXP_LNKCTL;
		ret = rte_pci_write_config(pdev, &lnk_word, 2, off);
		if (ret != 2) {
			DLB2_LOG_ERR("[%s()] failed to write the pcie config space at offset %d",
				__func__, (int)off);
			return ret;
		}

		off = pcie_cap_offset + RTE_PCI_EXP_SLTCTL;
		ret = rte_pci_write_config(pdev, &slt_word, 2, off);
		if (ret != 2) {
			DLB2_LOG_ERR("[%s()] failed to write the pcie config space at offset %d",
				__func__, (int)off);
			return ret;
		}

		off = pcie_cap_offset + RTE_PCI_EXP_RTCTL;
		ret = rte_pci_write_config(pdev, &rt_ctl_word, 2, off);
		if (ret != 2) {
			DLB2_LOG_ERR("[%s()] failed to write the pcie config space at offset %d",
				__func__, (int)off);
			return ret;
		}

		off = pcie_cap_offset + RTE_PCI_EXP_DEVCTL2;
		ret = rte_pci_write_config(pdev, &dev_ctl2_word, 2, off);
		if (ret != 2) {
			DLB2_LOG_ERR("[%s()] failed to write the pcie config space at offset %d",
				__func__, (int)off);
			return ret;
		}

		off = pcie_cap_offset + RTE_PCI_EXP_LNKCTL2;
		ret = rte_pci_write_config(pdev, &lnk_word2, 2, off);
		if (ret != 2) {
			DLB2_LOG_ERR("[%s()] failed to write the pcie config space at offset %d",
				__func__, (int)off);
			return ret;
		}

		off = pcie_cap_offset + RTE_PCI_EXP_SLTCTL2;
		ret = rte_pci_write_config(pdev, &slt_word2, 2, off);
		if (ret != 2) {
			DLB2_LOG_ERR("[%s()] failed to write the pcie config space at offset %d",
				__func__, (int)off);
			return ret;
		}
	}

	if (pri_cap_offset >= 0) {
		pri_ctrl_word = RTE_PCI_PRI_CTRL_ENABLE;

		off = pri_cap_offset + RTE_PCI_PRI_ALLOC_REQ;
		ret = rte_pci_write_config(pdev, &pri_reqs_dword, 4, off);
		if (ret != 4) {
			DLB2_LOG_ERR("[%s()] failed to write the pcie config space at offset %d",
				__func__, (int)off);
			return ret;
		}

		off = pri_cap_offset + RTE_PCI_PRI_CTRL;
		ret = rte_pci_write_config(pdev, &pri_ctrl_word, 2, off);
		if (ret != 2) {
			DLB2_LOG_ERR("[%s()] failed to write the pcie config space at offset %d",
				__func__, (int)off);
			return ret;
		}
	}

	off = RTE_PCI_EXT_CAP_ID_ERR;
	err_cap_offset = rte_pci_find_ext_capability(pdev, off);

	if (err_cap_offset >= 0) {
		uint32_t tmp;

		off = err_cap_offset + RTE_PCI_ERR_ROOT_STATUS;
		if (rte_pci_read_config(pdev, &tmp, 4, off) != 4)
			tmp = 0;

		ret = rte_pci_write_config(pdev, &tmp, 4, off);
		if (ret != 4) {
			DLB2_LOG_ERR("[%s()] failed to write the pcie config space at offset %d",
				__func__, (int)off);
			return ret;
		}

		off = err_cap_offset + RTE_PCI_ERR_COR_STATUS;
		if (rte_pci_read_config(pdev, &tmp, 4, off) != 4)
			tmp = 0;

		ret = rte_pci_write_config(pdev, &tmp, 4, off);
		if (ret != 4) {
			DLB2_LOG_ERR("[%s()] failed to write the pcie config space at offset %d",
				__func__, (int)off);
			return ret;
		}

		off = err_cap_offset + RTE_PCI_ERR_UNCOR_STATUS;
		if (rte_pci_read_config(pdev, &tmp, 4, off) != 4)
			tmp = 0;

		ret = rte_pci_write_config(pdev, &tmp, 4, off);
		if (ret != 4) {
			DLB2_LOG_ERR("[%s()] failed to write the pcie config space at offset %d",
				__func__, (int)off);
			return ret;
		}
	}

	for (i = 16; i > 0; i--) {
		off = (i - 1) * 4;
		ret = rte_pci_write_config(pdev, &dword[i - 1], 4, off);
		if (ret != 4) {
			DLB2_LOG_ERR("[%s()] failed to write the pcie config space at offset %d",
				__func__, (int)off);
			return ret;
		}
	}

	off = RTE_PCI_COMMAND;
	if (rte_pci_read_config(pdev, &cmd, 2, off) == 2) {
		cmd &= ~RTE_PCI_COMMAND_INTX_DISABLE;
		if (rte_pci_write_config(pdev, &cmd, 2, off) != 2) {
			DLB2_LOG_ERR("[%s()] failed to write the pci command",
			       __func__);
			return ret;
		}
	}

	msix_cap_offset = rte_pci_find_capability(pdev, RTE_PCI_CAP_ID_MSIX);
	if (msix_cap_offset >= 0) {
		off = msix_cap_offset + RTE_PCI_MSIX_FLAGS;
		if (rte_pci_read_config(pdev, &cmd, 2, off) == 2) {
			cmd |= RTE_PCI_MSIX_FLAGS_ENABLE;
			cmd |= RTE_PCI_MSIX_FLAGS_MASKALL;
			if (rte_pci_write_config(pdev, &cmd, 2, off) != 2) {
				DLB2_LOG_ERR("[%s()] failed to write msix flags",
				       __func__);
				return ret;
			}
		}

		off = msix_cap_offset + RTE_PCI_MSIX_FLAGS;
		if (rte_pci_read_config(pdev, &cmd, 2, off) == 2) {
			cmd &= ~RTE_PCI_MSIX_FLAGS_MASKALL;
			if (rte_pci_write_config(pdev, &cmd, 2, off) != 2) {
				DLB2_LOG_ERR("[%s()] failed to write msix flags",
				       __func__);
				return ret;
			}
		}
	}

	off = RTE_PCI_EXT_CAP_ID_ACS;
	acs_cap_offset = rte_pci_find_ext_capability(pdev, off);

	if (acs_cap_offset >= 0) {
		uint16_t acs_cap, acs_ctrl, acs_mask;
		off = acs_cap_offset + RTE_PCI_ACS_CAP;
		if (rte_pci_read_config(pdev, &acs_cap, 2, off) != 2)
			acs_cap = 0;

		off = acs_cap_offset + RTE_PCI_ACS_CTRL;
		if (rte_pci_read_config(pdev, &acs_ctrl, 2, off) != 2)
			acs_ctrl = 0;

		acs_mask = RTE_PCI_ACS_SV | RTE_PCI_ACS_RR;
		acs_mask |= (RTE_PCI_ACS_CR | RTE_PCI_ACS_UF);
		acs_ctrl |= (acs_cap & acs_mask);

		ret = rte_pci_write_config(pdev, &acs_ctrl, 2, off);
		if (ret != 2) {
			DLB2_LOG_ERR("[%s()] failed to write the pcie config space at offset %d",
				__func__, (int)off);
			return ret;
		}

		off = acs_cap_offset + RTE_PCI_ACS_CTRL;
		if (rte_pci_read_config(pdev, &acs_ctrl, 2, off) != 2)
			acs_ctrl = 0;

		acs_mask = RTE_PCI_ACS_RR | RTE_PCI_ACS_CR;
		acs_mask |= RTE_PCI_ACS_EC;
		acs_ctrl &= ~acs_mask;

		off = acs_cap_offset + RTE_PCI_ACS_CTRL;
		ret = rte_pci_write_config(pdev, &acs_ctrl, 2, off);
		if (ret != 2) {
			DLB2_LOG_ERR("[%s()] failed to write the pcie config space at offset %d",
				__func__, (int)off);
			return ret;
		}
	}

	/* Disable PASID if it is enabled by default, which
	 * breaks the DLB if enabled.
	 */
	off = DLB2_PCI_PASID_CAP_OFFSET;
	if (rte_pci_pasid_set_state(pdev, off, false) < 0) {
		DLB2_LOG_ERR("[%s()] failed to write the pcie config space at offset %d",
				__func__, (int)off);
		return -1;
	}

	return 0;
}

int
dlb2_pf_create_sched_domain(struct dlb2_hw *hw,
			    struct dlb2_create_sched_domain_args *args,
			    struct dlb2_cmd_response *resp)
{
	return dlb2_hw_create_sched_domain(hw, args, resp, NOT_VF_REQ,
					   PF_ID_ZERO);
}

int
dlb2_pf_reset_domain(struct dlb2_hw *hw, u32 id)
{
	return dlb2_reset_domain(hw, id, NOT_VF_REQ, PF_ID_ZERO);
}

int
dlb2_pf_create_ldb_queue(struct dlb2_hw *hw,
			 u32 id,
			 struct dlb2_create_ldb_queue_args *args,
			 struct dlb2_cmd_response *resp)
{
	return dlb2_hw_create_ldb_queue(hw, id, args, resp, NOT_VF_REQ,
					PF_ID_ZERO);
}

int
dlb2_pf_create_ldb_port(struct dlb2_hw *hw,
			u32 id,
			struct dlb2_create_ldb_port_args *args,
			uintptr_t cq_dma_base,
			struct dlb2_cmd_response *resp)
{
	return dlb2_hw_create_ldb_port(hw, id, args,
				       cq_dma_base,
				       resp,
				       NOT_VF_REQ,
				       PF_ID_ZERO);
}

int
dlb2_pf_create_dir_port(struct dlb2_hw *hw,
			u32 id,
			struct dlb2_create_dir_port_args *args,
			uintptr_t cq_dma_base,
			struct dlb2_cmd_response *resp)
{
	return dlb2_hw_create_dir_port(hw, id, args,
				       cq_dma_base,
				       resp,
				       NOT_VF_REQ,
				       PF_ID_ZERO);
}

int
dlb2_pf_create_dir_queue(struct dlb2_hw *hw,
			 u32 id,
			 struct dlb2_create_dir_queue_args *args,
			 struct dlb2_cmd_response *resp)
{
	return dlb2_hw_create_dir_queue(hw, id, args, resp, NOT_VF_REQ,
					PF_ID_ZERO);
}

int
dlb2_pf_start_domain(struct dlb2_hw *hw,
		     u32 id,
		     struct dlb2_start_domain_args *args,
		     struct dlb2_cmd_response *resp)
{
	return dlb2_hw_start_domain(hw, id, args, resp, NOT_VF_REQ,
				    PF_ID_ZERO);
}
