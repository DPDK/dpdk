/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2026 Marvell.
 */

#include <errno.h>
#include <inttypes.h>
#include <string.h>

#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_malloc.h>

#include "otx_ep_common.h"
#include "otx2_ep_vf.h"
#include "cn20k_ep_vf.h"
#include "cn20k_ep_mbox.h"

#define MBOX_RSP_TIMEOUT_MS     10000
#define MBOX_CMD_TIMEOUT_US     1000000

#define CN20K_MBOX_MSGS_OFFSET  RTE_ALIGN(sizeof(struct cn20k_mbox_hdr), MBOX_MSG_ALIGN)

static int
cn20k_mbox_wait_wr_cmd_out(struct otx_ep_device *otx_ep, uint16_t offset)
{
	uint64_t timeout = (MBOX_CMD_TIMEOUT_US * rte_get_timer_hz()) / 1000000;
	uint64_t start = rte_get_timer_cycles();

	while (oct_ep_read64(otx_ep->hw_addr + CN20K_SDP_RMT_VFX_MBOX_WR_CMD_CTL) &
	       CN20K_MBOX_WR_CMD_OUT) {
		if (rte_get_timer_cycles() - start >= timeout) {
			otx_ep_err("Mbox write timeout waiting for CMD_OUT clear (offset %u)",
				   offset);
			return -ETIMEDOUT;
		}
		rte_pause();
	}

	return 0;
}

static int
cn20k_mbox_wait_rd_cmd_out(struct otx_ep_device *otx_ep, uint16_t offset)
{
	uint64_t timeout = (MBOX_CMD_TIMEOUT_US * rte_get_timer_hz()) / 1000000;
	uint64_t start = rte_get_timer_cycles();

	while (oct_ep_read64(otx_ep->hw_addr + CN20K_SDP_RMT_VFX_MBOX_RD_CMD_CTL) &
	       CN20K_MBOX_RD_CMD_OUT) {
		if (rte_get_timer_cycles() - start >= timeout) {
			otx_ep_err("Mbox read timeout waiting for CMD_OUT clear (offset %u)",
				   offset);
			return -ETIMEDOUT;
		}
		rte_pause();
	}

	return 0;
}

static int
otx_ep_cn20k_mbox_write(struct otx_ep_device *otx_ep, uint16_t offset, void *buf, size_t len)
{
	uint64_t *data = (uint64_t *)buf;
	size_t num_words = (len + 7) / 8;
	uint64_t ctl;
	size_t i;
	int ret;

	otx_ep_dbg("CN20K: mbox_write called: offset=%u, len=%zu", offset, len);

	if (offset & 0x7) {
		otx_ep_err("Mailbox offset 0x%x not 8-byte aligned", offset);
		return -EINVAL;
	}

	ret = cn20k_mbox_wait_wr_cmd_out(otx_ep, offset);
	if (ret)
		return ret;

	oct_ep_write64(offset, otx_ep->hw_addr + CN20K_SDP_RMT_VFX_MBOX_WR_CMD_OFFSET);

	for (i = 0; i < num_words; i++) {
		uint64_t timeout = (MBOX_CMD_TIMEOUT_US * rte_get_timer_hz()) / 1000000;
		uint64_t start = rte_get_timer_cycles();

		oct_ep_write64(data[i], otx_ep->hw_addr + CN20K_SDP_RMT_VFX_MBOX_WR_CMD_DATA);

		do {
			ctl = oct_ep_read64(otx_ep->hw_addr + CN20K_SDP_RMT_VFX_MBOX_WR_CMD_CTL);
			if (rte_get_timer_cycles() - start >= timeout) {
				otx_ep_err("Mbox write timeout waiting for CMD_DONE");
				return -ETIMEDOUT;
			}
			rte_pause();
		} while (!(ctl & CN20K_MBOX_WR_CMD_DONE));

		if (ctl & CN20K_MBOX_WR_CMD_ERR) {
			otx_ep_err("Mbox write error at offset %u", offset + (uint16_t)(i * 8));
			oct_ep_write64(CN20K_MBOX_WR_CMD_ERR,
				       otx_ep->hw_addr + CN20K_SDP_RMT_VFX_MBOX_WR_CMD_CTL);
			return -EIO;
		}

		if (ctl & CN20K_MBOX_WR_DROP) {
			otx_ep_err("Mbox write dropped at offset %u", offset + (uint16_t)(i * 8));
			oct_ep_write64(CN20K_MBOX_WR_DROP,
				       otx_ep->hw_addr + CN20K_SDP_RMT_VFX_MBOX_WR_CMD_CTL);
			return -EIO;
		}
	}

	return 0;
}

static int
otx_ep_cn20k_mbox_read(struct otx_ep_device *otx_ep, uint16_t offset, void *buf, size_t len)
{
	size_t i, num_words = (len + 7) / 8;
	uint64_t *data = (uint64_t *)buf;
	uint64_t ctl;
	int ret;

	otx_ep_dbg("CN20K: mbox_read called: offset=%u, len=%zu", offset, len);

	if (offset & 0x7) {
		otx_ep_err("Mailbox offset 0x%x not 8-byte aligned", offset);
		return -EINVAL;
	}

	ret = cn20k_mbox_wait_rd_cmd_out(otx_ep, offset);
	if (ret)
		return ret;

	oct_ep_write64(offset, otx_ep->hw_addr + CN20K_SDP_RMT_VFX_MBOX_RD_CMD_OFFSET);

	for (i = 0; i < num_words; i++) {
		uint64_t start = rte_get_timer_cycles();
		uint64_t timeout = (MBOX_CMD_TIMEOUT_US * rte_get_timer_hz()) / 1000000;

		rte_atomic_thread_fence(rte_memory_order_release);
		if (i == 0) {
			/* Dummy write, loads Offset = 0 data into CSR. Value doesn't matter,
			 * it gets ignored by hardware.
			 */
			oct_ep_write64(0, otx_ep->hw_addr + CN20K_SDP_RMT_VFX_MBOX_RD_CMD_DATA);
			rte_atomic_thread_fence(rte_memory_order_acquire);
			do {
				ctl = oct_ep_read64(otx_ep->hw_addr +
						    CN20K_SDP_RMT_VFX_MBOX_RD_CMD_CTL);
				if (rte_get_timer_cycles() - start >= timeout) {
					otx_ep_err("Mbox read timeout waiting for CMD_DONE");
					return -ETIMEDOUT;
				}
				rte_pause();
			} while (!(ctl & CN20K_MBOX_RD_CMD_DONE));
		}

		rte_atomic_thread_fence(rte_memory_order_acquire);
		data[i] = oct_ep_read64(otx_ep->hw_addr + CN20K_SDP_RMT_VFX_MBOX_RD_CMD_DATA);

		do {
			rte_atomic_thread_fence(rte_memory_order_acquire);
			ctl = oct_ep_read64(otx_ep->hw_addr + CN20K_SDP_RMT_VFX_MBOX_RD_CMD_CTL);
			if (rte_get_timer_cycles() - start >= timeout) {
				otx_ep_err("Mbox read timeout waiting for CMD_DONE");
				return -ETIMEDOUT;
			}
			rte_pause();
		} while (!(ctl & CN20K_MBOX_RD_CMD_DONE));

		if (ctl & CN20K_MBOX_RD_CMD_ERR) {
			otx_ep_err("Mbox read error at offset %u", offset + (uint16_t)(i * 8));
			oct_ep_write64(CN20K_MBOX_RD_CMD_ERR,
				       otx_ep->hw_addr + CN20K_SDP_RMT_VFX_MBOX_RD_CMD_CTL);
			data[i] = 0;
			return -EIO;
		}
	}

	ret = cn20k_mbox_wait_rd_cmd_out(otx_ep, offset);

	return 0;
}

static int
otx_ep_cn20k_mbox_msg_send(struct otx_ep_cn20k_mbox_priv *mbox)
{
	struct otx_ep_device *otx_ep = mbox->otx_ep;
	struct cn20k_mbox_hdr tx_hdr, rx_hdr_zero = {0};
	int ret = 0;

	tx_hdr.msg_size = mbox->msg_size;
	tx_hdr.num_msgs = mbox->num_msgs;
	tx_hdr.sig = OCTEP_CN20K_MBOX_REQ_SIG;

	rte_atomic_thread_fence(rte_memory_order_release);

	if (mbox->mbase != mbox->hwbase) {
		ret = otx_ep_cn20k_mbox_write(otx_ep, (uint16_t)mbox->tx_start, &tx_hdr,
					      sizeof(tx_hdr));
		if (ret) {
			otx_ep_err("Failed to write TX header");
			goto err;
		}

		rte_atomic_thread_fence(rte_memory_order_release);
		ret = otx_ep_cn20k_mbox_write(otx_ep,
					      (uint16_t)(mbox->tx_start + CN20K_MBOX_MSGS_OFFSET),
					      (uint8_t *)mbox->mbase + mbox->tx_start +
					      CN20K_MBOX_MSGS_OFFSET, mbox->msg_size);
		if (ret) {
			otx_ep_err("Failed to write message payload");
			goto err;
		}

		rte_atomic_thread_fence(rte_memory_order_release);
		ret = otx_ep_cn20k_mbox_write(otx_ep, (uint16_t)mbox->rx_start, &rx_hdr_zero,
					      sizeof(rx_hdr_zero));
		if (ret) {
			otx_ep_err("Failed to clear RX header");
			goto err;
		}
	}

	rte_atomic_thread_fence(rte_memory_order_release);
	oct_ep_write64(MBOX_DOWN_MSG, otx_ep->hw_addr + mbox->trigger);

err:
	return ret;
}

static int
otx_ep_cn20k_mbox_check_rsp_msgs(struct otx_ep_cn20k_mbox_priv *mbox)
{
	struct otx_ep_device *otx_ep = mbox->otx_ep;
	struct cn20k_mbox_hdr rx_hdr;
	int ret;

	ret = otx_ep_cn20k_mbox_read(otx_ep, (uint16_t)mbox->rx_start, &rx_hdr, sizeof(rx_hdr));
	if (ret) {
		otx_ep_err("Failed to read RX header");
		return ret;
	}

	if (rx_hdr.num_msgs == 0)
		return 0;

	if (rx_hdr.msg_size > mbox->rx_size - CN20K_MBOX_MSGS_OFFSET) {
		otx_ep_err("RX message size %u exceeds buffer (%u)",
			   (uint32_t)rx_hdr.msg_size,
			   (uint32_t)(mbox->rx_size - CN20K_MBOX_MSGS_OFFSET));
		return -EINVAL;
	}

	if (mbox->mbase != mbox->hwbase) {
		ret = otx_ep_cn20k_mbox_read(otx_ep,
					     (uint16_t)(mbox->rx_start + CN20K_MBOX_MSGS_OFFSET),
					     (uint8_t *)mbox->mbase + mbox->rx_start +
					     CN20K_MBOX_MSGS_OFFSET, rx_hdr.msg_size);
		if (ret) {
			otx_ep_err("Failed to read response messages");
			return ret;
		}
	}

	mbox->msgs_acked = rx_hdr.num_msgs;

	return 0;
}

static int
otx_ep_cn20k_mbox_wait_for_rsp(struct otx_ep_cn20k_mbox_priv *mbox)
{
	uint64_t start = rte_get_timer_cycles();
	uint64_t timeout = (MBOX_RSP_TIMEOUT_MS * rte_get_timer_hz()) / 1000;

	while (rte_get_timer_cycles() - start < timeout) {
		if (mbox->num_msgs == mbox->msgs_acked)
			return 0;
		rte_delay_us(1000);
		otx_ep_cn20k_mbox_check_rsp_msgs(mbox);
	}

	otx_ep_err("Mbox response timeout");

	return -ETIMEDOUT;
}

static void *
otx_ep_cn20k_mbox_alloc_msg(struct otx_ep_cn20k_mbox_priv *mbox, int size, int size_rsp)
{
	struct otx_ep_cn20k_mbox_msghdr *msghdr;

	if ((uint32_t)(mbox->msg_size + size) > mbox->tx_size - CN20K_MBOX_MSGS_OFFSET) {
		otx_ep_err("Mailbox message size exceeds limit");
		return NULL;
	}

	msghdr = (struct otx_ep_cn20k_mbox_msghdr *)((uint8_t *)mbox->mbase + mbox->tx_start +
					      CN20K_MBOX_MSGS_OFFSET + mbox->msg_size);

	memset(msghdr, 0, size);

	msghdr->ver = OCTEP_CN20K_MBOX_VERSION;
	mbox->msg_size += size;
	mbox->rsp_size += size_rsp;
	mbox->num_msgs++;
	msghdr->next_msgoff = mbox->msg_size + CN20K_MBOX_MSGS_OFFSET;

	return msghdr;
}

static struct otx_ep_cn20k_ready_msg_req *
otx_ep_cn20k_mbox_alloc_msg_ready(struct otx_ep_device *otx_ep)
{
	struct otx_ep_cn20k_mbox *cn20k_mbox = otx_ep->mbox_info;
	struct otx_ep_cn20k_mbox_priv *mbox = &cn20k_mbox->mbox;
	struct otx_ep_cn20k_ready_msg_req *req;

	req = otx_ep_cn20k_mbox_alloc_msg(mbox, sizeof(*req),
					  sizeof(struct otx_ep_cn20k_ready_msg_rsp));
	if (!req)
		return NULL;

	req->hdr.id = OCTEP_CN20K_MBOX_CMD_VF_READY;
	req->hdr.sig = OCTEP_CN20K_MBOX_REQ_SIG;
	req->hdr.pcifunc = 0;

	return req;
}

static int
otx_ep_cn20k_mbox_setup(struct otx_ep_cn20k_mbox_priv *mbox, int direction)
{
	switch (direction) {
	case MBOX_DIR_HOSTVF_HOSTPF:
		mbox->tx_start = CN20K_MBOX_DOWN_RX_START;
		mbox->tx_size = CN20K_MBOX_DOWN_RX_SIZE;
		mbox->rx_start = CN20K_MBOX_DOWN_TX_START;
		mbox->rx_size = CN20K_MBOX_DOWN_TX_SIZE;
		mbox->trigger = CN20K_SDP_RMT_VFX_MBOX_SEND_INT;
		break;
	case MBOX_DIR_HOSTVF_HOSTPF_UP:
		mbox->tx_start = CN20K_MBOX_UP_TX_START;
		mbox->tx_size = CN20K_MBOX_UP_TX_SIZE;
		mbox->rx_start = CN20K_MBOX_UP_RX_START;
		mbox->rx_size = CN20K_MBOX_UP_RX_SIZE;
		mbox->trigger = CN20K_SDP_RMT_VFX_MBOX_SEND_INT;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int
otx_ep_cn20k_mbox_bbuf_init(struct otx_ep_cn20k_mbox *mbox_info)
{
	mbox_info->bbuf_base = rte_zmalloc("cn20k_mbox_bbuf", CN20K_VF_MBOX_SIZE,
					   RTE_CACHE_LINE_SIZE);
	if (!mbox_info->bbuf_base)
		return -ENOMEM;

	mbox_info->mbox.mbase = mbox_info->bbuf_base;
	mbox_info->mbox_up.mbase = mbox_info->bbuf_base;

	return 0;
}

static int
otx_ep_cn20k_setup_mbox(struct otx_ep_device *otx_ep)
{
	struct otx_ep_cn20k_mbox *mbox_info;
	int ret;

	if (otx_ep->mbox_info)
		return 0;

	mbox_info = rte_zmalloc("otx_ep_mbox", sizeof(*mbox_info), RTE_CACHE_LINE_SIZE);
	if (!mbox_info) {
		otx_ep_err("MBOX structure allocation failed");
		return -ENOMEM;
	}

	mbox_info->otx_ep = otx_ep;
	mbox_info->mbox.otx_ep = otx_ep;
	otx_ep->mbox_info = mbox_info;
	mbox_info->mbox_up.otx_ep = otx_ep;

	ret = otx_ep_cn20k_mbox_setup(&mbox_info->mbox, MBOX_DIR_HOSTVF_HOSTPF);
	if (ret) {
		otx_ep_err("Failed to setup downward mailbox");
		goto free_mbox;
	}

	ret = otx_ep_cn20k_mbox_setup(&mbox_info->mbox_up, MBOX_DIR_HOSTVF_HOSTPF_UP);
	if (ret) {
		otx_ep_err("Failed to setup upward mailbox");
		goto free_mbox;
	}

	ret = otx_ep_cn20k_mbox_bbuf_init(mbox_info);
	if (ret) {
		otx_ep_err("Failed to init bounce buffer");
		goto free_mbox;
	}

	mbox_info->mbox.hwbase = NULL;
	mbox_info->mbox_up.hwbase = NULL;

	otx_ep_dbg("CN20K VF mailbox initialized (CMD register based)");

	return 0;

free_mbox:
	if (mbox_info->bbuf_base)
		rte_free(mbox_info->bbuf_base);
	rte_free(mbox_info);
	otx_ep->mbox_info = NULL;
	return ret;
}

static void
otx_ep_cn20k_delete_mbox(struct otx_ep_device *otx_ep)
{
	struct otx_ep_cn20k_mbox *mbox_info = otx_ep->mbox_info;

	if (!mbox_info)
		return;

	if (mbox_info->bbuf_base)
		rte_free(mbox_info->bbuf_base);

	rte_free(mbox_info);
	otx_ep->mbox_info = NULL;

	otx_ep_dbg("CN20K VF mailbox cleaned up");
}

int
otx_ep_cn20k_mbox_send_cmd(struct otx_ep_device *otx_ep, union otx_ep_mbox_word cmd,
			   union otx_ep_mbox_word *rsp)
{
	struct otx_ep_cn20k_mbox *mbox_info = otx_ep->mbox_info;
	struct otx_ep_cn20k_mbox_priv *mbox;
	struct otx_ep_cn20k_mbox_msghdr *msghdr;
	union otx_ep_mbox_word *msg_data;
	int ret;

	if (!mbox_info)
		return -EINVAL;

	mbox = &mbox_info->mbox;

	mbox->msg_size = 0;
	mbox->rsp_size = 0;
	mbox->num_msgs = 0;
	mbox->msgs_acked = 0;

	msghdr = otx_ep_cn20k_mbox_alloc_msg(mbox,
					     sizeof(struct otx_ep_cn20k_mbox_msghdr) + sizeof(cmd),
					     sizeof(struct otx_ep_cn20k_mbox_msghdr) +
					     sizeof(*rsp));
	if (!msghdr)
		return -ENOMEM;

	msghdr->id = cmd.s.opcode;
	msghdr->sig = OCTEP_CN20K_MBOX_REQ_SIG;
	msghdr->ver = OCTEP_CN20K_MBOX_VERSION;
	msghdr->pcifunc = 0;

	msg_data = (union otx_ep_mbox_word *)(msghdr + 1);
	*msg_data = cmd;

	ret = otx_ep_cn20k_mbox_msg_send(mbox);
	if (ret)
		return ret;

	ret = otx_ep_cn20k_mbox_wait_for_rsp(mbox);
	if (ret)
		return ret;

	ret = otx_ep_cn20k_mbox_check_rsp_msgs(mbox);
	if (ret)
		return ret;

	msghdr = (struct otx_ep_cn20k_mbox_msghdr *)((uint8_t *)mbox->mbase + mbox->rx_start +
					      CN20K_MBOX_MSGS_OFFSET);
	msg_data = (union otx_ep_mbox_word *)(msghdr + 1);
	*rsp = *msg_data;

	if (msghdr->rc) {
		otx_ep_err("Mailbox command failed: rc=%d", msghdr->rc);
		return msghdr->rc;
	}

	return 0;
}

int
otx_ep_cn20k_mbox_bulk_read(struct otx_ep_device *otx_ep, enum otx_ep_mbox_opcode opcode,
			    uint8_t *data, int32_t max_size, int32_t *size)
{
	union otx_ep_mbox_word cmd = {0};
	union otx_ep_mbox_word rsp;
	int data_len, tmp_len, read_cnt, i, ret;

	if (!otx_ep->mbox_info || !data || !size || max_size <= 0)
		return -EINVAL;

	rte_spinlock_lock(&otx_ep->mbox_lock);

	cmd.s_data.opcode = opcode;
	cmd.s_data.frag = 0;
	ret = otx_ep_cn20k_mbox_send_cmd(otx_ep, cmd, &rsp);
	if (ret) {
		otx_ep_err("CN20K mbox bulk read request failed");
		goto unlock;
	}

	memcpy(&data_len, rsp.s_data.data, sizeof(data_len));
	tmp_len = data_len;
	if (data_len <= 0 || data_len > max_size) {
		otx_ep_err("CN20K mbox bulk read invalid length %d", data_len);
		ret = -EINVAL;
		goto unlock;
	}

	otx_ep->mbox_data_index = 0;
	cmd.u64 = 0;
	cmd.s_data.opcode = opcode;
	cmd.s_data.frag = 1;
	while (data_len) {
		ret = otx_ep_cn20k_mbox_send_cmd(otx_ep, cmd, &rsp);
		if (ret) {
			otx_ep_err("CN20K mbox bulk read fragment failed");
			otx_ep->mbox_data_index = 0;
			memset(otx_ep->mbox_data_buf, 0, MBOX_MAX_DATA_BUF_SIZE);
			goto unlock;
		}
		if (data_len > OTX_EP_MBOX_MAX_DATA_SIZE) {
			data_len -= OTX_EP_MBOX_MAX_DATA_SIZE;
			read_cnt = OTX_EP_MBOX_MAX_DATA_SIZE;
		} else {
			read_cnt = data_len;
			data_len = 0;
		}
		if (otx_ep->mbox_data_index + read_cnt > MBOX_MAX_DATA_BUF_SIZE) {
			otx_ep_err("CN20K mbox bulk read buffer overflow");
			otx_ep->mbox_data_index = 0;
			memset(otx_ep->mbox_data_buf, 0, MBOX_MAX_DATA_BUF_SIZE);
			ret = -EINVAL;
			goto unlock;
		}
		for (i = 0; i < read_cnt; i++) {
			otx_ep->mbox_data_buf[otx_ep->mbox_data_index] =
				rsp.s_data.data[i];
			otx_ep->mbox_data_index++;
		}
		cmd.u64 = 0;
		cmd.s_data.opcode = opcode;
		cmd.s_data.frag = 1;
	}

	memcpy(data, otx_ep->mbox_data_buf, tmp_len);
	*size = tmp_len;
	otx_ep->mbox_data_index = 0;
	memset(otx_ep->mbox_data_buf, 0, MBOX_MAX_DATA_BUF_SIZE);

unlock:
	rte_spinlock_unlock(&otx_ep->mbox_lock);
	return ret;
}

static void
otx_ep_cn20k_mbox_intr_handler(void *param)
{
	struct rte_eth_dev *eth_dev = (struct rte_eth_dev *)param;
	struct otx_ep_device *otx_ep = (struct otx_ep_device *)eth_dev->data->dev_private;
	uint64_t intr_status;

	/* Read and clear interrupt */
	intr_status = oct_ep_read64(otx_ep->hw_addr + CN20K_SDP_RMT_VFX_MBOX_RINT);
	if (intr_status & CN20K_MBOX_INTR) {
		/* Clear interrupt (W1C) */
		oct_ep_write64(CN20K_MBOX_INTR, otx_ep->hw_addr + CN20K_SDP_RMT_VFX_MBOX_RINT);
	}
	if (intr_status & ~CN20K_MBOX_INTR)
		otx_ep_err("Unexpected mailbox interrupt bits: 0x%" PRIX64,
			   intr_status & ~CN20K_MBOX_INTR);
}

int
otx_ep_cn20k_mbox_init(struct rte_eth_dev *eth_dev)
{
	struct rte_pci_device *pdev = RTE_CLASS_TO_BUS_DEVICE(eth_dev, *pdev);
	struct otx_ep_device *otx_ep = eth_dev->data->dev_private;
	int rc;

	rc = otx_ep_cn20k_setup_mbox(otx_ep);
	if (rc) {
		otx_ep_err("Failed to setup CN20K PF-VF mailbox");
		return rc;
	}

	rte_intr_callback_register(pdev->intr_handle, otx_ep_cn20k_mbox_intr_handler,
				   (void *)eth_dev);

	rc = rte_intr_enable(pdev->intr_handle);
	if (rc != 0) {
		otx_ep_err("rte_intr_enable failed: %d", rc);
		return rc;
	}

	oct_ep_write64(CN20K_MBOX_INTR, otx_ep->hw_addr + CN20K_SDP_RMT_VFX_MBOX_RINT_ENA_W1S);

	otx_ep_dbg("CN20K mailbox initialized successfully");

	return 0;
}

void
otx_ep_cn20k_mbox_uninit(struct rte_eth_dev *eth_dev)
{
	struct rte_pci_device *pdev = RTE_CLASS_TO_BUS_DEVICE(eth_dev, *pdev);
	struct otx_ep_device *otx_ep = eth_dev->data->dev_private;

	oct_ep_write64(CN20K_MBOX_INTR, otx_ep->hw_addr + CN20K_SDP_RMT_VFX_MBOX_RINT_ENA_W1C);

	rte_intr_disable(pdev->intr_handle);
	rte_intr_callback_unregister(pdev->intr_handle, otx_ep_cn20k_mbox_intr_handler,
				     (void *)eth_dev);

	otx_ep_cn20k_delete_mbox(otx_ep);
}

int
otx_ep_cn20k_mbox_send_ready(struct otx_ep_device *otx_ep)
{
	struct otx_ep_cn20k_mbox *cn20k_mbox;
	struct otx_ep_cn20k_mbox_priv *mbox;
	struct otx_ep_cn20k_ready_msg_req *req;
	struct otx_ep_cn20k_ready_msg_rsp *rsp;
	int ret;

	if (!otx_ep->mbox_info) {
		otx_ep_err("CN20K mailbox not initialized");
		return -EINVAL;
	}

	cn20k_mbox = otx_ep->mbox_info;
	mbox = &cn20k_mbox->mbox;

	mbox->msg_size = 0;
	mbox->num_msgs = 0;
	mbox->msgs_acked = 0;

	req = otx_ep_cn20k_mbox_alloc_msg_ready(otx_ep);
	if (!req) {
		otx_ep_err("CN20K: Failed to allocate VF_READY message");
		return -ENOMEM;
	}

	otx_ep_dbg("CN20K: Allocated VF_READY request (id=0x%x, sig=0x%x, ver=0x%x)", req->hdr.id,
		   req->hdr.sig, req->hdr.ver);

	ret = otx_ep_cn20k_mbox_msg_send(mbox);
	if (ret) {
		otx_ep_err("CN20K: Failed to send VF_READY request: %d", ret);
		return ret;
	}

	otx_ep_dbg("CN20K: VF_READY request sent to PF");

	ret = otx_ep_cn20k_mbox_wait_for_rsp(mbox);
	if (ret) {
		otx_ep_err("CN20K: Timeout waiting for VF_READY response: %d", ret);
		return ret;
	}

	ret = otx_ep_cn20k_mbox_check_rsp_msgs(mbox);
	if (ret) {
		otx_ep_err("CN20K: Failed to read VF_READY response: %d", ret);
		return ret;
	}

	rsp = (struct otx_ep_cn20k_ready_msg_rsp *)((uint8_t *)mbox->mbase +
						    mbox->rx_start + CN20K_MBOX_MSGS_OFFSET);

	if (rsp->hdr.sig != OCTEP_CN20K_MBOX_RSP_SIG) {
		otx_ep_err("CN20K: Invalid VF_READY response signature: 0x%x (expected 0x%x)",
			   rsp->hdr.sig, OCTEP_CN20K_MBOX_RSP_SIG);
		return -EINVAL;
	}

	if (rsp->hdr.rc != 0) {
		otx_ep_err("CN20K: VF_READY rejected by PF: rc=%d", rsp->hdr.rc);
		return rsp->hdr.rc;
	}

	otx_ep_dbg("CN20K: VF_READY acknowledged by PF (sig=0x%x, rc=%d)", rsp->hdr.sig,
		   rsp->hdr.rc);

	return 0;
}

int
otx_ep_cn20k_mbox_alloc_sdp_rings(struct otx_ep_device *otx_ep, uint16_t nr_rings)
{
	struct otx_ep_cn20k_mbox *cn20k_mbox = otx_ep->mbox_info;
	struct otx_ep_cn20k_mbox_priv *mbox;
	struct sdp_rings_alloc_req *req;
	struct sdp_rings_alloc_rsp *rsp;
	struct otx_ep_cn20k_mbox_msghdr *msghdr;
	int ret;

	if (!cn20k_mbox) {
		otx_ep_err("CN20K mailbox not initialized");
		return -EINVAL;
	}

	if (nr_rings == 0) {
		otx_ep_err("Invalid nr_rings=0");
		return -EINVAL;
	}

	mbox = &cn20k_mbox->mbox;

	mbox->msg_size = 0;
	mbox->rsp_size = 0;
	mbox->num_msgs = 0;
	mbox->msgs_acked = 0;

	req = otx_ep_cn20k_mbox_alloc_msg(mbox, sizeof(*req), sizeof(struct sdp_rings_alloc_rsp));
	if (!req) {
		otx_ep_err("Failed to allocate SDP_RING_ALLOC message");
		return -ENOMEM;
	}

	req->hdr.id = MBOX_MSG_SDP_RING_ALLOC;
	req->hdr.sig = OCTEP_CN20K_MBOX_REQ_SIG;
	req->hdr.pcifunc = 0;
	req->nr_rings = nr_rings;
	memset(req->rsvd, 0, sizeof(req->rsvd));

	otx_ep_dbg("VF requesting %u SDP rings from PF", nr_rings);

	ret = otx_ep_cn20k_mbox_msg_send(mbox);
	if (ret) {
		otx_ep_err("Failed to send SDP_RING_ALLOC message: %d", ret);
		return ret;
	}

	ret = otx_ep_cn20k_mbox_wait_for_rsp(mbox);
	if (ret) {
		otx_ep_err("Failed to get response: %d", ret);
		return ret;
	}

	msghdr = (struct otx_ep_cn20k_mbox_msghdr *)((uint8_t *)mbox->mbase +
						     mbox->rx_start + CN20K_MBOX_MSGS_OFFSET);
	if (msghdr->rc) {
		otx_ep_err("PF returned error for SDP_RING_ALLOC: %d", msghdr->rc);
		return msghdr->rc;
	}

	rsp = (struct sdp_rings_alloc_rsp *)msghdr;

	if (rsp->count == 0) {
		otx_ep_err("PF allocated 0 SDP rings");
		return -ENOENT;
	}

	return rsp->count;
}

int
otx_ep_cn20k_mbox_free_sdp_rings(struct otx_ep_device *otx_ep, uint16_t ring, uint8_t all)
{
	struct otx_ep_cn20k_mbox *mbox_info = otx_ep->mbox_info;
	struct otx_ep_cn20k_mbox_msghdr *msghdr;
	struct otx_ep_cn20k_mbox_priv *mbox;
	struct sdp_rings_free_req *req;
	int ret;

	if (!mbox_info) {
		otx_ep_err("CN20K mailbox not initialized");
		return -EINVAL;
	}

	mbox = &mbox_info->mbox;
	mbox->msg_size = 0;
	mbox->rsp_size = 0;
	mbox->num_msgs = 0;
	mbox->msgs_acked = 0;

	req = otx_ep_cn20k_mbox_alloc_msg(mbox, sizeof(*req), sizeof(struct otx_ep_cn20k_msg_rsp));
	if (!req) {
		otx_ep_err("Failed to allocate SDP_RING_FREE message");
		return -ENOMEM;
	}

	req->hdr.id = MBOX_MSG_SDP_RING_FREE;
	req->hdr.sig = OCTEP_CN20K_MBOX_REQ_SIG;
	req->hdr.pcifunc = 0;
	req->ring = ring;
	req->all = all;

	if (all)
		otx_ep_dbg("VF requesting to free all SDP rings");
	else
		otx_ep_dbg("VF requesting to free SDP ring %u", ring);

	ret = otx_ep_cn20k_mbox_msg_send(mbox);
	if (ret) {
		otx_ep_err("Failed to send SDP_RING_FREE message: %d", ret);
		return ret;
	}

	ret = otx_ep_cn20k_mbox_wait_for_rsp(mbox);
	if (ret) {
		otx_ep_err("Failed to get rsp for SDP_RING_FREE message: %d", ret);
		return ret;
	}

	msghdr = (struct otx_ep_cn20k_mbox_msghdr *)((uint8_t *)mbox->mbase +
						    mbox->rx_start + CN20K_MBOX_MSGS_OFFSET);
	if (msghdr->rc) {
		otx_ep_err("PF returned error for SDP_RING_FREE: %d", msghdr->rc);
		return msghdr->rc;
	}

	return 0;
}
