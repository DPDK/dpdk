/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 ZTE Corporation
 */

#include <stdint.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>

#include "zxdh_queue.h"
#include "zxdh_logs.h"
#include "zxdh_pci.h"
#include "zxdh_common.h"
#include "zxdh_msg.h"

struct rte_mbuf *
zxdh_virtqueue_detach_unused(struct zxdh_virtqueue *vq)
{
	struct rte_mbuf *cookie = NULL;
	int32_t          idx    = 0;

	if (vq == NULL)
		return NULL;

	for (idx = 0; idx < vq->vq_nentries; idx++) {
		cookie = vq->vq_descx[idx].cookie;
		if (cookie != NULL) {
			vq->vq_descx[idx].cookie = NULL;
			return cookie;
		}
	}
	return NULL;
}

static int32_t
zxdh_release_channel(struct rte_eth_dev *dev)
{
	struct zxdh_hw *hw = dev->data->dev_private;
	uint16_t nr_vq = hw->queue_num;
	uint32_t var  = 0;
	uint32_t addr = 0;
	uint32_t widx = 0;
	uint32_t bidx = 0;
	uint16_t pch  = 0;
	uint16_t lch  = 0;
	int32_t ret = 0;

	ret = zxdh_timedlock(hw, 1000);
	if (ret) {
		PMD_DRV_LOG(ERR, "Acquiring hw lock got failed, timeout");
		return -1;
	}

	for (lch = 0; lch < nr_vq; lch++) {
		if (hw->channel_context[lch].valid == 0) {
			PMD_DRV_LOG(DEBUG, "Logic channel %d does not need to release", lch);
			continue;
		}

		pch  = hw->channel_context[lch].ph_chno;
		widx = pch / 32;
		bidx = pch % 32;

		addr = ZXDH_QUERES_SHARE_BASE + (widx * sizeof(uint32_t));
		var  = zxdh_read_bar_reg(dev, ZXDH_BAR0_INDEX, addr);
		var &= ~(1 << bidx);
		zxdh_write_bar_reg(dev, ZXDH_BAR0_INDEX, addr, var);

		hw->channel_context[lch].valid = 0;
		hw->channel_context[lch].ph_chno = 0;
	}

	zxdh_release_lock(hw);

	return 0;
}

int32_t
zxdh_get_queue_type(uint16_t vtpci_queue_idx)
{
	if (vtpci_queue_idx % 2 == 0)
		return ZXDH_VTNET_RQ;
	else
		return ZXDH_VTNET_TQ;
}

int32_t
zxdh_free_queues(struct rte_eth_dev *dev)
{
	struct zxdh_hw *hw = dev->data->dev_private;
	uint16_t nr_vq = hw->queue_num;
	struct zxdh_virtqueue *vq = NULL;
	int32_t queue_type = 0;
	uint16_t i = 0;

	if (hw->vqs == NULL)
		return 0;

	if (zxdh_release_channel(dev) < 0) {
		PMD_DRV_LOG(ERR, "Failed to clear coi table");
		return -1;
	}

	for (i = 0; i < nr_vq; i++) {
		vq = hw->vqs[i];
		if (vq == NULL)
			continue;

		ZXDH_VTPCI_OPS(hw)->del_queue(hw, vq);
		queue_type = zxdh_get_queue_type(i);
		if (queue_type == ZXDH_VTNET_RQ) {
			rte_free(vq->sw_ring);
			rte_memzone_free(vq->rxq.mz);
		} else if (queue_type == ZXDH_VTNET_TQ) {
			rte_memzone_free(vq->txq.mz);
			rte_memzone_free(vq->txq.zxdh_net_hdr_mz);
		}

		rte_free(vq);
		hw->vqs[i] = NULL;
		PMD_DRV_LOG(DEBUG, "Release to queue %d success!", i);
	}

	rte_free(hw->vqs);
	hw->vqs = NULL;

	return 0;
}
