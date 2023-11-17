/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Microsoft Corporation
 */

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifndef RTE_EXEC_ENV_WINDOWS
#include <net/if.h>
#include <sys/uio.h>
#endif

#include <bus_driver.h>
#include <rte_common.h>
#include <rte_cycles.h>
#include <dev_driver.h>
#include <rte_errno.h>
#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_mbuf.h>
#include <rte_os_shim.h>
#include <rte_pcapng.h>
#include <rte_reciprocal.h>
#include <rte_time.h>

#include "pcapng_proto.h"

/* conversion from DPDK speed to PCAPNG */
#define PCAPNG_MBPS_SPEED 1000000ull

/* upper bound for section, stats and interface blocks */
#define PCAPNG_BLKSIZ	2048

/* Format of the capture file handle */
struct rte_pcapng {
	int  outfd;		/* output file */
	unsigned int ports;	/* number of interfaces added */
	uint64_t offset_ns;	/* ns since 1/1/1970 when initialized */
	uint64_t tsc_base;	/* TSC when started */

	/* DPDK port id to interface index in file */
	uint32_t port_index[RTE_MAX_ETHPORTS];
};

#ifdef RTE_EXEC_ENV_WINDOWS
/*
 * Windows does not have writev() call.
 * Emulate this by copying to a new buffer.
 * The copy is necessary since pcapng needs to be thread-safe
 * and do atomic write operations.
 */

#define IOV_MAX 128
struct iovec {
	void   *iov_base;
	size_t  iov_len;
};

static ssize_t writev(int fd, const struct iovec *iov, int iovcnt)
{
	size_t bytes = 0;
	uint8_t *ptr;
	void *tmp_buf;
	ssize_t ret;
	int i;

	for (i = 0; i < iovcnt; i++)
		bytes += iov[i].iov_len;

	if (unlikely(bytes == 0))
		return 0;

	tmp_buf = malloc(bytes);
	if (unlikely(tmp_buf == NULL)) {
		errno = ENOMEM;
		return -1;
	}

	ptr = tmp_buf;
	for (i = 0; i < iovcnt; i++) {
		rte_memcpy(ptr, iov[i].iov_base, iov[i].iov_len);
		ptr += iov[i].iov_len;
	}

	ret = write(fd, tmp_buf, bytes);
	free(tmp_buf);
	return ret;
}

#define IF_NAMESIZE	16
/* compatibility wrapper because name is optional */
#define if_indextoname(ifindex, ifname) NULL
#endif

/* Convert from TSC (CPU cycles) to nanoseconds */
static uint64_t
pcapng_timestamp(const rte_pcapng_t *self, uint64_t cycles)
{
	uint64_t delta, rem, secs, ns;
	const uint64_t hz = rte_get_tsc_hz();

	delta = cycles - self->tsc_base;

	/* Avoid numeric wraparound by computing seconds first */
	secs = delta / hz;
	rem = delta % hz;
	ns = (rem * NS_PER_S) / hz;

	return secs * NS_PER_S + ns + self->offset_ns;
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
	uint8_t buf[PCAPNG_BLKSIZ];
	uint32_t len;

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

	if (len > sizeof(buf))
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

	return write(self->outfd, buf, len);
}

/* Write an interface block for a DPDK port */
int
rte_pcapng_add_interface(rte_pcapng_t *self, uint16_t port,
			 const char *ifname, const char *ifdescr,
			 const char *filter)
{
	struct pcapng_interface_block *hdr;
	struct rte_eth_dev_info dev_info;
	struct rte_ether_addr *ea, macaddr;
	const struct rte_device *dev;
	struct rte_eth_link link;
	struct pcapng_option *opt;
	const uint8_t tsresol = 9;	/* nanosecond resolution */
	uint32_t len;
	uint8_t buf[PCAPNG_BLKSIZ];
	char ifname_buf[IF_NAMESIZE];
	char ifhw[256];
	uint64_t speed = 0;

	if (rte_eth_dev_info_get(port, &dev_info) < 0)
		return -1;

	/* make something like an interface name */
	if (ifname == NULL) {
		/* Use kernel name if available */
		ifname = if_indextoname(dev_info.if_index, ifname_buf);
		if (ifname == NULL) {
			snprintf(ifname_buf, IF_NAMESIZE, "dpdk:%u", port);
			ifname = ifname_buf;
		}
	}

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

	if (ifdescr)
		len += pcapng_optlen(strlen(ifdescr));
	if (ea)
		len += pcapng_optlen(RTE_ETHER_ADDR_LEN); /* macaddr */
	if (speed != 0)
		len += pcapng_optlen(sizeof(uint64_t));
	if (filter)
		len += pcapng_optlen(strlen(filter) + 1);
	if (dev)
		len += pcapng_optlen(strlen(ifhw));

	len += pcapng_optlen(0);
	len += sizeof(uint32_t);

	if (len > sizeof(buf))
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
	if (ifdescr)
		opt = pcapng_add_option(opt, PCAPNG_IFB_DESCRIPTION,
					ifdescr, strlen(ifdescr));
	if (ea)
		opt = pcapng_add_option(opt, PCAPNG_IFB_MACADDR,
					ea, RTE_ETHER_ADDR_LEN);
	if (speed != 0)
		opt = pcapng_add_option(opt, PCAPNG_IFB_SPEED,
					 &speed, sizeof(uint64_t));
	if (dev)
		opt = pcapng_add_option(opt, PCAPNG_IFB_HARDWARE,
					 ifhw, strlen(ifhw));
	if (filter) {
		size_t len;

		len = strlen(filter) + 1;
		opt->code = PCAPNG_IFB_FILTER;
		opt->length = len;
		/* Encoding is that the first octet indicates string vs BPF */
		opt->data[0] = 0;
		memcpy(opt->data + 1, filter, strlen(filter));

		opt = (struct pcapng_option *)((uint8_t *)opt + pcapng_optlen(len));
	}

	opt = pcapng_add_option(opt, PCAPNG_OPT_END, NULL, 0);

	/* clone block_length after optionsa */
	memcpy(opt, &hdr->block_length, sizeof(uint32_t));

	/* remember the file index */
	self->port_index[port] = self->ports++;

	return write(self->outfd, buf, len);
}

/*
 * Write an Interface statistics block at the end of capture.
 */
ssize_t
rte_pcapng_write_stats(rte_pcapng_t *self, uint16_t port_id,
		       uint64_t ifrecv, uint64_t ifdrop,
		       const char *comment)
{
	struct pcapng_statistics *hdr;
	struct pcapng_option *opt;
	uint64_t start_time = self->offset_ns;
	uint64_t sample_time;
	uint32_t optlen, len;
	uint8_t buf[PCAPNG_BLKSIZ];

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -EINVAL);

	optlen = 0;

	if (ifrecv != UINT64_MAX)
		optlen += pcapng_optlen(sizeof(ifrecv));
	if (ifdrop != UINT64_MAX)
		optlen += pcapng_optlen(sizeof(ifdrop));

	if (start_time != 0)
		optlen += pcapng_optlen(sizeof(start_time));

	if (comment)
		optlen += pcapng_optlen(strlen(comment));
	if (optlen != 0)
		optlen += pcapng_optlen(0);

	len = sizeof(*hdr) + optlen + sizeof(uint32_t);
	if (len > sizeof(buf))
		return -1;

	hdr = (struct pcapng_statistics *)buf;
	opt = (struct pcapng_option *)(hdr + 1);

	if (comment)
		opt = pcapng_add_option(opt, PCAPNG_OPT_COMMENT,
					comment, strlen(comment));
	if (start_time != 0)
		opt = pcapng_add_option(opt, PCAPNG_ISB_STARTTIME,
					 &start_time, sizeof(start_time));
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

	sample_time = pcapng_timestamp(self, rte_get_tsc_cycles());
	hdr->timestamp_hi = sample_time >> 32;
	hdr->timestamp_lo = (uint32_t)sample_time;

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
		uint32_t length,
		enum rte_pcapng_direction direction,
		const char *comment)
{
	struct pcapng_enhance_packet_block *epb;
	uint32_t orig_len, data_len, padding, flags;
	struct pcapng_option *opt;
	uint64_t timestamp;
	uint16_t optlen;
	struct rte_mbuf *mc;
	bool rss_hash;

#ifdef RTE_LIBRTE_ETHDEV_DEBUG
	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, NULL);
#endif
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

	/* record HASH on incoming packets */
	rss_hash = (direction == RTE_PCAPNG_DIRECTION_IN &&
		    (md->ol_flags & RTE_MBUF_F_RX_RSS_HASH));

	/* pad the packet to 32 bit boundary */
	data_len = rte_pktmbuf_data_len(mc);
	padding = RTE_ALIGN(data_len, sizeof(uint32_t)) - data_len;
	if (padding > 0) {
		void *tail = rte_pktmbuf_append(mc, padding);

		if (tail == NULL)
			goto fail;
		memset(tail, 0, padding);
	}

	optlen = pcapng_optlen(sizeof(flags));
	optlen += pcapng_optlen(sizeof(queue));
	if (rss_hash)
		optlen += pcapng_optlen(sizeof(uint8_t) + sizeof(uint32_t));

	if (comment)
		optlen += pcapng_optlen(strlen(comment));

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

	if (rss_hash) {
		uint8_t hash_opt[5];

		/* The algorithm could be something else if
		 * using rte_flow_action_rss; but the current API does not
		 * have a way for ethdev to report  this on a per-packet basis.
		 */
		hash_opt[0] = PCAPNG_HASH_TOEPLITZ;

		memcpy(&hash_opt[1], &md->hash.rss, sizeof(uint32_t));
		opt = pcapng_add_option(opt, PCAPNG_EPB_HASH,
					&hash_opt, sizeof(hash_opt));
	}

	if (comment)
		opt = pcapng_add_option(opt, PCAPNG_OPT_COMMENT, comment,
					strlen(comment));

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

	/* Put timestamp in cycles here - adjust in packet write */
	timestamp = rte_get_tsc_cycles();
	epb->timestamp_hi = timestamp >> 32;
	epb->timestamp_lo = (uint32_t)timestamp;
	epb->capture_length = data_len;
	epb->original_length = orig_len;

	/* set trailer of block length */
	*(uint32_t *)opt = epb->block_length;

	return mc;

fail:
	rte_pktmbuf_free(mc);
	return NULL;
}

/* Write pre-formatted packets to file. */
ssize_t
rte_pcapng_write_packets(rte_pcapng_t *self,
			 struct rte_mbuf *pkts[], uint16_t nb_pkts)
{
	struct iovec iov[IOV_MAX];
	unsigned int i, cnt = 0;
	ssize_t ret, total = 0;

	for (i = 0; i < nb_pkts; i++) {
		struct rte_mbuf *m = pkts[i];
		struct pcapng_enhance_packet_block *epb;
		uint64_t cycles, timestamp;

		/* sanity check that is really a pcapng mbuf */
		epb = rte_pktmbuf_mtod(m, struct pcapng_enhance_packet_block *);
		if (unlikely(epb->block_type != PCAPNG_ENHANCED_PACKET_BLOCK ||
			     epb->block_length != rte_pktmbuf_data_len(m))) {
			rte_errno = EINVAL;
			return -1;
		}

		/* check that this interface was added. */
		epb->interface_id = self->port_index[m->port];
		if (unlikely(epb->interface_id > RTE_MAX_ETHPORTS)) {
			rte_errno = EINVAL;
			return -1;
		}

		/* adjust timestamp recorded in packet */
		cycles = (uint64_t)epb->timestamp_hi << 32;
		cycles += epb->timestamp_lo;
		timestamp = pcapng_timestamp(self, cycles);
		epb->timestamp_hi = timestamp >> 32;
		epb->timestamp_lo = (uint32_t)timestamp;

		/*
		 * Handle case of highly fragmented and large burst size
		 * Note: this assumes that max segments per mbuf < IOV_MAX
		 */
		if (unlikely(cnt + m->nb_segs >= IOV_MAX)) {
			ret = writev(self->outfd, iov, cnt);
			if (unlikely(ret < 0)) {
				rte_errno = errno;
				return -1;
			}
			total += ret;
			cnt = 0;
		}

		/*
		 * The DPDK port is recorded during pcapng_copy.
		 * Map that to PCAPNG interface in file.
		 */
		do {
			iov[cnt].iov_base = rte_pktmbuf_mtod(m, void *);
			iov[cnt].iov_len = rte_pktmbuf_data_len(m);
			++cnt;
		} while ((m = m->next));
	}

	ret = writev(self->outfd, iov, cnt);
	if (unlikely(ret < 0)) {
		rte_errno = errno;
		return -1;
	}
	return total + ret;
}

/* Create new pcapng writer handle */
rte_pcapng_t *
rte_pcapng_fdopen(int fd,
		  const char *osname, const char *hardware,
		  const char *appname, const char *comment)
{
	unsigned int i;
	rte_pcapng_t *self;
	struct timespec ts;
	uint64_t cycles;

	self = malloc(sizeof(*self));
	if (!self) {
		rte_errno = ENOMEM;
		return NULL;
	}

	self->outfd = fd;
	self->ports = 0;

	/* record start time in ns since 1/1/1970 */
	cycles = rte_get_tsc_cycles();
	clock_gettime(CLOCK_REALTIME, &ts);
	self->tsc_base = (cycles + rte_get_tsc_cycles()) / 2;
	self->offset_ns = rte_timespec_to_ns(&ts);

	for (i = 0; i < RTE_MAX_ETHPORTS; i++)
		self->port_index[i] = UINT32_MAX;

	if (pcapng_section_block(self, osname, hardware, appname, comment) < 0)
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
