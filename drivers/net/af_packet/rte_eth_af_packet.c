/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014 John W. Linville <linville@tuxdriver.com>
 * Originally based upon librte_pmd_pcap code:
 * Copyright(c) 2010-2015 Intel Corporation.
 * Copyright(c) 2014 6WIND S.A.
 * All rights reserved.
 */

#include <rte_mbuf.h>
#include <rte_ethdev_driver.h>
#include <rte_ethdev_vdev.h>
#include <rte_malloc.h>
#include <rte_kvargs.h>
#include <rte_bus_vdev.h>

#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <poll.h>

#define ETH_AF_PACKET_IFACE_ARG		"iface"
#define ETH_AF_PACKET_NUM_Q_ARG		"qpairs"
#define ETH_AF_PACKET_BLOCKSIZE_ARG	"blocksz"
#define ETH_AF_PACKET_FRAMESIZE_ARG	"framesz"
#define ETH_AF_PACKET_FRAMECOUNT_ARG	"framecnt"
#define ETH_AF_PACKET_QDISC_BYPASS_ARG	"qdisc_bypass"

#define DFLT_BLOCK_SIZE		(1 << 12)
#define DFLT_FRAME_SIZE		(1 << 11)
#define DFLT_FRAME_COUNT	(1 << 9)

#define RTE_PMD_AF_PACKET_MAX_RINGS 16

struct pkt_rx_queue {
	int sockfd;

	struct iovec *rd;
	uint8_t *map;
	unsigned int framecount;
	unsigned int framenum;

	struct rte_mempool *mb_pool;
	uint16_t in_port;

	volatile unsigned long rx_pkts;
	volatile unsigned long err_pkts;
	volatile unsigned long rx_bytes;
};

struct pkt_tx_queue {
	int sockfd;
	unsigned int frame_data_size;

	struct iovec *rd;
	uint8_t *map;
	unsigned int framecount;
	unsigned int framenum;

	volatile unsigned long tx_pkts;
	volatile unsigned long err_pkts;
	volatile unsigned long tx_bytes;
};

struct pmd_internals {
	unsigned nb_queues;

	int if_index;
	char *if_name;
	struct ether_addr eth_addr;

	struct tpacket_req req;

	struct pkt_rx_queue rx_queue[RTE_PMD_AF_PACKET_MAX_RINGS];
	struct pkt_tx_queue tx_queue[RTE_PMD_AF_PACKET_MAX_RINGS];
};

static const char *valid_arguments[] = {
	ETH_AF_PACKET_IFACE_ARG,
	ETH_AF_PACKET_NUM_Q_ARG,
	ETH_AF_PACKET_BLOCKSIZE_ARG,
	ETH_AF_PACKET_FRAMESIZE_ARG,
	ETH_AF_PACKET_FRAMECOUNT_ARG,
	ETH_AF_PACKET_QDISC_BYPASS_ARG,
	NULL
};

static struct rte_eth_link pmd_link = {
	.link_speed = ETH_SPEED_NUM_10G,
	.link_duplex = ETH_LINK_FULL_DUPLEX,
	.link_status = ETH_LINK_DOWN,
	.link_autoneg = ETH_LINK_FIXED,
};

static int af_packet_logtype;

#define PMD_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, af_packet_logtype, \
		"%s(): " fmt "\n", __func__, ##args)

static uint16_t
eth_af_packet_rx(void *queue, struct rte_mbuf **bufs, uint16_t nb_pkts)
{
	unsigned i;
	struct tpacket2_hdr *ppd;
	struct rte_mbuf *mbuf;
	uint8_t *pbuf;
	struct pkt_rx_queue *pkt_q = queue;
	uint16_t num_rx = 0;
	unsigned long num_rx_bytes = 0;
	unsigned int framecount, framenum;

	if (unlikely(nb_pkts == 0))
		return 0;

	/*
	 * Reads the given number of packets from the AF_PACKET socket one by
	 * one and copies the packet data into a newly allocated mbuf.
	 */
	framecount = pkt_q->framecount;
	framenum = pkt_q->framenum;
	for (i = 0; i < nb_pkts; i++) {
		/* point at the next incoming frame */
		ppd = (struct tpacket2_hdr *) pkt_q->rd[framenum].iov_base;
		if ((ppd->tp_status & TP_STATUS_USER) == 0)
			break;

		/* allocate the next mbuf */
		mbuf = rte_pktmbuf_alloc(pkt_q->mb_pool);
		if (unlikely(mbuf == NULL))
			break;

		/* packet will fit in the mbuf, go ahead and receive it */
		rte_pktmbuf_pkt_len(mbuf) = rte_pktmbuf_data_len(mbuf) = ppd->tp_snaplen;
		pbuf = (uint8_t *) ppd + ppd->tp_mac;
		memcpy(rte_pktmbuf_mtod(mbuf, void *), pbuf, rte_pktmbuf_data_len(mbuf));

		/* check for vlan info */
		if (ppd->tp_status & TP_STATUS_VLAN_VALID) {
			mbuf->vlan_tci = ppd->tp_vlan_tci;
			mbuf->ol_flags |= (PKT_RX_VLAN | PKT_RX_VLAN_STRIPPED);
		}

		/* release incoming frame and advance ring buffer */
		ppd->tp_status = TP_STATUS_KERNEL;
		if (++framenum >= framecount)
			framenum = 0;
		mbuf->port = pkt_q->in_port;

		/* account for the receive frame */
		bufs[i] = mbuf;
		num_rx++;
		num_rx_bytes += mbuf->pkt_len;
	}
	pkt_q->framenum = framenum;
	pkt_q->rx_pkts += num_rx;
	pkt_q->rx_bytes += num_rx_bytes;
	return num_rx;
}

/*
 * Callback to handle sending packets through a real NIC.
 */
static uint16_t
eth_af_packet_tx(void *queue, struct rte_mbuf **bufs, uint16_t nb_pkts)
{
	struct tpacket2_hdr *ppd;
	struct rte_mbuf *mbuf;
	uint8_t *pbuf;
	unsigned int framecount, framenum;
	struct pollfd pfd;
	struct pkt_tx_queue *pkt_q = queue;
	uint16_t num_tx = 0;
	unsigned long num_tx_bytes = 0;
	int i;

	if (unlikely(nb_pkts == 0))
		return 0;

	memset(&pfd, 0, sizeof(pfd));
	pfd.fd = pkt_q->sockfd;
	pfd.events = POLLOUT;
	pfd.revents = 0;

	framecount = pkt_q->framecount;
	framenum = pkt_q->framenum;
	ppd = (struct tpacket2_hdr *) pkt_q->rd[framenum].iov_base;
	for (i = 0; i < nb_pkts; i++) {
		mbuf = *bufs++;

		/* drop oversized packets */
		if (mbuf->pkt_len > pkt_q->frame_data_size) {
			rte_pktmbuf_free(mbuf);
			continue;
		}

		/* insert vlan info if necessary */
		if (mbuf->ol_flags & PKT_TX_VLAN_PKT) {
			if (rte_vlan_insert(&mbuf)) {
				rte_pktmbuf_free(mbuf);
				continue;
			}
		}

		/* point at the next incoming frame */
		if ((ppd->tp_status != TP_STATUS_AVAILABLE) &&
		    (poll(&pfd, 1, -1) < 0))
			break;

		/* copy the tx frame data */
		pbuf = (uint8_t *) ppd + TPACKET2_HDRLEN -
			sizeof(struct sockaddr_ll);

		struct rte_mbuf *tmp_mbuf = mbuf;
		while (tmp_mbuf) {
			uint16_t data_len = rte_pktmbuf_data_len(tmp_mbuf);
			memcpy(pbuf, rte_pktmbuf_mtod(tmp_mbuf, void*), data_len);
			pbuf += data_len;
			tmp_mbuf = tmp_mbuf->next;
		}

		ppd->tp_len = mbuf->pkt_len;
		ppd->tp_snaplen = mbuf->pkt_len;

		/* release incoming frame and advance ring buffer */
		ppd->tp_status = TP_STATUS_SEND_REQUEST;
		if (++framenum >= framecount)
			framenum = 0;
		ppd = (struct tpacket2_hdr *) pkt_q->rd[framenum].iov_base;

		num_tx++;
		num_tx_bytes += mbuf->pkt_len;
		rte_pktmbuf_free(mbuf);
	}

	/* kick-off transmits */
	if (sendto(pkt_q->sockfd, NULL, 0, MSG_DONTWAIT, NULL, 0) == -1) {
		/* error sending -- no packets transmitted */
		num_tx = 0;
		num_tx_bytes = 0;
	}

	pkt_q->framenum = framenum;
	pkt_q->tx_pkts += num_tx;
	pkt_q->err_pkts += i - num_tx;
	pkt_q->tx_bytes += num_tx_bytes;
	return i;
}

static int
eth_dev_start(struct rte_eth_dev *dev)
{
	dev->data->dev_link.link_status = ETH_LINK_UP;
	return 0;
}

/*
 * This function gets called when the current port gets stopped.
 */
static void
eth_dev_stop(struct rte_eth_dev *dev)
{
	unsigned i;
	int sockfd;
	struct pmd_internals *internals = dev->data->dev_private;

	for (i = 0; i < internals->nb_queues; i++) {
		sockfd = internals->rx_queue[i].sockfd;
		if (sockfd != -1)
			close(sockfd);

		/* Prevent use after free in case tx fd == rx fd */
		if (sockfd != internals->tx_queue[i].sockfd) {
			sockfd = internals->tx_queue[i].sockfd;
			if (sockfd != -1)
				close(sockfd);
		}

		internals->rx_queue[i].sockfd = -1;
		internals->tx_queue[i].sockfd = -1;
	}

	dev->data->dev_link.link_status = ETH_LINK_DOWN;
}

static int
eth_dev_configure(struct rte_eth_dev *dev __rte_unused)
{
	return 0;
}

static void
eth_dev_info(struct rte_eth_dev *dev, struct rte_eth_dev_info *dev_info)
{
	struct pmd_internals *internals = dev->data->dev_private;

	dev_info->if_index = internals->if_index;
	dev_info->max_mac_addrs = 1;
	dev_info->max_rx_pktlen = (uint32_t)ETH_FRAME_LEN;
	dev_info->max_rx_queues = (uint16_t)internals->nb_queues;
	dev_info->max_tx_queues = (uint16_t)internals->nb_queues;
	dev_info->min_rx_bufsize = 0;
}

static int
eth_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *igb_stats)
{
	unsigned i, imax;
	unsigned long rx_total = 0, tx_total = 0, tx_err_total = 0;
	unsigned long rx_bytes_total = 0, tx_bytes_total = 0;
	const struct pmd_internals *internal = dev->data->dev_private;

	imax = (internal->nb_queues < RTE_ETHDEV_QUEUE_STAT_CNTRS ?
	        internal->nb_queues : RTE_ETHDEV_QUEUE_STAT_CNTRS);
	for (i = 0; i < imax; i++) {
		igb_stats->q_ipackets[i] = internal->rx_queue[i].rx_pkts;
		igb_stats->q_ibytes[i] = internal->rx_queue[i].rx_bytes;
		rx_total += igb_stats->q_ipackets[i];
		rx_bytes_total += igb_stats->q_ibytes[i];
	}

	imax = (internal->nb_queues < RTE_ETHDEV_QUEUE_STAT_CNTRS ?
	        internal->nb_queues : RTE_ETHDEV_QUEUE_STAT_CNTRS);
	for (i = 0; i < imax; i++) {
		igb_stats->q_opackets[i] = internal->tx_queue[i].tx_pkts;
		igb_stats->q_errors[i] = internal->tx_queue[i].err_pkts;
		igb_stats->q_obytes[i] = internal->tx_queue[i].tx_bytes;
		tx_total += igb_stats->q_opackets[i];
		tx_err_total += igb_stats->q_errors[i];
		tx_bytes_total += igb_stats->q_obytes[i];
	}

	igb_stats->ipackets = rx_total;
	igb_stats->ibytes = rx_bytes_total;
	igb_stats->opackets = tx_total;
	igb_stats->oerrors = tx_err_total;
	igb_stats->obytes = tx_bytes_total;
	return 0;
}

static void
eth_stats_reset(struct rte_eth_dev *dev)
{
	unsigned i;
	struct pmd_internals *internal = dev->data->dev_private;

	for (i = 0; i < internal->nb_queues; i++) {
		internal->rx_queue[i].rx_pkts = 0;
		internal->rx_queue[i].rx_bytes = 0;
	}

	for (i = 0; i < internal->nb_queues; i++) {
		internal->tx_queue[i].tx_pkts = 0;
		internal->tx_queue[i].err_pkts = 0;
		internal->tx_queue[i].tx_bytes = 0;
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
	struct pkt_rx_queue *pkt_q = &internals->rx_queue[rx_queue_id];
	unsigned int buf_size, data_size;

	pkt_q->mb_pool = mb_pool;

	/* Now get the space available for data in the mbuf */
	buf_size = rte_pktmbuf_data_room_size(pkt_q->mb_pool) -
		RTE_PKTMBUF_HEADROOM;
	data_size = internals->req.tp_frame_size;
	data_size -= TPACKET2_HDRLEN - sizeof(struct sockaddr_ll);

	if (data_size > buf_size) {
		PMD_LOG(ERR,
			"%s: %d bytes will not fit in mbuf (%d bytes)",
			dev->device->name, data_size, buf_size);
		return -ENOMEM;
	}

	dev->data->rx_queues[rx_queue_id] = pkt_q;
	pkt_q->in_port = dev->data->port_id;

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

static int
eth_dev_mtu_set(struct rte_eth_dev *dev, uint16_t mtu)
{
	struct pmd_internals *internals = dev->data->dev_private;
	struct ifreq ifr = { .ifr_mtu = mtu };
	int ret;
	int s;
	unsigned int data_size = internals->req.tp_frame_size -
				 TPACKET2_HDRLEN -
				 sizeof(struct sockaddr_ll);

	if (mtu > data_size)
		return -EINVAL;

	s = socket(PF_INET, SOCK_DGRAM, 0);
	if (s < 0)
		return -EINVAL;

	snprintf(ifr.ifr_name, IFNAMSIZ, "%s", internals->if_name);
	ret = ioctl(s, SIOCSIFMTU, &ifr);
	close(s);

	if (ret < 0)
		return -EINVAL;

	return 0;
}

static void
eth_dev_change_flags(char *if_name, uint32_t flags, uint32_t mask)
{
	struct ifreq ifr;
	int s;

	s = socket(PF_INET, SOCK_DGRAM, 0);
	if (s < 0)
		return;

	snprintf(ifr.ifr_name, IFNAMSIZ, "%s", if_name);
	if (ioctl(s, SIOCGIFFLAGS, &ifr) < 0)
		goto out;
	ifr.ifr_flags &= mask;
	ifr.ifr_flags |= flags;
	if (ioctl(s, SIOCSIFFLAGS, &ifr) < 0)
		goto out;
out:
	close(s);
}

static void
eth_dev_promiscuous_enable(struct rte_eth_dev *dev)
{
	struct pmd_internals *internals = dev->data->dev_private;

	eth_dev_change_flags(internals->if_name, IFF_PROMISC, ~0);
}

static void
eth_dev_promiscuous_disable(struct rte_eth_dev *dev)
{
	struct pmd_internals *internals = dev->data->dev_private;

	eth_dev_change_flags(internals->if_name, 0, ~IFF_PROMISC);
}

static const struct eth_dev_ops ops = {
	.dev_start = eth_dev_start,
	.dev_stop = eth_dev_stop,
	.dev_close = eth_dev_close,
	.dev_configure = eth_dev_configure,
	.dev_infos_get = eth_dev_info,
	.mtu_set = eth_dev_mtu_set,
	.promiscuous_enable = eth_dev_promiscuous_enable,
	.promiscuous_disable = eth_dev_promiscuous_disable,
	.rx_queue_setup = eth_rx_queue_setup,
	.tx_queue_setup = eth_tx_queue_setup,
	.rx_queue_release = eth_queue_release,
	.tx_queue_release = eth_queue_release,
	.link_update = eth_link_update,
	.stats_get = eth_stats_get,
	.stats_reset = eth_stats_reset,
};

/*
 * Opens an AF_PACKET socket
 */
static int
open_packet_iface(const char *key __rte_unused,
                  const char *value __rte_unused,
                  void *extra_args)
{
	int *sockfd = extra_args;

	/* Open an AF_PACKET socket... */
	*sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if (*sockfd == -1) {
		PMD_LOG(ERR, "Could not open AF_PACKET socket");
		return -1;
	}

	return 0;
}

static struct rte_vdev_driver pmd_af_packet_drv;

static int
rte_pmd_init_internals(struct rte_vdev_device *dev,
                       const int sockfd,
                       const unsigned nb_queues,
                       unsigned int blocksize,
                       unsigned int blockcnt,
                       unsigned int framesize,
                       unsigned int framecnt,
		       unsigned int qdisc_bypass,
                       struct pmd_internals **internals,
                       struct rte_eth_dev **eth_dev,
                       struct rte_kvargs *kvlist)
{
	const char *name = rte_vdev_device_name(dev);
	const unsigned int numa_node = dev->device.numa_node;
	struct rte_eth_dev_data *data = NULL;
	struct rte_kvargs_pair *pair = NULL;
	struct ifreq ifr;
	size_t ifnamelen;
	unsigned k_idx;
	struct sockaddr_ll sockaddr;
	struct tpacket_req *req;
	struct pkt_rx_queue *rx_queue;
	struct pkt_tx_queue *tx_queue;
	int rc, tpver, discard;
	int qsockfd = -1;
	unsigned int i, q, rdsize;
#if defined(PACKET_FANOUT)
	int fanout_arg;
#endif

	for (k_idx = 0; k_idx < kvlist->count; k_idx++) {
		pair = &kvlist->pairs[k_idx];
		if (strstr(pair->key, ETH_AF_PACKET_IFACE_ARG) != NULL)
			break;
	}
	if (pair == NULL) {
		PMD_LOG(ERR,
			"%s: no interface specified for AF_PACKET ethdev",
		        name);
		return -1;
	}

	PMD_LOG(INFO,
		"%s: creating AF_PACKET-backed ethdev on numa socket %u",
		name, numa_node);

	*internals = rte_zmalloc_socket(name, sizeof(**internals),
	                                0, numa_node);
	if (*internals == NULL)
		return -1;

	for (q = 0; q < nb_queues; q++) {
		(*internals)->rx_queue[q].map = MAP_FAILED;
		(*internals)->tx_queue[q].map = MAP_FAILED;
	}

	req = &((*internals)->req);

	req->tp_block_size = blocksize;
	req->tp_block_nr = blockcnt;
	req->tp_frame_size = framesize;
	req->tp_frame_nr = framecnt;

	ifnamelen = strlen(pair->value);
	if (ifnamelen < sizeof(ifr.ifr_name)) {
		memcpy(ifr.ifr_name, pair->value, ifnamelen);
		ifr.ifr_name[ifnamelen] = '\0';
	} else {
		PMD_LOG(ERR,
			"%s: I/F name too long (%s)",
			name, pair->value);
		return -1;
	}
	if (ioctl(sockfd, SIOCGIFINDEX, &ifr) == -1) {
		PMD_LOG(ERR,
			"%s: ioctl failed (SIOCGIFINDEX)",
		        name);
		return -1;
	}
	(*internals)->if_name = strdup(pair->value);
	if ((*internals)->if_name == NULL)
		return -1;
	(*internals)->if_index = ifr.ifr_ifindex;

	if (ioctl(sockfd, SIOCGIFHWADDR, &ifr) == -1) {
		PMD_LOG(ERR,
			"%s: ioctl failed (SIOCGIFHWADDR)",
		        name);
		return -1;
	}
	memcpy(&(*internals)->eth_addr, ifr.ifr_hwaddr.sa_data, ETH_ALEN);

	memset(&sockaddr, 0, sizeof(sockaddr));
	sockaddr.sll_family = AF_PACKET;
	sockaddr.sll_protocol = htons(ETH_P_ALL);
	sockaddr.sll_ifindex = (*internals)->if_index;

#if defined(PACKET_FANOUT)
	fanout_arg = (getpid() ^ (*internals)->if_index) & 0xffff;
	fanout_arg |= (PACKET_FANOUT_HASH | PACKET_FANOUT_FLAG_DEFRAG) << 16;
#if defined(PACKET_FANOUT_FLAG_ROLLOVER)
	fanout_arg |= PACKET_FANOUT_FLAG_ROLLOVER << 16;
#endif
#endif

	for (q = 0; q < nb_queues; q++) {
		/* Open an AF_PACKET socket for this queue... */
		qsockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
		if (qsockfd == -1) {
			PMD_LOG(ERR,
				"%s: could not open AF_PACKET socket",
			        name);
			return -1;
		}

		tpver = TPACKET_V2;
		rc = setsockopt(qsockfd, SOL_PACKET, PACKET_VERSION,
				&tpver, sizeof(tpver));
		if (rc == -1) {
			PMD_LOG(ERR,
				"%s: could not set PACKET_VERSION on AF_PACKET socket for %s",
				name, pair->value);
			goto error;
		}

		discard = 1;
		rc = setsockopt(qsockfd, SOL_PACKET, PACKET_LOSS,
				&discard, sizeof(discard));
		if (rc == -1) {
			PMD_LOG(ERR,
				"%s: could not set PACKET_LOSS on AF_PACKET socket for %s",
				name, pair->value);
			goto error;
		}

#if defined(PACKET_QDISC_BYPASS)
		rc = setsockopt(qsockfd, SOL_PACKET, PACKET_QDISC_BYPASS,
				&qdisc_bypass, sizeof(qdisc_bypass));
		if (rc == -1) {
			PMD_LOG(ERR,
				"%s: could not set PACKET_QDISC_BYPASS on AF_PACKET socket for %s",
				name, pair->value);
			goto error;
		}
#else
		RTE_SET_USED(qdisc_bypass);
#endif

		rc = setsockopt(qsockfd, SOL_PACKET, PACKET_RX_RING, req, sizeof(*req));
		if (rc == -1) {
			PMD_LOG(ERR,
				"%s: could not set PACKET_RX_RING on AF_PACKET socket for %s",
				name, pair->value);
			goto error;
		}

		rc = setsockopt(qsockfd, SOL_PACKET, PACKET_TX_RING, req, sizeof(*req));
		if (rc == -1) {
			PMD_LOG(ERR,
				"%s: could not set PACKET_TX_RING on AF_PACKET "
				"socket for %s", name, pair->value);
			goto error;
		}

		rx_queue = &((*internals)->rx_queue[q]);
		rx_queue->framecount = req->tp_frame_nr;

		rx_queue->map = mmap(NULL, 2 * req->tp_block_size * req->tp_block_nr,
				    PROT_READ | PROT_WRITE, MAP_SHARED | MAP_LOCKED,
				    qsockfd, 0);
		if (rx_queue->map == MAP_FAILED) {
			PMD_LOG(ERR,
				"%s: call to mmap failed on AF_PACKET socket for %s",
				name, pair->value);
			goto error;
		}

		/* rdsize is same for both Tx and Rx */
		rdsize = req->tp_frame_nr * sizeof(*(rx_queue->rd));

		rx_queue->rd = rte_zmalloc_socket(name, rdsize, 0, numa_node);
		if (rx_queue->rd == NULL)
			goto error;
		for (i = 0; i < req->tp_frame_nr; ++i) {
			rx_queue->rd[i].iov_base = rx_queue->map + (i * framesize);
			rx_queue->rd[i].iov_len = req->tp_frame_size;
		}
		rx_queue->sockfd = qsockfd;

		tx_queue = &((*internals)->tx_queue[q]);
		tx_queue->framecount = req->tp_frame_nr;
		tx_queue->frame_data_size = req->tp_frame_size;
		tx_queue->frame_data_size -= TPACKET2_HDRLEN -
			sizeof(struct sockaddr_ll);

		tx_queue->map = rx_queue->map + req->tp_block_size * req->tp_block_nr;

		tx_queue->rd = rte_zmalloc_socket(name, rdsize, 0, numa_node);
		if (tx_queue->rd == NULL)
			goto error;
		for (i = 0; i < req->tp_frame_nr; ++i) {
			tx_queue->rd[i].iov_base = tx_queue->map + (i * framesize);
			tx_queue->rd[i].iov_len = req->tp_frame_size;
		}
		tx_queue->sockfd = qsockfd;

		rc = bind(qsockfd, (const struct sockaddr*)&sockaddr, sizeof(sockaddr));
		if (rc == -1) {
			PMD_LOG(ERR,
				"%s: could not bind AF_PACKET socket to %s",
			        name, pair->value);
			goto error;
		}

#if defined(PACKET_FANOUT)
		rc = setsockopt(qsockfd, SOL_PACKET, PACKET_FANOUT,
				&fanout_arg, sizeof(fanout_arg));
		if (rc == -1) {
			PMD_LOG(ERR,
				"%s: could not set PACKET_FANOUT on AF_PACKET socket "
				"for %s", name, pair->value);
			goto error;
		}
#endif
	}

	/* reserve an ethdev entry */
	*eth_dev = rte_eth_vdev_allocate(dev, 0);
	if (*eth_dev == NULL)
		goto error;

	/*
	 * now put it all together
	 * - store queue data in internals,
	 * - store numa_node in eth_dev
	 * - point eth_dev_data to internals
	 * - and point eth_dev structure to new eth_dev_data structure
	 */

	(*internals)->nb_queues = nb_queues;

	data = (*eth_dev)->data;
	data->dev_private = *internals;
	data->nb_rx_queues = (uint16_t)nb_queues;
	data->nb_tx_queues = (uint16_t)nb_queues;
	data->dev_link = pmd_link;
	data->mac_addrs = &(*internals)->eth_addr;

	(*eth_dev)->dev_ops = &ops;

	return 0;

error:
	if (qsockfd != -1)
		close(qsockfd);
	for (q = 0; q < nb_queues; q++) {
		munmap((*internals)->rx_queue[q].map,
		       2 * req->tp_block_size * req->tp_block_nr);

		rte_free((*internals)->rx_queue[q].rd);
		rte_free((*internals)->tx_queue[q].rd);
		if (((*internals)->rx_queue[q].sockfd != 0) &&
			((*internals)->rx_queue[q].sockfd != qsockfd))
			close((*internals)->rx_queue[q].sockfd);
	}
	free((*internals)->if_name);
	rte_free(*internals);
	return -1;
}

static int
rte_eth_from_packet(struct rte_vdev_device *dev,
                    int const *sockfd,
                    struct rte_kvargs *kvlist)
{
	const char *name = rte_vdev_device_name(dev);
	struct pmd_internals *internals = NULL;
	struct rte_eth_dev *eth_dev = NULL;
	struct rte_kvargs_pair *pair = NULL;
	unsigned k_idx;
	unsigned int blockcount;
	unsigned int blocksize = DFLT_BLOCK_SIZE;
	unsigned int framesize = DFLT_FRAME_SIZE;
	unsigned int framecount = DFLT_FRAME_COUNT;
	unsigned int qpairs = 1;
	unsigned int qdisc_bypass = 1;

	/* do some parameter checking */
	if (*sockfd < 0)
		return -1;

	/*
	 * Walk arguments for configurable settings
	 */
	for (k_idx = 0; k_idx < kvlist->count; k_idx++) {
		pair = &kvlist->pairs[k_idx];
		if (strstr(pair->key, ETH_AF_PACKET_NUM_Q_ARG) != NULL) {
			qpairs = atoi(pair->value);
			if (qpairs < 1 ||
			    qpairs > RTE_PMD_AF_PACKET_MAX_RINGS) {
				PMD_LOG(ERR,
					"%s: invalid qpairs value",
				        name);
				return -1;
			}
			continue;
		}
		if (strstr(pair->key, ETH_AF_PACKET_BLOCKSIZE_ARG) != NULL) {
			blocksize = atoi(pair->value);
			if (!blocksize) {
				PMD_LOG(ERR,
					"%s: invalid blocksize value",
				        name);
				return -1;
			}
			continue;
		}
		if (strstr(pair->key, ETH_AF_PACKET_FRAMESIZE_ARG) != NULL) {
			framesize = atoi(pair->value);
			if (!framesize) {
				PMD_LOG(ERR,
					"%s: invalid framesize value",
				        name);
				return -1;
			}
			continue;
		}
		if (strstr(pair->key, ETH_AF_PACKET_FRAMECOUNT_ARG) != NULL) {
			framecount = atoi(pair->value);
			if (!framecount) {
				PMD_LOG(ERR,
					"%s: invalid framecount value",
				        name);
				return -1;
			}
			continue;
		}
		if (strstr(pair->key, ETH_AF_PACKET_QDISC_BYPASS_ARG) != NULL) {
			qdisc_bypass = atoi(pair->value);
			if (qdisc_bypass > 1) {
				PMD_LOG(ERR,
					"%s: invalid bypass value",
					name);
				return -1;
			}
			continue;
		}
	}

	if (framesize > blocksize) {
		PMD_LOG(ERR,
			"%s: AF_PACKET MMAP frame size exceeds block size!",
		        name);
		return -1;
	}

	blockcount = framecount / (blocksize / framesize);
	if (!blockcount) {
		PMD_LOG(ERR,
			"%s: invalid AF_PACKET MMAP parameters", name);
		return -1;
	}

	PMD_LOG(INFO, "%s: AF_PACKET MMAP parameters:", name);
	PMD_LOG(INFO, "%s:\tblock size %d", name, blocksize);
	PMD_LOG(INFO, "%s:\tblock count %d", name, blockcount);
	PMD_LOG(INFO, "%s:\tframe size %d", name, framesize);
	PMD_LOG(INFO, "%s:\tframe count %d", name, framecount);

	if (rte_pmd_init_internals(dev, *sockfd, qpairs,
				   blocksize, blockcount,
				   framesize, framecount,
				   qdisc_bypass,
				   &internals, &eth_dev,
				   kvlist) < 0)
		return -1;

	eth_dev->rx_pkt_burst = eth_af_packet_rx;
	eth_dev->tx_pkt_burst = eth_af_packet_tx;

	rte_eth_dev_probing_finish(eth_dev);
	return 0;
}

static int
rte_pmd_af_packet_probe(struct rte_vdev_device *dev)
{
	int ret = 0;
	struct rte_kvargs *kvlist;
	int sockfd = -1;
	struct rte_eth_dev *eth_dev;
	const char *name = rte_vdev_device_name(dev);

	PMD_LOG(INFO, "Initializing pmd_af_packet for %s", name);

	if (rte_eal_process_type() == RTE_PROC_SECONDARY &&
	    strlen(rte_vdev_device_args(dev)) == 0) {
		eth_dev = rte_eth_dev_attach_secondary(name);
		if (!eth_dev) {
			PMD_LOG(ERR, "Failed to probe %s", name);
			return -1;
		}
		/* TODO: request info from primary to set up Rx and Tx */
		eth_dev->dev_ops = &ops;
		rte_eth_dev_probing_finish(eth_dev);
		return 0;
	}

	kvlist = rte_kvargs_parse(rte_vdev_device_args(dev), valid_arguments);
	if (kvlist == NULL) {
		ret = -1;
		goto exit;
	}

	/*
	 * If iface argument is passed we open the NICs and use them for
	 * reading / writing
	 */
	if (rte_kvargs_count(kvlist, ETH_AF_PACKET_IFACE_ARG) == 1) {

		ret = rte_kvargs_process(kvlist, ETH_AF_PACKET_IFACE_ARG,
		                         &open_packet_iface, &sockfd);
		if (ret < 0)
			goto exit;
	}

	if (dev->device.numa_node == SOCKET_ID_ANY)
		dev->device.numa_node = rte_socket_id();

	ret = rte_eth_from_packet(dev, &sockfd, kvlist);
	close(sockfd); /* no longer needed */

exit:
	rte_kvargs_free(kvlist);
	return ret;
}

static int
rte_pmd_af_packet_remove(struct rte_vdev_device *dev)
{
	struct rte_eth_dev *eth_dev = NULL;
	struct pmd_internals *internals;
	unsigned q;

	PMD_LOG(INFO, "Closing AF_PACKET ethdev on numa socket %u",
		rte_socket_id());

	if (dev == NULL)
		return -1;

	/* find the ethdev entry */
	eth_dev = rte_eth_dev_allocated(rte_vdev_device_name(dev));
	if (eth_dev == NULL)
		return -1;

	internals = eth_dev->data->dev_private;
	for (q = 0; q < internals->nb_queues; q++) {
		rte_free(internals->rx_queue[q].rd);
		rte_free(internals->tx_queue[q].rd);
	}
	free(internals->if_name);

	rte_free(eth_dev->data->dev_private);

	rte_eth_dev_release_port(eth_dev);

	return 0;
}

static struct rte_vdev_driver pmd_af_packet_drv = {
	.probe = rte_pmd_af_packet_probe,
	.remove = rte_pmd_af_packet_remove,
};

RTE_PMD_REGISTER_VDEV(net_af_packet, pmd_af_packet_drv);
RTE_PMD_REGISTER_ALIAS(net_af_packet, eth_af_packet);
RTE_PMD_REGISTER_PARAM_STRING(net_af_packet,
	"iface=<string> "
	"qpairs=<int> "
	"blocksz=<int> "
	"framesz=<int> "
	"framecnt=<int> "
	"qdisc_bypass=<0|1>");

RTE_INIT(af_packet_init_log);
static void
af_packet_init_log(void)
{
	af_packet_logtype = rte_log_register("pmd.net.packet");
	if (af_packet_logtype >= 0)
		rte_log_set_level(af_packet_logtype, RTE_LOG_NOTICE);
}
