/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C), 2025, Wuxi Stars Micro System Technologies Co., Ltd.
 */

#include <rte_os.h>
#include <rte_tailq.h>
#include <rte_malloc.h>
#include "sxe2_ethdev.h"
#include "sxe2_vsi.h"
#include "sxe2_common_log.h"
#include "sxe2_cmd_chnl.h"

void sxe2_sw_vsi_ctx_hw_cap_set(struct sxe2_adapter *adapter,
		struct sxe2_drv_vsi_caps *vsi_caps)
{
	adapter->vsi_ctxt.dpdk_vsi_id = vsi_caps->dpdk_vsi_id;
	adapter->vsi_ctxt.kernel_vsi_id = vsi_caps->kernel_vsi_id;
	adapter->vsi_ctxt.vsi_type = vsi_caps->vsi_type;
}

static struct sxe2_vsi *
sxe2_vsi_node_alloc(struct sxe2_adapter *adapter, uint16_t vsi_id, uint16_t vsi_type)
{
	struct sxe2_vsi *vsi = NULL;
	vsi = rte_zmalloc("sxe2_vsi", sizeof(*vsi), 0);
	if (vsi == NULL) {
		PMD_LOG_ERR(DRV, "Failed to malloc vf vsi struct.");
		goto l_end;
	}
	vsi->adapter = adapter;

	vsi->vsi_id = vsi_id;
	vsi->vsi_type = vsi_type;

l_end:
	return vsi;
}

static void sxe2_vsi_queues_num_set(struct sxe2_vsi *vsi, uint16_t num_queues, uint16_t base_idx)
{
	vsi->txqs.q_cnt = num_queues;
	vsi->rxqs.q_cnt = num_queues;
	vsi->txqs.base_idx_in_func = base_idx;
	vsi->rxqs.base_idx_in_func = base_idx;
}

static void sxe2_vsi_queues_cfg(struct sxe2_vsi *vsi)
{
	vsi->txqs.depth = vsi->txqs.depth ? : SXE2_DFLT_NUM_TX_DESC;
	vsi->rxqs.depth = vsi->rxqs.depth ? : SXE2_DFLT_NUM_RX_DESC;

	PMD_LOG_INFO(DRV, "vsi:%u queue_cnt:%u txq_depth:%u rxq_depth:%u.",
			vsi->vsi_id, vsi->txqs.q_cnt,
			vsi->txqs.depth, vsi->rxqs.depth);
}

static void sxe2_vsi_irqs_cfg(struct sxe2_vsi *vsi, uint16_t num_irqs, uint16_t base_idx)
{
	vsi->irqs.avail_cnt = num_irqs;
	vsi->irqs.base_idx_in_pf = base_idx;
}

static struct sxe2_vsi *sxe2_vsi_node_create(struct sxe2_adapter *adapter,
					     uint16_t vsi_id,
					     uint16_t vsi_type)
{
	struct sxe2_vsi *vsi = NULL;
	uint16_t num_queues = 0;
	uint16_t queue_base_idx = 0;
	uint16_t num_irqs = 0;
	uint16_t irq_base_idx = 0;

	vsi = sxe2_vsi_node_alloc(adapter, vsi_id, vsi_type);
	if (vsi == NULL)
		goto l_end;

	if (vsi_type == SXE2_VSI_T_DPDK_PF ||
			vsi_type == SXE2_VSI_T_DPDK_VF) {
		num_queues = adapter->q_ctxt.qp_cnt_assign;
		queue_base_idx = adapter->q_ctxt.base_idx_in_pf;

		num_irqs = adapter->irq_ctxt.max_cnt_hw;
		irq_base_idx = adapter->irq_ctxt.base_idx_in_func;
	} else if (vsi_type == SXE2_VSI_T_DPDK_ESW) {
		num_queues = 1;
		num_irqs = 1;
	}

	sxe2_vsi_queues_num_set(vsi, num_queues, queue_base_idx);

	sxe2_vsi_queues_cfg(vsi);

	sxe2_vsi_irqs_cfg(vsi, num_irqs, irq_base_idx);

l_end:
	return vsi;
}

static void sxe2_vsi_node_free(struct sxe2_vsi *vsi)
{
	struct sxe2_adapter *adapter;

	if (!vsi)
		return;

	adapter = vsi->adapter;
	if (vsi->vsi_type == SXE2_VSI_T_ESW)
		TAILQ_REMOVE(&adapter->vsi_ctxt.other_vsi_list, vsi, next);

	rte_free(vsi);
	vsi = NULL;
}

static int32_t sxe2_vsi_destroy(struct sxe2_adapter *adapter, struct sxe2_vsi *vsi)
{
	int32_t ret = 0;

	if (vsi == NULL) {
		PMD_LOG_INFO(DRV, "vsi is not created, no need to destroy.");
		goto l_end;
	}

	if (vsi->vsi_type != SXE2_VSI_T_DPDK_ESW) {
		ret = sxe2_drv_vsi_del(adapter, vsi);
		if (ret) {
			PMD_LOG_ERR(DRV, "Failed to del vsi from fw, ret=%d", ret);
			if (ret == -EPERM)
				goto l_free;
			goto l_end;
		}
	}

l_free:
	rte_free(vsi);
	vsi = NULL;

	PMD_LOG_DEBUG(DRV, "vsi destroyed.");
l_end:
	return ret;
}

static int32_t sxe2_main_vsi_create(struct sxe2_adapter *adapter)
{
	int32_t ret = 0;
	uint16_t vsi_id = adapter->vsi_ctxt.dpdk_vsi_id;
	uint16_t vsi_type = adapter->vsi_ctxt.vsi_type;
	bool is_reused = (vsi_id != SXE2_INVALID_VSI_ID);

	PMD_INIT_FUNC_TRACE();

	if (!is_reused)
		vsi_type = SXE2_VSI_T_DPDK_PF;
	else
		PMD_LOG_INFO(DRV, "Reusing existing HW vsi_id:%u", vsi_id);

	adapter->vsi_ctxt.main_vsi = sxe2_vsi_node_create(adapter, vsi_id, vsi_type);
	if (adapter->vsi_ctxt.main_vsi == NULL) {
		PMD_LOG_ERR(DRV, "Failed to create vsi struct, ret=%d", ret);
		ret = -ENOMEM;
		goto l_end;
	}

	if (!is_reused) {
		ret = sxe2_drv_vsi_add(adapter, adapter->vsi_ctxt.main_vsi);
		if (ret) {
			PMD_LOG_ERR(DRV, "Failed to config vsi to fw, ret=%d", ret);
			goto l_free_vsi;
		}

		adapter->vsi_ctxt.dpdk_vsi_id = adapter->vsi_ctxt.main_vsi->vsi_id;
		PMD_LOG_DEBUG(DRV, "Successfully created and synced new VSI");
	}

	goto l_end;

l_free_vsi:
	sxe2_vsi_node_free(adapter->vsi_ctxt.main_vsi);
	adapter->vsi_ctxt.main_vsi = NULL;
l_end:
	return ret;
}

int32_t sxe2_other_vsi_create(struct sxe2_adapter *adapter, uint16_t cnt_vf)
{
	int32_t ret = 0;
	struct sxe2_vsi *other_vsi = NULL;
	uint16_t vsi_id = SXE2_INVALID_VSI_ID;

	PMD_INIT_FUNC_TRACE();

	other_vsi = sxe2_vsi_node_create(adapter, SXE2_INVALID_VSI_ID, SXE2_VSI_T_DPDK_ESW);
	if (other_vsi == NULL) {
		ret = -ENOMEM;
		goto l_end;
	}

	ret = sxe2_drv_switchdev_cpvsi_get(adapter, &vsi_id);
	if (ret) {
		PMD_LOG_ERR(DRV, "Failed to query vsi from fw, ret=%d", ret);
		goto l_free_vsi;
	}

	other_vsi->vsi_id = vsi_id;
	other_vsi->vsi_type = SXE2_VSI_T_DPDK_ESW;

	ret = sxe2_drv_vsi_info_get(adapter, other_vsi);
	if (ret) {
		PMD_LOG_ERR(DRV, "Failed to query vsi info from fw, ret=%d", ret);
		goto l_free_vsi;
	}

	other_vsi->txqs.q_cnt = RTE_MIN(cnt_vf, other_vsi->txqs.q_cnt);
	other_vsi->rxqs.q_cnt = RTE_MIN(cnt_vf, other_vsi->rxqs.q_cnt);
	other_vsi->irqs.avail_cnt = RTE_MIN(cnt_vf, other_vsi->irqs.avail_cnt);

	TAILQ_INSERT_TAIL(&adapter->vsi_ctxt.other_vsi_list, other_vsi, next);
	goto l_end;

l_free_vsi:
	sxe2_vsi_node_free(other_vsi);
l_end:
	return ret;
}

int32_t sxe2_vsi_init(struct rte_eth_dev *dev)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	int32_t ret = 0;
	uint16_t srcvsi_list[SXE2_SRCVSI_PRUNE_MAX_NUM];
	uint16_t srcvsi_cnt;

	PMD_INIT_FUNC_TRACE();

	ret = sxe2_main_vsi_create(adapter);
	if (ret) {
		PMD_LOG_ERR(DRV, "Failed to create main VSI, ret=%d", ret);
		goto l_end;
	}

	if (adapter->vsi_ctxt.kernel_vsi_id != SXE2_INVALID_VSI_ID) {
		srcvsi_list[0] = adapter->vsi_ctxt.kernel_vsi_id;
		srcvsi_list[1] = adapter->vsi_ctxt.dpdk_vsi_id;
		srcvsi_cnt = 2;
		ret = sxe2_drv_srcvsi_prune_config(adapter, srcvsi_list, srcvsi_cnt, true);
		if (ret) {
			PMD_LOG_ERR(DRV, "Failed to set src vsi to fw, ret=%d", ret);
			goto l_end;
		}
		PMD_LOG_DEBUG(DRV, "Successfully set src vsi");
	}

l_end:
	return ret;
}

void sxe2_vsi_uninit(struct rte_eth_dev *dev)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	struct sxe2_vsi *var, *tvar;
	int32_t ret;
	uint16_t srcvsi_list[SXE2_SRCVSI_PRUNE_MAX_NUM];
	uint16_t srcvsi_cnt;

	if (adapter->vsi_ctxt.main_vsi == NULL) {
		PMD_LOG_INFO(DRV, "vsi is not created, no need to destroy.");
		goto l_end;
	}

	if (adapter->vsi_ctxt.kernel_vsi_id != SXE2_INVALID_VSI_ID) {
		srcvsi_list[0] = adapter->vsi_ctxt.kernel_vsi_id;
		srcvsi_list[1] = adapter->vsi_ctxt.dpdk_vsi_id;
		srcvsi_cnt = 2;
		ret = sxe2_drv_srcvsi_prune_config(adapter, srcvsi_list,
						   srcvsi_cnt, false);
		if (ret) {
			PMD_LOG_ERR(DRV, "Failed to clear src vsi to fw, ret=%d", ret);
			if (ret == -EPERM)
				goto l_free;
			goto l_end;
		}
		PMD_LOG_DEBUG(DRV, "Successfully clear src vsi");
	}

l_free:
	ret = sxe2_vsi_destroy(adapter, adapter->vsi_ctxt.main_vsi);
	if (ret) {
		PMD_LOG_ERR(DRV, "Failed to del vsi from fw, ret=%d", ret);
		goto l_end;
	}
	RTE_TAILQ_FOREACH_SAFE(var, &adapter->vsi_ctxt.other_vsi_list, next, tvar) {
		ret = sxe2_vsi_destroy(adapter, var);
		if (ret) {
			PMD_LOG_ERR(DRV, "Failed to del vsi from fw, ret=%d", ret);
			break;
		}
	}

	PMD_LOG_DEBUG(DRV, "vsi destroyed.");

l_end:
	return;
}

int32_t sxe2_vsi_repr_main_vsi_create(struct rte_eth_dev *dev,
				  struct sxe2_adapter *parent_adapter,
				  uint16_t repr_id)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	struct sxe2_vsi *vsi = NULL;
	struct sxe2_vsi *pos;
	int32_t ret = 0;

	TAILQ_FOREACH(pos, &parent_adapter->vsi_ctxt.other_vsi_list, next) {
		if (pos->vsi_type == SXE2_VSI_T_DPDK_ESW) {
			vsi = pos;
			break;
		}
	}

	if (vsi == NULL) {
		PMD_LOG_ERR(INIT, "Failed to get dpdk vsi.");
		ret = -EINVAL;
		goto l_end;
	}

	TAILQ_INIT(&adapter->vsi_ctxt.other_vsi_list);

	adapter->vsi_ctxt.vsi_type = SXE2_VSI_T_DPDK_ESW;
	adapter->vsi_ctxt.kernel_vsi_id = SXE2_INVALID_VSI_ID;

	adapter->cdev = parent_adapter->cdev;

	adapter->q_ctxt.base_idx_in_pf = vsi->txqs.base_idx_in_func +
		RTE_MIN(vsi->txqs.q_cnt, repr_id);
	adapter->irq_ctxt.base_idx_in_func = vsi->irqs.base_idx_in_pf +
		RTE_MIN(vsi->irqs.avail_cnt, repr_id);
	adapter->q_ctxt.qp_cnt_assign = RTE_MIN(vsi->txqs.q_cnt, 1);
	adapter->irq_ctxt.max_cnt_hw = RTE_MIN(vsi->irqs.avail_cnt, 1);

	adapter->vsi_ctxt.main_vsi =
		sxe2_vsi_node_create(adapter, vsi->vsi_id, SXE2_VSI_T_DPDK_ESW);
	if (adapter->vsi_ctxt.main_vsi == NULL) {
		ret = -ENOMEM;
		PMD_LOG_ERR(DRV, "Failed to create vsi struct, ret=%d", ret);
		goto l_end;
	}
	adapter->vsi_ctxt.dpdk_vsi_id = adapter->vsi_ctxt.main_vsi->vsi_id;

	ret = 0;

l_end:
	return ret;
}

void sxe2_vsi_repr_main_vsi_destroy(struct rte_eth_dev *dev)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);

	sxe2_vsi_node_free(adapter->vsi_ctxt.main_vsi);
}
