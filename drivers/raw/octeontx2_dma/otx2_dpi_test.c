/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <rte_common.h>
#include <rte_debug.h>
#include <rte_eal.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>
#include <rte_mbuf_pool_ops.h>
#include <rte_memcpy.h>
#include <rte_memory.h>
#include <rte_mempool.h>
#include <rte_per_lcore.h>
#include <rte_rawdev.h>

#include "otx2_dpi_rawdev.h"

static struct dpi_cring_data_s cring;

static uint8_t
buffer_fill(uint8_t *addr, int len, uint8_t val)
{
	int j = 0;

	memset(addr, 0, len);
	for (j = 0; j < len; j++)
		*(addr + j) = val++;

	return val;
}

static int
validate_buffer(uint8_t *saddr, uint8_t *daddr, int len)
{
	int j = 0, ret = 0;

	for (j = 0; j < len; j++) {
		if (*(saddr + j) != *(daddr + j)) {
			otx2_dpi_dbg("FAIL: Data Integrity failed");
			otx2_dpi_dbg("index: %d, Expected: 0x%x, Actual: 0x%x",
				     j, *(saddr + j), *(daddr + j));
			ret = -1;
			break;
		}
	}

	return ret;
}

static inline int
dma_test_internal(int dma_port, int buf_size)
{
	struct dpi_dma_req_compl_s *comp_data;
	struct dpi_dma_queue_ctx_s ctx = {0};
	struct rte_rawdev_buf buf = {0};
	struct rte_rawdev_buf *d_buf[1];
	struct rte_rawdev_buf *bufp[1];
	struct dpi_dma_buf_ptr_s cmd;
	union dpi_dma_ptr_u rptr = { {0} };
	union dpi_dma_ptr_u wptr = { {0} };
	uint8_t *fptr, *lptr;
	int ret;

	fptr = (uint8_t *)rte_malloc("dummy", buf_size, 128);
	lptr = (uint8_t *)rte_malloc("dummy", buf_size, 128);
	comp_data = rte_malloc("dummy", buf_size, 128);
	if (fptr == NULL || lptr == NULL || comp_data == NULL) {
		otx2_dpi_dbg("Unable to allocate internal memory");
		return -ENOMEM;
	}

	buffer_fill(fptr, buf_size, 0);
	memset(&cmd, 0, sizeof(struct dpi_dma_buf_ptr_s));
	memset(lptr, 0, buf_size);
	memset(comp_data, 0, buf_size);
	rptr.s.ptr = (uint64_t)fptr;
	rptr.s.length = buf_size;
	wptr.s.ptr = (uint64_t)lptr;
	wptr.s.length = buf_size;
	cmd.rptr[0] = &rptr;
	cmd.wptr[0] = &wptr;
	cmd.rptr_cnt = 1;
	cmd.wptr_cnt = 1;
	cmd.comp_ptr = comp_data;
	buf.buf_addr = (void *)&cmd;
	bufp[0] = &buf;

	ctx.xtype = DPI_XTYPE_INTERNAL_ONLY;
	ctx.pt = 0;
	ctx.c_ring = &cring;

	ret = rte_rawdev_enqueue_buffers(dma_port,
					 (struct rte_rawdev_buf **)bufp, 1,
					 &ctx);
	if (ret < 0) {
		otx2_dpi_dbg("Enqueue request failed");
		return 0;
	}

	/* Wait and dequeue completion */
	do {
		sleep(1);
		ret = rte_rawdev_dequeue_buffers(dma_port, &d_buf[0], 1, &ctx);
		if (ret)
			break;

		otx2_dpi_dbg("Dequeue request not completed");
	} while (1);

	if (validate_buffer(fptr, lptr, buf_size)) {
		otx2_dpi_dbg("DMA transfer failed\n");
		return -EAGAIN;
	}
	otx2_dpi_dbg("Internal Only DMA transfer successfully completed");

	if (lptr)
		rte_free(lptr);
	if (fptr)
		rte_free(fptr);
	if (comp_data)
		rte_free(comp_data);

	return 0;
}

static void *
dpi_create_mempool(void)
{
	void *chunk_pool = NULL;
	char pool_name[25];
	int ret;

	snprintf(pool_name, sizeof(pool_name), "dpi_chunk_pool");

	chunk_pool = (void *)rte_mempool_create_empty(pool_name, 1024, 1024,
						      0, 0, rte_socket_id(), 0);
	if (chunk_pool == NULL) {
		otx2_dpi_dbg("Unable to create memory pool.");
		return NULL;
	}

	ret = rte_mempool_set_ops_byname(chunk_pool,
					 rte_mbuf_platform_mempool_ops(), NULL);
	if (ret < 0) {
		otx2_dpi_dbg("Unable to set pool ops");
		rte_mempool_free(chunk_pool);
		return NULL;
	}

	ret = rte_mempool_populate_default(chunk_pool);
	if (ret < 0) {
		otx2_dpi_dbg("Unable to populate pool");
		return NULL;
	}

	return chunk_pool;
}

int
test_otx2_dma_rawdev(uint16_t val)
{
	struct rte_rawdev_info rdev_info = {0};
	struct dpi_rawdev_conf_s conf = {0};
	int ret, i, size = 1024;
	int nb_ports;

	RTE_SET_USED(val);
	nb_ports = rte_rawdev_count();
	if (nb_ports == 0) {
		otx2_dpi_dbg("No Rawdev ports - bye");
		return -ENODEV;
	}

	i = rte_rawdev_get_dev_id("DPI:5:00.1");
	/* Configure rawdev ports */
	conf.chunk_pool = dpi_create_mempool();
	rdev_info.dev_private = &conf;
	ret = rte_rawdev_configure(i, (rte_rawdev_obj_t)&rdev_info);
	if (ret) {
		otx2_dpi_dbg("Unable to configure DPIVF %d", i);
		return -ENODEV;
	}
	otx2_dpi_dbg("rawdev %d configured successfully", i);

	/* Each stream allocate its own completion ring data, store it in
	 * application context. Each stream needs to use same application
	 * context for enqueue/dequeue.
	 */
	cring.compl_data = rte_malloc("dummy", sizeof(void *) * 1024, 128);
	if (!cring.compl_data) {
		otx2_dpi_dbg("Completion allocation failed");
		return -ENOMEM;
	}

	cring.max_cnt = 1024;
	cring.head = 0;
	cring.tail = 0;

	ret = dma_test_internal(i, size);
	if (ret)
		otx2_dpi_dbg("DMA transfer failed for queue %d", i);

	if (rte_rawdev_close(i))
		otx2_dpi_dbg("Dev close failed for port %d", i);

	if (conf.chunk_pool)
		rte_mempool_free(conf.chunk_pool);

	return ret;
}
