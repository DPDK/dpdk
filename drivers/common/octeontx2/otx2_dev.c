/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include <fcntl.h>
#include <inttypes.h>
#include <sys/mman.h>
#include <unistd.h>

#include <rte_alarm.h>
#include <rte_common.h>
#include <rte_eal.h>
#include <rte_memcpy.h>
#include <rte_eal_paging.h>

#include "otx2_dev.h"
#include "otx2_mbox.h"

#define RVU_MAX_VF		64 /* RVU_PF_VFPF_MBOX_INT(0..1) */
#define RVU_MAX_INT_RETRY	3

/* PF/VF message handling timer */
#define VF_PF_MBOX_TIMER_MS	(20 * 1000)

static void *
mbox_mem_map(off_t off, size_t size)
{
	void *va = MAP_FAILED;
	int mem_fd;

	if (size <= 0)
		goto error;

	mem_fd = open("/dev/mem", O_RDWR);
	if (mem_fd < 0)
		goto error;

	va = rte_mem_map(NULL, size, RTE_PROT_READ | RTE_PROT_WRITE,
			RTE_MAP_SHARED, mem_fd, off);
	close(mem_fd);

	if (va == NULL)
		otx2_err("Failed to mmap sz=0x%zx, fd=%d, off=%jd",
			 size, mem_fd, (intmax_t)off);
error:
	return va;
}

static void
mbox_mem_unmap(void *va, size_t size)
{
	if (va)
		rte_mem_unmap(va, size);
}

static int
pf_af_sync_msg(struct otx2_dev *dev, struct mbox_msghdr **rsp)
{
	uint32_t timeout = 0, sleep = 1; struct otx2_mbox *mbox = dev->mbox;
	struct otx2_mbox_dev *mdev = &mbox->dev[0];
	volatile uint64_t int_status;
	struct mbox_msghdr *msghdr;
	uint64_t off;
	int rc = 0;

	/* We need to disable PF interrupts. We are in timer interrupt */
	otx2_write64(~0ull, dev->bar2 + RVU_PF_INT_ENA_W1C);

	/* Send message */
	otx2_mbox_msg_send(mbox, 0);

	do {
		rte_delay_ms(sleep);
		timeout += sleep;
		if (timeout >= MBOX_RSP_TIMEOUT) {
			otx2_err("Message timeout: %dms", MBOX_RSP_TIMEOUT);
			rc = -EIO;
			break;
		}
		int_status = otx2_read64(dev->bar2 + RVU_PF_INT);
	} while ((int_status & 0x1) != 0x1);

	/* Clear */
	otx2_write64(int_status, dev->bar2 + RVU_PF_INT);

	/* Enable interrupts */
	otx2_write64(~0ull, dev->bar2 + RVU_PF_INT_ENA_W1S);

	if (rc == 0) {
		/* Get message */
		off = mbox->rx_start +
			RTE_ALIGN(sizeof(struct mbox_hdr), MBOX_MSG_ALIGN);
		msghdr = (struct mbox_msghdr *)((uintptr_t)mdev->mbase + off);
		if (rsp)
			*rsp = msghdr;
		rc = msghdr->rc;
	}

	return rc;
}

static int
af_pf_wait_msg(struct otx2_dev *dev, uint16_t vf, int num_msg)
{
	uint32_t timeout = 0, sleep = 1; struct otx2_mbox *mbox = dev->mbox;
	struct otx2_mbox_dev *mdev = &mbox->dev[0];
	volatile uint64_t int_status;
	struct mbox_hdr *req_hdr;
	struct mbox_msghdr *msg;
	struct mbox_msghdr *rsp;
	uint64_t offset;
	size_t size;
	int i;

	/* We need to disable PF interrupts. We are in timer interrupt */
	otx2_write64(~0ull, dev->bar2 + RVU_PF_INT_ENA_W1C);

	/* Send message */
	otx2_mbox_msg_send(mbox, 0);

	do {
		rte_delay_ms(sleep);
		timeout++;
		if (timeout >= MBOX_RSP_TIMEOUT) {
			otx2_err("Routed messages %d timeout: %dms",
				 num_msg, MBOX_RSP_TIMEOUT);
			break;
		}
		int_status = otx2_read64(dev->bar2 + RVU_PF_INT);
	} while ((int_status & 0x1) != 0x1);

	/* Clear */
	otx2_write64(~0ull, dev->bar2 + RVU_PF_INT);

	/* Enable interrupts */
	otx2_write64(~0ull, dev->bar2 + RVU_PF_INT_ENA_W1S);

	rte_spinlock_lock(&mdev->mbox_lock);

	req_hdr = (struct mbox_hdr *)((uintptr_t)mdev->mbase + mbox->rx_start);
	if (req_hdr->num_msgs != num_msg)
		otx2_err("Routed messages: %d received: %d", num_msg,
			 req_hdr->num_msgs);

	/* Get messages from mbox */
	offset = mbox->rx_start +
			RTE_ALIGN(sizeof(struct mbox_hdr), MBOX_MSG_ALIGN);
	for (i = 0; i < req_hdr->num_msgs; i++) {
		msg = (struct mbox_msghdr *)((uintptr_t)mdev->mbase + offset);
		size = mbox->rx_start + msg->next_msgoff - offset;

		/* Reserve PF/VF mbox message */
		size = RTE_ALIGN(size, MBOX_MSG_ALIGN);
		rsp = otx2_mbox_alloc_msg(&dev->mbox_vfpf, vf, size);
		otx2_mbox_rsp_init(msg->id, rsp);

		/* Copy message from AF<->PF mbox to PF<->VF mbox */
		otx2_mbox_memcpy((uint8_t *)rsp + sizeof(struct mbox_msghdr),
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
			if (dev->ops && dev->ops->link_status_get)
				dev->ops->link_status_get(dev, &linfo);

			sz = RTE_ALIGN(otx2_mbox_id2size(
				MBOX_MSG_CGX_LINK_EVENT), MBOX_MSG_ALIGN);
			/* Prepare the message to be sent */
			vf_msg = otx2_mbox_alloc_msg(&dev->mbox_vfpf_up, vf,
						     sz);
			otx2_mbox_req_init(MBOX_MSG_CGX_LINK_EVENT, vf_msg);
			memcpy((uint8_t *)vf_msg + sizeof(struct mbox_msghdr),
			       &linfo, sizeof(struct cgx_link_user_info));

			vf_msg->rc = msg->rc;
			vf_msg->pcifunc = msg->pcifunc;
			/* Send to VF */
			otx2_mbox_msg_send(&dev->mbox_vfpf_up, vf);
		}
		offset = mbox->rx_start + msg->next_msgoff;
	}
	rte_spinlock_unlock(&mdev->mbox_lock);

	return req_hdr->num_msgs;
}

static int
vf_pf_process_msgs(struct otx2_dev *dev, uint16_t vf)
{
	int offset, routed = 0; struct otx2_mbox *mbox = &dev->mbox_vfpf;
	struct otx2_mbox_dev *mdev = &mbox->dev[vf];
	struct mbox_hdr *req_hdr;
	struct mbox_msghdr *msg;
	size_t size;
	uint16_t i;

	req_hdr = (struct mbox_hdr *)((uintptr_t)mdev->mbase + mbox->rx_start);
	if (!req_hdr->num_msgs)
		return 0;

	offset = mbox->rx_start + RTE_ALIGN(sizeof(*req_hdr), MBOX_MSG_ALIGN);

	for (i = 0; i < req_hdr->num_msgs; i++) {

		msg = (struct mbox_msghdr *)((uintptr_t)mdev->mbase + offset);
		size = mbox->rx_start + msg->next_msgoff - offset;

		/* RVU_PF_FUNC_S */
		msg->pcifunc = otx2_pfvf_func(dev->pf, vf);

		if (msg->id == MBOX_MSG_READY) {
			struct ready_msg_rsp *rsp;
			uint16_t max_bits = sizeof(dev->active_vfs[0]) * 8;

			/* Handle READY message in PF */
			dev->active_vfs[vf / max_bits] |=
						BIT_ULL(vf % max_bits);
			rsp = (struct ready_msg_rsp *)
			       otx2_mbox_alloc_msg(mbox, vf, sizeof(*rsp));
			otx2_mbox_rsp_init(msg->id, rsp);

			/* PF/VF function ID */
			rsp->hdr.pcifunc = msg->pcifunc;
			rsp->hdr.rc = 0;
		} else {
			struct mbox_msghdr *af_req;
			/* Reserve AF/PF mbox message */
			size = RTE_ALIGN(size, MBOX_MSG_ALIGN);
			af_req = otx2_mbox_alloc_msg(dev->mbox, 0, size);
			otx2_mbox_req_init(msg->id, af_req);

			/* Copy message from VF<->PF mbox to PF<->AF mbox */
			otx2_mbox_memcpy((uint8_t *)af_req +
				   sizeof(struct mbox_msghdr),
				   (uint8_t *)msg + sizeof(struct mbox_msghdr),
				   size - sizeof(struct mbox_msghdr));
			af_req->pcifunc = msg->pcifunc;
			routed++;
		}
		offset = mbox->rx_start + msg->next_msgoff;
	}

	if (routed > 0) {
		otx2_base_dbg("pf:%d routed %d messages from vf:%d to AF",
			      dev->pf, routed, vf);
		af_pf_wait_msg(dev, vf, routed);
		otx2_mbox_reset(dev->mbox, 0);
	}

	/* Send mbox responses to VF */
	if (mdev->num_msgs) {
		otx2_base_dbg("pf:%d reply %d messages to vf:%d",
			      dev->pf, mdev->num_msgs, vf);
		otx2_mbox_msg_send(mbox, vf);
	}

	return i;
}

static int
vf_pf_process_up_msgs(struct otx2_dev *dev, uint16_t vf)
{
	struct otx2_mbox *mbox = &dev->mbox_vfpf_up;
	struct otx2_mbox_dev *mdev = &mbox->dev[vf];
	struct mbox_hdr *req_hdr;
	struct mbox_msghdr *msg;
	int msgs_acked = 0;
	int offset;
	uint16_t i;

	req_hdr = (struct mbox_hdr *)((uintptr_t)mdev->mbase + mbox->rx_start);
	if (req_hdr->num_msgs == 0)
		return 0;

	offset = mbox->rx_start + RTE_ALIGN(sizeof(*req_hdr), MBOX_MSG_ALIGN);

	for (i = 0; i < req_hdr->num_msgs; i++) {
		msg = (struct mbox_msghdr *)((uintptr_t)mdev->mbase + offset);

		msgs_acked++;
		/* RVU_PF_FUNC_S */
		msg->pcifunc = otx2_pfvf_func(dev->pf, vf);

		switch (msg->id) {
		case MBOX_MSG_CGX_LINK_EVENT:
			otx2_base_dbg("PF: Msg 0x%x (%s) fn:0x%x (pf:%d,vf:%d)",
				      msg->id, otx2_mbox_id2name(msg->id),
				      msg->pcifunc, otx2_get_pf(msg->pcifunc),
				      otx2_get_vf(msg->pcifunc));
			break;
		case MBOX_MSG_CGX_PTP_RX_INFO:
			otx2_base_dbg("PF: Msg 0x%x (%s) fn:0x%x (pf:%d,vf:%d)",
				      msg->id, otx2_mbox_id2name(msg->id),
				      msg->pcifunc, otx2_get_pf(msg->pcifunc),
				      otx2_get_vf(msg->pcifunc));
			break;
		default:
			otx2_err("Not handled UP msg 0x%x (%s) func:0x%x",
				 msg->id, otx2_mbox_id2name(msg->id),
				 msg->pcifunc);
		}
		offset = mbox->rx_start + msg->next_msgoff;
	}
	otx2_mbox_reset(mbox, vf);
	mdev->msgs_acked = msgs_acked;
	rte_wmb();

	return i;
}

static void
otx2_vf_pf_mbox_handle_msg(void *param)
{
	uint16_t vf, max_vf, max_bits;
	struct otx2_dev *dev = param;

	max_bits = sizeof(dev->intr.bits[0]) * sizeof(uint64_t);
	max_vf = max_bits * MAX_VFPF_DWORD_BITS;

	for (vf = 0; vf < max_vf; vf++) {
		if (dev->intr.bits[vf/max_bits] & BIT_ULL(vf%max_bits)) {
			otx2_base_dbg("Process vf:%d request (pf:%d, vf:%d)",
				       vf, dev->pf, dev->vf);
			vf_pf_process_msgs(dev, vf);
			/* UP messages */
			vf_pf_process_up_msgs(dev, vf);
			dev->intr.bits[vf/max_bits] &= ~(BIT_ULL(vf%max_bits));
		}
	}
	dev->timer_set = 0;
}

static void
otx2_vf_pf_mbox_irq(void *param)
{
	struct otx2_dev *dev = param;
	bool alarm_set = false;
	uint64_t intr;
	int vfpf;

	for (vfpf = 0; vfpf < MAX_VFPF_DWORD_BITS; ++vfpf) {
		intr = otx2_read64(dev->bar2 + RVU_PF_VFPF_MBOX_INTX(vfpf));
		if (!intr)
			continue;

		otx2_base_dbg("vfpf: %d intr: 0x%" PRIx64 " (pf:%d, vf:%d)",
			      vfpf, intr, dev->pf, dev->vf);

		/* Save and clear intr bits */
		dev->intr.bits[vfpf] |= intr;
		otx2_write64(intr, dev->bar2 + RVU_PF_VFPF_MBOX_INTX(vfpf));
		alarm_set = true;
	}

	if (!dev->timer_set && alarm_set) {
		dev->timer_set = 1;
		/* Start timer to handle messages */
		rte_eal_alarm_set(VF_PF_MBOX_TIMER_MS,
				  otx2_vf_pf_mbox_handle_msg, dev);
	}
}

static void
otx2_process_msgs(struct otx2_dev *dev, struct otx2_mbox *mbox)
{
	struct otx2_mbox_dev *mdev = &mbox->dev[0];
	struct mbox_hdr *req_hdr;
	struct mbox_msghdr *msg;
	int msgs_acked = 0;
	int offset;
	uint16_t i;

	req_hdr = (struct mbox_hdr *)((uintptr_t)mdev->mbase + mbox->rx_start);
	if (req_hdr->num_msgs == 0)
		return;

	offset = mbox->rx_start + RTE_ALIGN(sizeof(*req_hdr), MBOX_MSG_ALIGN);
	for (i = 0; i < req_hdr->num_msgs; i++) {
		msg = (struct mbox_msghdr *)((uintptr_t)mdev->mbase + offset);

		msgs_acked++;
		otx2_base_dbg("Message 0x%x (%s) pf:%d/vf:%d",
			      msg->id, otx2_mbox_id2name(msg->id),
			      otx2_get_pf(msg->pcifunc),
			      otx2_get_vf(msg->pcifunc));

		switch (msg->id) {
			/* Add message id's that are handled here */
		case MBOX_MSG_READY:
			/* Get our identity */
			dev->pf_func = msg->pcifunc;
			break;

		default:
			if (msg->rc)
				otx2_err("Message (%s) response has err=%d",
					 otx2_mbox_id2name(msg->id), msg->rc);
			break;
		}
		offset = mbox->rx_start + msg->next_msgoff;
	}

	otx2_mbox_reset(mbox, 0);
	/* Update acked if someone is waiting a message */
	mdev->msgs_acked = msgs_acked;
	rte_wmb();
}

/* Copies the message received from AF and sends it to VF */
static void
pf_vf_mbox_send_up_msg(struct otx2_dev *dev, void *rec_msg)
{
	uint16_t max_bits = sizeof(dev->active_vfs[0]) * sizeof(uint64_t);
	struct otx2_mbox *vf_mbox = &dev->mbox_vfpf_up;
	struct msg_req *msg = rec_msg;
	struct mbox_msghdr *vf_msg;
	uint16_t vf;
	size_t size;

	size = RTE_ALIGN(otx2_mbox_id2size(msg->hdr.id), MBOX_MSG_ALIGN);
	/* Send UP message to all VF's */
	for (vf = 0; vf < vf_mbox->ndevs; vf++) {
		/* VF active */
		if (!(dev->active_vfs[vf / max_bits] & (BIT_ULL(vf))))
			continue;

		otx2_base_dbg("(%s) size: %zx to VF: %d",
			      otx2_mbox_id2name(msg->hdr.id), size, vf);

		/* Reserve PF/VF mbox message */
		vf_msg = otx2_mbox_alloc_msg(vf_mbox, vf, size);
		if (!vf_msg) {
			otx2_err("Failed to alloc VF%d UP message", vf);
			continue;
		}
		otx2_mbox_req_init(msg->hdr.id, vf_msg);

		/*
		 * Copy message from AF<->PF UP mbox
		 * to PF<->VF UP mbox
		 */
		otx2_mbox_memcpy((uint8_t *)vf_msg +
				 sizeof(struct mbox_msghdr), (uint8_t *)msg
				 + sizeof(struct mbox_msghdr), size -
				 sizeof(struct mbox_msghdr));

		vf_msg->rc = msg->hdr.rc;
		/* Set PF to be a sender */
		vf_msg->pcifunc = dev->pf_func;

		/* Send to VF */
		otx2_mbox_msg_send(vf_mbox, vf);
	}
}

static int
otx2_mbox_up_handler_cgx_link_event(struct otx2_dev *dev,
				    struct cgx_link_info_msg *msg,
				    struct msg_rsp *rsp)
{
	struct cgx_link_user_info *linfo = &msg->link_info;

	otx2_base_dbg("pf:%d/vf:%d NIC Link %s --> 0x%x (%s) from: pf:%d/vf:%d",
		      otx2_get_pf(dev->pf_func), otx2_get_vf(dev->pf_func),
		      linfo->link_up ? "UP" : "DOWN", msg->hdr.id,
		      otx2_mbox_id2name(msg->hdr.id),
		      otx2_get_pf(msg->hdr.pcifunc),
		      otx2_get_vf(msg->hdr.pcifunc));

	/* PF gets link notification from AF */
	if (otx2_get_pf(msg->hdr.pcifunc) == 0) {
		if (dev->ops && dev->ops->link_status_update)
			dev->ops->link_status_update(dev, linfo);

		/* Forward the same message as received from AF to VF */
		pf_vf_mbox_send_up_msg(dev, msg);
	} else {
		/* VF gets link up notification */
		if (dev->ops && dev->ops->link_status_update)
			dev->ops->link_status_update(dev, linfo);
	}

	rsp->hdr.rc = 0;
	return 0;
}

static int
otx2_mbox_up_handler_cgx_ptp_rx_info(struct otx2_dev *dev,
				     struct cgx_ptp_rx_info_msg *msg,
				     struct msg_rsp *rsp)
{
	otx2_nix_dbg("pf:%d/vf:%d PTP mode %s --> 0x%x (%s) from: pf:%d/vf:%d",
		 otx2_get_pf(dev->pf_func),
		 otx2_get_vf(dev->pf_func),
		 msg->ptp_en ? "ENABLED" : "DISABLED",
		 msg->hdr.id, otx2_mbox_id2name(msg->hdr.id),
		 otx2_get_pf(msg->hdr.pcifunc),
		 otx2_get_vf(msg->hdr.pcifunc));

	/* PF gets PTP notification from AF */
	if (otx2_get_pf(msg->hdr.pcifunc) == 0) {
		if (dev->ops && dev->ops->ptp_info_update)
			dev->ops->ptp_info_update(dev, msg->ptp_en);

		/* Forward the same message as received from AF to VF */
		pf_vf_mbox_send_up_msg(dev, msg);
	} else {
		/* VF gets PTP notification */
		if (dev->ops && dev->ops->ptp_info_update)
			dev->ops->ptp_info_update(dev, msg->ptp_en);
	}

	rsp->hdr.rc = 0;
	return 0;
}

static int
mbox_process_msgs_up(struct otx2_dev *dev, struct mbox_msghdr *req)
{
	/* Check if valid, if not reply with a invalid msg */
	if (req->sig != OTX2_MBOX_REQ_SIG)
		return -EIO;

	switch (req->id) {
#define M(_name, _id, _fn_name, _req_type, _rsp_type)		\
	case _id: {						\
		struct _rsp_type *rsp;				\
		int err;					\
								\
		rsp = (struct _rsp_type *)otx2_mbox_alloc_msg(	\
			&dev->mbox_up, 0,			\
			sizeof(struct _rsp_type));		\
		if (!rsp)					\
			return -ENOMEM;				\
								\
		rsp->hdr.id = _id;				\
		rsp->hdr.sig = OTX2_MBOX_RSP_SIG;		\
		rsp->hdr.pcifunc = dev->pf_func;		\
		rsp->hdr.rc = 0;				\
								\
		err = otx2_mbox_up_handler_ ## _fn_name(	\
			dev, (struct _req_type *)req, rsp);	\
		return err;					\
	}
MBOX_UP_CGX_MESSAGES
#undef M

	default :
		otx2_reply_invalid_msg(&dev->mbox_up, 0, 0, req->id);
	}

	return -ENODEV;
}

static void
otx2_process_msgs_up(struct otx2_dev *dev, struct otx2_mbox *mbox)
{
	struct otx2_mbox_dev *mdev = &mbox->dev[0];
	struct mbox_hdr *req_hdr;
	struct mbox_msghdr *msg;
	int i, err, offset;

	req_hdr = (struct mbox_hdr *)((uintptr_t)mdev->mbase + mbox->rx_start);
	if (req_hdr->num_msgs == 0)
		return;

	offset = mbox->rx_start + RTE_ALIGN(sizeof(*req_hdr), MBOX_MSG_ALIGN);
	for (i = 0; i < req_hdr->num_msgs; i++) {
		msg = (struct mbox_msghdr *)((uintptr_t)mdev->mbase + offset);

		otx2_base_dbg("Message 0x%x (%s) pf:%d/vf:%d",
				msg->id, otx2_mbox_id2name(msg->id),
				otx2_get_pf(msg->pcifunc),
				otx2_get_vf(msg->pcifunc));
		err = mbox_process_msgs_up(dev, msg);
		if (err)
			otx2_err("Error %d handling 0x%x (%s)",
				 err, msg->id, otx2_mbox_id2name(msg->id));
		offset = mbox->rx_start + msg->next_msgoff;
	}
	/* Send mbox responses */
	if (mdev->num_msgs) {
		otx2_base_dbg("Reply num_msgs:%d", mdev->num_msgs);
		otx2_mbox_msg_send(mbox, 0);
	}
}

static void
otx2_pf_vf_mbox_irq(void *param)
{
	struct otx2_dev *dev = param;
	uint64_t intr;

	intr = otx2_read64(dev->bar2 + RVU_VF_INT);
	if (intr == 0)
		otx2_base_dbg("Proceeding to check mbox UP messages if any");

	otx2_write64(intr, dev->bar2 + RVU_VF_INT);
	otx2_base_dbg("Irq 0x%" PRIx64 "(pf:%d,vf:%d)", intr, dev->pf, dev->vf);

	/* First process all configuration messages */
	otx2_process_msgs(dev, dev->mbox);

	/* Process Uplink messages */
	otx2_process_msgs_up(dev, &dev->mbox_up);
}

static void
otx2_af_pf_mbox_irq(void *param)
{
	struct otx2_dev *dev = param;
	uint64_t intr;

	intr = otx2_read64(dev->bar2 + RVU_PF_INT);
	if (intr == 0)
		otx2_base_dbg("Proceeding to check mbox UP messages if any");

	otx2_write64(intr, dev->bar2 + RVU_PF_INT);
	otx2_base_dbg("Irq 0x%" PRIx64 "(pf:%d,vf:%d)", intr, dev->pf, dev->vf);

	/* First process all configuration messages */
	otx2_process_msgs(dev, dev->mbox);

	/* Process Uplink messages */
	otx2_process_msgs_up(dev, &dev->mbox_up);
}

static int
mbox_register_pf_irq(struct rte_pci_device *pci_dev, struct otx2_dev *dev)
{
	struct rte_intr_handle *intr_handle = pci_dev->intr_handle;
	int i, rc;

	/* HW clear irq */
	for (i = 0; i < MAX_VFPF_DWORD_BITS; ++i)
		otx2_write64(~0ull, dev->bar2 +
			     RVU_PF_VFPF_MBOX_INT_ENA_W1CX(i));

	otx2_write64(~0ull, dev->bar2 + RVU_PF_INT_ENA_W1C);

	dev->timer_set = 0;

	/* MBOX interrupt for VF(0...63) <-> PF */
	rc = otx2_register_irq(intr_handle, otx2_vf_pf_mbox_irq, dev,
			       RVU_PF_INT_VEC_VFPF_MBOX0);

	if (rc) {
		otx2_err("Fail to register PF(VF0-63) mbox irq");
		return rc;
	}
	/* MBOX interrupt for VF(64...128) <-> PF */
	rc = otx2_register_irq(intr_handle, otx2_vf_pf_mbox_irq, dev,
			       RVU_PF_INT_VEC_VFPF_MBOX1);

	if (rc) {
		otx2_err("Fail to register PF(VF64-128) mbox irq");
		return rc;
	}
	/* MBOX interrupt AF <-> PF */
	rc = otx2_register_irq(intr_handle, otx2_af_pf_mbox_irq,
			       dev, RVU_PF_INT_VEC_AFPF_MBOX);
	if (rc) {
		otx2_err("Fail to register AF<->PF mbox irq");
		return rc;
	}

	/* HW enable intr */
	for (i = 0; i < MAX_VFPF_DWORD_BITS; ++i)
		otx2_write64(~0ull, dev->bar2 +
			RVU_PF_VFPF_MBOX_INT_ENA_W1SX(i));

	otx2_write64(~0ull, dev->bar2 + RVU_PF_INT);
	otx2_write64(~0ull, dev->bar2 + RVU_PF_INT_ENA_W1S);

	return rc;
}

static int
mbox_register_vf_irq(struct rte_pci_device *pci_dev, struct otx2_dev *dev)
{
	struct rte_intr_handle *intr_handle = pci_dev->intr_handle;
	int rc;

	/* Clear irq */
	otx2_write64(~0ull, dev->bar2 + RVU_VF_INT_ENA_W1C);

	/* MBOX interrupt PF <-> VF */
	rc = otx2_register_irq(intr_handle, otx2_pf_vf_mbox_irq,
			       dev, RVU_VF_INT_VEC_MBOX);
	if (rc) {
		otx2_err("Fail to register PF<->VF mbox irq");
		return rc;
	}

	/* HW enable intr */
	otx2_write64(~0ull, dev->bar2 + RVU_VF_INT);
	otx2_write64(~0ull, dev->bar2 + RVU_VF_INT_ENA_W1S);

	return rc;
}

static int
mbox_register_irq(struct rte_pci_device *pci_dev, struct otx2_dev *dev)
{
	if (otx2_dev_is_vf(dev))
		return mbox_register_vf_irq(pci_dev, dev);
	else
		return mbox_register_pf_irq(pci_dev, dev);
}

static void
mbox_unregister_pf_irq(struct rte_pci_device *pci_dev, struct otx2_dev *dev)
{
	struct rte_intr_handle *intr_handle = pci_dev->intr_handle;
	int i;

	/* HW clear irq */
	for (i = 0; i < MAX_VFPF_DWORD_BITS; ++i)
		otx2_write64(~0ull, dev->bar2 +
			     RVU_PF_VFPF_MBOX_INT_ENA_W1CX(i));

	otx2_write64(~0ull, dev->bar2 + RVU_PF_INT_ENA_W1C);

	dev->timer_set = 0;

	rte_eal_alarm_cancel(otx2_vf_pf_mbox_handle_msg, dev);

	/* Unregister the interrupt handler for each vectors */
	/* MBOX interrupt for VF(0...63) <-> PF */
	otx2_unregister_irq(intr_handle, otx2_vf_pf_mbox_irq, dev,
			    RVU_PF_INT_VEC_VFPF_MBOX0);

	/* MBOX interrupt for VF(64...128) <-> PF */
	otx2_unregister_irq(intr_handle, otx2_vf_pf_mbox_irq, dev,
			    RVU_PF_INT_VEC_VFPF_MBOX1);

	/* MBOX interrupt AF <-> PF */
	otx2_unregister_irq(intr_handle, otx2_af_pf_mbox_irq, dev,
			    RVU_PF_INT_VEC_AFPF_MBOX);

}

static void
mbox_unregister_vf_irq(struct rte_pci_device *pci_dev, struct otx2_dev *dev)
{
	struct rte_intr_handle *intr_handle = pci_dev->intr_handle;

	/* Clear irq */
	otx2_write64(~0ull, dev->bar2 + RVU_VF_INT_ENA_W1C);

	/* Unregister the interrupt handler */
	otx2_unregister_irq(intr_handle, otx2_pf_vf_mbox_irq, dev,
			    RVU_VF_INT_VEC_MBOX);
}

static void
mbox_unregister_irq(struct rte_pci_device *pci_dev, struct otx2_dev *dev)
{
	if (otx2_dev_is_vf(dev))
		mbox_unregister_vf_irq(pci_dev, dev);
	else
		mbox_unregister_pf_irq(pci_dev, dev);
}

static int
vf_flr_send_msg(struct otx2_dev *dev, uint16_t vf)
{
	struct otx2_mbox *mbox = dev->mbox;
	struct msg_req *req;
	int rc;

	req = otx2_mbox_alloc_msg_vf_flr(mbox);
	/* Overwrite pcifunc to indicate VF */
	req->hdr.pcifunc = otx2_pfvf_func(dev->pf, vf);

	/* Sync message in interrupt context */
	rc = pf_af_sync_msg(dev, NULL);
	if (rc)
		otx2_err("Failed to send VF FLR mbox msg, rc=%d", rc);

	return rc;
}

static void
otx2_pf_vf_flr_irq(void *param)
{
	struct otx2_dev *dev = (struct otx2_dev *)param;
	uint16_t max_vf = 64, vf;
	uintptr_t bar2;
	uint64_t intr;
	int i;

	max_vf = (dev->maxvf > 0) ? dev->maxvf : 64;
	bar2 = dev->bar2;

	otx2_base_dbg("FLR VF interrupt: max_vf: %d", max_vf);

	for (i = 0; i < MAX_VFPF_DWORD_BITS; ++i) {
		intr = otx2_read64(bar2 + RVU_PF_VFFLR_INTX(i));
		if (!intr)
			continue;

		for (vf = 0; vf < max_vf; vf++) {
			if (!(intr & (1ULL << vf)))
				continue;

			otx2_base_dbg("FLR: i :%d intr: 0x%" PRIx64 ", vf-%d",
				      i, intr, (64 * i + vf));
			/* Clear interrupt */
			otx2_write64(BIT_ULL(vf), bar2 + RVU_PF_VFFLR_INTX(i));
			/* Disable the interrupt */
			otx2_write64(BIT_ULL(vf),
				     bar2 + RVU_PF_VFFLR_INT_ENA_W1CX(i));
			/* Inform AF about VF reset */
			vf_flr_send_msg(dev, vf);

			/* Signal FLR finish */
			otx2_write64(BIT_ULL(vf), bar2 + RVU_PF_VFTRPENDX(i));
			/* Enable interrupt */
			otx2_write64(~0ull,
				     bar2 + RVU_PF_VFFLR_INT_ENA_W1SX(i));
		}
	}
}

static int
vf_flr_unregister_irqs(struct rte_pci_device *pci_dev, struct otx2_dev *dev)
{
	struct rte_intr_handle *intr_handle = pci_dev->intr_handle;
	int i;

	otx2_base_dbg("Unregister VF FLR interrupts for %s", pci_dev->name);

	/* HW clear irq */
	for (i = 0; i < MAX_VFPF_DWORD_BITS; i++)
		otx2_write64(~0ull, dev->bar2 + RVU_PF_VFFLR_INT_ENA_W1CX(i));

	otx2_unregister_irq(intr_handle, otx2_pf_vf_flr_irq, dev,
			    RVU_PF_INT_VEC_VFFLR0);

	otx2_unregister_irq(intr_handle, otx2_pf_vf_flr_irq, dev,
			    RVU_PF_INT_VEC_VFFLR1);

	return 0;
}

static int
vf_flr_register_irqs(struct rte_pci_device *pci_dev, struct otx2_dev *dev)
{
	struct rte_intr_handle *handle = pci_dev->intr_handle;
	int i, rc;

	otx2_base_dbg("Register VF FLR interrupts for %s", pci_dev->name);

	rc = otx2_register_irq(handle, otx2_pf_vf_flr_irq, dev,
			       RVU_PF_INT_VEC_VFFLR0);
	if (rc)
		otx2_err("Failed to init RVU_PF_INT_VEC_VFFLR0 rc=%d", rc);

	rc = otx2_register_irq(handle, otx2_pf_vf_flr_irq, dev,
			       RVU_PF_INT_VEC_VFFLR1);
	if (rc)
		otx2_err("Failed to init RVU_PF_INT_VEC_VFFLR1 rc=%d", rc);

	/* Enable HW interrupt */
	for (i = 0; i < MAX_VFPF_DWORD_BITS; ++i) {
		otx2_write64(~0ull, dev->bar2 + RVU_PF_VFFLR_INTX(i));
		otx2_write64(~0ull, dev->bar2 + RVU_PF_VFTRPENDX(i));
		otx2_write64(~0ull, dev->bar2 + RVU_PF_VFFLR_INT_ENA_W1SX(i));
	}
	return 0;
}

/**
 * @internal
 * Get number of active VFs for the given PF device.
 */
int
otx2_dev_active_vfs(void *otx2_dev)
{
	struct otx2_dev *dev = otx2_dev;
	int i, count = 0;

	for (i = 0; i < MAX_VFPF_DWORD_BITS; i++)
		count += __builtin_popcount(dev->active_vfs[i]);

	return count;
}

static void
otx2_update_vf_hwcap(struct rte_pci_device *pci_dev, struct otx2_dev *dev)
{
	switch (pci_dev->id.device_id) {
	case PCI_DEVID_OCTEONTX2_RVU_PF:
		break;
	case PCI_DEVID_OCTEONTX2_RVU_SSO_TIM_VF:
	case PCI_DEVID_OCTEONTX2_RVU_NPA_VF:
	case PCI_DEVID_OCTEONTX2_RVU_CPT_VF:
	case PCI_DEVID_OCTEONTX2_RVU_AF_VF:
	case PCI_DEVID_OCTEONTX2_RVU_VF:
	case PCI_DEVID_OCTEONTX2_RVU_SDP_VF:
		dev->hwcap |= OTX2_HWCAP_F_VF;
		break;
	}
}

/**
 * @internal
 * Initialize the otx2 device
 */
int
otx2_dev_priv_init(struct rte_pci_device *pci_dev, void *otx2_dev)
{
	int up_direction = MBOX_DIR_PFAF_UP;
	int rc, direction = MBOX_DIR_PFAF;
	uint64_t intr_offset = RVU_PF_INT;
	struct otx2_dev *dev = otx2_dev;
	uintptr_t bar2, bar4;
	uint64_t bar4_addr;
	void *hwbase;

	bar2 = (uintptr_t)pci_dev->mem_resource[2].addr;
	bar4 = (uintptr_t)pci_dev->mem_resource[4].addr;

	if (bar2 == 0 || bar4 == 0) {
		otx2_err("Failed to get pci bars");
		rc = -ENODEV;
		goto error;
	}

	dev->node = pci_dev->device.numa_node;
	dev->maxvf = pci_dev->max_vfs;
	dev->bar2 = bar2;
	dev->bar4 = bar4;

	otx2_update_vf_hwcap(pci_dev, dev);

	if (otx2_dev_is_vf(dev)) {
		direction = MBOX_DIR_VFPF;
		up_direction = MBOX_DIR_VFPF_UP;
		intr_offset = RVU_VF_INT;
	}

	/* Initialize the local mbox */
	rc = otx2_mbox_init(&dev->mbox_local, bar4, bar2, direction, 1,
			    intr_offset);
	if (rc)
		goto error;
	dev->mbox = &dev->mbox_local;

	rc = otx2_mbox_init(&dev->mbox_up, bar4, bar2, up_direction, 1,
			    intr_offset);
	if (rc)
		goto error;

	/* Register mbox interrupts */
	rc = mbox_register_irq(pci_dev, dev);
	if (rc)
		goto mbox_fini;

	/* Check the readiness of PF/VF */
	rc = otx2_send_ready_msg(dev->mbox, &dev->pf_func);
	if (rc)
		goto mbox_unregister;

	dev->pf = otx2_get_pf(dev->pf_func);
	dev->vf = otx2_get_vf(dev->pf_func);
	memset(&dev->active_vfs, 0, sizeof(dev->active_vfs));

	/* Found VF devices in a PF device */
	if (pci_dev->max_vfs > 0) {

		/* Remap mbox area for all vf's */
		bar4_addr = otx2_read64(bar2 + RVU_PF_VF_BAR4_ADDR);
		if (bar4_addr == 0) {
			rc = -ENODEV;
			goto mbox_fini;
		}

		hwbase = mbox_mem_map(bar4_addr, MBOX_SIZE * pci_dev->max_vfs);
		if (hwbase == MAP_FAILED) {
			rc = -ENOMEM;
			goto mbox_fini;
		}
		/* Init mbox object */
		rc = otx2_mbox_init(&dev->mbox_vfpf, (uintptr_t)hwbase,
				    bar2, MBOX_DIR_PFVF, pci_dev->max_vfs,
				    intr_offset);
		if (rc)
			goto iounmap;

		/* PF -> VF UP messages */
		rc = otx2_mbox_init(&dev->mbox_vfpf_up, (uintptr_t)hwbase,
				    bar2, MBOX_DIR_PFVF_UP, pci_dev->max_vfs,
				    intr_offset);
		if (rc)
			goto mbox_fini;
	}

	/* Register VF-FLR irq handlers */
	if (otx2_dev_is_pf(dev)) {
		rc = vf_flr_register_irqs(pci_dev, dev);
		if (rc)
			goto iounmap;
	}
	dev->mbox_active = 1;
	return rc;

iounmap:
	mbox_mem_unmap(hwbase, MBOX_SIZE * pci_dev->max_vfs);
mbox_unregister:
	mbox_unregister_irq(pci_dev, dev);
mbox_fini:
	otx2_mbox_fini(dev->mbox);
	otx2_mbox_fini(&dev->mbox_up);
error:
	return rc;
}

/**
 * @internal
 * Finalize the otx2 device
 */
void
otx2_dev_fini(struct rte_pci_device *pci_dev, void *otx2_dev)
{
	struct rte_intr_handle *intr_handle = pci_dev->intr_handle;
	struct otx2_dev *dev = otx2_dev;
	struct otx2_idev_cfg *idev;
	struct otx2_mbox *mbox;

	/* Clear references to this pci dev */
	idev = otx2_intra_dev_get_cfg();
	if (idev->npa_lf && idev->npa_lf->pci_dev == pci_dev)
		idev->npa_lf = NULL;

	mbox_unregister_irq(pci_dev, dev);

	if (otx2_dev_is_pf(dev))
		vf_flr_unregister_irqs(pci_dev, dev);
	/* Release PF - VF */
	mbox = &dev->mbox_vfpf;
	if (mbox->hwbase && mbox->dev)
		mbox_mem_unmap((void *)mbox->hwbase,
			       MBOX_SIZE * pci_dev->max_vfs);
	otx2_mbox_fini(mbox);
	mbox = &dev->mbox_vfpf_up;
	otx2_mbox_fini(mbox);

	/* Release PF - AF */
	mbox = dev->mbox;
	otx2_mbox_fini(mbox);
	mbox = &dev->mbox_up;
	otx2_mbox_fini(mbox);
	dev->mbox_active = 0;

	/* Disable MSIX vectors */
	otx2_disable_irqs(intr_handle);
}
