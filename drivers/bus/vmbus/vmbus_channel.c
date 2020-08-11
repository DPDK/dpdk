/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2018, Microsoft Corporation.
 * All Rights Reserved.
 */

#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <sys/uio.h>

#include <rte_eal.h>
#include <rte_tailq.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_bus.h>
#include <rte_atomic.h>
#include <rte_memory.h>
#include <rte_bus_vmbus.h>

#include "private.h"

static inline void
vmbus_sync_set_bit(volatile uint32_t *addr, uint32_t mask)
{
	/* Use GCC builtin which atomic does atomic OR operation */
	__sync_or_and_fetch(addr, mask);
}

static inline void
vmbus_set_monitor(const struct rte_vmbus_device *dev, uint32_t monitor_id)
{
	uint32_t *monitor_addr, monitor_mask;
	unsigned int trigger_index;

	trigger_index = monitor_id / HV_MON_TRIG_LEN;
	monitor_mask = 1u << (monitor_id % HV_MON_TRIG_LEN);

	monitor_addr = &dev->monitor_page->trigs[trigger_index].pending;
	vmbus_sync_set_bit(monitor_addr, monitor_mask);
}

static void
vmbus_set_event(const struct rte_vmbus_device *dev,
		const struct vmbus_channel *chan)
{
	vmbus_set_monitor(dev, chan->monitor_id);
}

/*
 * Set the wait between when hypervisor examines the trigger.
 */
void
rte_vmbus_set_latency(const struct rte_vmbus_device *dev,
		      const struct vmbus_channel *chan,
		      uint32_t latency)
{
	uint32_t trig_idx = chan->monitor_id / VMBUS_MONTRIG_LEN;
	uint32_t trig_offs = chan->monitor_id % VMBUS_MONTRIG_LEN;

	if (latency >= UINT16_MAX * 100) {
		VMBUS_LOG(ERR, "invalid latency value %u", latency);
		return;
	}

	if (trig_idx >= VMBUS_MONTRIGS_MAX) {
		VMBUS_LOG(ERR, "invalid monitor trigger %u",
			  trig_idx);
		return;
	}

	/* Host value is expressed in 100 nanosecond units */
	dev->monitor_page->lat[trig_idx][trig_offs] = latency / 100;
}

/*
 * Notify host that there are data pending on our TX bufring.
 *
 * Since this in userspace, rely on the monitor page.
 * Can't do a hypercall from userspace.
 */
void
rte_vmbus_chan_signal_tx(const struct vmbus_channel *chan)
{
	const struct rte_vmbus_device *dev = chan->device;
	const struct vmbus_br *tbr = &chan->txbr;

	/* Make sure all updates are done before signaling host */
	rte_smp_wmb();

	/* If host is ignoring interrupts? */
	if (tbr->vbr->imask)
		return;

	vmbus_set_event(dev, chan);
}


/* Do a simple send directly using transmit ring. */
int rte_vmbus_chan_send(struct vmbus_channel *chan, uint16_t type,
			void *data, uint32_t dlen,
			uint64_t xactid, uint32_t flags, bool *need_sig)
{
	struct vmbus_chanpkt pkt;
	unsigned int pktlen, pad_pktlen;
	const uint32_t hlen = sizeof(pkt);
	bool send_evt = false;
	uint64_t pad = 0;
	struct iovec iov[3];
	int error;

	pktlen = hlen + dlen;
	pad_pktlen = RTE_ALIGN(pktlen, sizeof(uint64_t));

	pkt.hdr.type = type;
	pkt.hdr.flags = flags;
	pkt.hdr.hlen = hlen >> VMBUS_CHANPKT_SIZE_SHIFT;
	pkt.hdr.tlen = pad_pktlen >> VMBUS_CHANPKT_SIZE_SHIFT;
	pkt.hdr.xactid = xactid;

	iov[0].iov_base = &pkt;
	iov[0].iov_len = hlen;
	iov[1].iov_base = data;
	iov[1].iov_len = dlen;
	iov[2].iov_base = &pad;
	iov[2].iov_len = pad_pktlen - pktlen;

	error = vmbus_txbr_write(&chan->txbr, iov, 3, &send_evt);

	/*
	 * caller sets need_sig to non-NULL if it will handle
	 * signaling if required later.
	 * if need_sig is NULL, signal now if needed.
	 */
	if (need_sig)
		*need_sig |= send_evt;
	else if (error == 0 && send_evt)
		rte_vmbus_chan_signal_tx(chan);
	return error;
}

/* Do a scatter/gather send where the descriptor points to data. */
int rte_vmbus_chan_send_sglist(struct vmbus_channel *chan,
			       struct vmbus_gpa sg[], uint32_t sglen,
			       void *data, uint32_t dlen,
			       uint64_t xactid, bool *need_sig)
{
	struct vmbus_chanpkt_sglist pkt;
	unsigned int pktlen, pad_pktlen, hlen;
	bool send_evt = false;
	struct iovec iov[4];
	uint64_t pad = 0;
	int error;

	hlen = offsetof(struct vmbus_chanpkt_sglist, gpa[sglen]);
	pktlen = hlen + dlen;
	pad_pktlen = RTE_ALIGN(pktlen, sizeof(uint64_t));

	pkt.hdr.type = VMBUS_CHANPKT_TYPE_GPA;
	pkt.hdr.flags = VMBUS_CHANPKT_FLAG_RC;
	pkt.hdr.hlen = hlen >> VMBUS_CHANPKT_SIZE_SHIFT;
	pkt.hdr.tlen = pad_pktlen >> VMBUS_CHANPKT_SIZE_SHIFT;
	pkt.hdr.xactid = xactid;
	pkt.rsvd = 0;
	pkt.gpa_cnt = sglen;

	iov[0].iov_base = &pkt;
	iov[0].iov_len = sizeof(pkt);
	iov[1].iov_base = sg;
	iov[1].iov_len = sizeof(struct vmbus_gpa) * sglen;
	iov[2].iov_base = data;
	iov[2].iov_len = dlen;
	iov[3].iov_base = &pad;
	iov[3].iov_len = pad_pktlen - pktlen;

	error = vmbus_txbr_write(&chan->txbr, iov, 4, &send_evt);

	/* if caller is batching, just propagate the status */
	if (need_sig)
		*need_sig |= send_evt;
	else if (error == 0 && send_evt)
		rte_vmbus_chan_signal_tx(chan);
	return error;
}

bool rte_vmbus_chan_rx_empty(const struct vmbus_channel *channel)
{
	const struct vmbus_br *br = &channel->rxbr;

	rte_smp_rmb();
	return br->vbr->rindex == br->vbr->windex;
}

/* Signal host after reading N bytes */
void rte_vmbus_chan_signal_read(struct vmbus_channel *chan, uint32_t bytes_read)
{
	struct vmbus_br *rbr = &chan->rxbr;
	uint32_t write_sz, pending_sz;

	/* No need for signaling on older versions */
	if (!rbr->vbr->feature_bits.feat_pending_send_sz)
		return;

	/* Make sure reading of pending happens after new read index */
	rte_smp_mb();

	pending_sz = rbr->vbr->pending_send;
	if (!pending_sz)
		return;

	rte_smp_rmb();
	write_sz = vmbus_br_availwrite(rbr, rbr->vbr->windex);

	/* If there was space before then host was not blocked */
	if (write_sz - bytes_read > pending_sz)
		return;

	/* If pending write will not fit */
	if (write_sz <= pending_sz)
		return;

	vmbus_set_event(chan->device, chan);
}

int rte_vmbus_chan_recv(struct vmbus_channel *chan, void *data, uint32_t *len,
			uint64_t *request_id)
{
	struct vmbus_chanpkt_hdr pkt;
	uint32_t dlen, hlen, bufferlen = *len;
	int error;

	*len = 0;

	error = vmbus_rxbr_peek(&chan->rxbr, &pkt, sizeof(pkt));
	if (error)
		return error;

	if (unlikely(pkt.hlen < VMBUS_CHANPKT_HLEN_MIN)) {
		VMBUS_LOG(ERR, "VMBUS recv, invalid hlen %u", pkt.hlen);
		/* XXX this channel is dead actually. */
		return -EIO;
	}

	if (unlikely(pkt.hlen > pkt.tlen)) {
		VMBUS_LOG(ERR, "VMBUS recv,invalid hlen %u and tlen %u",
			  pkt.hlen, pkt.tlen);
		return -EIO;
	}

	/* Length are in quad words */
	hlen = pkt.hlen << VMBUS_CHANPKT_SIZE_SHIFT;
	dlen = (pkt.tlen << VMBUS_CHANPKT_SIZE_SHIFT) - hlen;
	*len = dlen;

	/* If caller buffer is not large enough */
	if (unlikely(dlen > bufferlen))
		return -ENOBUFS;

	if (request_id)
		*request_id = pkt.xactid;

	/* Read data and skip packet header */
	error = vmbus_rxbr_read(&chan->rxbr, data, dlen, hlen);
	if (error)
		return error;

	rte_vmbus_chan_signal_read(chan, dlen + hlen + sizeof(uint64_t));
	return 0;
}

/* TODO: replace this with inplace ring buffer (no copy) */
int rte_vmbus_chan_recv_raw(struct vmbus_channel *chan,
			    void *data, uint32_t *len)
{
	struct vmbus_chanpkt_hdr pkt;
	uint32_t dlen, bufferlen = *len;
	int error;

	error = vmbus_rxbr_peek(&chan->rxbr, &pkt, sizeof(pkt));
	if (error)
		return error;

	if (unlikely(pkt.hlen < VMBUS_CHANPKT_HLEN_MIN)) {
		VMBUS_LOG(ERR, "VMBUS recv, invalid hlen %u", pkt.hlen);
		/* XXX this channel is dead actually. */
		return -EIO;
	}

	if (unlikely(pkt.hlen > pkt.tlen)) {
		VMBUS_LOG(ERR, "VMBUS recv,invalid hlen %u and tlen %u",
			pkt.hlen, pkt.tlen);
		return -EIO;
	}

	/* Length are in quad words */
	dlen = pkt.tlen << VMBUS_CHANPKT_SIZE_SHIFT;
	*len = dlen;

	/* If caller buffer is not large enough */
	if (unlikely(dlen > bufferlen))
		return -ENOBUFS;

	/* Read data and skip packet header */
	error = vmbus_rxbr_read(&chan->rxbr, data, dlen, 0);
	if (error)
		return error;

	/* Return the number of bytes read */
	return dlen + sizeof(uint64_t);
}

int vmbus_chan_create(const struct rte_vmbus_device *device,
		      uint16_t relid, uint16_t subid, uint8_t monitor_id,
		      struct vmbus_channel **new_chan)
{
	struct vmbus_channel *chan;
	int err;

	chan = rte_zmalloc_socket("VMBUS", sizeof(*chan), RTE_CACHE_LINE_SIZE,
				  device->device.numa_node);
	if (!chan)
		return -ENOMEM;

	STAILQ_INIT(&chan->subchannel_list);
	chan->device = device;
	chan->subchannel_id = subid;
	chan->relid = relid;
	chan->monitor_id = monitor_id;
	*new_chan = chan;

	err = vmbus_uio_map_rings(chan);
	if (err) {
		rte_free(chan);
		return err;
	}

	return 0;
}

/* Setup the primary channel */
int rte_vmbus_chan_open(struct rte_vmbus_device *device,
			struct vmbus_channel **new_chan)
{
	struct mapped_vmbus_resource *uio_res;
	int err;

	uio_res = vmbus_uio_find_resource(device);
	if (!uio_res) {
		VMBUS_LOG(ERR, "can't find uio resource");
		return -EINVAL;
	}

	err = vmbus_chan_create(device, device->relid, 0,
				device->monitor_id, new_chan);
	if (!err) {
		device->primary = *new_chan;
		uio_res->primary = *new_chan;
	}

	return err;
}

int rte_vmbus_max_channels(const struct rte_vmbus_device *device)
{
	if (vmbus_uio_subchannels_supported(device, device->primary))
		return VMBUS_MAX_CHANNELS;
	else
		return 1;
}

/* Setup secondary channel */
int rte_vmbus_subchan_open(struct vmbus_channel *primary,
			   struct vmbus_channel **new_chan)
{
	struct vmbus_channel *chan;
	int err;

	err = vmbus_uio_get_subchan(primary, &chan);
	if (err)
		return err;

	STAILQ_INSERT_TAIL(&primary->subchannel_list, chan, next);
	*new_chan = chan;
	return 0;
}

uint16_t rte_vmbus_sub_channel_index(const struct vmbus_channel *chan)
{
	return chan->subchannel_id;
}

void rte_vmbus_chan_close(struct vmbus_channel *chan)
{
	const struct rte_vmbus_device *device = chan->device;
	struct vmbus_channel *primary = device->primary;

	/*
	 * intentionally leak primary channel because
	 * secondary may still reference it
	 */
	if (chan != primary) {
		STAILQ_REMOVE(&primary->subchannel_list, chan,
			      vmbus_channel, next);
		rte_free(chan);
	}

}

static void vmbus_dump_ring(FILE *f, const char *id, const struct vmbus_br *br)
{
	const struct vmbus_bufring *vbr = br->vbr;
	struct vmbus_chanpkt_hdr pkt;

	fprintf(f, "%s windex=%u rindex=%u mask=%u pending=%u feature=%#x\n",
		id, vbr->windex, vbr->rindex, vbr->imask,
		vbr->pending_send, vbr->feature_bits.value);
	fprintf(f, " size=%u avail write=%u read=%u\n",
		br->dsize, vmbus_br_availwrite(br, vbr->windex),
		vmbus_br_availread(br));

	if (vmbus_rxbr_peek(br, &pkt, sizeof(pkt)) == 0)
		fprintf(f, "  pkt type %#x len %u flags %#x xactid %#"PRIx64"\n",
			pkt.type,
			pkt.tlen << VMBUS_CHANPKT_SIZE_SHIFT,
			pkt.flags, pkt.xactid);
}

void rte_vmbus_chan_dump(FILE *f, const struct vmbus_channel *chan)
{
	fprintf(f, "channel[%u] relid=%u monitor=%u\n",
		chan->subchannel_id, chan->relid, chan->monitor_id);
	vmbus_dump_ring(f, "rxbr", &chan->rxbr);
	vmbus_dump_ring(f, "txbr", &chan->txbr);
}
