/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Microsoft Corporation
 */

#include <errno.h>
#include <net/if.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/uio.h>
#include <time.h>
#include <unistd.h>

#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_dev.h>
#include <rte_errno.h>
#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_mbuf.h>
#include <rte_pcapng.h>
#include <rte_time.h>

#include "pcapng_proto.h"

/* conversion from DPDK speed to PCAPNG */
#define PCAPNG_MBPS_SPEED 1000000ull

/* Format of the capture file handle */
struct rte_pcapng {
	int  outfd;		/* output file */
	/* DPDK port id to interface index in file */
	uint32_t port_index[RTE_MAX_ETHPORTS];
};

/* For converting TSC cycles to PCAPNG ns format */
struct pcapng_time {
	uint64_t ns;
	uint64_t cycles;
} pcapng_time;

RTE_INIT(pcapng_init)
{
	struct timespec ts;

	pcapng_time.cycles = rte_get_tsc_cycles();
	clock_gettime(CLOCK_REALTIME, &ts);
	pcapng_time.ns = rte_timespec_to_ns(&ts);
}

/* PCAPNG timestamps are in nanoseconds */
static uint64_t pcapng_tsc_to_ns(uint64_t cycles)
{
	uint64_t delta;

	delta = cycles - pcapng_time.cycles;
	return pcapng_time.ns + (delta * NSEC_PER_SEC) / rte_get_tsc_hz();
}

/* length of option including padding */
static uint16_t pcapng_optlen(uint16_t len)
{
	return RTE_ALIGN(sizeof(struct pcapng_option) + len,
			 sizeof(uint32_t));
}

/* build TLV option and return location of next */
static struct pcapng_option *
pcapng_add_option(struct pcapng_option *popt, uint16_t code,
		  const void *data, uint16_t len)
{
	popt->code = code;
	popt->length = len;
	memcpy(popt->data, data, len);

	return (struct pcapng_option *)((uint8_t *)popt + pcapng_optlen(len));
}

/*
 * Write required initial section header describing the capture
 */
static int
pcapng_section_block(rte_pcapng_t *self,
		    const char *os, const char *hw,
		    const char *app, const char *comment)
{
	struct pcapng_section_header *hdr;
	struct pcapng_option *opt;
	void *buf;
	uint32_t len;
	ssize_t cc;

	len = sizeof(*hdr);
	if (hw)
		len += pcapng_optlen(strlen(hw));
	if (os)
		len += pcapng_optlen(strlen(os));
	if (app)
		len += pcapng_optlen(strlen(app));
	if (comment)
		len += pcapng_optlen(strlen(comment));

	/* reserve space for OPT_END */
	len += pcapng_optlen(0);
	len += sizeof(uint32_t);

	buf = calloc(1, len);
	if (!buf)
		return -1;

	hdr = (struct pcapng_section_header *)buf;
	*hdr = (struct pcapng_section_header) {
		.block_type = PCAPNG_SECTION_BLOCK,
		.block_length = len,
		.byte_order_magic = PCAPNG_BYTE_ORDER_MAGIC,
		.major_version = PCAPNG_MAJOR_VERS,
		.minor_version = PCAPNG_MINOR_VERS,
		.section_length = UINT64_MAX,
	};

	/* After the section header insert variable length options. */
	opt = (struct pcapng_option *)(hdr + 1);
	if (comment)
		opt = pcapng_add_option(opt, PCAPNG_OPT_COMMENT,
					comment, strlen(comment));
	if (hw)
		opt = pcapng_add_option(opt, PCAPNG_SHB_HARDWARE,
					hw, strlen(hw));
	if (os)
		opt = pcapng_add_option(opt, PCAPNG_SHB_OS,
					os, strlen(os));
	if (app)
		opt = pcapng_add_option(opt, PCAPNG_SHB_USERAPPL,
					app, strlen(app));

	/* The standard requires last option to be OPT_END */
	opt = pcapng_add_option(opt, PCAPNG_OPT_END, NULL, 0);

	/* clone block_length after option */
	memcpy(opt, &hdr->block_length, sizeof(uint32_t));

	cc = write(self->outfd, buf, len);
	free(buf);

	return cc;
}

/* Write an interface block for a DPDK port */
static int
pcapng_add_interface(rte_pcapng_t *self, uint16_t port)
{
	struct pcapng_interface_block *hdr;
	struct rte_eth_dev_info dev_info;
	struct rte_ether_addr *ea, macaddr;
	const struct rte_device *dev;
	struct rte_eth_link link;
	struct pcapng_option *opt;
	const uint8_t tsresol = 9;	/* nanosecond resolution */
	uint32_t len;
	void *buf;
	char ifname[IF_NAMESIZE];
	char ifhw[256];
	uint64_t speed = 0;

	if (rte_eth_dev_info_get(port, &dev_info) < 0)
		return -1;

	/* make something like an interface name */
	if (if_indextoname(dev_info.if_index, ifname) == NULL)
		snprintf(ifname, IF_NAMESIZE, "dpdk:%u", port);

	/* make a useful device hardware string */
	dev = dev_info.device;
	if (dev)
		snprintf(ifhw, sizeof(ifhw),
			 "%s-%s", dev->bus->name, dev->name);

	/* DPDK reports in units of Mbps */
	if (rte_eth_link_get(port, &link) == 0 &&
	    link.link_status == RTE_ETH_LINK_UP)
		speed = link.link_speed * PCAPNG_MBPS_SPEED;

	if (rte_eth_macaddr_get(port, &macaddr) < 0)
		ea = NULL;
	else
		ea = &macaddr;

	/* Compute length of interface block options */
	len = sizeof(*hdr);

	len += pcapng_optlen(sizeof(tsresol));	/* timestamp */
	len += pcapng_optlen(strlen(ifname));	/* ifname */

	if (ea)
		len += pcapng_optlen(RTE_ETHER_ADDR_LEN); /* macaddr */
	if (speed != 0)
		len += pcapng_optlen(sizeof(uint64_t));
	if (dev)
		len += pcapng_optlen(strlen(ifhw));

	len += pcapng_optlen(0);
	len += sizeof(uint32_t);

	buf = alloca(len);
	if (!buf)
		return -1;

	hdr = (struct pcapng_interface_block *)buf;
	*hdr = (struct pcapng_interface_block) {
		.block_type = PCAPNG_INTERFACE_BLOCK,
		.link_type = 1,		/* DLT_EN10MB - Ethernet */
		.block_length = len,
	};

	opt = (struct pcapng_option *)(hdr + 1);
	opt = pcapng_add_option(opt, PCAPNG_IFB_TSRESOL,
				&tsresol, sizeof(tsresol));
	opt = pcapng_add_option(opt, PCAPNG_IFB_NAME,
				ifname, strlen(ifname));
	if (ea)
		opt = pcapng_add_option(opt, PCAPNG_IFB_MACADDR,
					ea, RTE_ETHER_ADDR_LEN);
	if (speed != 0)
		opt = pcapng_add_option(opt, PCAPNG_IFB_SPEED,
					 &speed, sizeof(uint64_t));
	if (dev)
		opt = pcapng_add_option(opt, PCAPNG_IFB_HARDWARE,
					 ifhw, strlen(ifhw));
	opt = pcapng_add_option(opt, PCAPNG_OPT_END, NULL, 0);

	/* clone block_length after optionsa */
	memcpy(opt, &hdr->block_length, sizeof(uint32_t));

	return write(self->outfd, buf, len);
}

/*
 * Write the list of possible interfaces at the start
 * of the file.
 */
static int
pcapng_interfaces(rte_pcapng_t *self)
{
	uint16_t port_id;
	uint16_t index = 0;

	RTE_ETH_FOREACH_DEV(port_id) {
		/* The list if ports in pcapng needs to be contiguous */
		self->port_index[port_id] = index++;
		if (pcapng_add_interface(self, port_id) < 0)
			return -1;
	}
	return 0;
}

/*
 * Write an Interface statistics block at the end of capture.
 */
ssize_t
rte_pcapng_write_stats(rte_pcapng_t *self, uint16_t port_id,
		       const char *comment,
		       uint64_t start_time, uint64_t end_time,
		       uint64_t ifrecv, uint64_t ifdrop)
{
	struct pcapng_statistics *hdr;
	struct pcapng_option *opt;
	uint32_t optlen, len;
	uint8_t *buf;
	uint64_t ns;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -EINVAL);

	optlen = 0;

	if (ifrecv != UINT64_MAX)
		optlen += pcapng_optlen(sizeof(ifrecv));
	if (ifdrop != UINT64_MAX)
		optlen += pcapng_optlen(sizeof(ifdrop));
	if (start_time != 0)
		optlen += pcapng_optlen(sizeof(start_time));
	if (end_time != 0)
		optlen += pcapng_optlen(sizeof(end_time));
	if (comment)
		optlen += pcapng_optlen(strlen(comment));
	if (optlen != 0)
		optlen += pcapng_optlen(0);

	len = sizeof(*hdr) + optlen + sizeof(uint32_t);
	buf = alloca(len);
	if (buf == NULL)
		return -1;

	hdr = (struct pcapng_statistics *)buf;
	opt = (struct pcapng_option *)(hdr + 1);

	if (comment)
		opt = pcapng_add_option(opt, PCAPNG_OPT_COMMENT,
					comment, strlen(comment));
	if (start_time != 0)
		opt = pcapng_add_option(opt, PCAPNG_ISB_STARTTIME,
					 &start_time, sizeof(start_time));
	if (end_time != 0)
		opt = pcapng_add_option(opt, PCAPNG_ISB_ENDTIME,
					 &end_time, sizeof(end_time));
	if (ifrecv != UINT64_MAX)
		opt = pcapng_add_option(opt, PCAPNG_ISB_IFRECV,
				&ifrecv, sizeof(ifrecv));
	if (ifdrop != UINT64_MAX)
		opt = pcapng_add_option(opt, PCAPNG_ISB_IFDROP,
				&ifdrop, sizeof(ifdrop));
	if (optlen != 0)
		opt = pcapng_add_option(opt, PCAPNG_OPT_END, NULL, 0);

	hdr->block_type = PCAPNG_INTERFACE_STATS_BLOCK;
	hdr->block_length = len;
	hdr->interface_id = self->port_index[port_id];

	ns = pcapng_tsc_to_ns(rte_get_tsc_cycles());
	hdr->timestamp_hi = ns >> 32;
	hdr->timestamp_lo = (uint32_t)ns;

	/* clone block_length after option */
	memcpy(opt, &len, sizeof(uint32_t));

	return write(self->outfd, buf, len);
}

uint32_t
rte_pcapng_mbuf_size(uint32_t length)
{
	/* The VLAN and EPB header must fit in the mbuf headroom. */
	RTE_ASSERT(sizeof(struct pcapng_enhance_packet_block) +
		   sizeof(struct rte_vlan_hdr) <= RTE_PKTMBUF_HEADROOM);

	/* The flags and queue information are added at the end. */
	return sizeof(struct rte_mbuf)
		+ RTE_ALIGN(length, sizeof(uint32_t))
		+ pcapng_optlen(sizeof(uint32_t)) /* flag option */
		+ pcapng_optlen(sizeof(uint32_t)) /* queue option */
		+ sizeof(uint32_t);		  /*  length */
}

/* More generalized version rte_vlan_insert() */
static int
pcapng_vlan_insert(struct rte_mbuf *m, uint16_t ether_type, uint16_t tci)
{
	struct rte_ether_hdr *nh, *oh;
	struct rte_vlan_hdr *vh;

	if (!RTE_MBUF_DIRECT(m) || rte_mbuf_refcnt_read(m) > 1)
		return -EINVAL;

	if (rte_pktmbuf_data_len(m) < sizeof(*oh))
		return -EINVAL;

	oh = rte_pktmbuf_mtod(m, struct rte_ether_hdr *);
	nh = (struct rte_ether_hdr *)
		rte_pktmbuf_prepend(m, sizeof(struct rte_vlan_hdr));
	if (nh == NULL)
		return -ENOSPC;

	memmove(nh, oh, 2 * RTE_ETHER_ADDR_LEN);
	nh->ether_type = rte_cpu_to_be_16(ether_type);

	vh = (struct rte_vlan_hdr *) (nh + 1);
	vh->vlan_tci = rte_cpu_to_be_16(tci);

	return 0;
}

/*
 *   The mbufs created use the Pcapng standard enhanced packet  block.
 *
 *                         1                   2                   3
 *     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  0 |                    Block Type = 0x00000006                    |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  4 |                      Block Total Length                       |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  8 |                         Interface ID                          |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 12 |                        Timestamp (High)                       |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 16 |                        Timestamp (Low)                        |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 20 |                    Captured Packet Length                     |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 24 |                    Original Packet Length                     |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 28 /                                                               /
 *    /                          Packet Data                          /
 *    /              variable length, padded to 32 bits               /
 *    /                                                               /
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *    |      Option Code = 0x0002     |     Option Length = 0x004     |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *    |              Flags (direction)                                |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *    |      Option Code = 0x0006     |     Option Length = 0x002     |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *    |              Queue id                                         |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *    |                      Block Total Length                       |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */

/* Make a copy of original mbuf with pcapng header and options */
struct rte_mbuf *
rte_pcapng_copy(uint16_t port_id, uint32_t queue,
		const struct rte_mbuf *md,
		struct rte_mempool *mp,
		uint32_t length, uint64_t cycles,
		enum rte_pcapng_direction direction)
{
	struct pcapng_enhance_packet_block *epb;
	uint32_t orig_len, data_len, padding, flags;
	struct pcapng_option *opt;
	const uint16_t optlen = pcapng_optlen(sizeof(flags)) + pcapng_optlen(sizeof(queue));
	struct rte_mbuf *mc;
	uint64_t ns;

#ifdef RTE_LIBRTE_ETHDEV_DEBUG
	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, NULL);
#endif
	ns = pcapng_tsc_to_ns(cycles);

	orig_len = rte_pktmbuf_pkt_len(md);

	/* Take snapshot of the data */
	mc = rte_pktmbuf_copy(md, mp, 0, length);
	if (unlikely(mc == NULL))
		return NULL;

	/* Expand any offloaded VLAN information */
	if ((direction == RTE_PCAPNG_DIRECTION_IN &&
	     (md->ol_flags & RTE_MBUF_F_RX_VLAN_STRIPPED)) ||
	    (direction == RTE_PCAPNG_DIRECTION_OUT &&
	     (md->ol_flags & RTE_MBUF_F_TX_VLAN))) {
		if (pcapng_vlan_insert(mc, RTE_ETHER_TYPE_VLAN,
				       md->vlan_tci) != 0)
			goto fail;
	}

	if ((direction == RTE_PCAPNG_DIRECTION_IN &&
	     (md->ol_flags & RTE_MBUF_F_RX_QINQ_STRIPPED)) ||
	    (direction == RTE_PCAPNG_DIRECTION_OUT &&
	     (md->ol_flags & RTE_MBUF_F_TX_QINQ))) {
		if (pcapng_vlan_insert(mc, RTE_ETHER_TYPE_QINQ,
				       md->vlan_tci_outer) != 0)
			goto fail;
	}

	/* pad the packet to 32 bit boundary */
	data_len = rte_pktmbuf_data_len(mc);
	padding = RTE_ALIGN(data_len, sizeof(uint32_t)) - data_len;
	if (padding > 0) {
		void *tail = rte_pktmbuf_append(mc, padding);

		if (tail == NULL)
			goto fail;
		memset(tail, 0, padding);
	}

	/* reserve trailing options and block length */
	opt = (struct pcapng_option *)
		rte_pktmbuf_append(mc, optlen + sizeof(uint32_t));
	if (unlikely(opt == NULL))
		goto fail;

	switch (direction) {
	case RTE_PCAPNG_DIRECTION_IN:
		flags = PCAPNG_IFB_INBOUND;
		break;
	case RTE_PCAPNG_DIRECTION_OUT:
		flags = PCAPNG_IFB_OUTBOUND;
		break;
	default:
		flags = 0;
	}

	opt = pcapng_add_option(opt, PCAPNG_EPB_FLAGS,
				&flags, sizeof(flags));

	opt = pcapng_add_option(opt, PCAPNG_EPB_QUEUE,
				&queue, sizeof(queue));

	/* Note: END_OPT necessary here. Wireshark doesn't do it. */

	/* Add PCAPNG packet header */
	epb = (struct pcapng_enhance_packet_block *)
		rte_pktmbuf_prepend(mc, sizeof(*epb));
	if (unlikely(epb == NULL))
		goto fail;

	epb->block_type = PCAPNG_ENHANCED_PACKET_BLOCK;
	epb->block_length = rte_pktmbuf_data_len(mc);

	/* Interface index is filled in later during write */
	mc->port = port_id;

	epb->timestamp_hi = ns >> 32;
	epb->timestamp_lo = (uint32_t)ns;
	epb->capture_length = data_len;
	epb->original_length = orig_len;

	/* set trailer of block length */
	*(uint32_t *)opt = epb->block_length;

	return mc;

fail:
	rte_pktmbuf_free(mc);
	return NULL;
}

/* Count how many segments are in this array of mbufs */
static unsigned int
mbuf_burst_segs(struct rte_mbuf *pkts[], unsigned int n)
{
	unsigned int i, iovcnt;

	for (iovcnt = 0, i = 0; i < n; i++) {
		const struct rte_mbuf *m = pkts[i];

		__rte_mbuf_sanity_check(m, 1);

		iovcnt += m->nb_segs;
	}
	return iovcnt;
}

/* Write pre-formatted packets to file. */
ssize_t
rte_pcapng_write_packets(rte_pcapng_t *self,
			 struct rte_mbuf *pkts[], uint16_t nb_pkts)
{
	int iovcnt = mbuf_burst_segs(pkts, nb_pkts);
	struct iovec iov[iovcnt];
	unsigned int i, cnt;
	ssize_t ret;

	for (i = cnt = 0; i < nb_pkts; i++) {
		struct rte_mbuf *m = pkts[i];
		struct pcapng_enhance_packet_block *epb;

		/* sanity check that is really a pcapng mbuf */
		epb = rte_pktmbuf_mtod(m, struct pcapng_enhance_packet_block *);
		if (unlikely(epb->block_type != PCAPNG_ENHANCED_PACKET_BLOCK ||
			     epb->block_length != rte_pktmbuf_data_len(m))) {
			rte_errno = EINVAL;
			return -1;
		}

		/*
		 * The DPDK port is recorded during pcapng_copy.
		 * Map that to PCAPNG interface in file.
		 */
		epb->interface_id = self->port_index[m->port];
		do {
			iov[cnt].iov_base = rte_pktmbuf_mtod(m, void *);
			iov[cnt].iov_len = rte_pktmbuf_data_len(m);
			++cnt;
		} while ((m = m->next));
	}

	ret = writev(self->outfd, iov, iovcnt);
	if (unlikely(ret < 0))
		rte_errno = errno;
	return ret;
}

/* Create new pcapng writer handle */
rte_pcapng_t *
rte_pcapng_fdopen(int fd,
		  const char *osname, const char *hardware,
		  const char *appname, const char *comment)
{
	rte_pcapng_t *self;

	self = malloc(sizeof(*self));
	if (!self) {
		rte_errno = ENOMEM;
		return NULL;
	}

	self->outfd = fd;

	if (pcapng_section_block(self, osname, hardware, appname, comment) < 0)
		goto fail;

	if (pcapng_interfaces(self) < 0)
		goto fail;

	return self;
fail:
	free(self);
	return NULL;
}

void
rte_pcapng_close(rte_pcapng_t *self)
{
	close(self->outfd);
	free(self);
}
