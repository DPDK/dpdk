/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include <fcntl.h>
#include <inttypes.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "roc_api.h"
#include "roc_priv.h"

/* PCI Extended capability ID */
#define ROC_PCI_EXT_CAP_ID_SRIOV 0x10 /* SRIOV cap */

/* Single Root I/O Virtualization */
#define ROC_PCI_SRIOV_TOTAL_VF 0x0e /* Total VFs */

/* VF Mbox handler thread name */
#define MBOX_HANDLER_NAME_MAX_LEN RTE_THREAD_INTERNAL_NAME_SIZE

/* VF interrupt message pending bits - mbox or flr */
#define ROC_DEV_MBOX_PEND BIT_ULL(0)
#define ROC_DEV_FLR_PEND  BIT_ULL(1)
static void *
mbox_mem_map(off_t off, size_t size)
{
	void *va = MAP_FAILED;
	int mem_fd;

	if (size <= 0 || !off) {
		plt_err("Invalid mbox area off 0x%lx size %lu", off, size);
		goto error;
	}

	mem_fd = open("/dev/mem", O_RDWR);
	if (mem_fd < 0)
		goto error;

	va = plt_mmap(NULL, size, PLT_PROT_READ | PLT_PROT_WRITE,
		      PLT_MAP_SHARED, mem_fd, off);
	close(mem_fd);

	if (va == MAP_FAILED)
		plt_err("Failed to mmap sz=0x%zx, fd=%d, off=%jd", size, mem_fd,
			(intmax_t)off);
error:
	return va;
}

static void
mbox_mem_unmap(void *va, size_t size)
{
	if (va)
		munmap(va, size);
}

static int
pf_af_sync_msg(struct dev *dev, struct mbox_msghdr **rsp)
{
	uint32_t timeout = 0, sleep = 1;
	struct mbox *mbox = dev->mbox;
	struct mbox_dev *mdev = &mbox->dev[0];

	volatile uint64_t int_status = 0;
	struct mbox_msghdr *msghdr;
	uint64_t off;
	int rc = 0;

	/* We need to disable PF interrupts. We are in timer interrupt */
	plt_write64(~0ull, dev->bar2 + RVU_PF_INT_ENA_W1C);

	/* Send message */
	mbox_msg_send(mbox, 0);

	do {
		plt_delay_ms(sleep);
		timeout += sleep;
		if (timeout >= mbox->rsp_tmo) {
			plt_err("Message timeout: %dms", mbox->rsp_tmo);
			rc = -EIO;
			break;
		}
		int_status = plt_read64(dev->bar2 + RVU_PF_INT);
	} while ((int_status & 0x1) != 0x1);

	/* Clear */
	plt_write64(int_status, dev->bar2 + RVU_PF_INT);

	/* Enable interrupts */
	plt_write64(~0ull, dev->bar2 + RVU_PF_INT_ENA_W1S);

	if (rc == 0) {
		/* Get message */
		off = mbox->rx_start +
		      PLT_ALIGN(sizeof(struct mbox_hdr), MBOX_MSG_ALIGN);
		msghdr = (struct mbox_msghdr *)((uintptr_t)mdev->mbase + off);
		if (rsp)
			*rsp = msghdr;
		rc = msghdr->rc;
	}

	return rc;
}

/* PF will send the messages to AF and wait for responses and forward the
 * responses to VF.
 */
static int
af_pf_wait_msg(struct dev *dev, uint16_t vf, int num_msg)
{
	uint32_t timeout = 0, sleep = 1;
	struct mbox *mbox = dev->mbox;
	struct mbox_dev *mdev = &mbox->dev[0];
	volatile uint64_t int_status;
	struct mbox_hdr *req_hdr;
	struct mbox_msghdr *msg;
	struct mbox_msghdr *rsp;
	uint64_t offset;
	size_t size;
	int i;

	/* We need to disable PF interrupts. We are in timer interrupt */
	plt_write64(~0ull, dev->bar2 + RVU_PF_INT_ENA_W1C);

	/* Send message to AF */
	mbox_msg_send(mbox, 0);

	/* Wait for AF response */
	do {
		plt_delay_ms(sleep);
		timeout++;
		if (timeout >= mbox->rsp_tmo) {
			plt_err("Routed messages %d timeout: %dms", num_msg,
				mbox->rsp_tmo);
			break;
		}
		int_status = plt_read64(dev->bar2 + RVU_PF_INT);
	} while ((int_status & 0x1) != 0x1);

	/* Clear */
	plt_write64(~0ull, dev->bar2 + RVU_PF_INT);

	/* Enable interrupts */
	plt_write64(~0ull, dev->bar2 + RVU_PF_INT_ENA_W1S);

	req_hdr = (struct mbox_hdr *)((uintptr_t)mdev->mbase + mbox->rx_start);
	if (req_hdr->num_msgs != num_msg)
		plt_err("Routed messages: %d received: %d", num_msg,
			req_hdr->num_msgs);

	/* Get messages from mbox */
	offset = mbox->rx_start +
		 PLT_ALIGN(sizeof(struct mbox_hdr), MBOX_MSG_ALIGN);
	for (i = 0; i < req_hdr->num_msgs; i++) {
		msg = (struct mbox_msghdr *)((uintptr_t)mdev->mbase + offset);
		size = mbox->rx_start + msg->next_msgoff - offset;

		/* Reserve PF/VF mbox message */
		size = PLT_ALIGN(size, MBOX_MSG_ALIGN);
		rsp = mbox_alloc_msg(&dev->mbox_vfpf, vf, size);
		if (!rsp) {
			plt_err("Failed to reserve VF%d message", vf);
			continue;
		}

		mbox_rsp_init(msg->id, rsp);

		/* Copy message from AF<->PF mbox to PF<->VF mbox */
		mbox_memcpy((uint8_t *)rsp + sizeof(struct mbox_msghdr),
			    (uint8_t *)msg + sizeof(struct mbox_msghdr),
			    size - sizeof(struct mbox_msghdr));

		/* Set status and sender pf_func data */
		rsp->rc = msg->rc;
		rsp->pcifunc = msg->pcifunc;

		/* Whenever a PF comes up, AF sends the link status to it but
		 * when VF comes up no such event is sent to respective VF.
		 * Using MBOX_MSG_NIX_LF_START_RX response from AF for the
		 * purpose and send the link status of PF to VF.
		 */
		if (msg->id == MBOX_MSG_NIX_LF_START_RX) {
			/* Send link status to VF */
			struct cgx_link_user_info linfo;
			struct mbox_msghdr *vf_msg;
			size_t sz;

			/* Get the link status */
			memset(&linfo, 0, sizeof(struct cgx_link_user_info));
			if (dev->ops && dev->ops->link_status_get)
				dev->ops->link_status_get(dev->roc_nix, &linfo);

			sz = PLT_ALIGN(mbox_id2size(MBOX_MSG_CGX_LINK_EVENT),
				       MBOX_MSG_ALIGN);
			/* Prepare the message to be sent */
			vf_msg = mbox_alloc_msg(&dev->mbox_vfpf_up, vf, sz);
			if (vf_msg) {
				mbox_req_init(MBOX_MSG_CGX_LINK_EVENT, vf_msg);
				mbox_memcpy((uint8_t *)vf_msg + sizeof(struct mbox_msghdr), &linfo,
					    sizeof(struct cgx_link_user_info));

				vf_msg->rc = msg->rc;
				vf_msg->pcifunc = msg->pcifunc;
				/* Send to VF */
				mbox_msg_send_up(&dev->mbox_vfpf_up, vf);
				mbox_wait_for_zero(&dev->mbox_vfpf_up, vf);
			}
		}

		offset = mbox->rx_start + msg->next_msgoff;
	}

	return req_hdr->num_msgs;
}

/* PF receives mbox DOWN messages from VF and forwards to AF */
static int
vf_pf_process_msgs(struct dev *dev, uint16_t vf)
{
	struct mbox *mbox = &dev->mbox_vfpf;
	struct mbox_dev *mdev = &mbox->dev[vf];
	struct mbox_hdr *req_hdr;
	struct mbox_msghdr *msg;
	int offset, routed = 0;
	size_t size;
	uint16_t i;

	req_hdr = (struct mbox_hdr *)((uintptr_t)mdev->mbase + mbox->rx_start);
	if (!req_hdr->num_msgs)
		return 0;

	offset = mbox->rx_start + PLT_ALIGN(sizeof(*req_hdr), MBOX_MSG_ALIGN);

	mbox_get(dev->mbox);
	for (i = 0; i < req_hdr->num_msgs; i++) {
		msg = (struct mbox_msghdr *)((uintptr_t)mdev->mbase + offset);
		size = mbox->rx_start + msg->next_msgoff - offset;

		/* RVU_PF_FUNC_S */
		msg->pcifunc = dev_pf_func(dev->pf, vf);

		if (msg->id == MBOX_MSG_READY) {
			struct ready_msg_rsp *rsp;
			uint16_t max_bits = sizeof(dev->active_vfs[0]) * 8;

			/* Handle READY message in PF */
			dev->active_vfs[vf / max_bits] |=
				BIT_ULL(vf % max_bits);
			rsp = (struct ready_msg_rsp *)mbox_alloc_msg(
				mbox, vf, sizeof(*rsp));
			if (!rsp) {
				plt_err("Failed to alloc VF%d READY message",
					vf);
				continue;
			}

			mbox_rsp_init(msg->id, rsp);

			/* PF/VF function ID */
			rsp->hdr.pcifunc = msg->pcifunc;
			rsp->hdr.rc = 0;
		} else {
			struct mbox_msghdr *af_req;
			/* Reserve AF/PF mbox message */
			size = PLT_ALIGN(size, MBOX_MSG_ALIGN);
			af_req = mbox_alloc_msg(dev->mbox, 0, size);
			if (af_req == NULL)
				return -ENOSPC;
			mbox_req_init(msg->id, af_req);

			/* Copy message from VF<->PF mbox to PF<->AF mbox */
			mbox_memcpy((uint8_t *)af_req +
					    sizeof(struct mbox_msghdr),
				    (uint8_t *)msg + sizeof(struct mbox_msghdr),
				    size - sizeof(struct mbox_msghdr));
			af_req->pcifunc = msg->pcifunc;
			routed++;
		}
		offset = mbox->rx_start + msg->next_msgoff;
	}

	if (routed > 0) {
		plt_base_dbg("pf:%d routed %d messages from vf:%d to AF",
			     dev->pf, routed, vf);
		/* PF will send the messages to AF and wait for responses */
		af_pf_wait_msg(dev, vf, routed);
		mbox_reset(dev->mbox, 0);
	}
	mbox_put(dev->mbox);

	/* Send mbox responses to VF */
	if (mdev->num_msgs) {
		plt_base_dbg("pf:%d reply %d messages to vf:%d", dev->pf,
			     mdev->num_msgs, vf);
		mbox_msg_send(mbox, vf);
	}

	return i;
}

/* VF sends Ack to PF's UP messages */
static int
vf_pf_process_up_msgs(struct dev *dev, uint16_t vf)
{
	struct mbox *mbox = &dev->mbox_vfpf_up;
	struct mbox_dev *mdev = &mbox->dev[vf];
	struct mbox_hdr *req_hdr;
	struct mbox_msghdr *msg;
	int msgs_acked = 0;
	int offset;
	uint16_t i;

	req_hdr = (struct mbox_hdr *)((uintptr_t)mdev->mbase + mbox->rx_start);
	if (req_hdr->num_msgs == 0)
		return 0;

	offset = mbox->rx_start + PLT_ALIGN(sizeof(*req_hdr), MBOX_MSG_ALIGN);

	for (i = 0; i < req_hdr->num_msgs; i++) {
		msg = (struct mbox_msghdr *)((uintptr_t)mdev->mbase + offset);

		msgs_acked++;
		/* RVU_PF_FUNC_S */
		msg->pcifunc = dev_pf_func(dev->pf, vf);

		switch (msg->id) {
		case MBOX_MSG_CGX_LINK_EVENT:
			plt_base_dbg("PF: Msg 0x%x (%s) fn:0x%x (pf:%d,vf:%d)",
				     msg->id, mbox_id2name(msg->id),
				     msg->pcifunc, dev_get_pf(msg->pcifunc),
				     dev_get_vf(msg->pcifunc));
			break;
		case MBOX_MSG_CGX_PTP_RX_INFO:
			plt_base_dbg("PF: Msg 0x%x (%s) fn:0x%x (pf:%d,vf:%d)",
				     msg->id, mbox_id2name(msg->id),
				     msg->pcifunc, dev_get_pf(msg->pcifunc),
				     dev_get_vf(msg->pcifunc));
			break;
		default:
			plt_err("Not handled UP msg 0x%x (%s) func:0x%x",
				msg->id, mbox_id2name(msg->id), msg->pcifunc);
		}
		offset = mbox->rx_start + msg->next_msgoff;
	}
	mbox_reset(mbox, vf);
	mdev->msgs_acked = msgs_acked;
	plt_wmb();

	return i;
}

/* PF handling messages from VF */
static void
roc_vf_pf_mbox_handle_msg(void *param, dev_intr_t *intr)
{
	uint16_t vf, max_vf, max_bits;
	struct dev *dev = param;

	max_bits = sizeof(dev->intr.bits[0]) * sizeof(uint64_t);
	max_vf = max_bits * MAX_VFPF_DWORD_BITS;

	for (vf = 0; vf < max_vf; vf++) {
		if (intr->bits[vf / max_bits] & BIT_ULL(vf % max_bits)) {
			plt_base_dbg("Process vf:%d request (pf:%d, vf:%d)", vf,
				     dev->pf, dev->vf);
			/* VF initiated down messages */
			vf_pf_process_msgs(dev, vf);
			/* VF replies to PF's UP messages */
			vf_pf_process_up_msgs(dev, vf);
			intr->bits[vf / max_bits] &= ~(BIT_ULL(vf % max_bits));
		}
	}
}

/* IRQ to PF from VF - PF context (interrupt thread) */
static void
roc_vf_pf_mbox_irq(void *param)
{
	bool signal_thread = false;
	struct dev *dev = param;
	dev_intr_t intrb;
	uint64_t intr;
	int vfpf, sz;

	sz = sizeof(intrb.bits[0]) * MAX_VFPF_DWORD_BITS;
	memset(intrb.bits, 0, sz);
	for (vfpf = 0; vfpf < MAX_VFPF_DWORD_BITS; ++vfpf) {
		intr = plt_read64(dev->bar2 + RVU_PF_VFPF_MBOX_INTX(vfpf));
		if (!intr)
			continue;

		plt_base_dbg("vfpf: %d intr: 0x%" PRIx64 " (pf:%d, vf:%d)",
			     vfpf, intr, dev->pf, dev->vf);

		/* Save and clear intr bits */
		intrb.bits[vfpf] |= intr;
		plt_write64(intr, dev->bar2 + RVU_PF_VFPF_MBOX_INTX(vfpf));
		signal_thread = true;
	}

	if (signal_thread) {
		pthread_mutex_lock(&dev->sync.mutex);
		/* Interrupt state was saved in local variable first, as dev->intr.bits
		 * is a shared resources between VF msg and interrupt thread.
		 */
		memcpy(dev->intr.bits, intrb.bits, sz);
		/* MBOX message received from VF */
		dev->sync.msg_avail |= ROC_DEV_MBOX_PEND;
		/* Signal vf message handler thread */
		pthread_cond_signal(&dev->sync.pfvf_msg_cond);
		pthread_mutex_unlock(&dev->sync.mutex);
	}
}

/* Received response from AF (PF context) / PF (VF context) */
static void
process_msgs(struct dev *dev, struct mbox *mbox)
{
	struct mbox_dev *mdev = &mbox->dev[0];
	struct mbox_hdr *req_hdr;
	struct mbox_msghdr *msg;
	int msgs_acked = 0;
	int offset;
	uint16_t i;

	req_hdr = (struct mbox_hdr *)((uintptr_t)mdev->mbase + mbox->rx_start);
	if (req_hdr->num_msgs == 0)
		return;

	offset = mbox->rx_start + PLT_ALIGN(sizeof(*req_hdr), MBOX_MSG_ALIGN);
	for (i = 0; i < req_hdr->num_msgs; i++) {
		msg = (struct mbox_msghdr *)((uintptr_t)mdev->mbase + offset);

		msgs_acked++;
		plt_base_dbg("Message 0x%x (%s) pf:%d/vf:%d", msg->id,
			     mbox_id2name(msg->id), dev_get_pf(msg->pcifunc),
			     dev_get_vf(msg->pcifunc));

		switch (msg->id) {
			/* Add message id's that are handled here */
		case MBOX_MSG_READY:
			/* Get our identity */
			dev->pf_func = msg->pcifunc;
			break;
		case MBOX_MSG_CGX_PRIO_FLOW_CTRL_CFG:
		case MBOX_MSG_CGX_CFG_PAUSE_FRM:
			/* Handling the case where one VF tries to disable PFC
			 * while PFC already configured on other VFs. This is
			 * not an error but a warning which can be ignored.
			 */
			if (msg->rc) {
				if (msg->rc == LMAC_AF_ERR_PERM_DENIED) {
					plt_mbox_dbg(
						"Receive Flow control disable not permitted "
						"as its used by other PFVFs");
					msg->rc = 0;
				} else {
					plt_err("Message (%s) response has err=%d",
						mbox_id2name(msg->id), msg->rc);
				}
			}
			break;
		case MBOX_MSG_CGX_PROMISC_DISABLE:
		case MBOX_MSG_CGX_PROMISC_ENABLE:
			if (msg->rc) {
				if (msg->rc == LMAC_AF_ERR_INVALID_PARAM) {
					plt_mbox_dbg("Already in same promisc state");
					msg->rc = 0;
				} else {
					plt_err("Message (%s) response has err=%d",
						mbox_id2name(msg->id), msg->rc);
				}
			}
			break;

		default:
			if (msg->rc)
				plt_err("Message (%s) response has err=%d (%s)",
					mbox_id2name(msg->id), msg->rc, roc_error_msg_get(msg->rc));
			break;
		}
		offset = mbox->rx_start + msg->next_msgoff;
	}

	mbox_reset(mbox, 0);
	/* Update acked if someone is waiting a message - mbox_wait is waiting */
	mdev->msgs_acked = msgs_acked;
	plt_wmb();
}

/* Copies the message received from AF and sends it to VF */
static void
pf_vf_mbox_send_up_msg(struct dev *dev, void *rec_msg)
{
	uint16_t max_bits = sizeof(dev->active_vfs[0]) * sizeof(uint64_t);
	struct mbox *vf_mbox = &dev->mbox_vfpf_up;
	struct msg_req *msg = rec_msg;
	struct mbox_msghdr *vf_msg;
	uint16_t vf;
	size_t size;

	size = PLT_ALIGN(mbox_id2size(msg->hdr.id), MBOX_MSG_ALIGN);
	if (size < sizeof(struct mbox_msghdr))
		return;
	/* Send UP message to all VF's */
	for (vf = 0; vf < vf_mbox->ndevs; vf++) {
		/* VF active */
		if (!(dev->active_vfs[vf / max_bits] & (BIT_ULL(vf))))
			continue;

		plt_base_dbg("(%s) size: %zx to VF: %d",
			     mbox_id2name(msg->hdr.id), size, vf);

		/* Reserve PF/VF mbox message */
		vf_msg = mbox_alloc_msg(vf_mbox, vf, size);
		if (!vf_msg) {
			plt_err("Failed to alloc VF%d UP message", vf);
			continue;
		}
		mbox_req_init(msg->hdr.id, vf_msg);

		/*
		 * Copy message from AF<->PF UP mbox
		 * to PF<->VF UP mbox
		 */
		mbox_memcpy((uint8_t *)vf_msg + sizeof(struct mbox_msghdr),
			    (uint8_t *)msg + sizeof(struct mbox_msghdr),
			    size - sizeof(struct mbox_msghdr));

		vf_msg->rc = msg->hdr.rc;
		/* Set PF to be a sender */
		vf_msg->pcifunc = dev->pf_func;

		/* Send to VF */
		mbox_msg_send(vf_mbox, vf);
		mbox_wait_for_zero(&dev->mbox_vfpf_up, vf);
	}
}

static int
mbox_up_handler_mcs_intr_notify(struct dev *dev, struct mcs_intr_info *info, struct msg_rsp *rsp)
{
	struct roc_mcs_event_desc desc = {0};
	struct roc_mcs *mcs;

	plt_base_dbg("pf:%d/vf:%d msg id 0x%x (%s) from: pf:%d/vf:%d", dev_get_pf(dev->pf_func),
		     dev_get_vf(dev->pf_func), info->hdr.id, mbox_id2name(info->hdr.id),
		     dev_get_pf(info->hdr.pcifunc), dev_get_vf(info->hdr.pcifunc));

	mcs = roc_idev_mcs_get(info->mcs_id);
	if (!mcs)
		goto exit;

	if (info->intr_mask) {
		switch (info->intr_mask) {
		case MCS_CPM_RX_SECTAG_V_EQ1_INT:
			desc.type = ROC_MCS_EVENT_SECTAG_VAL_ERR;
			desc.subtype = ROC_MCS_EVENT_RX_SECTAG_V_EQ1;
			break;
		case MCS_CPM_RX_SECTAG_E_EQ0_C_EQ1_INT:
			desc.type = ROC_MCS_EVENT_SECTAG_VAL_ERR;
			desc.subtype = ROC_MCS_EVENT_RX_SECTAG_E_EQ0_C_EQ1;
			break;
		case MCS_CPM_RX_SECTAG_SL_GTE48_INT:
			desc.type = ROC_MCS_EVENT_SECTAG_VAL_ERR;
			desc.subtype = ROC_MCS_EVENT_RX_SECTAG_SL_GTE48;
			break;
		case MCS_CPM_RX_SECTAG_ES_EQ1_SC_EQ1_INT:
			desc.type = ROC_MCS_EVENT_SECTAG_VAL_ERR;
			desc.subtype = ROC_MCS_EVENT_RX_SECTAG_ES_EQ1_SC_EQ1;
			break;
		case MCS_CPM_RX_SECTAG_SC_EQ1_SCB_EQ1_INT:
			desc.type = ROC_MCS_EVENT_SECTAG_VAL_ERR;
			desc.subtype = ROC_MCS_EVENT_RX_SECTAG_SC_EQ1_SCB_EQ1;
			break;
		case MCS_CPM_RX_PACKET_XPN_EQ0_INT:
			desc.type = ROC_MCS_EVENT_RX_SA_PN_HARD_EXP;
			desc.metadata.sa_idx = info->sa_id;
			break;
		case MCS_CPM_RX_PN_THRESH_REACHED_INT:
			desc.type = ROC_MCS_EVENT_RX_SA_PN_SOFT_EXP;
			desc.metadata.sa_idx = info->sa_id;
			break;
		case MCS_CPM_TX_PACKET_XPN_EQ0_INT:
			desc.type = ROC_MCS_EVENT_TX_SA_PN_HARD_EXP;
			desc.metadata.sa_idx = info->sa_id;
			break;
		case MCS_CPM_TX_PN_THRESH_REACHED_INT:
			desc.type = ROC_MCS_EVENT_TX_SA_PN_SOFT_EXP;
			desc.metadata.sa_idx = info->sa_id;
			break;
		case MCS_CPM_TX_SA_NOT_VALID_INT:
			desc.type = ROC_MCS_EVENT_SA_NOT_VALID;
			break;
		case MCS_BBE_RX_DFIFO_OVERFLOW_INT:
		case MCS_BBE_TX_DFIFO_OVERFLOW_INT:
			desc.type = ROC_MCS_EVENT_FIFO_OVERFLOW;
			desc.subtype = ROC_MCS_EVENT_DATA_FIFO_OVERFLOW;
			desc.metadata.lmac_id = info->lmac_id;
			break;
		case MCS_BBE_RX_PLFIFO_OVERFLOW_INT:
		case MCS_BBE_TX_PLFIFO_OVERFLOW_INT:
			desc.type = ROC_MCS_EVENT_FIFO_OVERFLOW;
			desc.subtype = ROC_MCS_EVENT_POLICY_FIFO_OVERFLOW;
			desc.metadata.lmac_id = info->lmac_id;
			break;
		case MCS_PAB_RX_CHAN_OVERFLOW_INT:
		case MCS_PAB_TX_CHAN_OVERFLOW_INT:
			desc.type = ROC_MCS_EVENT_FIFO_OVERFLOW;
			desc.subtype = ROC_MCS_EVENT_PKT_ASSM_FIFO_OVERFLOW;
			desc.metadata.lmac_id = info->lmac_id;
			break;
		default:
			goto exit;
		}

		mcs_event_cb_process(mcs, &desc);
	}

exit:
	rsp->hdr.rc = 0;
	return 0;
}

static int
mbox_up_handler_cgx_link_event(struct dev *dev, struct cgx_link_info_msg *msg,
			       struct msg_rsp *rsp)
{
	struct cgx_link_user_info *linfo = &msg->link_info;
	void *roc_nix = dev->roc_nix;

	plt_base_dbg("pf:%d/vf:%d NIC Link %s --> 0x%x (%s) from: pf:%d/vf:%d",
		     dev_get_pf(dev->pf_func), dev_get_vf(dev->pf_func),
		     linfo->link_up ? "UP" : "DOWN", msg->hdr.id,
		     mbox_id2name(msg->hdr.id), dev_get_pf(msg->hdr.pcifunc),
		     dev_get_vf(msg->hdr.pcifunc));

	/* PF gets link notification from AF */
	if (dev_get_pf(msg->hdr.pcifunc) == 0) {
		if (dev->ops && dev->ops->link_status_update)
			dev->ops->link_status_update(roc_nix, linfo);

		/* Forward the same message as received from AF to VF */
		pf_vf_mbox_send_up_msg(dev, msg);
	} else {
		/* VF gets link up notification */
		if (dev->ops && dev->ops->link_status_update)
			dev->ops->link_status_update(roc_nix, linfo);
	}

	rsp->hdr.rc = 0;
	return 0;
}

static int
mbox_up_handler_cgx_ptp_rx_info(struct dev *dev,
				struct cgx_ptp_rx_info_msg *msg,
				struct msg_rsp *rsp)
{
	void *roc_nix = dev->roc_nix;

	plt_base_dbg("pf:%d/vf:%d PTP mode %s --> 0x%x (%s) from: pf:%d/vf:%d",
		     dev_get_pf(dev->pf_func), dev_get_vf(dev->pf_func),
		     msg->ptp_en ? "ENABLED" : "DISABLED", msg->hdr.id,
		     mbox_id2name(msg->hdr.id), dev_get_pf(msg->hdr.pcifunc),
		     dev_get_vf(msg->hdr.pcifunc));

	/* PF gets PTP notification from AF */
	if (dev_get_pf(msg->hdr.pcifunc) == 0) {
		if (dev->ops && dev->ops->ptp_info_update)
			dev->ops->ptp_info_update(roc_nix, msg->ptp_en);

		/* Forward the same message as received from AF to VF */
		pf_vf_mbox_send_up_msg(dev, msg);
	} else {
		/* VF gets PTP notification */
		if (dev->ops && dev->ops->ptp_info_update)
			dev->ops->ptp_info_update(roc_nix, msg->ptp_en);
	}

	rsp->hdr.rc = 0;
	return 0;
}

static int
mbox_process_msgs_up(struct dev *dev, struct mbox_msghdr *req)
{
	/* Check if valid, if not reply with a invalid msg */
	if (req->sig != MBOX_REQ_SIG)
		return -EIO;

	switch (req->id) {
	default:
		reply_invalid_msg(&dev->mbox_up, 0, 0, req->id);
		break;
#define M(_name, _id, _fn_name, _req_type, _rsp_type)                          \
	case _id: {                                                            \
		struct _rsp_type *rsp;                                         \
		int err;                                                       \
		rsp = (struct _rsp_type *)mbox_alloc_msg(                      \
			&dev->mbox_up, 0, sizeof(struct _rsp_type));           \
		if (!rsp)                                                      \
			return -ENOMEM;                                        \
		rsp->hdr.id = _id;                                             \
		rsp->hdr.sig = MBOX_RSP_SIG;                                   \
		rsp->hdr.pcifunc = dev->pf_func;                               \
		rsp->hdr.rc = 0;                                               \
		err = mbox_up_handler_##_fn_name(dev, (struct _req_type *)req, \
						 rsp);                         \
		return err;                                                    \
	}
		MBOX_UP_CGX_MESSAGES
		MBOX_UP_MCS_MESSAGES
#undef M
	}

	return -ENODEV;
}

/* Received up messages from AF (PF context) / PF (in context) */
static void
process_msgs_up(struct dev *dev, struct mbox *mbox)
{
	struct mbox_dev *mdev = &mbox->dev[0];
	struct mbox_hdr *req_hdr;
	struct mbox_msghdr *msg;
	int i, err, offset;

	req_hdr = (struct mbox_hdr *)((uintptr_t)mdev->mbase + mbox->rx_start);
	if (req_hdr->num_msgs == 0)
		return;

	offset = mbox->rx_start + PLT_ALIGN(sizeof(*req_hdr), MBOX_MSG_ALIGN);
	for (i = 0; i < req_hdr->num_msgs; i++) {
		msg = (struct mbox_msghdr *)((uintptr_t)mdev->mbase + offset);

		plt_base_dbg("Message 0x%x (%s) pf:%d/vf:%d", msg->id,
			     mbox_id2name(msg->id), dev_get_pf(msg->pcifunc),
			     dev_get_vf(msg->pcifunc));
		err = mbox_process_msgs_up(dev, msg);
		if (err)
			plt_err("Error %d handling 0x%x (%s)", err, msg->id,
				mbox_id2name(msg->id));
		offset = mbox->rx_start + msg->next_msgoff;
	}
	/* Send mbox responses */
	if (mdev->num_msgs) {
		plt_base_dbg("Reply num_msgs:%d", mdev->num_msgs);
		mbox_msg_send(mbox, 0);
	}
}

/* IRQ to VF from PF - VF context (interrupt thread) */
static void
roc_pf_vf_mbox_irq(void *param)
{
	struct dev *dev = param;
	uint64_t mbox_data;
	uint64_t intr;

	intr = plt_read64(dev->bar2 + RVU_VF_INT);
	if (intr == 0)
		plt_base_dbg("Proceeding to check mbox UP messages if any");

	plt_write64(intr, dev->bar2 + RVU_VF_INT);
	plt_base_dbg("Irq 0x%" PRIx64 "(pf:%d,vf:%d)", intr, dev->pf, dev->vf);

	/* Reading for UP/DOWN message, next message sending will be delayed
	 * by 1ms until this region is zeroed mbox_wait_for_zero()
	 */
	mbox_data = plt_read64(dev->bar2 + RVU_VF_VFPF_MBOX0);
	/* If interrupt occurred for down message */
	if (mbox_data & MBOX_DOWN_MSG) {
		mbox_data &= ~MBOX_DOWN_MSG;
		plt_write64(mbox_data, dev->bar2 + RVU_VF_VFPF_MBOX0);

		/* First process all configuration messages */
		process_msgs(dev, dev->mbox);
	}
	/* If interrupt occurred for UP message */
	if (mbox_data & MBOX_UP_MSG) {
		mbox_data &= ~MBOX_UP_MSG;
		plt_write64(mbox_data, dev->bar2 + RVU_VF_VFPF_MBOX0);

		/* Process Uplink messages */
		process_msgs_up(dev, &dev->mbox_up);
	}
}

/* IRQ to PF from AF - PF context (interrupt thread) */
static void
roc_af_pf_mbox_irq(void *param)
{
	struct dev *dev = param;
	uint64_t mbox_data;
	uint64_t intr;

	intr = plt_read64(dev->bar2 + RVU_PF_INT);
	if (intr == 0)
		plt_base_dbg("Proceeding to check mbox UP messages if any");

	plt_write64(intr, dev->bar2 + RVU_PF_INT);
	plt_base_dbg("Irq 0x%" PRIx64 "(pf:%d,vf:%d)", intr, dev->pf, dev->vf);

	/* Reading for UP/DOWN message, next message sending will be delayed
	 * by 1ms until this region is zeroed mbox_wait_for_zero()
	 */
	mbox_data = plt_read64(dev->bar2 + RVU_PF_PFAF_MBOX0);
	/* If interrupt occurred for down message */
	if (mbox_data & MBOX_DOWN_MSG) {
		mbox_data &= ~MBOX_DOWN_MSG;
		plt_write64(mbox_data, dev->bar2 + RVU_PF_PFAF_MBOX0);

		/* First process all configuration messages */
		process_msgs(dev, dev->mbox);
	}
	/* If interrupt occurred for up message */
	if (mbox_data & MBOX_UP_MSG) {
		mbox_data &= ~MBOX_UP_MSG;
		plt_write64(mbox_data, dev->bar2 + RVU_PF_PFAF_MBOX0);

		/* Process Uplink messages */
		process_msgs_up(dev, &dev->mbox_up);
	}
}

static int
mbox_register_pf_irq(struct plt_pci_device *pci_dev, struct dev *dev)
{
	struct plt_intr_handle *intr_handle = pci_dev->intr_handle;
	int i, rc;

	/* HW clear irq */
	for (i = 0; i < MAX_VFPF_DWORD_BITS; ++i)
		plt_write64(~0ull,
			    dev->bar2 + RVU_PF_VFPF_MBOX_INT_ENA_W1CX(i));

	plt_write64(~0ull, dev->bar2 + RVU_PF_INT_ENA_W1C);

	/* MBOX interrupt for VF(0...63) <-> PF */
	rc = dev_irq_register(intr_handle, roc_vf_pf_mbox_irq, dev,
			      RVU_PF_INT_VEC_VFPF_MBOX0);

	if (rc) {
		plt_err("Fail to register PF(VF0-63) mbox irq");
		return rc;
	}
	/* MBOX interrupt for VF(64...128) <-> PF */
	rc = dev_irq_register(intr_handle, roc_vf_pf_mbox_irq, dev,
			      RVU_PF_INT_VEC_VFPF_MBOX1);

	if (rc) {
		plt_err("Fail to register PF(VF64-128) mbox irq");
		return rc;
	}
	/* MBOX interrupt AF <-> PF */
	rc = dev_irq_register(intr_handle, roc_af_pf_mbox_irq, dev,
			      RVU_PF_INT_VEC_AFPF_MBOX);
	if (rc) {
		plt_err("Fail to register AF<->PF mbox irq");
		return rc;
	}

	/* HW enable intr */
	for (i = 0; i < MAX_VFPF_DWORD_BITS; ++i)
		plt_write64(~0ull,
			    dev->bar2 + RVU_PF_VFPF_MBOX_INT_ENA_W1SX(i));

	plt_write64(~0ull, dev->bar2 + RVU_PF_INT);
	plt_write64(~0ull, dev->bar2 + RVU_PF_INT_ENA_W1S);

	return rc;
}

static int
mbox_register_vf_irq(struct plt_pci_device *pci_dev, struct dev *dev)
{
	struct plt_intr_handle *intr_handle = pci_dev->intr_handle;
	int rc;

	/* Clear irq */
	plt_write64(~0ull, dev->bar2 + RVU_VF_INT_ENA_W1C);

	/* MBOX interrupt PF <-> VF */
	rc = dev_irq_register(intr_handle, roc_pf_vf_mbox_irq, dev,
			      RVU_VF_INT_VEC_MBOX);
	if (rc) {
		plt_err("Fail to register PF<->VF mbox irq");
		return rc;
	}

	/* HW enable intr */
	plt_write64(~0ull, dev->bar2 + RVU_VF_INT);
	plt_write64(~0ull, dev->bar2 + RVU_VF_INT_ENA_W1S);

	return rc;
}

int
dev_mbox_register_irq(struct plt_pci_device *pci_dev, struct dev *dev)
{
	if (dev_is_vf(dev))
		return mbox_register_vf_irq(pci_dev, dev);
	else
		return mbox_register_pf_irq(pci_dev, dev);
}

static void
mbox_unregister_pf_irq(struct plt_pci_device *pci_dev, struct dev *dev)
{
	struct plt_intr_handle *intr_handle = pci_dev->intr_handle;
	int i;

	/* HW clear irq */
	for (i = 0; i < MAX_VFPF_DWORD_BITS; ++i)
		plt_write64(~0ull,
			    dev->bar2 + RVU_PF_VFPF_MBOX_INT_ENA_W1CX(i));

	plt_write64(~0ull, dev->bar2 + RVU_PF_INT_ENA_W1C);

	/* Unregister the interrupt handler for each vectors */
	/* MBOX interrupt for VF(0...63) <-> PF */
	dev_irq_unregister(intr_handle, roc_vf_pf_mbox_irq, dev,
			   RVU_PF_INT_VEC_VFPF_MBOX0);

	/* MBOX interrupt for VF(64...128) <-> PF */
	dev_irq_unregister(intr_handle, roc_vf_pf_mbox_irq, dev,
			   RVU_PF_INT_VEC_VFPF_MBOX1);

	/* MBOX interrupt AF <-> PF */
	dev_irq_unregister(intr_handle, roc_af_pf_mbox_irq, dev,
			   RVU_PF_INT_VEC_AFPF_MBOX);
}

static void
mbox_unregister_vf_irq(struct plt_pci_device *pci_dev, struct dev *dev)
{
	struct plt_intr_handle *intr_handle = pci_dev->intr_handle;

	/* Clear irq */
	plt_write64(~0ull, dev->bar2 + RVU_VF_INT_ENA_W1C);

	/* Unregister the interrupt handler */
	dev_irq_unregister(intr_handle, roc_pf_vf_mbox_irq, dev,
			   RVU_VF_INT_VEC_MBOX);
}

static void
mbox_unregister_irq(struct plt_pci_device *pci_dev, struct dev *dev)
{
	if (dev_is_vf(dev))
		mbox_unregister_vf_irq(pci_dev, dev);
	else
		mbox_unregister_pf_irq(pci_dev, dev);
}

static int
vf_flr_send_msg(struct dev *dev, uint16_t vf)
{
	struct mbox *mbox = dev->mbox;
	struct msg_req *req;
	int rc;

	req = mbox_alloc_msg_vf_flr(mbox_get(mbox));
	if (req == NULL)
		return -ENOSPC;
	/* Overwrite pcifunc to indicate VF */
	req->hdr.pcifunc = dev_pf_func(dev->pf, vf);

	/* Sync message in interrupt context */
	rc = pf_af_sync_msg(dev, NULL);
	if (rc)
		plt_err("Failed to send VF FLR mbox msg, rc=%d", rc);

	mbox_put(mbox);

	return rc;
}

static void
roc_pf_vf_flr_irq(void *param)
{
	struct dev *dev = (struct dev *)param;
	bool signal_thread = false;
	dev_intr_t flr;
	uintptr_t bar2;
	uint64_t intr;
	int i, sz;

	bar2 = dev->bar2;

	sz = sizeof(flr.bits[0]) * MAX_VFPF_DWORD_BITS;
	memset(flr.bits, 0, sz);
	for (i = 0; i < MAX_VFPF_DWORD_BITS; ++i) {
		intr = plt_read64(bar2 + RVU_PF_VFFLR_INTX(i));
		if (!intr)
			continue;

		/* Clear interrupt */
		plt_write64(intr, bar2 + RVU_PF_VFFLR_INTX(i));
		/* Disable the interrupt */
		plt_write64(intr,
			    bar2 + RVU_PF_VFFLR_INT_ENA_W1CX(i));

		/* Save FLR interrupts per VF as bits */
		flr.bits[i] |= intr;
		/* Enable interrupt */
		plt_write64(~0ull,
			    bar2 + RVU_PF_VFFLR_INT_ENA_W1SX(i));
		signal_thread = true;
	}

	if (signal_thread) {
		pthread_mutex_lock(&dev->sync.mutex);
		/* Interrupt state was saved in local variable first, as dev->flr.bits
		 * is a shared resources between VF msg and interrupt thread.
		 */
		memcpy(dev->flr.bits, flr.bits, sz);
		/* FLR message received from VF */
		dev->sync.msg_avail |= ROC_DEV_FLR_PEND;
		/* Signal vf message handler thread */
		pthread_cond_signal(&dev->sync.pfvf_msg_cond);
		pthread_mutex_unlock(&dev->sync.mutex);
	}
}

static int
vf_flr_unregister_irqs(struct plt_pci_device *pci_dev, struct dev *dev)
{
	struct plt_intr_handle *intr_handle = pci_dev->intr_handle;
	int i;

	plt_base_dbg("Unregister VF FLR interrupts for %s", pci_dev->name);

	/* HW clear irq */
	for (i = 0; i < MAX_VFPF_DWORD_BITS; i++)
		plt_write64(~0ull, dev->bar2 + RVU_PF_VFFLR_INT_ENA_W1CX(i));

	dev_irq_unregister(intr_handle, roc_pf_vf_flr_irq, dev,
			   RVU_PF_INT_VEC_VFFLR0);

	dev_irq_unregister(intr_handle, roc_pf_vf_flr_irq, dev,
			   RVU_PF_INT_VEC_VFFLR1);

	return 0;
}

int
dev_vf_flr_register_irqs(struct plt_pci_device *pci_dev, struct dev *dev)
{
	struct plt_intr_handle *handle = pci_dev->intr_handle;
	int i, rc;

	plt_base_dbg("Register VF FLR interrupts for %s", pci_dev->name);

	rc = dev_irq_register(handle, roc_pf_vf_flr_irq, dev,
			      RVU_PF_INT_VEC_VFFLR0);
	if (rc)
		plt_err("Failed to init RVU_PF_INT_VEC_VFFLR0 rc=%d", rc);

	rc = dev_irq_register(handle, roc_pf_vf_flr_irq, dev,
			      RVU_PF_INT_VEC_VFFLR1);
	if (rc)
		plt_err("Failed to init RVU_PF_INT_VEC_VFFLR1 rc=%d", rc);

	/* Enable HW interrupt */
	for (i = 0; i < MAX_VFPF_DWORD_BITS; ++i) {
		plt_write64(~0ull, dev->bar2 + RVU_PF_VFFLR_INTX(i));
		plt_write64(~0ull, dev->bar2 + RVU_PF_VFTRPENDX(i));
		plt_write64(~0ull, dev->bar2 + RVU_PF_VFFLR_INT_ENA_W1SX(i));
	}
	return 0;
}

static void
vf_flr_handle_msg(void *param, dev_intr_t *flr)
{
	uint16_t vf, max_vf, max_bits;
	struct dev *dev = param;

	max_bits = sizeof(flr->bits[0]) * sizeof(uint64_t);
	max_vf = max_bits * MAX_VFPF_DWORD_BITS;

	for (vf = 0; vf < max_vf; vf++) {
		if (flr->bits[vf / max_bits] & BIT_ULL(vf % max_bits)) {
			plt_base_dbg("Process FLR vf:%d request (pf:%d, vf:%d)",
				     vf, dev->pf, dev->vf);
			/* Inform AF about VF reset */
			vf_flr_send_msg(dev, vf);
			flr->bits[vf / max_bits] &= ~(BIT_ULL(vf % max_bits));

			/* Signal FLR finish */
			plt_write64(BIT_ULL(vf % max_bits),
				    dev->bar2 + RVU_PF_VFTRPENDX(vf / max_bits));
		}
	}
}

static uint32_t
pf_vf_mbox_thread_main(void *arg)
{
	struct dev *dev = arg;
	bool is_flr, is_mbox;
	dev_intr_t flr, intr;
	int sz, rc;

	sz = sizeof(intr.bits[0]) * MAX_VFPF_DWORD_BITS;
	pthread_mutex_lock(&dev->sync.mutex);
	while (dev->sync.start_thread) {
		do {
			rc = pthread_cond_wait(&dev->sync.pfvf_msg_cond, &dev->sync.mutex);
		} while (rc != 0);

		if (!dev->sync.msg_avail) {
			continue;
		} else {
			while (dev->sync.msg_avail) {
				/* Check which VF msg received */
				is_mbox = dev->sync.msg_avail & ROC_DEV_MBOX_PEND;
				is_flr = dev->sync.msg_avail & ROC_DEV_FLR_PEND;
				memcpy(intr.bits, dev->intr.bits, sz);
				memcpy(flr.bits, dev->flr.bits, sz);
				memset(dev->flr.bits, 0, sz);
				memset(dev->intr.bits, 0, sz);
				dev->sync.msg_avail = 0;
				/* Unlocking for interrupt thread to grab lock
				 * and update msg_avail field.
				 */
				pthread_mutex_unlock(&dev->sync.mutex);
				/* Calling respective message handlers */
				if (is_mbox)
					roc_vf_pf_mbox_handle_msg(dev, &intr);
				if (is_flr)
					vf_flr_handle_msg(dev, &flr);
				/* Locking as cond wait will unlock before wait */
				pthread_mutex_lock(&dev->sync.mutex);
			}
		}
	}

	pthread_mutex_unlock(&dev->sync.mutex);

	return 0;
}

static void
clear_rvum_interrupts(struct dev *dev)
{
	uint64_t intr;
	int i;

	if (dev_is_vf(dev)) {
		/* Clear VF mbox interrupt */
		intr = plt_read64(dev->bar2 + RVU_VF_INT);
		if (intr)
			plt_write64(intr, dev->bar2 + RVU_VF_INT);
	} else {
		/* Clear AF PF interrupt line */
		intr = plt_read64(dev->bar2 + RVU_PF_INT);
		if (intr)
			plt_write64(intr, dev->bar2 + RVU_PF_INT);
		for (i = 0; i < MAX_VFPF_DWORD_BITS; ++i) {
			/* Clear MBOX interrupts */
			intr = plt_read64(dev->bar2 + RVU_PF_VFPF_MBOX_INTX(i));
			if (intr)
				plt_write64(intr,
					    dev->bar2 +
						    RVU_PF_VFPF_MBOX_INTX(i));
			/* Clear VF FLR interrupts */
			intr = plt_read64(dev->bar2 + RVU_PF_VFFLR_INTX(i));
			if (intr)
				plt_write64(intr,
					    dev->bar2 + RVU_PF_VFFLR_INTX(i));
		}
	}
}

int
dev_active_vfs(struct dev *dev)
{
	int i, count = 0;

	for (i = 0; i < MAX_VFPF_DWORD_BITS; i++)
		count += plt_popcount32(dev->active_vfs[i]);

	return count;
}

static void
dev_vf_hwcap_update(struct plt_pci_device *pci_dev, struct dev *dev)
{
	switch (pci_dev->id.device_id) {
	case PCI_DEVID_CNXK_RVU_PF:
		break;
	case PCI_DEVID_CNXK_RVU_SSO_TIM_VF:
	case PCI_DEVID_CNXK_RVU_NPA_VF:
	case PCI_DEVID_CN10K_RVU_CPT_VF:
	case PCI_DEVID_CN9K_RVU_CPT_VF:
	case PCI_DEVID_CNXK_RVU_AF_VF:
	case PCI_DEVID_CNXK_RVU_VF:
	case PCI_DEVID_CNXK_RVU_SDP_VF:
	case PCI_DEVID_CNXK_RVU_NIX_INL_VF:
		dev->hwcap |= DEV_HWCAP_F_VF;
		break;
	}
}

static uintptr_t
dev_vf_mbase_get(struct plt_pci_device *pci_dev, struct dev *dev)
{
	void *vf_mbase = NULL;
	uintptr_t pa;

	if (dev_is_vf(dev))
		return 0;

	/* For CN10K onwards, it is just after PF MBOX */
	if (!roc_model_is_cn9k())
		return dev->bar4 + MBOX_SIZE;

	pa = plt_read64(dev->bar2 + RVU_PF_VF_BAR4_ADDR);
	if (!pa) {
		plt_err("Invalid VF mbox base pa");
		return pa;
	}

	vf_mbase = mbox_mem_map(pa, MBOX_SIZE * pci_dev->max_vfs);
	if (vf_mbase == MAP_FAILED) {
		plt_err("Failed to mmap vf mbase at pa 0x%lx, rc=%d", pa,
			errno);
		return 0;
	}
	return (uintptr_t)vf_mbase;
}

static void
dev_vf_mbase_put(struct plt_pci_device *pci_dev, uintptr_t vf_mbase)
{
	if (!vf_mbase || !pci_dev->max_vfs || !roc_model_is_cn9k())
		return;

	mbox_mem_unmap((void *)vf_mbase, MBOX_SIZE * pci_dev->max_vfs);
}

static int
dev_setup_shared_lmt_region(struct mbox *mbox, bool valid_iova, uint64_t iova)
{
	struct lmtst_tbl_setup_req *req;
	int rc;

	req = mbox_alloc_msg_lmtst_tbl_setup(mbox_get(mbox));
	if (!req) {
		rc = -ENOSPC;
		goto exit;
	}

	/* This pcifunc is defined with primary pcifunc whose LMT address
	 * will be shared. If call contains valid IOVA, following pcifunc
	 * field is of no use.
	 */
	req->pcifunc = valid_iova ? 0 : idev_lmt_pffunc_get();
	req->use_local_lmt_region = valid_iova;
	req->lmt_iova = iova;

	rc = mbox_process(mbox);
exit:
	mbox_put(mbox);
	return rc;
}

/* Total no of lines * size of each lmtline */
#define LMT_REGION_SIZE (ROC_NUM_LMT_LINES * ROC_LMT_LINE_SZ)
static int
dev_lmt_setup(struct dev *dev)
{
	char name[PLT_MEMZONE_NAMESIZE];
	const struct plt_memzone *mz;
	struct idev_cfg *idev;
	int rc;

	if (roc_model_is_cn9k()) {
		dev->lmt_base = dev->bar2 + (RVU_BLOCK_ADDR_LMT << 20);
		return 0;
	}

	/* [CN10K, .) */

	/* Set common lmt region from second pf_func onwards. */
	if (!dev->disable_shared_lmt && idev_lmt_pffunc_get() &&
	    dev->pf_func != idev_lmt_pffunc_get()) {
		rc = dev_setup_shared_lmt_region(dev->mbox, false, 0);
		if (!rc) {
			/* On success, updating lmt base of secondary pf_funcs
			 * with primary pf_func's lmt base.
			 */
			dev->lmt_base = roc_idev_lmt_base_addr_get();
			return rc;
		}
		plt_err("Failed to setup shared lmt region, pf_func %d err %d "
			"Using respective LMT region per pf func",
			dev->pf_func, rc);
	}

	/* Allocating memory for LMT region */
	sprintf(name, "LMT_MAP%x", dev->pf_func);

	/* Setting alignment to ensure correct masking for resetting to lmt base
	 * of a core after all lmt lines under that core are used.
	 * Alignment value LMT_REGION_SIZE to handle the case where all lines
	 * are used by 1 core.
	 */
	mz = plt_lmt_region_reserve_aligned(name, LMT_REGION_SIZE,
					    LMT_REGION_SIZE);
	if (!mz) {
		plt_err("Memory alloc failed: %s", strerror(errno));
		goto fail;
	}

	/* Share the IOVA address with Kernel */
	rc = dev_setup_shared_lmt_region(dev->mbox, true, mz->iova);
	if (rc) {
		errno = rc;
		goto free;
	}

	dev->lmt_base = mz->iova;
	dev->lmt_mz = mz;
	/* Base LMT address should be chosen from only those pci funcs which
	 * participate in LMT shared mode.
	 */
	if (!dev->disable_shared_lmt) {
		idev = idev_get_cfg();
		if (!idev) {
			errno = EFAULT;
			goto free;
		}

		if (!__atomic_load_n(&idev->lmt_pf_func, __ATOMIC_ACQUIRE)) {
			idev->lmt_base_addr = dev->lmt_base;
			idev->lmt_pf_func = dev->pf_func;
			idev->num_lmtlines = RVU_LMT_LINE_MAX;
		}
	}

	return 0;
free:
	plt_memzone_free(mz);
fail:
	return -errno;
}

static bool
dev_cache_line_size_valid(void)
{
	if (roc_model_is_cn9k()) {
		if (PLT_CACHE_LINE_SIZE != 128) {
			plt_err("Cache line size of %d is wrong for CN9K",
				PLT_CACHE_LINE_SIZE);
			return false;
		}
	} else if (roc_model_is_cn10k()) {
		if (PLT_CACHE_LINE_SIZE == 128) {
			plt_warn("Cache line size of %d might affect performance",
				 PLT_CACHE_LINE_SIZE);
		} else if (PLT_CACHE_LINE_SIZE != 64) {
			plt_err("Cache line size of %d is wrong for CN10K",
				PLT_CACHE_LINE_SIZE);
			return false;
		}
	}

	return true;
}

int
dev_init(struct dev *dev, struct plt_pci_device *pci_dev)
{
	char name[MBOX_HANDLER_NAME_MAX_LEN];
	int direction, up_direction, rc;
	uintptr_t bar2, bar4, mbox;
	uintptr_t vf_mbase = 0;
	uint64_t intr_offset;

	if (!dev_cache_line_size_valid())
		return -EFAULT;

	if (!roc_plt_lmt_validate()) {
		plt_err("Failed to validate LMT line");
		return -EFAULT;
	}

	bar2 = (uintptr_t)pci_dev->mem_resource[2].addr;
	bar4 = (uintptr_t)pci_dev->mem_resource[4].addr;
	if (bar2 == 0 || bar4 == 0) {
		plt_err("Failed to get PCI bars");
		rc = -ENODEV;
		goto error;
	}

	/* Trigger fault on bar2 and bar4 regions
	 * to avoid BUG_ON in remap_pfn_range()
	 * in latest kernel.
	 */
	*(volatile uint64_t *)bar2;
	*(volatile uint64_t *)bar4;

	/* Check ROC model supported */
	if (roc_model->flag == 0) {
		rc = UTIL_ERR_INVALID_MODEL;
		goto error;
	}

	dev->maxvf = pci_dev->max_vfs;
	dev->bar2 = bar2;
	dev->bar4 = bar4;
	dev_vf_hwcap_update(pci_dev, dev);

	if (dev_is_vf(dev)) {
		mbox = (roc_model_is_cn9k() ?
			bar4 : (bar2 + RVU_VF_MBOX_REGION));
		direction = MBOX_DIR_VFPF;
		up_direction = MBOX_DIR_VFPF_UP;
		intr_offset = RVU_VF_INT;
	} else {
		mbox = bar4;
		direction = MBOX_DIR_PFAF;
		up_direction = MBOX_DIR_PFAF_UP;
		intr_offset = RVU_PF_INT;
	}

	/* Clear all RVUM interrupts */
	clear_rvum_interrupts(dev);

	/* Initialize the local mbox */
	rc = mbox_init(&dev->mbox_local, mbox, bar2, direction, 1, intr_offset);
	if (rc)
		goto error;
	dev->mbox = &dev->mbox_local;

	rc = mbox_init(&dev->mbox_up, mbox, bar2, up_direction, 1, intr_offset);
	if (rc)
		goto mbox_fini;

	/* Register mbox interrupts */
	rc = dev_mbox_register_irq(pci_dev, dev);
	if (rc)
		goto mbox_fini;

	/* Check the readiness of PF/VF */
	rc = send_ready_msg(dev->mbox, &dev->pf_func);
	if (rc)
		goto mbox_unregister;

	dev->pf = dev_get_pf(dev->pf_func);
	dev->vf = dev_get_vf(dev->pf_func);
	memset(&dev->active_vfs, 0, sizeof(dev->active_vfs));

	/* Allocate memory for device ops */
	dev->ops = plt_zmalloc(sizeof(struct dev_ops), 0);
	if (dev->ops == NULL) {
		rc = -ENOMEM;
		goto mbox_unregister;
	}

	/* Found VF devices in a PF device */
	if (pci_dev->max_vfs > 0) {
		/* Remap mbox area for all vf's */
		vf_mbase = dev_vf_mbase_get(pci_dev, dev);
		if (!vf_mbase) {
			rc = -ENODEV;
			goto mbox_unregister;
		}
		/* Init mbox object */
		rc = mbox_init(&dev->mbox_vfpf, vf_mbase, bar2, MBOX_DIR_PFVF,
			       pci_dev->max_vfs, intr_offset);
		if (rc)
			goto iounmap;

		/* PF -> VF UP messages */
		rc = mbox_init(&dev->mbox_vfpf_up, vf_mbase, bar2,
			       MBOX_DIR_PFVF_UP, pci_dev->max_vfs, intr_offset);
		if (rc)
			goto iounmap;

		/* Create a thread for handling msgs from VFs */
		pthread_cond_init(&dev->sync.pfvf_msg_cond, NULL);
		pthread_mutex_init(&dev->sync.mutex, NULL);

		snprintf(name, MBOX_HANDLER_NAME_MAX_LEN, "mbox_pf%d", dev->pf);
		dev->sync.start_thread = true;
		rc = plt_thread_create_control(&dev->sync.pfvf_msg_thread, name,
				pf_vf_mbox_thread_main, dev);
		if (rc != 0) {
			plt_err("Failed to create thread for VF mbox handling\n");
			goto thread_fail;
		}
	}

	/* Register VF-FLR irq handlers */
	if (!dev_is_vf(dev)) {
		rc = dev_vf_flr_register_irqs(pci_dev, dev);
		if (rc)
			goto stop_msg_thrd;
	}
	dev->mbox_active = 1;

	rc = npa_lf_init(dev, pci_dev);
	if (rc)
		goto stop_msg_thrd;

	/* Setup LMT line base */
	rc = dev_lmt_setup(dev);
	if (rc)
		goto stop_msg_thrd;

	return rc;
stop_msg_thrd:
	/* Exiting the mbox sync thread */
	if (dev->sync.start_thread) {
		dev->sync.start_thread = false;
		pthread_cond_signal(&dev->sync.pfvf_msg_cond);
		plt_thread_join(dev->sync.pfvf_msg_thread, NULL);
	}
thread_fail:
	pthread_mutex_destroy(&dev->sync.mutex);
	pthread_cond_destroy(&dev->sync.pfvf_msg_cond);
iounmap:
	dev_vf_mbase_put(pci_dev, vf_mbase);
mbox_unregister:
	mbox_unregister_irq(pci_dev, dev);
	if (dev->ops)
		plt_free(dev->ops);
mbox_fini:
	mbox_fini(dev->mbox);
	mbox_fini(&dev->mbox_up);
error:
	return rc;
}

int
dev_fini(struct dev *dev, struct plt_pci_device *pci_dev)
{
	struct plt_intr_handle *intr_handle = pci_dev->intr_handle;
	struct mbox *mbox;

	/* Check if this dev hosts npalf and has 1+ refs */
	if (idev_npa_lf_active(dev) > 1)
		return -EAGAIN;

	/* Exiting the mbox sync thread */
	if (dev->sync.start_thread) {
		dev->sync.start_thread = false;
		pthread_cond_signal(&dev->sync.pfvf_msg_cond);
		plt_thread_join(dev->sync.pfvf_msg_thread, NULL);
		pthread_mutex_destroy(&dev->sync.mutex);
		pthread_cond_destroy(&dev->sync.pfvf_msg_cond);
	}

	/* Clear references to this pci dev */
	npa_lf_fini();

	/* Releasing memory allocated for lmt region */
	if (dev->lmt_mz)
		plt_memzone_free(dev->lmt_mz);

	mbox_unregister_irq(pci_dev, dev);

	if (!dev_is_vf(dev))
		vf_flr_unregister_irqs(pci_dev, dev);
	/* Release PF - VF */
	mbox = &dev->mbox_vfpf;
	if (mbox->hwbase && mbox->dev)
		dev_vf_mbase_put(pci_dev, mbox->hwbase);

	if (dev->ops)
		plt_free(dev->ops);

	mbox_fini(mbox);
	mbox = &dev->mbox_vfpf_up;
	mbox_fini(mbox);

	/* Release PF - AF */
	mbox = dev->mbox;
	mbox_fini(mbox);
	mbox = &dev->mbox_up;
	mbox_fini(mbox);
	dev->mbox_active = 0;

	/* Disable MSIX vectors */
	dev_irqs_disable(intr_handle);
	return 0;
}
