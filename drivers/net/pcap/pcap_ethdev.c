/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2016 Intel Corporation.
 * Copyright(c) 2014 6WIND S.A.
 * All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <inttypes.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <pcap.h>

#include <rte_cycles.h>
#include <rte_ring.h>
#include <rte_ethdev.h>
#include <ethdev_driver.h>
#include <ethdev_vdev.h>
#include <rte_kvargs.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>
#include <rte_mbuf.h>
#include <rte_mbuf_dyn.h>
#include <bus_vdev_driver.h>
#include <rte_os_shim.h>
#include <rte_time.h>

#include "pcap_osdep.h"

#define ETH_PCAP_RX_PCAP_ARG  "rx_pcap"
#define ETH_PCAP_TX_PCAP_ARG  "tx_pcap"
#define ETH_PCAP_RX_IFACE_ARG "rx_iface"
#define ETH_PCAP_RX_IFACE_IN_ARG "rx_iface_in"
#define ETH_PCAP_TX_IFACE_ARG "tx_iface"
#define ETH_PCAP_IFACE_ARG    "iface"
#define ETH_PCAP_PHY_MAC_ARG  "phy_mac"
#define ETH_PCAP_INFINITE_RX_ARG  "infinite_rx"
#define ETH_PCAP_SNAPSHOT_LEN_ARG "snaplen"

#define ETH_PCAP_SNAPSHOT_LEN_DEFAULT 65535

/* This is defined in libpcap but not exposed in headers */
#define ETH_PCAP_MAXIMUM_SNAPLEN      262144

#define ETH_PCAP_ARG_MAXLEN	64

#define RTE_PMD_PCAP_MAX_QUEUES 16

static struct timespec start_time;
static uint64_t start_cycles;
static uint64_t hz;

static uint64_t timestamp_rx_dynflag;
static int timestamp_dynfield_offset = -1;

struct queue_stat {
	volatile unsigned long pkts;
	volatile unsigned long bytes;
	volatile unsigned long err_pkts;
	volatile unsigned long rx_nombuf;
};

struct queue_missed_stat {
	/* last value retrieved from pcap */
	unsigned int pcap;
	/* stores values lost by pcap stop or rollover */
	unsigned long mnemonic;
	/* value on last reset */
	unsigned long reset;
};

struct pcap_rx_queue {
	uint16_t port_id;
	uint16_t queue_id;
	bool vlan_strip;
	bool rx_scatter;
	bool timestamp_offloading;
	struct rte_mempool *mb_pool;
	struct queue_stat rx_stat;
	struct queue_missed_stat missed_stat;
	char name[PATH_MAX];
	char type[ETH_PCAP_ARG_MAXLEN];

	/* Contains pre-generated packets to be looped through */
	struct rte_ring *pkts;
};

struct pcap_tx_queue {
	uint16_t port_id;
	uint16_t queue_id;
	struct queue_stat tx_stat;
	char name[PATH_MAX];
	char type[ETH_PCAP_ARG_MAXLEN];

	/* Temp buffer used for non-linear packets */
	uint8_t *bounce_buf;
};

struct pmd_internals {
	struct pcap_rx_queue rx_queue[RTE_PMD_PCAP_MAX_QUEUES];
	struct pcap_tx_queue tx_queue[RTE_PMD_PCAP_MAX_QUEUES];
	char devargs[ETH_PCAP_ARG_MAXLEN];
	struct rte_ether_addr eth_addr;
	int if_index;
	uint32_t snapshot_len;
	bool single_iface;
	bool phy_mac;
	bool infinite_rx;
	bool vlan_strip;
	bool rx_scatter;
	bool timestamp_offloading;
};

struct pmd_process_private {
	pcap_t *rx_pcap[RTE_PMD_PCAP_MAX_QUEUES];
	pcap_t *tx_pcap[RTE_PMD_PCAP_MAX_QUEUES];
	pcap_dumper_t *tx_dumper[RTE_PMD_PCAP_MAX_QUEUES];
};

struct pmd_devargs {
	uint16_t num_of_queue;
	bool phy_mac;
	struct devargs_queue {
		pcap_dumper_t *dumper;
		/* pcap and name/type fields... */
		pcap_t *pcap;
		const char *name;
		const char *type;
	} queue[RTE_PMD_PCAP_MAX_QUEUES];
	uint32_t snapshot_len;
};

struct pmd_devargs_all {
	struct pmd_devargs rx_queues;
	struct pmd_devargs tx_queues;
	uint32_t snapshot_len;
	bool single_iface;
	bool is_tx_pcap;
	bool is_tx_iface;
	bool is_rx_pcap;
	bool is_rx_iface;
	bool infinite_rx;
};

static const char *valid_arguments[] = {
	ETH_PCAP_RX_PCAP_ARG,
	ETH_PCAP_TX_PCAP_ARG,
	ETH_PCAP_RX_IFACE_ARG,
	ETH_PCAP_RX_IFACE_IN_ARG,
	ETH_PCAP_TX_IFACE_ARG,
	ETH_PCAP_IFACE_ARG,
	ETH_PCAP_PHY_MAC_ARG,
	ETH_PCAP_INFINITE_RX_ARG,
	ETH_PCAP_SNAPSHOT_LEN_ARG,
	NULL
};

RTE_LOG_REGISTER_DEFAULT(eth_pcap_logtype, NOTICE);

static struct queue_missed_stat*
queue_missed_stat_update(struct rte_eth_dev *dev, unsigned int qid)
{
	struct pmd_internals *internals = dev->data->dev_private;
	struct queue_missed_stat *missed_stat =
			&internals->rx_queue[qid].missed_stat;
	const struct pmd_process_private *pp = dev->process_private;
	pcap_t *pcap = pp->rx_pcap[qid];
	struct pcap_stat stat;

	if (!pcap || (pcap_stats(pcap, &stat) != 0))
		return missed_stat;

	/* rollover check - best effort fixup assuming single rollover */
	if (stat.ps_drop < missed_stat->pcap)
		missed_stat->mnemonic += UINT_MAX;
	missed_stat->pcap = stat.ps_drop;

	return missed_stat;
}

static void
queue_missed_stat_on_stop_update(struct rte_eth_dev *dev, unsigned int qid)
{
	struct queue_missed_stat *missed_stat =
			queue_missed_stat_update(dev, qid);

	missed_stat->mnemonic += missed_stat->pcap;
	missed_stat->pcap = 0;
}

static void
queue_missed_stat_reset(struct rte_eth_dev *dev, unsigned int qid)
{
	struct queue_missed_stat *missed_stat =
			queue_missed_stat_update(dev, qid);

	missed_stat->reset = missed_stat->pcap;
	missed_stat->mnemonic = 0;
}

static unsigned long
queue_missed_stat_get(struct rte_eth_dev *dev, unsigned int qid)
{
	const struct queue_missed_stat *missed_stat =
			queue_missed_stat_update(dev, qid);

	return missed_stat->pcap + missed_stat->mnemonic - missed_stat->reset;
}

static int
eth_pcap_rx_jumbo(struct rte_mempool *mb_pool, struct rte_mbuf *mbuf,
		const u_char *data, uint16_t data_len)
{
	/* Copy the first segment. */
	uint16_t len = rte_pktmbuf_tailroom(mbuf);
	struct rte_mbuf *m = mbuf;

	rte_memcpy(rte_pktmbuf_append(mbuf, len), data, len);
	data_len -= len;
	data += len;

	while (data_len > 0) {
		/* Allocate next mbuf and point to that. */
		m->next = rte_pktmbuf_alloc(mb_pool);

		if (unlikely(!m->next))
			return -1;

		m = m->next;

		/* Headroom is not needed in chained mbufs. */
		rte_pktmbuf_prepend(m, rte_pktmbuf_headroom(m));
		m->pkt_len = 0;
		m->data_len = 0;

		/* Copy next segment. */
		len = RTE_MIN(rte_pktmbuf_tailroom(m), data_len);
		rte_memcpy(rte_pktmbuf_append(m, len), data, len);

		mbuf->nb_segs++;
		data_len -= len;
		data += len;
	}

	return mbuf->nb_segs;
}

static uint16_t
eth_pcap_rx_infinite(void *queue, struct rte_mbuf **bufs, uint16_t nb_pkts)
{
	int i;
	struct pcap_rx_queue *pcap_q = queue;
	uint32_t rx_bytes = 0;

	if (unlikely(nb_pkts == 0))
		return 0;

	if (rte_pktmbuf_alloc_bulk(pcap_q->mb_pool, bufs, nb_pkts) != 0)
		return 0;

	for (i = 0; i < nb_pkts; i++) {
		struct rte_mbuf *pcap_buf;
		int err = rte_ring_dequeue(pcap_q->pkts, (void **)&pcap_buf);
		if (err)
			return i;

		rte_memcpy(rte_pktmbuf_mtod(bufs[i], void *),
				rte_pktmbuf_mtod(pcap_buf, void *),
				pcap_buf->data_len);
		bufs[i]->data_len = pcap_buf->data_len;
		bufs[i]->pkt_len = pcap_buf->pkt_len;
		bufs[i]->port = pcap_q->port_id;

		if (pcap_q->vlan_strip)
			rte_vlan_strip(bufs[i]);

		if (pcap_q->timestamp_offloading) {
			struct timespec ts;

			timespec_get(&ts, TIME_UTC);
			*RTE_MBUF_DYNFIELD(bufs[i], timestamp_dynfield_offset,
					   rte_mbuf_timestamp_t *) = rte_timespec_to_ns(&ts);
			bufs[i]->ol_flags |= timestamp_rx_dynflag;
		}

		rx_bytes += bufs[i]->data_len;

		/* Enqueue packet back on ring to allow infinite rx. */
		rte_ring_enqueue(pcap_q->pkts, pcap_buf);
	}

	pcap_q->rx_stat.pkts += i;
	pcap_q->rx_stat.bytes += rx_bytes;

	return i;
}

static uint16_t
eth_pcap_rx(void *queue, struct rte_mbuf **bufs, uint16_t nb_pkts)
{
	unsigned int i;
	struct pcap_pkthdr *header;
	struct pmd_process_private *pp;
	const u_char *packet;
	struct rte_mbuf *mbuf;
	struct pcap_rx_queue *pcap_q = queue;
	uint16_t num_rx = 0;
	uint32_t rx_bytes = 0;
	pcap_t *pcap;

	pp = rte_eth_devices[pcap_q->port_id].process_private;
	pcap = pp->rx_pcap[pcap_q->queue_id];

	if (unlikely(pcap == NULL || nb_pkts == 0))
		return 0;

	/* Reads the given number of packets from the pcap file one by one
	 * and copies the packet data into a newly allocated mbuf to return.
	 */
	for (i = 0; i < nb_pkts; i++) {
		/* Get the next PCAP packet */
		int ret = pcap_next_ex(pcap, &header, &packet);
		if (ret != 1) {
			if (ret == PCAP_ERROR)
				pcap_q->rx_stat.err_pkts++;

			break;
		}

		mbuf = rte_pktmbuf_alloc(pcap_q->mb_pool);
		if (unlikely(mbuf == NULL)) {
			pcap_q->rx_stat.rx_nombuf++;
			break;
		}

		uint32_t len = header->caplen;
		if (len <= rte_pktmbuf_tailroom(mbuf)) {
			/* pcap packet will fit in the mbuf, can copy it */
			rte_memcpy(rte_pktmbuf_mtod(mbuf, void *), packet, len);
			mbuf->data_len = len;
		} else if (pcap_q->rx_scatter) {
			/* Scatter into multi-segment mbufs. */
			if (unlikely(eth_pcap_rx_jumbo(pcap_q->mb_pool,
						       mbuf, packet, len) == -1)) {
				pcap_q->rx_stat.err_pkts++;
				rte_pktmbuf_free(mbuf);
				break;
			}
		} else {
			/* Packet too large and scatter not enabled, drop it. */
			pcap_q->rx_stat.err_pkts++;
			rte_pktmbuf_free(mbuf);
			continue;
		}

		mbuf->pkt_len = len;

		if (pcap_q->vlan_strip)
			rte_vlan_strip(mbuf);

		if (pcap_q->timestamp_offloading) {
			/*
			 * The use of tv_usec as nanoseconds is not a bug here.
			 * Interface is always created with nanosecond precision, and
			 * that is how pcap API bodged in nanoseconds support.
			 */
			uint64_t ns = (uint64_t)header->ts.tv_sec * NSEC_PER_SEC
				+  header->ts.tv_usec;

			*RTE_MBUF_DYNFIELD(mbuf, timestamp_dynfield_offset,
					   rte_mbuf_timestamp_t *) = ns;

			mbuf->ol_flags |= timestamp_rx_dynflag;
		}

		mbuf->port = pcap_q->port_id;
		bufs[num_rx] = mbuf;
		num_rx++;
		rx_bytes += len;
	}
	pcap_q->rx_stat.pkts += num_rx;
	pcap_q->rx_stat.bytes += rx_bytes;

	return num_rx;
}

static uint16_t
eth_null_rx(void *queue __rte_unused,
		struct rte_mbuf **bufs __rte_unused,
		uint16_t nb_pkts __rte_unused)
{
	return 0;
}

/*
 * Calculate current timestamp in nanoseconds by computing
 * offset from starting time value.
 *
 * Note: it is not a bug that this code is putting nanosecond
 * value into microsecond timeval field. The pcap API is old
 * and nanoseconds were bodged on as an after thought.
 * As long as the pcap stream is set to nanosecond precision
 * it expects nanoseconds here.
 */
static inline void
calculate_timestamp(struct timeval *ts)
{
	uint64_t cycles;
	struct timespec cur_time;

	cycles = rte_get_timer_cycles() - start_cycles;
	cur_time.tv_sec = cycles / hz;
	cur_time.tv_nsec = (cycles % hz) * NSEC_PER_SEC / hz;

	ts->tv_sec = start_time.tv_sec + cur_time.tv_sec;
	ts->tv_usec = start_time.tv_nsec + cur_time.tv_nsec;
	if (ts->tv_usec >= NSEC_PER_SEC) {
		ts->tv_usec -= NSEC_PER_SEC;
		ts->tv_sec += 1;
	}
}

/*
 * Insert VLAN tag into packet.
 *
 * rte_vlan_insert() modifies the mbuf in place, prepending
 * RTE_VLAN_HLEN bytes. If mbuf cannot safely be modified in place
 * a private copy is made first.
 *
 * The caller's mbuf pointer is updated on success; on failure the
 * original mbuf is freed and -1 is returned.
 */
static int
pcap_tx_vlan_insert(struct rte_mbuf **m)
{
	struct rte_mbuf *mbuf = *m;

	if (rte_mbuf_refcnt_read(mbuf) > 1 ||
	    rte_pktmbuf_headroom(mbuf) < RTE_VLAN_HLEN) {
		struct rte_mbuf *copy;

		copy = rte_pktmbuf_copy(mbuf, mbuf->pool, 0, UINT32_MAX);
		if (unlikely(copy == NULL)) {
			rte_pktmbuf_free(mbuf);
			return -1;
		}
		copy->ol_flags |= RTE_MBUF_F_TX_VLAN;
		copy->vlan_tci = mbuf->vlan_tci;
		rte_pktmbuf_free(mbuf);
		*m = copy;
		mbuf = copy;
	}

	if (unlikely(rte_vlan_insert(&mbuf) != 0)) {
		rte_pktmbuf_free(mbuf);
		return -1;
	}
	*m = mbuf;
	return 0;
}

/*
 * Callback to handle writing packets to a pcap file.
 */
static uint16_t
eth_pcap_tx_dumper(void *queue, struct rte_mbuf **bufs, uint16_t nb_pkts)
{
	struct pcap_tx_queue *dumper_q = queue;
	struct rte_eth_dev *dev = &rte_eth_devices[dumper_q->port_id];
	struct pmd_internals *internals = dev->data->dev_private;
	struct pmd_process_private *pp = dev->process_private;
	pcap_dumper_t *dumper = pp->tx_dumper[dumper_q->queue_id];
	unsigned char *temp_data = dumper_q->bounce_buf;
	uint32_t snaplen = internals->snapshot_len;
	uint16_t num_tx = 0;
	uint32_t tx_bytes = 0;
	struct pcap_pkthdr header;
	unsigned int i;

	if (unlikely(dumper == NULL))
		return 0;

	/* all packets in burst have same timestamp */
	calculate_timestamp(&header.ts);

	/* writes the nb_pkts packets to the previously opened pcap file dumper */
	for (i = 0; i < nb_pkts; i++) {
		struct rte_mbuf *mbuf = bufs[i];
		uint32_t len, caplen;
		const uint8_t *data;

		/* Do VLAN tag insertion */
		if (unlikely(mbuf->ol_flags & RTE_MBUF_F_TX_VLAN)) {
			if (pcap_tx_vlan_insert(&mbuf) != 0) {
				dumper_q->tx_stat.err_pkts++;
				continue;
			}
			bufs[i] = mbuf;
		}

		len = rte_pktmbuf_pkt_len(mbuf);
		caplen = RTE_MIN(len, snaplen);

		header.len = len;
		header.caplen = caplen;

		data = rte_pktmbuf_read(mbuf, 0, caplen, temp_data);

		/* This could only happen if mbuf is bogus pkt_len > data_len */
		RTE_ASSERT(data != NULL);
		pcap_dump((u_char *)dumper, &header, data);

		num_tx++;
		tx_bytes += caplen;

		rte_pktmbuf_free(mbuf);
	}

	/*
	 * Since there's no place to hook a callback when the forwarding
	 * process stops and to make sure the pcap file is actually written,
	 * we flush the pcap dumper within each burst.
	 */
	pcap_dump_flush(dumper);
	dumper_q->tx_stat.pkts += num_tx;
	dumper_q->tx_stat.bytes += tx_bytes;

	return i;
}

/*
 * Callback to handle dropping packets in the infinite rx case.
 */
static uint16_t
eth_tx_drop(void *queue, struct rte_mbuf **bufs, uint16_t nb_pkts)
{
	unsigned int i;
	uint32_t tx_bytes = 0;
	struct pcap_tx_queue *tx_queue = queue;

	for (i = 0; i < nb_pkts; i++) {
		tx_bytes += bufs[i]->pkt_len;
		rte_pktmbuf_free(bufs[i]);
	}

	tx_queue->tx_stat.pkts += nb_pkts;
	tx_queue->tx_stat.bytes += tx_bytes;

	return nb_pkts;
}

/*
 * Send a burst of packets to a pcap device.
 *
 * On Linux, pcap_sendpacket() calls send() on a blocking PF_PACKET
 * socket with default kernel buffer sizes and no TX ring (PACKET_TX_RING).
 * The send() call only blocks when the kernel socket send buffer is full,
 * providing limited backpressure.
 *
 * On error, pcap_sendpacket() returns non-zero and the loop breaks,
 * leaving remaining packets unsent.
 *
 * Bottom line: backpressure is not an error.
 */
static uint16_t
eth_pcap_tx(void *queue, struct rte_mbuf **bufs, uint16_t nb_pkts)
{
	struct pcap_tx_queue *tx_queue = queue;
	struct rte_eth_dev *dev = &rte_eth_devices[tx_queue->port_id];
	struct pmd_process_private *pp = dev->process_private;
	struct pmd_internals *internals = dev->data->dev_private;
	uint32_t snaplen = internals->snapshot_len;
	pcap_t *pcap = pp->tx_pcap[tx_queue->queue_id];
	unsigned char *temp_data = tx_queue->bounce_buf;
	uint16_t num_tx = 0;
	uint32_t tx_bytes = 0;
	unsigned int i;

	if (unlikely(pcap == NULL))
		return 0;

	for (i = 0; i < nb_pkts; i++) {
		struct rte_mbuf *mbuf = bufs[i];
		uint32_t len;
		const uint8_t *data;

		/* Do VLAN tag insertion */
		if (unlikely(mbuf->ol_flags & RTE_MBUF_F_TX_VLAN)) {
			if (pcap_tx_vlan_insert(&mbuf) != 0) {
				tx_queue->tx_stat.err_pkts++;
				continue;
			}
			bufs[i] = mbuf;
		}

		len = rte_pktmbuf_pkt_len(mbuf);

		/*
		 * multi-segment transmit that has to go through bounce buffer.
		 * Make sure it fits; don't want to truncate the packet.
		 */
		if (unlikely(!rte_pktmbuf_is_contiguous(mbuf) && len > snaplen)) {
			PMD_TX_LOG(ERR, "Multi segment len (%u) > snaplen (%u)",
				   len, snaplen);
			rte_pktmbuf_free(mbuf);
			tx_queue->tx_stat.err_pkts++;
			continue;
		}

		data = rte_pktmbuf_read(mbuf, 0, len, temp_data);
		RTE_ASSERT(data != NULL);

		if (unlikely(pcap_sendpacket(pcap, data, len) != 0)) {
			/* Assume failure is backpressure */
			PMD_LOG(ERR, "pcap_sendpacket() failed: %s", pcap_geterr(pcap));
			break;
		}
		num_tx++;
		tx_bytes += len;
		rte_pktmbuf_free(mbuf);
	}

	tx_queue->tx_stat.pkts += num_tx;
	tx_queue->tx_stat.bytes += tx_bytes;

	return i;
}

/*
 * pcap_open_live wrapper function
 */
static inline int
open_iface_live(const char *iface, pcap_t **pcap, uint32_t snaplen)
{
	char errbuf[PCAP_ERRBUF_SIZE];
	pcap_t *pc;
	int status;

	pc = pcap_create(iface, errbuf);
	if (pc == NULL) {
		PMD_LOG(ERR, "Couldn't create %s: %s", iface, errbuf);
		goto error;
	}

	status = pcap_set_tstamp_precision(pc, PCAP_TSTAMP_PRECISION_NANO);
	if (status != 0) {
		PMD_LOG(ERR, "%s: Could not set to ns precision: %s",
			iface, pcap_statustostr(status));
		goto error;
	}

	status = pcap_set_immediate_mode(pc, 1);
	if (status != 0)
		PMD_LOG(WARNING, "%s: Could not set to immediate mode: %s",
			iface, pcap_statustostr(status));

	status = pcap_set_promisc(pc, 1);
	if (status != 0)
		PMD_LOG(WARNING, "%s: Could not set to promiscuous: %s",
			iface, pcap_statustostr(status));

	status = pcap_set_snaplen(pc, snaplen);
	if (status != 0)
		PMD_LOG(WARNING, "%s: Could not set snapshot length: %s",
			iface, pcap_statustostr(status));

	status = pcap_activate(pc);
	if (status < 0) {
		char *cp = pcap_geterr(pc);

		if (status == PCAP_ERROR)
			PMD_LOG(ERR, "%s: could not activate: %s", iface, cp);
		else
			PMD_LOG(ERR, "%s: %s (%s)", iface, pcap_statustostr(status), cp);
		goto error;
	} else if (status > 0) {
		/* Warning condition - log but continue */
		PMD_LOG(WARNING, "%s: %s", iface, pcap_statustostr(status));
	}

	/*
	 * Verify interface supports Ethernet link type.
	 * Loopback on FreeBSD/macOS uses DLT_NULL which expects a 4-byte
	 * address family header instead of Ethernet, causing kernel warnings.
	 */
	if (pcap_datalink(pc) != DLT_EN10MB) {
		PMD_LOG(ERR, "%s: not Ethernet (link type %d)",
			iface, pcap_datalink(pc));
		goto error;
	}

	if (pcap_setnonblock(pc, 1, errbuf)) {
		PMD_LOG(ERR, "Couldn't set non-blocking on %s: %s", iface, errbuf);
		goto error;
	}

	*pcap = pc;
	return 0;

error:
	if (pc != NULL)
		pcap_close(pc);
	return -1;
}

static int
open_single_iface(const char *iface, pcap_t **pcap, uint32_t snaplen)
{
	if (open_iface_live(iface, pcap, snaplen) < 0) {
		PMD_LOG(ERR, "Couldn't open interface %s", iface);
		return -1;
	}

	return 0;
}

static int
open_single_tx_pcap(const char *pcap_filename, pcap_dumper_t **dumper,
		    uint32_t snaplen)
{
	pcap_t *tx_pcap;

	/*
	 * We need to create a dummy empty pcap_t to use it
	 * with pcap_dump_open(). We create big enough an Ethernet
	 * pcap holder.
	 */
	tx_pcap = pcap_open_dead_with_tstamp_precision(DLT_EN10MB,
			snaplen, PCAP_TSTAMP_PRECISION_NANO);
	if (tx_pcap == NULL) {
		PMD_LOG(ERR, "Couldn't create dead pcap");
		return -1;
	}

	/* The dumper is created using the previous pcap_t reference */
	*dumper = pcap_dump_open(tx_pcap, pcap_filename);
	if (*dumper == NULL) {
		PMD_LOG(ERR, "Couldn't open %s for writing: %s",
			pcap_filename, pcap_geterr(tx_pcap));
		pcap_close(tx_pcap);
		return -1;
	}

	pcap_close(tx_pcap);
	return 0;
}

static int
open_single_rx_pcap(const char *pcap_filename, pcap_t **pcap)
{
	char errbuf[PCAP_ERRBUF_SIZE];

	*pcap = pcap_open_offline_with_tstamp_precision(pcap_filename,
							PCAP_TSTAMP_PRECISION_NANO, errbuf);
	if (*pcap == NULL) {
		PMD_LOG(ERR, "Couldn't open %s: %s", pcap_filename,
			errbuf);
		return -1;
	}

	return 0;
}

static uint64_t
count_packets_in_pcap(pcap_t **pcap, struct pcap_rx_queue *pcap_q)
{
	const u_char *packet;
	struct pcap_pkthdr header;
	uint64_t pcap_pkt_count = 0;

	while ((packet = pcap_next(*pcap, &header)))
		pcap_pkt_count++;

	/* The pcap is reopened so it can be used as normal later. */
	pcap_close(*pcap);
	*pcap = NULL;
	open_single_rx_pcap(pcap_q->name, pcap);

	return pcap_pkt_count;
}

static int
set_iface_direction(const char *iface, pcap_t *pcap,
		pcap_direction_t direction)
{
	const char *direction_str = (direction == PCAP_D_IN) ? "IN" : "OUT";
	if (pcap_setdirection(pcap, direction) < 0) {
		PMD_LOG(ERR, "Setting %s pcap direction %s failed - %s",
				iface, direction_str, pcap_geterr(pcap));
		return -1;
	}
	PMD_LOG(INFO, "Setting %s pcap direction %s",
			iface, direction_str);
	return 0;
}

static int
eth_dev_start(struct rte_eth_dev *dev)
{
	unsigned int i;
	struct pmd_internals *internals = dev->data->dev_private;
	struct pmd_process_private *pp = dev->process_private;
	struct pcap_tx_queue *tx;
	struct pcap_rx_queue *rx;
	uint32_t snaplen = internals->snapshot_len;

	/* Special iface case. Single pcap is open and shared between tx/rx. */
	if (internals->single_iface) {
		tx = &internals->tx_queue[0];
		rx = &internals->rx_queue[0];

		if (!pp->tx_pcap[0] && strcmp(tx->type, ETH_PCAP_IFACE_ARG) == 0) {
			if (open_single_iface(tx->name, &pp->tx_pcap[0], snaplen) < 0)
				return -1;
			pp->rx_pcap[0] = pp->tx_pcap[0];
		}

		goto status_up;
	}

	/* If not open already, open tx pcaps/dumpers */
	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		tx = &internals->tx_queue[i];

		if (!pp->tx_dumper[i] && strcmp(tx->type, ETH_PCAP_TX_PCAP_ARG) == 0) {
			if (open_single_tx_pcap(tx->name, &pp->tx_dumper[i], snaplen) < 0)
				return -1;
		} else if (!pp->tx_pcap[i] && strcmp(tx->type, ETH_PCAP_TX_IFACE_ARG) == 0) {
			if (open_single_iface(tx->name, &pp->tx_pcap[i], snaplen) < 0)
				return -1;
		}
	}

	/* If not open already, open rx pcaps */
	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		rx = &internals->rx_queue[i];

		if (pp->rx_pcap[i] != NULL)
			continue;

		if (strcmp(rx->type, ETH_PCAP_RX_PCAP_ARG) == 0) {
			if (open_single_rx_pcap(rx->name, &pp->rx_pcap[i]) < 0)
				return -1;
		} else if (strcmp(rx->type, ETH_PCAP_RX_IFACE_ARG) == 0 ||
			   strcmp(rx->type, ETH_PCAP_RX_IFACE_IN_ARG) == 0) {
			if (open_single_iface(rx->name, &pp->rx_pcap[i], snaplen) < 0)
				return -1;
			/* Set direction for rx_iface_in */
			if (strcmp(rx->type, ETH_PCAP_RX_IFACE_IN_ARG) == 0)
				set_iface_direction(rx->name, pp->rx_pcap[i],
						    PCAP_D_IN);
		}
	}

status_up:
	for (i = 0; i < dev->data->nb_rx_queues; i++)
		dev->data->rx_queue_state[i] = RTE_ETH_QUEUE_STATE_STARTED;

	for (i = 0; i < dev->data->nb_tx_queues; i++)
		dev->data->tx_queue_state[i] = RTE_ETH_QUEUE_STATE_STARTED;

	dev->data->dev_link.link_status = RTE_ETH_LINK_UP;

	return 0;
}

/*
 * This function gets called when the current port gets stopped.
 * Is the only place for us to close all the tx streams dumpers.
 * If not called the dumpers will be flushed within each tx burst.
 */
static int
eth_dev_stop(struct rte_eth_dev *dev)
{
	unsigned int i;
	struct pmd_internals *internals = dev->data->dev_private;
	struct pmd_process_private *pp = dev->process_private;

	/* Special iface case. Single pcap is open and shared between tx/rx. */
	if (internals->single_iface) {
		queue_missed_stat_on_stop_update(dev, 0);
		if (pp->tx_pcap[0] != NULL) {
			pcap_close(pp->tx_pcap[0]);
			pp->tx_pcap[0] = NULL;
			pp->rx_pcap[0] = NULL;
		}
		goto status_down;
	}

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		if (pp->tx_dumper[i] != NULL) {
			pcap_dump_close(pp->tx_dumper[i]);
			pp->tx_dumper[i] = NULL;
		}

		if (pp->tx_pcap[i] != NULL) {
			pcap_close(pp->tx_pcap[i]);
			pp->tx_pcap[i] = NULL;
		}
	}

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		if (pp->rx_pcap[i] != NULL) {
			queue_missed_stat_on_stop_update(dev, i);
			pcap_close(pp->rx_pcap[i]);
			pp->rx_pcap[i] = NULL;
		}
	}

status_down:
	for (i = 0; i < dev->data->nb_rx_queues; i++)
		dev->data->rx_queue_state[i] = RTE_ETH_QUEUE_STATE_STOPPED;

	for (i = 0; i < dev->data->nb_tx_queues; i++)
		dev->data->tx_queue_state[i] = RTE_ETH_QUEUE_STATE_STOPPED;

	dev->data->dev_link.link_status = RTE_ETH_LINK_DOWN;

	return 0;
}

static int
eth_dev_configure(struct rte_eth_dev *dev)
{
	struct pmd_internals *internals = dev->data->dev_private;
	struct rte_eth_conf *dev_conf = &dev->data->dev_conf;
	const struct rte_eth_rxmode *rxmode = &dev_conf->rxmode;

	internals->vlan_strip = !!(rxmode->offloads & RTE_ETH_RX_OFFLOAD_VLAN_STRIP);
	internals->rx_scatter = !!(rxmode->offloads & RTE_ETH_RX_OFFLOAD_SCATTER);
	internals->timestamp_offloading = !!(rxmode->offloads & RTE_ETH_RX_OFFLOAD_TIMESTAMP);

	if (internals->timestamp_offloading && timestamp_rx_dynflag == 0) {
		int ret = rte_mbuf_dyn_rx_timestamp_register(&timestamp_dynfield_offset,
							     &timestamp_rx_dynflag);
		if (ret != 0) {
			PMD_LOG(ERR, "Failed to register Rx timestamp field/flag");
			return ret;
		}
	}

	return 0;
}

static int
eth_dev_info(struct rte_eth_dev *dev,
		struct rte_eth_dev_info *dev_info)
{
	struct pmd_internals *internals = dev->data->dev_private;

	dev_info->if_index = internals->if_index;
	dev_info->max_mac_addrs = 1;
	dev_info->max_rx_pktlen = internals->snapshot_len;
	dev_info->max_rx_queues = dev->data->nb_rx_queues;
	dev_info->max_tx_queues = dev->data->nb_tx_queues;
	dev_info->min_rx_bufsize = RTE_ETHER_MIN_LEN;
	dev_info->max_mtu = internals->snapshot_len - RTE_ETHER_HDR_LEN;
	dev_info->tx_offload_capa = RTE_ETH_TX_OFFLOAD_MULTI_SEGS |
		RTE_ETH_TX_OFFLOAD_VLAN_INSERT;
	dev_info->rx_offload_capa = RTE_ETH_RX_OFFLOAD_VLAN_STRIP |
		RTE_ETH_RX_OFFLOAD_TIMESTAMP;

	if (!internals->infinite_rx)
		dev_info->rx_offload_capa |= RTE_ETH_RX_OFFLOAD_SCATTER;

	return 0;
}

static int
eth_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *stats,
	      struct eth_queue_stats *qstats)
{
	unsigned int i;
	unsigned long rx_packets_total = 0, rx_bytes_total = 0;
	unsigned long rx_missed_total = 0;
	unsigned long rx_nombuf_total = 0, rx_err_total = 0;
	unsigned long tx_packets_total = 0, tx_bytes_total = 0;
	unsigned long tx_packets_err_total = 0;
	const struct pmd_internals *internal = dev->data->dev_private;

	for (i = 0; i < RTE_ETHDEV_QUEUE_STAT_CNTRS &&
			i < dev->data->nb_rx_queues; i++) {
		if (qstats != NULL) {
			qstats->q_ipackets[i] = internal->rx_queue[i].rx_stat.pkts;
			qstats->q_ibytes[i] = internal->rx_queue[i].rx_stat.bytes;
		}
		rx_nombuf_total += internal->rx_queue[i].rx_stat.rx_nombuf;
		rx_err_total += internal->rx_queue[i].rx_stat.err_pkts;
		rx_packets_total += internal->rx_queue[i].rx_stat.pkts;
		rx_bytes_total += internal->rx_queue[i].rx_stat.bytes;
		rx_missed_total += queue_missed_stat_get(dev, i);
	}

	for (i = 0; i < RTE_ETHDEV_QUEUE_STAT_CNTRS &&
			i < dev->data->nb_tx_queues; i++) {
		if (qstats != NULL) {
			qstats->q_opackets[i] = internal->tx_queue[i].tx_stat.pkts;
			qstats->q_obytes[i] = internal->tx_queue[i].tx_stat.bytes;
		}
		tx_packets_total += internal->tx_queue[i].tx_stat.pkts;
		tx_bytes_total += internal->tx_queue[i].tx_stat.bytes;
		tx_packets_err_total += internal->tx_queue[i].tx_stat.err_pkts;
	}

	stats->ipackets = rx_packets_total;
	stats->ibytes = rx_bytes_total;
	stats->imissed = rx_missed_total;
	stats->ierrors = rx_err_total;
	stats->rx_nombuf = rx_nombuf_total;
	stats->opackets = tx_packets_total;
	stats->obytes = tx_bytes_total;
	stats->oerrors = tx_packets_err_total;

	return 0;
}

static int
eth_stats_reset(struct rte_eth_dev *dev)
{
	unsigned int i;
	struct pmd_internals *internal = dev->data->dev_private;

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		internal->rx_queue[i].rx_stat.pkts = 0;
		internal->rx_queue[i].rx_stat.bytes = 0;
		internal->rx_queue[i].rx_stat.err_pkts = 0;
		internal->rx_queue[i].rx_stat.rx_nombuf = 0;
		queue_missed_stat_reset(dev, i);
	}

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		internal->tx_queue[i].tx_stat.pkts = 0;
		internal->tx_queue[i].tx_stat.bytes = 0;
		internal->tx_queue[i].tx_stat.err_pkts = 0;
	}

	return 0;
}

static inline void
infinite_rx_ring_free(struct rte_ring *pkts)
{
	struct rte_mbuf *bufs;

	while (!rte_ring_dequeue(pkts, (void **)&bufs))
		rte_pktmbuf_free(bufs);

	rte_ring_free(pkts);
}

static int
eth_dev_close(struct rte_eth_dev *dev)
{
	unsigned int i;
	struct pmd_internals *internals = dev->data->dev_private;

	PMD_LOG(INFO, "Closing pcap ethdev on NUMA socket %d",
			rte_socket_id());

	eth_dev_stop(dev);

	rte_free(dev->process_private);

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	/* Device wide flag, but cleanup must be performed per queue. */
	if (internals->infinite_rx) {
		for (i = 0; i < dev->data->nb_rx_queues; i++) {
			struct pcap_rx_queue *pcap_q = &internals->rx_queue[i];

			/*
			 * 'pcap_q->pkts' can be NULL if 'eth_dev_close()'
			 * called before 'eth_rx_queue_setup()' has been called
			 */
			if (pcap_q->pkts == NULL)
				continue;

			infinite_rx_ring_free(pcap_q->pkts);
		}
	}

	if (!internals->phy_mac)
		/* not dynamically allocated, must not be freed */
		dev->data->mac_addrs = NULL;

	return 0;
}

static int
eth_link_update(struct rte_eth_dev *dev, int wait_to_complete __rte_unused)
{
	struct pmd_internals *internals = dev->data->dev_private;
	struct rte_eth_link link = {
		.link_speed = RTE_ETH_SPEED_NUM_10G,
		.link_duplex = RTE_ETH_LINK_FULL_DUPLEX,
		.link_autoneg = RTE_ETH_LINK_FIXED,
	};

	/*
	 * For pass-through mode (single_iface), query whether the
	 * underlying interface is up. Otherwise use default values.
	 */
	if (internals->single_iface) {
		link.link_status = (osdep_iface_link_status(internals->rx_queue[0].name) > 0) ?
			RTE_ETH_LINK_UP : RTE_ETH_LINK_DOWN;
	} else {
		link.link_status = dev->data->dev_started ?
			RTE_ETH_LINK_UP : RTE_ETH_LINK_DOWN;
	}

	return rte_eth_linkstatus_set(dev, &link);
}

static int
eth_rx_queue_setup(struct rte_eth_dev *dev,
		uint16_t rx_queue_id,
		uint16_t nb_rx_desc __rte_unused,
		unsigned int socket_id __rte_unused,
		const struct rte_eth_rxconf *rx_conf __rte_unused,
		struct rte_mempool *mb_pool)
{
	struct pmd_internals *internals = dev->data->dev_private;
	struct pcap_rx_queue *pcap_q = &internals->rx_queue[rx_queue_id];
	uint16_t buf_size;
	bool rx_scatter;

	buf_size = rte_pktmbuf_data_room_size(mb_pool) - RTE_PKTMBUF_HEADROOM;
	rx_scatter = !!(dev->data->dev_conf.rxmode.offloads &
			RTE_ETH_RX_OFFLOAD_SCATTER);

	/*
	 * If Rx scatter is not enabled, verify that the mbuf data room
	 * can hold the largest received packet in a single segment.
	 * Use the MTU-derived frame size as the expected maximum, not
	 * snapshot_len which is a capture truncation limit rather than
	 * an expected packet size.
	 */
	if (!rx_scatter) {
		uint32_t max_rx_pktlen = dev->data->mtu + RTE_ETHER_HDR_LEN;

		if (max_rx_pktlen > buf_size) {
			PMD_LOG(ERR,
				"Rx scatter is disabled and RxQ mbuf pool object size is too small "
				"(buf_size=%u, max_rx_pkt_len=%u)",
				buf_size, max_rx_pktlen);
			return -EINVAL;
		}
	}

	pcap_q->mb_pool = mb_pool;
	pcap_q->port_id = dev->data->port_id;
	pcap_q->queue_id = rx_queue_id;
	pcap_q->vlan_strip = internals->vlan_strip;
	pcap_q->rx_scatter = rx_scatter;
	dev->data->rx_queues[rx_queue_id] = pcap_q;
	pcap_q->timestamp_offloading = internals->timestamp_offloading;

	if (internals->infinite_rx) {
		struct pmd_process_private *pp;
		char ring_name[RTE_RING_NAMESIZE];
		static uint32_t ring_number;
		uint64_t pcap_pkt_count = 0;
		struct rte_mbuf *bufs[1];
		pcap_t **pcap;
		bool save_vlan_strip;

		if (rx_scatter) {
			PMD_LOG(ERR,
				"Rx scatter is not supported with infinite_rx mode");
			return -EINVAL;
		}

		pp = rte_eth_devices[pcap_q->port_id].process_private;
		pcap = &pp->rx_pcap[pcap_q->queue_id];

		if (unlikely(*pcap == NULL))
			return -ENOENT;

		pcap_pkt_count = count_packets_in_pcap(pcap, pcap_q);

		snprintf(ring_name, sizeof(ring_name), "PCAP_RING%" PRIu32,
				ring_number);

		pcap_q->pkts = rte_ring_create(ring_name,
				rte_align64pow2(pcap_pkt_count + 1), 0,
				RING_F_SP_ENQ | RING_F_SC_DEQ);
		ring_number++;
		if (!pcap_q->pkts)
			return -ENOENT;

		/*
		 * Temporarily disable offloads while filling the ring
		 * with raw packets. VLAN strip and timestamp will be
		 * applied later in eth_pcap_rx_infinite() on each copy.
		 */
		save_vlan_strip = pcap_q->vlan_strip;
		pcap_q->vlan_strip = false;

		/* Fill ring with packets from PCAP file one by one. */
		while (eth_pcap_rx(pcap_q, bufs, 1)) {
			/* Check for multiseg mbufs. */
			if (bufs[0]->nb_segs != 1) {
				infinite_rx_ring_free(pcap_q->pkts);
				pcap_q->vlan_strip = save_vlan_strip;
				PMD_LOG(ERR,
					"Multiseg mbufs are not supported in infinite_rx mode.");
				return -EINVAL;
			}

			rte_ring_enqueue_bulk(pcap_q->pkts,
					(void * const *)bufs, 1, NULL);
		}

		/* Restore offloads for use during packet delivery */
		pcap_q->vlan_strip = save_vlan_strip;

		if (rte_ring_count(pcap_q->pkts) < pcap_pkt_count) {
			infinite_rx_ring_free(pcap_q->pkts);
			PMD_LOG(ERR,
				"Not enough mbufs to accommodate packets in pcap file. "
				"At least %" PRIu64 " mbufs per queue is required.",
				pcap_pkt_count);
			return -EINVAL;
		}

		/*
		 * Reset the stats for this queue since eth_pcap_rx calls above
		 * didn't result in the application receiving packets.
		 */
		pcap_q->rx_stat.pkts = 0;
		pcap_q->rx_stat.bytes = 0;
	}

	return 0;
}

static int
eth_tx_queue_setup(struct rte_eth_dev *dev,
		uint16_t tx_queue_id,
		uint16_t nb_tx_desc __rte_unused,
		unsigned int socket_id,
		const struct rte_eth_txconf *tx_conf __rte_unused)
{
	struct pmd_internals *internals = dev->data->dev_private;
	struct pcap_tx_queue *pcap_q = &internals->tx_queue[tx_queue_id];

	pcap_q->port_id = dev->data->port_id;
	pcap_q->queue_id = tx_queue_id;
	pcap_q->bounce_buf = rte_malloc_socket(NULL, internals->snapshot_len,
					       RTE_CACHE_LINE_SIZE, socket_id);
	if (pcap_q->bounce_buf == NULL)
		return -ENOMEM;

	dev->data->tx_queues[tx_queue_id] = pcap_q;

	return 0;
}

static void
eth_tx_queue_release(struct rte_eth_dev *dev, uint16_t tx_queue_id)
{
	struct pmd_internals *internals = dev->data->dev_private;
	struct pcap_tx_queue *pcap_q = &internals->tx_queue[tx_queue_id];

	rte_free(pcap_q->bounce_buf);
	pcap_q->bounce_buf = NULL;
}

static int
eth_rx_queue_start(struct rte_eth_dev *dev, uint16_t rx_queue_id)
{
	dev->data->rx_queue_state[rx_queue_id] = RTE_ETH_QUEUE_STATE_STARTED;

	return 0;
}

static int
eth_tx_queue_start(struct rte_eth_dev *dev, uint16_t tx_queue_id)
{
	dev->data->tx_queue_state[tx_queue_id] = RTE_ETH_QUEUE_STATE_STARTED;

	return 0;
}

static int
eth_rx_queue_stop(struct rte_eth_dev *dev, uint16_t rx_queue_id)
{
	dev->data->rx_queue_state[rx_queue_id] = RTE_ETH_QUEUE_STATE_STOPPED;

	return 0;
}

static int
eth_tx_queue_stop(struct rte_eth_dev *dev, uint16_t tx_queue_id)
{
	dev->data->tx_queue_state[tx_queue_id] = RTE_ETH_QUEUE_STATE_STOPPED;

	return 0;
}

/* Timestamp values in receive packets from libpcap are in nanoseconds */
static int
eth_dev_read_clock(struct rte_eth_dev *dev __rte_unused, uint64_t *timestamp)
{
	struct timespec cur_time;

	timespec_get(&cur_time, TIME_UTC);
	*timestamp = rte_timespec_to_ns(&cur_time);
	return 0;
}

static int
eth_vlan_offload_set(struct rte_eth_dev *dev, int mask)
{
	struct pmd_internals *internals = dev->data->dev_private;
	unsigned int i;

	if (mask & RTE_ETH_VLAN_STRIP_MASK) {
		bool vlan_strip = !!(dev->data->dev_conf.rxmode.offloads &
				     RTE_ETH_RX_OFFLOAD_VLAN_STRIP);

		internals->vlan_strip = vlan_strip;

		/* Update all RX queues */
		for (i = 0; i < dev->data->nb_rx_queues; i++)
			internals->rx_queue[i].vlan_strip = vlan_strip;
	}

	return 0;
}

static const struct eth_dev_ops ops = {
	.dev_start = eth_dev_start,
	.dev_stop = eth_dev_stop,
	.dev_close = eth_dev_close,
	.dev_configure = eth_dev_configure,
	.dev_infos_get = eth_dev_info,
	.read_clock = eth_dev_read_clock,
	.rx_queue_setup = eth_rx_queue_setup,
	.tx_queue_setup = eth_tx_queue_setup,
	.tx_queue_release = eth_tx_queue_release,
	.rx_queue_start = eth_rx_queue_start,
	.tx_queue_start = eth_tx_queue_start,
	.rx_queue_stop = eth_rx_queue_stop,
	.tx_queue_stop = eth_tx_queue_stop,
	.link_update = eth_link_update,
	.stats_get = eth_stats_get,
	.stats_reset = eth_stats_reset,
	.vlan_offload_set = eth_vlan_offload_set,
};

static int
add_queue(struct pmd_devargs *pmd, const char *name, const char *type,
		pcap_t *pcap, pcap_dumper_t *dumper)
{
	if (pmd->num_of_queue >= RTE_PMD_PCAP_MAX_QUEUES)
		return -1;
	if (pcap)
		pmd->queue[pmd->num_of_queue].pcap = pcap;
	if (dumper)
		pmd->queue[pmd->num_of_queue].dumper = dumper;
	pmd->queue[pmd->num_of_queue].name = name;
	pmd->queue[pmd->num_of_queue].type = type;
	pmd->num_of_queue++;
	return 0;
}

/*
 * Function handler that opens the pcap file for reading a stores a
 * reference of it for use it later on.
 */
static int
open_rx_pcap(const char *key, const char *value, void *extra_args)
{
	const char *pcap_filename = value;
	struct pmd_devargs *rx = extra_args;
	pcap_t *pcap = NULL;

	if (open_single_rx_pcap(pcap_filename, &pcap) < 0)
		return -1;

	if (add_queue(rx, pcap_filename, key, pcap, NULL) < 0) {
		pcap_close(pcap);
		return -1;
	}

	return 0;
}

/*
 * Opens a pcap file for writing and stores a reference to it
 * for use it later on.
 */
static int
open_tx_pcap(const char *key, const char *value, void *extra_args)
{
	const char *pcap_filename = value;
	struct pmd_devargs *dumpers = extra_args;
	pcap_dumper_t *dumper;

	if (open_single_tx_pcap(pcap_filename, &dumper,
				dumpers->snapshot_len) < 0)
		return -1;

	if (add_queue(dumpers, pcap_filename, key, NULL, dumper) < 0) {
		pcap_dump_close(dumper);
		return -1;
	}

	return 0;
}

/*
 * Opens an interface for reading and writing
 */
static inline int
open_rx_tx_iface(const char *key, const char *value, void *extra_args)
{
	const char *iface = value;
	struct pmd_devargs *tx = extra_args;
	pcap_t *pcap = NULL;

	if (open_single_iface(iface, &pcap, tx->snapshot_len) < 0)
		return -1;

	tx->queue[0].pcap = pcap;
	tx->queue[0].name = iface;
	tx->queue[0].type = key;

	return 0;
}

static inline int
open_iface(const char *key, const char *value, void *extra_args)
{
	const char *iface = value;
	struct pmd_devargs *pmd = extra_args;
	pcap_t *pcap = NULL;

	if (open_single_iface(iface, &pcap, pmd->snapshot_len) < 0)
		return -1;
	if (add_queue(pmd, iface, key, pcap, NULL) < 0) {
		pcap_close(pcap);
		return -1;
	}

	return 0;
}

/*
 * Opens a NIC for reading packets from it
 */
static inline int
open_rx_iface(const char *key, const char *value, void *extra_args)
{
	int ret = open_iface(key, value, extra_args);
	if (ret < 0)
		return ret;
	if (strcmp(key, ETH_PCAP_RX_IFACE_IN_ARG) == 0) {
		struct pmd_devargs *pmd = extra_args;
		unsigned int qid = pmd->num_of_queue - 1;

		set_iface_direction(pmd->queue[qid].name,
				pmd->queue[qid].pcap,
				PCAP_D_IN);
	}

	return 0;
}

static inline int
rx_iface_args_process(const char *key, const char *value, void *extra_args)
{
	if (strcmp(key, ETH_PCAP_RX_IFACE_ARG) == 0 ||
			strcmp(key, ETH_PCAP_RX_IFACE_IN_ARG) == 0)
		return open_rx_iface(key, value, extra_args);

	return 0;
}

/*
 * Opens a NIC for writing packets to it
 */
static int
open_tx_iface(const char *key, const char *value, void *extra_args)
{
	return open_iface(key, value, extra_args);
}

static int
process_bool_flag(const char *key, const char *value, void *extra_args)
{
	bool *flag = extra_args;

	if (value == NULL || *value == '\0') {
		*flag = true; /* default with no additional argument */
	} else if (strcmp(value, "0") == 0) {
		*flag = false;
	} else if (strcmp(value, "1") == 0) {
		*flag = true;
	} else {
		PMD_LOG(ERR, "Invalid '%s' value '%s'", key, value);
		return -1;
	}
	return 0;
}

static int
process_snapshot_len(const char *key, const char *value, void *extra_args)
{
	uint32_t *snaplen = extra_args;
	unsigned long val;
	char *endptr;

	if (value == NULL || *value == '\0') {
		PMD_LOG(ERR, "Argument '%s' requires a value", key);
		return -1;
	}

	errno = 0;
	val = strtoul(value, &endptr, 10);
	if (errno != 0 || *endptr != '\0' ||
	    val < RTE_ETHER_HDR_LEN ||
	    val > ETH_PCAP_MAXIMUM_SNAPLEN) {
		PMD_LOG(ERR, "Invalid '%s' value '%s'", key, value);
		return -1;
	}

	*snaplen = (uint32_t)val;
	return 0;
}

static int
pmd_init_internals(struct rte_vdev_device *vdev,
		const unsigned int nb_rx_queues,
		const unsigned int nb_tx_queues,
		struct pmd_internals **internals,
		struct rte_eth_dev **eth_dev)
{
	struct rte_eth_dev_data *data;
	struct pmd_process_private *pp;
	unsigned int numa_node = vdev->device.numa_node;

	PMD_LOG(INFO, "Creating pcap-backed ethdev on numa socket %u",
		numa_node);

	pp = rte_zmalloc_socket("pcap_private", sizeof(struct pmd_process_private),
		RTE_CACHE_LINE_SIZE, numa_node);
	if (pp == NULL) {
		PMD_LOG(ERR,
			"Failed to allocate memory for process private");
		return -1;
	}

	/* reserve an ethdev entry */
	*eth_dev = rte_eth_vdev_allocate(vdev, sizeof(**internals));
	if (!(*eth_dev)) {
		rte_free(pp);
		return -1;
	}
	(*eth_dev)->process_private = pp;
	/* now put it all together
	 * - store queue data in internals,
	 * - store numa_node info in eth_dev
	 * - point eth_dev_data to internals
	 * - and point eth_dev structure to new eth_dev_data structure
	 */
	*internals = (*eth_dev)->data->dev_private;
	/*
	 * Interface MAC = 02:70:63:61:70:<iface_idx>
	 * derived from: 'locally administered':'p':'c':'a':'p':'iface_idx'
	 * where the middle 4 characters are converted to hex.
	 */
	static uint8_t iface_idx;
	(*internals)->eth_addr = (struct rte_ether_addr) {
		.addr_bytes = { 0x02, 0x70, 0x63, 0x61, 0x70, iface_idx++ }
	};
	(*internals)->phy_mac = 0;
	data = (*eth_dev)->data;
	data->nb_rx_queues = (uint16_t)nb_rx_queues;
	data->nb_tx_queues = (uint16_t)nb_tx_queues;
	data->dev_link = (struct rte_eth_link) {
		.link_speed = RTE_ETH_SPEED_NUM_NONE,
		.link_duplex = RTE_ETH_LINK_FULL_DUPLEX,
		.link_status = RTE_ETH_LINK_DOWN,
		.link_autoneg = RTE_ETH_LINK_FIXED,
	};
	data->mac_addrs = &(*internals)->eth_addr;
	data->promiscuous = 1;
	data->all_multicast = 1;
	data->dev_flags |= RTE_ETH_DEV_AUTOFILL_QUEUE_XSTATS;

	/*
	 * NOTE: we'll replace the data element, of originally allocated
	 * eth_dev so the rings are local per-process
	 */
	(*eth_dev)->dev_ops = &ops;

	strlcpy((*internals)->devargs, rte_vdev_device_args(vdev),
			ETH_PCAP_ARG_MAXLEN);

	return 0;
}

static int
eth_pcap_update_mac(const char *if_name, struct rte_eth_dev *eth_dev,
		const unsigned int numa_node)
{
	void *mac_addrs;
	struct rte_ether_addr mac;

	if (osdep_iface_mac_get(if_name, &mac) < 0)
		return -1;

	mac_addrs = rte_zmalloc_socket("pcap_mac", RTE_ETHER_ADDR_LEN, 0, numa_node);
	if (mac_addrs == NULL)
		return -1;

	PMD_LOG(INFO, "Setting phy MAC for %s", if_name);
	memcpy(mac_addrs, mac.addr_bytes, RTE_ETHER_ADDR_LEN);
	eth_dev->data->mac_addrs = mac_addrs;
	return 0;
}

static int
eth_from_pcaps_common(struct rte_vdev_device *vdev,
		struct pmd_devargs_all *devargs_all,
		struct pmd_internals **internals, struct rte_eth_dev **eth_dev)
{
	struct pmd_process_private *pp;
	struct pmd_devargs *rx_queues = &devargs_all->rx_queues;
	struct pmd_devargs *tx_queues = &devargs_all->tx_queues;
	const unsigned int nb_rx_queues = rx_queues->num_of_queue;
	const unsigned int nb_tx_queues = tx_queues->num_of_queue;
	unsigned int i;

	if (pmd_init_internals(vdev, nb_rx_queues, nb_tx_queues, internals,
			eth_dev) < 0)
		return -1;

	pp = (*eth_dev)->process_private;
	for (i = 0; i < nb_rx_queues; i++) {
		struct pcap_rx_queue *rx = &(*internals)->rx_queue[i];
		struct devargs_queue *queue = &rx_queues->queue[i];

		pp->rx_pcap[i] = queue->pcap;
		strlcpy(rx->name, queue->name, sizeof(rx->name));
		strlcpy(rx->type, queue->type, sizeof(rx->type));
	}

	for (i = 0; i < nb_tx_queues; i++) {
		struct pcap_tx_queue *tx = &(*internals)->tx_queue[i];
		struct devargs_queue *queue = &tx_queues->queue[i];

		pp->tx_dumper[i] = queue->dumper;
		pp->tx_pcap[i] = queue->pcap;
		strlcpy(tx->name, queue->name, sizeof(tx->name));
		strlcpy(tx->type, queue->type, sizeof(tx->type));
	}

	return 0;
}

static int
eth_from_pcaps(struct rte_vdev_device *vdev,
		struct pmd_devargs_all *devargs_all)
{
	struct pmd_internals *internals = NULL;
	struct rte_eth_dev *eth_dev = NULL;
	struct pmd_devargs *rx_queues = &devargs_all->rx_queues;
	int single_iface = devargs_all->single_iface;
	unsigned int infinite_rx = devargs_all->infinite_rx;
	int ret;

	ret = eth_from_pcaps_common(vdev, devargs_all, &internals, &eth_dev);

	if (ret < 0)
		return ret;

	/* store weather we are using a single interface for rx/tx or not */
	internals->single_iface = single_iface;

	if (single_iface) {
		internals->if_index =
			osdep_iface_index_get(rx_queues->queue[0].name);

		/* phy_mac arg is applied only if "iface" devarg is provided */
		if (rx_queues->phy_mac) {
			if (eth_pcap_update_mac(rx_queues->queue[0].name,
					eth_dev, vdev->device.numa_node) == 0)
				internals->phy_mac = 1;
		}
	}

	internals->infinite_rx = infinite_rx;
	internals->snapshot_len = devargs_all->snapshot_len;

	/* Assign rx ops. */
	if (infinite_rx)
		eth_dev->rx_pkt_burst = eth_pcap_rx_infinite;
	else if (devargs_all->is_rx_pcap || devargs_all->is_rx_iface ||
			single_iface)
		eth_dev->rx_pkt_burst = eth_pcap_rx;
	else
		eth_dev->rx_pkt_burst = eth_null_rx;

	/* Assign tx ops. */
	if (devargs_all->is_tx_pcap)
		eth_dev->tx_pkt_burst = eth_pcap_tx_dumper;
	else if (devargs_all->is_tx_iface || single_iface)
		eth_dev->tx_pkt_burst = eth_pcap_tx;
	else
		eth_dev->tx_pkt_burst = eth_tx_drop;

	rte_eth_dev_probing_finish(eth_dev);
	return 0;
}

static void
eth_release_pcaps(struct pmd_devargs *pcaps,
		struct pmd_devargs *dumpers,
		int single_iface)
{
	unsigned int i;

	if (single_iface) {
		if (pcaps->queue[0].pcap)
			pcap_close(pcaps->queue[0].pcap);
		return;
	}

	for (i = 0; i < dumpers->num_of_queue; i++) {
		if (dumpers->queue[i].dumper)
			pcap_dump_close(dumpers->queue[i].dumper);

		if (dumpers->queue[i].pcap)
			pcap_close(dumpers->queue[i].pcap);
	}

	for (i = 0; i < pcaps->num_of_queue; i++) {
		if (pcaps->queue[i].pcap)
			pcap_close(pcaps->queue[i].pcap);
	}
}

static int
pmd_pcap_probe(struct rte_vdev_device *dev)
{
	const char *name;
	struct rte_kvargs *kvlist;
	struct pmd_devargs pcaps = {0};
	struct pmd_devargs dumpers = {0};
	struct rte_eth_dev *eth_dev =  NULL;
	struct pmd_internals *internal;
	int ret = 0;

	struct pmd_devargs_all devargs_all = {
		.snapshot_len = ETH_PCAP_SNAPSHOT_LEN_DEFAULT,
		.single_iface = 0,
		.is_tx_pcap = 0,
		.is_tx_iface = 0,
		.infinite_rx = 0,
	};

	name = rte_vdev_device_name(dev);
	PMD_LOG(INFO, "Initializing pmd_pcap for %s", name);

	/* Record info for timestamps on first probe */
	if (hz == 0) {
		hz = rte_get_timer_hz();
		if (hz == 0) {
			PMD_LOG(ERR, "Reported hz is zero!");
			return -1;
		}
		timespec_get(&start_time, TIME_UTC);
		start_cycles = rte_get_timer_cycles();
	}

	if (rte_eal_process_type() == RTE_PROC_SECONDARY) {
		eth_dev = rte_eth_dev_attach_secondary(name);
		if (!eth_dev) {
			PMD_LOG(ERR, "Failed to probe %s", name);
			return -1;
		}

		internal = eth_dev->data->dev_private;

		kvlist = rte_kvargs_parse(internal->devargs, valid_arguments);
		if (kvlist == NULL)
			return -1;
	} else {
		kvlist = rte_kvargs_parse(rte_vdev_device_args(dev),
				valid_arguments);
		if (kvlist == NULL)
			return -1;
	}

	/*
	 * Process optional snapshot length argument first, so the value
	 * is available when opening pcap handles for files and interfaces.
	 */
	if (rte_kvargs_count(kvlist, ETH_PCAP_SNAPSHOT_LEN_ARG) == 1) {
		ret = rte_kvargs_process(kvlist, ETH_PCAP_SNAPSHOT_LEN_ARG,
					 &process_snapshot_len,
					 &devargs_all.snapshot_len);
		if (ret < 0)
			goto free_kvlist;
	}

	/*
	 * Propagate snapshot length to per-queue devargs so that
	 * the open callbacks can access it.
	 */
	devargs_all.rx_queues.snapshot_len = devargs_all.snapshot_len;
	devargs_all.tx_queues.snapshot_len = devargs_all.snapshot_len;

	/*
	 * If iface argument is passed we open the NICs and use them for
	 * reading / writing
	 */
	if (rte_kvargs_count(kvlist, ETH_PCAP_IFACE_ARG) == 1) {

		ret = rte_kvargs_process(kvlist, ETH_PCAP_IFACE_ARG,
				&open_rx_tx_iface, &pcaps);
		if (ret < 0)
			goto free_kvlist;

		dumpers.queue[0] = pcaps.queue[0];

		ret = rte_kvargs_process(kvlist, ETH_PCAP_PHY_MAC_ARG,
					 &process_bool_flag, &pcaps.phy_mac);
		if (ret < 0)
			goto free_kvlist;

		dumpers.phy_mac = pcaps.phy_mac;

		devargs_all.single_iface = 1;
		pcaps.num_of_queue = 1;
		dumpers.num_of_queue = 1;

		goto create_eth;
	}

	/*
	 * We check whether we want to open a RX stream from a real NIC, a
	 * pcap file or open a dummy RX stream
	 */
	devargs_all.is_rx_pcap =
		rte_kvargs_count(kvlist, ETH_PCAP_RX_PCAP_ARG) ? 1 : 0;
	devargs_all.is_rx_iface =
		(rte_kvargs_count(kvlist, ETH_PCAP_RX_IFACE_ARG) +
		 rte_kvargs_count(kvlist, ETH_PCAP_RX_IFACE_IN_ARG)) ? 1 : 0;
	pcaps.num_of_queue = 0;

	devargs_all.is_tx_pcap =
		rte_kvargs_count(kvlist, ETH_PCAP_TX_PCAP_ARG) ? 1 : 0;
	devargs_all.is_tx_iface =
		rte_kvargs_count(kvlist, ETH_PCAP_TX_IFACE_ARG) ? 1 : 0;
	dumpers.num_of_queue = 0;

	if (devargs_all.is_rx_pcap) {
		/*
		 * We check whether we want to infinitely rx the pcap file.
		 */
		unsigned int infinite_rx_arg_cnt = rte_kvargs_count(kvlist,
				ETH_PCAP_INFINITE_RX_ARG);

		if (infinite_rx_arg_cnt == 1) {
			ret = rte_kvargs_process(kvlist,
					ETH_PCAP_INFINITE_RX_ARG,
					 &process_bool_flag,
					 &devargs_all.infinite_rx);
			if (ret < 0)
				goto free_kvlist;
			PMD_LOG(INFO, "infinite_rx has been %s for %s",
					devargs_all.infinite_rx ? "enabled" : "disabled",
					name);

		} else if (infinite_rx_arg_cnt > 1) {
			PMD_LOG(WARNING, "infinite_rx has not been enabled since the "
					"argument has been provided more than once "
					"for %s", name);
		}

		ret = rte_kvargs_process(kvlist, ETH_PCAP_RX_PCAP_ARG,
				&open_rx_pcap, &pcaps);
	} else if (devargs_all.is_rx_iface) {
		ret = rte_kvargs_process(kvlist, NULL,
				&rx_iface_args_process, &pcaps);
	} else if (devargs_all.is_tx_iface || devargs_all.is_tx_pcap) {
		unsigned int i;

		/* Count number of tx queue args passed before dummy rx queue
		 * creation so a dummy rx queue can be created for each tx queue
		 */
		unsigned int num_tx_queues =
			(rte_kvargs_count(kvlist, ETH_PCAP_TX_PCAP_ARG) +
			rte_kvargs_count(kvlist, ETH_PCAP_TX_IFACE_ARG));

		PMD_LOG(INFO, "Creating null rx queue since no rx queues were provided.");

		/* Creating a dummy rx queue for each tx queue passed */
		for (i = 0; i < num_tx_queues; i++)
			ret = add_queue(&pcaps, "dummy_rx", "rx_null", NULL,
					NULL);
	} else {
		PMD_LOG(ERR, "Error - No rx or tx queues provided");
		ret = -ENOENT;
	}
	if (ret < 0)
		goto free_kvlist;

	/*
	 * We check whether we want to open a TX stream to a real NIC,
	 * a pcap file, or drop packets on tx
	 */
	if (devargs_all.is_tx_pcap) {
		ret = rte_kvargs_process(kvlist, ETH_PCAP_TX_PCAP_ARG,
				&open_tx_pcap, &dumpers);
	} else if (devargs_all.is_tx_iface) {
		ret = rte_kvargs_process(kvlist, ETH_PCAP_TX_IFACE_ARG,
				&open_tx_iface, &dumpers);
	} else {
		unsigned int i;

		PMD_LOG(INFO, "Dropping packets on tx since no tx queues were provided.");

		/* Add 1 dummy queue per rxq which counts and drops packets. */
		for (i = 0; i < pcaps.num_of_queue; i++)
			ret = add_queue(&dumpers, "dummy_tx", "tx_drop", NULL,
					NULL);
	}

	if (ret < 0)
		goto free_kvlist;

create_eth:
	if (rte_eal_process_type() == RTE_PROC_SECONDARY) {
		struct pmd_process_private *pp;
		unsigned int i;

		internal = eth_dev->data->dev_private;
		pp = rte_zmalloc("pcap_private", sizeof(struct pmd_process_private),
			 RTE_CACHE_LINE_SIZE);
		if (pp == NULL) {
			PMD_LOG(ERR,
				"Failed to allocate memory for process private");
			ret = -1;
			goto free_kvlist;
		}

		eth_dev->dev_ops = &ops;
		eth_dev->device = &dev->device;

		/* setup process private */
		for (i = 0; i < pcaps.num_of_queue; i++)
			pp->rx_pcap[i] = pcaps.queue[i].pcap;

		for (i = 0; i < dumpers.num_of_queue; i++) {
			pp->tx_dumper[i] = dumpers.queue[i].dumper;
			pp->tx_pcap[i] = dumpers.queue[i].pcap;
		}

		eth_dev->process_private = pp;
		eth_dev->rx_pkt_burst = eth_pcap_rx;
		if (devargs_all.is_tx_pcap)
			eth_dev->tx_pkt_burst = eth_pcap_tx_dumper;
		else
			eth_dev->tx_pkt_burst = eth_pcap_tx;

		rte_eth_dev_probing_finish(eth_dev);
		goto free_kvlist;
	}

	devargs_all.rx_queues = pcaps;
	devargs_all.tx_queues = dumpers;

	ret = eth_from_pcaps(dev, &devargs_all);

free_kvlist:
	rte_kvargs_free(kvlist);

	if (ret < 0)
		eth_release_pcaps(&pcaps, &dumpers, devargs_all.single_iface);

	return ret;
}

static int
pmd_pcap_remove(struct rte_vdev_device *dev)
{
	struct rte_eth_dev *eth_dev = NULL;

	if (!dev)
		return -1;

	eth_dev = rte_eth_dev_allocated(rte_vdev_device_name(dev));
	if (eth_dev == NULL)
		return 0; /* port already released */

	eth_dev_close(eth_dev);
	rte_eth_dev_release_port(eth_dev);

	return 0;
}

static struct rte_vdev_driver pmd_pcap_drv = {
	.probe = pmd_pcap_probe,
	.remove = pmd_pcap_remove,
};

RTE_PMD_REGISTER_VDEV(net_pcap, pmd_pcap_drv);
RTE_PMD_REGISTER_ALIAS(net_pcap, eth_pcap);
RTE_PMD_REGISTER_PARAM_STRING(net_pcap,
	ETH_PCAP_RX_PCAP_ARG "=<string> "
	ETH_PCAP_TX_PCAP_ARG "=<string> "
	ETH_PCAP_RX_IFACE_ARG "=<ifc> "
	ETH_PCAP_RX_IFACE_IN_ARG "=<ifc> "
	ETH_PCAP_TX_IFACE_ARG "=<ifc> "
	ETH_PCAP_IFACE_ARG "=<ifc> "
	ETH_PCAP_PHY_MAC_ARG "=<0|1> "
	ETH_PCAP_INFINITE_RX_ARG "=<0|1> "
	ETH_PCAP_SNAPSHOT_LEN_ARG "=<int>");
