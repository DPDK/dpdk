/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2021 NXP
 */

#include <rte_dpaa_bus.h>
#include <rte_dmadev_pmd.h>

#include "dpaa_qdma.h"
#include "dpaa_qdma_logs.h"

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

static void
dma_release(void *fsl_chan)
{
	((struct fsl_qdma_chan *)fsl_chan)->free = true;
	fsl_qdma_free_chan_resources((struct fsl_qdma_chan *)fsl_chan);
}

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
