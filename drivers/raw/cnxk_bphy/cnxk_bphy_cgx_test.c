/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */
#include <stdint.h>

#include <rte_cycles.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_rawdev.h>

#include "cnxk_bphy_cgx.h"
#include "rte_pmd_bphy.h"

static int
cnxk_bphy_cgx_enq_msg(uint16_t dev_id, unsigned int queue, void *msg)
{
	struct rte_rawdev_buf *bufs[1];
	struct rte_rawdev_buf buf;
	void *q;
	int ret;

	q = (void *)(size_t)queue;
	buf.buf_addr = msg;
	bufs[0] = &buf;

	ret = rte_rawdev_enqueue_buffers(dev_id, bufs, 1, q);
	if (ret < 0)
		return ret;
	if (ret != 1)
		return -EIO;

	return 0;
}

static int
cnxk_bphy_cgx_deq_msg(uint16_t dev_id, unsigned int queue, void **msg)
{
	struct rte_rawdev_buf *bufs[1];
	struct rte_rawdev_buf buf;
	void *q;
	int ret;

	q = (void *)(size_t)queue;
	bufs[0] = &buf;

	ret = rte_rawdev_dequeue_buffers(dev_id, bufs, 1, q);
	if (ret < 0)
		return ret;
	if (ret != 1)
		return -EIO;

	*msg = buf.buf_addr;

	return 0;
}

static int
cnxk_bphy_cgx_link_cond(uint16_t dev_id, unsigned int queue, int cond)
{
	int tries = 10, ret;

	do {
		struct cnxk_bphy_cgx_msg_link_info *link_info = NULL;
		struct cnxk_bphy_cgx_msg msg;

		msg.type = CNXK_BPHY_CGX_MSG_TYPE_GET_LINKINFO;
		ret = cnxk_bphy_cgx_enq_msg(dev_id, queue, &msg);
		if (ret)
			return ret;

		ret = cnxk_bphy_cgx_deq_msg(dev_id, queue, (void **)&link_info);
		if (ret)
			return ret;

		if (link_info->link_up == cond) {
			rte_free(link_info);
			break;
		}

		rte_free(link_info);
		rte_delay_ms(500);
	} while (--tries);

	if (tries)
		return !!cond;

	return -ETIMEDOUT;
}

static int
cnxk_bphy_cgx_get_supported_fec(uint16_t dev_id, unsigned int queue,
				enum cnxk_bphy_cgx_eth_link_fec *fec)
{
	struct cnxk_bphy_cgx_msg msg = {
		.type = CNXK_BPHY_CGX_MSG_TYPE_GET_SUPPORTED_FEC,
	};
	int ret;

	ret = cnxk_bphy_cgx_enq_msg(dev_id, queue, &msg);
	if (ret)
		return ret;

	return cnxk_bphy_cgx_deq_msg(dev_id, queue, (void **)&fec);
}

int
cnxk_bphy_cgx_dev_selftest(uint16_t dev_id)
{
	unsigned int queues, i;
	int ret;

	queues = rte_rawdev_queue_count(dev_id);
	if (queues == 0)
		return -ENODEV;

	ret = rte_rawdev_start(dev_id);
	if (ret)
		return ret;

	for (i = 0; i < queues; i++) {
		struct cnxk_bphy_cgx_msg_set_link_state link_state;
		enum cnxk_bphy_cgx_eth_link_fec fec;
		struct cnxk_bphy_cgx_msg msg;
		unsigned int descs;

		ret = rte_rawdev_queue_conf_get(dev_id, i, &descs,
						sizeof(descs));
		if (ret)
			break;
		if (descs != 1) {
			RTE_LOG(ERR, PMD, "Wrong number of descs reported\n");
			ret = -ENODEV;
			break;
		}

		RTE_LOG(INFO, PMD, "Testing queue %d\n", i);

		/* stop rx/tx */
		msg.type = CNXK_BPHY_CGX_MSG_TYPE_STOP_RXTX;
		ret = cnxk_bphy_cgx_enq_msg(dev_id, i, &msg);
		if (ret) {
			RTE_LOG(ERR, PMD, "Failed to stop rx/tx\n");
			break;
		}

		/* start rx/tx */
		msg.type = CNXK_BPHY_CGX_MSG_TYPE_START_RXTX;
		ret = cnxk_bphy_cgx_enq_msg(dev_id, i, &msg);
		if (ret) {
			RTE_LOG(ERR, PMD, "Failed to start rx/tx\n");
			break;
		}

		/* set link down */
		link_state.state = false;
		msg.type = CNXK_BPHY_CGX_MSG_TYPE_SET_LINK_STATE;
		msg.data = &link_state;
		ret = cnxk_bphy_cgx_enq_msg(dev_id, i, &msg);
		if (ret) {
			RTE_LOG(ERR, PMD, "Failed to set link down\n");
			break;
		}

		ret = cnxk_bphy_cgx_link_cond(dev_id, i, 0);
		if (ret != 0)
			RTE_LOG(ERR, PMD,
				"Timed out waiting for a link down\n");

		/* set link up */
		link_state.state = true;
		msg.type = CNXK_BPHY_CGX_MSG_TYPE_SET_LINK_STATE;
		msg.data = &link_state;
		ret = cnxk_bphy_cgx_enq_msg(dev_id, i, &msg);
		if (ret) {
			RTE_LOG(ERR, PMD, "Failed to set link up\n");
			break;
		}

		ret = cnxk_bphy_cgx_link_cond(dev_id, i, 1);
		if (ret != 1)
			RTE_LOG(ERR, PMD, "Timed out waiting for a link up\n");

		/* enable internal loopback */
		msg.type = CNXK_BPHY_CGX_MSG_TYPE_INTLBK_ENABLE;
		ret = cnxk_bphy_cgx_enq_msg(dev_id, i, &msg);
		if (ret) {
			RTE_LOG(ERR, PMD, "Failed to enable internal lbk\n");
			break;
		}

		/* disable internal loopback */
		msg.type = CNXK_BPHY_CGX_MSG_TYPE_INTLBK_DISABLE;
		ret = cnxk_bphy_cgx_enq_msg(dev_id, i, &msg);
		if (ret) {
			RTE_LOG(ERR, PMD, "Failed to disable internal lbk\n");
			break;
		}

		/* enable ptp */
		msg.type = CNXK_BPHY_CGX_MSG_TYPE_PTP_RX_ENABLE;
		ret = cnxk_bphy_cgx_enq_msg(dev_id, i, &msg);
		/* ptp not available on RPM */
		if (ret < 0 && ret != -ENOTSUP) {
			RTE_LOG(ERR, PMD, "Failed to enable ptp\n");
			break;
		}
		ret = 0;

		/* disable ptp */
		msg.type = CNXK_BPHY_CGX_MSG_TYPE_PTP_RX_DISABLE;
		ret = cnxk_bphy_cgx_enq_msg(dev_id, i, &msg);
		/* ptp not available on RPM */
		if (ret < 0 && ret != -ENOTSUP) {
			RTE_LOG(ERR, PMD, "Failed to disable ptp\n");
			break;
		}
		ret = 0;

		ret = cnxk_bphy_cgx_get_supported_fec(dev_id, i, &fec);
		if (ret) {
			RTE_LOG(ERR, PMD, "Failed to get supported FEC\n");
			break;
		}

		/* set supported fec */
		msg.type = CNXK_BPHY_CGX_MSG_TYPE_SET_FEC;
		msg.data = &fec;
		ret = cnxk_bphy_cgx_enq_msg(dev_id, i, &msg);
		if (ret) {
			RTE_LOG(ERR, PMD, "Failed to set FEC to %d\n", fec);
			break;
		}

		/* disable fec */
		fec = CNXK_BPHY_CGX_ETH_LINK_FEC_NONE;
		msg.type = CNXK_BPHY_CGX_MSG_TYPE_SET_FEC;
		msg.data = &fec;
		ret = cnxk_bphy_cgx_enq_msg(dev_id, i, &msg);
		if (ret) {
			RTE_LOG(ERR, PMD, "Failed to disable FEC\n");
			break;
		}
	}

	rte_rawdev_stop(dev_id);

	return ret;
}
