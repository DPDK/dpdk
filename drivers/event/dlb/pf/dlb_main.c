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

#include "base/dlb_resource.h"
#include "base/dlb_osdep.h"
#include "base/dlb_regs.h"
#include "../dlb_priv.h"
#include "../dlb_inline_fns.h"
#include "../dlb_user.h"
#include "dlb_main.h"

unsigned int dlb_unregister_timeout_s = DLB_DEFAULT_UNREGISTER_TIMEOUT_S;

#define DLB_PCI_CAP_POINTER 0x34
#define DLB_PCI_CAP_NEXT(hdr) (((hdr) >> 8) & 0xFC)
#define DLB_PCI_CAP_ID(hdr) ((hdr) & 0xFF)
#define DLB_PCI_ERR_UNCOR_MASK 8
#define DLB_PCI_ERR_UNC_UNSUP  0x00100000

#define DLB_PCI_LNKCTL 16
#define DLB_PCI_SLTCTL 24
#define DLB_PCI_RTCTL 28
#define DLB_PCI_EXP_DEVCTL2 40
#define DLB_PCI_LNKCTL2 48
#define DLB_PCI_SLTCTL2 56
#define DLB_PCI_CMD 4
#define DLB_PCI_EXP_DEVSTA 10
#define DLB_PCI_EXP_DEVSTA_TRPND 0x20
#define DLB_PCI_EXP_DEVCTL_BCR_FLR 0x8000

#define DLB_PCI_CAP_ID_EXP       0x10
#define DLB_PCI_CAP_ID_MSIX      0x11
#define DLB_PCI_EXT_CAP_ID_PRI   0x13
#define DLB_PCI_EXT_CAP_ID_ACS   0xD

#define DLB_PCI_PRI_CTRL_ENABLE         0x1
#define DLB_PCI_PRI_ALLOC_REQ           0xC
#define DLB_PCI_PRI_CTRL                0x4
#define DLB_PCI_MSIX_FLAGS              0x2
#define DLB_PCI_MSIX_FLAGS_ENABLE       0x8000
#define DLB_PCI_MSIX_FLAGS_MASKALL      0x4000
#define DLB_PCI_ERR_ROOT_STATUS         0x30
#define DLB_PCI_ERR_COR_STATUS          0x10
#define DLB_PCI_ERR_UNCOR_STATUS        0x4
#define DLB_PCI_COMMAND_INTX_DISABLE    0x400
#define DLB_PCI_ACS_CAP                 0x4
#define DLB_PCI_ACS_CTRL                0x6
#define DLB_PCI_ACS_SV                  0x1
#define DLB_PCI_ACS_RR                  0x4
#define DLB_PCI_ACS_CR                  0x8
#define DLB_PCI_ACS_UF                  0x10
#define DLB_PCI_ACS_EC                  0x20

static int dlb_pci_find_capability(struct rte_pci_device *pdev, uint32_t id)
{
	uint8_t pos;
	int ret;
	uint16_t hdr;

	ret = rte_pci_read_config(pdev, &pos, 1, DLB_PCI_CAP_POINTER);
	pos &= 0xFC;

	if (ret != 1)
		return -1;

	while (pos > 0x3F) {
		ret = rte_pci_read_config(pdev, &hdr, 2, pos);
		if (ret != 2)
			return -1;

		if (DLB_PCI_CAP_ID(hdr) == id)
			return pos;

		if (DLB_PCI_CAP_ID(hdr) == 0xFF)
			return -1;

		pos = DLB_PCI_CAP_NEXT(hdr);
	}

	return -1;
}

static int dlb_mask_ur_err(struct rte_pci_device *pdev)
{
	uint32_t mask;
	size_t sz = sizeof(mask);
	int pos = rte_pci_find_ext_capability(pdev, RTE_PCI_EXT_CAP_ID_ERR);

	if (pos < 0) {
		DLB_LOG_ERR("[%s()] failed to find the aer capability\n",
		       __func__);
		return pos;
	}

	pos += DLB_PCI_ERR_UNCOR_MASK;

	if (rte_pci_read_config(pdev, &mask, sz, pos) != (int)sz) {
		DLB_LOG_ERR("[%s()] Failed to read uncorrectable error mask reg\n",
		       __func__);
		return -1;
	}

	/* Mask Unsupported Request errors */
	mask |= DLB_PCI_ERR_UNC_UNSUP;

	if (rte_pci_write_config(pdev, &mask, sz, pos) != (int)sz) {
		DLB_LOG_ERR("[%s()] Failed to write uncorrectable error mask reg at offset %d\n",
		       __func__, pos);
		return -1;
	}

	return 0;
}

struct dlb_dev *
dlb_probe(struct rte_pci_device *pdev)
{
	struct dlb_dev *dlb_dev;
	int ret = 0;

	DLB_INFO(dlb_dev, "probe\n");

	dlb_dev = rte_malloc("DLB_PF", sizeof(struct dlb_dev),
			     RTE_CACHE_LINE_SIZE);

	if (dlb_dev == NULL) {
		ret = -ENOMEM;
		goto dlb_dev_malloc_fail;
	}

	/* PCI Bus driver has already mapped bar space into process.
	 * Save off our IO register and FUNC addresses.
	 */

	/* BAR 0 */
	if (pdev->mem_resource[0].addr == NULL) {
		DLB_ERR(dlb_dev, "probe: BAR 0 addr (csr_kva) is NULL\n");
		ret = -EINVAL;
		goto pci_mmap_bad_addr;
	}
	dlb_dev->hw.func_kva = (void *)(uintptr_t)pdev->mem_resource[0].addr;
	dlb_dev->hw.func_phys_addr = pdev->mem_resource[0].phys_addr;

	DLB_INFO(dlb_dev, "DLB FUNC VA=%p, PA=%p, len=%"PRIu64"\n",
		 (void *)dlb_dev->hw.func_kva,
		 (void *)dlb_dev->hw.func_phys_addr,
		 pdev->mem_resource[0].len);

	/* BAR 2 */
	if (pdev->mem_resource[2].addr == NULL) {
		DLB_ERR(dlb_dev, "probe: BAR 2 addr (func_kva) is NULL\n");
		ret = -EINVAL;
		goto pci_mmap_bad_addr;
	}
	dlb_dev->hw.csr_kva = (void *)(uintptr_t)pdev->mem_resource[2].addr;
	dlb_dev->hw.csr_phys_addr = pdev->mem_resource[2].phys_addr;

	DLB_INFO(dlb_dev, "DLB CSR VA=%p, PA=%p, len=%"PRIu64"\n",
		 (void *)dlb_dev->hw.csr_kva,
		 (void *)dlb_dev->hw.csr_phys_addr,
		 pdev->mem_resource[2].len);

	dlb_dev->pdev = pdev;

	ret = dlb_pf_reset(dlb_dev);
	if (ret)
		goto dlb_reset_fail;

	/* DLB incorrectly sends URs in response to certain messages. Mask UR
	 * errors to prevent these from being propagated to the MCA.
	 */
	ret = dlb_mask_ur_err(pdev);
	if (ret)
		goto mask_ur_err_fail;

	ret = dlb_pf_init_driver_state(dlb_dev);
	if (ret)
		goto init_driver_state_fail;

	ret = dlb_resource_init(&dlb_dev->hw);
	if (ret)
		goto resource_init_fail;

	dlb_dev->revision = os_get_dev_revision(&dlb_dev->hw);

	dlb_pf_init_hardware(dlb_dev);

	return dlb_dev;

resource_init_fail:
	dlb_resource_free(&dlb_dev->hw);
init_driver_state_fail:
mask_ur_err_fail:
dlb_reset_fail:
pci_mmap_bad_addr:
	rte_free(dlb_dev);
dlb_dev_malloc_fail:
	rte_errno = ret;
	return NULL;
}

int
dlb_pf_reset(struct dlb_dev *dlb_dev)
{
	int msix_cap_offset, err_cap_offset, acs_cap_offset, wait_count;
	uint16_t dev_ctl_word, dev_ctl2_word, lnk_word, lnk_word2;
	uint16_t rt_ctl_word, pri_ctrl_word;
	struct rte_pci_device *pdev = dlb_dev->pdev;
	uint16_t devsta_busy_word, devctl_word;
	int pcie_cap_offset, pri_cap_offset;
	uint16_t slt_word, slt_word2, cmd;
	int ret = 0, i = 0;
	uint32_t dword[16], pri_reqs_dword;
	off_t off;

	/* Save PCI config state */

	for (i = 0; i < 16; i++) {
		if (rte_pci_read_config(pdev, &dword[i], 4, i * 4) != 4)
			return ret;
	}

	pcie_cap_offset = dlb_pci_find_capability(pdev, DLB_PCI_CAP_ID_EXP);

	if (pcie_cap_offset < 0) {
		DLB_LOG_ERR("[%s()] failed to find the pcie capability\n",
		       __func__);
		return pcie_cap_offset;
	}

	off = pcie_cap_offset + RTE_PCI_EXP_DEVCTL;
	if (rte_pci_read_config(pdev, &dev_ctl_word, 2, off) != 2)
		dev_ctl_word = 0;

	off = pcie_cap_offset + DLB_PCI_LNKCTL;
	if (rte_pci_read_config(pdev, &lnk_word, 2, off) != 2)
		lnk_word = 0;

	off = pcie_cap_offset + DLB_PCI_SLTCTL;
	if (rte_pci_read_config(pdev, &slt_word, 2, off) != 2)
		slt_word = 0;

	off = pcie_cap_offset + DLB_PCI_RTCTL;
	if (rte_pci_read_config(pdev, &rt_ctl_word, 2, off) != 2)
		rt_ctl_word = 0;

	off = pcie_cap_offset + DLB_PCI_EXP_DEVCTL2;
	if (rte_pci_read_config(pdev, &dev_ctl2_word, 2, off) != 2)
		dev_ctl2_word = 0;

	off = pcie_cap_offset + DLB_PCI_LNKCTL2;
	if (rte_pci_read_config(pdev, &lnk_word2, 2, off) != 2)
		lnk_word2 = 0;

	off = pcie_cap_offset + DLB_PCI_SLTCTL2;
	if (rte_pci_read_config(pdev, &slt_word2, 2, off) != 2)
		slt_word2 = 0;

	pri_cap_offset = rte_pci_find_ext_capability(pdev,
						     DLB_PCI_EXT_CAP_ID_PRI);
	if (pri_cap_offset >= 0) {
		off = pri_cap_offset + DLB_PCI_PRI_ALLOC_REQ;
		if (rte_pci_read_config(pdev, &pri_reqs_dword, 4, off) != 4)
			pri_reqs_dword = 0;
	}

	/* clear the PCI command register before issuing the FLR */

	off = DLB_PCI_CMD;
	cmd = 0;
	if (rte_pci_write_config(pdev, &cmd, 2, off) != 2) {
		DLB_LOG_ERR("[%s()] failed to write pci config space at offset %d\n",
		       __func__, (int)off);
		return -1;
	}

	/* issue the FLR */
	for (wait_count = 0; wait_count < 4; wait_count++) {
		int sleep_time;

		off = pcie_cap_offset + DLB_PCI_EXP_DEVSTA;
		ret = rte_pci_read_config(pdev, &devsta_busy_word, 2, off);
		if (ret != 2) {
			DLB_LOG_ERR("[%s()] failed to read the pci device status\n",
			       __func__);
			return ret;
		}

		if (!(devsta_busy_word & DLB_PCI_EXP_DEVSTA_TRPND))
			break;

		sleep_time = (1 << (wait_count)) * 100;
		rte_delay_ms(sleep_time);
	}

	if (wait_count == 4) {
		DLB_LOG_ERR("[%s()] wait for pci pending transactions timed out\n",
		       __func__);
		return -1;
	}

	off = pcie_cap_offset + RTE_PCI_EXP_DEVCTL;
	ret = rte_pci_read_config(pdev, &devctl_word, 2, off);
	if (ret != 2) {
		DLB_LOG_ERR("[%s()] failed to read the pcie device control\n",
		       __func__);
		return ret;
	}

	devctl_word |= DLB_PCI_EXP_DEVCTL_BCR_FLR;

	if (rte_pci_write_config(pdev, &devctl_word, 2, off) != 2) {
		DLB_LOG_ERR("[%s()] failed to write the pcie device control at offset %d\n",
		       __func__, (int)off);
		return -1;
	}

	rte_delay_ms(100);

	/* Restore PCI config state */

	if (pcie_cap_offset >= 0) {
		off = pcie_cap_offset + RTE_PCI_EXP_DEVCTL;
		if (rte_pci_write_config(pdev, &dev_ctl_word, 2, off) != 2) {
			DLB_LOG_ERR("[%s()] failed to write the pcie device control at offset %d\n",
			       __func__, (int)off);
			return -1;
		}

		off = pcie_cap_offset + DLB_PCI_LNKCTL;
		if (rte_pci_write_config(pdev, &lnk_word, 2, off) != 2) {
			DLB_LOG_ERR("[%s()] failed to write pci config space at offset %d\n",
			       __func__, (int)off);
			return -1;
		}

		off = pcie_cap_offset + DLB_PCI_SLTCTL;
		if (rte_pci_write_config(pdev, &slt_word, 2, off) != 2) {
			DLB_LOG_ERR("[%s()] failed to write pci config space at offset %d\n",
			       __func__, (int)off);
			return -1;
		}

		off = pcie_cap_offset + DLB_PCI_RTCTL;
		if (rte_pci_write_config(pdev, &rt_ctl_word, 2, off) != 2) {
			DLB_LOG_ERR("[%s()] failed to write pci config space at offset %d\n",
			       __func__, (int)off);
			return -1;
		}

		off = pcie_cap_offset + DLB_PCI_EXP_DEVCTL2;
		if (rte_pci_write_config(pdev, &dev_ctl2_word, 2, off) != 2) {
			DLB_LOG_ERR("[%s()] failed to write pci config space at offset %d\n",
			       __func__, (int)off);
			return -1;
		}

		off = pcie_cap_offset + DLB_PCI_LNKCTL2;
		if (rte_pci_write_config(pdev, &lnk_word2, 2, off) != 2) {
			DLB_LOG_ERR("[%s()] failed to write pci config space at offset %d\n",
			       __func__, (int)off);
			return -1;
		}

		off = pcie_cap_offset + DLB_PCI_SLTCTL2;
		if (rte_pci_write_config(pdev, &slt_word2, 2, off) != 2) {
			DLB_LOG_ERR("[%s()] failed to write pci config space at offset %d\n",
			       __func__, (int)off);
			return -1;
		}
	}

	if (pri_cap_offset >= 0) {
		pri_ctrl_word = DLB_PCI_PRI_CTRL_ENABLE;

		off = pri_cap_offset + DLB_PCI_PRI_ALLOC_REQ;
		if (rte_pci_write_config(pdev, &pri_reqs_dword, 4, off) != 4) {
			DLB_LOG_ERR("[%s()] failed to write pci config space at offset %d\n",
			       __func__, (int)off);
			return -1;
		}

		off = pri_cap_offset + DLB_PCI_PRI_CTRL;
		if (rte_pci_write_config(pdev, &pri_ctrl_word, 2, off) != 2) {
			DLB_LOG_ERR("[%s()] failed to write pci config space at offset %d\n",
			       __func__, (int)off);
			return -1;
		}
	}

	err_cap_offset = rte_pci_find_ext_capability(pdev,
						     RTE_PCI_EXT_CAP_ID_ERR);
	if (err_cap_offset >= 0) {
		uint32_t tmp;

		off = err_cap_offset + DLB_PCI_ERR_ROOT_STATUS;
		if (rte_pci_read_config(pdev, &tmp, 4, off) != 4)
			tmp = 0;

		if (rte_pci_write_config(pdev, &tmp, 4, off) != 4) {
			DLB_LOG_ERR("[%s()] failed to write pci config space at offset %d\n",
			       __func__, (int)off);
			return -1;
		}

		off = err_cap_offset + DLB_PCI_ERR_COR_STATUS;
		if (rte_pci_read_config(pdev, &tmp, 4, off) != 4)
			tmp = 0;

		if (rte_pci_write_config(pdev, &tmp, 4, off) != 4) {
			DLB_LOG_ERR("[%s()] failed to write pci config space at offset %d\n",
			       __func__, (int)off);
			return -1;
		}

		off = err_cap_offset + DLB_PCI_ERR_UNCOR_STATUS;
		if (rte_pci_read_config(pdev, &tmp, 4, off) != 4)
			tmp = 0;

		if (rte_pci_write_config(pdev, &tmp, 4, off) != 4) {
			DLB_LOG_ERR("[%s()] failed to write pci config space at offset %d\n",
			       __func__, (int)off);
			return -1;
		}
	}

	for (i = 16; i > 0; i--) {
		off = (i - 1) * 4;
		if (rte_pci_write_config(pdev, &dword[i - 1], 4, off) != 4) {
			DLB_LOG_ERR("[%s()] failed to write pci config space at offset %d\n",
			       __func__, (int)off);
			return -1;
		}
	}

	off = DLB_PCI_CMD;
	if (rte_pci_read_config(pdev, &cmd, 2, off) == 2) {
		cmd &= ~DLB_PCI_COMMAND_INTX_DISABLE;
		if (rte_pci_write_config(pdev, &cmd, 2, off) != 2) {
			DLB_LOG_ERR("[%s()] failed to write pci config space\n",
			       __func__);
			return -1;
		}
	}

	msix_cap_offset = dlb_pci_find_capability(pdev, DLB_PCI_CAP_ID_MSIX);
	if (msix_cap_offset >= 0) {
		off = msix_cap_offset + DLB_PCI_MSIX_FLAGS;
		if (rte_pci_read_config(pdev, &cmd, 2, off) == 2) {
			cmd |= DLB_PCI_MSIX_FLAGS_ENABLE;
			cmd |= DLB_PCI_MSIX_FLAGS_MASKALL;
			if (rte_pci_write_config(pdev, &cmd, 2, off) != 2) {
				DLB_LOG_ERR("[%s()] failed to write msix flags\n",
				       __func__);
				return -1;
			}
		}

		off = msix_cap_offset + DLB_PCI_MSIX_FLAGS;
		if (rte_pci_read_config(pdev, &cmd, 2, off) == 2) {
			cmd &= ~DLB_PCI_MSIX_FLAGS_MASKALL;
			if (rte_pci_write_config(pdev, &cmd, 2, off) != 2) {
				DLB_LOG_ERR("[%s()] failed to write msix flags\n",
				       __func__);
				return -1;
			}
		}
	}

	acs_cap_offset = rte_pci_find_ext_capability(pdev,
						     DLB_PCI_EXT_CAP_ID_ACS);
	if (acs_cap_offset >= 0) {
		uint16_t acs_cap, acs_ctrl, acs_mask;
		off = acs_cap_offset + DLB_PCI_ACS_CAP;
		if (rte_pci_read_config(pdev, &acs_cap, 2, off) != 2)
			acs_cap = 0;

		off = acs_cap_offset + DLB_PCI_ACS_CTRL;
		if (rte_pci_read_config(pdev, &acs_ctrl, 2, off) != 2)
			acs_ctrl = 0;

		acs_mask = DLB_PCI_ACS_SV | DLB_PCI_ACS_RR;
		acs_mask |= (DLB_PCI_ACS_CR | DLB_PCI_ACS_UF);
		acs_ctrl |= (acs_cap & acs_mask);

		if (rte_pci_write_config(pdev, &acs_ctrl, 2, off) != 2) {
			DLB_LOG_ERR("[%s()] failed to write pci config space at offset %d\n",
			       __func__, (int)off);
			return -1;
		}

		off = acs_cap_offset + DLB_PCI_ACS_CTRL;
		if (rte_pci_read_config(pdev, &acs_ctrl, 2, off) != 2)
			acs_ctrl = 0;

		acs_mask = DLB_PCI_ACS_RR | DLB_PCI_ACS_CR | DLB_PCI_ACS_EC;
		acs_ctrl &= ~acs_mask;

		off = acs_cap_offset + DLB_PCI_ACS_CTRL;
		if (rte_pci_write_config(pdev, &acs_ctrl, 2, off) != 2) {
			DLB_LOG_ERR("[%s()] failed to write pci config space at offset %d\n",
			       __func__, (int)off);
			return -1;
		}
	}

	return 0;
}

/*******************************/
/****** Driver management ******/
/*******************************/

int
dlb_pf_init_driver_state(struct dlb_dev *dlb_dev)
{
	/* Initialize software state */
	rte_spinlock_init(&dlb_dev->resource_mutex);
	rte_spinlock_init(&dlb_dev->measurement_lock);

	return 0;
}

void
dlb_pf_init_hardware(struct dlb_dev *dlb_dev)
{
	dlb_disable_dp_vasr_feature(&dlb_dev->hw);

	dlb_enable_excess_tokens_alarm(&dlb_dev->hw);

	if (dlb_dev->revision >= DLB_REV_B0) {
		dlb_hw_enable_sparse_ldb_cq_mode(&dlb_dev->hw);
		dlb_hw_enable_sparse_dir_cq_mode(&dlb_dev->hw);
	}

	if (dlb_dev->revision >= DLB_REV_B0) {
		dlb_hw_disable_pf_to_vf_isr_pend_err(&dlb_dev->hw);
		dlb_hw_disable_vf_to_pf_isr_pend_err(&dlb_dev->hw);
	}
}
