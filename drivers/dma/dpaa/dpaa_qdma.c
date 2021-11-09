/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2021 NXP
 */

#include <rte_dpaa_bus.h>
#include <rte_dmadev_pmd.h>

#include "dpaa_qdma.h"
#include "dpaa_qdma_logs.h"

static inline void
qdma_desc_addr_set64(struct fsl_qdma_format *ccdf, u64 addr)
{
	ccdf->addr_hi = upper_32_bits(addr);
	ccdf->addr_lo = rte_cpu_to_le_32(lower_32_bits(addr));
}

static inline u64
qdma_ccdf_get_queue(const struct fsl_qdma_format *ccdf)
{
	return ccdf->cfg8b_w1 & 0xff;
}

static inline int
qdma_ccdf_get_offset(const struct fsl_qdma_format *ccdf)
{
	return (rte_le_to_cpu_32(ccdf->cfg) & QDMA_CCDF_MASK)
		>> QDMA_CCDF_OFFSET;
}

static inline void
qdma_ccdf_set_format(struct fsl_qdma_format *ccdf, int offset)
{
	ccdf->cfg = rte_cpu_to_le_32(QDMA_CCDF_FOTMAT | offset);
}

static inline int
qdma_ccdf_get_status(const struct fsl_qdma_format *ccdf)
{
	return (rte_le_to_cpu_32(ccdf->status) & QDMA_CCDF_MASK)
		>> QDMA_CCDF_STATUS;
}

static inline void
qdma_ccdf_set_ser(struct fsl_qdma_format *ccdf, int status)
{
	ccdf->status = rte_cpu_to_le_32(QDMA_CCDF_SER | status);
}

static inline void
qdma_csgf_set_len(struct fsl_qdma_format *csgf, int len)
{
	csgf->cfg = rte_cpu_to_le_32(len & QDMA_SG_LEN_MASK);
}

static inline void
qdma_csgf_set_f(struct fsl_qdma_format *csgf, int len)
{
	csgf->cfg = rte_cpu_to_le_32(QDMA_SG_FIN | (len & QDMA_SG_LEN_MASK));
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

static void
*dma_pool_alloc(int size, int aligned, dma_addr_t *phy_addr)
{
	void *virt_addr;

	virt_addr = rte_malloc("dma pool alloc", size, aligned);
	if (!virt_addr)
		return NULL;

	*phy_addr = rte_mem_virt2iova(virt_addr);

	return virt_addr;
}

static void
dma_pool_free(void *addr)
{
	rte_free(addr);
}

static void
fsl_qdma_free_chan_resources(struct fsl_qdma_chan *fsl_chan)
{
	struct fsl_qdma_queue *fsl_queue = fsl_chan->queue;
	struct fsl_qdma_engine *fsl_qdma = fsl_chan->qdma;
	struct fsl_qdma_comp *comp_temp, *_comp_temp;
	int id;

	if (--fsl_queue->count)
		goto finally;

	id = (fsl_qdma->block_base - fsl_queue->block_base) /
	      fsl_qdma->block_offset;

	while (rte_atomic32_read(&wait_task[id]) == 1)
		rte_delay_us(QDMA_DELAY);

	list_for_each_entry_safe(comp_temp, _comp_temp,
				 &fsl_queue->comp_used,	list) {
		list_del(&comp_temp->list);
		dma_pool_free(comp_temp->virt_addr);
		dma_pool_free(comp_temp->desc_virt_addr);
		rte_free(comp_temp);
	}

	list_for_each_entry_safe(comp_temp, _comp_temp,
				 &fsl_queue->comp_free, list) {
		list_del(&comp_temp->list);
		dma_pool_free(comp_temp->virt_addr);
		dma_pool_free(comp_temp->desc_virt_addr);
		rte_free(comp_temp);
	}

finally:
	fsl_qdma->desc_allocated--;
}

static void
fsl_qdma_comp_fill_memcpy(struct fsl_qdma_comp *fsl_comp,
				      dma_addr_t dst, dma_addr_t src, u32 len)
{
	struct fsl_qdma_format *csgf_src, *csgf_dest;

	/* Note: command table (fsl_comp->virt_addr) is getting filled
	 * directly in cmd descriptors of queues while enqueuing the descriptor
	 * please refer fsl_qdma_enqueue_desc
	 * frame list table (virt_addr) + 1) and source,
	 * destination descriptor table
	 * (fsl_comp->desc_virt_addr and fsl_comp->desc_virt_addr+1) move to
	 * the control path to fsl_qdma_pre_request_enqueue_comp_sd_desc
	 */
	csgf_src = (struct fsl_qdma_format *)fsl_comp->virt_addr + 2;
	csgf_dest = (struct fsl_qdma_format *)fsl_comp->virt_addr + 3;

	/* Status notification is enqueued to status queue. */
	qdma_desc_addr_set64(csgf_src, src);
	qdma_csgf_set_len(csgf_src, len);
	qdma_desc_addr_set64(csgf_dest, dst);
	qdma_csgf_set_len(csgf_dest, len);
	/* This entry is the last entry. */
	qdma_csgf_set_f(csgf_dest, len);
}

/*
 * Pre-request command descriptor and compound S/G for enqueue.
 */
static int
fsl_qdma_pre_request_enqueue_comp_sd_desc(
					struct fsl_qdma_queue *queue,
					int size, int aligned)
{
	struct fsl_qdma_comp *comp_temp, *_comp_temp;
	struct fsl_qdma_sdf *sdf;
	struct fsl_qdma_ddf *ddf;
	struct fsl_qdma_format *csgf_desc;
	int i;

	for (i = 0; i < (int)(queue->n_cq + COMMAND_QUEUE_OVERFLOW); i++) {
		comp_temp = rte_zmalloc("qdma: comp temp",
					sizeof(*comp_temp), 0);
		if (!comp_temp)
			return -ENOMEM;

		comp_temp->virt_addr =
		dma_pool_alloc(size, aligned, &comp_temp->bus_addr);
		if (!comp_temp->virt_addr) {
			rte_free(comp_temp);
			goto fail;
		}

		comp_temp->desc_virt_addr =
		dma_pool_alloc(size, aligned, &comp_temp->desc_bus_addr);
		if (!comp_temp->desc_virt_addr) {
			rte_free(comp_temp->virt_addr);
			rte_free(comp_temp);
			goto fail;
		}

		memset(comp_temp->virt_addr, 0, FSL_QDMA_COMMAND_BUFFER_SIZE);
		memset(comp_temp->desc_virt_addr, 0,
		       FSL_QDMA_DESCRIPTOR_BUFFER_SIZE);

		csgf_desc = (struct fsl_qdma_format *)comp_temp->virt_addr + 1;
		sdf = (struct fsl_qdma_sdf *)comp_temp->desc_virt_addr;
		ddf = (struct fsl_qdma_ddf *)comp_temp->desc_virt_addr + 1;
		/* Compound Command Descriptor(Frame List Table) */
		qdma_desc_addr_set64(csgf_desc, comp_temp->desc_bus_addr);
		/* It must be 32 as Compound S/G Descriptor */
		qdma_csgf_set_len(csgf_desc, 32);
		/* Descriptor Buffer */
		sdf->cmd = rte_cpu_to_le_32(FSL_QDMA_CMD_RWTTYPE <<
			       FSL_QDMA_CMD_RWTTYPE_OFFSET);
		ddf->cmd = rte_cpu_to_le_32(FSL_QDMA_CMD_RWTTYPE <<
			       FSL_QDMA_CMD_RWTTYPE_OFFSET);
		ddf->cmd |= rte_cpu_to_le_32(FSL_QDMA_CMD_LWC <<
				FSL_QDMA_CMD_LWC_OFFSET);

		list_add_tail(&comp_temp->list, &queue->comp_free);
	}

	return 0;

fail:
	list_for_each_entry_safe(comp_temp, _comp_temp,
				 &queue->comp_free, list) {
		list_del(&comp_temp->list);
		rte_free(comp_temp->virt_addr);
		rte_free(comp_temp->desc_virt_addr);
		rte_free(comp_temp);
	}

	return -ENOMEM;
}

/*
 * Request a command descriptor for enqueue.
 */
static struct fsl_qdma_comp *
fsl_qdma_request_enqueue_desc(struct fsl_qdma_chan *fsl_chan)
{
	struct fsl_qdma_queue *queue = fsl_chan->queue;
	struct fsl_qdma_comp *comp_temp;

	if (!list_empty(&queue->comp_free)) {
		comp_temp = list_first_entry(&queue->comp_free,
					     struct fsl_qdma_comp,
					     list);
		list_del(&comp_temp->list);
		return comp_temp;
	}

	return NULL;
}

static struct fsl_qdma_queue
*fsl_qdma_alloc_queue_resources(struct fsl_qdma_engine *fsl_qdma)
{
	struct fsl_qdma_queue *queue_head, *queue_temp;
	int len, i, j;
	int queue_num;
	int blocks;
	unsigned int queue_size[FSL_QDMA_QUEUE_MAX];

	queue_num = fsl_qdma->n_queues;
	blocks = fsl_qdma->num_blocks;

	len = sizeof(*queue_head) * queue_num * blocks;
	queue_head = rte_zmalloc("qdma: queue head", len, 0);
	if (!queue_head)
		return NULL;

	for (i = 0; i < FSL_QDMA_QUEUE_MAX; i++)
		queue_size[i] = QDMA_QUEUE_SIZE;

	for (j = 0; j < blocks; j++) {
		for (i = 0; i < queue_num; i++) {
			if (queue_size[i] > FSL_QDMA_CIRCULAR_DESC_SIZE_MAX ||
			    queue_size[i] < FSL_QDMA_CIRCULAR_DESC_SIZE_MIN) {
				DPAA_QDMA_ERR("Get wrong queue-sizes.\n");
				goto fail;
			}
			queue_temp = queue_head + i + (j * queue_num);

			queue_temp->cq =
			dma_pool_alloc(sizeof(struct fsl_qdma_format) *
				       queue_size[i],
				       sizeof(struct fsl_qdma_format) *
				       queue_size[i], &queue_temp->bus_addr);

			if (!queue_temp->cq)
				goto fail;

			memset(queue_temp->cq, 0x0, queue_size[i] *
			       sizeof(struct fsl_qdma_format));

			queue_temp->block_base = fsl_qdma->block_base +
				FSL_QDMA_BLOCK_BASE_OFFSET(fsl_qdma, j);
			queue_temp->n_cq = queue_size[i];
			queue_temp->id = i;
			queue_temp->count = 0;
			queue_temp->pending = 0;
			queue_temp->virt_head = queue_temp->cq;
			queue_temp->stats = (struct rte_dma_stats){0};
		}
	}
	return queue_head;

fail:
	for (j = 0; j < blocks; j++) {
		for (i = 0; i < queue_num; i++) {
			queue_temp = queue_head + i + (j * queue_num);
			dma_pool_free(queue_temp->cq);
		}
	}
	rte_free(queue_head);

	return NULL;
}

static struct
fsl_qdma_queue *fsl_qdma_prep_status_queue(void)
{
	struct fsl_qdma_queue *status_head;
	unsigned int status_size;

	status_size = QDMA_STATUS_SIZE;
	if (status_size > FSL_QDMA_CIRCULAR_DESC_SIZE_MAX ||
	    status_size < FSL_QDMA_CIRCULAR_DESC_SIZE_MIN) {
		DPAA_QDMA_ERR("Get wrong status_size.\n");
		return NULL;
	}

	status_head = rte_zmalloc("qdma: status head", sizeof(*status_head), 0);
	if (!status_head)
		return NULL;

	/*
	 * Buffer for queue command
	 */
	status_head->cq = dma_pool_alloc(sizeof(struct fsl_qdma_format) *
					 status_size,
					 sizeof(struct fsl_qdma_format) *
					 status_size,
					 &status_head->bus_addr);

	if (!status_head->cq) {
		rte_free(status_head);
		return NULL;
	}

	memset(status_head->cq, 0x0, status_size *
	       sizeof(struct fsl_qdma_format));
	status_head->n_cq = status_size;
	status_head->virt_head = status_head->cq;

	return status_head;
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

static int
fsl_qdma_queue_transfer_complete(struct fsl_qdma_engine *fsl_qdma,
				 void *block, int id, const uint16_t nb_cpls,
				 uint16_t *last_idx,
				 enum rte_dma_status_code *status)
{
	struct fsl_qdma_queue *fsl_queue = fsl_qdma->queue;
	struct fsl_qdma_queue *fsl_status = fsl_qdma->status[id];
	struct fsl_qdma_queue *temp_queue;
	struct fsl_qdma_format *status_addr;
	struct fsl_qdma_comp *fsl_comp = NULL;
	u32 reg, i;
	int count = 0;

	while (count < nb_cpls) {
		reg = qdma_readl_be(block + FSL_QDMA_BSQSR);
		if (reg & FSL_QDMA_BSQSR_QE_BE)
			return count;

		status_addr = fsl_status->virt_head;

		i = qdma_ccdf_get_queue(status_addr) +
			id * fsl_qdma->n_queues;
		temp_queue = fsl_queue + i;
		fsl_comp = list_first_entry(&temp_queue->comp_used,
					    struct fsl_qdma_comp,
					    list);
		list_del(&fsl_comp->list);

		reg = qdma_readl_be(block + FSL_QDMA_BSQMR);
		reg |= FSL_QDMA_BSQMR_DI_BE;

		qdma_desc_addr_set64(status_addr, 0x0);
		fsl_status->virt_head++;
		if (fsl_status->virt_head == fsl_status->cq + fsl_status->n_cq)
			fsl_status->virt_head = fsl_status->cq;
		qdma_writel_be(reg, block + FSL_QDMA_BSQMR);
		*last_idx = fsl_comp->index;
		if (status != NULL)
			status[count] = RTE_DMA_STATUS_SUCCESSFUL;

		list_add_tail(&fsl_comp->list, &temp_queue->comp_free);
		count++;

	}
	return count;
}

static int
fsl_qdma_reg_init(struct fsl_qdma_engine *fsl_qdma)
{
	struct fsl_qdma_queue *fsl_queue = fsl_qdma->queue;
	struct fsl_qdma_queue *temp;
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
		for (i = 0; i < fsl_qdma->n_queues; i++) {
			temp = fsl_queue + i + (j * fsl_qdma->n_queues);
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
			reg |= FSL_QDMA_BCQMR_CD_THLD(ilog2(temp->n_cq) - 4);
			reg |= FSL_QDMA_BCQMR_CQ_SIZE(ilog2(temp->n_cq) - 6);
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

		qdma_writel(
			    upper_32_bits(fsl_qdma->status[j]->bus_addr),
			    block + FSL_QDMA_SQEEPAR);
		qdma_writel(
			    lower_32_bits(fsl_qdma->status[j]->bus_addr),
			    block + FSL_QDMA_SQEPAR);
		qdma_writel(
			    upper_32_bits(fsl_qdma->status[j]->bus_addr),
			    block + FSL_QDMA_SQEDPAR);
		qdma_writel(
			    lower_32_bits(fsl_qdma->status[j]->bus_addr),
			    block + FSL_QDMA_SQDPAR);
		/* Desiable status queue interrupt. */

		qdma_writel(0x0, block + FSL_QDMA_BCQIER(0));
		qdma_writel(0x0, block + FSL_QDMA_BSQICR);
		qdma_writel(0x0, block + FSL_QDMA_CQIER);

		/* Initialize the status queue mode. */
		reg = FSL_QDMA_BSQMR_EN;
		val = ilog2(fsl_qdma->status[j]->n_cq) - 6;
		reg |= FSL_QDMA_BSQMR_CQ_SIZE(val);
		qdma_writel(reg, block + FSL_QDMA_BSQMR);
	}

	reg = qdma_readl(ctrl + FSL_QDMA_DMR);
	reg &= ~FSL_QDMA_DMR_DQD;
	qdma_writel(reg, ctrl + FSL_QDMA_DMR);

	return 0;
}

static void *
fsl_qdma_prep_memcpy(void *fsl_chan, dma_addr_t dst,
			   dma_addr_t src, size_t len,
			   void *call_back,
			   void *param)
{
	struct fsl_qdma_comp *fsl_comp;

	fsl_comp =
	fsl_qdma_request_enqueue_desc((struct fsl_qdma_chan *)fsl_chan);
	if (!fsl_comp)
		return NULL;

	fsl_comp->qchan = fsl_chan;
	fsl_comp->call_back_func = call_back;
	fsl_comp->params = param;

	fsl_qdma_comp_fill_memcpy(fsl_comp, dst, src, len);
	return (void *)fsl_comp;
}

static int
fsl_qdma_enqueue_desc(struct fsl_qdma_chan *fsl_chan,
				  struct fsl_qdma_comp *fsl_comp,
				  uint64_t flags)
{
	struct fsl_qdma_queue *fsl_queue = fsl_chan->queue;
	void *block = fsl_queue->block_base;
	struct fsl_qdma_format *ccdf;
	u32 reg;

	/* retrieve and store the register value in big endian
	 * to avoid bits swap
	 */
	reg = qdma_readl_be(block +
			 FSL_QDMA_BCQSR(fsl_queue->id));
	if (reg & (FSL_QDMA_BCQSR_QF_XOFF_BE))
		return -1;

	/* filling descriptor  command table */
	ccdf = (struct fsl_qdma_format *)fsl_queue->virt_head;
	qdma_desc_addr_set64(ccdf, fsl_comp->bus_addr + 16);
	qdma_ccdf_set_format(ccdf, qdma_ccdf_get_offset(fsl_comp->virt_addr));
	qdma_ccdf_set_ser(ccdf, qdma_ccdf_get_status(fsl_comp->virt_addr));
	fsl_comp->index = fsl_queue->virt_head - fsl_queue->cq;
	fsl_queue->virt_head++;

	if (fsl_queue->virt_head == fsl_queue->cq + fsl_queue->n_cq)
		fsl_queue->virt_head = fsl_queue->cq;

	list_add_tail(&fsl_comp->list, &fsl_queue->comp_used);

	if (flags == RTE_DMA_OP_FLAG_SUBMIT) {
		reg = qdma_readl_be(block + FSL_QDMA_BCQMR(fsl_queue->id));
		reg |= FSL_QDMA_BCQMR_EI_BE;
		qdma_writel_be(reg, block + FSL_QDMA_BCQMR(fsl_queue->id));
		fsl_queue->stats.submitted++;
	} else {
		fsl_queue->pending++;
	}
	return fsl_comp->index;
}

static int
fsl_qdma_alloc_chan_resources(struct fsl_qdma_chan *fsl_chan)
{
	struct fsl_qdma_queue *fsl_queue = fsl_chan->queue;
	struct fsl_qdma_engine *fsl_qdma = fsl_chan->qdma;
	int ret;

	if (fsl_queue->count++)
		goto finally;

	INIT_LIST_HEAD(&fsl_queue->comp_free);
	INIT_LIST_HEAD(&fsl_queue->comp_used);

	ret = fsl_qdma_pre_request_enqueue_comp_sd_desc(fsl_queue,
				FSL_QDMA_COMMAND_BUFFER_SIZE, 64);
	if (ret) {
		DPAA_QDMA_ERR(
			"failed to alloc dma buffer for comp descriptor\n");
		goto exit;
	}

finally:
	return fsl_qdma->desc_allocated++;

exit:
	return -ENOMEM;
}

static int
dpaa_info_get(const struct rte_dma_dev *dev, struct rte_dma_info *dev_info,
	      uint32_t info_sz)
{
#define DPAADMA_MAX_DESC        64
#define DPAADMA_MIN_DESC        64

	RTE_SET_USED(dev);
	RTE_SET_USED(info_sz);

	dev_info->dev_capa = RTE_DMA_CAPA_MEM_TO_MEM |
			     RTE_DMA_CAPA_MEM_TO_DEV |
			     RTE_DMA_CAPA_DEV_TO_DEV |
			     RTE_DMA_CAPA_DEV_TO_MEM |
			     RTE_DMA_CAPA_SILENT |
			     RTE_DMA_CAPA_OPS_COPY;
	dev_info->max_vchans = 1;
	dev_info->max_desc = DPAADMA_MAX_DESC;
	dev_info->min_desc = DPAADMA_MIN_DESC;

	return 0;
}

static int
dpaa_get_channel(struct fsl_qdma_engine *fsl_qdma,  uint16_t vchan)
{
	u32 i, start, end;
	int ret;

	start = fsl_qdma->free_block_id * QDMA_QUEUES;
	fsl_qdma->free_block_id++;

	end = start + 1;
	for (i = start; i < end; i++) {
		struct fsl_qdma_chan *fsl_chan = &fsl_qdma->chans[i];

		if (fsl_chan->free) {
			fsl_chan->free = false;
			ret = fsl_qdma_alloc_chan_resources(fsl_chan);
			if (ret)
				return ret;

			fsl_qdma->vchan_map[vchan] = i;
			return 0;
		}
	}

	return -1;
}

static void
dma_release(void *fsl_chan)
{
	((struct fsl_qdma_chan *)fsl_chan)->free = true;
	fsl_qdma_free_chan_resources((struct fsl_qdma_chan *)fsl_chan);
}

static int
dpaa_qdma_configure(__rte_unused struct rte_dma_dev *dmadev,
		    __rte_unused const struct rte_dma_conf *dev_conf,
		    __rte_unused uint32_t conf_sz)
{
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
	struct fsl_qdma_engine *fsl_qdma = (struct fsl_qdma_engine *)dev_private;
	struct fsl_qdma_chan *fsl_chan =
		&fsl_qdma->chans[fsl_qdma->vchan_map[vchan]];
	struct fsl_qdma_queue *fsl_queue = fsl_chan->queue;
	void *block = fsl_queue->block_base;
	u32 reg;

	while (fsl_queue->pending) {
		reg = qdma_readl_be(block + FSL_QDMA_BCQMR(fsl_queue->id));
		reg |= FSL_QDMA_BCQMR_EI_BE;
		qdma_writel_be(reg, block + FSL_QDMA_BCQMR(fsl_queue->id));
		fsl_queue->pending--;
		fsl_queue->stats.submitted++;
	}

	return 0;
}

static int
dpaa_qdma_enqueue(void *dev_private, uint16_t vchan,
		  rte_iova_t src, rte_iova_t dst,
		  uint32_t length, uint64_t flags)
{
	struct fsl_qdma_engine *fsl_qdma = (struct fsl_qdma_engine *)dev_private;
	struct fsl_qdma_chan *fsl_chan =
		&fsl_qdma->chans[fsl_qdma->vchan_map[vchan]];
	int ret;

	void *fsl_comp = NULL;

	fsl_comp = fsl_qdma_prep_memcpy(fsl_chan,
			(dma_addr_t)dst, (dma_addr_t)src,
			length, NULL, NULL);
	if (!fsl_comp) {
		DPAA_QDMA_DP_DEBUG("fsl_comp is NULL\n");
		return -1;
	}
	ret = fsl_qdma_enqueue_desc(fsl_chan, fsl_comp, flags);

	return ret;
}

static uint16_t
dpaa_qdma_dequeue_status(void *dev_private, uint16_t vchan,
			 const uint16_t nb_cpls, uint16_t *last_idx,
			 enum rte_dma_status_code *st)
{
	struct fsl_qdma_engine *fsl_qdma = (struct fsl_qdma_engine *)dev_private;
	int id = (int)((fsl_qdma->vchan_map[vchan]) / QDMA_QUEUES);
	void *block;
	int intr;
	void *status = fsl_qdma->status_base;
	struct fsl_qdma_chan *fsl_chan =
		&fsl_qdma->chans[fsl_qdma->vchan_map[vchan]];
	struct fsl_qdma_queue *fsl_queue = fsl_chan->queue;

	intr = qdma_readl_be(status + FSL_QDMA_DEDR);
	if (intr) {
		DPAA_QDMA_ERR("DMA transaction error! %x\n", intr);
		intr = qdma_readl(status + FSL_QDMA_DECFDW0R);
		DPAA_QDMA_INFO("reg FSL_QDMA_DECFDW0R %x\n", intr);
		intr = qdma_readl(status + FSL_QDMA_DECFDW1R);
		DPAA_QDMA_INFO("reg FSL_QDMA_DECFDW1R %x\n", intr);
		intr = qdma_readl(status + FSL_QDMA_DECFDW2R);
		DPAA_QDMA_INFO("reg FSL_QDMA_DECFDW2R %x\n", intr);
		intr = qdma_readl(status + FSL_QDMA_DECFDW3R);
		DPAA_QDMA_INFO("reg FSL_QDMA_DECFDW3R %x\n", intr);
		intr = qdma_readl(status + FSL_QDMA_DECFQIDR);
		DPAA_QDMA_INFO("reg FSL_QDMA_DECFQIDR %x\n", intr);
		intr = qdma_readl(status + FSL_QDMA_DECBR);
		DPAA_QDMA_INFO("reg FSL_QDMA_DECBR %x\n", intr);
		qdma_writel(0xffffffff,
			    status + FSL_QDMA_DEDR);
		intr = qdma_readl(status + FSL_QDMA_DEDR);
		fsl_queue->stats.errors++;
	}

	block = fsl_qdma->block_base +
		FSL_QDMA_BLOCK_BASE_OFFSET(fsl_qdma, id);

	intr = fsl_qdma_queue_transfer_complete(fsl_qdma, block, id, nb_cpls,
						last_idx, st);
	fsl_queue->stats.completed += intr;

	return intr;
}


static uint16_t
dpaa_qdma_dequeue(void *dev_private,
		  uint16_t vchan, const uint16_t nb_cpls,
		  uint16_t *last_idx, bool *has_error)
{
	struct fsl_qdma_engine *fsl_qdma = (struct fsl_qdma_engine *)dev_private;
	int id = (int)((fsl_qdma->vchan_map[vchan]) / QDMA_QUEUES);
	void *block;
	int intr;
	void *status = fsl_qdma->status_base;
	struct fsl_qdma_chan *fsl_chan =
		&fsl_qdma->chans[fsl_qdma->vchan_map[vchan]];
	struct fsl_qdma_queue *fsl_queue = fsl_chan->queue;

	intr = qdma_readl_be(status + FSL_QDMA_DEDR);
	if (intr) {
		DPAA_QDMA_ERR("DMA transaction error! %x\n", intr);
		intr = qdma_readl(status + FSL_QDMA_DECFDW0R);
		DPAA_QDMA_INFO("reg FSL_QDMA_DECFDW0R %x\n", intr);
		intr = qdma_readl(status + FSL_QDMA_DECFDW1R);
		DPAA_QDMA_INFO("reg FSL_QDMA_DECFDW1R %x\n", intr);
		intr = qdma_readl(status + FSL_QDMA_DECFDW2R);
		DPAA_QDMA_INFO("reg FSL_QDMA_DECFDW2R %x\n", intr);
		intr = qdma_readl(status + FSL_QDMA_DECFDW3R);
		DPAA_QDMA_INFO("reg FSL_QDMA_DECFDW3R %x\n", intr);
		intr = qdma_readl(status + FSL_QDMA_DECFQIDR);
		DPAA_QDMA_INFO("reg FSL_QDMA_DECFQIDR %x\n", intr);
		intr = qdma_readl(status + FSL_QDMA_DECBR);
		DPAA_QDMA_INFO("reg FSL_QDMA_DECBR %x\n", intr);
		qdma_writel(0xffffffff,
			    status + FSL_QDMA_DEDR);
		intr = qdma_readl(status + FSL_QDMA_DEDR);
		*has_error = true;
		fsl_queue->stats.errors++;
	}

	block = fsl_qdma->block_base +
		FSL_QDMA_BLOCK_BASE_OFFSET(fsl_qdma, id);

	intr = fsl_qdma_queue_transfer_complete(fsl_qdma, block, id, nb_cpls,
						last_idx, NULL);
	fsl_queue->stats.completed += intr;

	return intr;
}

static int
dpaa_qdma_stats_get(const struct rte_dma_dev *dmadev, uint16_t vchan,
		    struct rte_dma_stats *rte_stats, uint32_t size)
{
	struct fsl_qdma_engine *fsl_qdma = dmadev->data->dev_private;
	struct fsl_qdma_chan *fsl_chan =
		&fsl_qdma->chans[fsl_qdma->vchan_map[vchan]];
	struct fsl_qdma_queue *fsl_queue = fsl_chan->queue;
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
	struct fsl_qdma_chan *fsl_chan =
		&fsl_qdma->chans[fsl_qdma->vchan_map[vchan]];
	struct fsl_qdma_queue *fsl_queue = fsl_chan->queue;

	fsl_queue->stats = (struct rte_dma_stats){0};

	return 0;
}

static struct rte_dma_dev_ops dpaa_qdma_ops = {
	.dev_info_get		  = dpaa_info_get,
	.dev_configure            = dpaa_qdma_configure,
	.dev_start                = dpaa_qdma_start,
	.dev_close                = dpaa_qdma_close,
	.vchan_setup		  = dpaa_qdma_queue_setup,
	.stats_get		  = dpaa_qdma_stats_get,
	.stats_reset		  = dpaa_qdma_stats_reset,
};

static int
dpaa_qdma_init(struct rte_dma_dev *dmadev)
{
	struct fsl_qdma_engine *fsl_qdma = dmadev->data->dev_private;
	struct fsl_qdma_chan *fsl_chan;
	uint64_t phys_addr;
	unsigned int len;
	int ccsr_qdma_fd;
	int regs_size;
	int ret;
	u32 i;

	fsl_qdma->desc_allocated = 0;
	fsl_qdma->n_chans = VIRT_CHANNELS;
	fsl_qdma->n_queues = QDMA_QUEUES;
	fsl_qdma->num_blocks = QDMA_BLOCKS;
	fsl_qdma->block_offset = QDMA_BLOCK_OFFSET;

	len = sizeof(*fsl_chan) * fsl_qdma->n_chans;
	fsl_qdma->chans = rte_zmalloc("qdma: fsl chans", len, 0);
	if (!fsl_qdma->chans)
		return -1;

	len = sizeof(struct fsl_qdma_queue *) * fsl_qdma->num_blocks;
	fsl_qdma->status = rte_zmalloc("qdma: fsl status", len, 0);
	if (!fsl_qdma->status) {
		rte_free(fsl_qdma->chans);
		return -1;
	}

	for (i = 0; i < fsl_qdma->num_blocks; i++) {
		rte_atomic32_init(&wait_task[i]);
		fsl_qdma->status[i] = fsl_qdma_prep_status_queue();
		if (!fsl_qdma->status[i])
			goto err;
	}

	ccsr_qdma_fd = open("/dev/mem", O_RDWR);
	if (unlikely(ccsr_qdma_fd < 0)) {
		DPAA_QDMA_ERR("Can not open /dev/mem for qdma CCSR map");
		goto err;
	}

	regs_size = fsl_qdma->block_offset * (fsl_qdma->num_blocks + 2);
	phys_addr = QDMA_CCSR_BASE;
	fsl_qdma->ctrl_base = mmap(NULL, regs_size, PROT_READ |
					 PROT_WRITE, MAP_SHARED,
					 ccsr_qdma_fd, phys_addr);

	close(ccsr_qdma_fd);
	if (fsl_qdma->ctrl_base == MAP_FAILED) {
		DPAA_QDMA_ERR("Can not map CCSR base qdma: Phys: %08" PRIx64
		       "size %d\n", phys_addr, regs_size);
		goto err;
	}

	fsl_qdma->status_base = fsl_qdma->ctrl_base + QDMA_BLOCK_OFFSET;
	fsl_qdma->block_base = fsl_qdma->status_base + QDMA_BLOCK_OFFSET;

	fsl_qdma->queue = fsl_qdma_alloc_queue_resources(fsl_qdma);
	if (!fsl_qdma->queue) {
		munmap(fsl_qdma->ctrl_base, regs_size);
		goto err;
	}

	for (i = 0; i < fsl_qdma->n_chans; i++) {
		struct fsl_qdma_chan *fsl_chan = &fsl_qdma->chans[i];

		fsl_chan->qdma = fsl_qdma;
		fsl_chan->queue = fsl_qdma->queue + i % (fsl_qdma->n_queues *
							fsl_qdma->num_blocks);
		fsl_chan->free = true;
	}

	ret = fsl_qdma_reg_init(fsl_qdma);
	if (ret) {
		DPAA_QDMA_ERR("Can't Initialize the qDMA engine.\n");
		munmap(fsl_qdma->ctrl_base, regs_size);
		goto err;
	}

	return 0;

err:
	rte_free(fsl_qdma->chans);
	rte_free(fsl_qdma->status);

	return -1;
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
	dmadev->fp_obj->submit = dpaa_qdma_submit;
	dmadev->fp_obj->completed = dpaa_qdma_dequeue;
	dmadev->fp_obj->completed_status = dpaa_qdma_dequeue_status;

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
	int i = 0, max = QDMA_QUEUES * QDMA_BLOCKS;

	for (i = 0; i < max; i++) {
		struct fsl_qdma_chan *fsl_chan = &fsl_qdma->chans[i];

		if (fsl_chan->free == false)
			dma_release(fsl_chan);
	}

	rte_free(fsl_qdma->status);
	rte_free(fsl_qdma->chans);

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
RTE_LOG_REGISTER_DEFAULT(dpaa_qdma_logtype, INFO);
