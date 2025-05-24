/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2021-2024 NXP
 */

#include <bus_dpaa_driver.h>
#include <rte_dmadev_pmd.h>
#include <rte_kvargs.h>

#include "dpaa_qdma.h"
#include "dpaa_qdma_logs.h"

static uint32_t s_sg_max_entry_sz = 2000;
static bool s_hw_err_check;

#define DPAA_DMA_ERROR_CHECK "dpaa_dma_err_check"

static inline void
qdma_desc_addr_set64(struct fsl_qdma_comp_cmd_desc *ccdf, u64 addr)
{
	ccdf->addr_hi = upper_32_bits(addr);
	ccdf->addr_lo = rte_cpu_to_le_32(lower_32_bits(addr));
}

static inline void
qdma_desc_sge_addr_set64(struct fsl_qdma_comp_sg_desc *sge, u64 addr)
{
	sge->addr_hi = upper_32_bits(addr);
	sge->addr_lo = rte_cpu_to_le_32(lower_32_bits(addr));
}

static inline int
qdma_ccdf_get_queue(struct fsl_qdma_comp_cmd_desc *ccdf,
	uint8_t *queue_idx)
{
	uint64_t addr = ((uint64_t)ccdf->addr_hi) << 32 | ccdf->addr_lo;

	if (addr && queue_idx)
		*queue_idx = ccdf->queue;
	if (addr) {
		ccdf->addr_hi = 0;
		ccdf->addr_lo = 0;
		return true;
	}

	return false;
}

static inline int
ilog2(int x)
{
	int log = 0;

	x >>= 1;

	while (x) {
		log++;
		x >>= 1;
	}
	return log;
}

static inline int
ilog2_qsize(uint32_t q_size)
{
	return (ilog2(q_size) - ilog2(64));
}

static inline int
ilog2_qthld(uint32_t q_thld)
{
    return (ilog2(q_thld) - ilog2(16));
}

static inline int
fsl_qdma_queue_bd_in_hw(struct fsl_qdma_queue *fsl_queue)
{
	struct rte_dma_stats *stats = &fsl_queue->stats;

	return (stats->submitted - stats->completed);
}

static u32
qdma_readl(void *addr)
{
	return QDMA_IN(addr);
}

static void
qdma_writel(u32 val, void *addr)
{
	QDMA_OUT(addr, val);
}

static u32
qdma_readl_be(void *addr)
{
	return QDMA_IN_BE(addr);
}

static void
qdma_writel_be(u32 val, void *addr)
{
	QDMA_OUT_BE(addr, val);
}

static void *
dma_pool_alloc(char *nm, int size, int aligned, dma_addr_t *phy_addr)
{
	void *virt_addr;

	virt_addr = rte_zmalloc(nm, size, aligned);
	if (!virt_addr)
		return NULL;

	*phy_addr = rte_mem_virt2iova(virt_addr);

	return virt_addr;
}

/*
 * Pre-request command descriptor and compound S/G for enqueue.
 */
static int
fsl_qdma_pre_comp_sd_desc(struct fsl_qdma_queue *queue)
{
	struct fsl_qdma_engine *fsl_qdma = queue->engine;
	struct fsl_qdma_sdf *sdf;
	struct fsl_qdma_ddf *ddf;
	struct fsl_qdma_comp_cmd_desc *ccdf;
	uint16_t i, j;
	struct fsl_qdma_cmpd_ft *ft;

	for (i = 0; i < queue->n_cq; i++) {
		dma_addr_t phy_ft = 0;

		queue->ft[i] = dma_pool_alloc(NULL,
			sizeof(struct fsl_qdma_cmpd_ft),
			RTE_CACHE_LINE_SIZE, &phy_ft);
		if (!queue->ft[i])
			goto fail;
		if (((uint64_t)queue->ft[i]) &
			(RTE_CACHE_LINE_SIZE - 1)) {
			DPAA_QDMA_ERR("FD[%d] addr(%p) not cache aligned",
				i, queue->ft[i]);
			rte_free(queue->ft[i]);
			queue->ft[i] = NULL;
			goto fail;
		}
		if (((uint64_t)(&queue->ft[i]->desc_ssge[0])) &
			(RTE_CACHE_LINE_SIZE - 1)) {
			DPAA_QDMA_ERR("FD[%d] SGE addr(%p) not cache aligned",
				i, &queue->ft[i]->desc_ssge[0]);
			rte_free(queue->ft[i]);
			queue->ft[i] = NULL;
			goto fail;
		}
		queue->ft[i]->phy_ssge = phy_ft +
			offsetof(struct fsl_qdma_cmpd_ft, desc_ssge);
		queue->ft[i]->phy_dsge = phy_ft +
			offsetof(struct fsl_qdma_cmpd_ft, desc_dsge);
		queue->ft[i]->phy_df = phy_ft +
			offsetof(struct fsl_qdma_cmpd_ft, df);

		ft = queue->ft[i];
		sdf = &ft->df.sdf;
		ddf = &ft->df.ddf;
		/* Compound Command Descriptor(Frame List Table) */
		qdma_desc_sge_addr_set64(&ft->desc_buf, ft->phy_df);
		/* It must be 32 as Compound S/G Descriptor */
		ft->desc_buf.length = sizeof(struct fsl_qdma_df);

		/* Descriptor Buffer */
		sdf->srttype = FSL_QDMA_CMD_RWTTYPE;
#ifdef RTE_DMA_DPAA_ERRATA_ERR050265
		sdf->prefetch = 1;
#endif
		ddf->dwttype = FSL_QDMA_CMD_RWTTYPE;
		ddf->lwc = FSL_QDMA_CMD_LWC;

		ccdf = &queue->cq[i];
		qdma_desc_addr_set64(ccdf, phy_ft);
		ccdf->format = FSL_QDMA_COMP_SG_FORMAT;
		if (!fsl_qdma->is_silent)
			ccdf->ser = 1;
		ccdf->queue = queue->queue_id;
	}
	queue->ci = 0;

	return 0;

fail:
	for (j = 0; j < i; j++)
		rte_free(queue->ft[j]);

	return -ENOMEM;
}

static int
fsl_qdma_alloc_queue_resources(struct fsl_qdma_engine *fsl_qdma,
	int queue_id, int block_id)
{
	struct fsl_qdma_queue *cmd_queue;
	uint32_t queue_size;
	char nm[RTE_MEMZONE_NAMESIZE];

	cmd_queue = &fsl_qdma->cmd_queues[block_id][queue_id];
	cmd_queue->engine = fsl_qdma;

	queue_size = sizeof(struct fsl_qdma_comp_cmd_desc) *
		QDMA_QUEUE_SIZE;

	sprintf(nm, "Command queue_%d_%d",
		block_id, queue_id);
	cmd_queue->cq = dma_pool_alloc(nm, queue_size,
		queue_size, &cmd_queue->bus_addr);
	if (!cmd_queue->cq) {
		DPAA_QDMA_ERR("%s alloc failed!", nm);
		return -ENOMEM;
	}

	cmd_queue->block_vir = fsl_qdma->block_base +
		FSL_QDMA_BLOCK_BASE_OFFSET(fsl_qdma, block_id);
	cmd_queue->n_cq = QDMA_QUEUE_SIZE;
	cmd_queue->queue_id = queue_id;
	cmd_queue->block_id = block_id;
	cmd_queue->pending_start = 0;
	cmd_queue->pending_num = 0;
	cmd_queue->complete_start = 0;

	sprintf(nm, "Compound Table_%d_%d",
		block_id, queue_id);
	cmd_queue->ft = rte_zmalloc(nm,
			sizeof(void *) * QDMA_QUEUE_SIZE, 0);
	if (!cmd_queue->ft) {
		DPAA_QDMA_ERR("%s zmalloc failed!", nm);
		rte_free(cmd_queue->cq);
		return -ENOMEM;
	}
	sprintf(nm, "Pending_desc_%d_%d",
		block_id, queue_id);
	cmd_queue->pending_desc = rte_zmalloc(nm,
		sizeof(struct fsl_qdma_desc) * FSL_QDMA_MAX_DESC_NUM, 0);
	if (!cmd_queue->pending_desc) {
		DPAA_QDMA_ERR("%s zmalloc failed!", nm);
		rte_free(cmd_queue->ft);
		rte_free(cmd_queue->cq);
		return -ENOMEM;
	}
	sprintf(nm, "complete-burst_ring_%d_%d",
		block_id, queue_id);
	cmd_queue->complete_burst = rte_ring_create(nm,
		QDMA_QUEUE_SIZE * 2, 0,
		RING_F_SP_ENQ | RING_F_SC_DEQ);
	if (!cmd_queue->complete_burst) {
		DPAA_QDMA_ERR("%s create failed!", nm);
		rte_free(cmd_queue->pending_desc);
		rte_free(cmd_queue->ft);
		rte_free(cmd_queue->cq);
		return -ENOMEM;
	}
	sprintf(nm, "complete-desc_ring_%d_%d",
		block_id, queue_id);
	cmd_queue->complete_desc = rte_ring_create(nm,
		FSL_QDMA_MAX_DESC_NUM * 2, 0,
		RING_F_SP_ENQ | RING_F_SC_DEQ);
	if (!cmd_queue->complete_desc) {
		DPAA_QDMA_ERR("%s create failed!", nm);
		rte_ring_free(cmd_queue->complete_burst);
		rte_free(cmd_queue->pending_desc);
		rte_free(cmd_queue->ft);
		rte_free(cmd_queue->cq);
		return -ENOMEM;
	}
	sprintf(nm, "complete-pool-desc_ring_%d_%d",
		block_id, queue_id);
	cmd_queue->complete_pool = rte_ring_create(nm,
		FSL_QDMA_MAX_DESC_NUM * 2, 0,
		RING_F_SP_ENQ | RING_F_SC_DEQ);
	if (!cmd_queue->complete_pool) {
		DPAA_QDMA_ERR("%s create failed!", nm);
		rte_ring_free(cmd_queue->complete_desc);
		rte_ring_free(cmd_queue->complete_burst);
		rte_free(cmd_queue->pending_desc);
		rte_free(cmd_queue->ft);
		rte_free(cmd_queue->cq);
		return -ENOMEM;
	}

	memset(&cmd_queue->stats, 0, sizeof(struct rte_dma_stats));
	cmd_queue->pending_max = FSL_QDMA_MAX_DESC_NUM;

	return 0;
}

static void
fsl_qdma_free_cmdq_res(struct fsl_qdma_queue *queue)
{
	rte_free(queue->ft);
	rte_free(queue->cq);
	rte_free(queue->pending_desc);
	rte_ring_free(queue->complete_burst);
	rte_ring_free(queue->complete_desc);
	rte_ring_free(queue->complete_pool);
}

static void
fsl_qdma_free_stq_res(struct fsl_qdma_status_queue *queue)
{
	rte_free(queue->cq);
}

static int
fsl_qdma_prep_status_queue(struct fsl_qdma_engine *fsl_qdma,
	uint32_t block_id)
{
	struct fsl_qdma_status_queue *status;
	uint32_t status_size;

	status = &fsl_qdma->stat_queues[block_id];
	status->engine = fsl_qdma;

	status_size = QDMA_STATUS_SIZE *
		sizeof(struct fsl_qdma_comp_cmd_desc);

	status->cq = dma_pool_alloc(NULL, status_size,
		status_size, &status->bus_addr);

	if (!status->cq)
		return -ENOMEM;

	memset(status->cq, 0x0, status_size);
	status->n_cq = QDMA_STATUS_SIZE;
	status->complete = 0;
	status->block_id = block_id;
	status->block_vir = fsl_qdma->block_base +
		FSL_QDMA_BLOCK_BASE_OFFSET(fsl_qdma, block_id);

	return 0;
}

static int
fsl_qdma_halt(struct fsl_qdma_engine *fsl_qdma)
{
	void *ctrl = fsl_qdma->ctrl_base;
	void *block;
	int i, count = RETRIES;
	unsigned int j;
	u32 reg;

	/* Disable the command queue and wait for idle state. */
	reg = qdma_readl(ctrl + FSL_QDMA_DMR);
	reg |= FSL_QDMA_DMR_DQD;
	qdma_writel(reg, ctrl + FSL_QDMA_DMR);
	for (j = 0; j < fsl_qdma->num_blocks; j++) {
		block = fsl_qdma->block_base +
			FSL_QDMA_BLOCK_BASE_OFFSET(fsl_qdma, j);
		for (i = 0; i < FSL_QDMA_QUEUE_NUM_MAX; i++)
			qdma_writel(0, block + FSL_QDMA_BCQMR(i));
	}
	while (true) {
		reg = qdma_readl(ctrl + FSL_QDMA_DSR);
		if (!(reg & FSL_QDMA_DSR_DB))
			break;
		if (count-- < 0)
			return -EBUSY;
		rte_delay_us(100);
	}

	for (j = 0; j < fsl_qdma->num_blocks; j++) {
		block = fsl_qdma->block_base +
			FSL_QDMA_BLOCK_BASE_OFFSET(fsl_qdma, j);

		/* Disable status queue. */
		qdma_writel(0, block + FSL_QDMA_BSQMR);

		/*
		 * clear the command queue interrupt detect register for
		 * all queues.
		 */
		qdma_writel(0xffffffff, block + FSL_QDMA_BCQIDR(0));
	}

	return 0;
}

static void
fsl_qdma_data_validation(struct fsl_qdma_desc *desc[],
	uint8_t num, struct fsl_qdma_queue *fsl_queue)
{
	uint32_t i, j;
	uint8_t *v_src, *v_dst;
	char err_msg[512];
	int offset;


	offset = sprintf(err_msg, "Fatal TC%d/queue%d: ",
		fsl_queue->block_id,
		fsl_queue->queue_id);
	for (i = 0; i < num; i++) {
		v_src = rte_mem_iova2virt(desc[i]->src);
		v_dst = rte_mem_iova2virt(desc[i]->dst);
		for (j = 0; j < desc[i]->len; j++) {
			if (v_src[j] != v_dst[j]) {
				sprintf(&err_msg[offset],
					"job[%"PRIu64"]:src(%p)[%d](%d)!=dst(%p)[%d](%d)",
					desc[i]->flag, v_src, j, v_src[j],
					v_dst, j, v_dst[j]);
				DPAA_QDMA_ERR("%s, stop validating!",
					err_msg);
				return;
			}
		}
	}
}

static int
fsl_qdma_reg_init(struct fsl_qdma_engine *fsl_qdma)
{
	struct fsl_qdma_queue *temp;
	struct fsl_qdma_status_queue *temp_stat;
	void *ctrl = fsl_qdma->ctrl_base;
	void *block;
	u32 i, j;
	u32 reg;
	int ret, val;

	/* Try to halt the qDMA engine first. */
	ret = fsl_qdma_halt(fsl_qdma);
	if (ret) {
		DPAA_QDMA_ERR("DMA halt failed!");
		return ret;
	}

	for (j = 0; j < fsl_qdma->num_blocks; j++) {
		block = fsl_qdma->block_base +
			FSL_QDMA_BLOCK_BASE_OFFSET(fsl_qdma, j);
		for (i = 0; i < QDMA_QUEUES; i++) {
			temp = &fsl_qdma->cmd_queues[j][i];
			/*
			 * Initialize Command Queue registers to
			 * point to the first
			 * command descriptor in memory.
			 * Dequeue Pointer Address Registers
			 * Enqueue Pointer Address Registers
			 */

			qdma_writel(lower_32_bits(temp->bus_addr),
				    block + FSL_QDMA_BCQDPA_SADDR(i));
			qdma_writel(upper_32_bits(temp->bus_addr),
				    block + FSL_QDMA_BCQEDPA_SADDR(i));
			qdma_writel(lower_32_bits(temp->bus_addr),
				    block + FSL_QDMA_BCQEPA_SADDR(i));
			qdma_writel(upper_32_bits(temp->bus_addr),
				    block + FSL_QDMA_BCQEEPA_SADDR(i));

			/* Initialize the queue mode. */
			reg = FSL_QDMA_BCQMR_EN;
			reg |= FSL_QDMA_BCQMR_CD_THLD(ilog2_qthld(temp->n_cq));
			reg |= FSL_QDMA_BCQMR_CQ_SIZE(ilog2_qsize(temp->n_cq));
			temp->le_cqmr = reg;
			qdma_writel(reg, block + FSL_QDMA_BCQMR(i));
		}

		/*
		 * Workaround for erratum: ERR010812.
		 * We must enable XOFF to avoid the enqueue rejection occurs.
		 * Setting SQCCMR ENTER_WM to 0x20.
		 */

		qdma_writel(FSL_QDMA_SQCCMR_ENTER_WM,
			    block + FSL_QDMA_SQCCMR);

		/*
		 * Initialize status queue registers to point to the first
		 * command descriptor in memory.
		 * Dequeue Pointer Address Registers
		 * Enqueue Pointer Address Registers
		 */

		temp_stat = &fsl_qdma->stat_queues[j];
		qdma_writel(upper_32_bits(temp_stat->bus_addr),
			block + FSL_QDMA_SQEEPAR);
		qdma_writel(lower_32_bits(temp_stat->bus_addr),
			block + FSL_QDMA_SQEPAR);
		qdma_writel(upper_32_bits(temp_stat->bus_addr),
			block + FSL_QDMA_SQEDPAR);
		qdma_writel(lower_32_bits(temp_stat->bus_addr),
			block + FSL_QDMA_SQDPAR);
		/* Desiable status queue interrupt. */

		qdma_writel(0x0, block + FSL_QDMA_BCQIER(0));
		qdma_writel(0x0, block + FSL_QDMA_BSQICR);
		qdma_writel(0x0, block + FSL_QDMA_CQIER);

		/* Initialize the status queue mode. */
		reg = FSL_QDMA_BSQMR_EN;
		val = ilog2_qsize(temp_stat->n_cq);
		reg |= FSL_QDMA_BSQMR_CQ_SIZE(val);
		qdma_writel(reg, block + FSL_QDMA_BSQMR);
	}

	reg = qdma_readl(ctrl + FSL_QDMA_DMR);
	reg &= ~FSL_QDMA_DMR_DQD;
	qdma_writel(reg, ctrl + FSL_QDMA_DMR);

	return 0;
}

static uint16_t
dpaa_qdma_block_dequeue(struct fsl_qdma_engine *fsl_qdma,
	uint8_t block_id)
{
	struct fsl_qdma_status_queue *stat_queue;
	struct fsl_qdma_queue *cmd_queue;
	struct fsl_qdma_comp_cmd_desc *cq;
	uint16_t start, count = 0;
	uint8_t qid = 0;
	uint32_t reg;
	int ret;
	uint8_t *block;
	uint16_t *dq_complete;
	struct fsl_qdma_desc *desc[FSL_QDMA_SG_MAX_ENTRY];

	stat_queue = &fsl_qdma->stat_queues[block_id];
	cq = stat_queue->cq;
	start = stat_queue->complete;

	block = fsl_qdma->block_base +
		FSL_QDMA_BLOCK_BASE_OFFSET(fsl_qdma, block_id);

	do {
		reg = qdma_readl_be(block + FSL_QDMA_BSQSR);
		if (reg & FSL_QDMA_BSQSR_QE_BE)
			break;

		qdma_writel_be(FSL_QDMA_BSQMR_DI, block + FSL_QDMA_BSQMR);
		ret = qdma_ccdf_get_queue(&cq[start], &qid);
		if (ret == true) {
			cmd_queue = &fsl_qdma->cmd_queues[block_id][qid];

			ret = rte_ring_dequeue(cmd_queue->complete_burst,
				(void **)&dq_complete);
			if (ret) {
				DPAA_QDMA_ERR("DQ desc number failed!");
				break;
			}

			ret = rte_ring_dequeue_bulk(cmd_queue->complete_desc,
				(void **)desc, *dq_complete, NULL);
			if (ret != (*dq_complete)) {
				DPAA_QDMA_ERR("DQ %d descs failed!(%d)",
					*dq_complete, ret);
				break;
			}

			fsl_qdma_data_validation(desc, *dq_complete, cmd_queue);

			ret = rte_ring_enqueue_bulk(cmd_queue->complete_pool,
				(void **)desc, (*dq_complete), NULL);
			if (ret != (*dq_complete)) {
				DPAA_QDMA_ERR("Failed desc eq %d!=%d to %s",
					ret, *dq_complete,
					cmd_queue->complete_pool->name);
				break;
			}

			cmd_queue->complete_start =
				(cmd_queue->complete_start + (*dq_complete)) &
				(cmd_queue->pending_max - 1);
			cmd_queue->stats.completed++;

			start++;
			if (unlikely(start == stat_queue->n_cq))
				start = 0;
			count++;
		} else {
			DPAA_QDMA_ERR("Block%d not empty but dq-queue failed!",
				block_id);
			break;
		}
	} while (1);
	stat_queue->complete = start;

	return count;
}

static int
fsl_qdma_enqueue_desc_to_ring(struct fsl_qdma_queue *fsl_queue,
	uint16_t num)
{
	struct fsl_qdma_engine *fsl_qdma = fsl_queue->engine;
	uint16_t i, idx, start, dq;
	int ret, dq_cnt;

	if (fsl_qdma->is_silent)
		return 0;

	fsl_queue->desc_in_hw[fsl_queue->ci] = num;
eq_again:
	ret = rte_ring_enqueue(fsl_queue->complete_burst,
			&fsl_queue->desc_in_hw[fsl_queue->ci]);
	if (ret) {
		DPAA_QDMA_DP_DEBUG("%s: Queue is full, try dequeue first",
			__func__);
		DPAA_QDMA_DP_DEBUG("%s: submitted:%"PRIu64", completed:%"PRIu64"",
			__func__, fsl_queue->stats.submitted,
			fsl_queue->stats.completed);
		dq_cnt = 0;
dq_again:
		dq = dpaa_qdma_block_dequeue(fsl_queue->engine,
			fsl_queue->block_id);
		dq_cnt++;
		if (dq > 0) {
			goto eq_again;
		} else {
			if (dq_cnt < 100)
				goto dq_again;
			DPAA_QDMA_ERR("%s: Dq block%d failed!",
				__func__, fsl_queue->block_id);
		}
		return ret;
	}
	start = fsl_queue->pending_start;
	for (i = 0; i < num; i++) {
		idx = (start + i) & (fsl_queue->pending_max - 1);
		ret = rte_ring_enqueue(fsl_queue->complete_desc,
				&fsl_queue->pending_desc[idx]);
		if (ret) {
			DPAA_QDMA_ERR("Descriptors eq failed!");
			return ret;
		}
	}

	return 0;
}

static int
fsl_qdma_enqueue_overflow(struct fsl_qdma_queue *fsl_queue)
{
	int overflow = 0;
	uint32_t reg;
	uint16_t blk_drain, check_num, drain_num;
	uint8_t *block = fsl_queue->block_vir;
	const struct rte_dma_stats *st = &fsl_queue->stats;
	struct fsl_qdma_engine *fsl_qdma = fsl_queue->engine;

	check_num = 0;
overflow_check:
	if (fsl_qdma->is_silent || unlikely(s_hw_err_check)) {
		reg = qdma_readl_be(block +
			 FSL_QDMA_BCQSR(fsl_queue->queue_id));
		overflow = (reg & FSL_QDMA_BCQSR_QF_XOFF_BE) ?
			1 : 0;
	} else {
		overflow = (fsl_qdma_queue_bd_in_hw(fsl_queue) >=
			QDMA_QUEUE_CR_WM) ? 1 : 0;
	}

	if (likely(!overflow)) {
		return 0;
	} else if (fsl_qdma->is_silent) {
		check_num++;
		if (check_num >= 10000) {
			DPAA_QDMA_WARN("Waiting for HW complete in silent mode");
			check_num = 0;
		}
		goto overflow_check;
	}

	DPAA_QDMA_DP_DEBUG("TC%d/Q%d submitted(%"PRIu64")-completed(%"PRIu64") >= %d",
		fsl_queue->block_id, fsl_queue->queue_id,
		st->submitted, st->completed, QDMA_QUEUE_CR_WM);
	drain_num = 0;

drain_again:
	blk_drain = dpaa_qdma_block_dequeue(fsl_qdma,
		fsl_queue->block_id);
	if (!blk_drain) {
		drain_num++;
		if (drain_num >= 10000) {
			DPAA_QDMA_WARN("TC%d failed drain, Q%d's %"PRIu64" bd in HW.",
				fsl_queue->block_id, fsl_queue->queue_id,
				st->submitted - st->completed);
			drain_num = 0;
		}
		goto drain_again;
	}
	check_num++;
	if (check_num >= 1000) {
		DPAA_QDMA_WARN("TC%d failed check, Q%d's %"PRIu64" bd in HW.",
			fsl_queue->block_id, fsl_queue->queue_id,
			st->submitted - st->completed);
		check_num = 0;
	}
	goto overflow_check;

	return 0;
}

static int
fsl_qdma_enqueue_desc_single(struct fsl_qdma_queue *fsl_queue,
	dma_addr_t dst, dma_addr_t src, size_t len)
{
	uint8_t *block = fsl_queue->block_vir;
	struct fsl_qdma_comp_sg_desc *csgf_src, *csgf_dest;
	struct fsl_qdma_cmpd_ft *ft;
	int ret;
#ifdef RTE_DMA_DPAA_ERRATA_ERR050757
	struct fsl_qdma_sdf *sdf;
#endif

	ret = fsl_qdma_enqueue_overflow(fsl_queue);
	if (unlikely(ret))
		return ret;

	ft = fsl_queue->ft[fsl_queue->ci];

#ifdef RTE_DMA_DPAA_ERRATA_ERR050757
	sdf = &ft->df.sdf;
	sdf->srttype = FSL_QDMA_CMD_RWTTYPE;
#ifdef RTE_DMA_DPAA_ERRATA_ERR050265
	sdf->prefetch = 1;
#endif
	if (len > FSL_QDMA_CMD_SS_ERR050757_LEN) {
		sdf->ssen = 1;
		sdf->sss = FSL_QDMA_CMD_SS_ERR050757_LEN;
		sdf->ssd = FSL_QDMA_CMD_SS_ERR050757_LEN;
	} else {
		sdf->ssen = 0;
		sdf->sss = 0;
		sdf->ssd = 0;
	}
#endif
	csgf_src = &ft->desc_sbuf;
	csgf_dest = &ft->desc_dbuf;
	qdma_desc_sge_addr_set64(csgf_src, src);
	csgf_src->length = len;
	csgf_src->extion = 0;
	qdma_desc_sge_addr_set64(csgf_dest, dst);
	csgf_dest->length = len;
	csgf_dest->extion = 0;
	/* This entry is the last entry. */
	csgf_dest->final = 1;

	ret = fsl_qdma_enqueue_desc_to_ring(fsl_queue, 1);
	if (ret)
		return ret;
	fsl_queue->ci = (fsl_queue->ci + 1) & (fsl_queue->n_cq - 1);

	qdma_writel(fsl_queue->le_cqmr | FSL_QDMA_BCQMR_EI,
		block + FSL_QDMA_BCQMR(fsl_queue->queue_id));
	fsl_queue->stats.submitted++;

	return 0;
}

static int
fsl_qdma_enqueue_desc_sg(struct fsl_qdma_queue *fsl_queue)
{
	uint8_t *block = fsl_queue->block_vir;
	struct fsl_qdma_comp_sg_desc *csgf_src, *csgf_dest;
	struct fsl_qdma_cmpd_ft *ft;
	uint32_t total_len;
	uint16_t start, idx, num, i, next_idx;
	int ret;
#ifdef RTE_DMA_DPAA_ERRATA_ERR050757
	struct fsl_qdma_sdf *sdf;
#endif

eq_sg:
	total_len = 0;
	start = fsl_queue->pending_start;
	if (fsl_queue->pending_desc[start].len > s_sg_max_entry_sz ||
		fsl_queue->pending_num == 1) {
		ret = fsl_qdma_enqueue_desc_single(fsl_queue,
			fsl_queue->pending_desc[start].dst,
			fsl_queue->pending_desc[start].src,
			fsl_queue->pending_desc[start].len);
		if (!ret) {
			fsl_queue->pending_start =
				(start + 1) & (fsl_queue->pending_max - 1);
			fsl_queue->pending_num--;
		}
		if (fsl_queue->pending_num > 0)
			goto eq_sg;

		return ret;
	}

	ret = fsl_qdma_enqueue_overflow(fsl_queue);
	if (unlikely(ret))
		return ret;

	if (fsl_queue->pending_num > FSL_QDMA_SG_MAX_ENTRY)
		num = FSL_QDMA_SG_MAX_ENTRY;
	else
		num = fsl_queue->pending_num;

	ft = fsl_queue->ft[fsl_queue->ci];
	csgf_src = &ft->desc_sbuf;
	csgf_dest = &ft->desc_dbuf;

	qdma_desc_sge_addr_set64(csgf_src, ft->phy_ssge);
	csgf_src->extion = 1;
	qdma_desc_sge_addr_set64(csgf_dest, ft->phy_dsge);
	csgf_dest->extion = 1;
	/* This entry is the last entry. */
	csgf_dest->final = 1;
	for (i = 0; i < num; i++) {
		idx = (start + i) & (fsl_queue->pending_max - 1);
		qdma_desc_sge_addr_set64(&ft->desc_ssge[i],
			fsl_queue->pending_desc[idx].src);
		ft->desc_ssge[i].length = fsl_queue->pending_desc[idx].len;
		ft->desc_ssge[i].final = 0;
		qdma_desc_sge_addr_set64(&ft->desc_dsge[i],
			fsl_queue->pending_desc[idx].dst);
		ft->desc_dsge[i].length = fsl_queue->pending_desc[idx].len;
		ft->desc_dsge[i].final = 0;
		total_len += fsl_queue->pending_desc[idx].len;
		if ((i + 1) != num) {
			next_idx = (idx + 1) & (fsl_queue->pending_max - 1);
			if (fsl_queue->pending_desc[next_idx].len >
				s_sg_max_entry_sz) {
				num = i + 1;
				break;
			}
		}
	}

	ft->desc_ssge[num - 1].final = 1;
	ft->desc_dsge[num - 1].final = 1;
	csgf_src->length = total_len;
	csgf_dest->length = total_len;
#ifdef RTE_DMA_DPAA_ERRATA_ERR050757
	sdf = &ft->df.sdf;
	sdf->srttype = FSL_QDMA_CMD_RWTTYPE;
#ifdef RTE_DMA_DPAA_ERRATA_ERR050265
	sdf->prefetch = 1;
#endif
	if (total_len > FSL_QDMA_CMD_SS_ERR050757_LEN) {
		sdf->ssen = 1;
		sdf->sss = FSL_QDMA_CMD_SS_ERR050757_LEN;
		sdf->ssd = FSL_QDMA_CMD_SS_ERR050757_LEN;
	} else {
		sdf->ssen = 0;
		sdf->sss = 0;
		sdf->ssd = 0;
	}
#endif
	ret = fsl_qdma_enqueue_desc_to_ring(fsl_queue, num);
	if (ret)
		return ret;

	fsl_queue->ci = (fsl_queue->ci + 1) & (fsl_queue->n_cq - 1);

	qdma_writel(fsl_queue->le_cqmr | FSL_QDMA_BCQMR_EI,
		block + FSL_QDMA_BCQMR(fsl_queue->queue_id));
	fsl_queue->stats.submitted++;

	fsl_queue->pending_start =
		(start + num) & (fsl_queue->pending_max - 1);
	fsl_queue->pending_num -= num;
	if (fsl_queue->pending_num > 0)
		goto eq_sg;

	return 0;
}

static int
fsl_qdma_enqueue_desc(struct fsl_qdma_queue *fsl_queue)
{
	uint16_t start = fsl_queue->pending_start;
	int ret;

	if (fsl_queue->pending_num == 1) {
		ret = fsl_qdma_enqueue_desc_single(fsl_queue,
			fsl_queue->pending_desc[start].dst,
			fsl_queue->pending_desc[start].src,
			fsl_queue->pending_desc[start].len);
		if (!ret) {
			fsl_queue->pending_start =
				(start + 1) & (fsl_queue->pending_max - 1);
			fsl_queue->pending_num = 0;
		}
		return ret;
	}

	return fsl_qdma_enqueue_desc_sg(fsl_queue);
}

static int
dpaa_qdma_info_get(const struct rte_dma_dev *dev,
	struct rte_dma_info *dev_info, __rte_unused uint32_t info_sz)
{
	struct fsl_qdma_engine *fsl_qdma = dev->data->dev_private;

	dev_info->dev_capa = RTE_DMA_CAPA_MEM_TO_MEM |
		RTE_DMA_CAPA_SILENT | RTE_DMA_CAPA_OPS_COPY |
		RTE_DMA_CAPA_OPS_COPY_SG;
	dev_info->dev_capa |= DPAA_QDMA_FLAGS_INDEX;
	dev_info->max_vchans = fsl_qdma->n_queues;
	dev_info->max_desc = FSL_QDMA_MAX_DESC_NUM;
	dev_info->min_desc = QDMA_QUEUE_SIZE;
	dev_info->max_sges = FSL_QDMA_SG_MAX_ENTRY;

	return 0;
}

static int
dpaa_get_channel(struct fsl_qdma_engine *fsl_qdma,
	uint16_t vchan)
{
	int ret, i, j, found = 0;
	struct fsl_qdma_queue *fsl_queue = fsl_qdma->chan[vchan];

	if (fsl_queue) {
		found = 1;
		goto queue_found;
	}

	for (i = 0; i < QDMA_BLOCKS; i++) {
		for (j = 0; j < QDMA_QUEUES; j++) {
			fsl_queue = &fsl_qdma->cmd_queues[i][j];

			if (fsl_queue->channel_id == vchan) {
				found = 1;
				fsl_qdma->chan[vchan] = fsl_queue;
				goto queue_found;
			}
		}
	}

queue_found:
	if (!found)
		return -ENXIO;

	if (fsl_queue->used)
		return 0;

	ret = fsl_qdma_pre_comp_sd_desc(fsl_queue);
	if (ret)
		return ret;

	fsl_queue->used = 1;
	fsl_qdma->block_queues[fsl_queue->block_id]++;

	return 0;
}

static int
dpaa_qdma_configure(struct rte_dma_dev *dmadev,
	const struct rte_dma_conf *dev_conf,
	__rte_unused uint32_t conf_sz)
{
	struct fsl_qdma_engine *fsl_qdma = dmadev->data->dev_private;

	fsl_qdma->is_silent = dev_conf->flags & RTE_DMA_CFG_FLAG_SILENT;
	return 0;
}

static int
dpaa_qdma_start(__rte_unused struct rte_dma_dev *dev)
{
	return 0;
}

static int
dpaa_qdma_close(__rte_unused struct rte_dma_dev *dev)
{
	return 0;
}

static int
dpaa_qdma_queue_setup(struct rte_dma_dev *dmadev,
		      uint16_t vchan,
		      __rte_unused const struct rte_dma_vchan_conf *conf,
		      __rte_unused uint32_t conf_sz)
{
	struct fsl_qdma_engine *fsl_qdma = dmadev->data->dev_private;

	return dpaa_get_channel(fsl_qdma, vchan);
}

static int
dpaa_qdma_submit(void *dev_private, uint16_t vchan)
{
	struct fsl_qdma_engine *fsl_qdma = dev_private;
	struct fsl_qdma_queue *fsl_queue = fsl_qdma->chan[vchan];

	if (!fsl_queue->pending_num)
		return 0;

	return fsl_qdma_enqueue_desc(fsl_queue);
}

static int
dpaa_qdma_enqueue(void *dev_private, uint16_t vchan,
	rte_iova_t src, rte_iova_t dst,
	uint32_t length, uint64_t flags)
{
	struct fsl_qdma_engine *fsl_qdma = dev_private;
	struct fsl_qdma_queue *fsl_queue = fsl_qdma->chan[vchan];
	uint16_t start = fsl_queue->pending_start;
	uint8_t pending = fsl_queue->pending_num;
	uint16_t idx;
	int ret;

	if (pending >= fsl_queue->pending_max) {
		DPAA_QDMA_ERR("Too many pending jobs(%d) on queue%d",
			pending, vchan);
		return -ENOSPC;
	}
	idx = (start + pending) & (fsl_queue->pending_max - 1);

	fsl_queue->pending_desc[idx].src = src;
	fsl_queue->pending_desc[idx].dst = dst;
	fsl_queue->pending_desc[idx].flag =
		DPAA_QDMA_IDX_FROM_FLAG(flags);
	fsl_queue->pending_desc[idx].len = length;
	fsl_queue->pending_num++;

	if (!(flags & RTE_DMA_OP_FLAG_SUBMIT))
		return idx;

	ret = fsl_qdma_enqueue_desc(fsl_queue);
	if (!ret)
		return fsl_queue->pending_start;

	return ret;
}

static int
dpaa_qdma_copy_sg(void *dev_private,
	uint16_t vchan,
	const struct rte_dma_sge *src,
	const struct rte_dma_sge *dst,
	uint16_t nb_src, uint16_t nb_dst,
	uint64_t flags)
{
	int ret;
	uint16_t i, start, idx;
	struct fsl_qdma_engine *fsl_qdma = dev_private;
	struct fsl_qdma_queue *fsl_queue = fsl_qdma->chan[vchan];
	const uint16_t *idx_addr = NULL;

	if (unlikely(nb_src != nb_dst)) {
		DPAA_QDMA_ERR("%s: nb_src(%d) != nb_dst(%d) on  queue%d",
			__func__, nb_src, nb_dst, vchan);
		return -EINVAL;
	}

	if ((fsl_queue->pending_num + nb_src) > FSL_QDMA_SG_MAX_ENTRY) {
		DPAA_QDMA_ERR("Too many pending jobs on queue%d",
			vchan);
		return -ENOSPC;
	}
	start = fsl_queue->pending_start + fsl_queue->pending_num;
	start = start & (fsl_queue->pending_max - 1);
	idx = start;

	idx_addr = DPAA_QDMA_IDXADDR_FROM_SG_FLAG(flags);

	for (i = 0; i < nb_src; i++) {
		if (unlikely(src[i].length != dst[i].length)) {
			DPAA_QDMA_ERR("src.len(%d) != dst.len(%d)",
				src[i].length, dst[i].length);
			return -EINVAL;
		}
		idx = (start + i) & (fsl_queue->pending_max - 1);
		fsl_queue->pending_desc[idx].src = src[i].addr;
		fsl_queue->pending_desc[idx].dst = dst[i].addr;
		fsl_queue->pending_desc[idx].len = dst[i].length;
		fsl_queue->pending_desc[idx].flag = idx_addr[i];
	}
	fsl_queue->pending_num += nb_src;

	if (!(flags & RTE_DMA_OP_FLAG_SUBMIT))
		return idx;

	ret = fsl_qdma_enqueue_desc(fsl_queue);
	if (!ret)
		return fsl_queue->pending_start;

	return ret;
}

static int
dpaa_qdma_err_handle(struct fsl_qdma_err_reg *reg)
{
	struct fsl_qdma_err_reg local;
	size_t i, offset = 0;
	char err_msg[512];

	local.dedr_be = rte_read32(&reg->dedr_be);
	if (!local.dedr_be)
		return 0;
	offset = sprintf(err_msg, "ERR detected:");
	if (local.dedr.ere) {
		offset += sprintf(&err_msg[offset],
			" ere(Enqueue rejection error)");
	}
	if (local.dedr.dde) {
		offset += sprintf(&err_msg[offset],
			" dde(Destination descriptor error)");
	}
	if (local.dedr.sde) {
		offset += sprintf(&err_msg[offset],
			" sde(Source descriptor error)");
	}
	if (local.dedr.cde) {
		offset += sprintf(&err_msg[offset],
			" cde(Command descriptor error)");
	}
	if (local.dedr.wte) {
		offset += sprintf(&err_msg[offset],
			" wte(Write transaction error)");
	}
	if (local.dedr.rte) {
		offset += sprintf(&err_msg[offset],
			" rte(Read transaction error)");
	}
	if (local.dedr.me) {
		offset += sprintf(&err_msg[offset],
			" me(Multiple errors of the same type)");
	}
	DPAA_QDMA_ERR("%s", err_msg);
	for (i = 0; i < FSL_QDMA_DECCD_ERR_NUM; i++) {
		local.deccd_le[FSL_QDMA_DECCD_ERR_NUM - 1 - i] =
			QDMA_IN(&reg->deccd_le[i]);
	}
	local.deccqidr_be = rte_read32(&reg->deccqidr_be);
	local.decbr = rte_read32(&reg->decbr);

	offset = sprintf(err_msg, "ERR command:");
	offset += sprintf(&err_msg[offset],
		" status: %02x, ser: %d, offset:%d, fmt: %02x",
		local.err_cmd.status, local.err_cmd.ser,
		local.err_cmd.offset, local.err_cmd.format);
	offset += sprintf(&err_msg[offset],
		" address: 0x%"PRIx64", queue: %d, dd: %02x",
		(uint64_t)local.err_cmd.addr_hi << 32 |
		local.err_cmd.addr_lo,
		local.err_cmd.queue, local.err_cmd.dd);
	DPAA_QDMA_ERR("%s", err_msg);
	DPAA_QDMA_ERR("ERR command block: %d, queue: %d",
		local.deccqidr.block, local.deccqidr.queue);

	rte_write32(local.dedr_be, &reg->dedr_be);

	return -EIO;
}

static uint16_t
dpaa_qdma_dequeue_status(void *dev_private, uint16_t vchan,
	const uint16_t nb_cpls, uint16_t *last_idx,
	enum rte_dma_status_code *st)
{
	struct fsl_qdma_engine *fsl_qdma = dev_private;
	int err;
	struct fsl_qdma_queue *fsl_queue = fsl_qdma->chan[vchan];
	void *status = fsl_qdma->status_base;
	struct fsl_qdma_desc *desc_complete[nb_cpls];
	uint16_t i, dq_num;

	if (unlikely(fsl_qdma->is_silent)) {
		DPAA_QDMA_WARN("Can't dq in silent mode");

		return 0;
	}

	dq_num = dpaa_qdma_block_dequeue(fsl_qdma,
			fsl_queue->block_id);
	DPAA_QDMA_DP_DEBUG("%s: block dq(%d)",
		__func__, dq_num);

	dq_num = rte_ring_dequeue_burst(fsl_queue->complete_pool,
			(void **)desc_complete, nb_cpls, NULL);
	for (i = 0; i < dq_num; i++)
		last_idx[i] = desc_complete[i]->flag;

	if (st) {
		for (i = 0; i < dq_num; i++)
			st[i] = RTE_DMA_STATUS_SUCCESSFUL;
	}

	if (s_hw_err_check) {
		err = dpaa_qdma_err_handle(status +
			FSL_QDMA_ERR_REG_STATUS_OFFSET);
		if (err)
			fsl_queue->stats.errors++;
	}

	return dq_num;
}

static uint16_t
dpaa_qdma_dequeue(void *dev_private,
	uint16_t vchan, const uint16_t nb_cpls,
	uint16_t *last_idx, bool *has_error)
{
	struct fsl_qdma_engine *fsl_qdma = dev_private;
	int err;
	struct fsl_qdma_queue *fsl_queue = fsl_qdma->chan[vchan];
	void *status = fsl_qdma->status_base;
	struct fsl_qdma_desc *desc_complete[nb_cpls];
	uint16_t i, dq_num;

	if (unlikely(fsl_qdma->is_silent)) {
		DPAA_QDMA_WARN("Can't dq in silent mode");

		return 0;
	}

	*has_error = false;
	dq_num = dpaa_qdma_block_dequeue(fsl_qdma,
		fsl_queue->block_id);
	DPAA_QDMA_DP_DEBUG("%s: block dq(%d)",
		__func__, dq_num);

	dq_num = rte_ring_dequeue_burst(fsl_queue->complete_pool,
			(void **)desc_complete, nb_cpls, NULL);
	for (i = 0; i < dq_num; i++)
		last_idx[i] = desc_complete[i]->flag;

	if (s_hw_err_check) {
		err = dpaa_qdma_err_handle(status +
			FSL_QDMA_ERR_REG_STATUS_OFFSET);
		if (err) {
			if (has_error)
				*has_error = true;
			fsl_queue->stats.errors++;
		}
	}

	return dq_num;
}

static int
dpaa_qdma_stats_get(const struct rte_dma_dev *dmadev,
	uint16_t vchan, struct rte_dma_stats *rte_stats, uint32_t size)
{
	struct fsl_qdma_engine *fsl_qdma = dmadev->data->dev_private;
	struct fsl_qdma_queue *fsl_queue = fsl_qdma->chan[vchan];
	struct rte_dma_stats *stats = &fsl_queue->stats;

	if (size < sizeof(rte_stats))
		return -EINVAL;
	if (rte_stats == NULL)
		return -EINVAL;

	*rte_stats = *stats;

	return 0;
}

static int
dpaa_qdma_stats_reset(struct rte_dma_dev *dmadev, uint16_t vchan)
{
	struct fsl_qdma_engine *fsl_qdma = dmadev->data->dev_private;
	struct fsl_qdma_queue *fsl_queue = fsl_qdma->chan[vchan];

	memset(&fsl_queue->stats, 0, sizeof(struct rte_dma_stats));

	return 0;
}

static uint16_t
dpaa_qdma_burst_capacity(const void *dev_private, uint16_t vchan)
{
	const struct fsl_qdma_engine *fsl_qdma = dev_private;
	struct fsl_qdma_queue *fsl_queue = fsl_qdma->chan[vchan];

	return fsl_queue->pending_max - fsl_queue->pending_num;
}

static struct rte_dma_dev_ops dpaa_qdma_ops = {
	.dev_info_get		  = dpaa_qdma_info_get,
	.dev_configure            = dpaa_qdma_configure,
	.dev_start                = dpaa_qdma_start,
	.dev_close                = dpaa_qdma_close,
	.vchan_setup		  = dpaa_qdma_queue_setup,
	.stats_get		  = dpaa_qdma_stats_get,
	.stats_reset		  = dpaa_qdma_stats_reset,
};

static int
check_devargs_handler(__rte_unused const char *key, const char *value,
		      __rte_unused void *opaque)
{
	if (strcmp(value, "1"))
		return -1;

	return 0;
}

static int
dpaa_get_devargs(struct rte_devargs *devargs, const char *key)
{
	struct rte_kvargs *kvlist;

	if (!devargs)
		return 0;

	kvlist = rte_kvargs_parse(devargs->args, NULL);
	if (!kvlist)
		return 0;

	if (!rte_kvargs_count(kvlist, key)) {
		rte_kvargs_free(kvlist);
		return 0;
	}

	if (rte_kvargs_process(kvlist, key,
			       check_devargs_handler, NULL) < 0) {
		rte_kvargs_free(kvlist);
		return 0;
	}
	rte_kvargs_free(kvlist);

	return 1;
}

static int
dpaa_qdma_init(struct rte_dma_dev *dmadev)
{
	struct fsl_qdma_engine *fsl_qdma = dmadev->data->dev_private;
	uint64_t phys_addr;
	int ccsr_qdma_fd;
	int regs_size;
	int ret;
	uint32_t i, j, k;

	if (dpaa_get_devargs(dmadev->device->devargs, DPAA_DMA_ERROR_CHECK)) {
		s_hw_err_check = true;
		DPAA_QDMA_INFO("Enable DMA error checks");
	}

	fsl_qdma->n_queues = QDMA_QUEUES * QDMA_BLOCKS;
	fsl_qdma->num_blocks = QDMA_BLOCKS;
	fsl_qdma->block_offset = QDMA_BLOCK_OFFSET;

	ccsr_qdma_fd = open("/dev/mem", O_RDWR);
	if (unlikely(ccsr_qdma_fd < 0)) {
		DPAA_QDMA_ERR("Can not open /dev/mem for qdma CCSR map");
		return ccsr_qdma_fd;
	}

	regs_size = fsl_qdma->block_offset * fsl_qdma->num_blocks;
	regs_size += (QDMA_CTRL_REGION_SIZE + QDMA_STATUS_REGION_SIZE);
	phys_addr = QDMA_CCSR_BASE;
	fsl_qdma->reg_base = mmap(NULL, regs_size,
		PROT_READ | PROT_WRITE, MAP_SHARED,
		ccsr_qdma_fd, phys_addr);

	close(ccsr_qdma_fd);
	if (fsl_qdma->reg_base == MAP_FAILED) {
		DPAA_QDMA_ERR("Map qdma reg: Phys(0x%"PRIx64"), size(%d)",
			phys_addr, regs_size);
		return -ENOMEM;
	}

	fsl_qdma->ctrl_base =
		fsl_qdma->reg_base + QDMA_CTRL_REGION_OFFSET;
	fsl_qdma->status_base =
		fsl_qdma->reg_base + QDMA_STATUS_REGION_OFFSET;
	fsl_qdma->block_base =
		fsl_qdma->status_base + QDMA_STATUS_REGION_SIZE;

	for (i = 0; i < QDMA_BLOCKS; i++) {
		ret = fsl_qdma_prep_status_queue(fsl_qdma, i);
		if (ret)
			goto mem_free;
	}

	k = 0;
	for (i = 0; i < QDMA_QUEUES; i++) {
		for (j = 0; j < QDMA_BLOCKS; j++) {
			ret = fsl_qdma_alloc_queue_resources(fsl_qdma, i, j);
			if (ret)
				goto mem_free;
			fsl_qdma->cmd_queues[j][i].channel_id = k;
			k++;
		}
	}

	ret = fsl_qdma_reg_init(fsl_qdma);
	if (ret) {
		DPAA_QDMA_ERR("Can't Initialize the qDMA engine.");
		goto mem_free;
	}

	return 0;

mem_free:
	for (i = 0; i < fsl_qdma->num_blocks; i++)
		fsl_qdma_free_stq_res(&fsl_qdma->stat_queues[i]);

	for (i = 0; i < fsl_qdma->num_blocks; i++) {
		for (j = 0; j < QDMA_QUEUES; j++)
			fsl_qdma_free_cmdq_res(&fsl_qdma->cmd_queues[i][j]);
	}

	munmap(fsl_qdma->ctrl_base, regs_size);

	return ret;
}

static int
dpaa_qdma_probe(__rte_unused struct rte_dpaa_driver *dpaa_drv,
		struct rte_dpaa_device *dpaa_dev)
{
	struct rte_dma_dev *dmadev;
	int ret;

	dmadev = rte_dma_pmd_allocate(dpaa_dev->device.name,
				      rte_socket_id(),
				      sizeof(struct fsl_qdma_engine));
	if (!dmadev) {
		DPAA_QDMA_ERR("Unable to allocate dmadevice");
		return -EINVAL;
	}

	dpaa_dev->dmadev = dmadev;
	dmadev->dev_ops = &dpaa_qdma_ops;
	dmadev->device = &dpaa_dev->device;
	dmadev->fp_obj->dev_private = dmadev->data->dev_private;
	dmadev->fp_obj->copy = dpaa_qdma_enqueue;
	dmadev->fp_obj->copy_sg = dpaa_qdma_copy_sg;
	dmadev->fp_obj->submit = dpaa_qdma_submit;
	dmadev->fp_obj->completed = dpaa_qdma_dequeue;
	dmadev->fp_obj->completed_status = dpaa_qdma_dequeue_status;
	dmadev->fp_obj->burst_capacity = dpaa_qdma_burst_capacity;

	/* Invoke PMD device initialization function */
	ret = dpaa_qdma_init(dmadev);
	if (ret) {
		(void)rte_dma_pmd_release(dpaa_dev->device.name);
		return ret;
	}

	dmadev->state = RTE_DMA_DEV_READY;
	return 0;
}

static int
dpaa_qdma_remove(struct rte_dpaa_device *dpaa_dev)
{
	struct rte_dma_dev *dmadev = dpaa_dev->dmadev;
	struct fsl_qdma_engine *fsl_qdma = dmadev->data->dev_private;
	uint32_t i, j, regs_size;

	regs_size = fsl_qdma->block_offset * fsl_qdma->num_blocks;
	regs_size += (QDMA_CTRL_REGION_SIZE + QDMA_STATUS_REGION_SIZE);

	for (i = 0; i < QDMA_BLOCKS; i++)
		fsl_qdma_free_stq_res(&fsl_qdma->stat_queues[i]);

	for (i = 0; i < QDMA_BLOCKS; i++) {
		for (j = 0; j < QDMA_QUEUES; j++)
			fsl_qdma_free_cmdq_res(&fsl_qdma->cmd_queues[i][j]);
	}

	munmap(fsl_qdma->ctrl_base, regs_size);

	(void)rte_dma_pmd_release(dpaa_dev->device.name);

	return 0;
}

static struct rte_dpaa_driver rte_dpaa_qdma_pmd;

static struct rte_dpaa_driver rte_dpaa_qdma_pmd = {
	.drv_type = FSL_DPAA_QDMA,
	.probe = dpaa_qdma_probe,
	.remove = dpaa_qdma_remove,
};

RTE_PMD_REGISTER_DPAA(dpaa_qdma, rte_dpaa_qdma_pmd);
RTE_PMD_REGISTER_PARAM_STRING(dpaa_qdma, DPAA_DMA_ERROR_CHECK "=<int>");
RTE_LOG_REGISTER_DEFAULT(dpaa_qdma_logtype, INFO);
