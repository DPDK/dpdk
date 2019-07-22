/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include <string.h>
#include <unistd.h>

#include <rte_bus.h>
#include <rte_bus_pci.h>
#include <rte_common.h>
#include <rte_eal.h>
#include <rte_lcore.h>
#include <rte_mempool.h>
#include <rte_pci.h>
#include <rte_rawdev.h>
#include <rte_rawdev_pmd.h>

#include <otx2_common.h>

#include "otx2_dpi_rawdev.h"

static const struct rte_pci_id pci_dma_map[] = {
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_CAVIUM,
			       PCI_DEVID_OCTEONTX2_DPI_VF)
	},
	{
		.vendor_id = 0,
	},
};

/* Enable/Disable DMA queue */
static inline int
dma_engine_enb_dis(struct dpi_vf_s *dpivf, const bool enb)
{
	if (enb)
		otx2_write64(0x1, dpivf->vf_bar0 + DPI_VDMA_EN);
	else
		otx2_write64(0x0, dpivf->vf_bar0 + DPI_VDMA_EN);

	return DPI_DMA_QUEUE_SUCCESS;
}

/* Free DMA Queue instruction buffers, and send close notification to PF */
static inline int
dma_queue_finish(struct dpi_vf_s *dpivf)
{
	uint32_t timeout = 0, sleep = 1;
	uint64_t reg = 0ULL;

	/* Wait for SADDR to become idle */
	reg = otx2_read64(dpivf->vf_bar0 + DPI_VDMA_SADDR);
	while (!(reg & BIT_ULL(DPI_VDMA_SADDR_REQ_IDLE))) {
		rte_delay_ms(sleep);
		timeout++;
		if (timeout >= DPI_QFINISH_TIMEOUT) {
			otx2_dpi_dbg("Timeout!!! Closing Forcibly");
			break;
		}
		reg = otx2_read64(dpivf->vf_bar0 + DPI_VDMA_SADDR);
	}

	if (otx2_dpi_queue_close(dpivf->vf_id) < 0)
		return -EACCES;

	rte_mempool_put(dpivf->chunk_pool, dpivf->base_ptr);
	dpivf->vf_bar0 = (uintptr_t)NULL;

	return DPI_DMA_QUEUE_SUCCESS;
}

/* Write an arbitrary number of command words to a command queue */
static __rte_always_inline enum dpi_dma_queue_result_e
dma_queue_write(struct dpi_vf_s *dpi, uint16_t cmd_count, uint64_t *cmds)
{
	if ((cmd_count < 1) || (cmd_count > 64))
		return DPI_DMA_QUEUE_INVALID_PARAM;

	if (cmds == NULL)
		return DPI_DMA_QUEUE_INVALID_PARAM;

	/* Room available in the current buffer for the command */
	if (dpi->index + cmd_count < dpi->pool_size_m1) {
		uint64_t *ptr = dpi->base_ptr;

		ptr += dpi->index;
		dpi->index += cmd_count;
		while (cmd_count--)
			*ptr++ = *cmds++;
	} else {
		void *new_buffer;
		uint64_t *ptr;
		int count;

		/* Allocate new command buffer, return if failed */
		if (rte_mempool_get(dpi->chunk_pool, &new_buffer) ||
		    new_buffer == NULL) {
			return DPI_DMA_QUEUE_NO_MEMORY;
		}
		ptr = dpi->base_ptr;
		/* Figure out how many command words will fit in this buffer.
		 * One location will be needed for the next buffer pointer.
		 **/
		count = dpi->pool_size_m1 - dpi->index;
		ptr += dpi->index;
		cmd_count -= count;
		while (count--)
			*ptr++ = *cmds++;
		/* Chunk next ptr is 2DWORDs, second DWORD is reserved. */
		*ptr++ = (uint64_t)new_buffer;
		*ptr   = 0;
		/* The current buffer is full and has a link to the next buffer.
		 * Time to write the rest of the commands into the new buffer.
		 **/
		dpi->base_ptr = new_buffer;
		dpi->index = cmd_count;
		ptr = new_buffer;
		while (cmd_count--)
			*ptr++ = *cmds++;
		/* queue index may greater than pool size */
		if (dpi->index >= dpi->pool_size_m1) {
			if (rte_mempool_get(dpi->chunk_pool, &new_buffer) ||
			    new_buffer == NULL) {
				return DPI_DMA_QUEUE_NO_MEMORY;
			}
			/* Write next buffer address */
			*ptr = (uint64_t)new_buffer;
			dpi->base_ptr = new_buffer;
			dpi->index = 0;
		}
	}
	return DPI_DMA_QUEUE_SUCCESS;
}

/* Submit a DMA command to the DMA queues. */
static __rte_always_inline int
dma_queue_submit(struct rte_rawdev *dev, uint16_t cmd_count, uint64_t *cmds)
{
	struct dpi_vf_s *dpivf = dev->dev_private;
	enum dpi_dma_queue_result_e result;

	result = dma_queue_write(dpivf, cmd_count, cmds);
	rte_wmb();
	if (likely(result == DPI_DMA_QUEUE_SUCCESS))
		otx2_write64((uint64_t)cmd_count,
			     dpivf->vf_bar0 + DPI_VDMA_DBELL);

	return result;
}

/* Enqueue buffers to DMA queue
 * returns number of buffers enqueued successfully
 */
static int
otx2_dpi_rawdev_enqueue_bufs(struct rte_rawdev *dev,
			     struct rte_rawdev_buf **buffers,
			     unsigned int count, rte_rawdev_obj_t context)
{
	struct dpi_dma_queue_ctx_s *ctx = (struct dpi_dma_queue_ctx_s *)context;
	struct dpi_dma_buf_ptr_s *cmd;
	uint32_t c = 0;

	for (c = 0; c < count; c++) {
		uint64_t dpi_cmd[DPI_DMA_CMD_SIZE] = {0};
		union dpi_dma_instr_hdr_u *hdr;
		uint16_t index = 0, i;

		hdr = (union dpi_dma_instr_hdr_u *)&dpi_cmd[0];
		cmd = (struct dpi_dma_buf_ptr_s *)buffers[c]->buf_addr;

		hdr->s.xtype = ctx->xtype & DPI_XTYPE_MASK;
		hdr->s.pt = ctx->pt & DPI_HDR_PT_MASK;
		/* Request initiated with byte write completion, but completion
		 * pointer not provided
		 */
		if ((hdr->s.pt == DPI_HDR_PT_ZBW_CA ||
		     hdr->s.pt == DPI_HDR_PT_ZBW_NC) && cmd->comp_ptr == NULL)
			return c;

		cmd->comp_ptr->cdata = DPI_REQ_CDATA;
		hdr->s.ptr = (uint64_t)cmd->comp_ptr;
		hdr->s.deallocv = ctx->deallocv;
		hdr->s.tt = ctx->tt & DPI_W0_TT_MASK;
		hdr->s.grp = ctx->grp & DPI_W0_GRP_MASK;

		/* If caller provides completion ring details, then only queue
		 * completion address for later polling.
		 */
		if (ctx->c_ring) {
			ctx->c_ring->compl_data[ctx->c_ring->tail] =
								 cmd->comp_ptr;
			STRM_INC(ctx->c_ring);
		}

		if (hdr->s.deallocv)
			hdr->s.pvfe = 1;

		if (hdr->s.pt == DPI_HDR_PT_WQP)
			hdr->s.ptr = hdr->s.ptr | DPI_HDR_PT_WQP_STATUSNC;

		index += 4;
		hdr->s.fport = 0;
		hdr->s.lport = 0;

		/* For inbound case, src pointers are last pointers.
		 * For all other cases, src pointers are first pointers.
		 */
		if (ctx->xtype ==  DPI_XTYPE_INBOUND) {
			hdr->s.nfst = cmd->wptr_cnt & DPI_MAX_POINTER;
			hdr->s.nlst = cmd->rptr_cnt & DPI_MAX_POINTER;
			for (i = 0; i < hdr->s.nfst; i++) {
				dpi_cmd[index++] = cmd->wptr[i]->u[0];
				dpi_cmd[index++] = cmd->wptr[i]->u[1];
			}
			for (i = 0; i < hdr->s.nlst; i++) {
				dpi_cmd[index++] = cmd->rptr[i]->u[0];
				dpi_cmd[index++] = cmd->rptr[i]->u[1];
			}
		} else {
			hdr->s.nfst = cmd->rptr_cnt & DPI_MAX_POINTER;
			hdr->s.nlst = cmd->wptr_cnt & DPI_MAX_POINTER;
			for (i = 0; i < hdr->s.nfst; i++) {
				dpi_cmd[index++] = cmd->rptr[i]->u[0];
				dpi_cmd[index++] = cmd->rptr[i]->u[1];
			}
			for (i = 0; i < hdr->s.nlst; i++) {
				dpi_cmd[index++] = cmd->wptr[i]->u[0];
				dpi_cmd[index++] = cmd->wptr[i]->u[1];
			}
		}
		if (dma_queue_submit(dev, index, dpi_cmd))
			return c;
	}
	return c;
}

/* Check for command completion, returns number of commands completed */
static int
otx2_dpi_rawdev_dequeue_bufs(struct rte_rawdev *dev __rte_unused,
			     struct rte_rawdev_buf **buffers,
			     unsigned int count, rte_rawdev_obj_t context)
{
	struct dpi_dma_queue_ctx_s *ctx = (struct dpi_dma_queue_ctx_s *)context;
	unsigned int i = 0, headp;

	/* No completion ring to poll */
	if (ctx->c_ring == NULL)
		return 0;

	headp = ctx->c_ring->head;
	for (i = 0; i < count && (headp != ctx->c_ring->tail); i++) {
		struct dpi_dma_req_compl_s *comp_ptr =
					 ctx->c_ring->compl_data[headp];

		if (comp_ptr->cdata)
			break;

		/* Request Completed */
		buffers[i] = (void *)comp_ptr;
		headp = (headp + 1) % ctx->c_ring->max_cnt;
	}
	ctx->c_ring->head = headp;

	return i;
}

static int
otx2_dpi_rawdev_start(struct rte_rawdev *dev)
{
	dev->started = DPI_QUEUE_START;

	return DPI_DMA_QUEUE_SUCCESS;
}

static void
otx2_dpi_rawdev_stop(struct rte_rawdev *dev)
{
	dev->started = DPI_QUEUE_STOP;
}

static int
otx2_dpi_rawdev_close(struct rte_rawdev *dev)
{
	dma_engine_enb_dis(dev->dev_private, false);
	dma_queue_finish(dev->dev_private);

	return DPI_DMA_QUEUE_SUCCESS;
}

static int
otx2_dpi_rawdev_reset(struct rte_rawdev *dev)
{
	return dev ? DPI_QUEUE_STOP : DPI_QUEUE_START;
}

static int
otx2_dpi_rawdev_configure(const struct rte_rawdev *dev, rte_rawdev_obj_t config)
{
	struct dpi_rawdev_conf_s *conf = config;
	struct dpi_vf_s *dpivf = NULL;
	void *buf = NULL;
	uintptr_t pool;
	uint32_t gaura;

	if (conf == NULL) {
		otx2_dpi_dbg("NULL configuration");
		return -EINVAL;
	}
	dpivf = (struct dpi_vf_s *)dev->dev_private;
	dpivf->chunk_pool = conf->chunk_pool;
	if (rte_mempool_get(conf->chunk_pool, &buf) || (buf == NULL)) {
		otx2_err("Unable allocate buffer");
		return -ENODEV;
	}
	dpivf->base_ptr = buf;
	otx2_write64(0x0, dpivf->vf_bar0 + DPI_VDMA_EN);
	dpivf->pool_size_m1 = (DPI_CHUNK_SIZE >> 3) - 2;
	pool = (uintptr_t)((struct rte_mempool *)conf->chunk_pool)->pool_id;
	gaura = npa_lf_aura_handle_to_aura(pool);
	otx2_write64(0, dpivf->vf_bar0 + DPI_VDMA_REQQ_CTL);
	otx2_write64(((uint64_t)buf >> 7) << 7,
		     dpivf->vf_bar0 + DPI_VDMA_SADDR);
	if (otx2_dpi_queue_open(dpivf->vf_id, DPI_CHUNK_SIZE, gaura) < 0) {
		otx2_err("Unable to open DPI VF %d", dpivf->vf_id);
		rte_mempool_put(conf->chunk_pool, buf);
		return -EACCES;
	}
	dma_engine_enb_dis(dpivf, true);

	return DPI_DMA_QUEUE_SUCCESS;
}

static const struct rte_rawdev_ops dpi_rawdev_ops = {
	.dev_configure = otx2_dpi_rawdev_configure,
	.dev_start = otx2_dpi_rawdev_start,
	.dev_stop = otx2_dpi_rawdev_stop,
	.dev_close = otx2_dpi_rawdev_close,
	.dev_reset = otx2_dpi_rawdev_reset,
	.enqueue_bufs = otx2_dpi_rawdev_enqueue_bufs,
	.dequeue_bufs = otx2_dpi_rawdev_dequeue_bufs,
	.dev_selftest = test_otx2_dma_rawdev,
};

static int
otx2_dpi_rawdev_probe(struct rte_pci_driver *pci_drv __rte_unused,
		      struct rte_pci_device *pci_dev)
{
	char name[RTE_RAWDEV_NAME_MAX_LEN];
	struct dpi_vf_s *dpivf = NULL;
	struct rte_rawdev *rawdev;
	uint16_t vf_id;

	/* For secondary processes, the primary has done all the work */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return DPI_DMA_QUEUE_SUCCESS;

	if (pci_dev->mem_resource[0].addr == NULL) {
		otx2_dpi_dbg("Empty bars %p %p", pci_dev->mem_resource[0].addr,
			     pci_dev->mem_resource[2].addr);
		return -ENODEV;
	}

	memset(name, 0, sizeof(name));
	snprintf(name, RTE_RAWDEV_NAME_MAX_LEN, "DPI:%x:%02x.%x",
		 pci_dev->addr.bus, pci_dev->addr.devid,
		 pci_dev->addr.function);

	/* Allocate device structure */
	rawdev = rte_rawdev_pmd_allocate(name, sizeof(struct dpi_vf_s),
					 rte_socket_id());
	if (rawdev == NULL) {
		otx2_err("Rawdev allocation failed");
		return -EINVAL;
	}

	rawdev->dev_ops = &dpi_rawdev_ops;
	rawdev->device = &pci_dev->device;
	rawdev->driver_name = pci_dev->driver->driver.name;

	dpivf = rawdev->dev_private;
	if (dpivf->state != DPI_QUEUE_STOP) {
		otx2_dpi_dbg("Device already started!!!");
		return -ENODEV;
	}

	vf_id = ((pci_dev->addr.devid & 0x1F) << 3) |
		 (pci_dev->addr.function & 0x7);
	vf_id -= 1;
	dpivf->state = DPI_QUEUE_START;
	dpivf->vf_id = vf_id;
	dpivf->vf_bar0 = (uintptr_t)pci_dev->mem_resource[0].addr;
	dpivf->vf_bar2 = (uintptr_t)pci_dev->mem_resource[2].addr;

	return DPI_DMA_QUEUE_SUCCESS;
}

static int
otx2_dpi_rawdev_remove(struct rte_pci_device *pci_dev)
{
	char name[RTE_RAWDEV_NAME_MAX_LEN];
	struct rte_rawdev *rawdev;
	struct dpi_vf_s *dpivf;

	if (pci_dev == NULL) {
		otx2_dpi_dbg("Invalid pci_dev of the device!");
		return -EINVAL;
	}

	memset(name, 0, sizeof(name));
	snprintf(name, RTE_RAWDEV_NAME_MAX_LEN, "DPI:%x:%02x.%x",
		 pci_dev->addr.bus, pci_dev->addr.devid,
		 pci_dev->addr.function);

	rawdev = rte_rawdev_pmd_get_named_dev(name);
	if (rawdev == NULL) {
		otx2_dpi_dbg("Invalid device name (%s)", name);
		return -EINVAL;
	}

	dpivf = (struct dpi_vf_s *)rawdev->dev_private;
	dma_engine_enb_dis(dpivf, false);
	dma_queue_finish(dpivf);

	/* rte_rawdev_close is called by pmd_release */
	return rte_rawdev_pmd_release(rawdev);
}

static struct rte_pci_driver rte_dpi_rawdev_pmd = {
	.id_table  = pci_dma_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_NEED_IOVA_AS_VA,
	.probe     = otx2_dpi_rawdev_probe,
	.remove    = otx2_dpi_rawdev_remove,
};

RTE_PMD_REGISTER_PCI(dpi_rawdev_pci_driver, rte_dpi_rawdev_pmd);
RTE_PMD_REGISTER_PCI_TABLE(dpi_rawdev_pci_driver, pci_dma_map);
RTE_PMD_REGISTER_KMOD_DEP(dpi_rawdev_pci_driver, "vfio-pci");
