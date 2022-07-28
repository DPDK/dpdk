/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Intel Corporation
 */

#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <inttypes.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>

#include <rte_eal.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>
#include <rte_io.h>
#include <rte_vfio.h>
#include <bus_pci_driver.h>
#include <bus_ifpga_driver.h>
#include <rte_rawdev.h>

#include "afu_pmd_core.h"
#include "afu_pmd_n3000.h"

static int nlb_afu_config(struct afu_rawdev *dev)
{
	struct n3000_afu_priv *priv = NULL;
	struct rte_pmd_afu_nlb_cfg *cfg = NULL;
	struct nlb_csr_cfg v;

	if (!dev)
		return -EINVAL;

	if (!dev->priv)
		return -ENOENT;

	priv = (struct n3000_afu_priv *)dev->priv;
	cfg = &priv->nlb_cfg;

	v.csr = 0;

	if (cfg->cont)
		v.cont = 1;

	if (cfg->cache_policy == NLB_WRPUSH_I)
		v.wrpush_i = 1;
	else
		v.wrthru_en = cfg->cache_policy;

	if (cfg->cache_hint == NLB_RDLINE_MIXED)
		v.rdsel = 3;
	else
		v.rdsel = cfg->cache_hint;

	v.mode = cfg->mode;
	v.chsel = cfg->read_vc;
	v.wr_chsel = cfg->write_vc;
	v.wrfence_chsel = cfg->wrfence_vc;
	v.wrthru_en = cfg->cache_policy;
	v.multicl_len = cfg->multi_cl - 1;

	IFPGA_RAWDEV_PMD_DEBUG("cfg: 0x%08x", v.csr);
	rte_write32(v.csr, priv->nlb_ctx.addr + CSR_CFG);

	return 0;
}

static void nlb_afu_report(struct afu_rawdev *dev, uint32_t cl)
{
	struct n3000_afu_priv *priv = NULL;
	struct rte_pmd_afu_nlb_cfg *cfg = NULL;
	struct nlb_dsm_status *stat = NULL;
	uint64_t ticks = 0;
	double num, rd_bw, wr_bw;

	if (!dev || !dev->priv)
		return;

	priv = (struct n3000_afu_priv *)dev->priv;

	cfg = &priv->nlb_cfg;
	stat = priv->nlb_ctx.status_ptr;

	if (cfg->cont)
		ticks = stat->num_clocks - stat->start_overhead;
	else
		ticks = stat->num_clocks -
			(stat->start_overhead + stat->end_overhead);

	if (cfg->freq_mhz == 0)
		cfg->freq_mhz = 200;

	num = (double)stat->num_reads;
	rd_bw = (num * CLS_TO_SIZE(1) * MHZ(cfg->freq_mhz)) / ticks;
	num = (double)stat->num_writes;
	wr_bw = (num * CLS_TO_SIZE(1) * MHZ(cfg->freq_mhz)) / ticks;

	printf("Cachelines  Read_Count Write_Count Clocks@%uMHz   "
		"Rd_Bandwidth   Wr_Bandwidth\n", cfg->freq_mhz);
	printf("%10u  %10u %11u  %12"PRIu64"   %7.3f GB/s   %7.3f GB/s\n",
		cl, stat->num_reads, stat->num_writes, ticks,
		rd_bw / 1e9, wr_bw / 1e9);
}

static int nlb_afu_test(struct afu_rawdev *dev)
{
	struct n3000_afu_priv *priv = NULL;
	struct nlb_afu_ctx *ctx = NULL;
	struct rte_pmd_afu_nlb_cfg *cfg = NULL;
	struct nlb_csr_ctl ctl;
	uint32_t *ptr = NULL;
	uint32_t i, j, cl, val = 0;
	uint64_t sval = 0;
	int ret = 0;

	if (!dev)
		return -EINVAL;

	if (!dev->priv)
		return -ENOENT;

	priv = (struct n3000_afu_priv *)dev->priv;
	ctx = &priv->nlb_ctx;
	cfg = &priv->nlb_cfg;

	/* initialize registers */
	IFPGA_RAWDEV_PMD_DEBUG("dsm_addr: 0x%"PRIx64, ctx->dsm_iova);
	rte_write64(ctx->dsm_iova, ctx->addr + CSR_AFU_DSM_BASEL);

	ctl.csr = 0;
	rte_write32(ctl.csr, ctx->addr + CSR_CTL);
	ctl.reset = 1;
	rte_write32(ctl.csr, ctx->addr + CSR_CTL);

	IFPGA_RAWDEV_PMD_DEBUG("src_addr: 0x%"PRIx64, ctx->src_iova);
	rte_write64(SIZE_TO_CLS(ctx->src_iova), ctx->addr + CSR_SRC_ADDR);
	IFPGA_RAWDEV_PMD_DEBUG("dst_addr: 0x%"PRIx64, ctx->dest_iova);
	rte_write64(SIZE_TO_CLS(ctx->dest_iova), ctx->addr + CSR_DST_ADDR);

	ret = nlb_afu_config(dev);
	if (ret)
		return ret;

	/* initialize src data */
	ptr = (uint32_t *)ctx->src_ptr;
	j = CLS_TO_SIZE(cfg->end) >> 2;
	for (i = 0; i < j; i++)
		*ptr++ = i;

	/* start test */
	for (cl = cfg->begin; cl <= cfg->end; cl += cfg->multi_cl) {
		memset(ctx->dest_ptr, 0, CLS_TO_SIZE(cl));
		memset(ctx->dsm_ptr, 0, DSM_SIZE);

		ctl.csr = 0;
		rte_write32(ctl.csr, ctx->addr + CSR_CTL);
		ctl.reset = 1;
		rte_write32(ctl.csr, ctx->addr + CSR_CTL);

		rte_write32(cl, ctx->addr + CSR_NUM_LINES);

		rte_delay_us(10);

		ctl.start = 1;
		rte_write32(ctl.csr, ctx->addr + CSR_CTL);

		if (cfg->cont) {
			rte_delay_ms(cfg->timeout * 1000);
			ctl.force_completion = 1;
			rte_write32(ctl.csr, ctx->addr + CSR_CTL);
			ret = dsm_poll_timeout(&ctx->status_ptr->test_complete,
				val, (val & 0x1) == 1, DSM_POLL_INTERVAL,
				DSM_TIMEOUT);
			if (ret) {
				printf("DSM poll timeout\n");
				goto end;
			}
		} else {
			ret = dsm_poll_timeout(&ctx->status_ptr->test_complete,
				val, (val & 0x1) == 1, DSM_POLL_INTERVAL,
				DSM_TIMEOUT);
			if (ret) {
				printf("DSM poll timeout\n");
				goto end;
			}
			ctl.force_completion = 1;
			rte_write32(ctl.csr, ctx->addr + CSR_CTL);
		}

		nlb_afu_report(dev, cl);

		i = 0;
		while (i++ < 100) {
			sval = rte_read64(ctx->addr + CSR_STATUS1);
			if (sval == 0)
				break;
			rte_delay_us(1000);
		}

		ptr = (uint32_t *)ctx->dest_ptr;
		j = CLS_TO_SIZE(cl) >> 2;
		for (i = 0; i < j; i++) {
			if (*ptr++ != i) {
				IFPGA_RAWDEV_PMD_ERR("Data mismatch @ %u", i);
				break;
			}
		}
	}

end:
	return ret;
}

static void dma_afu_buf_free(struct dma_afu_ctx *ctx)
{
	int i = 0;

	if (!ctx)
		return;

	for (i = 0; i < NUM_DMA_BUF; i++) {
		rte_free(ctx->dma_buf[i]);
		ctx->dma_buf[i] = NULL;
	}

	rte_free(ctx->data_buf);
	ctx->data_buf = NULL;

	rte_free(ctx->ref_buf);
	ctx->ref_buf = NULL;
}

static int dma_afu_buf_alloc(struct dma_afu_ctx *ctx,
	struct rte_pmd_afu_dma_cfg *cfg)
{
	size_t page_sz = sysconf(_SC_PAGE_SIZE);
	int i, ret = 0;

	if (!ctx || !cfg)
		return -EINVAL;

	for (i = 0; i < NUM_DMA_BUF; i++) {
		ctx->dma_buf[i] = (uint64_t *)rte_zmalloc(NULL, cfg->size,
			TEST_MEM_ALIGN);
		if (!ctx->dma_buf[i]) {
			ret = -ENOMEM;
			goto free_dma_buf;
		}
		ctx->dma_iova[i] = rte_malloc_virt2iova(ctx->dma_buf[i]);
		if (ctx->dma_iova[i] == RTE_BAD_IOVA) {
			ret = -ENOMEM;
			goto free_dma_buf;
		}
	}

	ctx->data_buf = rte_malloc(NULL, cfg->length, page_sz);
	if (!ctx->data_buf) {
		ret = -ENOMEM;
		goto free_dma_buf;
	}

	ctx->ref_buf = rte_malloc(NULL, cfg->length, page_sz);
	if (!ctx->ref_buf) {
		ret = -ENOMEM;
		goto free_data_buf;
	}

	return 0;

free_data_buf:
	rte_free(ctx->data_buf);
	ctx->data_buf = NULL;
free_dma_buf:
	for (i = 0; i < NUM_DMA_BUF; i++) {
		rte_free(ctx->dma_buf[i]);
		ctx->dma_buf[i] = NULL;
	}
	return ret;
}

static void dma_afu_buf_init(struct dma_afu_ctx *ctx, size_t size)
{
	int *ptr = NULL;
	size_t i = 0;
	size_t dword_size = 0;

	if (!ctx || !size)
		return;

	ptr = (int *)ctx->ref_buf;

	if (ctx->pattern) {
		memset(ptr, ctx->pattern, size);
	} else {
		srand(99);
		dword_size = size >> 2;
		for (i = 0; i < dword_size; i++)
			*ptr++ = rand();
	}
	rte_memcpy(ctx->data_buf, ctx->ref_buf, size);
}

static int dma_afu_buf_verify(struct dma_afu_ctx *ctx, size_t size)
{
	uint8_t *src = NULL;
	uint8_t *dst = NULL;
	size_t i = 0;
	int n = 0;

	if (!ctx || !size)
		return -EINVAL;

	src = (uint8_t *)ctx->ref_buf;
	dst = (uint8_t *)ctx->data_buf;

	if (memcmp(src, dst, size)) {
		printf("Transfer is corrupted\n");
		if (ctx->verbose) {
			for (i = 0; i < size; i++) {
				if (*src != *dst) {
					if (++n >= ERR_CHECK_LIMIT)
						break;
					printf("Mismatch at 0x%zx, "
						"Expected %02x  Actual %02x\n",
						i, *src, *dst);
				}
				src++;
				dst++;
			}
			if (n < ERR_CHECK_LIMIT) {
				printf("Found %d error bytes\n", n);
			} else {
				printf("......\n");
				printf("Found more than %d error bytes\n", n);
			}
		}
		return -1;
	}

	printf("Transfer is verified\n");
	return 0;
}

static void blk_write64(uint64_t *dev_addr, uint64_t *host_addr, uint64_t bytes)
{
	uint64_t qwords = bytes / sizeof(uint64_t);

	if (!IS_ALIGNED_QWORD((uint64_t)dev_addr) ||
		!IS_ALIGNED_QWORD((uint64_t)bytes))
		return;

	for (; qwords > 0; qwords--, host_addr++, dev_addr++)
		rte_write64(*host_addr, dev_addr);
}

static void blk_read64(uint64_t *dev_addr, uint64_t *host_addr, uint64_t bytes)
{
	uint64_t qwords = bytes / sizeof(uint64_t);

	if (!IS_ALIGNED_QWORD((uint64_t)dev_addr) ||
		!IS_ALIGNED_QWORD((uint64_t)bytes))
		return;

	for (; qwords > 0; qwords--, host_addr++, dev_addr++)
		*host_addr = rte_read64(dev_addr);
}

static void switch_ase_page(struct dma_afu_ctx *ctx, uint64_t addr)
{
	uint64_t requested_page = addr & ~DMA_ASE_WINDOW_MASK;

	if (!ctx)
		return;

	if (requested_page != ctx->cur_ase_page) {
		rte_write64(requested_page, ctx->ase_ctrl_addr);
		ctx->cur_ase_page = requested_page;
	}
}

static int ase_write_unaligned(struct dma_afu_ctx *ctx, uint64_t dev_addr,
	uint64_t host_addr, uint32_t count)
{
	uint64_t dev_aligned_addr = 0;
	uint64_t shift = 0;
	uint64_t val = 0;
	uintptr_t addr = (uintptr_t)host_addr;  /* transfer to pointer size */

	IFPGA_RAWDEV_PMD_DEBUG("0x%"PRIx64" --> 0x%"PRIx64" (0x%x)", host_addr,
		dev_addr, count);

	if (!ctx || (count >= QWORD_BYTES))
		return -EINVAL;

	if (!count)
		return 0;

	switch_ase_page(ctx, dev_addr);

	shift = dev_addr % QWORD_BYTES;
	dev_aligned_addr = (dev_addr - shift) & DMA_ASE_WINDOW_MASK;
	val = rte_read64(ctx->ase_data_addr + dev_aligned_addr);
	rte_memcpy(((char *)(&val)) + shift, (void *)addr, count);

	/* write back to device */
	rte_write64(val, ctx->ase_data_addr + dev_aligned_addr);

	return 0;
}

static int ase_write(struct dma_afu_ctx *ctx, uint64_t *dst_ptr,
	uint64_t *src_ptr, uint64_t *count)
{
	uint64_t src = *src_ptr;
	uint64_t dst = *dst_ptr;
	uint64_t align_bytes = *count;
	uint64_t offset = 0;
	uint64_t left_in_page = DMA_ASE_WINDOW;
	uint64_t size_to_copy = 0;

	IFPGA_RAWDEV_PMD_DEBUG("0x%"PRIx64" --> 0x%"PRIx64" (0x%"PRIx64")", src, dst,
		align_bytes);

	if (!ctx || !IS_ALIGNED_DWORD(dst))
		return -EINVAL;

	if (align_bytes < DWORD_BYTES)
		return 0;

	if (!IS_ALIGNED_QWORD(dst)) {
		/* Write out a single DWORD to get QWORD aligned */
		switch_ase_page(ctx, dst);
		offset = dst & DMA_ASE_WINDOW_MASK;

		rte_write32(*(uint32_t *)(uintptr_t)src,
			ctx->ase_data_addr + offset);
		src += DWORD_BYTES;
		dst += DWORD_BYTES;
		align_bytes -= DWORD_BYTES;
	}

	if (!align_bytes)
		return 0;

	/* Write out blocks of 64-bit values */
	while (align_bytes >= QWORD_BYTES) {
		left_in_page -= dst & DMA_ASE_WINDOW_MASK;
		size_to_copy =
			MIN(left_in_page, (align_bytes & ~(QWORD_BYTES - 1)));
		if (size_to_copy < QWORD_BYTES)
			break;
		switch_ase_page(ctx, dst);
		offset = dst & DMA_ASE_WINDOW_MASK;
		blk_write64((uint64_t *)(ctx->ase_data_addr + offset),
			(uint64_t *)(uintptr_t)src, size_to_copy);
		src += size_to_copy;
		dst += size_to_copy;
		align_bytes -= size_to_copy;
	}

	if (align_bytes >= DWORD_BYTES) {
		/* Write out remaining DWORD */
		switch_ase_page(ctx, dst);
		offset = dst & DMA_ASE_WINDOW_MASK;
		rte_write32(*(uint32_t *)(uintptr_t)src,
			ctx->ase_data_addr + offset);
		src += DWORD_BYTES;
		dst += DWORD_BYTES;
		align_bytes -= DWORD_BYTES;
	}

	*src_ptr = src;
	*dst_ptr = dst;
	*count = align_bytes;

	return 0;
}

static int ase_host_to_fpga(struct dma_afu_ctx *ctx, uint64_t *dst_ptr,
	uint64_t *src_ptr, uint64_t count)
{
	uint64_t dst = *dst_ptr;
	uint64_t src = *src_ptr;
	uint64_t count_left = count;
	uint64_t unaligned_size = 0;
	int ret = 0;

	IFPGA_RAWDEV_PMD_DEBUG("0x%"PRIx64" --> 0x%"PRIx64" (0x%"PRIx64")", src, dst,
		count);

	/* aligns address to 8 byte using dst masking method */
	if (!IS_ALIGNED_DWORD(dst) && !IS_ALIGNED_QWORD(dst)) {
		unaligned_size = QWORD_BYTES - (dst % QWORD_BYTES);
		if (unaligned_size > count_left)
			unaligned_size = count_left;
		ret = ase_write_unaligned(ctx, dst, src, unaligned_size);
		if (ret)
			return ret;
		count_left -= unaligned_size;
		src += unaligned_size;
		dst += unaligned_size;
	}

	/* Handles 8/4 byte MMIO transfer */
	ret = ase_write(ctx, &dst, &src, &count_left);
	if (ret)
		return ret;

	/* Left over unaligned bytes transferred using dst masking method */
	unaligned_size = QWORD_BYTES - (dst % QWORD_BYTES);
	if (unaligned_size > count_left)
		unaligned_size = count_left;

	ret = ase_write_unaligned(ctx, dst, src, unaligned_size);
	if (ret)
		return ret;

	count_left -= unaligned_size;
	*dst_ptr = dst + unaligned_size;
	*src_ptr = src + unaligned_size;

	return 0;
}

static int ase_read_unaligned(struct dma_afu_ctx *ctx, uint64_t dev_addr,
	uint64_t host_addr, uint32_t count)
{
	uint64_t dev_aligned_addr = 0;
	uint64_t shift = 0;
	uint64_t val = 0;
	uintptr_t addr = (uintptr_t)host_addr;  /* transfer to pointer size */

	IFPGA_RAWDEV_PMD_DEBUG("0x%"PRIx64" <-- 0x%"PRIx64" (0x%x)", host_addr,
		dev_addr, count);

	if (!ctx || (count >= QWORD_BYTES))
		return -EINVAL;

	if (!count)
		return 0;

	switch_ase_page(ctx, dev_addr);

	shift = dev_addr % QWORD_BYTES;
	dev_aligned_addr = (dev_addr - shift) & DMA_ASE_WINDOW_MASK;
	val = rte_read64(ctx->ase_data_addr + dev_aligned_addr);
	rte_memcpy((void *)addr, ((char *)(&val)) + shift, count);

	return 0;
}

static int ase_read(struct dma_afu_ctx *ctx, uint64_t *src_ptr,
	uint64_t *dst_ptr, uint64_t *count)
{
	uint64_t src = *src_ptr;
	uint64_t dst = *dst_ptr;
	uint64_t align_bytes = *count;
	uint64_t offset = 0;
	uint64_t left_in_page = DMA_ASE_WINDOW;
	uint64_t size_to_copy = 0;

	IFPGA_RAWDEV_PMD_DEBUG("0x%"PRIx64" <-- 0x%"PRIx64" (0x%"PRIx64")", dst, src,
		align_bytes);

	if (!ctx || !IS_ALIGNED_DWORD(src))
		return -EINVAL;

	if (align_bytes < DWORD_BYTES)
		return 0;

	if (!IS_ALIGNED_QWORD(src)) {
		/* Read a single DWORD to get QWORD aligned */
		switch_ase_page(ctx, src);
		offset = src & DMA_ASE_WINDOW_MASK;
		*(uint32_t *)(uintptr_t)dst =
			rte_read32(ctx->ase_data_addr + offset);
		src += DWORD_BYTES;
		dst += DWORD_BYTES;
		align_bytes -= DWORD_BYTES;
	}

	if (!align_bytes)
		return 0;

	/* Read blocks of 64-bit values */
	while (align_bytes >= QWORD_BYTES) {
		left_in_page -= src & DMA_ASE_WINDOW_MASK;
		size_to_copy =
			MIN(left_in_page, (align_bytes & ~(QWORD_BYTES - 1)));
		if (size_to_copy < QWORD_BYTES)
			break;
		switch_ase_page(ctx, src);
		offset = src & DMA_ASE_WINDOW_MASK;
		blk_read64((uint64_t *)(ctx->ase_data_addr + offset),
			(uint64_t *)(uintptr_t)dst, size_to_copy);
		src += size_to_copy;
		dst += size_to_copy;
		align_bytes -= size_to_copy;
	}

	if (align_bytes >= DWORD_BYTES) {
		/* Read remaining DWORD */
		switch_ase_page(ctx, src);
		offset = src & DMA_ASE_WINDOW_MASK;
		*(uint32_t *)(uintptr_t)dst =
			rte_read32(ctx->ase_data_addr + offset);
		src += DWORD_BYTES;
		dst += DWORD_BYTES;
		align_bytes -= DWORD_BYTES;
	}

	*src_ptr = src;
	*dst_ptr = dst;
	*count = align_bytes;

	return 0;
}

static int ase_fpga_to_host(struct dma_afu_ctx *ctx, uint64_t *src_ptr,
	uint64_t *dst_ptr, uint64_t count)
{
	uint64_t src = *src_ptr;
	uint64_t dst = *dst_ptr;
	uint64_t count_left = count;
	uint64_t unaligned_size = 0;
	int ret = 0;

	IFPGA_RAWDEV_PMD_DEBUG("0x%"PRIx64" --> 0x%"PRIx64" (0x%"PRIx64")", src, dst,
		count);

	/* Aligns address to 8 byte using src masking method */
	if (!IS_ALIGNED_DWORD(src) && !IS_ALIGNED_QWORD(src)) {
		unaligned_size = QWORD_BYTES - (src % QWORD_BYTES);
		if (unaligned_size > count_left)
			unaligned_size = count_left;
		ret = ase_read_unaligned(ctx, src, dst, unaligned_size);
		if (ret)
			return ret;
		count_left -= unaligned_size;
		dst += unaligned_size;
		src += unaligned_size;
	}

	/* Handles 8/4 byte MMIO transfer */
	ret = ase_read(ctx, &src, &dst, &count_left);
	if (ret)
		return ret;

	/* Left over unaligned bytes transferred using src masking method */
	unaligned_size = QWORD_BYTES - (src % QWORD_BYTES);
	if (unaligned_size > count_left)
		unaligned_size = count_left;

	ret = ase_read_unaligned(ctx, src, dst, unaligned_size);
	if (ret)
		return ret;

	count_left -= unaligned_size;
	*dst_ptr = dst + unaligned_size;
	*src_ptr = src + unaligned_size;

	return 0;
}

static void clear_interrupt(struct dma_afu_ctx *ctx)
{
	/* clear interrupt by writing 1 to IRQ bit in status register */
	msgdma_status status;

	if (!ctx)
		return;

	status.csr = 0;
	status.irq = 1;
	rte_write32(status.csr, CSR_STATUS(ctx->csr_addr));
}

static int poll_interrupt(struct dma_afu_ctx *ctx)
{
	struct pollfd pfd = {0};
	uint64_t count = 0;
	ssize_t bytes_read = 0;
	int poll_ret = 0;
	int ret = 0;

	if (!ctx || (ctx->event_fd < 0))
		return -EINVAL;

	pfd.fd = ctx->event_fd;
	pfd.events = POLLIN;
	poll_ret = poll(&pfd, 1, DMA_TIMEOUT_MSEC);
	if (poll_ret < 0) {
		IFPGA_RAWDEV_PMD_ERR("Error %s", strerror(errno));
		ret = -EFAULT;
		goto out;
	} else if (poll_ret == 0) {
		IFPGA_RAWDEV_PMD_ERR("Timeout");
		ret = -ETIMEDOUT;
	} else {
		bytes_read = read(pfd.fd, &count, sizeof(count));
		if (bytes_read > 0) {
			if (ctx->verbose)
				IFPGA_RAWDEV_PMD_DEBUG("Successful, ret %d, cnt %"PRIu64,
					poll_ret, count);
			ret = 0;
		} else {
			IFPGA_RAWDEV_PMD_ERR("Failed %s", bytes_read > 0 ?
				strerror(errno) : "zero bytes read");
			ret = -EIO;
		}
	}
out:
	clear_interrupt(ctx);
	return ret;
}

static void send_descriptor(struct dma_afu_ctx *ctx, msgdma_ext_desc *desc)
{
	msgdma_status status;
	uint64_t fpga_queue_full = 0;

	if (!ctx)
		return;

	if (ctx->verbose) {
		IFPGA_RAWDEV_PMD_DEBUG("descriptor.rd_address = 0x%x%08x",
			desc->rd_address_ext, desc->rd_address);
		IFPGA_RAWDEV_PMD_DEBUG("descriptor.wr_address = 0x%x%08x",
			desc->wr_address_ext, desc->wr_address);
		IFPGA_RAWDEV_PMD_DEBUG("descriptor.len = %u", desc->len);
		IFPGA_RAWDEV_PMD_DEBUG("descriptor.wr_burst_count = %u",
			desc->wr_burst_count);
		IFPGA_RAWDEV_PMD_DEBUG("descriptor.rd_burst_count = %u",
			desc->rd_burst_count);
		IFPGA_RAWDEV_PMD_DEBUG("descriptor.wr_stride %u", desc->wr_stride);
		IFPGA_RAWDEV_PMD_DEBUG("descriptor.rd_stride %u", desc->rd_stride);
	}

	do {
		status.csr = rte_read32(CSR_STATUS(ctx->csr_addr));
		if (fpga_queue_full++ > 100000000) {
			IFPGA_RAWDEV_PMD_DEBUG("DMA queue full retry");
			fpga_queue_full = 0;
		}
	} while (status.desc_buf_full);

	blk_write64((uint64_t *)ctx->desc_addr, (uint64_t *)desc,
		sizeof(*desc));
}

static int do_dma(struct dma_afu_ctx *ctx, uint64_t dst, uint64_t src,
	int count, int is_last_desc, fpga_dma_type type, int intr_en)
{
	msgdma_ext_desc *desc = NULL;
	int alignment_offset = 0;
	int segment_size = 0;

	if (!ctx)
		return -EINVAL;

	/* src, dst and count must be 64-byte aligned */
	if (!IS_DMA_ALIGNED(src) || !IS_DMA_ALIGNED(dst) ||
		!IS_DMA_ALIGNED(count))
		return -EINVAL;
	memset(ctx->desc_buf, 0, sizeof(msgdma_ext_desc));

	/* these fields are fixed for all DMA transfers */
	desc = ctx->desc_buf;
	desc->seq_num = 0;
	desc->wr_stride = 1;
	desc->rd_stride = 1;
	desc->control.go = 1;
	if (intr_en)
		desc->control.transfer_irq_en = 1;
	else
		desc->control.transfer_irq_en = 0;

	if (!is_last_desc)
		desc->control.early_done_en = 1;
	else
		desc->control.early_done_en = 0;

	if (type == FPGA_TO_FPGA) {
		desc->rd_address = src & DMA_MASK_32_BIT;
		desc->wr_address = dst & DMA_MASK_32_BIT;
		desc->len = count;
		desc->wr_burst_count = 4;
		desc->rd_burst_count = 4;
		desc->rd_address_ext = (src >> 32) & DMA_MASK_32_BIT;
		desc->wr_address_ext = (dst >> 32) & DMA_MASK_32_BIT;
		send_descriptor(ctx, desc);
	} else {
		/* check CCIP (host) address is aligned to 4CL (256B) */
		alignment_offset = (type == HOST_TO_FPGA)
			? (src % CCIP_ALIGN_BYTES) : (dst % CCIP_ALIGN_BYTES);
		/* performing a short transfer to get aligned */
		if (alignment_offset != 0) {
			desc->rd_address = src & DMA_MASK_32_BIT;
			desc->wr_address = dst & DMA_MASK_32_BIT;
			desc->wr_burst_count = 1;
			desc->rd_burst_count = 1;
			desc->rd_address_ext = (src >> 32) & DMA_MASK_32_BIT;
			desc->wr_address_ext = (dst >> 32) & DMA_MASK_32_BIT;
			/* count isn't large enough to hit next 4CL boundary */
			if ((CCIP_ALIGN_BYTES - alignment_offset) >= count) {
				segment_size = count;
				count = 0;
			} else {
				segment_size = CCIP_ALIGN_BYTES
					- alignment_offset;
				src += segment_size;
				dst += segment_size;
				count -= segment_size;
				desc->control.transfer_irq_en = 0;
			}
			/* post short transfer to align to a 4CL (256 byte) */
			desc->len = segment_size;
			send_descriptor(ctx, desc);
		}
		/* at this point we are 4CL (256 byte) aligned */
		if (count >= CCIP_ALIGN_BYTES) {
			desc->rd_address = src & DMA_MASK_32_BIT;
			desc->wr_address = dst & DMA_MASK_32_BIT;
			desc->wr_burst_count = 4;
			desc->rd_burst_count = 4;
			desc->rd_address_ext = (src >> 32) & DMA_MASK_32_BIT;
			desc->wr_address_ext = (dst >> 32) & DMA_MASK_32_BIT;
			/* buffer ends on 4CL boundary */
			if ((count % CCIP_ALIGN_BYTES) == 0) {
				segment_size = count;
				count = 0;
			} else {
				segment_size = count
					- (count % CCIP_ALIGN_BYTES);
				src += segment_size;
				dst += segment_size;
				count -= segment_size;
				desc->control.transfer_irq_en = 0;
			}
			desc->len = segment_size;
			send_descriptor(ctx, desc);
		}
		/* post short transfer to handle the remainder */
		if (count > 0) {
			desc->rd_address = src & DMA_MASK_32_BIT;
			desc->wr_address = dst & DMA_MASK_32_BIT;
			desc->len = count;
			desc->wr_burst_count = 1;
			desc->rd_burst_count = 1;
			desc->rd_address_ext = (src >> 32) & DMA_MASK_32_BIT;
			desc->wr_address_ext = (dst >> 32) & DMA_MASK_32_BIT;
			if (intr_en)
				desc->control.transfer_irq_en = 1;
			send_descriptor(ctx, desc);
		}
	}

	return 0;
}

static int issue_magic(struct dma_afu_ctx *ctx)
{
	*(ctx->magic_buf) = 0ULL;
	return do_dma(ctx, DMA_WF_HOST_ADDR(ctx->magic_iova),
		DMA_WF_MAGIC_ROM, 64, 1, FPGA_TO_HOST, 1);
}

static void wait_magic(struct dma_afu_ctx *ctx)
{
	int magic_timeout = 0;

	if (!ctx)
		return;

	poll_interrupt(ctx);
	while (*(ctx->magic_buf) != DMA_WF_MAGIC) {
		if (magic_timeout++ > 1000) {
			IFPGA_RAWDEV_PMD_ERR("DMA magic operation timeout");
			magic_timeout = 0;
			break;
		}
	}
	*(ctx->magic_buf) = 0ULL;
}

static int dma_tx_buf(struct dma_afu_ctx *ctx, uint64_t dst, uint64_t src,
	uint64_t chunk, int is_last_chunk, int *intr_issued)
{
	int intr_en = 0;
	int ret = 0;

	if (!ctx || !intr_issued)
		return -EINVAL;

	src += chunk * ctx->dma_buf_size;
	dst += chunk * ctx->dma_buf_size;

	if (((chunk % HALF_DMA_BUF) == (HALF_DMA_BUF - 1)) || is_last_chunk) {
		if (*intr_issued) {
			ret = poll_interrupt(ctx);
			if (ret)
				return ret;
		}
		intr_en = 1;
	}

	chunk %= NUM_DMA_BUF;
	rte_memcpy(ctx->dma_buf[chunk], (void *)(uintptr_t)src,
		ctx->dma_buf_size);
	ret = do_dma(ctx, dst, DMA_HOST_ADDR(ctx->dma_iova[chunk]),
			ctx->dma_buf_size, 0, HOST_TO_FPGA, intr_en);
	if (intr_en)
		*intr_issued = 1;

	return ret;
}

static int dma_host_to_fpga(struct dma_afu_ctx *ctx, uint64_t dst, uint64_t src,
	size_t count)
{
	uint64_t i = 0;
	uint64_t count_left = count;
	uint64_t aligned_addr = 0;
	uint64_t align_bytes = 0;
	uint64_t dma_chunks = 0;
	uint64_t dma_tx_bytes = 0;
	uint64_t offset = 0;
	int issued_intr = 0;
	int ret = 0;

	IFPGA_RAWDEV_PMD_DEBUG("0x%"PRIx64" ---> 0x%"PRIx64" (%zu)", src, dst,
		count);

	if (!ctx)
		return -EINVAL;

	if (!IS_DMA_ALIGNED(dst)) {
		if (count_left < DMA_ALIGN_BYTES)
			return ase_host_to_fpga(ctx, &dst, &src, count_left);

		aligned_addr = ((dst / DMA_ALIGN_BYTES) + 1)
			* DMA_ALIGN_BYTES;
		align_bytes = aligned_addr - dst;
		ret = ase_host_to_fpga(ctx, &dst, &src, align_bytes);
		if (ret)
			return ret;
		count_left = count_left - align_bytes;
	}

	if (count_left) {
		dma_chunks = count_left / ctx->dma_buf_size;
		offset = dma_chunks * ctx->dma_buf_size;
		count_left -= offset;
		IFPGA_RAWDEV_PMD_DEBUG("0x%"PRIx64" ---> 0x%"PRIx64
			" (%"PRIu64"...0x%"PRIx64")",
			src, dst, dma_chunks, count_left);
		for (i = 0; i < dma_chunks; i++) {
			ret = dma_tx_buf(ctx, dst, src, i,
				i == (dma_chunks - 1), &issued_intr);
			if (ret)
				return ret;
		}

		if (issued_intr) {
			ret = poll_interrupt(ctx);
			if (ret)
				return ret;
		}

		if (count_left) {
			i = count_left / DMA_ALIGN_BYTES;
			if (i > 0) {
				dma_tx_bytes = i * DMA_ALIGN_BYTES;
				IFPGA_RAWDEV_PMD_DEBUG("left over 0x%"PRIx64" to DMA",
					dma_tx_bytes);
				rte_memcpy(ctx->dma_buf[0],
					(void *)(uintptr_t)(src + offset),
					dma_tx_bytes);
				ret = do_dma(ctx, dst + offset,
					DMA_HOST_ADDR(ctx->dma_iova[0]),
					dma_tx_bytes, 1, HOST_TO_FPGA, 1);
				if (ret)
					return ret;
				ret = poll_interrupt(ctx);
				if (ret)
					return ret;
			}

			count_left -= dma_tx_bytes;
			if (count_left) {
				IFPGA_RAWDEV_PMD_DEBUG("left over 0x%"PRIx64" to ASE",
					count_left);
				dst += offset + dma_tx_bytes;
				src += offset + dma_tx_bytes;
				ret = ase_host_to_fpga(ctx, &dst, &src,
					count_left);
			}
		}
	}

	return ret;
}

static int dma_rx_buf(struct dma_afu_ctx *ctx, uint64_t dst, uint64_t src,
	uint64_t chunk, int is_last_chunk, uint64_t *rx_count, int *wf_issued)
{
	uint64_t i = chunk % NUM_DMA_BUF;
	uint64_t n = *rx_count;
	uint64_t num_pending = 0;
	int ret = 0;

	if (!ctx || !wf_issued)
		return -EINVAL;

	ret = do_dma(ctx, DMA_HOST_ADDR(ctx->dma_iova[i]),
		src + chunk * ctx->dma_buf_size,
		ctx->dma_buf_size, 1, FPGA_TO_HOST, 0);
	if (ret)
		return ret;

	num_pending = chunk - n + 1;
	if (num_pending == HALF_DMA_BUF) {
		ret = issue_magic(ctx);
		if (ret) {
			IFPGA_RAWDEV_PMD_DEBUG("Magic issue failed");
			return ret;
		}
		*wf_issued = 1;
	}

	if ((num_pending > (NUM_DMA_BUF - 1)) || is_last_chunk) {
		if (*wf_issued) {
			wait_magic(ctx);
			for (i = 0; i < HALF_DMA_BUF; i++) {
				rte_memcpy((void *)(uintptr_t)(dst +
						n * ctx->dma_buf_size),
					ctx->dma_buf[n % NUM_DMA_BUF],
					ctx->dma_buf_size);
				n++;
			}
			*wf_issued = 0;
			*rx_count = n;
		}
		ret = issue_magic(ctx);
		if (ret) {
			IFPGA_RAWDEV_PMD_DEBUG("Magic issue failed");
			return ret;
		}
		*wf_issued = 1;
	}

	return ret;
}

static int dma_fpga_to_host(struct dma_afu_ctx *ctx, uint64_t dst, uint64_t src,
	size_t count)
{
	uint64_t i = 0;
	uint64_t count_left = count;
	uint64_t aligned_addr = 0;
	uint64_t align_bytes = 0;
	uint64_t dma_chunks = 0;
	uint64_t pending_buf = 0;
	uint64_t dma_rx_bytes = 0;
	uint64_t offset = 0;
	int wf_issued = 0;
	int ret = 0;

	IFPGA_RAWDEV_PMD_DEBUG("0x%"PRIx64" ---> 0x%"PRIx64" (%zu)", src, dst,
		count);

	if (!ctx)
		return -EINVAL;

	if (!IS_DMA_ALIGNED(src)) {
		if (count_left < DMA_ALIGN_BYTES)
			return ase_fpga_to_host(ctx, &src, &dst, count_left);

		aligned_addr = ((src / DMA_ALIGN_BYTES) + 1)
			 * DMA_ALIGN_BYTES;
		align_bytes = aligned_addr - src;
		ret = ase_fpga_to_host(ctx, &src, &dst, align_bytes);
		if (ret)
			return ret;
		count_left = count_left - align_bytes;
	}

	if (count_left) {
		dma_chunks = count_left / ctx->dma_buf_size;
		offset = dma_chunks * ctx->dma_buf_size;
		count_left -= offset;
		IFPGA_RAWDEV_PMD_DEBUG("0x%"PRIx64" ---> 0x%"PRIx64
			" (%"PRIu64"...0x%"PRIx64")",
			src, dst, dma_chunks, count_left);
		for (i = 0; i < dma_chunks; i++) {
			ret = dma_rx_buf(ctx, dst, src, i,
				i == (dma_chunks - 1),
				&pending_buf, &wf_issued);
			if (ret)
				return ret;
		}

		if (wf_issued)
			wait_magic(ctx);

		/* clear out final dma memcpy operations */
		while (pending_buf < dma_chunks) {
			/* constant size transfer; no length check required */
			rte_memcpy((void *)(uintptr_t)(dst +
					pending_buf * ctx->dma_buf_size),
				ctx->dma_buf[pending_buf % NUM_DMA_BUF],
				ctx->dma_buf_size);
			pending_buf++;
		}

		if (count_left > 0) {
			i = count_left / DMA_ALIGN_BYTES;
			if (i > 0) {
				dma_rx_bytes = i * DMA_ALIGN_BYTES;
				IFPGA_RAWDEV_PMD_DEBUG("left over 0x%"PRIx64" to DMA",
					dma_rx_bytes);
				ret = do_dma(ctx,
					DMA_HOST_ADDR(ctx->dma_iova[0]),
					src + offset,
					dma_rx_bytes, 1, FPGA_TO_HOST, 0);
				if (ret)
					return ret;
				ret = issue_magic(ctx);
				if (ret)
					return ret;
				wait_magic(ctx);
				rte_memcpy((void *)(uintptr_t)(dst + offset),
					ctx->dma_buf[0], dma_rx_bytes);
			}

			count_left -= dma_rx_bytes;
			if (count_left) {
				IFPGA_RAWDEV_PMD_DEBUG("left over 0x%"PRIx64" to ASE",
					count_left);
				dst += offset + dma_rx_bytes;
				src += offset + dma_rx_bytes;
				ret = ase_fpga_to_host(ctx, &src, &dst,
							count_left);
			}
		}
	}

	return ret;
}

static int dma_fpga_to_fpga(struct dma_afu_ctx *ctx, uint64_t dst, uint64_t src,
	size_t count)
{
	uint64_t i = 0;
	uint64_t count_left = count;
	uint64_t dma_chunks = 0;
	uint64_t offset = 0;
	uint64_t tx_chunks = 0;
	uint64_t *tmp_buf = NULL;
	int ret = 0;

	IFPGA_RAWDEV_PMD_DEBUG("0x%"PRIx64" ---> 0x%"PRIx64" (%zu)", src, dst,
		count);

	if (!ctx)
		return -EINVAL;

	if (IS_DMA_ALIGNED(dst) && IS_DMA_ALIGNED(src)
	    && IS_DMA_ALIGNED(count_left)) {
		dma_chunks = count_left / ctx->dma_buf_size;
		offset = dma_chunks * ctx->dma_buf_size;
		count_left -= offset;
		IFPGA_RAWDEV_PMD_DEBUG("0x%"PRIx64" ---> 0x%"PRIx64
			" (%"PRIu64"...0x%"PRIx64")",
			src, dst, dma_chunks, count_left);
		for (i = 0; i < dma_chunks; i++) {
			ret = do_dma(ctx, dst + i * ctx->dma_buf_size,
				src + i * ctx->dma_buf_size,
				ctx->dma_buf_size, 0, FPGA_TO_FPGA, 0);
			if (ret)
				return ret;
			if ((((i + 1) % NUM_DMA_BUF) == 0) ||
				(i == (dma_chunks - 1))) {
				ret = issue_magic(ctx);
				if (ret)
					return ret;
				wait_magic(ctx);
			}
		}

		if (count_left > 0) {
			IFPGA_RAWDEV_PMD_DEBUG("left over 0x%"PRIx64" to DMA", count_left);
			ret = do_dma(ctx, dst + offset, src + offset,
				count_left, 1, FPGA_TO_FPGA, 0);
			if (ret)
				return ret;
			ret = issue_magic(ctx);
			if (ret)
				return ret;
			wait_magic(ctx);
		}
	} else {
		if ((src < dst) && (src + count_left > dst)) {
			IFPGA_RAWDEV_PMD_ERR("Overlapping: 0x%"PRIx64
				" -> 0x%"PRIx64" (0x%"PRIx64")",
				src, dst, count_left);
			return -EINVAL;
		}
		tx_chunks = count_left / ctx->dma_buf_size;
		offset = tx_chunks * ctx->dma_buf_size;
		count_left -= offset;
		IFPGA_RAWDEV_PMD_DEBUG("0x%"PRIx64" --> 0x%"PRIx64
			" (%"PRIu64"...0x%"PRIx64")",
			src, dst, tx_chunks, count_left);
		tmp_buf = (uint64_t *)rte_malloc(NULL, ctx->dma_buf_size,
			DMA_ALIGN_BYTES);
		for (i = 0; i < tx_chunks; i++) {
			ret = dma_fpga_to_host(ctx, (uint64_t)tmp_buf,
				src + i * ctx->dma_buf_size,
				ctx->dma_buf_size);
			if (ret)
				goto free_buf;
			ret = dma_host_to_fpga(ctx,
				dst + i * ctx->dma_buf_size,
				(uint64_t)tmp_buf, ctx->dma_buf_size);
			if (ret)
				goto free_buf;
		}

		if (count_left > 0) {
			ret = dma_fpga_to_host(ctx, (uint64_t)tmp_buf,
				src + offset, count_left);
			if (ret)
				goto free_buf;
			ret = dma_host_to_fpga(ctx, dst + offset,
				(uint64_t)tmp_buf, count_left);
			if (ret)
				goto free_buf;
		}
free_buf:
		rte_free(tmp_buf);
	}

	return ret;
}

static int dma_transfer_sync(struct dma_afu_ctx *ctx, uint64_t dst,
	uint64_t src, size_t count, fpga_dma_type type)
{
	int ret = 0;

	if (!ctx)
		return -EINVAL;

	if (type == HOST_TO_FPGA)
		ret = dma_host_to_fpga(ctx, dst, src, count);
	else if (type == FPGA_TO_HOST)
		ret = dma_fpga_to_host(ctx, dst, src, count);
	else if (type == FPGA_TO_FPGA)
		ret = dma_fpga_to_fpga(ctx, dst, src, count);
	else
		return -EINVAL;

	return ret;
}

static double get_duration(struct timespec start, struct timespec end)
{
	uint64_t diff = 1000000000L * (end.tv_sec - start.tv_sec)
		+ end.tv_nsec - start.tv_nsec;
	return (double)diff / (double)1000000000L;
}

#define SWEEP_ITERS 1
static int sweep_test(struct dma_afu_ctx *ctx, uint32_t length,
	uint64_t ddr_offset, uint64_t buf_offset, uint64_t size_decrement)
{
	struct timespec start, end;
	uint64_t test_size = 0;
	uint64_t *dma_buf_ptr = NULL;
	double throughput, total_time = 0.0;
	int i = 0;
	int ret = 0;

	if (!ctx || !ctx->data_buf || !ctx->ref_buf) {
		IFPGA_RAWDEV_PMD_ERR("Buffer for DMA test is not allocated");
		return -EINVAL;
	}

	if (length < (buf_offset + size_decrement)) {
		IFPGA_RAWDEV_PMD_ERR("Test length does not match unaligned parameter");
		return -EINVAL;
	}
	test_size = length - (buf_offset + size_decrement);
	if ((ddr_offset + test_size) > ctx->mem_size) {
		IFPGA_RAWDEV_PMD_ERR("Test is out of DDR memory space");
		return -EINVAL;
	}

	dma_buf_ptr = (uint64_t *)((uint8_t *)ctx->data_buf + buf_offset);
	printf("Sweep Host %p to FPGA 0x%"PRIx64
		" with 0x%"PRIx64" bytes ...\n",
		(void *)dma_buf_ptr, ddr_offset, test_size);

	for (i = 0; i < SWEEP_ITERS; i++) {
		clock_gettime(CLOCK_MONOTONIC, &start);
		ret = dma_transfer_sync(ctx, ddr_offset, (uint64_t)dma_buf_ptr,
			test_size, HOST_TO_FPGA);
		clock_gettime(CLOCK_MONOTONIC, &end);
		if (ret) {
			IFPGA_RAWDEV_PMD_ERR("Failed");
			return ret;
		}
		total_time += get_duration(start, end);
	}
	throughput = (test_size * SWEEP_ITERS) / (total_time * 1000000);
	printf("Measured bandwidth = %lf MB/s\n", throughput);

	printf("Sweep FPGA 0x%"PRIx64" to Host %p with 0x%"PRIx64" bytes ...\n",
		ddr_offset, (void *)dma_buf_ptr, test_size);

	total_time = 0.0;
	memset((char *)dma_buf_ptr, 0, test_size);
	for (i = 0; i < SWEEP_ITERS; i++) {
		clock_gettime(CLOCK_MONOTONIC, &start);
		ret = dma_transfer_sync(ctx, (uint64_t)dma_buf_ptr, ddr_offset,
			test_size, FPGA_TO_HOST);
		clock_gettime(CLOCK_MONOTONIC, &end);
		if (ret) {
			IFPGA_RAWDEV_PMD_ERR("Failed");
			return ret;
		}
		total_time += get_duration(start, end);
	}
	throughput = (test_size * SWEEP_ITERS) / (total_time * 1000000);
	printf("Measured bandwidth = %lf MB/s\n", throughput);

	printf("Verifying buffer ...\n");
	return dma_afu_buf_verify(ctx, test_size);
}

static int dma_afu_test(struct afu_rawdev *dev)
{
	struct n3000_afu_priv *priv = NULL;
	struct dma_afu_ctx *ctx = NULL;
	struct rte_pmd_afu_dma_cfg *cfg = NULL;
	msgdma_ctrl ctrl;
	uint64_t offset = 0;
	uint32_t i = 0;
	int ret = 0;

	if (!dev)
		return -EINVAL;

	if (!dev->priv)
		return -ENOENT;

	priv = (struct n3000_afu_priv *)dev->priv;
	cfg = &priv->dma_cfg;
	if (cfg->index >= NUM_N3000_DMA)
		return -EINVAL;
	ctx = &priv->dma_ctx[cfg->index];

	ctx->pattern = (int)cfg->pattern;
	ctx->verbose = (int)cfg->verbose;
	ctx->dma_buf_size = cfg->size;

	ret = dma_afu_buf_alloc(ctx, cfg);
	if (ret)
		goto free;

	printf("Initialize test buffer\n");
	dma_afu_buf_init(ctx, cfg->length);

	/* enable interrupt */
	ctrl.csr = 0;
	ctrl.global_intr_en_mask = 1;
	rte_write32(ctrl.csr, CSR_CONTROL(ctx->csr_addr));

	printf("Host %p to FPGA 0x%x with 0x%x bytes\n", ctx->data_buf,
		cfg->offset, cfg->length);
	ret = dma_transfer_sync(ctx, cfg->offset, (uint64_t)ctx->data_buf,
		cfg->length, HOST_TO_FPGA);
	if (ret) {
		IFPGA_RAWDEV_PMD_ERR("Failed to transfer data from host to FPGA");
		goto end;
	}
	memset(ctx->data_buf, 0, cfg->length);

	printf("FPGA 0x%x to Host %p with 0x%x bytes\n", cfg->offset,
		ctx->data_buf, cfg->length);
	ret = dma_transfer_sync(ctx, (uint64_t)ctx->data_buf, cfg->offset,
		cfg->length, FPGA_TO_HOST);
	if (ret) {
		IFPGA_RAWDEV_PMD_ERR("Failed to transfer data from FPGA to host");
		goto end;
	}
	ret = dma_afu_buf_verify(ctx, cfg->length);
	if (ret)
		goto end;

	if ((cfg->offset + cfg->length * 2) <= ctx->mem_size)
		offset = cfg->offset + cfg->length;
	else if (cfg->offset > cfg->length)
		offset = 0;
	else
		goto end;

	printf("FPGA 0x%x to FPGA 0x%"PRIx64" with 0x%x bytes\n",
		cfg->offset, offset, cfg->length);
	ret = dma_transfer_sync(ctx, offset, cfg->offset, cfg->length,
		FPGA_TO_FPGA);
	if (ret) {
		IFPGA_RAWDEV_PMD_ERR("Failed to transfer data from FPGA to FPGA");
		goto end;
	}

	printf("FPGA 0x%"PRIx64" to Host %p with 0x%x bytes\n", offset,
		ctx->data_buf, cfg->length);
	ret = dma_transfer_sync(ctx, (uint64_t)ctx->data_buf, offset,
		cfg->length, FPGA_TO_HOST);
	if (ret) {
		IFPGA_RAWDEV_PMD_ERR("Failed to transfer data from FPGA to host");
		goto end;
	}
	ret = dma_afu_buf_verify(ctx, cfg->length);
	if (ret)
		goto end;

	printf("Sweep with aligned address and size\n");
	ret = sweep_test(ctx, cfg->length, cfg->offset, 0, 0);
	if (ret)
		goto end;

	if (cfg->unaligned) {
		printf("Sweep with unaligned address and size\n");
		struct unaligned_set {
			uint64_t addr_offset;
			uint64_t size_dec;
		} param[] = {{61, 5}, {3, 0}, {7, 3}, {0, 3}, {0, 61}, {0, 7}};
		for (i = 0; i < ARRAY_SIZE(param); i++) {
			ret = sweep_test(ctx, cfg->length, cfg->offset,
				param[i].addr_offset, param[i].size_dec);
			if (ret)
				break;
		}
	}

end:
	/* disable interrupt */
	ctrl.global_intr_en_mask = 0;
	rte_write32(ctrl.csr, CSR_CONTROL(ctx->csr_addr));

free:
	dma_afu_buf_free(ctx);
	return ret;
}

static struct rte_pci_device *n3000_afu_get_pci_dev(struct afu_rawdev *dev)
{
	struct rte_afu_device *afudev = NULL;

	if (!dev || !dev->rawdev || !dev->rawdev->device)
		return NULL;

	afudev = RTE_DEV_TO_AFU(dev->rawdev->device);
	if (!afudev->rawdev || !afudev->rawdev->device)
		return NULL;

	return RTE_DEV_TO_PCI(afudev->rawdev->device);
}

#ifdef VFIO_PRESENT
static int dma_afu_set_irqs(struct afu_rawdev *dev, uint32_t vec_start,
	uint32_t count, int *efds)
{
	struct rte_pci_device *pci_dev = NULL;
	struct vfio_irq_set *irq_set = NULL;
	int vfio_dev_fd = 0;
	size_t sz = 0;
	int ret = 0;

	if (!dev || !efds || (count == 0) || (count > MAX_MSIX_VEC))
		return -EINVAL;

	pci_dev = n3000_afu_get_pci_dev(dev);
	if (!pci_dev)
		return -ENODEV;
	vfio_dev_fd = rte_intr_dev_fd_get(pci_dev->intr_handle);

	sz = sizeof(*irq_set) + sizeof(*efds) * count;
	irq_set = rte_zmalloc(NULL, sz, 0);
	if (!irq_set)
		return -ENOMEM;

	irq_set->argsz = (uint32_t)sz;
	irq_set->count = count;
	irq_set->flags = VFIO_IRQ_SET_DATA_EVENTFD |
		VFIO_IRQ_SET_ACTION_TRIGGER;
	irq_set->index = VFIO_PCI_MSIX_IRQ_INDEX;
	irq_set->start = vec_start;

	rte_memcpy(&irq_set->data, efds, sizeof(*efds) * count);
	ret = ioctl(vfio_dev_fd, VFIO_DEVICE_SET_IRQS, irq_set);
	if (ret)
		IFPGA_RAWDEV_PMD_ERR("Error enabling MSI-X interrupts\n");

	rte_free(irq_set);
	return ret;
}
#endif

static void *n3000_afu_get_port_addr(struct afu_rawdev *dev)
{
	struct rte_pci_device *pci_dev = NULL;
	uint8_t *addr = NULL;
	uint64_t val = 0;
	uint32_t bar = 0;

	pci_dev = n3000_afu_get_pci_dev(dev);
	if (!pci_dev)
		return NULL;

	addr = (uint8_t *)pci_dev->mem_resource[0].addr;
	val = rte_read64(addr + PORT_ATTR_REG(dev->port));
	if (!PORT_IMPLEMENTED(val)) {
		IFPGA_RAWDEV_PMD_INFO("FIU port %d is not implemented", dev->port);
		return NULL;
	}

	bar = PORT_BAR(val);
	if (bar >= PCI_MAX_RESOURCE) {
		IFPGA_RAWDEV_PMD_ERR("BAR index %u is out of limit", bar);
		return NULL;
	}

	addr = (uint8_t *)pci_dev->mem_resource[bar].addr + PORT_OFFSET(val);
	return addr;
}

static int n3000_afu_get_irq_capability(struct afu_rawdev *dev,
	uint32_t *vec_start, uint32_t *vec_count)
{
	uint8_t *addr = NULL;
	uint64_t val = 0;
	uint64_t header = 0;
	uint64_t next_offset = 0;

	addr = (uint8_t *)n3000_afu_get_port_addr(dev);
	if (!addr)
		return -ENOENT;

	do {
		addr += next_offset;
		header = rte_read64(addr);
		if ((DFH_TYPE(header) == DFH_TYPE_PRIVATE) &&
			(DFH_FEATURE_ID(header) == PORT_FEATURE_UINT_ID)) {
			val = rte_read64(addr + PORT_UINT_CAP_REG);
			if (vec_start)
				*vec_start = PORT_VEC_START(val);
			if (vec_count)
				*vec_count = PORT_VEC_COUNT(val);
			return 0;
		}
		next_offset = DFH_NEXT_OFFSET(header);
		if (((next_offset & 0xffff) == 0xffff) || (next_offset == 0))
			break;
	} while (!DFH_EOL(header));

	return -ENOENT;
}

static int nlb_afu_ctx_release(struct afu_rawdev *dev)
{
	struct n3000_afu_priv *priv = NULL;
	struct nlb_afu_ctx *ctx = NULL;

	if (!dev)
		return -EINVAL;

	priv = (struct n3000_afu_priv *)dev->priv;
	if (!priv)
		return -ENOENT;

	ctx = &priv->nlb_ctx;

	rte_free(ctx->dsm_ptr);
	ctx->dsm_ptr = NULL;
	ctx->status_ptr = NULL;

	rte_free(ctx->src_ptr);
	ctx->src_ptr = NULL;

	rte_free(ctx->dest_ptr);
	ctx->dest_ptr = NULL;

	return 0;
}

static int nlb_afu_ctx_init(struct afu_rawdev *dev, uint8_t *addr)
{
	struct n3000_afu_priv *priv = NULL;
	struct nlb_afu_ctx *ctx = NULL;
	int ret = 0;

	if (!dev || !addr)
		return -EINVAL;

	priv = (struct n3000_afu_priv *)dev->priv;
	if (!priv)
		return -ENOENT;

	ctx = &priv->nlb_ctx;
	ctx->addr = addr;

	ctx->dsm_ptr = (uint8_t *)rte_zmalloc(NULL, DSM_SIZE, TEST_MEM_ALIGN);
	if (!ctx->dsm_ptr)
		return -ENOMEM;

	ctx->dsm_iova = rte_malloc_virt2iova(ctx->dsm_ptr);
	if (ctx->dsm_iova == RTE_BAD_IOVA) {
		ret = -ENOMEM;
		goto release_dsm;
	}

	ctx->src_ptr = (uint8_t *)rte_zmalloc(NULL, NLB_BUF_SIZE,
		TEST_MEM_ALIGN);
	if (!ctx->src_ptr) {
		ret = -ENOMEM;
		goto release_dsm;
	}
	ctx->src_iova = rte_malloc_virt2iova(ctx->src_ptr);
	if (ctx->src_iova == RTE_BAD_IOVA) {
		ret = -ENOMEM;
		goto release_src;
	}

	ctx->dest_ptr = (uint8_t *)rte_zmalloc(NULL, NLB_BUF_SIZE,
		TEST_MEM_ALIGN);
	if (!ctx->dest_ptr) {
		ret = -ENOMEM;
		goto release_src;
	}
	ctx->dest_iova = rte_malloc_virt2iova(ctx->dest_ptr);
	if (ctx->dest_iova == RTE_BAD_IOVA) {
		ret = -ENOMEM;
		goto release_dest;
	}

	ctx->status_ptr = (struct nlb_dsm_status *)(ctx->dsm_ptr + DSM_STATUS);
	return 0;

release_dest:
	rte_free(ctx->dest_ptr);
	ctx->dest_ptr = NULL;
release_src:
	rte_free(ctx->src_ptr);
	ctx->src_ptr = NULL;
release_dsm:
	rte_free(ctx->dsm_ptr);
	ctx->dsm_ptr = NULL;
	return ret;
}

static int dma_afu_ctx_release(struct afu_rawdev *dev)
{
	struct n3000_afu_priv *priv = NULL;
	struct dma_afu_ctx *ctx = NULL;

	if (!dev)
		return -EINVAL;

	priv = (struct n3000_afu_priv *)dev->priv;
	if (!priv)
		return -ENOENT;

	ctx = &priv->dma_ctx[0];

	rte_free(ctx->desc_buf);
	ctx->desc_buf = NULL;

	rte_free(ctx->magic_buf);
	ctx->magic_buf = NULL;

	close(ctx->event_fd);
	return 0;
}

static int dma_afu_ctx_init(struct afu_rawdev *dev, int index, uint8_t *addr)
{
	struct n3000_afu_priv *priv = NULL;
	struct dma_afu_ctx *ctx = NULL;
	uint64_t mem_sz[] = {0x100000000, 0x100000000, 0x40000000, 0x1000000};
	static int efds[1] = {0};
	uint32_t vec_start = 0;
	int ret = 0;

	if (!dev || (index < 0) || (index >= NUM_N3000_DMA) || !addr)
		return -EINVAL;

	priv = (struct n3000_afu_priv *)dev->priv;
	if (!priv)
		return -ENOENT;

	ctx = &priv->dma_ctx[index];
	ctx->index = index;
	ctx->addr = addr;
	ctx->csr_addr = addr + DMA_CSR;
	ctx->desc_addr = addr + DMA_DESC;
	ctx->ase_ctrl_addr = addr + DMA_ASE_CTRL;
	ctx->ase_data_addr = addr + DMA_ASE_DATA;
	ctx->mem_size = mem_sz[ctx->index];
	ctx->cur_ase_page = INVALID_ASE_PAGE;
	if (ctx->index == 0) {
		ret = n3000_afu_get_irq_capability(dev, &vec_start, NULL);
		if (ret)
			return ret;

		efds[0] = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
		if (efds[0] < 0) {
			IFPGA_RAWDEV_PMD_ERR("eventfd create failed");
			return -EBADF;
		}
#ifdef VFIO_PRESENT
		if (dma_afu_set_irqs(dev, vec_start, 1, efds))
			IFPGA_RAWDEV_PMD_ERR("DMA interrupt setup failed");
#endif
	}
	ctx->event_fd = efds[0];

	ctx->desc_buf = (msgdma_ext_desc *)rte_zmalloc(NULL,
		sizeof(msgdma_ext_desc), DMA_ALIGN_BYTES);
	if (!ctx->desc_buf) {
		ret = -ENOMEM;
		goto release;
	}

	ctx->magic_buf = (uint64_t *)rte_zmalloc(NULL, MAGIC_BUF_SIZE,
		TEST_MEM_ALIGN);
	if (!ctx->magic_buf) {
		ret = -ENOMEM;
		goto release;
	}
	ctx->magic_iova = rte_malloc_virt2iova(ctx->magic_buf);
	if (ctx->magic_iova == RTE_BAD_IOVA) {
		ret = -ENOMEM;
		goto release;
	}

	return 0;

release:
	dma_afu_ctx_release(dev);
	return ret;
}

static int n3000_afu_ctx_init(struct afu_rawdev *dev)
{
	struct n3000_afu_priv *priv = NULL;
	uint8_t *addr = NULL;
	uint64_t header = 0;
	uint64_t uuid_hi = 0;
	uint64_t uuid_lo = 0;
	uint64_t next_offset = 0;
	int ret = 0;

	if (!dev)
		return -EINVAL;

	priv = (struct n3000_afu_priv *)dev->priv;
	if (!priv)
		return -ENOENT;

	addr = (uint8_t *)dev->addr;
	do {
		addr += next_offset;
		header = rte_read64(addr);
		uuid_lo = rte_read64(addr + DFH_UUID_L_OFFSET);
		uuid_hi = rte_read64(addr + DFH_UUID_H_OFFSET);

		if ((DFH_TYPE(header) == DFH_TYPE_AFU) &&
			(uuid_lo == N3000_NLB0_UUID_L) &&
			(uuid_hi == N3000_NLB0_UUID_H)) {
			IFPGA_RAWDEV_PMD_INFO("AFU NLB0 found @ %p", (void *)addr);
			ret = nlb_afu_ctx_init(dev, addr);
			if (ret)
				return ret;
		} else if ((DFH_TYPE(header) == DFH_TYPE_BBB) &&
			(uuid_lo == N3000_DMA_UUID_L) &&
			(uuid_hi == N3000_DMA_UUID_H) &&
			(priv->num_dma < NUM_N3000_DMA)) {
			IFPGA_RAWDEV_PMD_INFO("AFU DMA%d found @ %p",
				priv->num_dma, (void *)addr);
			ret = dma_afu_ctx_init(dev, priv->num_dma, addr);
			if (ret)
				return ret;
			priv->num_dma++;
		} else {
			IFPGA_RAWDEV_PMD_DEBUG("DFH: type %"PRIu64
				", uuid %016"PRIx64"%016"PRIx64,
				DFH_TYPE(header), uuid_hi, uuid_lo);
		}

		next_offset = DFH_NEXT_OFFSET(header);
		if (((next_offset & 0xffff) == 0xffff) || (next_offset == 0))
			break;
	} while (!DFH_EOL(header));

	return 0;
}

static int n3000_afu_init(struct afu_rawdev *dev)
{
	if (!dev)
		return -EINVAL;

	if (!dev->priv) {
		dev->priv = rte_zmalloc(NULL, sizeof(struct n3000_afu_priv), 0);
		if (!dev->priv)
			return -ENOMEM;
	}

	return n3000_afu_ctx_init(dev);
}

static int n3000_afu_config(struct afu_rawdev *dev, void *config,
	size_t config_size)
{
	struct n3000_afu_priv *priv = NULL;
	struct rte_pmd_afu_n3000_cfg *cfg = NULL;
	int i = 0;
	uint64_t top = 0;

	if (!dev || !config || !config_size)
		return -EINVAL;

	priv = (struct n3000_afu_priv *)dev->priv;
	if (!priv)
		return -ENOENT;

	if (config_size != sizeof(struct rte_pmd_afu_n3000_cfg))
		return -EINVAL;

	cfg = (struct rte_pmd_afu_n3000_cfg *)config;
	if (cfg->type == RTE_PMD_AFU_N3000_NLB) {
		if (cfg->nlb_cfg.mode != NLB_MODE_LPBK)
			return -EINVAL;
		if ((cfg->nlb_cfg.read_vc > NLB_VC_RANDOM) ||
			(cfg->nlb_cfg.write_vc > NLB_VC_RANDOM))
			return -EINVAL;
		if (cfg->nlb_cfg.wrfence_vc > NLB_VC_VH1)
			return -EINVAL;
		if (cfg->nlb_cfg.cache_hint > NLB_RDLINE_MIXED)
			return -EINVAL;
		if (cfg->nlb_cfg.cache_policy > NLB_WRPUSH_I)
			return -EINVAL;
		if ((cfg->nlb_cfg.multi_cl != 1) &&
			(cfg->nlb_cfg.multi_cl != 2) &&
			(cfg->nlb_cfg.multi_cl != 4))
			return -EINVAL;
		if ((cfg->nlb_cfg.begin < MIN_CACHE_LINES) ||
			(cfg->nlb_cfg.begin > MAX_CACHE_LINES))
			return -EINVAL;
		if ((cfg->nlb_cfg.end < cfg->nlb_cfg.begin) ||
			(cfg->nlb_cfg.end > MAX_CACHE_LINES))
			return -EINVAL;
		rte_memcpy(&priv->nlb_cfg, &cfg->nlb_cfg,
			sizeof(struct rte_pmd_afu_nlb_cfg));
	} else if (cfg->type == RTE_PMD_AFU_N3000_DMA) {
		if (cfg->dma_cfg.index >= NUM_N3000_DMA)
			return -EINVAL;
		i = cfg->dma_cfg.index;
		if (cfg->dma_cfg.length > priv->dma_ctx[i].mem_size)
			return -EINVAL;
		if (cfg->dma_cfg.offset >= priv->dma_ctx[i].mem_size)
			return -EINVAL;
		top = cfg->dma_cfg.length + cfg->dma_cfg.offset;
		if ((top == 0) || (top > priv->dma_ctx[i].mem_size))
			return -EINVAL;
		if (i == 3) {  /* QDR connected to DMA3 */
			if (cfg->dma_cfg.length & 0x3f) {
				cfg->dma_cfg.length &= ~0x3f;
				IFPGA_RAWDEV_PMD_INFO("Round size to %x for QDR",
					cfg->dma_cfg.length);
			}
		}
		rte_memcpy(&priv->dma_cfg, &cfg->dma_cfg,
			sizeof(struct rte_pmd_afu_dma_cfg));
	} else {
		IFPGA_RAWDEV_PMD_ERR("Invalid type of N3000 AFU");
		return -EINVAL;
	}

	priv->cfg_type = cfg->type;
	return 0;
}

static int n3000_afu_test(struct afu_rawdev *dev)
{
	struct n3000_afu_priv *priv = NULL;
	int ret = 0;

	if (!dev)
		return -EINVAL;

	if (!dev->priv)
		return -ENOENT;

	priv = (struct n3000_afu_priv *)dev->priv;

	if (priv->cfg_type == RTE_PMD_AFU_N3000_NLB) {
		IFPGA_RAWDEV_PMD_INFO("Test NLB");
		ret = nlb_afu_test(dev);
	} else if (priv->cfg_type == RTE_PMD_AFU_N3000_DMA) {
		IFPGA_RAWDEV_PMD_INFO("Test DMA%u", priv->dma_cfg.index);
		ret = dma_afu_test(dev);
	} else {
		IFPGA_RAWDEV_PMD_ERR("Please configure AFU before test");
		ret = -EINVAL;
	}

	return ret;
}

static int n3000_afu_close(struct afu_rawdev *dev)
{
	if (!dev)
		return -EINVAL;

	nlb_afu_ctx_release(dev);
	dma_afu_ctx_release(dev);

	rte_free(dev->priv);
	dev->priv = NULL;

	return 0;
}

static int n3000_afu_dump(struct afu_rawdev *dev, FILE *f)
{
	struct n3000_afu_priv *priv = NULL;

	if (!dev)
		return -EINVAL;

	priv = (struct n3000_afu_priv *)dev->priv;
	if (!priv)
		return -ENOENT;

	if (!f)
		f = stdout;

	if (priv->cfg_type == RTE_PMD_AFU_N3000_NLB) {
		struct nlb_afu_ctx *ctx = &priv->nlb_ctx;
		fprintf(f, "addr:\t\t%p\n", (void *)ctx->addr);
		fprintf(f, "dsm_ptr:\t%p\n", (void *)ctx->dsm_ptr);
		fprintf(f, "dsm_iova:\t0x%"PRIx64"\n", ctx->dsm_iova);
		fprintf(f, "src_ptr:\t%p\n", (void *)ctx->src_ptr);
		fprintf(f, "src_iova:\t0x%"PRIx64"\n", ctx->src_iova);
		fprintf(f, "dest_ptr:\t%p\n", (void *)ctx->dest_ptr);
		fprintf(f, "dest_iova:\t0x%"PRIx64"\n", ctx->dest_iova);
		fprintf(f, "status_ptr:\t%p\n", (void *)ctx->status_ptr);
	} else if (priv->cfg_type == RTE_PMD_AFU_N3000_DMA) {
		struct dma_afu_ctx *ctx = &priv->dma_ctx[priv->dma_cfg.index];
		fprintf(f, "index:\t\t%d\n", ctx->index);
		fprintf(f, "addr:\t\t%p\n", (void *)ctx->addr);
		fprintf(f, "csr_addr:\t%p\n", (void *)ctx->csr_addr);
		fprintf(f, "desc_addr:\t%p\n", (void *)ctx->desc_addr);
		fprintf(f, "ase_ctrl_addr:\t%p\n", (void *)ctx->ase_ctrl_addr);
		fprintf(f, "ase_data_addr:\t%p\n", (void *)ctx->ase_data_addr);
		fprintf(f, "desc_buf:\t%p\n", (void *)ctx->desc_buf);
		fprintf(f, "magic_buf:\t%p\n", (void *)ctx->magic_buf);
		fprintf(f, "magic_iova:\t0x%"PRIx64"\n", ctx->magic_iova);
	} else {
		return -EINVAL;
	}

	return 0;
}

static int n3000_afu_reset(struct afu_rawdev *dev)
{
	uint8_t *addr = NULL;
	uint64_t val = 0;

	addr = (uint8_t *)n3000_afu_get_port_addr(dev);
	if (!addr)
		return -ENOENT;

	val = rte_read64(addr + PORT_CTRL_REG);
	val |= PORT_SOFT_RESET;
	rte_write64(val, addr + PORT_CTRL_REG);
	rte_delay_us(100);
	val &= ~PORT_SOFT_RESET;
	rte_write64(val, addr + PORT_CTRL_REG);

	return 0;
}

static struct afu_ops n3000_afu_ops = {
	.init = n3000_afu_init,
	.config = n3000_afu_config,
	.start = NULL,
	.stop = NULL,
	.test = n3000_afu_test,
	.close = n3000_afu_close,
	.dump = n3000_afu_dump,
	.reset = n3000_afu_reset
};

static struct afu_rawdev_drv n3000_afu_drv = {
	.uuid = { N3000_AFU_UUID_L, N3000_AFU_UUID_H },
	.ops = &n3000_afu_ops
};

AFU_PMD_REGISTER(n3000_afu_drv);
