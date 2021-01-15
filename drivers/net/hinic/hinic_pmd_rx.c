/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Huawei Technologies Co., Ltd
 */

#include <rte_ether.h>
#include <rte_mbuf.h>
#ifdef RTE_ARCH_ARM64
#include <arm_neon.h>
#endif

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

/* rxq cqe done and status bit */
#define HINIC_GET_RX_DONE_BE(status)	\
	((status) & 0x80U)

#define HINIC_RX_CSUM_OFFLOAD_EN	0xFFF

#define RQ_CQE_SGE_VLAN_SHIFT			0
#define RQ_CQE_SGE_LEN_SHIFT			16

#define RQ_CQE_SGE_VLAN_MASK			0xFFFFU
#define RQ_CQE_SGE_LEN_MASK			0xFFFFU

#define RQ_CQE_SGE_GET(val, member)		\
	(((val) >> RQ_CQE_SGE_##member##_SHIFT) & RQ_CQE_SGE_##member##_MASK)

#define HINIC_GET_RX_VLAN_TAG(vlan_len)	\
		RQ_CQE_SGE_GET(vlan_len, VLAN)

#define HINIC_GET_RX_PKT_LEN(vlan_len)	\
		RQ_CQE_SGE_GET(vlan_len, LEN)

#define RQ_CQE_STATUS_CSUM_ERR_SHIFT		0
#define RQ_CQE_STATUS_NUM_LRO_SHIFT		16
#define RQ_CQE_STATUS_LRO_PUSH_SHIFT		25
#define RQ_CQE_STATUS_LRO_ENTER_SHIFT		26
#define RQ_CQE_STATUS_LRO_INTR_SHIFT		27

#define RQ_CQE_STATUS_BP_EN_SHIFT		30
#define RQ_CQE_STATUS_RXDONE_SHIFT		31
#define RQ_CQE_STATUS_FLUSH_SHIFT		28

#define RQ_CQE_STATUS_CSUM_ERR_MASK		0xFFFFU
#define RQ_CQE_STATUS_NUM_LRO_MASK		0xFFU
#define RQ_CQE_STATUS_LRO_PUSH_MASK		0X1U
#define RQ_CQE_STATUS_LRO_ENTER_MASK		0X1U
#define RQ_CQE_STATUS_LRO_INTR_MASK		0X1U
#define RQ_CQE_STATUS_BP_EN_MASK		0X1U
#define RQ_CQE_STATUS_RXDONE_MASK		0x1U
#define RQ_CQE_STATUS_FLUSH_MASK		0x1U

#define RQ_CQE_STATUS_GET(val, member)		\
		(((val) >> RQ_CQE_STATUS_##member##_SHIFT) & \
				RQ_CQE_STATUS_##member##_MASK)

#define RQ_CQE_STATUS_CLEAR(val, member)	\
		((val) & (~(RQ_CQE_STATUS_##member##_MASK << \
				RQ_CQE_STATUS_##member##_SHIFT)))

#define HINIC_GET_RX_CSUM_ERR(status)	\
		RQ_CQE_STATUS_GET(status, CSUM_ERR)

#define HINIC_GET_RX_DONE(status)	\
		RQ_CQE_STATUS_GET(status, RXDONE)

#define HINIC_GET_RX_FLUSH(status)	\
		RQ_CQE_STATUS_GET(status, FLUSH)

#define HINIC_GET_RX_BP_EN(status)	\
		RQ_CQE_STATUS_GET(status, BP_EN)

#define HINIC_GET_RX_NUM_LRO(status)	\
		RQ_CQE_STATUS_GET(status, NUM_LRO)

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

#define RQ_CQE_PKT_NUM_SHIFT			1
#define RQ_CQE_PKT_FIRST_LEN_SHIFT		19
#define RQ_CQE_PKT_LAST_LEN_SHIFT		6
#define RQ_CQE_SUPER_CQE_EN_SHIFT		0

#define RQ_CQE_PKT_FIRST_LEN_MASK		0x1FFFU
#define RQ_CQE_PKT_LAST_LEN_MASK		0x1FFFU
#define RQ_CQE_PKT_NUM_MASK			0x1FU
#define RQ_CQE_SUPER_CQE_EN_MASK		0x1

#define RQ_CQE_PKT_NUM_GET(val, member)		\
	(((val) >> RQ_CQE_PKT_##member##_SHIFT) & RQ_CQE_PKT_##member##_MASK)

#define HINIC_GET_RQ_CQE_PKT_NUM(pkt_info) RQ_CQE_PKT_NUM_GET(pkt_info, NUM)

#define RQ_CQE_SUPER_CQE_EN_GET(val, member)	\
	(((val) >> RQ_CQE_##member##_SHIFT) & RQ_CQE_##member##_MASK)

#define HINIC_GET_SUPER_CQE_EN(pkt_info)	\
	RQ_CQE_SUPER_CQE_EN_GET(pkt_info, SUPER_CQE_EN)

#define RQ_CQE_OFFOLAD_TYPE_VLAN_EN_SHIFT		21
#define RQ_CQE_OFFOLAD_TYPE_VLAN_EN_MASK		0x1U

#define RQ_CQE_OFFOLAD_TYPE_PKT_TYPE_SHIFT		0
#define RQ_CQE_OFFOLAD_TYPE_PKT_TYPE_MASK		0xFFFU

#define RQ_CQE_OFFOLAD_TYPE_PKT_UMBCAST_SHIFT		19
#define RQ_CQE_OFFOLAD_TYPE_PKT_UMBCAST_MASK		0x3U

#define RQ_CQE_OFFOLAD_TYPE_RSS_TYPE_SHIFT		24
#define RQ_CQE_OFFOLAD_TYPE_RSS_TYPE_MASK		0xFFU

#define RQ_CQE_OFFOLAD_TYPE_GET(val, member)		(((val) >> \
				RQ_CQE_OFFOLAD_TYPE_##member##_SHIFT) & \
				RQ_CQE_OFFOLAD_TYPE_##member##_MASK)

#define HINIC_GET_RX_VLAN_OFFLOAD_EN(offload_type)	\
		RQ_CQE_OFFOLAD_TYPE_GET(offload_type, VLAN_EN)

#define HINIC_GET_RSS_TYPES(offload_type)	\
		RQ_CQE_OFFOLAD_TYPE_GET(offload_type, RSS_TYPE)

#define HINIC_GET_RX_PKT_TYPE(offload_type)	\
		RQ_CQE_OFFOLAD_TYPE_GET(offload_type, PKT_TYPE)

#define HINIC_GET_RX_PKT_UMBCAST(offload_type)	\
		RQ_CQE_OFFOLAD_TYPE_GET(offload_type, PKT_UMBCAST)

#define RQ_CQE_STATUS_CSUM_BYPASS_VAL			0x80U
#define RQ_CQE_STATUS_CSUM_ERR_IP_MASK			0x39U
#define RQ_CQE_STATUS_CSUM_ERR_L4_MASK			0x46U
#define RQ_CQE_STATUS_CSUM_ERR_OTHER			0x100U

#define HINIC_CSUM_ERR_BYPASSED(csum_err)	 \
	((csum_err) == RQ_CQE_STATUS_CSUM_BYPASS_VAL)

#define HINIC_CSUM_ERR_IP(csum_err)	 \
	((csum_err) & RQ_CQE_STATUS_CSUM_ERR_IP_MASK)

#define HINIC_CSUM_ERR_L4(csum_err)	 \
	((csum_err) & RQ_CQE_STATUS_CSUM_ERR_L4_MASK)

#define HINIC_CSUM_ERR_OTHER(csum_err)	 \
	((csum_err) == RQ_CQE_STATUS_CSUM_ERR_OTHER)


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

int hinic_create_rq(struct hinic_hwdev *hwdev, u16 q_id,
			u16 rq_depth, unsigned int socket_id)
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
			HINIC_RQ_WQEBB_SHIFT, nic_io->rq_depth, socket_id);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to allocate WQ for RQ");
		return err;
	}
	rq->wq = &nic_io->rq_wq[q_id];

	rq->pi_virt_addr = (volatile u16 *)dma_zalloc_coherent(hwdev,
			HINIC_PAGE_SIZE, &rq->pi_dma_addr, socket_id);
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

void hinic_rxq_get_stats(struct hinic_rxq *rxq, struct hinic_rxq_stats *stats)
{
	if (!rxq || !stats)
		return;

	memcpy(stats, &rxq->rxq_stats, sizeof(rxq->rxq_stats));
}

void hinic_rxq_stats_reset(struct hinic_rxq *rxq)
{
	struct hinic_rxq_stats *rxq_stats;

	if (rxq == NULL)
		return;

	rxq_stats = &rxq->rxq_stats;
	memset(rxq_stats, 0, sizeof(*rxq_stats));
}

static int hinic_rx_alloc_cqe(struct hinic_rxq *rxq, unsigned int socket_id)
{
	size_t cqe_mem_size;

	cqe_mem_size = sizeof(struct hinic_rq_cqe) * rxq->q_depth;
	rxq->cqe_start_vaddr = dma_zalloc_coherent(rxq->nic_dev->hwdev,
				cqe_mem_size, &rxq->cqe_start_paddr, socket_id);
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
	rxq->rx_info = rte_zmalloc_socket("rx_info", rx_info_sz,
				RTE_CACHE_LINE_SIZE, rxq->socket_id);
	if (!rxq->rx_info)
		return -ENOMEM;

	err = hinic_rx_alloc_cqe(rxq, rxq->socket_id);
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
	rte_free(rxq->rx_info);
	rxq->rx_info = NULL;

	return err;
}

void hinic_free_rx_resources(struct hinic_rxq *rxq)
{
	if (rxq->rx_info == NULL)
		return;

	hinic_rx_free_cqe(rxq);
	rte_free(rxq->rx_info);
	rxq->rx_info = NULL;
}

void hinic_free_all_rx_resources(struct rte_eth_dev *eth_dev)
{
	u16 q_id;
	struct hinic_nic_dev *nic_dev =
				HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(eth_dev);

	for (q_id = 0; q_id < nic_dev->num_rq; q_id++) {
		if (eth_dev->data->rx_queues != NULL)
			eth_dev->data->rx_queues[q_id] = NULL;

		if (nic_dev->rxqs[q_id] == NULL)
			continue;

		hinic_free_all_rx_mbufs(nic_dev->rxqs[q_id]);
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
		hinic_free_all_rx_mbufs(nic_dev->rxqs[q_id]);
}

static void hinic_recv_jumbo_pkt(struct hinic_rxq *rxq,
				 struct rte_mbuf *head_mbuf,
				 u32 remain_pkt_len)
{
	struct hinic_nic_dev *nic_dev = rxq->nic_dev;
	struct rte_mbuf *cur_mbuf, *rxm = NULL;
	struct hinic_rx_info *rx_info;
	u16 sw_ci, rx_buf_len = rxq->buf_len;
	u32 pkt_len;

	while (remain_pkt_len > 0) {
		sw_ci = hinic_get_rq_local_ci(nic_dev->hwdev, rxq->q_id);
		rx_info = &rxq->rx_info[sw_ci];

		hinic_update_rq_local_ci(nic_dev->hwdev, rxq->q_id, 1);

		pkt_len = remain_pkt_len > rx_buf_len ?
			rx_buf_len : remain_pkt_len;
		remain_pkt_len -= pkt_len;

		cur_mbuf = rx_info->mbuf;
		cur_mbuf->data_len = (u16)pkt_len;
		cur_mbuf->next = NULL;

		head_mbuf->pkt_len += cur_mbuf->data_len;
		head_mbuf->nb_segs++;

		if (!rxm)
			head_mbuf->next = cur_mbuf;
		else
			rxm->next = cur_mbuf;

		rxm = cur_mbuf;
	}
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
			  __rte_unused u8 *rq2iq_map,
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
	int err;
	bool lro_en;
	int max_lro_size;
	int lro_wqe_num;
	int buf_size;

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
		nic_dev->rx_csum_en = HINIC_RX_CSUM_OFFLOAD_EN;

	err = hinic_set_rx_csum_offload(nic_dev->hwdev,
					HINIC_RX_CSUM_OFFLOAD_EN);
	if (err)
		goto rx_csum_ofl_err;

	/* config lro */
	lro_en = dev->data->dev_conf.rxmode.offloads & DEV_RX_OFFLOAD_TCP_LRO ?
			true : false;
	max_lro_size = dev->data->dev_conf.rxmode.max_lro_pkt_size;
	buf_size = nic_dev->hwdev->nic_io->rq_buf_size;
	lro_wqe_num = max_lro_size / buf_size ? (max_lro_size / buf_size) : 1;

	err = hinic_set_rx_lro(nic_dev->hwdev, lro_en, lro_en, lro_wqe_num);
	if (err) {
		PMD_DRV_LOG(ERR, "%s %s lro failed, err: %d, max_lro_size: %d",
				dev->data->name, lro_en ? "Enable" : "Disable",
				err, max_lro_size);
		goto set_rx_lro_err;
	}

	return 0;

set_rx_lro_err:
rx_csum_ofl_err:
rss_config_err:

	hinic_destroy_num_qps(nic_dev);

	return HINIC_ERROR;
}

static void hinic_rx_remove_lro(struct hinic_nic_dev *nic_dev)
{
	int err;

	err = hinic_set_rx_lro(nic_dev->hwdev, false, false, 0);
	if (err)
		PMD_DRV_LOG(ERR, "%s disable LRO failed",
			    nic_dev->proc_dev_name);
}

void hinic_rx_remove_configure(struct rte_eth_dev *dev)
{
	struct hinic_nic_dev *nic_dev = HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);

	if (nic_dev->flags & ETH_MQ_RX_RSS_FLAG) {
		hinic_rss_deinit(nic_dev);
		hinic_destroy_num_qps(nic_dev);
	}

	hinic_rx_remove_lro(nic_dev);
}

void hinic_free_all_rx_mbufs(struct hinic_rxq *rxq)
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

static inline void hinic_rq_cqe_be_to_cpu32(void *dst_le32,
					    volatile void *src_be32)
{
#if defined(RTE_ARCH_X86_64)
	volatile __m128i *wqe_be = (volatile __m128i *)src_be32;
	__m128i *wqe_le = (__m128i *)dst_le32;
	__m128i shuf_mask =  _mm_set_epi8(12, 13, 14, 15, 8, 9, 10,
					11, 4, 5, 6, 7, 0, 1, 2, 3);

	/* l2nic just use first 128 bits */
	wqe_le[0] = _mm_shuffle_epi8(wqe_be[0], shuf_mask);
#elif defined(RTE_ARCH_ARM64)
	volatile uint8x16_t *wqe_be = (volatile uint8x16_t *)src_be32;
	uint8x16_t *wqe_le = (uint8x16_t *)dst_le32;
	const uint8x16_t shuf_mask = {3, 2, 1, 0, 7, 6, 5, 4, 11, 10,
					9, 8, 15, 14, 13, 12};

	/* l2nic just use first 128 bits */
	wqe_le[0] = vqtbl1q_u8(wqe_be[0], shuf_mask);
#else
	u32 i;
	volatile u32 *wqe_be = (volatile u32 *)src_be32;
	u32 *wqe_le = (u32 *)dst_le32;

#define HINIC_L2NIC_RQ_CQE_USED		4 /* 4Bytes unit */

	for (i = 0; i < HINIC_L2NIC_RQ_CQE_USED; i++) {
		*wqe_le = rte_be_to_cpu_32(*wqe_be);
		wqe_be++;
		wqe_le++;
	}
#endif
}

static inline uint64_t hinic_rx_rss_hash(uint32_t offload_type,
					 uint32_t cqe_hass_val,
					 uint32_t *rss_hash)
{
	uint32_t rss_type;

	rss_type = HINIC_GET_RSS_TYPES(offload_type);
	if (likely(rss_type != 0)) {
		*rss_hash = cqe_hass_val;
		return PKT_RX_RSS_HASH;
	}

	return 0;
}

static inline uint64_t hinic_rx_csum(uint32_t status, struct hinic_rxq *rxq)
{
	uint32_t checksum_err;
	uint64_t flags;
	struct hinic_nic_dev *nic_dev = rxq->nic_dev;

	if (unlikely(!(nic_dev->rx_csum_en & HINIC_RX_CSUM_OFFLOAD_EN)))
		return PKT_RX_IP_CKSUM_UNKNOWN;

	/* most case checksum is ok */
	checksum_err = HINIC_GET_RX_CSUM_ERR(status);
	if (likely(checksum_err == 0))
		return (PKT_RX_IP_CKSUM_GOOD | PKT_RX_L4_CKSUM_GOOD);

	/* If BYPASS bit set, all other status indications should be ignored */
	if (unlikely(HINIC_CSUM_ERR_BYPASSED(checksum_err)))
		return PKT_RX_IP_CKSUM_UNKNOWN;

	flags = 0;

	/* IP checksum error */
	if (HINIC_CSUM_ERR_IP(checksum_err))
		flags |= PKT_RX_IP_CKSUM_BAD;
	else
		flags |= PKT_RX_IP_CKSUM_GOOD;

	/* L4 checksum error */
	if (HINIC_CSUM_ERR_L4(checksum_err))
		flags |= PKT_RX_L4_CKSUM_BAD;
	else
		flags |= PKT_RX_L4_CKSUM_GOOD;

	if (unlikely(HINIC_CSUM_ERR_OTHER(checksum_err)))
		flags = PKT_RX_L4_CKSUM_NONE;

	rxq->rxq_stats.errors++;

	return flags;
}

static inline uint64_t hinic_rx_vlan(uint32_t offload_type, uint32_t vlan_len,
				     uint16_t *vlan_tci)
{
	uint16_t vlan_tag;

	vlan_tag = HINIC_GET_RX_VLAN_TAG(vlan_len);
	if (!HINIC_GET_RX_VLAN_OFFLOAD_EN(offload_type) || 0 == vlan_tag) {
		*vlan_tci = 0;
		return 0;
	}

	*vlan_tci = vlan_tag;

	return PKT_RX_VLAN | PKT_RX_VLAN_STRIPPED;
}

static inline u32 hinic_rx_alloc_mbuf_bulk(struct hinic_rxq *rxq,
					   struct rte_mbuf **mbufs,
					   u32 exp_mbuf_cnt)
{
	int rc;
	u32 avail_cnt;

	rc = rte_pktmbuf_alloc_bulk(rxq->mb_pool, mbufs, exp_mbuf_cnt);
	if (likely(rc == HINIC_OK)) {
		avail_cnt = exp_mbuf_cnt;
	} else {
		avail_cnt = 0;
		rxq->rxq_stats.rx_nombuf += exp_mbuf_cnt;
	}

	return avail_cnt;
}

static struct rte_mbuf *hinic_rx_alloc_mbuf(struct hinic_rxq *rxq,
					dma_addr_t *dma_addr)
{
	struct rte_mbuf *mbuf = NULL;
	int rc;

	rc = rte_pktmbuf_alloc_bulk(rxq->mb_pool, &mbuf, 1);
	if (unlikely(rc != HINIC_OK))
		return NULL;

	*dma_addr = rte_mbuf_data_iova_default(mbuf);

	return mbuf;
}

static inline void hinic_rearm_rxq_mbuf(struct hinic_rxq *rxq)
{
	u16 pi;
	u32 i, free_wqebbs, rearm_wqebbs, exp_wqebbs;
	dma_addr_t dma_addr;
	struct hinic_rq_wqe *rq_wqe;
	struct rte_mbuf **rearm_mbufs;

	/* check free wqebb fo rearm */
	free_wqebbs = HINIC_GET_RQ_FREE_WQEBBS(rxq);
	if (unlikely(free_wqebbs < rxq->rx_free_thresh))
		return;

	/* get rearm mbuf array */
	pi = HINIC_GET_RQ_LOCAL_PI(rxq);
	rearm_mbufs = (struct rte_mbuf **)(&rxq->rx_info[pi]);

	/* check rxq free wqebbs turn around */
	exp_wqebbs = rxq->q_depth - pi;
	if (free_wqebbs < exp_wqebbs)
		exp_wqebbs = free_wqebbs;

	/* alloc mbuf in bulk */
	rearm_wqebbs = hinic_rx_alloc_mbuf_bulk(rxq, rearm_mbufs, exp_wqebbs);
	if (unlikely(rearm_wqebbs == 0))
		return;

	/* rearm rx mbuf */
	rq_wqe = WQ_WQE_ADDR(rxq->wq, (u32)pi);
	for (i = 0; i < rearm_wqebbs; i++) {
		dma_addr = rte_mbuf_data_iova_default(rearm_mbufs[i]);
		rq_wqe->buf_desc.addr_high =
					cpu_to_be32(upper_32_bits(dma_addr));
		rq_wqe->buf_desc.addr_low =
					cpu_to_be32(lower_32_bits(dma_addr));
		rq_wqe++;
	}
	rxq->wq->prod_idx += rearm_wqebbs;
	rxq->wq->delta -= rearm_wqebbs;

	/* update rq hw_pi */
	rte_wmb();
	HINIC_UPDATE_RQ_HW_PI(rxq, pi + rearm_wqebbs);
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

u16 hinic_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts, u16 nb_pkts)
{
	struct rte_mbuf *rxm;
	struct hinic_rxq *rxq = rx_queue;
	struct hinic_rx_info *rx_info;
	volatile struct hinic_rq_cqe *rx_cqe;
	u16 rx_buf_len, pkts = 0;
	u16 sw_ci, ci_mask, wqebb_cnt = 0;
	u32 pkt_len, status, vlan_len, lro_num;
	u64 rx_bytes = 0;
	struct hinic_rq_cqe cqe;
	u32 offload_type, rss_hash;

	rx_buf_len = rxq->buf_len;

	/* 1. get polling start ci */
	ci_mask = HINIC_GET_RQ_WQE_MASK(rxq);
	sw_ci = HINIC_GET_RQ_LOCAL_CI(rxq);

	while (pkts < nb_pkts) {
		 /* 2. current ci is done */
		rx_cqe = &rxq->rx_cqe[sw_ci];
		status = __atomic_load_n(&rx_cqe->status, __ATOMIC_ACQUIRE);
		if (!HINIC_GET_RX_DONE_BE(status))
			break;

		/* convert cqe and get packet length */
		hinic_rq_cqe_be_to_cpu32(&cqe, (volatile void *)rx_cqe);
		vlan_len = cqe.vlan_len;

		rx_info = &rxq->rx_info[sw_ci];
		rxm = rx_info->mbuf;

		/* 3. next ci point and prefetch */
		sw_ci++;
		sw_ci &= ci_mask;

		/* prefetch next mbuf first 64B */
		rte_prefetch0(rxq->rx_info[sw_ci].mbuf);

		/* 4. jumbo frame process */
		pkt_len = HINIC_GET_RX_PKT_LEN(vlan_len);
		if (likely(pkt_len <= rx_buf_len)) {
			rxm->data_len = pkt_len;
			rxm->pkt_len = pkt_len;
			wqebb_cnt++;
		} else {
			rxm->data_len = rx_buf_len;
			rxm->pkt_len = rx_buf_len;

			/* if receive jumbo, updating ci will be done by
			 * hinic_recv_jumbo_pkt function.
			 */
			HINIC_UPDATE_RQ_LOCAL_CI(rxq, wqebb_cnt + 1);
			wqebb_cnt = 0;
			hinic_recv_jumbo_pkt(rxq, rxm, pkt_len - rx_buf_len);
			sw_ci = HINIC_GET_RQ_LOCAL_CI(rxq);
		}

		/* 5. vlan/checksum/rss/pkt_type/gro offload */
		rxm->data_off = RTE_PKTMBUF_HEADROOM;
		rxm->port = rxq->port_id;
		offload_type = cqe.offload_type;

		/* vlan offload */
		rxm->ol_flags |= hinic_rx_vlan(offload_type, vlan_len,
					       &rxm->vlan_tci);

		/* checksum offload */
		rxm->ol_flags |= hinic_rx_csum(cqe.status, rxq);

		/* rss hash offload */
		rss_hash = cqe.rss_hash;
		rxm->ol_flags |= hinic_rx_rss_hash(offload_type, rss_hash,
						   &rxm->hash.rss);

		/* lro offload */
		lro_num = HINIC_GET_RX_NUM_LRO(cqe.status);
		if (unlikely(lro_num != 0)) {
			rxm->ol_flags |= PKT_RX_LRO;
			rxm->tso_segsz = pkt_len / lro_num;
		}

		/* 6. clear done bit */
		rx_cqe->status = 0;

		rx_bytes += pkt_len;
		rx_pkts[pkts++] = rxm;
	}

	if (pkts) {
		/* 7. update ci */
		HINIC_UPDATE_RQ_LOCAL_CI(rxq, wqebb_cnt);

		/* do packet stats */
		rxq->rxq_stats.packets += pkts;
		rxq->rxq_stats.bytes += rx_bytes;
	}
	rxq->rxq_stats.burst_pkts = pkts;

	/* 8. rearm mbuf to rxq */
	hinic_rearm_rxq_mbuf(rxq);

	return pkts;
}
