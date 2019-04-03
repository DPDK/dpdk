/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2016 Intel Corporation.
 * Copyright(c) 2014 6WIND S.A.
 * All rights reserved.
 */

#include <time.h>

#include <net/if.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <unistd.h>

#if defined(RTE_EXEC_ENV_FREEBSD)
#include <sys/sysctl.h>
#include <net/if_dl.h>
#endif

#include <pcap.h>

#include <rte_cycles.h>
#include <rte_ethdev_driver.h>
#include <rte_ethdev_vdev.h>
#include <rte_kvargs.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>
#include <rte_bus_vdev.h>
#include <rte_string_fns.h>

#define RTE_ETH_PCAP_SNAPSHOT_LEN 65535
#define RTE_ETH_PCAP_SNAPLEN ETHER_MAX_JUMBO_FRAME_LEN
#define RTE_ETH_PCAP_PROMISC 1
#define RTE_ETH_PCAP_TIMEOUT -1

#define ETH_PCAP_RX_PCAP_ARG  "rx_pcap"
#define ETH_PCAP_TX_PCAP_ARG  "tx_pcap"
#define ETH_PCAP_RX_IFACE_ARG "rx_iface"
#define ETH_PCAP_RX_IFACE_IN_ARG "rx_iface_in"
#define ETH_PCAP_TX_IFACE_ARG "tx_iface"
#define ETH_PCAP_IFACE_ARG    "iface"
#define ETH_PCAP_PHY_MAC_ARG  "phy_mac"

#define ETH_PCAP_ARG_MAXLEN	64

#define RTE_PMD_PCAP_MAX_QUEUES 16

static char errbuf[PCAP_ERRBUF_SIZE];
static unsigned char tx_pcap_data[RTE_ETH_PCAP_SNAPLEN];
static struct timeval start_time;
static uint64_t start_cycles;
static uint64_t hz;
static uint8_t iface_idx;

struct queue_stat {
	volatile unsigned long pkts;
	volatile unsigned long bytes;
	volatile unsigned long err_pkts;
};

struct pcap_rx_queue {
	uint16_t port_id;
	uint16_t queue_id;
	struct rte_mempool *mb_pool;
	struct queue_stat rx_stat;
	char name[PATH_MAX];
	char type[ETH_PCAP_ARG_MAXLEN];
};

struct pcap_tx_queue {
	uint16_t port_id;
	uint16_t queue_id;
	struct queue_stat tx_stat;
	char name[PATH_MAX];
	char type[ETH_PCAP_ARG_MAXLEN];
};

struct pmd_internals {
	struct pcap_rx_queue rx_queue[RTE_PMD_PCAP_MAX_QUEUES];
	struct pcap_tx_queue tx_queue[RTE_PMD_PCAP_MAX_QUEUES];
	char devargs[ETH_PCAP_ARG_MAXLEN];
	struct ether_addr eth_addr;
	int if_index;
	int single_iface;
	int phy_mac;
};

struct pmd_process_private {
	pcap_t *rx_pcap[RTE_PMD_PCAP_MAX_QUEUES];
	pcap_t *tx_pcap[RTE_PMD_PCAP_MAX_QUEUES];
	pcap_dumper_t *tx_dumper[RTE_PMD_PCAP_MAX_QUEUES];
};

struct pmd_devargs {
	unsigned int num_of_queue;
	struct devargs_queue {
		pcap_dumper_t *dumper;
		pcap_t *pcap;
		const char *name;
		const char *type;
	} queue[RTE_PMD_PCAP_MAX_QUEUES];
	int phy_mac;
};

static const char *valid_arguments[] = {
	ETH_PCAP_RX_PCAP_ARG,
	ETH_PCAP_TX_PCAP_ARG,
	ETH_PCAP_RX_IFACE_ARG,
	ETH_PCAP_RX_IFACE_IN_ARG,
	ETH_PCAP_TX_IFACE_ARG,
	ETH_PCAP_IFACE_ARG,
	ETH_PCAP_PHY_MAC_ARG,
	NULL
};

static struct rte_eth_link pmd_link = {
		.link_speed = ETH_SPEED_NUM_10G,
		.link_duplex = ETH_LINK_FULL_DUPLEX,
		.link_status = ETH_LINK_DOWN,
		.link_autoneg = ETH_LINK_FIXED,
};

static int eth_pcap_logtype;

#define PMD_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, eth_pcap_logtype, \
		"%s(): " fmt "\n", __func__, ##args)

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
eth_pcap_rx(void *queue, struct rte_mbuf **bufs, uint16_t nb_pkts)
{
	unsigned int i;
	struct pcap_pkthdr header;
	struct pmd_process_private *pp;
	const u_char *packet;
	struct rte_mbuf *mbuf;
	struct pcap_rx_queue *pcap_q = queue;
	uint16_t num_rx = 0;
	uint16_t buf_size;
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
		packet = pcap_next(pcap, &header);
		if (unlikely(packet == NULL))
			break;

		mbuf = rte_pktmbuf_alloc(pcap_q->mb_pool);
		if (unlikely(mbuf == NULL))
			break;

		/* Now get the space available for data in the mbuf */
		buf_size = rte_pktmbuf_data_room_size(pcap_q->mb_pool) -
				RTE_PKTMBUF_HEADROOM;

		if (header.caplen <= buf_size) {
			/* pcap packet will fit in the mbuf, can copy it */
			rte_memcpy(rte_pktmbuf_mtod(mbuf, void *), packet,
					header.caplen);
			mbuf->data_len = (uint16_t)header.caplen;
		} else {
			/* Try read jumbo frame into multi mbufs. */
			if (unlikely(eth_pcap_rx_jumbo(pcap_q->mb_pool,
						       mbuf,
						       packet,
						       header.caplen) == -1)) {
				rte_pktmbuf_free(mbuf);
				break;
			}
		}

		mbuf->pkt_len = (uint16_t)header.caplen;
		mbuf->port = pcap_q->port_id;
		bufs[num_rx] = mbuf;
		num_rx++;
		rx_bytes += header.caplen;
	}
	pcap_q->rx_stat.pkts += num_rx;
	pcap_q->rx_stat.bytes += rx_bytes;

	return num_rx;
}

static inline void
calculate_timestamp(struct timeval *ts) {
	uint64_t cycles;
	struct timeval cur_time;

	cycles = rte_get_timer_cycles() - start_cycles;
	cur_time.tv_sec = cycles / hz;
	cur_time.tv_usec = (cycles % hz) * 1e6 / hz;
	timeradd(&start_time, &cur_time, ts);
}

/*
 * Callback to handle writing packets to a pcap file.
 */
static uint16_t
eth_pcap_tx_dumper(void *queue, struct rte_mbuf **bufs, uint16_t nb_pkts)
{
	unsigned int i;
	struct rte_mbuf *mbuf;
	struct pmd_process_private *pp;
	struct pcap_tx_queue *dumper_q = queue;
	uint16_t num_tx = 0;
	uint32_t tx_bytes = 0;
	struct pcap_pkthdr header;
	pcap_dumper_t *dumper;

	pp = rte_eth_devices[dumper_q->port_id].process_private;
	dumper = pp->tx_dumper[dumper_q->queue_id];

	if (dumper == NULL || nb_pkts == 0)
		return 0;

	/* writes the nb_pkts packets to the previously opened pcap file
	 * dumper */
	for (i = 0; i < nb_pkts; i++) {
		mbuf = bufs[i];
		calculate_timestamp(&header.ts);
		header.len = mbuf->pkt_len;
		header.caplen = header.len;

		if (likely(mbuf->nb_segs == 1)) {
			pcap_dump((u_char *)dumper, &header,
				  rte_pktmbuf_mtod(mbuf, void*));
		} else {
			if (mbuf->pkt_len <= ETHER_MAX_JUMBO_FRAME_LEN) {
				eth_pcap_gather_data(tx_pcap_data, mbuf);
				pcap_dump((u_char *)dumper, &header,
					  tx_pcap_data);
			} else {
				PMD_LOG(ERR,
					"Dropping PCAP packet. Size (%d) > max jumbo size (%d).",
					mbuf->pkt_len,
					ETHER_MAX_JUMBO_FRAME_LEN);

				rte_pktmbuf_free(mbuf);
				break;
			}
		}

		num_tx++;
		tx_bytes += mbuf->pkt_len;
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
	dumper_q->tx_stat.err_pkts += nb_pkts - num_tx;

	return num_tx;
}

/*
 * Callback to handle sending packets through a real NIC.
 */
static uint16_t
eth_pcap_tx(void *queue, struct rte_mbuf **bufs, uint16_t nb_pkts)
{
	unsigned int i;
	int ret;
	struct rte_mbuf *mbuf;
	struct pmd_process_private *pp;
	struct pcap_tx_queue *tx_queue = queue;
	uint16_t num_tx = 0;
	uint32_t tx_bytes = 0;
	pcap_t *pcap;

	pp = rte_eth_devices[tx_queue->port_id].process_private;
	pcap = pp->tx_pcap[tx_queue->queue_id];

	if (unlikely(nb_pkts == 0 || pcap == NULL))
		return 0;

	for (i = 0; i < nb_pkts; i++) {
		mbuf = bufs[i];

		if (likely(mbuf->nb_segs == 1)) {
			ret = pcap_sendpacket(pcap,
					rte_pktmbuf_mtod(mbuf, u_char *),
					mbuf->pkt_len);
		} else {
			if (mbuf->pkt_len <= ETHER_MAX_JUMBO_FRAME_LEN) {
				eth_pcap_gather_data(tx_pcap_data, mbuf);
				ret = pcap_sendpacket(pcap,
						tx_pcap_data, mbuf->pkt_len);
			} else {
				PMD_LOG(ERR,
					"Dropping PCAP packet. Size (%d) > max jumbo size (%d).",
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

	tx_queue->tx_stat.pkts += num_tx;
	tx_queue->tx_stat.bytes += tx_bytes;
	tx_queue->tx_stat.err_pkts += nb_pkts - num_tx;

	return num_tx;
}

/*
 * pcap_open_live wrapper function
 */
static inline int
open_iface_live(const char *iface, pcap_t **pcap) {
	*pcap = pcap_open_live(iface, RTE_ETH_PCAP_SNAPLEN,
			RTE_ETH_PCAP_PROMISC, RTE_ETH_PCAP_TIMEOUT, errbuf);

	if (*pcap == NULL) {
		PMD_LOG(ERR, "Couldn't open %s: %s", iface, errbuf);
		return -1;
	}

	return 0;
}

static int
open_single_iface(const char *iface, pcap_t **pcap)
{
	if (open_iface_live(iface, pcap) < 0) {
		PMD_LOG(ERR, "Couldn't open interface %s", iface);
		return -1;
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
	tx_pcap = pcap_open_dead(DLT_EN10MB, RTE_ETH_PCAP_SNAPSHOT_LEN);
	if (tx_pcap == NULL) {
		PMD_LOG(ERR, "Couldn't create dead pcap");
		return -1;
	}

	/* The dumper is created using the previous pcap_t reference */
	*dumper = pcap_dump_open(tx_pcap, pcap_filename);
	if (*dumper == NULL) {
		pcap_close(tx_pcap);
		PMD_LOG(ERR, "Couldn't open %s for writing.",
			pcap_filename);
		return -1;
	}

	pcap_close(tx_pcap);
	return 0;
}

static int
open_single_rx_pcap(const char *pcap_filename, pcap_t **pcap)
{
	*pcap = pcap_open_offline(pcap_filename, errbuf);
	if (*pcap == NULL) {
		PMD_LOG(ERR, "Couldn't open %s: %s", pcap_filename,
			errbuf);
		return -1;
	}

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

	/* Special iface case. Single pcap is open and shared between tx/rx. */
	if (internals->single_iface) {
		tx = &internals->tx_queue[0];
		rx = &internals->rx_queue[0];

		if (!pp->tx_pcap[0] &&
			strcmp(tx->type, ETH_PCAP_IFACE_ARG) == 0) {
			if (open_single_iface(tx->name, &pp->tx_pcap[0]) < 0)
				return -1;
			pp->rx_pcap[0] = pp->tx_pcap[0];
		}

		goto status_up;
	}

	/* If not open already, open tx pcaps/dumpers */
	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		tx = &internals->tx_queue[i];

		if (!pp->tx_dumper[i] &&
				strcmp(tx->type, ETH_PCAP_TX_PCAP_ARG) == 0) {
			if (open_single_tx_pcap(tx->name,
				&pp->tx_dumper[i]) < 0)
				return -1;
		} else if (!pp->tx_pcap[i] &&
				strcmp(tx->type, ETH_PCAP_TX_IFACE_ARG) == 0) {
			if (open_single_iface(tx->name, &pp->tx_pcap[i]) < 0)
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
		} else if (strcmp(rx->type, ETH_PCAP_RX_IFACE_ARG) == 0) {
			if (open_single_iface(rx->name, &pp->rx_pcap[i]) < 0)
				return -1;
		}
	}

status_up:
	for (i = 0; i < dev->data->nb_rx_queues; i++)
		dev->data->rx_queue_state[i] = RTE_ETH_QUEUE_STATE_STARTED;

	for (i = 0; i < dev->data->nb_tx_queues; i++)
		dev->data->tx_queue_state[i] = RTE_ETH_QUEUE_STATE_STARTED;

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
	unsigned int i;
	struct pmd_internals *internals = dev->data->dev_private;
	struct pmd_process_private *pp = dev->process_private;

	/* Special iface case. Single pcap is open and shared between tx/rx. */
	if (internals->single_iface) {
		pcap_close(pp->tx_pcap[0]);
		pp->tx_pcap[0] = NULL;
		pp->rx_pcap[0] = NULL;
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
			pcap_close(pp->rx_pcap[i]);
			pp->rx_pcap[i] = NULL;
		}
	}

status_down:
	for (i = 0; i < dev->data->nb_rx_queues; i++)
		dev->data->rx_queue_state[i] = RTE_ETH_QUEUE_STATE_STOPPED;

	for (i = 0; i < dev->data->nb_tx_queues; i++)
		dev->data->tx_queue_state[i] = RTE_ETH_QUEUE_STATE_STOPPED;

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

	dev_info->if_index = internals->if_index;
	dev_info->max_mac_addrs = 1;
	dev_info->max_rx_pktlen = (uint32_t) -1;
	dev_info->max_rx_queues = dev->data->nb_rx_queues;
	dev_info->max_tx_queues = dev->data->nb_tx_queues;
	dev_info->min_rx_bufsize = 0;
}

static int
eth_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *stats)
{
	unsigned int i;
	unsigned long rx_packets_total = 0, rx_bytes_total = 0;
	unsigned long tx_packets_total = 0, tx_bytes_total = 0;
	unsigned long tx_packets_err_total = 0;
	const struct pmd_internals *internal = dev->data->dev_private;

	for (i = 0; i < RTE_ETHDEV_QUEUE_STAT_CNTRS &&
			i < dev->data->nb_rx_queues; i++) {
		stats->q_ipackets[i] = internal->rx_queue[i].rx_stat.pkts;
		stats->q_ibytes[i] = internal->rx_queue[i].rx_stat.bytes;
		rx_packets_total += stats->q_ipackets[i];
		rx_bytes_total += stats->q_ibytes[i];
	}

	for (i = 0; i < RTE_ETHDEV_QUEUE_STAT_CNTRS &&
			i < dev->data->nb_tx_queues; i++) {
		stats->q_opackets[i] = internal->tx_queue[i].tx_stat.pkts;
		stats->q_obytes[i] = internal->tx_queue[i].tx_stat.bytes;
		stats->q_errors[i] = internal->tx_queue[i].tx_stat.err_pkts;
		tx_packets_total += stats->q_opackets[i];
		tx_bytes_total += stats->q_obytes[i];
		tx_packets_err_total += stats->q_errors[i];
	}

	stats->ipackets = rx_packets_total;
	stats->ibytes = rx_bytes_total;
	stats->opackets = tx_packets_total;
	stats->obytes = tx_bytes_total;
	stats->oerrors = tx_packets_err_total;

	return 0;
}

static void
eth_stats_reset(struct rte_eth_dev *dev)
{
	unsigned int i;
	struct pmd_internals *internal = dev->data->dev_private;

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		internal->rx_queue[i].rx_stat.pkts = 0;
		internal->rx_queue[i].rx_stat.bytes = 0;
	}

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		internal->tx_queue[i].tx_stat.pkts = 0;
		internal->tx_queue[i].tx_stat.bytes = 0;
		internal->tx_queue[i].tx_stat.err_pkts = 0;
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
	pcap_q->port_id = dev->data->port_id;
	pcap_q->queue_id = rx_queue_id;
	dev->data->rx_queues[rx_queue_id] = pcap_q;

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
	struct pcap_tx_queue *pcap_q = &internals->tx_queue[tx_queue_id];

	pcap_q->port_id = dev->data->port_id;
	pcap_q->queue_id = tx_queue_id;
	dev->data->tx_queues[tx_queue_id] = pcap_q;

	return 0;
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

static const struct eth_dev_ops ops = {
	.dev_start = eth_dev_start,
	.dev_stop = eth_dev_stop,
	.dev_close = eth_dev_close,
	.dev_configure = eth_dev_configure,
	.dev_infos_get = eth_dev_info,
	.rx_queue_setup = eth_rx_queue_setup,
	.tx_queue_setup = eth_tx_queue_setup,
	.rx_queue_start = eth_rx_queue_start,
	.tx_queue_start = eth_tx_queue_start,
	.rx_queue_stop = eth_rx_queue_stop,
	.tx_queue_stop = eth_tx_queue_stop,
	.rx_queue_release = eth_queue_release,
	.tx_queue_release = eth_queue_release,
	.link_update = eth_link_update,
	.stats_get = eth_stats_get,
	.stats_reset = eth_stats_reset,
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

	if (open_single_tx_pcap(pcap_filename, &dumper) < 0)
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

	if (open_single_iface(iface, &pcap) < 0)
		return -1;

	tx->queue[0].pcap = pcap;
	tx->queue[0].name = iface;
	tx->queue[0].type = key;

	return 0;
}

static inline int
set_iface_direction(const char *iface, pcap_t *pcap,
		pcap_direction_t direction)
{
	const char *direction_str = (direction == PCAP_D_IN) ? "IN" : "OUT";
	if (pcap_setdirection(pcap, direction) < 0) {
		PMD_LOG(ERR, "Setting %s pcap direction %s failed - %s\n",
				iface, direction_str, pcap_geterr(pcap));
		return -1;
	}
	PMD_LOG(INFO, "Setting %s pcap direction %s\n",
			iface, direction_str);
	return 0;
}

static inline int
open_iface(const char *key, const char *value, void *extra_args)
{
	const char *iface = value;
	struct pmd_devargs *pmd = extra_args;
	pcap_t *pcap = NULL;

	if (open_single_iface(iface, &pcap) < 0)
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
select_phy_mac(const char *key __rte_unused, const char *value,
		void *extra_args)
{
	if (extra_args) {
		const int phy_mac = atoi(value);
		int *enable_phy_mac = extra_args;

		if (phy_mac)
			*enable_phy_mac = 1;
	}
	return 0;
}

static struct rte_vdev_driver pmd_pcap_drv;

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

	PMD_LOG(INFO, "Creating pcap-backed ethdev on numa socket %d",
		numa_node);

	pp = (struct pmd_process_private *)
		rte_zmalloc(NULL, sizeof(struct pmd_process_private),
				RTE_CACHE_LINE_SIZE);

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
	(*internals)->eth_addr = (struct ether_addr) {
		.addr_bytes = { 0x02, 0x70, 0x63, 0x61, 0x70, iface_idx++ }
	};
	(*internals)->phy_mac = 0;
	data = (*eth_dev)->data;
	data->nb_rx_queues = (uint16_t)nb_rx_queues;
	data->nb_tx_queues = (uint16_t)nb_tx_queues;
	data->dev_link = pmd_link;
	data->mac_addrs = &(*internals)->eth_addr;

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
#if defined(RTE_EXEC_ENV_LINUX)
	void *mac_addrs;
	struct ifreq ifr;
	int if_fd = socket(AF_INET, SOCK_DGRAM, 0);

	if (if_fd == -1)
		return -1;

	rte_strscpy(ifr.ifr_name, if_name, sizeof(ifr.ifr_name));
	if (ioctl(if_fd, SIOCGIFHWADDR, &ifr)) {
		close(if_fd);
		return -1;
	}

	mac_addrs = rte_zmalloc_socket(NULL, ETHER_ADDR_LEN, 0, numa_node);
	if (!mac_addrs) {
		close(if_fd);
		return -1;
	}

	PMD_LOG(INFO, "Setting phy MAC for %s", if_name);
	eth_dev->data->mac_addrs = mac_addrs;
	rte_memcpy(eth_dev->data->mac_addrs[0].addr_bytes,
			ifr.ifr_hwaddr.sa_data, ETHER_ADDR_LEN);

	close(if_fd);

	return 0;

#elif defined(RTE_EXEC_ENV_FREEBSD)
	void *mac_addrs;
	struct if_msghdr *ifm;
	struct sockaddr_dl *sdl;
	int mib[6];
	size_t len = 0;
	char *buf;

	mib[0] = CTL_NET;
	mib[1] = AF_ROUTE;
	mib[2] = 0;
	mib[3] = AF_LINK;
	mib[4] = NET_RT_IFLIST;
	mib[5] = if_nametoindex(if_name);

	if (sysctl(mib, 6, NULL, &len, NULL, 0) < 0)
		return -1;

	if (len == 0)
		return -1;

	buf = rte_malloc(NULL, len, 0);
	if (!buf)
		return -1;

	if (sysctl(mib, 6, buf, &len, NULL, 0) < 0) {
		rte_free(buf);
		return -1;
	}
	ifm = (struct if_msghdr *)buf;
	sdl = (struct sockaddr_dl *)(ifm + 1);

	mac_addrs = rte_zmalloc_socket(NULL, ETHER_ADDR_LEN, 0, numa_node);
	if (!mac_addrs) {
		rte_free(buf);
		return -1;
	}

	PMD_LOG(INFO, "Setting phy MAC for %s", if_name);
	eth_dev->data->mac_addrs = mac_addrs;
	rte_memcpy(eth_dev->data->mac_addrs[0].addr_bytes,
			LLADDR(sdl), ETHER_ADDR_LEN);

	rte_free(buf);

	return 0;
#else
	return -1;
#endif
}

static int
eth_from_pcaps_common(struct rte_vdev_device *vdev,
		struct pmd_devargs *rx_queues, const unsigned int nb_rx_queues,
		struct pmd_devargs *tx_queues, const unsigned int nb_tx_queues,
		struct pmd_internals **internals, struct rte_eth_dev **eth_dev)
{
	struct pmd_process_private *pp;
	unsigned int i;

	/* do some parameter checking */
	if (rx_queues == NULL && nb_rx_queues > 0)
		return -1;
	if (tx_queues == NULL && nb_tx_queues > 0)
		return -1;

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
		struct pmd_devargs *rx_queues, const unsigned int nb_rx_queues,
		struct pmd_devargs *tx_queues, const unsigned int nb_tx_queues,
		int single_iface, unsigned int using_dumpers)
{
	struct pmd_internals *internals = NULL;
	struct rte_eth_dev *eth_dev = NULL;
	int ret;

	ret = eth_from_pcaps_common(vdev, rx_queues, nb_rx_queues,
		tx_queues, nb_tx_queues, &internals, &eth_dev);

	if (ret < 0)
		return ret;

	/* store weather we are using a single interface for rx/tx or not */
	internals->single_iface = single_iface;

	if (single_iface) {
		internals->if_index = if_nametoindex(rx_queues->queue[0].name);

		/* phy_mac arg is applied only only if "iface" devarg is provided */
		if (rx_queues->phy_mac) {
			int ret = eth_pcap_update_mac(rx_queues->queue[0].name,
					eth_dev, vdev->device.numa_node);
			if (ret == 0)
				internals->phy_mac = 1;
		}
	}

	eth_dev->rx_pkt_burst = eth_pcap_rx;

	if (using_dumpers)
		eth_dev->tx_pkt_burst = eth_pcap_tx_dumper;
	else
		eth_dev->tx_pkt_burst = eth_pcap_tx;

	rte_eth_dev_probing_finish(eth_dev);
	return 0;
}

static int
pmd_pcap_probe(struct rte_vdev_device *dev)
{
	const char *name;
	unsigned int is_rx_pcap = 0, is_tx_pcap = 0;
	struct rte_kvargs *kvlist;
	struct pmd_devargs pcaps = {0};
	struct pmd_devargs dumpers = {0};
	struct rte_eth_dev *eth_dev =  NULL;
	struct pmd_internals *internal;
	int single_iface = 0;
	int ret;

	name = rte_vdev_device_name(dev);
	PMD_LOG(INFO, "Initializing pmd_pcap for %s", name);

	gettimeofday(&start_time, NULL);
	start_cycles = rte_get_timer_cycles();
	hz = rte_get_timer_hz();

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
				&select_phy_mac, &pcaps.phy_mac);
		if (ret < 0)
			goto free_kvlist;

		dumpers.phy_mac = pcaps.phy_mac;

		single_iface = 1;
		pcaps.num_of_queue = 1;
		dumpers.num_of_queue = 1;

		goto create_eth;
	}

	/*
	 * We check whether we want to open a RX stream from a real NIC or a
	 * pcap file
	 */
	is_rx_pcap = rte_kvargs_count(kvlist, ETH_PCAP_RX_PCAP_ARG) ? 1 : 0;
	pcaps.num_of_queue = 0;

	if (is_rx_pcap) {
		ret = rte_kvargs_process(kvlist, ETH_PCAP_RX_PCAP_ARG,
				&open_rx_pcap, &pcaps);
	} else {
		ret = rte_kvargs_process(kvlist, NULL,
				&rx_iface_args_process, &pcaps);
	}

	if (ret < 0)
		goto free_kvlist;

	/*
	 * We check whether we want to open a TX stream to a real NIC or a
	 * pcap file
	 */
	is_tx_pcap = rte_kvargs_count(kvlist, ETH_PCAP_TX_PCAP_ARG) ? 1 : 0;
	dumpers.num_of_queue = 0;

	if (is_tx_pcap)
		ret = rte_kvargs_process(kvlist, ETH_PCAP_TX_PCAP_ARG,
				&open_tx_pcap, &dumpers);
	else
		ret = rte_kvargs_process(kvlist, ETH_PCAP_TX_IFACE_ARG,
				&open_tx_iface, &dumpers);

	if (ret < 0)
		goto free_kvlist;

create_eth:
	if (rte_eal_process_type() == RTE_PROC_SECONDARY) {
		struct pmd_process_private *pp;
		unsigned int i;

		internal = eth_dev->data->dev_private;
			pp = (struct pmd_process_private *)
				rte_zmalloc(NULL,
					sizeof(struct pmd_process_private),
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
		if (is_tx_pcap)
			eth_dev->tx_pkt_burst = eth_pcap_tx_dumper;
		else
			eth_dev->tx_pkt_burst = eth_pcap_tx;

		rte_eth_dev_probing_finish(eth_dev);
		goto free_kvlist;
	}

	ret = eth_from_pcaps(dev, &pcaps, pcaps.num_of_queue, &dumpers,
		dumpers.num_of_queue, single_iface, is_tx_pcap);

free_kvlist:
	rte_kvargs_free(kvlist);

	return ret;
}

static int
pmd_pcap_remove(struct rte_vdev_device *dev)
{
	struct pmd_internals *internals = NULL;
	struct rte_eth_dev *eth_dev = NULL;

	PMD_LOG(INFO, "Closing pcap ethdev on numa socket %d",
			rte_socket_id());

	if (!dev)
		return -1;

	/* reserve an ethdev entry */
	eth_dev = rte_eth_dev_allocated(rte_vdev_device_name(dev));
	if (eth_dev == NULL)
		return -1;

	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		internals = eth_dev->data->dev_private;
		if (internals != NULL && internals->phy_mac == 0)
			/* not dynamically allocated, must not be freed */
			eth_dev->data->mac_addrs = NULL;
	}

	rte_free(eth_dev->process_private);
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
	ETH_PCAP_PHY_MAC_ARG "=<int>");

RTE_INIT(eth_pcap_init_log)
{
	eth_pcap_logtype = rte_log_register("pmd.net.pcap");
	if (eth_pcap_logtype >= 0)
		rte_log_set_level(eth_pcap_logtype, RTE_LOG_NOTICE);
}
