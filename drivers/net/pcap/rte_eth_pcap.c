/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2015 Intel Corporation. All rights reserved.
 *   Copyright(c) 2014 6WIND S.A.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <time.h>
#include <rte_mbuf.h>
#include <rte_ethdev.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>
#include <rte_string_fns.h>
#include <rte_cycles.h>
#include <rte_kvargs.h>
#include <rte_dev.h>

#include <net/if.h>

#include <pcap.h>

#define RTE_ETH_PCAP_SNAPSHOT_LEN 65535
#define RTE_ETH_PCAP_SNAPLEN ETHER_MAX_JUMBO_FRAME_LEN
#define RTE_ETH_PCAP_PROMISC 1
#define RTE_ETH_PCAP_TIMEOUT -1
#define ETH_PCAP_RX_PCAP_ARG  "rx_pcap"
#define ETH_PCAP_TX_PCAP_ARG  "tx_pcap"
#define ETH_PCAP_RX_IFACE_ARG "rx_iface"
#define ETH_PCAP_TX_IFACE_ARG "tx_iface"
#define ETH_PCAP_IFACE_ARG    "iface"

#define ETH_PCAP_ARG_MAXLEN	64

static char errbuf[PCAP_ERRBUF_SIZE];
static unsigned char tx_pcap_data[RTE_ETH_PCAP_SNAPLEN];
static struct timeval start_time;
static uint64_t start_cycles;
static uint64_t hz;

struct pcap_rx_queue {
	pcap_t *pcap;
	uint8_t in_port;
	struct rte_mempool *mb_pool;
	volatile unsigned long rx_pkts;
	volatile unsigned long rx_bytes;
	volatile unsigned long err_pkts;
	char name[PATH_MAX];
	char type[ETH_PCAP_ARG_MAXLEN];
};

struct pcap_tx_queue {
	pcap_dumper_t *dumper;
	pcap_t *pcap;
	volatile unsigned long tx_pkts;
	volatile unsigned long tx_bytes;
	volatile unsigned long err_pkts;
	char name[PATH_MAX];
	char type[ETH_PCAP_ARG_MAXLEN];
};

struct rx_pcaps {
	unsigned num_of_rx;
	pcap_t *pcaps[RTE_PMD_RING_MAX_RX_RINGS];
	const char *names[RTE_PMD_RING_MAX_RX_RINGS];
	const char *types[RTE_PMD_RING_MAX_RX_RINGS];
};

struct tx_pcaps {
	unsigned num_of_tx;
	pcap_dumper_t *dumpers[RTE_PMD_RING_MAX_TX_RINGS];
	pcap_t *pcaps[RTE_PMD_RING_MAX_RX_RINGS];
	const char *names[RTE_PMD_RING_MAX_RX_RINGS];
	const char *types[RTE_PMD_RING_MAX_RX_RINGS];
};

struct pmd_internals {
	struct pcap_rx_queue rx_queue[RTE_PMD_RING_MAX_RX_RINGS];
	struct pcap_tx_queue tx_queue[RTE_PMD_RING_MAX_TX_RINGS];
	int if_index;
	int single_iface;
};

const char *valid_arguments[] = {
	ETH_PCAP_RX_PCAP_ARG,
	ETH_PCAP_TX_PCAP_ARG,
	ETH_PCAP_RX_IFACE_ARG,
	ETH_PCAP_TX_IFACE_ARG,
	ETH_PCAP_IFACE_ARG,
	NULL
};

static int open_single_tx_pcap(const char *pcap_filename, pcap_dumper_t **dumper);
static int open_single_rx_pcap(const char *pcap_filename, pcap_t **pcap);
static int open_single_iface(const char *iface, pcap_t **pcap);

static struct ether_addr eth_addr = { .addr_bytes = { 0, 0, 0, 0x1, 0x2, 0x3 } };
static const char *drivername = "Pcap PMD";
static struct rte_eth_link pmd_link = {
		.link_speed = ETH_SPEED_NUM_10G,
		.link_duplex = ETH_LINK_FULL_DUPLEX,
		.link_status = ETH_LINK_DOWN,
		.link_autoneg = ETH_LINK_SPEED_FIXED,
};

static int
eth_pcap_rx_jumbo(struct rte_mempool *mb_pool,
		  struct rte_mbuf *mbuf,
		  const u_char *data,
		  uint16_t data_len)
{
	struct rte_mbuf *m = mbuf;

	/* Copy the first segment. */
	uint16_t len = rte_pktmbuf_tailroom(mbuf);

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

/* Copy data from mbuf chain to a buffer suitable for writing to a PCAP file. */
static void
eth_pcap_gather_data(unsigned char *data, struct rte_mbuf *mbuf)
{
	uint16_t data_len = 0;

	while (mbuf) {
		rte_memcpy(data + data_len, rte_pktmbuf_mtod(mbuf, void *),
			   mbuf->data_len);

		data_len += mbuf->data_len;
		mbuf = mbuf->next;
	}
}

static uint16_t
eth_pcap_rx(void *queue,
		struct rte_mbuf **bufs,
		uint16_t nb_pkts)
{
	unsigned i;
	struct pcap_pkthdr header;
	const u_char *packet;
	struct rte_mbuf *mbuf;
	struct pcap_rx_queue *pcap_q = queue;
	uint16_t num_rx = 0;
	uint16_t buf_size;
	uint32_t rx_bytes = 0;

	if (unlikely(pcap_q->pcap == NULL || nb_pkts == 0))
		return 0;

	/* Reads the given number of packets from the pcap file one by one
	 * and copies the packet data into a newly allocated mbuf to return.
	 */
	for (i = 0; i < nb_pkts; i++) {
		/* Get the next PCAP packet */
		packet = pcap_next(pcap_q->pcap, &header);
		if (unlikely(packet == NULL))
			break;
		else
			mbuf = rte_pktmbuf_alloc(pcap_q->mb_pool);
		if (unlikely(mbuf == NULL))
			break;

		/* Now get the space available for data in the mbuf */
		buf_size = (uint16_t)(rte_pktmbuf_data_room_size(pcap_q->mb_pool) -
				RTE_PKTMBUF_HEADROOM);

		if (header.caplen <= buf_size) {
			/* pcap packet will fit in the mbuf, go ahead and copy */
			rte_memcpy(rte_pktmbuf_mtod(mbuf, void *), packet,
					header.caplen);
			mbuf->data_len = (uint16_t)header.caplen;
		} else {
			/* Try read jumbo frame into multi mbufs. */
			if (unlikely(eth_pcap_rx_jumbo(pcap_q->mb_pool,
						       mbuf,
						       packet,
						       header.caplen) == -1))
				break;
		}

		mbuf->pkt_len = (uint16_t)header.caplen;
		mbuf->port = pcap_q->in_port;
		bufs[num_rx] = mbuf;
		num_rx++;
		rx_bytes += header.caplen;
	}
	pcap_q->rx_pkts += num_rx;
	pcap_q->rx_bytes += rx_bytes;
	return num_rx;
}

static inline void
calculate_timestamp(struct timeval *ts) {
	uint64_t cycles;
	struct timeval cur_time;

	cycles = rte_get_timer_cycles() - start_cycles;
	cur_time.tv_sec = cycles / hz;
	cur_time.tv_usec = (cycles % hz) * 10e6 / hz;
	timeradd(&start_time, &cur_time, ts);
}

/*
 * Callback to handle writing packets to a pcap file.
 */
static uint16_t
eth_pcap_tx_dumper(void *queue,
		struct rte_mbuf **bufs,
		uint16_t nb_pkts)
{
	unsigned i;
	struct rte_mbuf *mbuf;
	struct pcap_tx_queue *dumper_q = queue;
	uint16_t num_tx = 0;
	uint32_t tx_bytes = 0;
	struct pcap_pkthdr header;

	if (dumper_q->dumper == NULL || nb_pkts == 0)
		return 0;

	/* writes the nb_pkts packets to the previously opened pcap file dumper */
	for (i = 0; i < nb_pkts; i++) {
		mbuf = bufs[i];
		calculate_timestamp(&header.ts);
		header.len = mbuf->pkt_len;
		header.caplen = header.len;

		if (likely(mbuf->nb_segs == 1)) {
			pcap_dump((u_char *)dumper_q->dumper, &header,
				  rte_pktmbuf_mtod(mbuf, void*));
		} else {
			if (mbuf->pkt_len <= ETHER_MAX_JUMBO_FRAME_LEN) {
				eth_pcap_gather_data(tx_pcap_data, mbuf);
				pcap_dump((u_char *)dumper_q->dumper, &header,
					  tx_pcap_data);
			} else {
				RTE_LOG(ERR, PMD,
					"Dropping PCAP packet. "
					"Size (%d) > max jumbo size (%d).\n",
					mbuf->pkt_len,
					ETHER_MAX_JUMBO_FRAME_LEN);

				rte_pktmbuf_free(mbuf);
				break;
			}
		}

		rte_pktmbuf_free(mbuf);
		num_tx++;
		tx_bytes += mbuf->pkt_len;
	}

	/*
	 * Since there's no place to hook a callback when the forwarding
	 * process stops and to make sure the pcap file is actually written,
	 * we flush the pcap dumper within each burst.
	 */
	pcap_dump_flush(dumper_q->dumper);
	dumper_q->tx_pkts += num_tx;
	dumper_q->tx_bytes += tx_bytes;
	dumper_q->err_pkts += nb_pkts - num_tx;
	return num_tx;
}

/*
 * Callback to handle sending packets through a real NIC.
 */
static uint16_t
eth_pcap_tx(void *queue,
		struct rte_mbuf **bufs,
		uint16_t nb_pkts)
{
	unsigned i;
	int ret;
	struct rte_mbuf *mbuf;
	struct pcap_tx_queue *tx_queue = queue;
	uint16_t num_tx = 0;
	uint32_t tx_bytes = 0;

	if (unlikely(nb_pkts == 0 || tx_queue->pcap == NULL))
		return 0;

	for (i = 0; i < nb_pkts; i++) {
		mbuf = bufs[i];

		if (likely(mbuf->nb_segs == 1)) {
			ret = pcap_sendpacket(tx_queue->pcap,
					      rte_pktmbuf_mtod(mbuf, u_char *),
					      mbuf->pkt_len);
		} else {
			if (mbuf->pkt_len <= ETHER_MAX_JUMBO_FRAME_LEN) {
				eth_pcap_gather_data(tx_pcap_data, mbuf);
				ret = pcap_sendpacket(tx_queue->pcap,
						      tx_pcap_data,
						      mbuf->pkt_len);
			} else {
				RTE_LOG(ERR, PMD,
					"Dropping PCAP packet. "
					"Size (%d) > max jumbo size (%d).\n",
					mbuf->pkt_len,
					ETHER_MAX_JUMBO_FRAME_LEN);

				rte_pktmbuf_free(mbuf);
				break;
			}
		}

		if (unlikely(ret != 0))
			break;
		num_tx++;
		tx_bytes += mbuf->pkt_len;
		rte_pktmbuf_free(mbuf);
	}

	tx_queue->tx_pkts += num_tx;
	tx_queue->tx_bytes += tx_bytes;
	tx_queue->err_pkts += nb_pkts - num_tx;
	return num_tx;
}

static int
eth_dev_start(struct rte_eth_dev *dev)
{
	unsigned i;
	struct pmd_internals *internals = dev->data->dev_private;
	struct pcap_tx_queue *tx;
	struct pcap_rx_queue *rx;

	/* Special iface case. Single pcap is open and shared between tx/rx. */
	if (internals->single_iface) {
		tx = &internals->tx_queue[0];
		rx = &internals->rx_queue[0];

		if (!tx->pcap && strcmp(tx->type, ETH_PCAP_IFACE_ARG) == 0) {
			if (open_single_iface(tx->name, &tx->pcap) < 0)
				return -1;
			rx->pcap = tx->pcap;
		}
		goto status_up;
	}

	/* If not open already, open tx pcaps/dumpers */
	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		tx = &internals->tx_queue[i];

		if (!tx->dumper && strcmp(tx->type, ETH_PCAP_TX_PCAP_ARG) == 0) {
			if (open_single_tx_pcap(tx->name, &tx->dumper) < 0)
				return -1;
		}

		else if (!tx->pcap && strcmp(tx->type, ETH_PCAP_TX_IFACE_ARG) == 0) {
			if (open_single_iface(tx->name, &tx->pcap) < 0)
				return -1;
		}
	}

	/* If not open already, open rx pcaps */
	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		rx = &internals->rx_queue[i];

		if (rx->pcap != NULL)
			continue;

		if (strcmp(rx->type, ETH_PCAP_RX_PCAP_ARG) == 0) {
			if (open_single_rx_pcap(rx->name, &rx->pcap) < 0)
				return -1;
		}

		else if (strcmp(rx->type, ETH_PCAP_RX_IFACE_ARG) == 0) {
			if (open_single_iface(rx->name, &rx->pcap) < 0)
				return -1;
		}
	}

status_up:

	dev->data->dev_link.link_status = ETH_LINK_UP;
	return 0;
}

/*
 * This function gets called when the current port gets stopped.
 * Is the only place for us to close all the tx streams dumpers.
 * If not called the dumpers will be flushed within each tx burst.
 */
static void
eth_dev_stop(struct rte_eth_dev *dev)
{
	unsigned i;
	struct pmd_internals *internals = dev->data->dev_private;
	struct pcap_tx_queue *tx;
	struct pcap_rx_queue *rx;

	/* Special iface case. Single pcap is open and shared between tx/rx. */
	if (internals->single_iface) {
		tx = &internals->tx_queue[0];
		rx = &internals->rx_queue[0];
		pcap_close(tx->pcap);
		tx->pcap = NULL;
		rx->pcap = NULL;
		goto status_down;
	}

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		tx = &internals->tx_queue[i];

		if (tx->dumper != NULL) {
			pcap_dump_close(tx->dumper);
			tx->dumper = NULL;
		}

		if (tx->pcap != NULL) {
			pcap_close(tx->pcap);
			tx->pcap = NULL;
		}
	}

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		rx = &internals->rx_queue[i];

		if (rx->pcap != NULL) {
			pcap_close(rx->pcap);
			rx->pcap = NULL;
		}
	}

status_down:
	dev->data->dev_link.link_status = ETH_LINK_DOWN;
}

static int
eth_dev_configure(struct rte_eth_dev *dev __rte_unused)
{
	return 0;
}

static void
eth_dev_info(struct rte_eth_dev *dev,
		struct rte_eth_dev_info *dev_info)
{
	struct pmd_internals *internals = dev->data->dev_private;
	dev_info->driver_name = drivername;
	dev_info->if_index = internals->if_index;
	dev_info->max_mac_addrs = 1;
	dev_info->max_rx_pktlen = (uint32_t) -1;
	dev_info->max_rx_queues = dev->data->nb_rx_queues;
	dev_info->max_tx_queues = dev->data->nb_tx_queues;
	dev_info->min_rx_bufsize = 0;
	dev_info->pci_dev = NULL;
}

static void
eth_stats_get(struct rte_eth_dev *dev,
		struct rte_eth_stats *igb_stats)
{
	unsigned i;
	unsigned long rx_packets_total = 0, rx_bytes_total = 0;
	unsigned long tx_packets_total = 0, tx_bytes_total = 0;
	unsigned long tx_packets_err_total = 0;
	const struct pmd_internals *internal = dev->data->dev_private;

	for (i = 0; i < RTE_ETHDEV_QUEUE_STAT_CNTRS &&
			i < dev->data->nb_rx_queues; i++) {
		igb_stats->q_ipackets[i] = internal->rx_queue[i].rx_pkts;
		igb_stats->q_ibytes[i] = internal->rx_queue[i].rx_bytes;
		rx_packets_total += igb_stats->q_ipackets[i];
		rx_bytes_total += igb_stats->q_ibytes[i];
	}

	for (i = 0; i < RTE_ETHDEV_QUEUE_STAT_CNTRS &&
			i < dev->data->nb_tx_queues; i++) {
		igb_stats->q_opackets[i] = internal->tx_queue[i].tx_pkts;
		igb_stats->q_obytes[i] = internal->tx_queue[i].tx_bytes;
		igb_stats->q_errors[i] = internal->tx_queue[i].err_pkts;
		tx_packets_total += igb_stats->q_opackets[i];
		tx_bytes_total += igb_stats->q_obytes[i];
		tx_packets_err_total += igb_stats->q_errors[i];
	}

	igb_stats->ipackets = rx_packets_total;
	igb_stats->ibytes = rx_bytes_total;
	igb_stats->opackets = tx_packets_total;
	igb_stats->obytes = tx_bytes_total;
	igb_stats->oerrors = tx_packets_err_total;
}

static void
eth_stats_reset(struct rte_eth_dev *dev)
{
	unsigned i;
	struct pmd_internals *internal = dev->data->dev_private;
	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		internal->rx_queue[i].rx_pkts = 0;
		internal->rx_queue[i].rx_bytes = 0;
	}
	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		internal->tx_queue[i].tx_pkts = 0;
		internal->tx_queue[i].tx_bytes = 0;
		internal->tx_queue[i].err_pkts = 0;
	}
}

static void
eth_dev_close(struct rte_eth_dev *dev __rte_unused)
{
}

static void
eth_queue_release(void *q __rte_unused)
{
}

static int
eth_link_update(struct rte_eth_dev *dev __rte_unused,
		int wait_to_complete __rte_unused)
{
	return 0;
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
	pcap_q->mb_pool = mb_pool;
	dev->data->rx_queues[rx_queue_id] = pcap_q;
	pcap_q->in_port = dev->data->port_id;
	return 0;
}

static int
eth_tx_queue_setup(struct rte_eth_dev *dev,
		uint16_t tx_queue_id,
		uint16_t nb_tx_desc __rte_unused,
		unsigned int socket_id __rte_unused,
		const struct rte_eth_txconf *tx_conf __rte_unused)
{

	struct pmd_internals *internals = dev->data->dev_private;
	dev->data->tx_queues[tx_queue_id] = &internals->tx_queue[tx_queue_id];
	return 0;
}

static const struct eth_dev_ops ops = {
	.dev_start = eth_dev_start,
	.dev_stop =	eth_dev_stop,
	.dev_close = eth_dev_close,
	.dev_configure = eth_dev_configure,
	.dev_infos_get = eth_dev_info,
	.rx_queue_setup = eth_rx_queue_setup,
	.tx_queue_setup = eth_tx_queue_setup,
	.rx_queue_release = eth_queue_release,
	.tx_queue_release = eth_queue_release,
	.link_update = eth_link_update,
	.stats_get = eth_stats_get,
	.stats_reset = eth_stats_reset,
};

/*
 * Function handler that opens the pcap file for reading a stores a
 * reference of it for use it later on.
 */
static int
open_rx_pcap(const char *key, const char *value, void *extra_args)
{
	unsigned i;
	const char *pcap_filename = value;
	struct rx_pcaps *pcaps = extra_args;
	pcap_t *pcap = NULL;

	for (i = 0; i < pcaps->num_of_rx; i++) {
		if (open_single_rx_pcap(pcap_filename, &pcap) < 0)
			return -1;

		pcaps->pcaps[i] = pcap;
		pcaps->names[i] = pcap_filename;
		pcaps->types[i] = key;
	}

	return 0;
}

static int
open_single_rx_pcap(const char *pcap_filename, pcap_t **pcap)
{
	if ((*pcap = pcap_open_offline(pcap_filename, errbuf)) == NULL) {
		RTE_LOG(ERR, PMD, "Couldn't open %s: %s\n", pcap_filename, errbuf);
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
	unsigned i;
	const char *pcap_filename = value;
	struct tx_pcaps *dumpers = extra_args;
	pcap_dumper_t *dumper;

	for (i = 0; i < dumpers->num_of_tx; i++) {
		if (open_single_tx_pcap(pcap_filename, &dumper) < 0)
			return -1;

		dumpers->dumpers[i] = dumper;
		dumpers->names[i] = pcap_filename;
		dumpers->types[i] = key;
	}

	return 0;
}

static int
open_single_tx_pcap(const char *pcap_filename, pcap_dumper_t **dumper)
{
	pcap_t *tx_pcap;
	/*
	 * We need to create a dummy empty pcap_t to use it
	 * with pcap_dump_open(). We create big enough an Ethernet
	 * pcap holder.
	 */

	if ((tx_pcap = pcap_open_dead(DLT_EN10MB, RTE_ETH_PCAP_SNAPSHOT_LEN))
			== NULL) {
		RTE_LOG(ERR, PMD, "Couldn't create dead pcap\n");
		return -1;
	}

	/* The dumper is created using the previous pcap_t reference */
	if ((*dumper = pcap_dump_open(tx_pcap, pcap_filename)) == NULL) {
		RTE_LOG(ERR, PMD, "Couldn't open %s for writing.\n", pcap_filename);
		return -1;
	}

	return 0;
}

/*
 * pcap_open_live wrapper function
 */
static inline int
open_iface_live(const char *iface, pcap_t **pcap) {
	*pcap = pcap_open_live(iface, RTE_ETH_PCAP_SNAPLEN,
			RTE_ETH_PCAP_PROMISC, RTE_ETH_PCAP_TIMEOUT, errbuf);

	if (*pcap == NULL) {
		RTE_LOG(ERR, PMD, "Couldn't open %s: %s\n", iface, errbuf);
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
	struct rx_pcaps *pcaps = extra_args;
	pcap_t *pcap = NULL;

	if (open_single_iface(iface, &pcap) < 0)
		return -1;

	pcaps->pcaps[0] = pcap;
	pcaps->names[0] = iface;
	pcaps->types[0] = key;

	return 0;
}

/*
 * Opens a NIC for reading packets from it
 */
static inline int
open_rx_iface(const char *key, const char *value, void *extra_args)
{
	unsigned i;
	const char *iface = value;
	struct rx_pcaps *pcaps = extra_args;
	pcap_t *pcap = NULL;

	for (i = 0; i < pcaps->num_of_rx; i++) {
		if (open_single_iface(iface, &pcap) < 0)
			return -1;
		pcaps->pcaps[i] = pcap;
		pcaps->names[i] = iface;
		pcaps->types[i] = key;
	}

	return 0;
}

/*
 * Opens a NIC for writing packets to it
 */
static int
open_tx_iface(const char *key, const char *value, void *extra_args)
{
	unsigned i;
	const char *iface = value;
	struct tx_pcaps *pcaps = extra_args;
	pcap_t *pcap;

	for (i = 0; i < pcaps->num_of_tx; i++) {
		if (open_single_iface(iface, &pcap) < 0)
			return -1;
		pcaps->pcaps[i] = pcap;
		pcaps->names[i] = iface;
		pcaps->types[i] = key;
	}

	return 0;
}

static int
open_single_iface(const char *iface, pcap_t **pcap)
{
	if (open_iface_live(iface, pcap) < 0) {
		RTE_LOG(ERR, PMD, "Couldn't open interface %s\n", iface);
		return -1;
	}

	return 0;
}

static int
rte_pmd_init_internals(const char *name, const unsigned nb_rx_queues,
		const unsigned nb_tx_queues,
		const unsigned numa_node,
		struct pmd_internals **internals,
		struct rte_eth_dev **eth_dev,
		struct rte_kvargs *kvlist)
{
	struct rte_eth_dev_data *data = NULL;
	unsigned k_idx;
	struct rte_kvargs_pair *pair = NULL;

	for (k_idx = 0; k_idx < kvlist->count; k_idx++) {
		pair = &kvlist->pairs[k_idx];
		if (strstr(pair->key, ETH_PCAP_IFACE_ARG) != NULL)
			break;
	}

	RTE_LOG(INFO, PMD,
			"Creating pcap-backed ethdev on numa socket %u\n", numa_node);

	/* now do all data allocation - for eth_dev structure
	 * and internal (private) data
	 */
	data = rte_zmalloc_socket(name, sizeof(*data), 0, numa_node);
	if (data == NULL)
		goto error;

	*internals = rte_zmalloc_socket(name, sizeof(**internals), 0, numa_node);
	if (*internals == NULL)
		goto error;

	/* reserve an ethdev entry */
	*eth_dev = rte_eth_dev_allocate(name, RTE_ETH_DEV_VIRTUAL);
	if (*eth_dev == NULL)
		goto error;

	/* check length of device name */
	if ((strlen((*eth_dev)->data->name) + 1) > sizeof(data->name))
		goto error;

	/* now put it all together
	 * - store queue data in internals,
	 * - store numa_node info in eth_dev
	 * - point eth_dev_data to internals
	 * - and point eth_dev structure to new eth_dev_data structure
	 */
	/* NOTE: we'll replace the data element, of originally allocated eth_dev
	 * so the rings are local per-process */

	if (pair == NULL)
		(*internals)->if_index = 0;
	else
		(*internals)->if_index = if_nametoindex(pair->value);

	data->dev_private = *internals;
	data->port_id = (*eth_dev)->data->port_id;
	snprintf(data->name, sizeof(data->name), "%s", (*eth_dev)->data->name);
	data->nb_rx_queues = (uint16_t)nb_rx_queues;
	data->nb_tx_queues = (uint16_t)nb_tx_queues;
	data->dev_link = pmd_link;
	data->mac_addrs = &eth_addr;
	strncpy(data->name,
		(*eth_dev)->data->name, strlen((*eth_dev)->data->name));

	(*eth_dev)->data = data;
	(*eth_dev)->dev_ops = &ops;
	(*eth_dev)->driver = NULL;
	data->dev_flags = RTE_ETH_DEV_DETACHABLE;
	data->kdrv = RTE_KDRV_NONE;
	data->drv_name = drivername;
	data->numa_node = numa_node;

	return 0;

error:
	rte_free(data);
	rte_free(*internals);

	return -1;
}

static int
rte_eth_from_pcaps_common(const char *name, struct rx_pcaps *rx_queues,
		const unsigned nb_rx_queues, struct tx_pcaps *tx_queues,
		const unsigned nb_tx_queues, const unsigned numa_node,
		struct rte_kvargs *kvlist, struct pmd_internals **internals,
		struct rte_eth_dev **eth_dev)
{
	unsigned i;

	/* do some parameter checking */
	if (rx_queues == NULL && nb_rx_queues > 0)
		return -1;
	if (tx_queues == NULL && nb_tx_queues > 0)
		return -1;

	if (rte_pmd_init_internals(name, nb_rx_queues, nb_tx_queues, numa_node,
			internals, eth_dev, kvlist) < 0)
		return -1;

	for (i = 0; i < nb_rx_queues; i++) {
		(*internals)->rx_queue[i].pcap = rx_queues->pcaps[i];
		snprintf((*internals)->rx_queue[i].name,
			sizeof((*internals)->rx_queue[i].name), "%s",
			rx_queues->names[i]);
		snprintf((*internals)->rx_queue[i].type,
			sizeof((*internals)->rx_queue[i].type), "%s",
			rx_queues->types[i]);
	}
	for (i = 0; i < nb_tx_queues; i++) {
		(*internals)->tx_queue[i].dumper = tx_queues->dumpers[i];
		snprintf((*internals)->tx_queue[i].name,
			sizeof((*internals)->tx_queue[i].name), "%s",
			tx_queues->names[i]);
		snprintf((*internals)->tx_queue[i].type,
			sizeof((*internals)->tx_queue[i].type), "%s",
			tx_queues->types[i]);
	}

	return 0;
}

static int
rte_eth_from_pcaps_n_dumpers(const char *name,
		struct rx_pcaps *rx_queues,
		const unsigned nb_rx_queues,
		struct tx_pcaps *tx_queues,
		const unsigned nb_tx_queues,
		const unsigned numa_node,
		struct rte_kvargs *kvlist)
{
	struct pmd_internals *internals = NULL;
	struct rte_eth_dev *eth_dev = NULL;
	int ret;

	ret = rte_eth_from_pcaps_common(name, rx_queues, nb_rx_queues,
			tx_queues, nb_tx_queues, numa_node, kvlist,
			&internals, &eth_dev);

	if (ret < 0)
		return ret;

	/* using multiple pcaps/interfaces */
	internals->single_iface = 0;

	eth_dev->rx_pkt_burst = eth_pcap_rx;
	eth_dev->tx_pkt_burst = eth_pcap_tx_dumper;

	return 0;
}

static int
rte_eth_from_pcaps(const char *name,
		struct rx_pcaps *rx_queues,
		const unsigned nb_rx_queues,
		struct tx_pcaps *tx_queues,
		const unsigned nb_tx_queues,
		const unsigned numa_node,
		struct rte_kvargs *kvlist,
		int single_iface)
{
	struct pmd_internals *internals = NULL;
	struct rte_eth_dev *eth_dev = NULL;
	int ret;

	ret = rte_eth_from_pcaps_common(name, rx_queues, nb_rx_queues,
			tx_queues, nb_tx_queues, numa_node, kvlist,
			&internals, &eth_dev);

	if (ret < 0)
		return ret;

	/* store wether we are using a single interface for rx/tx or not */
	internals->single_iface = single_iface;

	eth_dev->rx_pkt_burst = eth_pcap_rx;
	eth_dev->tx_pkt_burst = eth_pcap_tx;

	return 0;
}


static int
rte_pmd_pcap_devinit(const char *name, const char *params)
{
	unsigned numa_node, using_dumpers = 0;
	int ret;
	struct rte_kvargs *kvlist;
	struct rx_pcaps pcaps = {0};
	struct tx_pcaps dumpers = {0};

	RTE_LOG(INFO, PMD, "Initializing pmd_pcap for %s\n", name);

	numa_node = rte_socket_id();

	gettimeofday(&start_time, NULL);
	start_cycles = rte_get_timer_cycles();
	hz = rte_get_timer_hz();

	kvlist = rte_kvargs_parse(params, valid_arguments);
	if (kvlist == NULL)
		return -1;

	/*
	 * If iface argument is passed we open the NICs and use them for
	 * reading / writing
	 */
	if (rte_kvargs_count(kvlist, ETH_PCAP_IFACE_ARG) == 1) {

		ret = rte_kvargs_process(kvlist, ETH_PCAP_IFACE_ARG,
				&open_rx_tx_iface, &pcaps);
		if (ret < 0)
			goto free_kvlist;
		dumpers.pcaps[0] = pcaps.pcaps[0];
		dumpers.names[0] = pcaps.names[0];
		dumpers.types[0] = pcaps.types[0];
		ret = rte_eth_from_pcaps(name, &pcaps, 1, &dumpers, 1,
				numa_node, kvlist, 1);
		goto free_kvlist;
	}

	/*
	 * We check whether we want to open a RX stream from a real NIC or a
	 * pcap file
	 */
	if ((pcaps.num_of_rx = rte_kvargs_count(kvlist, ETH_PCAP_RX_PCAP_ARG))) {
		ret = rte_kvargs_process(kvlist, ETH_PCAP_RX_PCAP_ARG,
				&open_rx_pcap, &pcaps);
	} else {
		pcaps.num_of_rx = rte_kvargs_count(kvlist,
				ETH_PCAP_RX_IFACE_ARG);
		ret = rte_kvargs_process(kvlist, ETH_PCAP_RX_IFACE_ARG,
				&open_rx_iface, &pcaps);
	}

	if (ret < 0)
		goto free_kvlist;

	/*
	 * We check whether we want to open a TX stream to a real NIC or a
	 * pcap file
	 */
	if ((dumpers.num_of_tx = rte_kvargs_count(kvlist,
			ETH_PCAP_TX_PCAP_ARG))) {
		ret = rte_kvargs_process(kvlist, ETH_PCAP_TX_PCAP_ARG,
				&open_tx_pcap, &dumpers);
		using_dumpers = 1;
	} else {
		dumpers.num_of_tx = rte_kvargs_count(kvlist,
				ETH_PCAP_TX_IFACE_ARG);
		ret = rte_kvargs_process(kvlist, ETH_PCAP_TX_IFACE_ARG,
				&open_tx_iface, &dumpers);
	}

	if (ret < 0)
		goto free_kvlist;

	if (using_dumpers)
		ret = rte_eth_from_pcaps_n_dumpers(name, &pcaps, pcaps.num_of_rx,
				&dumpers, dumpers.num_of_tx, numa_node, kvlist);
	else
		ret = rte_eth_from_pcaps(name, &pcaps, pcaps.num_of_rx, &dumpers,
			dumpers.num_of_tx, numa_node, kvlist, 0);

free_kvlist:
	rte_kvargs_free(kvlist);
	return ret;
}

static int
rte_pmd_pcap_devuninit(const char *name)
{
	struct rte_eth_dev *eth_dev = NULL;

	RTE_LOG(INFO, PMD, "Closing pcap ethdev on numa socket %u\n",
			rte_socket_id());

	if (name == NULL)
		return -1;

	/* reserve an ethdev entry */
	eth_dev = rte_eth_dev_allocated(name);
	if (eth_dev == NULL)
		return -1;

	rte_free(eth_dev->data->dev_private);
	rte_free(eth_dev->data);

	rte_eth_dev_release_port(eth_dev);

	return 0;
}

static struct rte_driver pmd_pcap_drv = {
	.type = PMD_VDEV,
	.init = rte_pmd_pcap_devinit,
	.uninit = rte_pmd_pcap_devuninit,
};

PMD_REGISTER_DRIVER(pmd_pcap_drv, eth_pcap);
DRIVER_REGISTER_PARAM_STRING(eth_pcap,
	"rx_pcap=<string> "
	"tx_pcap=<string> "
	"rx_iface=<ifc> "
	"tx_iface=<ifc> "
	"iface=<ifc>");
