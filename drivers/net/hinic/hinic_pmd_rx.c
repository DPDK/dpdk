/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Huawei Technologies Co., Ltd
 */

#include <rte_ether.h>
#include <rte_mbuf.h>

#include "base/hinic_compat.h"
#include "base/hinic_pmd_hwdev.h"
#include "base/hinic_pmd_wq.h"
#include "base/hinic_pmd_niccfg.h"
#include "base/hinic_pmd_nicio.h"
#include "hinic_pmd_ethdev.h"
#include "hinic_pmd_rx.h"


void hinic_destroy_rq(struct hinic_hwdev *hwdev, u16 q_id)
{
	struct hinic_nic_io *nic_io = hwdev->nic_io;
	struct hinic_qp *qp = &nic_io->qps[q_id];
	struct hinic_rq *rq = &qp->rq;

	if (qp->rq.wq == NULL)
		return;

	dma_free_coherent_volatile(hwdev, HINIC_PAGE_SIZE,
				   (volatile void *)rq->pi_virt_addr,
				   rq->pi_dma_addr);
	hinic_wq_free(nic_io->hwdev, qp->rq.wq);
	qp->rq.wq = NULL;
}

static void hinic_rx_free_cqe(struct hinic_rxq *rxq)
{
	size_t cqe_mem_size;

	cqe_mem_size = sizeof(struct hinic_rq_cqe) * rxq->q_depth;
	dma_free_coherent(rxq->nic_dev->hwdev, cqe_mem_size,
			  rxq->cqe_start_vaddr, rxq->cqe_start_paddr);
	rxq->cqe_start_vaddr = NULL;
}

void hinic_free_rx_resources(struct hinic_rxq *rxq)
{
	if (rxq->rx_info == NULL)
		return;

	hinic_rx_free_cqe(rxq);
	kfree(rxq->rx_info);
	rxq->rx_info = NULL;
}

void hinic_free_all_rx_resources(struct rte_eth_dev *eth_dev)
{
	u16 q_id;
	struct hinic_nic_dev *nic_dev =
				HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(eth_dev);

	for (q_id = 0; q_id < nic_dev->num_rq; q_id++) {
		eth_dev->data->rx_queues[q_id] = NULL;

		if (nic_dev->rxqs[q_id] == NULL)
			continue;

		hinic_free_all_rx_skbs(nic_dev->rxqs[q_id]);
		hinic_free_rx_resources(nic_dev->rxqs[q_id]);
		kfree(nic_dev->rxqs[q_id]);
		nic_dev->rxqs[q_id] = NULL;
	}
}

static void
hinic_add_rq_to_rx_queue_list(struct hinic_nic_dev *nic_dev, u16 queue_id)
{
	u8 rss_queue_count = nic_dev->num_rss;

	RTE_ASSERT(rss_queue_count <= (RTE_DIM(nic_dev->rx_queue_list) - 1));

	nic_dev->rx_queue_list[rss_queue_count] = queue_id;
	nic_dev->num_rss++;
}

/**
 * hinic_setup_num_qps - determine num_qps from rss_tmpl_id
 * @nic_dev: pointer to the private ethernet device
 * Return: 0 on Success, error code otherwise.
 **/
static int hinic_setup_num_qps(struct hinic_nic_dev *nic_dev)
{
	int err, i;

	if (!(nic_dev->flags & ETH_MQ_RX_RSS_FLAG)) {
		nic_dev->flags &= ~ETH_MQ_RX_RSS_FLAG;
		nic_dev->num_rss = 0;
		if (nic_dev->num_rq > 1) {
			/* get rss template id */
			err = hinic_rss_template_alloc(nic_dev->hwdev,
						       &nic_dev->rss_tmpl_idx);
			if (err) {
				PMD_DRV_LOG(WARNING, "Alloc rss template failed");
				return err;
			}
			nic_dev->flags |= ETH_MQ_RX_RSS_FLAG;
			for (i = 0; i < nic_dev->num_rq; i++)
				hinic_add_rq_to_rx_queue_list(nic_dev, i);
		}
	}

	return 0;
}

static void hinic_destroy_num_qps(struct hinic_nic_dev *nic_dev)
{
	if (nic_dev->flags & ETH_MQ_RX_RSS_FLAG) {
		if (hinic_rss_template_free(nic_dev->hwdev,
					    nic_dev->rss_tmpl_idx))
			PMD_DRV_LOG(WARNING, "Free rss template failed");

		nic_dev->flags &= ~ETH_MQ_RX_RSS_FLAG;
	}
}

static int hinic_config_mq_rx_rss(struct hinic_nic_dev *nic_dev, bool on)
{
	int ret = 0;

	if (on) {
		ret = hinic_setup_num_qps(nic_dev);
		if (ret)
			PMD_DRV_LOG(ERR, "Setup num_qps failed");
	} else {
		hinic_destroy_num_qps(nic_dev);
	}

	return ret;
}

int hinic_config_mq_mode(struct rte_eth_dev *dev, bool on)
{
	struct hinic_nic_dev *nic_dev = HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	struct rte_eth_conf *dev_conf = &dev->data->dev_conf;
	int ret = 0;

	switch (dev_conf->rxmode.mq_mode) {
	case ETH_MQ_RX_RSS:
		ret = hinic_config_mq_rx_rss(nic_dev, on);
		break;
	default:
		break;
	}

	return ret;
}

void hinic_free_all_rx_skbs(struct hinic_rxq *rxq)
{
	struct hinic_nic_dev *nic_dev = rxq->nic_dev;
	struct hinic_rx_info *rx_info;
	int free_wqebbs =
		hinic_get_rq_free_wqebbs(nic_dev->hwdev, rxq->q_id) + 1;
	volatile struct hinic_rq_cqe *rx_cqe;
	u16 ci;

	while (free_wqebbs++ < rxq->q_depth) {
		ci = hinic_get_rq_local_ci(nic_dev->hwdev, rxq->q_id);

		rx_cqe = &rxq->rx_cqe[ci];

		/* clear done bit */
		rx_cqe->status = 0;

		rx_info = &rxq->rx_info[ci];
		rte_pktmbuf_free(rx_info->mbuf);
		rx_info->mbuf = NULL;

		hinic_update_rq_local_ci(nic_dev->hwdev, rxq->q_id, 1);
	}
}
