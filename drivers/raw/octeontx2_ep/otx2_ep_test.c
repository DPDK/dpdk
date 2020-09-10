/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <rte_common.h>
#include <rte_eal.h>
#include <rte_lcore.h>
#include <rte_mempool.h>

#include <rte_rawdev.h>
#include <rte_rawdev_pmd.h>

#include "otx2_common.h"
#include "otx2_ep_rawdev.h"

#define SDP_IOQ_NUM_BUFS   (4 * 1024)
#define SDP_IOQ_BUF_SIZE   (2 * 1024)

#define SDP_TEST_PKT_FSZ   (0)
#define SDP_TEST_PKT_SIZE  (1024)

static int
sdp_validate_data(struct sdp_droq_pkt *oq_pkt, uint8_t *iq_pkt,
		  uint32_t pkt_len)
{
	if (!oq_pkt)
		return -EINVAL;

	if (pkt_len != oq_pkt->len) {
		otx2_err("Invalid packet length");
		return -EINVAL;
	}

	if (memcmp(oq_pkt->data, iq_pkt, pkt_len) != 0) {
		otx2_err("Data validation failed");
		return -EINVAL;
	}
	otx2_sdp_dbg("Data validation successful");

	return 0;
}

static void
sdp_ioq_buffer_fill(uint8_t *addr, uint32_t len)
{
	uint32_t idx;

	memset(addr, 0, len);

	for (idx = 0; idx < len; idx++)
		addr[idx] = idx;
}

static struct rte_mempool*
sdp_ioq_mempool_create(void)
{
	struct rte_mempool *mpool;

	mpool = rte_mempool_create("ioqbuf_pool",
				   SDP_IOQ_NUM_BUFS /*num elt*/,
				   SDP_IOQ_BUF_SIZE /*elt size*/,
				   0 /*cache_size*/,
				   0 /*private_data_size*/,
				   NULL /*mp_init*/,
				   NULL /*mp_init arg*/,
				   NULL /*obj_init*/,
				   NULL /*obj_init arg*/,
				   rte_socket_id() /*socket id*/,
				   (MEMPOOL_F_SP_PUT | MEMPOOL_F_SC_GET));

	return mpool;
}


int
sdp_rawdev_selftest(uint16_t dev_id)
{
	struct sdp_rawdev_info app_info = {0};
	struct rte_rawdev_info dev_info = {0};

	struct rte_rawdev_buf *d_buf[1];
	struct sdp_droq_pkt oq_pkt;
	struct sdp_soft_instr si;
	struct sdp_device sdpvf;

	uint32_t buf_size;
	int ret = 0;
	void *buf;

	otx2_info("SDP RAWDEV Self Test: Started");

	memset(&oq_pkt, 0x00, sizeof(oq_pkt));
	d_buf[0] = (struct rte_rawdev_buf *)&oq_pkt;

	struct rte_mempool *ioq_mpool = sdp_ioq_mempool_create();
	if (!ioq_mpool) {
		otx2_err("IOQ mpool creation failed");
		return -ENOMEM;
	}

	app_info.enqdeq_mpool = ioq_mpool;
	app_info.app_conf = NULL; /* Use default conf */

	dev_info.dev_private = &app_info;

	ret = rte_rawdev_configure(dev_id, &dev_info, sizeof(app_info));
	if (ret) {
		otx2_err("Unable to configure SDP_VF %d", dev_id);
		rte_mempool_free(ioq_mpool);
		return -ENOMEM;
	}
	otx2_info("SDP VF rawdev[%d] configured successfully", dev_id);

	memset(&si, 0x00, sizeof(si));
	memset(&sdpvf, 0x00, sizeof(sdpvf));

	buf_size = SDP_TEST_PKT_SIZE;

	si.q_no = 0;
	si.reqtype = SDP_REQTYPE_NORESP;
	si.rptr = NULL;

	si.ih.fsz = SDP_TEST_PKT_FSZ;
	si.ih.tlen = buf_size;
	si.ih.gather = 0;

	/* Enqueue raw pkt data */
	rte_mempool_get(ioq_mpool, &buf);
	if (!buf) {
		otx2_err("Buffer allocation failed");
		rte_mempool_free(ioq_mpool);
		rte_rawdev_close(dev_id);
		return -ENOMEM;
	}

	sdp_ioq_buffer_fill(buf, buf_size);
	si.dptr = (uint8_t *)buf;

	rte_rawdev_enqueue_buffers(dev_id, NULL, 1, &si);
	usleep(10000);

	/* Dequeue raw pkt data */
	ret = 0;
	while (ret < 1) {
		ret = rte_rawdev_dequeue_buffers(dev_id, &d_buf[0], 1, &si);
		rte_pause();
	}

	/* Validate the dequeued raw pkt data */
	if (sdp_validate_data((struct sdp_droq_pkt *)d_buf[0],
			      buf, buf_size) != 0) {
		otx2_err("Data invalid");
		rte_mempool_put(ioq_mpool,
				((struct sdp_droq_pkt *)d_buf[0])->data);
		rte_mempool_free(ioq_mpool);
		rte_rawdev_close(dev_id);
		return -EINVAL;
	}

	rte_mempool_put(ioq_mpool, ((struct sdp_droq_pkt *)d_buf[0])->data);
	rte_mempool_free(ioq_mpool);
	rte_rawdev_close(dev_id);

	otx2_info("SDP RAWDEV Self Test: Successful");

	return 0;
}
