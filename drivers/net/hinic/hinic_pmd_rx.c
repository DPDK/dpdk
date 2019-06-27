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

/* rxq wq operations */
#define HINIC_GET_RQ_WQE_MASK(rxq)	\
	((rxq)->wq->mask)

#define HINIC_GET_RQ_LOCAL_CI(rxq)	\
	(((rxq)->wq->cons_idx) & HINIC_GET_RQ_WQE_MASK(rxq))

#define HINIC_GET_RQ_LOCAL_PI(rxq)	\
	(((rxq)->wq->prod_idx) & HINIC_GET_RQ_WQE_MASK(rxq))

#define HINIC_UPDATE_RQ_LOCAL_CI(rxq, wqebb_cnt)	\
	do {						\
		(rxq)->wq->cons_idx += (wqebb_cnt);	\
		(rxq)->wq->delta += (wqebb_cnt);	\
	} while (0)

#define HINIC_UPDATE_RQ_HW_PI(rxq, pi)	\
	(*((rxq)->pi_virt_addr) =	\
		cpu_to_be16((pi) & HINIC_GET_RQ_WQE_MASK(rxq)))

#define HINIC_GET_RQ_FREE_WQEBBS(rxq)	((rxq)->wq->delta - 1)

#define HINIC_RX_CSUM_OFFLOAD_EN	0xFFF

/* RQ_CTRL */
#define	RQ_CTRL_BUFDESC_SECT_LEN_SHIFT		0
#define	RQ_CTRL_COMPLETE_FORMAT_SHIFT		15
#define RQ_CTRL_COMPLETE_LEN_SHIFT		27
#define RQ_CTRL_LEN_SHIFT			29

#define	RQ_CTRL_BUFDESC_SECT_LEN_MASK		0xFFU
#define	RQ_CTRL_COMPLETE_FORMAT_MASK		0x1U
#define RQ_CTRL_COMPLETE_LEN_MASK		0x3U
#define RQ_CTRL_LEN_MASK			0x3U

#define RQ_CTRL_SET(val, member)		\
	(((val) & RQ_CTRL_##member##_MASK) << RQ_CTRL_##member##_SHIFT)

#define RQ_CTRL_GET(val, member)		\
	(((val) >> RQ_CTRL_##member##_SHIFT) & RQ_CTRL_##member##_MASK)

#define RQ_CTRL_CLEAR(val, member)		\
	((val) & (~(RQ_CTRL_##member##_MASK << RQ_CTRL_##member##_SHIFT)))


void hinic_get_func_rx_buf_size(struct hinic_nic_dev *nic_dev)
{
	struct hinic_rxq *rxq;
	u16 q_id;
	u16 buf_size = 0;

	for (q_id = 0; q_id < nic_dev->num_rq; q_id++) {
		rxq = nic_dev->rxqs[q_id];

		if (rxq == NULL)
			continue;

		if (q_id == 0)
			buf_size = rxq->buf_len;

		buf_size = buf_size > rxq->buf_len ? rxq->buf_len : buf_size;
	}

	nic_dev->hwdev->nic_io->rq_buf_size = buf_size;
}

int hinic_create_rq(struct hinic_hwdev *hwdev, u16 q_id, u16 rq_depth)
{
	int err;
	struct hinic_nic_io *nic_io = hwdev->nic_io;
	struct hinic_qp *qp = &nic_io->qps[q_id];
	struct hinic_rq *rq = &qp->rq;

	/* in case of hardware still generate interrupt, do not use msix 0 */
	rq->msix_entry_idx = 1;
	rq->q_id = q_id;
	rq->rq_depth = rq_depth;
	nic_io->rq_depth = rq_depth;

	err = hinic_wq_allocate(hwdev, &nic_io->rq_wq[q_id],
				HINIC_RQ_WQEBB_SHIFT, nic_io->rq_depth);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to allocate WQ for RQ");
		return err;
	}
	rq->wq = &nic_io->rq_wq[q_id];

	rq->pi_virt_addr =
		(volatile u16 *)dma_zalloc_coherent(hwdev, HINIC_PAGE_SIZE,
						    &rq->pi_dma_addr,
						    GFP_KERNEL);
	if (!rq->pi_virt_addr) {
		PMD_DRV_LOG(ERR, "Failed to allocate rq pi virt addr");
		err = -ENOMEM;
		goto rq_pi_alloc_err;
	}

	return HINIC_OK;

rq_pi_alloc_err:
	hinic_wq_free(hwdev, &nic_io->rq_wq[q_id]);

	return err;
}

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

static void
hinic_prepare_rq_wqe(void *wqe, __rte_unused u16 pi, dma_addr_t buf_addr,
			dma_addr_t cqe_dma)
{
	struct hinic_rq_wqe *rq_wqe = wqe;
	struct hinic_rq_ctrl *ctrl = &rq_wqe->ctrl;
	struct hinic_rq_cqe_sect *cqe_sect = &rq_wqe->cqe_sect;
	struct hinic_rq_bufdesc *buf_desc = &rq_wqe->buf_desc;
	u32 rq_ceq_len = sizeof(struct hinic_rq_cqe);

	ctrl->ctrl_fmt =
		RQ_CTRL_SET(SIZE_8BYTES(sizeof(*ctrl)),  LEN) |
		RQ_CTRL_SET(SIZE_8BYTES(sizeof(*cqe_sect)), COMPLETE_LEN) |
		RQ_CTRL_SET(SIZE_8BYTES(sizeof(*buf_desc)), BUFDESC_SECT_LEN) |
		RQ_CTRL_SET(RQ_COMPLETE_SGE, COMPLETE_FORMAT);

	hinic_set_sge(&cqe_sect->sge, cqe_dma, rq_ceq_len);

	buf_desc->addr_high = upper_32_bits(buf_addr);
	buf_desc->addr_low = lower_32_bits(buf_addr);
}

static int hinic_rx_alloc_cqe(struct hinic_rxq *rxq)
{
	size_t cqe_mem_size;

	/* allocate continuous cqe memory for saving number of memory zone */
	cqe_mem_size = sizeof(struct hinic_rq_cqe) * rxq->q_depth;
	rxq->cqe_start_vaddr =
		dma_zalloc_coherent(rxq->nic_dev->hwdev,
				    cqe_mem_size, &rxq->cqe_start_paddr,
				    GFP_KERNEL);
	if (!rxq->cqe_start_vaddr) {
		PMD_DRV_LOG(ERR, "Allocate cqe dma memory failed");
		return -ENOMEM;
	}

	rxq->rx_cqe = (struct hinic_rq_cqe *)rxq->cqe_start_vaddr;

	return HINIC_OK;
}

static void hinic_rx_free_cqe(struct hinic_rxq *rxq)
{
	size_t cqe_mem_size;

	cqe_mem_size = sizeof(struct hinic_rq_cqe) * rxq->q_depth;
	dma_free_coherent(rxq->nic_dev->hwdev, cqe_mem_size,
			  rxq->cqe_start_vaddr, rxq->cqe_start_paddr);
	rxq->cqe_start_vaddr = NULL;
}

static int hinic_rx_fill_wqe(struct hinic_rxq *rxq)
{
	struct hinic_nic_dev *nic_dev = rxq->nic_dev;
	struct hinic_rq_wqe *rq_wqe;
	dma_addr_t buf_dma_addr, cqe_dma_addr;
	u16 pi = 0;
	int i;

	buf_dma_addr = 0;
	cqe_dma_addr = rxq->cqe_start_paddr;
	for (i = 0; i < rxq->q_depth; i++) {
		rq_wqe = hinic_get_rq_wqe(nic_dev->hwdev, rxq->q_id, &pi);
		if (!rq_wqe) {
			PMD_DRV_LOG(ERR, "Get rq wqe failed");
			break;
		}

		hinic_prepare_rq_wqe(rq_wqe, pi, buf_dma_addr, cqe_dma_addr);
		cqe_dma_addr +=  sizeof(struct hinic_rq_cqe);

		hinic_cpu_to_be32(rq_wqe, sizeof(struct hinic_rq_wqe));
	}

	hinic_return_rq_wqe(nic_dev->hwdev, rxq->q_id, i);

	return i;
}

/* alloc cqe and prepare rqe */
int hinic_setup_rx_resources(struct hinic_rxq *rxq)
{
	u64 rx_info_sz;
	int err, pkts;

	rx_info_sz = rxq->q_depth * sizeof(*rxq->rx_info);
	rxq->rx_info = kzalloc_aligned(rx_info_sz, GFP_KERNEL);
	if (!rxq->rx_info)
		return -ENOMEM;

	err = hinic_rx_alloc_cqe(rxq);
	if (err) {
		PMD_DRV_LOG(ERR, "Allocate rx cqe failed");
		goto rx_cqe_err;
	}

	pkts = hinic_rx_fill_wqe(rxq);
	if (pkts != rxq->q_depth) {
		PMD_DRV_LOG(ERR, "Fill rx wqe failed");
		err = -ENOMEM;
		goto rx_fill_err;
	}

	return 0;

rx_fill_err:
	hinic_rx_free_cqe(rxq);

rx_cqe_err:
	kfree(rxq->rx_info);
	rxq->rx_info = NULL;

	return err;
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

void hinic_free_all_rx_mbuf(struct rte_eth_dev *eth_dev)
{
	struct hinic_nic_dev *nic_dev =
				HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(eth_dev);
	u16 q_id;

	for (q_id = 0; q_id < nic_dev->num_rq; q_id++)
		hinic_free_all_rx_skbs(nic_dev->rxqs[q_id]);
}

static void hinic_rss_deinit(struct hinic_nic_dev *nic_dev)
{
	u8 prio_tc[HINIC_DCB_UP_MAX] = {0};
	(void)hinic_rss_cfg(nic_dev->hwdev, 0,
			    nic_dev->rss_tmpl_idx, 0, prio_tc);
}

static int hinic_rss_key_init(struct hinic_nic_dev *nic_dev,
			      struct rte_eth_rss_conf *rss_conf)
{
	u8 default_rss_key[HINIC_RSS_KEY_SIZE] = {
			 0x6d, 0x5a, 0x56, 0xda, 0x25, 0x5b, 0x0e, 0xc2,
			 0x41, 0x67, 0x25, 0x3d, 0x43, 0xa3, 0x8f, 0xb0,
			 0xd0, 0xca, 0x2b, 0xcb, 0xae, 0x7b, 0x30, 0xb4,
			 0x77, 0xcb, 0x2d, 0xa3, 0x80, 0x30, 0xf2, 0x0c,
			 0x6a, 0x42, 0xb7, 0x3b, 0xbe, 0xac, 0x01, 0xfa};
	u8 hashkey[HINIC_RSS_KEY_SIZE] = {0};
	u8 tmpl_idx = nic_dev->rss_tmpl_idx;

	if (rss_conf->rss_key == NULL)
		memcpy(hashkey, default_rss_key, HINIC_RSS_KEY_SIZE);
	else
		memcpy(hashkey, rss_conf->rss_key, rss_conf->rss_key_len);

	return hinic_rss_set_template_tbl(nic_dev->hwdev, tmpl_idx, hashkey);
}

static void hinic_fill_rss_type(struct nic_rss_type *rss_type,
				struct rte_eth_rss_conf *rss_conf)
{
	u64 rss_hf = rss_conf->rss_hf;

	rss_type->ipv4 = (rss_hf & (ETH_RSS_IPV4 | ETH_RSS_FRAG_IPV4)) ? 1 : 0;
	rss_type->tcp_ipv4 = (rss_hf & ETH_RSS_NONFRAG_IPV4_TCP) ? 1 : 0;
	rss_type->ipv6 = (rss_hf & (ETH_RSS_IPV6 | ETH_RSS_FRAG_IPV6)) ? 1 : 0;
	rss_type->ipv6_ext = (rss_hf & ETH_RSS_IPV6_EX) ? 1 : 0;
	rss_type->tcp_ipv6 = (rss_hf & ETH_RSS_NONFRAG_IPV6_TCP) ? 1 : 0;
	rss_type->tcp_ipv6_ext = (rss_hf & ETH_RSS_IPV6_TCP_EX) ? 1 : 0;
	rss_type->udp_ipv4 = (rss_hf & ETH_RSS_NONFRAG_IPV4_UDP) ? 1 : 0;
	rss_type->udp_ipv6 = (rss_hf & ETH_RSS_NONFRAG_IPV6_UDP) ? 1 : 0;
}

static void hinic_fillout_indir_tbl(struct hinic_nic_dev *nic_dev, u32 *indir)
{
	u8 rss_queue_count = nic_dev->num_rss;
	int i = 0, j;

	if (rss_queue_count == 0) {
		/* delete q_id from indir tbl */
		for (i = 0; i < HINIC_RSS_INDIR_SIZE; i++)
			indir[i] = 0xFF;	/* Invalid value in indir tbl */
	} else {
		while (i < HINIC_RSS_INDIR_SIZE)
			for (j = 0; (j < rss_queue_count) &&
			     (i < HINIC_RSS_INDIR_SIZE); j++)
				indir[i++] = nic_dev->rx_queue_list[j];
	}
}

static int hinic_rss_init(struct hinic_nic_dev *nic_dev,
			  __attribute__((unused)) u8 *rq2iq_map,
			  struct rte_eth_rss_conf *rss_conf)
{
	u32 indir_tbl[HINIC_RSS_INDIR_SIZE] = {0};
	struct nic_rss_type rss_type = {0};
	u8 prio_tc[HINIC_DCB_UP_MAX] = {0};
	u8 tmpl_idx = 0xFF, num_tc = 0;
	int err;

	tmpl_idx = nic_dev->rss_tmpl_idx;

	err = hinic_rss_key_init(nic_dev, rss_conf);
	if (err)
		return err;

	if (!nic_dev->rss_indir_flag) {
		hinic_fillout_indir_tbl(nic_dev, indir_tbl);
		err = hinic_rss_set_indir_tbl(nic_dev->hwdev, tmpl_idx,
					      indir_tbl);
		if (err)
			return err;
	}

	hinic_fill_rss_type(&rss_type, rss_conf);
	err = hinic_set_rss_type(nic_dev->hwdev, tmpl_idx, rss_type);
	if (err)
		return err;

	err = hinic_rss_set_hash_engine(nic_dev->hwdev, tmpl_idx,
					HINIC_RSS_HASH_ENGINE_TYPE_TOEP);
	if (err)
		return err;

	return hinic_rss_cfg(nic_dev->hwdev, 1, tmpl_idx, num_tc, prio_tc);
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

int hinic_rx_configure(struct rte_eth_dev *dev)
{
	struct hinic_nic_dev *nic_dev = HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	struct rte_eth_rss_conf rss_conf =
		dev->data->dev_conf.rx_adv_conf.rss_conf;
	u32 csum_en = 0;
	int err;

	if (nic_dev->flags & ETH_MQ_RX_RSS_FLAG) {
		if (rss_conf.rss_hf == 0) {
			rss_conf.rss_hf = HINIC_RSS_OFFLOAD_ALL;
		} else if ((rss_conf.rss_hf & HINIC_RSS_OFFLOAD_ALL) == 0) {
			PMD_DRV_LOG(ERR, "Do not support rss offload all");
			goto rss_config_err;
		}

		err = hinic_rss_init(nic_dev, NULL, &rss_conf);
		if (err) {
			PMD_DRV_LOG(ERR, "Init rss failed");
			goto rss_config_err;
		}
	}

	/* Enable both L3/L4 rx checksum offload */
	if (dev->data->dev_conf.rxmode.offloads & DEV_RX_OFFLOAD_CHECKSUM)
		csum_en = HINIC_RX_CSUM_OFFLOAD_EN;

	err = hinic_set_rx_csum_offload(nic_dev->hwdev, csum_en);
	if (err)
		goto rx_csum_ofl_err;

	return 0;

rx_csum_ofl_err:
rss_config_err:
	hinic_destroy_num_qps(nic_dev);

	return HINIC_ERROR;
}

void hinic_rx_remove_configure(struct rte_eth_dev *dev)
{
	struct hinic_nic_dev *nic_dev = HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);

	if (nic_dev->flags & ETH_MQ_RX_RSS_FLAG) {
		hinic_rss_deinit(nic_dev);
		hinic_destroy_num_qps(nic_dev);
	}
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

static struct rte_mbuf *hinic_rx_alloc_mbuf(struct hinic_rxq *rxq,
					dma_addr_t *dma_addr)
{
	struct rte_mbuf *mbuf;

	mbuf = rte_mbuf_raw_alloc(rxq->mb_pool);
	if (unlikely(!mbuf))
		return NULL;

	*dma_addr = rte_mbuf_data_iova_default(mbuf);

	return mbuf;
}

void hinic_rx_alloc_pkts(struct hinic_rxq *rxq)
{
	struct hinic_nic_dev *nic_dev = rxq->nic_dev;
	struct hinic_rq_wqe *rq_wqe;
	struct hinic_rx_info *rx_info;
	struct rte_mbuf *mb;
	dma_addr_t dma_addr;
	u16 pi = 0;
	int i, free_wqebbs;

	free_wqebbs = HINIC_GET_RQ_FREE_WQEBBS(rxq);
	for (i = 0; i < free_wqebbs; i++) {
		mb = hinic_rx_alloc_mbuf(rxq, &dma_addr);
		if (unlikely(!mb)) {
			rxq->rxq_stats.rx_nombuf++;
			break;
		}

		rq_wqe = hinic_get_rq_wqe(nic_dev->hwdev, rxq->q_id, &pi);
		if (unlikely(!rq_wqe)) {
			rte_pktmbuf_free(mb);
			break;
		}

		/* fill buffer address only */
		rq_wqe->buf_desc.addr_high =
				cpu_to_be32(upper_32_bits(dma_addr));
		rq_wqe->buf_desc.addr_low =
				cpu_to_be32(lower_32_bits(dma_addr));

		rx_info = &rxq->rx_info[pi];
		rx_info->mbuf = mb;
	}

	if (likely(i > 0)) {
		rte_wmb();
		HINIC_UPDATE_RQ_HW_PI(rxq, pi + 1);
	}
}
