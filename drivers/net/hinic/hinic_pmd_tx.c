/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Huawei Technologies Co., Ltd
 */

#include <rte_mbuf.h>
#include <rte_tcp.h>
#include <rte_sctp.h>
#include <rte_udp.h>
#include <rte_ip.h>
#ifdef RTE_ARCH_ARM64
#include <arm_neon.h>
#endif

#include "base/hinic_compat.h"
#include "base/hinic_pmd_hwdev.h"
#include "base/hinic_pmd_hwif.h"
#include "base/hinic_pmd_wq.h"
#include "base/hinic_pmd_nicio.h"
#include "base/hinic_pmd_niccfg.h"
#include "hinic_pmd_ethdev.h"
#include "hinic_pmd_tx.h"

/* packet header and tx offload info */
#define ETHER_LEN_NO_VLAN		14
#define ETHER_LEN_WITH_VLAN		18
#define VXLANLEN			8
#define MAX_PLD_OFFSET			221
#define MAX_SINGLE_SGE_SIZE		65536
#define TSO_ENABLE			1
#define TX_MSS_DEFAULT			0x3E00
#define TX_MSS_MIN			0x50

#define HINIC_NONTSO_PKT_MAX_SGE		17	/* non-tso max sge 17 */
#define HINIC_NONTSO_SEG_NUM_INVALID(num)	\
			((num) > HINIC_NONTSO_PKT_MAX_SGE)

#define HINIC_TSO_PKT_MAX_SGE			127	/* tso max sge 127 */
#define HINIC_TSO_SEG_NUM_INVALID(num)		((num) > HINIC_TSO_PKT_MAX_SGE)

/* sizeof(struct hinic_sq_bufdesc) == 16, shift 4 */
#define HINIC_BUF_DESC_SIZE(nr_descs)	(SIZE_8BYTES(((u32)nr_descs) << 4))

#define MASKED_SQ_IDX(sq, idx)		((idx) & (sq)->wq->mask)

/* SQ_CTRL */
#define SQ_CTRL_BUFDESC_SECT_LEN_SHIFT		0
#define SQ_CTRL_TASKSECT_LEN_SHIFT		16
#define SQ_CTRL_DATA_FORMAT_SHIFT		22
#define SQ_CTRL_LEN_SHIFT			29
#define SQ_CTRL_OWNER_SHIFT			31

#define SQ_CTRL_BUFDESC_SECT_LEN_MASK		0xFFU
#define SQ_CTRL_TASKSECT_LEN_MASK		0x1FU
#define SQ_CTRL_DATA_FORMAT_MASK		0x1U
#define SQ_CTRL_LEN_MASK			0x3U
#define SQ_CTRL_OWNER_MASK			0x1U

#define SQ_CTRL_SET(val, member)	\
	(((val) & SQ_CTRL_##member##_MASK) << SQ_CTRL_##member##_SHIFT)

#define SQ_CTRL_QUEUE_INFO_PLDOFF_SHIFT		2
#define SQ_CTRL_QUEUE_INFO_UFO_SHIFT		10
#define SQ_CTRL_QUEUE_INFO_TSO_SHIFT		11
#define SQ_CTRL_QUEUE_INFO_TCPUDP_CS_SHIFT	12
#define SQ_CTRL_QUEUE_INFO_MSS_SHIFT		13
#define SQ_CTRL_QUEUE_INFO_SCTP_SHIFT		27
#define SQ_CTRL_QUEUE_INFO_UC_SHIFT		28
#define SQ_CTRL_QUEUE_INFO_PRI_SHIFT		29

#define SQ_CTRL_QUEUE_INFO_PLDOFF_MASK		0xFFU
#define SQ_CTRL_QUEUE_INFO_UFO_MASK		0x1U
#define SQ_CTRL_QUEUE_INFO_TSO_MASK		0x1U
#define SQ_CTRL_QUEUE_INFO_TCPUDP_CS_MASK	0x1U
#define SQ_CTRL_QUEUE_INFO_MSS_MASK		0x3FFFU
#define SQ_CTRL_QUEUE_INFO_SCTP_MASK		0x1U
#define SQ_CTRL_QUEUE_INFO_UC_MASK		0x1U
#define SQ_CTRL_QUEUE_INFO_PRI_MASK		0x7U

#define SQ_CTRL_QUEUE_INFO_SET(val, member)	\
	(((u32)(val) & SQ_CTRL_QUEUE_INFO_##member##_MASK) <<	\
			SQ_CTRL_QUEUE_INFO_##member##_SHIFT)

#define SQ_CTRL_QUEUE_INFO_GET(val, member)	\
	(((val) >> SQ_CTRL_QUEUE_INFO_##member##_SHIFT) &	\
			SQ_CTRL_QUEUE_INFO_##member##_MASK)

#define SQ_CTRL_QUEUE_INFO_CLEAR(val, member)	\
	((val) & (~(SQ_CTRL_QUEUE_INFO_##member##_MASK << \
			SQ_CTRL_QUEUE_INFO_##member##_SHIFT)))

#define	SQ_TASK_INFO0_L2HDR_LEN_SHIFT		0
#define	SQ_TASK_INFO0_L4OFFLOAD_SHIFT		8
#define	SQ_TASK_INFO0_INNER_L3TYPE_SHIFT	10
#define	SQ_TASK_INFO0_VLAN_OFFLOAD_SHIFT	12
#define	SQ_TASK_INFO0_PARSE_FLAG_SHIFT		13
#define	SQ_TASK_INFO0_UFO_AVD_SHIFT		14
#define	SQ_TASK_INFO0_TSO_UFO_SHIFT		15
#define SQ_TASK_INFO0_VLAN_TAG_SHIFT		16

#define	SQ_TASK_INFO0_L2HDR_LEN_MASK		0xFFU
#define	SQ_TASK_INFO0_L4OFFLOAD_MASK		0x3U
#define	SQ_TASK_INFO0_INNER_L3TYPE_MASK		0x3U
#define	SQ_TASK_INFO0_VLAN_OFFLOAD_MASK		0x1U
#define	SQ_TASK_INFO0_PARSE_FLAG_MASK		0x1U
#define	SQ_TASK_INFO0_UFO_AVD_MASK		0x1U
#define SQ_TASK_INFO0_TSO_UFO_MASK		0x1U
#define SQ_TASK_INFO0_VLAN_TAG_MASK		0xFFFFU

#define SQ_TASK_INFO0_SET(val, member)			\
	(((u32)(val) & SQ_TASK_INFO0_##member##_MASK) <<	\
			SQ_TASK_INFO0_##member##_SHIFT)

#define	SQ_TASK_INFO1_MD_TYPE_SHIFT		8
#define SQ_TASK_INFO1_INNER_L4LEN_SHIFT		16
#define SQ_TASK_INFO1_INNER_L3LEN_SHIFT		24

#define	SQ_TASK_INFO1_MD_TYPE_MASK		0xFFU
#define SQ_TASK_INFO1_INNER_L4LEN_MASK		0xFFU
#define SQ_TASK_INFO1_INNER_L3LEN_MASK		0xFFU

#define SQ_TASK_INFO1_SET(val, member)			\
	(((val) & SQ_TASK_INFO1_##member##_MASK) <<	\
			SQ_TASK_INFO1_##member##_SHIFT)

#define SQ_TASK_INFO2_TUNNEL_L4LEN_SHIFT	0
#define SQ_TASK_INFO2_OUTER_L3LEN_SHIFT		8
#define SQ_TASK_INFO2_TUNNEL_L4TYPE_SHIFT	16
#define SQ_TASK_INFO2_OUTER_L3TYPE_SHIFT	24

#define SQ_TASK_INFO2_TUNNEL_L4LEN_MASK		0xFFU
#define SQ_TASK_INFO2_OUTER_L3LEN_MASK		0xFFU
#define SQ_TASK_INFO2_TUNNEL_L4TYPE_MASK	0x7U
#define SQ_TASK_INFO2_OUTER_L3TYPE_MASK		0x3U

#define SQ_TASK_INFO2_SET(val, member)			\
	(((val) & SQ_TASK_INFO2_##member##_MASK) <<	\
			SQ_TASK_INFO2_##member##_SHIFT)

#define	SQ_TASK_INFO4_L2TYPE_SHIFT		31

#define	SQ_TASK_INFO4_L2TYPE_MASK		0x1U

#define SQ_TASK_INFO4_SET(val, member)		\
	(((u32)(val) & SQ_TASK_INFO4_##member##_MASK) << \
			SQ_TASK_INFO4_##member##_SHIFT)

/* SQ_DB */
#define SQ_DB_OFF				0x00000800
#define SQ_DB_INFO_HI_PI_SHIFT			0
#define SQ_DB_INFO_QID_SHIFT			8
#define SQ_DB_INFO_CFLAG_SHIFT			23
#define SQ_DB_INFO_COS_SHIFT			24
#define SQ_DB_INFO_TYPE_SHIFT			27

#define SQ_DB_INFO_HI_PI_MASK			0xFFU
#define SQ_DB_INFO_QID_MASK			0x3FFU
#define SQ_DB_INFO_CFLAG_MASK			0x1U
#define SQ_DB_INFO_COS_MASK			0x7U
#define SQ_DB_INFO_TYPE_MASK			0x1FU
#define SQ_DB_INFO_SET(val, member)		\
	(((u32)(val) & SQ_DB_INFO_##member##_MASK) <<	\
			SQ_DB_INFO_##member##_SHIFT)

#define SQ_DB					1
#define SQ_CFLAG_DP				0	/* CFLAG_DATA_PATH */

#define SQ_DB_PI_LOW_MASK			0xFF
#define SQ_DB_PI_LOW(pi)			((pi) & SQ_DB_PI_LOW_MASK)
#define SQ_DB_PI_HI_SHIFT			8
#define SQ_DB_PI_HIGH(pi)			((pi) >> SQ_DB_PI_HI_SHIFT)
#define SQ_DB_ADDR(sq, pi)		\
	((u64 *)((u8 __iomem *)((sq)->db_addr) + SQ_DB_OFF) + SQ_DB_PI_LOW(pi))

/* txq wq operations */
#define HINIC_GET_SQ_WQE_MASK(txq)		((txq)->wq->mask)

#define HINIC_GET_SQ_HW_CI(txq)	\
	((be16_to_cpu(*(txq)->cons_idx_addr)) & HINIC_GET_SQ_WQE_MASK(txq))

#define HINIC_GET_SQ_LOCAL_CI(txq)	\
	(((txq)->wq->cons_idx) & HINIC_GET_SQ_WQE_MASK(txq))

#define HINIC_UPDATE_SQ_LOCAL_CI(txq, wqebb_cnt)	\
	do {						\
		(txq)->wq->cons_idx += wqebb_cnt;	\
		(txq)->wq->delta += wqebb_cnt;		\
	} while (0)

#define HINIC_GET_SQ_FREE_WQEBBS(txq)	((txq)->wq->delta - 1)

#define HINIC_IS_SQ_EMPTY(txq)	(((txq)->wq->delta) == ((txq)->q_depth))

#define BUF_DESC_SIZE_SHIFT		4

#define HINIC_SQ_WQE_SIZE(num_sge)		\
	(sizeof(struct hinic_sq_ctrl) + sizeof(struct hinic_sq_task) +  \
			(unsigned int)((num_sge) << BUF_DESC_SIZE_SHIFT))

#define HINIC_SQ_WQEBB_CNT(num_sge)	\
	(int)(ALIGN(HINIC_SQ_WQE_SIZE((u32)num_sge), \
			HINIC_SQ_WQEBB_SIZE) >> HINIC_SQ_WQEBB_SHIFT)


static inline void hinic_sq_wqe_cpu_to_be32(void *data, int nr_wqebb)
{
#if defined(RTE_ARCH_X86_64)
	int i;
	__m128i *wqe_line = (__m128i *)data;
	__m128i shuf_mask = _mm_set_epi8(12, 13, 14, 15, 8, 9, 10,
					11, 4, 5, 6, 7, 0, 1, 2, 3);

	for (i = 0; i < nr_wqebb; i++) {
		/* convert 64B wqebb using 4 SSE instructions */
		wqe_line[0] = _mm_shuffle_epi8(wqe_line[0], shuf_mask);
		wqe_line[1] = _mm_shuffle_epi8(wqe_line[1], shuf_mask);
		wqe_line[2] = _mm_shuffle_epi8(wqe_line[2], shuf_mask);
		wqe_line[3] = _mm_shuffle_epi8(wqe_line[3], shuf_mask);
		wqe_line += 4;
	}
#elif defined(RTE_ARCH_ARM64)
	int i;
	uint8x16_t *wqe_line = (uint8x16_t *)data;
	const uint8x16_t shuf_mask = {3, 2, 1, 0, 7, 6, 5, 4, 11, 10,
					9, 8, 15, 14, 13, 12};

	for (i = 0; i < nr_wqebb; i++) {
		wqe_line[0] = vqtbl1q_u8(wqe_line[0], shuf_mask);
		wqe_line[1] = vqtbl1q_u8(wqe_line[1], shuf_mask);
		wqe_line[2] = vqtbl1q_u8(wqe_line[2], shuf_mask);
		wqe_line[3] = vqtbl1q_u8(wqe_line[3], shuf_mask);
		wqe_line += 4;
	}
#else
	hinic_cpu_to_be32(data, nr_wqebb * HINIC_SQ_WQEBB_SIZE);
#endif
}

static inline void hinic_sge_cpu_to_be32(void *data, int nr_sge)
{
#if defined(RTE_ARCH_X86_64)
	int i;
	__m128i *sge_line = (__m128i *)data;
	__m128i shuf_mask = _mm_set_epi8(12, 13, 14, 15, 8, 9, 10,
					11, 4, 5, 6, 7, 0, 1, 2, 3);

	for (i = 0; i < nr_sge; i++) {
		/* convert 16B sge using 1 SSE instructions */
		*sge_line = _mm_shuffle_epi8(*sge_line, shuf_mask);
		sge_line++;
	}
#elif defined(RTE_ARCH_ARM64)
	int i;
	uint8x16_t *sge_line = (uint8x16_t *)data;
	const uint8x16_t shuf_mask = {3, 2, 1, 0, 7, 6, 5, 4, 11, 10,
					9, 8, 15, 14, 13, 12};

	for (i = 0; i < nr_sge; i++) {
		*sge_line = vqtbl1q_u8(*sge_line, shuf_mask);
		sge_line++;
	}
#else
	hinic_cpu_to_be32(data, nr_sge * sizeof(struct hinic_sq_bufdesc));
#endif
}

void hinic_txq_get_stats(struct hinic_txq *txq, struct hinic_txq_stats *stats)
{
	if (!txq || !stats) {
		PMD_DRV_LOG(ERR, "Txq or stats is NULL");
		return;
	}

	memcpy(stats, &txq->txq_stats, sizeof(txq->txq_stats));
}

void hinic_txq_stats_reset(struct hinic_txq *txq)
{
	struct hinic_txq_stats *txq_stats;

	if (txq == NULL)
		return;

	txq_stats = &txq->txq_stats;
	memset(txq_stats, 0, sizeof(*txq_stats));
}

static inline struct rte_mbuf *hinic_copy_tx_mbuf(struct hinic_nic_dev *nic_dev,
						  struct rte_mbuf *mbuf,
						  u16 sge_cnt)
{
	struct rte_mbuf *dst_mbuf;
	u32 offset = 0;
	u16 i;

	if (unlikely(!nic_dev->cpy_mpool))
		return NULL;

	dst_mbuf = rte_pktmbuf_alloc(nic_dev->cpy_mpool);
	if (unlikely(!dst_mbuf))
		return NULL;

	dst_mbuf->data_off = 0;
	for (i = 0; i < sge_cnt; i++) {
		rte_memcpy((char *)dst_mbuf->buf_addr + offset,
			   (char *)mbuf->buf_addr + mbuf->data_off,
			   mbuf->data_len);
		dst_mbuf->data_len += mbuf->data_len;
		offset += mbuf->data_len;
		mbuf = mbuf->next;
	}

	dst_mbuf->pkt_len = dst_mbuf->data_len;

	return dst_mbuf;
}

static inline bool hinic_mbuf_dma_map_sge(struct hinic_txq *txq,
					  struct rte_mbuf *mbuf,
					  struct hinic_sq_bufdesc *sges,
					  struct hinic_wqe_info *sqe_info)
{
	dma_addr_t dma_addr;
	u16 i, around_sges;
	u16 nb_segs = sqe_info->sge_cnt - sqe_info->cpy_mbuf_cnt;
	u16 real_nb_segs = mbuf->nb_segs;
	struct hinic_sq_bufdesc *sge_idx = sges;

	if (unlikely(sqe_info->around)) {
		/* parts of wqe is in sq bottom while parts
		 * of wqe is in sq head
		 */
		i = 0;
		for (sge_idx = sges; (u64)sge_idx <= txq->sq_bot_sge_addr;
		     sge_idx++) {
			if (unlikely(mbuf == NULL)) {
				txq->txq_stats.mbuf_null++;
				return false;
			}

			dma_addr = rte_mbuf_data_iova(mbuf);
			if (unlikely(mbuf->data_len == 0)) {
				txq->txq_stats.sge_len0++;
				return false;
			}
			hinic_set_sge((struct hinic_sge *)sge_idx, dma_addr,
				      mbuf->data_len);
			mbuf = mbuf->next;
			i++;
		}

		around_sges = nb_segs - i;
		sge_idx = (struct hinic_sq_bufdesc *)
				((void *)txq->sq_head_addr);
		for (; i < nb_segs; i++) {
			if (unlikely(mbuf == NULL)) {
				txq->txq_stats.mbuf_null++;
				return false;
			}

			dma_addr = rte_mbuf_data_iova(mbuf);
			if (unlikely(mbuf->data_len == 0)) {
				txq->txq_stats.sge_len0++;
				return false;
			}
			hinic_set_sge((struct hinic_sge *)sge_idx, dma_addr,
				      mbuf->data_len);
			mbuf = mbuf->next;
			sge_idx++;
		}

		/* covert sges at head to big endian */
		hinic_sge_cpu_to_be32((void *)txq->sq_head_addr, around_sges);
	} else {
		/* wqe is in continuous space */
		for (i = 0; i < nb_segs; i++) {
			if (unlikely(mbuf == NULL)) {
				txq->txq_stats.mbuf_null++;
				return false;
			}

			dma_addr = rte_mbuf_data_iova(mbuf);
			if (unlikely(mbuf->data_len == 0)) {
				txq->txq_stats.sge_len0++;
				return false;
			}
			hinic_set_sge((struct hinic_sge *)sge_idx, dma_addr,
				      mbuf->data_len);
			mbuf = mbuf->next;
			sge_idx++;
		}
	}

	/* for now: support non-tso over 17 sge, copy the last 2 mbuf */
	if (unlikely(sqe_info->cpy_mbuf_cnt != 0)) {
		/* copy invalid mbuf segs to a valid buffer, lost performance */
		txq->txq_stats.cpy_pkts += 1;
		mbuf = hinic_copy_tx_mbuf(txq->nic_dev, mbuf,
					  real_nb_segs - nb_segs);
		if (unlikely(!mbuf))
			return false;

		txq->tx_info[sqe_info->pi].cpy_mbuf = mbuf;

		/* deal with the last mbuf */
		dma_addr = rte_mbuf_data_iova(mbuf);
		if (unlikely(mbuf->data_len == 0)) {
			txq->txq_stats.sge_len0++;
			return false;
		}
		hinic_set_sge((struct hinic_sge *)sge_idx, dma_addr,
			      mbuf->data_len);
		if (unlikely(sqe_info->around))
			hinic_sge_cpu_to_be32((void *)sge_idx, 1);
	}

	return true;
}

static inline void hinic_fill_sq_wqe_header(struct hinic_sq_ctrl *ctrl,
					    u32 queue_info, int nr_descs,
					    u8 owner)
{
	u32 ctrl_size, task_size, bufdesc_size;

	ctrl_size = SIZE_8BYTES(sizeof(struct hinic_sq_ctrl));
	task_size = SIZE_8BYTES(sizeof(struct hinic_sq_task));
	bufdesc_size = HINIC_BUF_DESC_SIZE(nr_descs);

	ctrl->ctrl_fmt = SQ_CTRL_SET(bufdesc_size, BUFDESC_SECT_LEN) |
			SQ_CTRL_SET(task_size, TASKSECT_LEN)	|
			SQ_CTRL_SET(SQ_NORMAL_WQE, DATA_FORMAT)	|
			SQ_CTRL_SET(ctrl_size, LEN)		|
			SQ_CTRL_SET(owner, OWNER);

	ctrl->queue_info = queue_info;
	ctrl->queue_info |= SQ_CTRL_QUEUE_INFO_SET(1U, UC);

	if (!SQ_CTRL_QUEUE_INFO_GET(ctrl->queue_info, MSS)) {
		ctrl->queue_info |=
			SQ_CTRL_QUEUE_INFO_SET(TX_MSS_DEFAULT, MSS);
	} else if (SQ_CTRL_QUEUE_INFO_GET(ctrl->queue_info, MSS) < TX_MSS_MIN) {
		/* mss should not be less than 80 */
		ctrl->queue_info =
				SQ_CTRL_QUEUE_INFO_CLEAR(ctrl->queue_info, MSS);
		ctrl->queue_info |= SQ_CTRL_QUEUE_INFO_SET(TX_MSS_MIN, MSS);
	}
}

static inline bool hinic_is_tso_sge_valid(struct rte_mbuf *mbuf,
					  struct hinic_tx_offload_info
					  *poff_info,
					  struct hinic_wqe_info *sqe_info)
{
	u32 total_len, limit_len, checked_len, left_len, adjust_mss;
	u32 i, first_mss_sges, left_sges;
	struct rte_mbuf *mbuf_head, *mbuf_pre;

	left_sges = mbuf->nb_segs;
	mbuf_head = mbuf;

	/* tso sge number validation */
	if (unlikely(left_sges >= HINIC_NONTSO_PKT_MAX_SGE)) {
		checked_len = 0;
		adjust_mss = mbuf->tso_segsz >= TX_MSS_MIN ?
				mbuf->tso_segsz : TX_MSS_MIN;
		limit_len = adjust_mss + poff_info->payload_offset;
		first_mss_sges = HINIC_NONTSO_PKT_MAX_SGE;

		/* each continues 17 mbufs segmust do one check */
		while (left_sges >= HINIC_NONTSO_PKT_MAX_SGE) {
			/* total len of first 16 mbufs must equal
			 * or more than limit_len
			 */
			total_len = 0;
			for (i = 0; i < first_mss_sges; i++) {
				total_len += mbuf->data_len;
				mbuf_pre = mbuf;
				mbuf = mbuf->next;
				if (total_len >= limit_len) {
					limit_len = adjust_mss;
					break;
				}
			}

			checked_len += total_len;

			/* try to copy if not valid */
			if (unlikely(first_mss_sges == i)) {
				left_sges -= first_mss_sges;
				checked_len -= mbuf_pre->data_len;

				left_len = mbuf_head->pkt_len - checked_len;
				if (left_len > HINIC_COPY_MBUF_SIZE)
					return false;

				sqe_info->sge_cnt = mbuf_head->nb_segs -
							left_sges;
				sqe_info->cpy_mbuf_cnt = 1;

				return true;
			}
			first_mss_sges = (HINIC_NONTSO_PKT_MAX_SGE - 1);

			/* continue next 16 mbufs */
			left_sges -= (i + 1);
		} /* end of while */
	}

	sqe_info->sge_cnt = mbuf_head->nb_segs;
	return true;
}

static inline void
hinic_set_l4_csum_info(struct hinic_sq_task *task,
		u32 *queue_info, struct hinic_tx_offload_info *poff_info)
{
	u32 tcp_udp_cs, sctp = 0;
	u16 l2hdr_len;

	if (unlikely(poff_info->inner_l4_type == SCTP_OFFLOAD_ENABLE))
		sctp = 1;

	tcp_udp_cs = poff_info->inner_l4_tcp_udp;

	if (poff_info->tunnel_type == TUNNEL_UDP_CSUM ||
	    poff_info->tunnel_type == TUNNEL_UDP_NO_CSUM) {
		l2hdr_len =  poff_info->outer_l2_len;

		task->pkt_info2 |=
		SQ_TASK_INFO2_SET(poff_info->outer_l3_type, OUTER_L3TYPE) |
		SQ_TASK_INFO2_SET(poff_info->outer_l3_len, OUTER_L3LEN);
		task->pkt_info2 |=
		SQ_TASK_INFO2_SET(poff_info->tunnel_type, TUNNEL_L4TYPE) |
		SQ_TASK_INFO2_SET(poff_info->tunnel_length, TUNNEL_L4LEN);
	} else {
		l2hdr_len = poff_info->inner_l2_len;
	}

	task->pkt_info0 |= SQ_TASK_INFO0_SET(l2hdr_len, L2HDR_LEN);
	task->pkt_info1 |=
		SQ_TASK_INFO1_SET(poff_info->inner_l3_len, INNER_L3LEN);
	task->pkt_info0 |=
		SQ_TASK_INFO0_SET(poff_info->inner_l3_type, INNER_L3TYPE);
	task->pkt_info1 |=
		SQ_TASK_INFO1_SET(poff_info->inner_l4_len, INNER_L4LEN);
	task->pkt_info0 |=
		SQ_TASK_INFO0_SET(poff_info->inner_l4_type, L4OFFLOAD);
	*queue_info |=
		SQ_CTRL_QUEUE_INFO_SET(poff_info->payload_offset, PLDOFF) |
		SQ_CTRL_QUEUE_INFO_SET(tcp_udp_cs, TCPUDP_CS) |
		SQ_CTRL_QUEUE_INFO_SET(sctp, SCTP);
}

static inline void
hinic_set_tso_info(struct hinic_sq_task *task,
		u32 *queue_info, struct rte_mbuf *mbuf,
		struct hinic_tx_offload_info *poff_info)
{
	hinic_set_l4_csum_info(task, queue_info, poff_info);

	/* wqe for tso */
	task->pkt_info0 |=
		SQ_TASK_INFO0_SET(poff_info->inner_l3_type, INNER_L3TYPE);
	task->pkt_info0 |= SQ_TASK_INFO0_SET(TSO_ENABLE, TSO_UFO);
	*queue_info |= SQ_CTRL_QUEUE_INFO_SET(TSO_ENABLE, TSO);
	/* qsf was initialized in prepare_sq_wqe */
	*queue_info = SQ_CTRL_QUEUE_INFO_CLEAR(*queue_info, MSS);
	*queue_info |= SQ_CTRL_QUEUE_INFO_SET(mbuf->tso_segsz, MSS);
}

static inline void
hinic_set_vlan_tx_offload(struct hinic_sq_task *task,
			u32 *queue_info, u16 vlan_tag, u16 vlan_pri)
{
	task->pkt_info0 |= SQ_TASK_INFO0_SET(vlan_tag, VLAN_TAG) |
				SQ_TASK_INFO0_SET(1U, VLAN_OFFLOAD);

	*queue_info |= SQ_CTRL_QUEUE_INFO_SET(vlan_pri, PRI);
}

static inline void
hinic_fill_tx_offload_info(struct rte_mbuf *mbuf,
		struct hinic_sq_task *task, u32 *queue_info,
		struct hinic_tx_offload_info *tx_off_info)
{
	u16 vlan_tag;
	uint64_t ol_flags = mbuf->ol_flags;

	/* clear DW0~2 of task section for offload */
	task->pkt_info0 = 0;
	task->pkt_info1 = 0;
	task->pkt_info2 = 0;

	/* Base VLAN */
	if (unlikely(ol_flags & RTE_MBUF_F_TX_VLAN)) {
		vlan_tag = mbuf->vlan_tci;
		hinic_set_vlan_tx_offload(task, queue_info, vlan_tag,
					  vlan_tag >> VLAN_PRIO_SHIFT);
	}

	/* non checksum or tso */
	if (unlikely(!(ol_flags & HINIC_TX_CKSUM_OFFLOAD_MASK)))
		return;

	if ((ol_flags & RTE_MBUF_F_TX_TCP_SEG))
		/* set tso info for task and qsf */
		hinic_set_tso_info(task, queue_info, mbuf, tx_off_info);
	else /* just support l4 checksum offload */
		hinic_set_l4_csum_info(task, queue_info, tx_off_info);
}

static inline void hinic_xmit_mbuf_cleanup(struct hinic_txq *txq)
{
	struct hinic_tx_info *tx_info;
	struct rte_mbuf *mbuf, *m, *mbuf_free[HINIC_MAX_TX_FREE_BULK];
	int i, nb_free = 0;
	u16 hw_ci, sw_ci, sq_mask;
	int wqebb_cnt = 0;

	hw_ci = HINIC_GET_SQ_HW_CI(txq);
	sw_ci = HINIC_GET_SQ_LOCAL_CI(txq);
	sq_mask = HINIC_GET_SQ_WQE_MASK(txq);

	for (i = 0; i < txq->tx_free_thresh; ++i) {
		tx_info = &txq->tx_info[sw_ci];
		if (hw_ci == sw_ci ||
			(((hw_ci - sw_ci) & sq_mask) < tx_info->wqebb_cnt))
			break;

		sw_ci = (sw_ci + tx_info->wqebb_cnt) & sq_mask;

		if (unlikely(tx_info->cpy_mbuf != NULL)) {
			rte_pktmbuf_free(tx_info->cpy_mbuf);
			tx_info->cpy_mbuf = NULL;
		}

		wqebb_cnt += tx_info->wqebb_cnt;
		mbuf = tx_info->mbuf;

		if (likely(mbuf->nb_segs == 1)) {
			m = rte_pktmbuf_prefree_seg(mbuf);
			tx_info->mbuf = NULL;

			if (unlikely(m == NULL))
				continue;

			mbuf_free[nb_free++] = m;
			if (unlikely(m->pool != mbuf_free[0]->pool ||
				nb_free >= HINIC_MAX_TX_FREE_BULK)) {
				rte_mempool_put_bulk(mbuf_free[0]->pool,
					(void **)mbuf_free, (nb_free - 1));
				nb_free = 0;
				mbuf_free[nb_free++] = m;
			}
		} else {
			rte_pktmbuf_free(mbuf);
			tx_info->mbuf = NULL;
		}
	}

	if (nb_free > 0)
		rte_mempool_put_bulk(mbuf_free[0]->pool, (void **)mbuf_free,
				     nb_free);

	HINIC_UPDATE_SQ_LOCAL_CI(txq, wqebb_cnt);
}

static inline struct hinic_sq_wqe *
hinic_get_sq_wqe(struct hinic_txq *txq, int wqebb_cnt,
		 struct hinic_wqe_info *wqe_info)
{
	u32 cur_pi, end_pi;
	u16 remain_wqebbs;
	struct hinic_sq *sq = txq->sq;
	struct hinic_wq *wq = txq->wq;

	/* record current pi */
	cur_pi = MASKED_WQE_IDX(wq, wq->prod_idx);
	end_pi = cur_pi + wqebb_cnt;

	/* update next pi and delta */
	wq->prod_idx += wqebb_cnt;
	wq->delta -= wqebb_cnt;

	/* return current pi and owner */
	wqe_info->pi = cur_pi;
	wqe_info->owner = sq->owner;
	wqe_info->around = 0;
	wqe_info->seq_wqebbs = wqebb_cnt;

	if (unlikely(end_pi >= txq->q_depth)) {
		/* update owner of next prod_idx */
		sq->owner = !sq->owner;

		/* turn around to head */
		if (unlikely(end_pi > txq->q_depth)) {
			wqe_info->around = 1;
			remain_wqebbs = txq->q_depth - cur_pi;
			wqe_info->seq_wqebbs = remain_wqebbs;
		}
	}

	return (struct hinic_sq_wqe *)WQ_WQE_ADDR(wq, cur_pi);
}

static inline uint16_t
hinic_ipv4_phdr_cksum(const struct rte_ipv4_hdr *ipv4_hdr, uint64_t ol_flags)
{
	struct ipv4_psd_header {
		uint32_t src_addr; /* IP address of source host. */
		uint32_t dst_addr; /* IP address of destination host. */
		uint8_t  zero;     /* zero. */
		uint8_t  proto;    /* L4 protocol type. */
		uint16_t len;      /* L4 length. */
	} psd_hdr;

	psd_hdr.src_addr = ipv4_hdr->src_addr;
	psd_hdr.dst_addr = ipv4_hdr->dst_addr;
	psd_hdr.zero = 0;
	psd_hdr.proto = ipv4_hdr->next_proto_id;
	if (ol_flags & RTE_MBUF_F_TX_TCP_SEG) {
		psd_hdr.len = 0;
	} else {
		psd_hdr.len =
		rte_cpu_to_be_16(rte_be_to_cpu_16(ipv4_hdr->total_length) -
				 rte_ipv4_hdr_len(ipv4_hdr));
	}
	return rte_raw_cksum(&psd_hdr, sizeof(psd_hdr));
}

static inline uint16_t
hinic_ipv6_phdr_cksum(const struct rte_ipv6_hdr *ipv6_hdr, uint64_t ol_flags)
{
	uint32_t sum;
	struct {
		uint32_t len;   /* L4 length. */
		uint32_t proto; /* L4 protocol - top 3 bytes must be zero */
	} psd_hdr;

	psd_hdr.proto = (ipv6_hdr->proto << 24);
	if (ol_flags & RTE_MBUF_F_TX_TCP_SEG)
		psd_hdr.len = 0;
	else
		psd_hdr.len = ipv6_hdr->payload_len;

	sum = __rte_raw_cksum(ipv6_hdr->src_addr,
		sizeof(ipv6_hdr->src_addr) + sizeof(ipv6_hdr->dst_addr), 0);
	sum = __rte_raw_cksum(&psd_hdr, sizeof(psd_hdr), sum);
	return __rte_raw_cksum_reduce(sum);
}

static inline void hinic_get_outer_cs_pld_offset(struct rte_mbuf *m,
					struct hinic_tx_offload_info *off_info)
{
	uint64_t ol_flags = m->ol_flags;

	if ((ol_flags & RTE_MBUF_F_TX_L4_MASK) == RTE_MBUF_F_TX_UDP_CKSUM)
		off_info->payload_offset = m->outer_l2_len + m->outer_l3_len +
					   m->l2_len + m->l3_len;
	else if ((ol_flags & RTE_MBUF_F_TX_TCP_CKSUM) || (ol_flags & RTE_MBUF_F_TX_TCP_SEG))
		off_info->payload_offset = m->outer_l2_len + m->outer_l3_len +
					   m->l2_len + m->l3_len + m->l4_len;
}

static inline void hinic_get_pld_offset(struct rte_mbuf *m,
					struct hinic_tx_offload_info *off_info)
{
	uint64_t ol_flags = m->ol_flags;

	if (((ol_flags & RTE_MBUF_F_TX_L4_MASK) == RTE_MBUF_F_TX_UDP_CKSUM) ||
	    ((ol_flags & RTE_MBUF_F_TX_L4_MASK) == RTE_MBUF_F_TX_SCTP_CKSUM))
		off_info->payload_offset = m->l2_len + m->l3_len;
	else if ((ol_flags & RTE_MBUF_F_TX_TCP_CKSUM) || (ol_flags & RTE_MBUF_F_TX_TCP_SEG))
		off_info->payload_offset = m->l2_len + m->l3_len +
					   m->l4_len;
}

static inline void hinic_analyze_tx_info(struct rte_mbuf *mbuf,
					 struct hinic_tx_offload_info *off_info)
{
	struct rte_ether_hdr *eth_hdr;
	struct rte_vlan_hdr *vlan_hdr;
	struct rte_ipv4_hdr *ipv4_hdr;
	u16 eth_type;

	eth_hdr = rte_pktmbuf_mtod(mbuf, struct rte_ether_hdr *);
	eth_type = rte_be_to_cpu_16(eth_hdr->ether_type);

	if (eth_type == RTE_ETHER_TYPE_VLAN) {
		off_info->outer_l2_len = ETHER_LEN_WITH_VLAN;
		vlan_hdr = (struct rte_vlan_hdr *)(eth_hdr + 1);
		eth_type = rte_be_to_cpu_16(vlan_hdr->eth_proto);
	} else {
		off_info->outer_l2_len = ETHER_LEN_NO_VLAN;
	}

	if (eth_type == RTE_ETHER_TYPE_IPV4) {
		ipv4_hdr = rte_pktmbuf_mtod_offset(mbuf, struct rte_ipv4_hdr *,
						   off_info->outer_l2_len);
		off_info->outer_l3_len = rte_ipv4_hdr_len(ipv4_hdr);
	} else if (eth_type == RTE_ETHER_TYPE_IPV6) {
		/* not support ipv6 extension header */
		off_info->outer_l3_len = sizeof(struct rte_ipv6_hdr);
	}
}

static inline void hinic_analyze_outer_ip_vxlan(struct rte_mbuf *mbuf,
					struct hinic_tx_offload_info *off_info)
{
	struct rte_ether_hdr *eth_hdr;
	struct rte_vlan_hdr *vlan_hdr;
	struct rte_ipv4_hdr *ipv4_hdr;
	struct rte_udp_hdr *udp_hdr;
	u16 eth_type = 0;

	eth_hdr = rte_pktmbuf_mtod(mbuf, struct rte_ether_hdr *);
	eth_type = rte_be_to_cpu_16(eth_hdr->ether_type);

	if (eth_type == RTE_ETHER_TYPE_VLAN) {
		vlan_hdr = (struct rte_vlan_hdr *)(eth_hdr + 1);
		eth_type = rte_be_to_cpu_16(vlan_hdr->eth_proto);
	}

	if (eth_type == RTE_ETHER_TYPE_IPV4) {
		ipv4_hdr = rte_pktmbuf_mtod_offset(mbuf, struct rte_ipv4_hdr *,
						   mbuf->outer_l2_len);
		off_info->outer_l3_type = IPV4_PKT_WITH_CHKSUM_OFFLOAD;
		ipv4_hdr->hdr_checksum = 0;

		udp_hdr = (struct rte_udp_hdr *)((char *)ipv4_hdr +
						 mbuf->outer_l3_len);
		udp_hdr->dgram_cksum = 0;
	} else if (eth_type == RTE_ETHER_TYPE_IPV6) {
		off_info->outer_l3_type = IPV6_PKT;

		udp_hdr = rte_pktmbuf_mtod_offset(mbuf, struct rte_udp_hdr *,
						  (mbuf->outer_l2_len +
						   mbuf->outer_l3_len));
		udp_hdr->dgram_cksum = 0;
	}
}

static inline uint8_t hinic_analyze_l3_type(struct rte_mbuf *mbuf)
{
	uint8_t l3_type;
	uint64_t ol_flags = mbuf->ol_flags;

	if (ol_flags & RTE_MBUF_F_TX_IPV4)
		l3_type = (ol_flags & RTE_MBUF_F_TX_IP_CKSUM) ?
			  IPV4_PKT_WITH_CHKSUM_OFFLOAD :
			  IPV4_PKT_NO_CHKSUM_OFFLOAD;
	else if (ol_flags & RTE_MBUF_F_TX_IPV6)
		l3_type = IPV6_PKT;
	else
		l3_type = UNKNOWN_L3TYPE;

	return l3_type;
}

static inline void hinic_calculate_tcp_checksum(struct rte_mbuf *mbuf,
					struct hinic_tx_offload_info *off_info,
					uint64_t inner_l3_offset)
{
	struct rte_ipv4_hdr *ipv4_hdr;
	struct rte_ipv6_hdr *ipv6_hdr;
	struct rte_tcp_hdr *tcp_hdr;
	uint64_t ol_flags = mbuf->ol_flags;

	if (ol_flags & RTE_MBUF_F_TX_IPV4) {
		ipv4_hdr = rte_pktmbuf_mtod_offset(mbuf, struct rte_ipv4_hdr *,
						   inner_l3_offset);

		if (ol_flags & RTE_MBUF_F_TX_IP_CKSUM)
			ipv4_hdr->hdr_checksum = 0;

		tcp_hdr = (struct rte_tcp_hdr *)((char *)ipv4_hdr +
						 mbuf->l3_len);
		tcp_hdr->cksum = hinic_ipv4_phdr_cksum(ipv4_hdr, ol_flags);
	} else {
		ipv6_hdr = rte_pktmbuf_mtod_offset(mbuf, struct rte_ipv6_hdr *,
						   inner_l3_offset);
		tcp_hdr = rte_pktmbuf_mtod_offset(mbuf, struct rte_tcp_hdr *,
						  (inner_l3_offset +
						   mbuf->l3_len));
		tcp_hdr->cksum = hinic_ipv6_phdr_cksum(ipv6_hdr, ol_flags);
	}

	off_info->inner_l4_type = TCP_OFFLOAD_ENABLE;
	off_info->inner_l4_tcp_udp = 1;
}

static inline void hinic_calculate_udp_checksum(struct rte_mbuf *mbuf,
					struct hinic_tx_offload_info *off_info,
					uint64_t inner_l3_offset)
{
	struct rte_ipv4_hdr *ipv4_hdr;
	struct rte_ipv6_hdr *ipv6_hdr;
	struct rte_udp_hdr *udp_hdr;
	uint64_t ol_flags = mbuf->ol_flags;

	if (ol_flags & RTE_MBUF_F_TX_IPV4) {
		ipv4_hdr = rte_pktmbuf_mtod_offset(mbuf, struct rte_ipv4_hdr *,
						   inner_l3_offset);

		if (ol_flags & RTE_MBUF_F_TX_IP_CKSUM)
			ipv4_hdr->hdr_checksum = 0;

		udp_hdr = (struct rte_udp_hdr *)((char *)ipv4_hdr +
						 mbuf->l3_len);
		udp_hdr->dgram_cksum = hinic_ipv4_phdr_cksum(ipv4_hdr,
							     ol_flags);
	} else {
		ipv6_hdr = rte_pktmbuf_mtod_offset(mbuf, struct rte_ipv6_hdr *,
						   inner_l3_offset);

		udp_hdr = rte_pktmbuf_mtod_offset(mbuf, struct rte_udp_hdr *,
						  (inner_l3_offset +
						   mbuf->l3_len));
		udp_hdr->dgram_cksum = hinic_ipv6_phdr_cksum(ipv6_hdr,
							     ol_flags);
	}

	off_info->inner_l4_type = UDP_OFFLOAD_ENABLE;
	off_info->inner_l4_tcp_udp = 1;
}

static inline void
hinic_calculate_sctp_checksum(struct hinic_tx_offload_info *off_info)
{
	off_info->inner_l4_type = SCTP_OFFLOAD_ENABLE;
	off_info->inner_l4_tcp_udp = 0;
	off_info->inner_l4_len = sizeof(struct rte_sctp_hdr);
}

static inline void hinic_calculate_checksum(struct rte_mbuf *mbuf,
					struct hinic_tx_offload_info *off_info,
					uint64_t inner_l3_offset)
{
	uint64_t ol_flags = mbuf->ol_flags;

	switch (ol_flags & RTE_MBUF_F_TX_L4_MASK) {
	case RTE_MBUF_F_TX_UDP_CKSUM:
		hinic_calculate_udp_checksum(mbuf, off_info, inner_l3_offset);
		break;

	case RTE_MBUF_F_TX_TCP_CKSUM:
		hinic_calculate_tcp_checksum(mbuf, off_info, inner_l3_offset);
		break;

	case RTE_MBUF_F_TX_SCTP_CKSUM:
		hinic_calculate_sctp_checksum(off_info);
		break;

	default:
		if (ol_flags & RTE_MBUF_F_TX_TCP_SEG)
			hinic_calculate_tcp_checksum(mbuf, off_info,
						     inner_l3_offset);
		break;
	}
}

static inline int hinic_tx_offload_pkt_prepare(struct rte_mbuf *m,
					struct hinic_tx_offload_info *off_info)
{
	uint64_t inner_l3_offset;
	uint64_t ol_flags = m->ol_flags;

	/* Check if the packets set available offload flags */
	if (!(ol_flags & HINIC_TX_CKSUM_OFFLOAD_MASK))
		return 0;

	/* Support only vxlan offload */
	if (unlikely((ol_flags & RTE_MBUF_F_TX_TUNNEL_MASK) &&
		     !(ol_flags & RTE_MBUF_F_TX_TUNNEL_VXLAN)))
		return -ENOTSUP;

#ifdef RTE_LIBRTE_ETHDEV_DEBUG
	if (rte_validate_tx_offload(m) != 0)
		return -EINVAL;
#endif

	if (ol_flags & RTE_MBUF_F_TX_TUNNEL_VXLAN) {
		off_info->tunnel_type = TUNNEL_UDP_NO_CSUM;

		/* inner_l4_tcp_udp csum should be set to calculate outer
		 * udp checksum when vxlan packets without inner l3 and l4
		 */
		off_info->inner_l4_tcp_udp = 1;

		if ((ol_flags & RTE_MBUF_F_TX_OUTER_IP_CKSUM) ||
		    (ol_flags & RTE_MBUF_F_TX_OUTER_IPV6) ||
		    (ol_flags & RTE_MBUF_F_TX_TCP_SEG)) {
			inner_l3_offset = m->l2_len + m->outer_l2_len +
					  m->outer_l3_len;
			off_info->outer_l2_len = m->outer_l2_len;
			off_info->outer_l3_len = m->outer_l3_len;
			/* just support vxlan tunneling pkt */
			off_info->inner_l2_len = m->l2_len - VXLANLEN -
						 sizeof(struct rte_udp_hdr);
			off_info->tunnel_length = m->l2_len;

			hinic_analyze_outer_ip_vxlan(m, off_info);

			hinic_get_outer_cs_pld_offset(m, off_info);
		} else {
			inner_l3_offset = m->l2_len;
			hinic_analyze_tx_info(m, off_info);
			/* just support vxlan tunneling pkt */
			off_info->inner_l2_len = m->l2_len - VXLANLEN -
						 sizeof(struct rte_udp_hdr) -
						 off_info->outer_l2_len -
						 off_info->outer_l3_len;
			off_info->tunnel_length = m->l2_len -
						  off_info->outer_l2_len -
						  off_info->outer_l3_len;
			off_info->outer_l3_type = IPV4_PKT_NO_CHKSUM_OFFLOAD;

			hinic_get_pld_offset(m, off_info);
		}
	} else {
		inner_l3_offset = m->l2_len;
		off_info->inner_l2_len = m->l2_len;
		off_info->tunnel_type = NOT_TUNNEL;

		hinic_get_pld_offset(m, off_info);
	}

	/* invalid udp or tcp header */
	if (unlikely(off_info->payload_offset > MAX_PLD_OFFSET))
		return -EINVAL;

	off_info->inner_l3_len = m->l3_len;
	off_info->inner_l4_len = m->l4_len;
	off_info->inner_l3_type = hinic_analyze_l3_type(m);

	/* Process the pseudo-header checksum */
	hinic_calculate_checksum(m, off_info, inner_l3_offset);

	return 0;
}

static inline bool hinic_get_sge_txoff_info(struct rte_mbuf *mbuf_pkt,
					    struct hinic_wqe_info *sqe_info,
					    struct hinic_tx_offload_info
					    *off_info)
{
	u16  i, total_len, sge_cnt = mbuf_pkt->nb_segs;
	struct rte_mbuf *mbuf;
	int ret;

	memset(off_info, 0, sizeof(*off_info));

	ret = hinic_tx_offload_pkt_prepare(mbuf_pkt, off_info);
	if (unlikely(ret))
		return false;

	sqe_info->cpy_mbuf_cnt = 0;

	/* non tso mbuf */
	if (likely(!(mbuf_pkt->ol_flags & RTE_MBUF_F_TX_TCP_SEG))) {
		if (unlikely(mbuf_pkt->pkt_len > MAX_SINGLE_SGE_SIZE)) {
			/* non tso packet len must less than 64KB */
			return false;
		} else if (unlikely(HINIC_NONTSO_SEG_NUM_INVALID(sge_cnt))) {
			/* non tso packet buffer number must less than 17
			 * the mbuf segs more than 17 must copy to one buffer
			 */
			total_len = 0;
			mbuf = mbuf_pkt;
			for (i = 0; i < (HINIC_NONTSO_PKT_MAX_SGE - 1) ; i++) {
				total_len += mbuf->data_len;
				mbuf = mbuf->next;
			}

			/* default support copy total 4k mbuf segs */
			if ((u32)(total_len + (u16)HINIC_COPY_MBUF_SIZE) <
				  mbuf_pkt->pkt_len)
				return false;

			sqe_info->sge_cnt = HINIC_NONTSO_PKT_MAX_SGE;
			sqe_info->cpy_mbuf_cnt = 1;
			return true;
		}

		/* valid non tso mbuf */
		sqe_info->sge_cnt = sge_cnt;
	} else {
		/* tso mbuf */
		if (unlikely(HINIC_TSO_SEG_NUM_INVALID(sge_cnt)))
			/* too many mbuf segs */
			return false;

		/* check tso mbuf segs are valid or not */
		if (unlikely(!hinic_is_tso_sge_valid(mbuf_pkt,
			     off_info, sqe_info)))
			return false;
	}

	return true;
}

static inline void hinic_sq_write_db(struct hinic_sq *sq, int cos)
{
	u16 prod_idx;
	u32 hi_prod_idx;
	struct hinic_sq_db sq_db;

	prod_idx = MASKED_SQ_IDX(sq, sq->wq->prod_idx);
	hi_prod_idx = SQ_DB_PI_HIGH(prod_idx);

	sq_db.db_info = SQ_DB_INFO_SET(hi_prod_idx, HI_PI) |
			SQ_DB_INFO_SET(SQ_DB, TYPE) |
			SQ_DB_INFO_SET(SQ_CFLAG_DP, CFLAG) |
			SQ_DB_INFO_SET(cos, COS) |
			SQ_DB_INFO_SET(sq->q_id, QID);

	/* Data should be written to HW in Big Endian Format */
	sq_db.db_info = cpu_to_be32(sq_db.db_info);

	/* Write all before the doorbell */
	rte_wmb();
	writel(sq_db.db_info, SQ_DB_ADDR(sq, prod_idx));
}

u16 hinic_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts, u16 nb_pkts)
{
	int free_wqebb_cnt, wqe_wqebb_cnt;
	u32 queue_info, tx_bytes = 0;
	u16 nb_tx;
	struct hinic_wqe_info sqe_info;
	struct hinic_tx_offload_info off_info;
	struct rte_mbuf *mbuf_pkt;
	struct hinic_txq *txq = tx_queue;
	struct hinic_tx_info *tx_info;
	struct hinic_sq_wqe *sq_wqe;
	struct hinic_sq_task *task;

	/* reclaim tx mbuf before xmit new packet */
	if (HINIC_GET_SQ_FREE_WQEBBS(txq) < txq->tx_free_thresh)
		hinic_xmit_mbuf_cleanup(txq);

	/* tx loop routine */
	for (nb_tx = 0; nb_tx < nb_pkts; nb_tx++) {
		mbuf_pkt = *tx_pkts++;
		queue_info = 0;

		/* 1. parse sge and tx offload info from mbuf */
		if (unlikely(!hinic_get_sge_txoff_info(mbuf_pkt,
						       &sqe_info, &off_info))) {
			txq->txq_stats.off_errs++;
			break;
		}

		/* 2. try to get enough wqebb */
		wqe_wqebb_cnt = HINIC_SQ_WQEBB_CNT(sqe_info.sge_cnt);
		free_wqebb_cnt = HINIC_GET_SQ_FREE_WQEBBS(txq);
		if (unlikely(wqe_wqebb_cnt > free_wqebb_cnt)) {
			/* reclaim again */
			hinic_xmit_mbuf_cleanup(txq);
			free_wqebb_cnt = HINIC_GET_SQ_FREE_WQEBBS(txq);
			if (unlikely(wqe_wqebb_cnt > free_wqebb_cnt)) {
				txq->txq_stats.tx_busy += (nb_pkts - nb_tx);
				break;
			}
		}

		/* 3. get sq tail wqe address from wqe_page,
		 * sq have enough wqebb for this packet
		 */
		sq_wqe = hinic_get_sq_wqe(txq, wqe_wqebb_cnt, &sqe_info);

		/* 4. fill sq wqe sge section */
		if (unlikely(!hinic_mbuf_dma_map_sge(txq, mbuf_pkt,
						     sq_wqe->buf_descs,
						     &sqe_info))) {
			hinic_return_sq_wqe(txq->nic_dev->hwdev, txq->q_id,
					    wqe_wqebb_cnt, sqe_info.owner);
			txq->txq_stats.off_errs++;
			break;
		}

		/* 5. fill sq wqe task section and queue info */
		task = &sq_wqe->task;

		/* tx packet offload configure */
		hinic_fill_tx_offload_info(mbuf_pkt, task, &queue_info,
					   &off_info);

		/* 6. record tx info */
		tx_info = &txq->tx_info[sqe_info.pi];
		tx_info->mbuf = mbuf_pkt;
		tx_info->wqebb_cnt = wqe_wqebb_cnt;

		/* 7. fill sq wqe header section */
		hinic_fill_sq_wqe_header(&sq_wqe->ctrl, queue_info,
					 sqe_info.sge_cnt, sqe_info.owner);

		/* 8.convert continue or bottom wqe byteorder to big endian */
		hinic_sq_wqe_cpu_to_be32(sq_wqe, sqe_info.seq_wqebbs);

		tx_bytes += mbuf_pkt->pkt_len;
	}

	/* 9. write sq doorbell in burst mode */
	if (nb_tx) {
		hinic_sq_write_db(txq->sq, txq->cos);

		txq->txq_stats.packets += nb_tx;
		txq->txq_stats.bytes += tx_bytes;
	}
	txq->txq_stats.burst_pkts = nb_tx;

	return nb_tx;
}

void hinic_free_all_tx_mbufs(struct hinic_txq *txq)
{
	u16 ci;
	struct hinic_nic_dev *nic_dev = txq->nic_dev;
	struct hinic_tx_info *tx_info;
	int free_wqebbs = hinic_get_sq_free_wqebbs(nic_dev->hwdev,
						   txq->q_id) + 1;

	while (free_wqebbs < txq->q_depth) {
		ci = hinic_get_sq_local_ci(nic_dev->hwdev, txq->q_id);

		tx_info = &txq->tx_info[ci];

		if (unlikely(tx_info->cpy_mbuf != NULL)) {
			rte_pktmbuf_free(tx_info->cpy_mbuf);
			tx_info->cpy_mbuf = NULL;
		}

		rte_pktmbuf_free(tx_info->mbuf);
		hinic_update_sq_local_ci(nic_dev->hwdev, txq->q_id,
					 tx_info->wqebb_cnt);

		free_wqebbs += tx_info->wqebb_cnt;
		tx_info->mbuf = NULL;
	}
}

void hinic_free_all_tx_resources(struct rte_eth_dev *eth_dev)
{
	u16 q_id;
	struct hinic_nic_dev *nic_dev =
				HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(eth_dev);

	for (q_id = 0; q_id < nic_dev->num_sq; q_id++) {
		if (eth_dev->data->tx_queues != NULL)
			eth_dev->data->tx_queues[q_id] = NULL;

		if (nic_dev->txqs[q_id] == NULL)
			continue;

		/* stop tx queue free tx mbuf */
		hinic_free_all_tx_mbufs(nic_dev->txqs[q_id]);
		hinic_free_tx_resources(nic_dev->txqs[q_id]);

		/* free txq */
		kfree(nic_dev->txqs[q_id]);
		nic_dev->txqs[q_id] = NULL;
	}
}

void hinic_free_all_tx_mbuf(struct rte_eth_dev *eth_dev)
{
	u16 q_id;
	struct hinic_nic_dev *nic_dev =
				HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(eth_dev);

	for (q_id = 0; q_id < nic_dev->num_sq; q_id++)
		/* stop tx queue free tx mbuf */
		hinic_free_all_tx_mbufs(nic_dev->txqs[q_id]);
}

int hinic_setup_tx_resources(struct hinic_txq *txq)
{
	u64 tx_info_sz;

	tx_info_sz = txq->q_depth * sizeof(*txq->tx_info);
	txq->tx_info = rte_zmalloc_socket("tx_info", tx_info_sz,
			RTE_CACHE_LINE_SIZE, txq->socket_id);
	if (!txq->tx_info)
		return -ENOMEM;

	return HINIC_OK;
}

void hinic_free_tx_resources(struct hinic_txq *txq)
{
	if (txq->tx_info == NULL)
		return;

	rte_free(txq->tx_info);
	txq->tx_info = NULL;
}

int hinic_create_sq(struct hinic_hwdev *hwdev, u16 q_id,
			u16 sq_depth, unsigned int socket_id)
{
	int err;
	struct hinic_nic_io *nic_io = hwdev->nic_io;
	struct hinic_qp *qp = &nic_io->qps[q_id];
	struct hinic_sq *sq = &qp->sq;
	void __iomem *db_addr;
	volatile u32 *ci_addr;

	sq->sq_depth = sq_depth;
	nic_io->sq_depth = sq_depth;

	/* alloc wq */
	err = hinic_wq_allocate(nic_io->hwdev, &nic_io->sq_wq[q_id],
				HINIC_SQ_WQEBB_SHIFT, nic_io->sq_depth,
				socket_id);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to allocate WQ for SQ");
		return err;
	}

	/* alloc sq doorbell space */
	err = hinic_alloc_db_addr(nic_io->hwdev, &db_addr);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to init db addr");
		goto alloc_db_err;
	}

	/* clear hardware ci */
	ci_addr = (volatile u32 *)HINIC_CI_VADDR(nic_io->ci_vaddr_base, q_id);
	*ci_addr = 0;

	sq->q_id = q_id;
	sq->wq = &nic_io->sq_wq[q_id];
	sq->owner = 1;
	sq->cons_idx_addr = (volatile u16 *)ci_addr;
	sq->db_addr = db_addr;

	return HINIC_OK;

alloc_db_err:
	hinic_wq_free(nic_io->hwdev, &nic_io->sq_wq[q_id]);

	return err;
}

void hinic_destroy_sq(struct hinic_hwdev *hwdev, u16 q_id)
{
	struct hinic_nic_io *nic_io;
	struct hinic_qp *qp;

	nic_io = hwdev->nic_io;
	qp = &nic_io->qps[q_id];

	if (qp->sq.wq == NULL)
		return;

	hinic_free_db_addr(nic_io->hwdev, qp->sq.db_addr);
	hinic_wq_free(nic_io->hwdev, qp->sq.wq);
	qp->sq.wq = NULL;
}
