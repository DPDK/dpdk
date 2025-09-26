/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2025 Nebulamatrix Technology Co., Ltd.
 */

#include "nbl_hw_leonis_snic.h"

static inline void nbl_wr32(void *priv, u64 reg, u32 value)
{
	struct nbl_hw_mgt *hw_mgt = (struct nbl_hw_mgt *)priv;

	rte_write32(rte_cpu_to_le_32(value), ((hw_mgt)->hw_addr + (reg)));
}

static void nbl_hw_update_tail_ptr(void *priv, u16 notify_qid, u16 tail_ptr)
{
	nbl_wr32(priv, NBL_NOTIFY_ADDR, ((u32)tail_ptr << NBL_TAIL_PTR_OFT | (u32)notify_qid));
}

static u8 *nbl_hw_get_tail_ptr(void *priv)
{
	struct nbl_hw_mgt *hw_mgt = (struct nbl_hw_mgt *)priv;

	return hw_mgt->hw_addr;
}

const struct nbl_hw_ops nbl_hw_ops = {
	.update_tail_ptr		= nbl_hw_update_tail_ptr,
	.get_tail_ptr			= nbl_hw_get_tail_ptr,
};

static int nbl_hw_setup_ops(struct nbl_hw_ops_tbl **hw_ops_tbl,
			    struct nbl_hw_mgt_leonis_snic *hw_mgt_leonis_snic)
{
	*hw_ops_tbl = calloc(1, sizeof(**hw_ops_tbl));
	if (!*hw_ops_tbl)
		return -ENOMEM;

	NBL_HW_OPS_TBL_TO_OPS(*hw_ops_tbl) = &nbl_hw_ops;
	NBL_HW_OPS_TBL_TO_PRIV(*hw_ops_tbl) = hw_mgt_leonis_snic;

	return 0;
}

static void nbl_hw_remove_ops(struct nbl_hw_ops_tbl **hw_ops_tbl)
{
	free(*hw_ops_tbl);
	*hw_ops_tbl = NULL;
}

int nbl_hw_init_leonis_snic(void *p)
{
	struct nbl_hw_mgt_leonis_snic **hw_mgt_leonis_snic;
	struct nbl_hw_mgt *hw_mgt;
	struct nbl_hw_ops_tbl **hw_ops_tbl;
	struct nbl_adapter *adapter = (struct nbl_adapter *)p;
	struct rte_pci_device *pci_dev = adapter->pci_dev;
	int ret = 0;

	hw_mgt_leonis_snic = (struct nbl_hw_mgt_leonis_snic **)&NBL_ADAPTER_TO_HW_MGT(adapter);
	hw_ops_tbl = &NBL_ADAPTER_TO_HW_OPS_TBL(adapter);

	*hw_mgt_leonis_snic = rte_zmalloc("nbl_hw_mgt", sizeof(**hw_mgt_leonis_snic), 0);
	if (!*hw_mgt_leonis_snic) {
		ret = -ENOMEM;
		goto alloc_hw_mgt_failed;
	}

	hw_mgt = &(*hw_mgt_leonis_snic)->hw_mgt;

	hw_mgt->hw_addr = (u8 *)pci_dev->mem_resource[0].addr;
	hw_mgt->memory_bar_pa = pci_dev->mem_resource[0].phys_addr;
	hw_mgt->mailbox_bar_hw_addr = (u8 *)pci_dev->mem_resource[2].addr;

	ret = nbl_hw_setup_ops(hw_ops_tbl, *hw_mgt_leonis_snic);
	if (ret)
		goto setup_ops_failed;

	return ret;

setup_ops_failed:
	rte_free(*hw_mgt_leonis_snic);
alloc_hw_mgt_failed:
	return ret;
}

void nbl_hw_remove_leonis_snic(void *p)
{
	struct nbl_hw_mgt_leonis_snic **hw_mgt_leonis_snic;
	struct nbl_hw_ops_tbl **hw_ops_tbl;
	struct nbl_adapter *adapter = (struct nbl_adapter *)p;

	hw_mgt_leonis_snic = (struct nbl_hw_mgt_leonis_snic **)&adapter->core.hw_mgt;
	hw_ops_tbl = &NBL_ADAPTER_TO_HW_OPS_TBL(adapter);

	rte_free(*hw_mgt_leonis_snic);

	nbl_hw_remove_ops(hw_ops_tbl);
}
