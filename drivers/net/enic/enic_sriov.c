/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2024 Cisco Systems, Inc.  All rights reserved.
 */

#include <rte_memzone.h>
#include <ethdev_driver.h>

#include "enic_compat.h"
#include "enic.h"
#include "enic_sriov.h"

static int enic_check_chan_capability(struct enic *enic);
static int enic_register_vf(struct enic *enic);
static void enic_unregister_vf(struct enic *enic);

static const char *msg_type_str[ENIC_MBOX_MAX] = {
	"VF_CAPABILITY_REQUEST",
	"VF_CAPABILITY_REPLY",
	"VF_REGISTER_REQUEST",
	"VF_REGISTER_REPLY",
	"VF_UNREGISTER_REQUEST",
	"VF_UNREGISTER_REPLY",
	"PF_LINK_STATE_NOTIF",
	"PF_LINK_STATE_ACK",
	"PF_GET_STATS_REQUEST",
	"PF_GET_STATS_REPLY",
	"VF_ADD_DEL_MAC_REQUEST",
	"VF_ADD_DEL_MAC_REPLY",
	"PF_SET_ADMIN_MAC_NOTIF",
	"PF_SET_ADMIN_MAC_ACK",
	"VF_SET_PKT_FILTER_FLAGS_REQUEST",
	"VF_SET_PKT_FILTER_FLAGS_REPLY",
};

static const char *enic_mbox_msg_type_str(enum enic_mbox_msg_type type)
{
	if (type >= 0 && type < ENIC_MBOX_MAX)
		return msg_type_str[type];
	return "INVALID";
}

static bool admin_chan_enabled(struct enic *enic)
{
	return enic->admin_chan_enabled;
}

static void lock_admin_chan(struct enic *enic)
{
	pthread_mutex_lock(&enic->admin_chan_lock);
}

static void unlock_admin_chan(struct enic *enic)
{
	pthread_mutex_unlock(&enic->admin_chan_lock);
}

static int enic_enable_admin_rq(struct enic *enic)
{
	uint32_t rqbuf_size = ENIC_ADMIN_BUF_SIZE;
	uint32_t desc_count = 256;
	struct rq_enet_desc *rqd;
	struct vnic_rq *rq;
	struct vnic_cq *cq;
	rte_iova_t dma;
	uint32_t i;
	int cq_idx;
	int err = 0;
	char name[RTE_MEMZONE_NAMESIZE];
	static int instance;

	ENICPMD_FUNC_TRACE();
	rq = &enic->admin_rq;
	cq_idx = ENIC_ADMIN_RQ_CQ;
	cq = &enic->admin_cq[cq_idx];
	err = vnic_admin_rq_alloc(enic->vdev, rq, desc_count,
				  sizeof(struct rq_enet_desc));
	if (err) {
		dev_err(enic, "failed to allocate admin RQ\n");
		return err;
	}
	err = vnic_admin_cq_alloc(enic->vdev, cq, cq_idx,
		SOCKET_ID_ANY, desc_count, sizeof(struct cq_enet_rq_desc));
	if (err) {
		dev_err(enic, "failed to allocate CQ for admin RQ\n");
		return err;
	}

	vnic_rq_init(rq, cq_idx, 0, 0);
	vnic_cq_clean(cq);
	vnic_cq_init(cq,
		     0 /* flow_control_enable */,
		     1 /* color_enable */,
		     0 /* cq_head */,
		     0 /* cq_tail */,
		     1 /* cq_tail_color */,
		     1 /* interrupt_enable */,
		     1 /* cq_entry_enable */,
		     0 /* cq_message_enable */,
		     ENICPMD_LSC_INTR_OFFSET /* interrupt_offset */,
		     0 /* cq_message_addr */);
	vnic_rq_enable(rq);

	/*
	 * Allocate RQ DMA buffers. The admin chan reuses these
	 * buffers and never allocates new ones again
	 */
	snprintf((char *)name, sizeof(name), "admin-rq-buf-%d", instance++);
	rq->admin_msg_rz = rte_memzone_reserve_aligned((const char *)name,
			desc_count * rqbuf_size, SOCKET_ID_ANY,
			RTE_MEMZONE_IOVA_CONTIG, ENIC_PAGE_SIZE);
	if (!rq->admin_msg_rz)
		return -ENOMEM;

	memset(rq->admin_msg_rz->addr, 0, desc_count * rqbuf_size);

	dma = rq->admin_msg_rz->iova;
	rqd = rq->ring.descs;
	for (i = 0; i < desc_count; i++) {
		rq_enet_desc_enc(rqd, dma, RQ_ENET_TYPE_ONLY_SOP,
				 rqbuf_size);
		dma += rqbuf_size;
		rqd++;
	}
	rte_rmb();
	rq->posted_index = rq->ring.desc_count - 1;
	rq->admin_next_idx = 0;
	ENICPMD_LOG(DEBUG, "admin rq posted_index %u", rq->posted_index);
	iowrite32(rq->posted_index, &rq->ctrl->posted_index);
	rte_wmb();
	return err;
}

static int enic_enable_admin_wq(struct enic *enic)
{
	uint32_t wqbuf_size = ENIC_ADMIN_BUF_SIZE;
	uint32_t desc_count = 256;
	struct vnic_wq *wq;
	struct vnic_cq *cq;
	int cq_idx;
	int err = 0;
	char name[RTE_MEMZONE_NAMESIZE];
	static int instance;

	ENICPMD_FUNC_TRACE();
	wq = &enic->admin_wq;
	cq_idx = ENIC_ADMIN_WQ_CQ;
	cq = &enic->admin_cq[cq_idx];
	err = vnic_admin_wq_alloc(enic->vdev, wq, desc_count, sizeof(struct wq_enet_desc));
	if (err) {
		dev_err(enic, "failed to allocate admin WQ\n");
		return err;
	}
	err = vnic_admin_cq_alloc(enic->vdev, cq, cq_idx,
		SOCKET_ID_ANY, desc_count, sizeof(struct cq_enet_wq_desc));
	if (err) {
		vnic_wq_free(wq);
		dev_err(enic, "failed to allocate CQ for admin WQ\n");
		return err;
	}
	snprintf((char *)name, sizeof(name),
		 "vnic_cqmsg-%s-admin-wq-%d", enic->bdf_name, instance++);
	wq->cqmsg_rz = rte_memzone_reserve_aligned((const char *)name,
			sizeof(uint32_t), SOCKET_ID_ANY,
			RTE_MEMZONE_IOVA_CONTIG, ENIC_PAGE_SIZE);
	if (!wq->cqmsg_rz)
		return -ENOMEM;

	vnic_wq_init(wq, cq_idx, 0, 0);
	vnic_cq_clean(cq);
	vnic_cq_init(cq,
		     0 /* flow_control_enable */,
		     1 /* color_enable */,
		     0 /* cq_head */,
		     0 /* cq_tail */,
		     1 /* cq_tail_color */,
		     0 /* interrupt_enable */,
		     0 /* cq_entry_enable */,
		     1 /* cq_message_enable */,
		     0 /* interrupt offset */,
		     (uint64_t)wq->cqmsg_rz->iova);

	vnic_wq_enable(wq);

	snprintf((char *)name, sizeof(name), "admin-wq-buf-%d", instance++);
	wq->admin_msg_rz = rte_memzone_reserve_aligned((const char *)name,
			desc_count * wqbuf_size, SOCKET_ID_ANY,
			RTE_MEMZONE_IOVA_CONTIG, ENIC_PAGE_SIZE);
	if (!wq->admin_msg_rz)
		return -ENOMEM;

	return err;
}

static void enic_admin_wq_post(struct enic *enic, void *msg)
{
	struct wq_enet_desc *desc;
	struct enic_mbox_hdr *hdr;
	unsigned int head_idx;
	struct vnic_wq *wq;
	rte_iova_t dma;
	int msg_size;
	void *va;

	ENICPMD_FUNC_TRACE();
	wq = &enic->admin_wq;
	hdr = msg;
	msg_size = hdr->msg_len;
	RTE_VERIFY(msg_size < ENIC_ADMIN_BUF_SIZE);

	head_idx = wq->head_idx;
	desc = (struct wq_enet_desc *)wq->ring.descs;
	desc = desc + head_idx;

	/* Copy message to pre-allocated WQ DMA buffer */
	dma = wq->admin_msg_rz->iova + ENIC_ADMIN_BUF_SIZE * head_idx;
	va = (void *)((char *)wq->admin_msg_rz->addr + ENIC_ADMIN_BUF_SIZE * head_idx);
	memcpy(va, msg, msg_size);

	ENICPMD_LOG(DEBUG, "post admin wq msg at %u", head_idx);

	/* Send message to PF: loopback=1 */
	wq_enet_desc_enc(desc, dma, msg_size,
			 0 /* mss */,
			 0 /* header_len */,
			 0 /* offload_mode */, 1 /* eop */, 1 /* cq */,
			 0 /* fcoe */,
			 1 /* vlan_tag_insert */,
			 0 /* vlan_id */,
			 1 /* loopback */);
	head_idx = enic_ring_incr(wq->ring.desc_count, head_idx);
	rte_wmb();
	iowrite32_relaxed(head_idx, &wq->ctrl->posted_index);
	wq->head_idx = head_idx;
}

static void enic_mbox_init_msg_hdr(struct enic *enic, void *msg,
				   enum enic_mbox_msg_type type)
{
	struct enic_mbox_hdr *hdr;
	int len;

	switch (type) {
	case ENIC_MBOX_VF_CAPABILITY_REQUEST:
		len = sizeof(struct enic_mbox_vf_capability_msg);
		break;
	case ENIC_MBOX_VF_REGISTER_REQUEST:
		len = sizeof(struct enic_mbox_vf_register_msg);
		break;
	case ENIC_MBOX_VF_UNREGISTER_REQUEST:
		len = sizeof(struct enic_mbox_vf_unregister_msg);
		break;
	case ENIC_MBOX_VF_SET_PKT_FILTER_FLAGS_REQUEST:
		len = sizeof(struct enic_mbox_vf_set_pkt_filter_flags_msg);
		break;
	case ENIC_MBOX_PF_LINK_STATE_ACK:
		len = sizeof(struct enic_mbox_pf_link_state_ack_msg);
		break;
	case ENIC_MBOX_PF_GET_STATS_REPLY:
		len = sizeof(struct enic_mbox_pf_get_stats_reply_msg);
		break;
	case ENIC_MBOX_VF_ADD_DEL_MAC_REQUEST:
		len = sizeof(struct enic_mbox_vf_add_del_mac_msg);
		break;
	default:
		RTE_VERIFY(false);
		break;
	}
	memset(msg, 0, len);
	hdr = msg;
	hdr->dst_vnic_id = ENIC_MBOX_DST_PF;
	hdr->src_vnic_id = enic->admin_chan_vf_id;
	hdr->msg_type = type;
	hdr->flags = 0;
	hdr->msg_len = len;
	hdr->msg_num = ++enic->admin_chan_msg_num;
}

/*
 * See if there is a new receive packet. If yes, copy it out.
 */
static int enic_admin_rq_peek(struct enic *enic, uint8_t *msg, int *msg_len)
{
	const int desc_size = sizeof(struct cq_enet_rq_desc);
	volatile struct cq_desc *cqd_ptr;
	uint16_t cq_idx, rq_idx, rq_num;
	struct cq_enet_rq_desc *cqrd;
	uint16_t seg_length;
	struct cq_desc cqd;
	struct vnic_rq *rq;
	struct vnic_cq *cq;
	uint8_t tc, color;
	int next_idx;
	void *va;

	rq = &enic->admin_rq;
	cq = &enic->admin_cq[ENIC_ADMIN_RQ_CQ];
	cq_idx = cq->to_clean;
	cqd_ptr = (struct cq_desc *)((uintptr_t)(cq->ring.descs) +
				     (uintptr_t)cq_idx * desc_size);
	color = cq->last_color;
	tc = *(volatile uint8_t *)((uintptr_t)cqd_ptr + desc_size - 1);
	/* No new packet, return */
	if ((tc & CQ_DESC_COLOR_MASK_NOSHIFT) == color)
		return -EAGAIN;
	ENICPMD_LOG(DEBUG, "admin RQ has a completion cq_idx %u color %u", cq_idx, color);

	cqd = *cqd_ptr;
	cqrd = (struct cq_enet_rq_desc *)&cqd;
	seg_length = rte_le_to_cpu_16(cqrd->bytes_written_flags) &
		CQ_ENET_RQ_DESC_BYTES_WRITTEN_MASK;

	rq_num = cqd.q_number & CQ_DESC_Q_NUM_MASK;
	rq_idx = (cqd.completed_index & CQ_DESC_COMP_NDX_MASK);
	ENICPMD_LOG(DEBUG, "rq_num %u rq_idx %u len %u", rq_num, rq_idx, seg_length);

	RTE_VERIFY(rq_num == 0);
	next_idx = rq->admin_next_idx;
	RTE_VERIFY(rq_idx == next_idx);
	rq->admin_next_idx = enic_ring_incr(rq->ring.desc_count, next_idx);

	/* Copy out the received message */
	va = (void *)((char *)rq->admin_msg_rz->addr + ENIC_ADMIN_BUF_SIZE * next_idx);
	*msg_len = seg_length;
	memset(msg, 0, ENIC_ADMIN_BUF_SIZE);
	memcpy(msg, va, seg_length);
	memset(va, 0, ENIC_ADMIN_BUF_SIZE);

	/* Advance CQ */
	cq_idx++;
	if (unlikely(cq_idx == cq->ring.desc_count)) {
		cq_idx = 0;
		cq->last_color ^= CQ_DESC_COLOR_MASK_NOSHIFT;
	}
	cq->to_clean = cq_idx;

	/* Recycle and post RQ buffer */
	rq->posted_index = enic_ring_add(rq->ring.desc_count,
					 rq->posted_index,
					 1);
	rte_wmb();
	iowrite32(rq->posted_index, &rq->ctrl->posted_index);
	rte_wmb();
	return 0;
}

int enic_enable_vf_admin_chan(struct enic *enic)
{
	struct vnic_sriov_stats *stats;
	int err;

	ENICPMD_FUNC_TRACE();
	pthread_mutex_init(&enic->admin_chan_lock, NULL);
	err = vnic_dev_enable_admin_qp(enic->vdev, 1);
	if (err) {
		ENICPMD_LOG(ERR, "failed to enable admin QP type");
		goto out;
	}
	err = vnic_dev_alloc_sriov_stats_mem(enic->vdev);
	if (err) {
		ENICPMD_LOG(ERR, "failed to allocate SR-IOV stats buffer");
		goto out;
	}
	err = vnic_dev_sriov_stats(enic->vdev, &stats);
	if (err) {
		ENICPMD_LOG(ERR, "failed to get SR-IOV stats");
		goto out;
	}
	enic->admin_chan_vf_id = stats->vf_index;
	enic->sriov_vf_soft_rx_stats = !!stats->sriov_host_rx_stats;
	ENICPMD_LOG(INFO, "SR-IOV VF index %u %s stats",
		    stats->vf_index, enic->sriov_vf_soft_rx_stats ? "soft" : "HW");
	err = enic_enable_admin_rq(enic);
	if (err) {
		ENICPMD_LOG(ERR, "failed to enable admin RQ");
		goto out;
	}
	err = enic_enable_admin_wq(enic);
	if (err) {
		ENICPMD_LOG(ERR, "failed to enable admin WQ");
		goto out;
	}
	enic->admin_chan_enabled = true;
	/* Now the admin channel is ready. Send CAPABILITY as the first message */
	err = enic_check_chan_capability(enic);
	if (err) {
		ENICPMD_LOG(ERR, "failed to exchange VF_CAPABILITY message");
		goto out;
	}
	if (enic->sriov_vf_compat_mode) {
		enic_disable_vf_admin_chan(enic, false);
		return 0;
	}
	/* Then register.. */
	err = enic_register_vf(enic);
	if (err) {
		ENICPMD_LOG(ERR, "failed to perform VF_REGISTER");
		goto out;
	}
	/*
	 * If we have to count RX packets (soft stats), do not use
	 * avx2 receive handlers
	 */
	if (enic->sriov_vf_soft_rx_stats)
		enic->enable_avx2_rx = 0;
out:
	return err;
}

int enic_disable_vf_admin_chan(struct enic *enic, bool unregister)
{
	struct vnic_rq *rq;
	struct vnic_wq *wq;
	struct vnic_cq *cq;

	ENICPMD_FUNC_TRACE();
	if (unregister)
		enic_unregister_vf(enic);
	enic->sriov_vf_soft_rx_stats = false;

	rq = &enic->admin_rq;
	vnic_rq_disable(rq);
	rte_memzone_free(rq->admin_msg_rz);
	vnic_rq_free(rq);

	cq = &enic->admin_cq[ENIC_ADMIN_RQ_CQ];
	vnic_cq_free(cq);

	wq = &enic->admin_wq;
	vnic_wq_disable(wq);
	rte_memzone_free(wq->admin_msg_rz);
	rte_memzone_free(wq->cqmsg_rz);
	vnic_wq_free(wq);

	cq = &enic->admin_cq[ENIC_ADMIN_WQ_CQ];
	vnic_cq_free(cq);

	enic->admin_chan_enabled = false;
	return 0;
}

static int common_hdr_check(struct enic *enic, void *msg)
{
	struct enic_mbox_hdr *hdr;

	hdr = (struct enic_mbox_hdr *)msg;
	ENICPMD_LOG(DEBUG, "RX dst %u src %u type %u(%s) flags %u len %u num %" PRIu64,
		    hdr->dst_vnic_id, hdr->src_vnic_id, hdr->msg_type,
		    enic_mbox_msg_type_str(hdr->msg_type),
		    hdr->flags, hdr->msg_len, hdr->msg_num);
	if (hdr->dst_vnic_id != enic->admin_chan_vf_id ||
	    hdr->src_vnic_id != ENIC_MBOX_DST_PF) {
		ENICPMD_LOG(ERR, "unexpected dst/src in reply: dst=%u (expected=%u) src=%u",
			    hdr->dst_vnic_id, enic->admin_chan_vf_id, hdr->src_vnic_id);
		return -EINVAL;
	}
	return 0;
}

static int common_reply_check(__rte_unused struct enic *enic, void *msg,
			      enum enic_mbox_msg_type type)
{
	struct enic_mbox_generic_reply_msg *reply;
	struct enic_mbox_hdr *hdr;

	hdr = (struct enic_mbox_hdr *)msg;
	reply = (struct enic_mbox_generic_reply_msg *)(hdr + 1);
	if (hdr->msg_type != type) {
		ENICPMD_LOG(ERR, "unexpected reply: expected=%u received=%u",
			    type, hdr->msg_type);
		return -EINVAL;
	}
	if (reply->ret_major != 0) {
		ENICPMD_LOG(ERR, "error reply: type=%u(%s) ret_major/minor=%u/%u",
			    type, enic_mbox_msg_type_str(type),
			    reply->ret_major, reply->ret_minor);
		return -EINVAL;
	}
	return 0;
}

static void handle_pf_link_state_notif(struct enic *enic, void *msg)
{
	struct enic_mbox_pf_link_state_notif_msg *notif = msg;
	struct enic_mbox_pf_link_state_ack_msg ack;
	struct rte_eth_link link;

	ENICPMD_FUNC_TRACE();
	ENICPMD_LOG(DEBUG, "PF_LINK_STAT_NOTIF: link_state=%u", notif->link_state);

	/*
	 * Do not use enic_link_update()
	 * Linux PF driver disables link-status notify in FW and uses
	 * this admin message instead. Notify does not work. Remember
	 * the status from PF.
	 */
	memset(&link, 0, sizeof(link));
	link.link_status = notif->link_state ? RTE_ETH_LINK_UP : RTE_ETH_LINK_DOWN;
	link.link_duplex = RTE_ETH_LINK_FULL_DUPLEX;
	link.link_speed = vnic_dev_port_speed(enic->vdev);
	rte_eth_linkstatus_set(enic->rte_dev, &link);
	rte_eth_dev_callback_process(enic->rte_dev, RTE_ETH_EVENT_INTR_LSC, NULL);
	ENICPMD_LOG(DEBUG, "eth_linkstatus: speed=%u duplex=%u autoneg=%u status=%u",
		    link.link_speed, link.link_duplex, link.link_autoneg,
		    link.link_status);

	enic_mbox_init_msg_hdr(enic, &ack, ENIC_MBOX_PF_LINK_STATE_ACK);
	enic_admin_wq_post(enic, &ack);
	ENICPMD_LOG(DEBUG, "sent PF_LINK_STATE_ACK");
}

static void handle_pf_get_stats(struct enic *enic, void *msg)
{
	struct enic_mbox_pf_get_stats_reply_msg reply;
	struct enic_mbox_pf_get_stats_msg *req;
	struct vnic_stats *hw_stats;
	struct vnic_stats *vs;
	unsigned int i;

	ENICPMD_FUNC_TRACE();
	req = msg;
	ENICPMD_LOG(DEBUG, "flags=0x%x", req->flags);
	enic_mbox_init_msg_hdr(enic, &reply, ENIC_MBOX_PF_GET_STATS_REPLY);
	vs = &reply.stats.vnic_stats;
	if (req->flags & ENIC_MBOX_GET_STATS_RX) {
		for (i = 0; i < enic->rq_count; i++) {
			vs->rx.rx_frames_ok += enic->rq[i].soft_stats_pkts;
			vs->rx.rx_bytes_ok += enic->rq[i].soft_stats_bytes;
		}
		vs->rx.rx_frames_total = vs->rx.rx_frames_ok;
		reply.stats.num_rx_stats = 6;
	}
	if (req->flags & ENIC_MBOX_GET_STATS_TX) {
		vnic_dev_stats_dump(enic->vdev, &hw_stats);
		vs->tx = hw_stats->tx;
		reply.stats.num_tx_stats = 11; /* all fields up to rsvd */
	}
	enic_admin_wq_post(enic, &reply);
	ENICPMD_LOG(DEBUG, "sent PF_GET_STATS_REPLY");
}

static void handle_pf_request_msg(struct enic *enic, void *msg)
{
	struct enic_mbox_hdr *hdr = msg;

	switch (hdr->msg_type) {
	case ENIC_MBOX_PF_LINK_STATE_NOTIF:
		handle_pf_link_state_notif(enic, msg);
		break;
	case ENIC_MBOX_PF_GET_STATS_REQUEST:
		handle_pf_get_stats(enic, msg);
		break;
	case ENIC_MBOX_PF_SET_ADMIN_MAC_NOTIF:
		ENICPMD_LOG(WARNING, "Ignore PF_SET_ADMIN_MAC_NOTIF from PF. The PF driver has changed VF MAC address. Reload the driver to use the new address.");
		break;
	default:
		ENICPMD_LOG(WARNING, "received unexpected non-request message from PF: received=%u(%s)",
			    hdr->msg_type, enic_mbox_msg_type_str(hdr->msg_type));
		break;
	}
}

void enic_poll_vf_admin_chan(struct enic *enic)
{
	uint8_t msg[ENIC_ADMIN_BUF_SIZE];
	int len;

	ENICPMD_FUNC_TRACE();
	lock_admin_chan(enic);
	while (!enic_admin_rq_peek(enic, msg, &len)) {
		if (common_hdr_check(enic, msg))
			continue;
		handle_pf_request_msg(enic, msg);
	}
	unlock_admin_chan(enic);
}

/*
 * Poll/receive messages until we see the wanted reply message.
 * That is, we wait for the wanted reply.
 */
#define RECV_REPLY_TIMEOUT 5 /* seconds */
static int recv_reply(struct enic *enic, void *msg, enum enic_mbox_msg_type type)
{
	struct enic_mbox_hdr *hdr;
	uint64_t start, end; /* seconds */
	int err, len;

	start = rte_rdtsc() / rte_get_tsc_hz();
again:
	end = rte_rdtsc() / rte_get_tsc_hz();
	if (end - start > RECV_REPLY_TIMEOUT) {
		ENICPMD_LOG(WARNING, "timed out while waiting for reply %u(%s)",
			    type, enic_mbox_msg_type_str(type));
		return -ETIMEDOUT;
	}
	if (enic_admin_rq_peek(enic, msg, &len))
		goto again;
	err = common_hdr_check(enic, msg);
	if (err)
		goto out;

	/* If not the reply we are looking for, process it and poll again */
	hdr = msg;
	if (hdr->msg_type != type) {
		handle_pf_request_msg(enic, msg);
		goto again;
	}

	err = common_reply_check(enic, msg, type);
	if (err)
		goto out;
out:
	return err;
}

/*
 * Ask the PF driver its level of the admin channel support. If the
 * answer is ver 0 (minimal) or no channel support (timed-out
 * request), work in the backward compat mode.
 *
 * In the compat mode, trust mode does not work, because the PF driver
 * does not support it. For example, VF cannot enable promisc mode,
 * and cannot change MAC address.
 */
static int enic_check_chan_capability(struct enic *enic)
{
	struct enic_mbox_vf_capability_reply_msg *reply;
	struct enic_mbox_vf_capability_msg req;
	uint8_t msg[ENIC_ADMIN_BUF_SIZE];
	int err;

	ENICPMD_FUNC_TRACE();

	enic_mbox_init_msg_hdr(enic, &req.hdr, ENIC_MBOX_VF_CAPABILITY_REQUEST);
	req.version = ENIC_MBOX_CAP_VERSION_1;
	enic_admin_wq_post(enic, &req);
	ENICPMD_LOG(DEBUG, "sent VF_CAPABILITY");

	err = recv_reply(enic, msg, ENIC_MBOX_VF_CAPABILITY_REPLY);
	if (err == -ETIMEDOUT)
		ENICPMD_LOG(WARNING, "PF driver has not responded to CAPABILITY request. Please update the host PF driver");
	else if (err)
		goto out;
	ENICPMD_LOG(DEBUG, "VF_CAPABILITY_REPLY ok");
	reply = (struct enic_mbox_vf_capability_reply_msg *)msg;
	enic->admin_pf_cap_version = reply->version;
	ENICPMD_LOG(DEBUG, "PF admin channel capability version %u",
		    enic->admin_pf_cap_version);
	if (err == -ETIMEDOUT || enic->admin_pf_cap_version == ENIC_MBOX_CAP_VERSION_0) {
		ENICPMD_LOG(WARNING, "PF driver does not have adequate admin channel support. VF works in backward compatible mode");
		err = 0;
		enic->sriov_vf_compat_mode = true;
	} else if (enic->admin_pf_cap_version == ENIC_MBOX_CAP_VERSION_INVALID) {
		ENICPMD_LOG(WARNING, "Unexpected version in CAPABILITY_REPLY from PF driver. cap_version %u",
			    enic->admin_pf_cap_version);
		err = -EINVAL;
	}
out:
	return err;
}

/*
 * The VF driver must 'register' with the PF driver first, before
 * sending any devcmd requests. Once registered, the VF driver must be
 * ready to process messages from the PF driver.
 */
static int enic_register_vf(struct enic *enic)
{
	struct enic_mbox_vf_register_msg req;
	uint8_t msg[ENIC_ADMIN_BUF_SIZE];
	int err;

	ENICPMD_FUNC_TRACE();
	enic_mbox_init_msg_hdr(enic, &req, ENIC_MBOX_VF_REGISTER_REQUEST);
	enic_admin_wq_post(enic, &req);
	ENICPMD_LOG(DEBUG, "sent VF_REGISTER");
	err = recv_reply(enic, msg, ENIC_MBOX_VF_REGISTER_REPLY);
	if (err)
		goto out;
	ENICPMD_LOG(DEBUG, "VF_REGISTER_REPLY ok");
out:
	return err;
}

/*
 * The PF driver expects unregister when the VF driver closes.  But,
 * it is not mandatory. For example, the VF driver may crash without
 * sending the unregister message. In this case, everything still
 * works fine.
 */
static void enic_unregister_vf(struct enic *enic)
{
	struct enic_mbox_vf_unregister_msg req;
	uint8_t msg[ENIC_ADMIN_BUF_SIZE];

	ENICPMD_FUNC_TRACE();
	enic_mbox_init_msg_hdr(enic, &req, ENIC_MBOX_VF_UNREGISTER_REQUEST);
	enic_admin_wq_post(enic, &req);
	ENICPMD_LOG(DEBUG, "sent VF_UNREGISTER");
	if (!recv_reply(enic, msg, ENIC_MBOX_VF_UNREGISTER_REPLY))
		ENICPMD_LOG(DEBUG, "VF_UNREGISTER_REPLY ok");
}

static int vf_set_packet_filter(struct enic *enic, int directed, int multicast,
				int broadcast, int promisc, int allmulti)
{
	struct enic_mbox_vf_set_pkt_filter_flags_msg req;
	uint8_t msg[ENIC_ADMIN_BUF_SIZE];
	uint16_t flags;
	int err;

	ENICPMD_FUNC_TRACE();
	enic_mbox_init_msg_hdr(enic, &req, ENIC_MBOX_VF_SET_PKT_FILTER_FLAGS_REQUEST);
	flags = 0;
	if (directed)
		flags |= ENIC_MBOX_PKT_FILTER_DIRECTED;
	if (multicast)
		flags |= ENIC_MBOX_PKT_FILTER_MULTICAST;
	if (broadcast)
		flags |= ENIC_MBOX_PKT_FILTER_BROADCAST;
	if (promisc)
		flags |= ENIC_MBOX_PKT_FILTER_PROMISC;
	if (allmulti)
		flags |= ENIC_MBOX_PKT_FILTER_ALLMULTI;
	req.flags = flags;
	req.pad = 0;
	/* Lock admin channel while we send and wait for the reply, to prevent
	 * enic_poll_vf_admin_chan() (RQ interrupt) from interfering.
	 */
	lock_admin_chan(enic);
	enic_admin_wq_post(enic, &req);
	ENICPMD_LOG(DEBUG, "sent VF_SET_PKT_FILTER_FLAGS flags=0x%x", flags);
	err = recv_reply(enic, msg, ENIC_MBOX_VF_SET_PKT_FILTER_FLAGS_REPLY);
	unlock_admin_chan(enic);
	if (err) {
		ENICPMD_LOG(DEBUG, "VF_SET_PKT_FILTER_FLAGS_REPLY failed");
		goto out;
	}
	ENICPMD_LOG(DEBUG, "VF_SET_PKT_FILTER_FLAGS_REPLY ok");
out:
	return err;
}

int enic_dev_packet_filter(struct enic *enic, int directed, int multicast,
			   int broadcast, int promisc, int allmulti)
{
	if (enic_is_vf(enic)) {
		RTE_VERIFY(admin_chan_enabled(enic));
		return vf_set_packet_filter(enic, directed, multicast,
					    broadcast, promisc, allmulti);
	}
	return vnic_dev_packet_filter(enic->vdev, directed, multicast,
				     broadcast, promisc, allmulti);
}

static int vf_add_del_addr(struct enic *enic, uint8_t *addr, bool delete)
{
	struct enic_mbox_vf_add_del_mac_msg req;
	uint8_t msg[ENIC_ADMIN_BUF_SIZE];
	int err;

	ENICPMD_FUNC_TRACE();
	enic_mbox_init_msg_hdr(enic, &req, ENIC_MBOX_VF_ADD_DEL_MAC_REQUEST);

	req.num_addrs = 1;
	memcpy(req.mac_addr.addr, addr, RTE_ETHER_ADDR_LEN);
	req.mac_addr.flags = delete ? 0 : MAC_ADDR_FLAG_ADD;

	lock_admin_chan(enic);
	enic_admin_wq_post(enic, &req);
	ENICPMD_LOG(DEBUG, "sent VF_ADD_DEL_MAC");
	err = recv_reply(enic, msg, ENIC_MBOX_VF_ADD_DEL_MAC_REPLY);
	unlock_admin_chan(enic);
	if (err) {
		ENICPMD_LOG(DEBUG, "VF_ADD_DEL_MAC_REPLY failed");
		goto out;
	}
	ENICPMD_LOG(DEBUG, "VF_ADD_DEL_MAC_REPLY ok");
out:
	return err;
}

int enic_dev_add_addr(struct enic *enic, uint8_t *addr)
{
	ENICPMD_FUNC_TRACE();
	if (enic_is_vf(enic)) {
		RTE_VERIFY(admin_chan_enabled(enic));
		return vf_add_del_addr(enic, addr, false);
	}
	return vnic_dev_add_addr(enic->vdev, addr);
}

int enic_dev_del_addr(struct enic *enic, uint8_t *addr)
{
	ENICPMD_FUNC_TRACE();
	if (enic_is_vf(enic)) {
		RTE_VERIFY(admin_chan_enabled(enic));
		return vf_add_del_addr(enic, addr, true);
	}
	return vnic_dev_del_addr(enic->vdev, addr);
}
