/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2017 Intel Corporation
 */

#include <rte_atomic.h>
#include <rte_branch_prediction.h>
#include <rte_byteorder.h>
#include <rte_common.h>
#include <rte_mbuf.h>
#include <rte_ethdev_driver.h>
#include <rte_ethdev_vdev.h>
#include <rte_malloc.h>
#include <rte_bus_vdev.h>
#include <rte_kvargs.h>
#include <rte_net.h>
#include <rte_debug.h>
#include <rte_ip.h>
#include <rte_string_fns.h>
#include <rte_ethdev.h>
#include <rte_errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/utsname.h>
#include <sys/mman.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/uio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <linux/if_tun.h>
#include <linux/if_ether.h>
#include <fcntl.h>
#include <ctype.h>

#include <tap_rss.h>
#include <rte_eth_tap.h>
#include <tap_flow.h>
#include <tap_netlink.h>
#include <tap_tcmsgs.h>

/* Linux based path to the TUN device */
#define TUN_TAP_DEV_PATH        "/dev/net/tun"
#define DEFAULT_TAP_NAME        "dtap"
#define DEFAULT_TUN_NAME        "dtun"

#define ETH_TAP_IFACE_ARG       "iface"
#define ETH_TAP_REMOTE_ARG      "remote"
#define ETH_TAP_MAC_ARG         "mac"
#define ETH_TAP_MAC_FIXED       "fixed"

#define ETH_TAP_USR_MAC_FMT     "xx:xx:xx:xx:xx:xx"
#define ETH_TAP_CMP_MAC_FMT     "0123456789ABCDEFabcdef"
#define ETH_TAP_MAC_ARG_FMT     ETH_TAP_MAC_FIXED "|" ETH_TAP_USR_MAC_FMT

#define TAP_GSO_MBUFS_PER_CORE	128
#define TAP_GSO_MBUF_SEG_SIZE	128
#define TAP_GSO_MBUF_CACHE_SIZE	4
#define TAP_GSO_MBUFS_NUM \
	(TAP_GSO_MBUFS_PER_CORE * TAP_GSO_MBUF_CACHE_SIZE)

/* IPC key for queue fds sync */
#define TAP_MP_KEY "tap_mp_sync_queues"

#define TAP_IOV_DEFAULT_MAX 1024

static int tap_devices_count;

static const char *valid_arguments[] = {
	ETH_TAP_IFACE_ARG,
	ETH_TAP_REMOTE_ARG,
	ETH_TAP_MAC_ARG,
	NULL
};

static volatile uint32_t tap_trigger;	/* Rx trigger */

static struct rte_eth_link pmd_link = {
	.link_speed = ETH_SPEED_NUM_10G,
	.link_duplex = ETH_LINK_FULL_DUPLEX,
	.link_status = ETH_LINK_DOWN,
	.link_autoneg = ETH_LINK_FIXED,
};

static void
tap_trigger_cb(int sig __rte_unused)
{
	/* Valid trigger values are nonzero */
	tap_trigger = (tap_trigger + 1) | 0x80000000;
}

/* Specifies on what netdevices the ioctl should be applied */
enum ioctl_mode {
	LOCAL_AND_REMOTE,
	LOCAL_ONLY,
	REMOTE_ONLY,
};

/* Message header to synchronize queues via IPC */
struct ipc_queues {
	char port_name[RTE_DEV_NAME_MAX_LEN];
	int rxq_count;
	int txq_count;
	/*
	 * The file descriptors are in the dedicated part
	 * of the Unix message to be translated by the kernel.
	 */
};

static int tap_intr_handle_set(struct rte_eth_dev *dev, int set);

/**
 * Tun/Tap allocation routine
 *
 * @param[in] pmd
 *   Pointer to private structure.
 *
 * @param[in] is_keepalive
 *   Keepalive flag
 *
 * @return
 *   -1 on failure, fd on success
 */
static int
tun_alloc(struct pmd_internals *pmd, int is_keepalive)
{
	struct ifreq ifr;
#ifdef IFF_MULTI_QUEUE
	unsigned int features;
#endif
	int fd;

	memset(&ifr, 0, sizeof(struct ifreq));

	/*
	 * Do not set IFF_NO_PI as packet information header will be needed
	 * to check if a received packet has been truncated.
	 */
	ifr.ifr_flags = (pmd->type == ETH_TUNTAP_TYPE_TAP) ?
		IFF_TAP : IFF_TUN | IFF_POINTOPOINT;
	strlcpy(ifr.ifr_name, pmd->name, IFNAMSIZ);

	fd = open(TUN_TAP_DEV_PATH, O_RDWR);
	if (fd < 0) {
		TAP_LOG(ERR, "Unable to open %s interface", TUN_TAP_DEV_PATH);
		goto error;
	}

#ifdef IFF_MULTI_QUEUE
	/* Grab the TUN features to verify we can work multi-queue */
	if (ioctl(fd, TUNGETFEATURES, &features) < 0) {
		TAP_LOG(ERR, "unable to get TUN/TAP features");
		goto error;
	}
	TAP_LOG(DEBUG, "%s Features %08x", TUN_TAP_DEV_PATH, features);

	if (features & IFF_MULTI_QUEUE) {
		TAP_LOG(DEBUG, "  Multi-queue support for %d queues",
			RTE_PMD_TAP_MAX_QUEUES);
		ifr.ifr_flags |= IFF_MULTI_QUEUE;
	} else
#endif
	{
		ifr.ifr_flags |= IFF_ONE_QUEUE;
		TAP_LOG(DEBUG, "  Single queue only support");
	}

	/* Set the TUN/TAP configuration and set the name if needed */
	if (ioctl(fd, TUNSETIFF, (void *)&ifr) < 0) {
		TAP_LOG(WARNING, "Unable to set TUNSETIFF for %s: %s",
			ifr.ifr_name, strerror(errno));
		goto error;
	}

	/*
	 * Name passed to kernel might be wildcard like dtun%d
	 * and need to find the resulting device.
	 */
	TAP_LOG(DEBUG, "Device name is '%s'", ifr.ifr_name);
	strlcpy(pmd->name, ifr.ifr_name, RTE_ETH_NAME_MAX_LEN);

	if (is_keepalive) {
		/*
		 * Detach the TUN/TAP keep-alive queue
		 * to avoid traffic through it
		 */
		ifr.ifr_flags = IFF_DETACH_QUEUE;
		if (ioctl(fd, TUNSETQUEUE, (void *)&ifr) < 0) {
			TAP_LOG(WARNING,
				"Unable to detach keep-alive queue for %s: %s",
				ifr.ifr_name, strerror(errno));
			goto error;
		}
	}

	/* Always set the file descriptor to non-blocking */
	if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0) {
		TAP_LOG(WARNING,
			"Unable to set %s to nonblocking: %s",
			ifr.ifr_name, strerror(errno));
		goto error;
	}

	/* Set up trigger to optimize empty Rx bursts */
	errno = 0;
	do {
		struct sigaction sa;
		int flags = fcntl(fd, F_GETFL);

		if (flags == -1 || sigaction(SIGIO, NULL, &sa) == -1)
			break;
		if (sa.sa_handler != tap_trigger_cb) {
			/*
			 * Make sure SIGIO is not already taken. This is done
			 * as late as possible to leave the application a
			 * chance to set up its own signal handler first.
			 */
			if (sa.sa_handler != SIG_IGN &&
			    sa.sa_handler != SIG_DFL) {
				errno = EBUSY;
				break;
			}
			sa = (struct sigaction){
				.sa_flags = SA_RESTART,
				.sa_handler = tap_trigger_cb,
			};
			if (sigaction(SIGIO, &sa, NULL) == -1)
				break;
		}
		/* Enable SIGIO on file descriptor */
		fcntl(fd, F_SETFL, flags | O_ASYNC);
		fcntl(fd, F_SETOWN, getpid());
	} while (0);

	if (errno) {
		/* Disable trigger globally in case of error */
		tap_trigger = 0;
		TAP_LOG(WARNING, "Rx trigger disabled: %s",
			strerror(errno));
	}

	return fd;

error:
	if (fd >= 0)
		close(fd);
	return -1;
}

static void
tap_verify_csum(struct rte_mbuf *mbuf)
{
	uint32_t l2 = mbuf->packet_type & RTE_PTYPE_L2_MASK;
	uint32_t l3 = mbuf->packet_type & RTE_PTYPE_L3_MASK;
	uint32_t l4 = mbuf->packet_type & RTE_PTYPE_L4_MASK;
	unsigned int l2_len = sizeof(struct rte_ether_hdr);
	unsigned int l3_len;
	uint16_t cksum = 0;
	void *l3_hdr;
	void *l4_hdr;

	if (l2 == RTE_PTYPE_L2_ETHER_VLAN)
		l2_len += 4;
	else if (l2 == RTE_PTYPE_L2_ETHER_QINQ)
		l2_len += 8;
	/* Don't verify checksum for packets with discontinuous L2 header */
	if (unlikely(l2_len + sizeof(struct rte_ipv4_hdr) >
		     rte_pktmbuf_data_len(mbuf)))
		return;
	l3_hdr = rte_pktmbuf_mtod_offset(mbuf, void *, l2_len);
	if (l3 == RTE_PTYPE_L3_IPV4 || l3 == RTE_PTYPE_L3_IPV4_EXT) {
		struct rte_ipv4_hdr *iph = l3_hdr;

		/* ihl contains the number of 4-byte words in the header */
		l3_len = 4 * (iph->version_ihl & 0xf);
		if (unlikely(l2_len + l3_len > rte_pktmbuf_data_len(mbuf)))
			return;
		/* check that the total length reported by header is not
		 * greater than the total received size
		 */
		if (l2_len + rte_be_to_cpu_16(iph->total_length) >
				rte_pktmbuf_data_len(mbuf))
			return;

		cksum = ~rte_raw_cksum(iph, l3_len);
		mbuf->ol_flags |= cksum ?
			PKT_RX_IP_CKSUM_BAD :
			PKT_RX_IP_CKSUM_GOOD;
	} else if (l3 == RTE_PTYPE_L3_IPV6) {
		struct rte_ipv6_hdr *iph = l3_hdr;

		l3_len = sizeof(struct rte_ipv6_hdr);
		/* check that the total length reported by header is not
		 * greater than the total received size
		 */
		if (l2_len + l3_len + rte_be_to_cpu_16(iph->payload_len) >
				rte_pktmbuf_data_len(mbuf))
			return;
	} else {
		/* IPv6 extensions are not supported */
		return;
	}
	if (l4 == RTE_PTYPE_L4_UDP || l4 == RTE_PTYPE_L4_TCP) {
		l4_hdr = rte_pktmbuf_mtod_offset(mbuf, void *, l2_len + l3_len);
		/* Don't verify checksum for multi-segment packets. */
		if (mbuf->nb_segs > 1)
			return;
		if (l3 == RTE_PTYPE_L3_IPV4)
			cksum = ~rte_ipv4_udptcp_cksum(l3_hdr, l4_hdr);
		else if (l3 == RTE_PTYPE_L3_IPV6)
			cksum = ~rte_ipv6_udptcp_cksum(l3_hdr, l4_hdr);
		mbuf->ol_flags |= cksum ?
			PKT_RX_L4_CKSUM_BAD :
			PKT_RX_L4_CKSUM_GOOD;
	}
}

static uint64_t
tap_rx_offload_get_port_capa(void)
{
	/*
	 * No specific port Rx offload capabilities.
	 */
	return 0;
}

static uint64_t
tap_rx_offload_get_queue_capa(void)
{
	return DEV_RX_OFFLOAD_SCATTER |
	       DEV_RX_OFFLOAD_IPV4_CKSUM |
	       DEV_RX_OFFLOAD_UDP_CKSUM |
	       DEV_RX_OFFLOAD_TCP_CKSUM;
}

/* Callback to handle the rx burst of packets to the correct interface and
 * file descriptor(s) in a multi-queue setup.
 */
static uint16_t
pmd_rx_burst(void *queue, struct rte_mbuf **bufs, uint16_t nb_pkts)
{
	struct rx_queue *rxq = queue;
	struct pmd_process_private *process_private;
	uint16_t num_rx;
	unsigned long num_rx_bytes = 0;
	uint32_t trigger = tap_trigger;

	if (trigger == rxq->trigger_seen)
		return 0;

	process_private = rte_eth_devices[rxq->in_port].process_private;
	for (num_rx = 0; num_rx < nb_pkts; ) {
		struct rte_mbuf *mbuf = rxq->pool;
		struct rte_mbuf *seg = NULL;
		struct rte_mbuf *new_tail = NULL;
		uint16_t data_off = rte_pktmbuf_headroom(mbuf);
		int len;

		len = readv(process_private->rxq_fds[rxq->queue_id],
			*rxq->iovecs,
			1 + (rxq->rxmode->offloads & DEV_RX_OFFLOAD_SCATTER ?
			     rxq->nb_rx_desc : 1));
		if (len < (int)sizeof(struct tun_pi))
			break;

		/* Packet couldn't fit in the provided mbuf */
		if (unlikely(rxq->pi.flags & TUN_PKT_STRIP)) {
			rxq->stats.ierrors++;
			continue;
		}

		len -= sizeof(struct tun_pi);

		mbuf->pkt_len = len;
		mbuf->port = rxq->in_port;
		while (1) {
			struct rte_mbuf *buf = rte_pktmbuf_alloc(rxq->mp);

			if (unlikely(!buf)) {
				rxq->stats.rx_nombuf++;
				/* No new buf has been allocated: do nothing */
				if (!new_tail || !seg)
					goto end;

				seg->next = NULL;
				rte_pktmbuf_free(mbuf);

				goto end;
			}
			seg = seg ? seg->next : mbuf;
			if (rxq->pool == mbuf)
				rxq->pool = buf;
			if (new_tail)
				new_tail->next = buf;
			new_tail = buf;
			new_tail->next = seg->next;

			/* iovecs[0] is reserved for packet info (pi) */
			(*rxq->iovecs)[mbuf->nb_segs].iov_len =
				buf->buf_len - data_off;
			(*rxq->iovecs)[mbuf->nb_segs].iov_base =
				(char *)buf->buf_addr + data_off;

			seg->data_len = RTE_MIN(seg->buf_len - data_off, len);
			seg->data_off = data_off;

			len -= seg->data_len;
			if (len <= 0)
				break;
			mbuf->nb_segs++;
			/* First segment has headroom, not the others */
			data_off = 0;
		}
		seg->next = NULL;
		mbuf->packet_type = rte_net_get_ptype(mbuf, NULL,
						      RTE_PTYPE_ALL_MASK);
		if (rxq->rxmode->offloads & DEV_RX_OFFLOAD_CHECKSUM)
			tap_verify_csum(mbuf);

		/* account for the receive frame */
		bufs[num_rx++] = mbuf;
		num_rx_bytes += mbuf->pkt_len;
	}
end:
	rxq->stats.ipackets += num_rx;
	rxq->stats.ibytes += num_rx_bytes;

	if (trigger && num_rx < nb_pkts)
		rxq->trigger_seen = trigger;

	return num_rx;
}

static uint64_t
tap_tx_offload_get_port_capa(void)
{
	/*
	 * No specific port Tx offload capabilities.
	 */
	return 0;
}

static uint64_t
tap_tx_offload_get_queue_capa(void)
{
	return DEV_TX_OFFLOAD_MULTI_SEGS |
	       DEV_TX_OFFLOAD_IPV4_CKSUM |
	       DEV_TX_OFFLOAD_UDP_CKSUM |
	       DEV_TX_OFFLOAD_TCP_CKSUM |
	       DEV_TX_OFFLOAD_TCP_TSO;
}

/* Finalize l4 checksum calculation */
static void
tap_tx_l4_cksum(uint16_t *l4_cksum, uint16_t l4_phdr_cksum,
		uint32_t l4_raw_cksum)
{
	if (l4_cksum) {
		uint32_t cksum;

		cksum = __rte_raw_cksum_reduce(l4_raw_cksum);
		cksum += l4_phdr_cksum;

		cksum = ((cksum & 0xffff0000) >> 16) + (cksum & 0xffff);
		cksum = (~cksum) & 0xffff;
		if (cksum == 0)
			cksum = 0xffff;
		*l4_cksum = cksum;
	}
}

/* Accumaulate L4 raw checksums */
static void
tap_tx_l4_add_rcksum(char *l4_data, unsigned int l4_len, uint16_t *l4_cksum,
			uint32_t *l4_raw_cksum)
{
	if (l4_cksum == NULL)
		return;

	*l4_raw_cksum = __rte_raw_cksum(l4_data, l4_len, *l4_raw_cksum);
}

/* L3 and L4 pseudo headers checksum offloads */
static void
tap_tx_l3_cksum(char *packet, uint64_t ol_flags, unsigned int l2_len,
		unsigned int l3_len, unsigned int l4_len, uint16_t **l4_cksum,
		uint16_t *l4_phdr_cksum, uint32_t *l4_raw_cksum)
{
	void *l3_hdr = packet + l2_len;

	if (ol_flags & (PKT_TX_IP_CKSUM | PKT_TX_IPV4)) {
		struct rte_ipv4_hdr *iph = l3_hdr;
		uint16_t cksum;

		iph->hdr_checksum = 0;
		cksum = rte_raw_cksum(iph, l3_len);
		iph->hdr_checksum = (cksum == 0xffff) ? cksum : ~cksum;
	}
	if (ol_flags & PKT_TX_L4_MASK) {
		void *l4_hdr;

		l4_hdr = packet + l2_len + l3_len;
		if ((ol_flags & PKT_TX_L4_MASK) == PKT_TX_UDP_CKSUM)
			*l4_cksum = &((struct rte_udp_hdr *)l4_hdr)->dgram_cksum;
		else if ((ol_flags & PKT_TX_L4_MASK) == PKT_TX_TCP_CKSUM)
			*l4_cksum = &((struct rte_tcp_hdr *)l4_hdr)->cksum;
		else
			return;
		**l4_cksum = 0;
		if (ol_flags & PKT_TX_IPV4)
			*l4_phdr_cksum = rte_ipv4_phdr_cksum(l3_hdr, 0);
		else
			*l4_phdr_cksum = rte_ipv6_phdr_cksum(l3_hdr, 0);
		*l4_raw_cksum = __rte_raw_cksum(l4_hdr, l4_len, 0);
	}
}

static inline int
tap_write_mbufs(struct tx_queue *txq, uint16_t num_mbufs,
			struct rte_mbuf **pmbufs,
			uint16_t *num_packets, unsigned long *num_tx_bytes)
{
	int i;
	uint16_t l234_hlen;
	struct pmd_process_private *process_private;

	process_private = rte_eth_devices[txq->out_port].process_private;

	for (i = 0; i < num_mbufs; i++) {
		struct rte_mbuf *mbuf = pmbufs[i];
		struct iovec iovecs[mbuf->nb_segs + 2];
		struct tun_pi pi = { .flags = 0, .proto = 0x00 };
		struct rte_mbuf *seg = mbuf;
		char m_copy[mbuf->data_len];
		int proto;
		int n;
		int j;
		int k; /* current index in iovecs for copying segments */
		uint16_t seg_len; /* length of first segment */
		uint16_t nb_segs;
		uint16_t *l4_cksum; /* l4 checksum (pseudo header + payload) */
		uint32_t l4_raw_cksum = 0; /* TCP/UDP payload raw checksum */
		uint16_t l4_phdr_cksum = 0; /* TCP/UDP pseudo header checksum */
		uint16_t is_cksum = 0; /* in case cksum should be offloaded */

		l4_cksum = NULL;
		if (txq->type == ETH_TUNTAP_TYPE_TUN) {
			/*
			 * TUN and TAP are created with IFF_NO_PI disabled.
			 * For TUN PMD this mandatory as fields are used by
			 * Kernel tun.c to determine whether its IP or non IP
			 * packets.
			 *
			 * The logic fetches the first byte of data from mbuf
			 * then compares whether its v4 or v6. If first byte
			 * is 4 or 6, then protocol field is updated.
			 */
			char *buff_data = rte_pktmbuf_mtod(seg, void *);
			proto = (*buff_data & 0xf0);
			pi.proto = (proto == 0x40) ?
				rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4) :
				((proto == 0x60) ?
					rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV6) :
					0x00);
		}

		k = 0;
		iovecs[k].iov_base = &pi;
		iovecs[k].iov_len = sizeof(pi);
		k++;

		nb_segs = mbuf->nb_segs;
		if (txq->csum &&
		    ((mbuf->ol_flags & (PKT_TX_IP_CKSUM | PKT_TX_IPV4) ||
		     (mbuf->ol_flags & PKT_TX_L4_MASK) == PKT_TX_UDP_CKSUM ||
		     (mbuf->ol_flags & PKT_TX_L4_MASK) == PKT_TX_TCP_CKSUM))) {
			is_cksum = 1;

			/* Support only packets with at least layer 4
			 * header included in the first segment
			 */
			seg_len = rte_pktmbuf_data_len(mbuf);
			l234_hlen = mbuf->l2_len + mbuf->l3_len + mbuf->l4_len;
			if (seg_len < l234_hlen)
				return -1;

			/* To change checksums, work on a * copy of l2, l3
			 * headers + l4 pseudo header
			 */
			rte_memcpy(m_copy, rte_pktmbuf_mtod(mbuf, void *),
					l234_hlen);
			tap_tx_l3_cksum(m_copy, mbuf->ol_flags,
				       mbuf->l2_len, mbuf->l3_len, mbuf->l4_len,
				       &l4_cksum, &l4_phdr_cksum,
				       &l4_raw_cksum);
			iovecs[k].iov_base = m_copy;
			iovecs[k].iov_len = l234_hlen;
			k++;

			/* Update next iovecs[] beyond l2, l3, l4 headers */
			if (seg_len > l234_hlen) {
				iovecs[k].iov_len = seg_len - l234_hlen;
				iovecs[k].iov_base =
					rte_pktmbuf_mtod(seg, char *) +
						l234_hlen;
				tap_tx_l4_add_rcksum(iovecs[k].iov_base,
					iovecs[k].iov_len, l4_cksum,
					&l4_raw_cksum);
				k++;
				nb_segs++;
			}
			seg = seg->next;
		}

		for (j = k; j <= nb_segs; j++) {
			iovecs[j].iov_len = rte_pktmbuf_data_len(seg);
			iovecs[j].iov_base = rte_pktmbuf_mtod(seg, void *);
			if (is_cksum)
				tap_tx_l4_add_rcksum(iovecs[j].iov_base,
					iovecs[j].iov_len, l4_cksum,
					&l4_raw_cksum);
			seg = seg->next;
		}

		if (is_cksum)
			tap_tx_l4_cksum(l4_cksum, l4_phdr_cksum, l4_raw_cksum);

		/* copy the tx frame data */
		n = writev(process_private->txq_fds[txq->queue_id], iovecs, j);
		if (n <= 0)
			return -1;

		(*num_packets)++;
		(*num_tx_bytes) += rte_pktmbuf_pkt_len(mbuf);
	}
	return 0;
}

/* Callback to handle sending packets from the tap interface
 */
static uint16_t
pmd_tx_burst(void *queue, struct rte_mbuf **bufs, uint16_t nb_pkts)
{
	struct tx_queue *txq = queue;
	uint16_t num_tx = 0;
	uint16_t num_packets = 0;
	unsigned long num_tx_bytes = 0;
	uint32_t max_size;
	int i;

	if (unlikely(nb_pkts == 0))
		return 0;

	struct rte_mbuf *gso_mbufs[MAX_GSO_MBUFS];
	max_size = *txq->mtu + (RTE_ETHER_HDR_LEN + RTE_ETHER_CRC_LEN + 4);
	for (i = 0; i < nb_pkts; i++) {
		struct rte_mbuf *mbuf_in = bufs[num_tx];
		struct rte_mbuf **mbuf;
		uint16_t num_mbufs = 0;
		uint16_t tso_segsz = 0;
		int ret;
		int num_tso_mbufs;
		uint16_t hdrs_len;
		uint64_t tso;

		tso = mbuf_in->ol_flags & PKT_TX_TCP_SEG;
		if (tso) {
			struct rte_gso_ctx *gso_ctx = &txq->gso_ctx;

			/* TCP segmentation implies TCP checksum offload */
			mbuf_in->ol_flags |= PKT_TX_TCP_CKSUM;

			/* gso size is calculated without RTE_ETHER_CRC_LEN */
			hdrs_len = mbuf_in->l2_len + mbuf_in->l3_len +
					mbuf_in->l4_len;
			tso_segsz = mbuf_in->tso_segsz + hdrs_len;
			if (unlikely(tso_segsz == hdrs_len) ||
				tso_segsz > *txq->mtu) {
				txq->stats.errs++;
				break;
			}
			gso_ctx->gso_size = tso_segsz;
			/* 'mbuf_in' packet to segment */
			num_tso_mbufs = rte_gso_segment(mbuf_in,
				gso_ctx, /* gso control block */
				(struct rte_mbuf **)&gso_mbufs, /* out mbufs */
				RTE_DIM(gso_mbufs)); /* max tso mbufs */

			/* ret contains the number of new created mbufs */
			if (num_tso_mbufs < 0)
				break;

			mbuf = gso_mbufs;
			num_mbufs = num_tso_mbufs;
		} else {
			/* stats.errs will be incremented */
			if (rte_pktmbuf_pkt_len(mbuf_in) > max_size)
				break;

			/* ret 0 indicates no new mbufs were created */
			num_tso_mbufs = 0;
			mbuf = &mbuf_in;
			num_mbufs = 1;
		}

		ret = tap_write_mbufs(txq, num_mbufs, mbuf,
				&num_packets, &num_tx_bytes);
		if (ret == -1) {
			txq->stats.errs++;
			/* free tso mbufs */
			if (num_tso_mbufs > 0)
				rte_pktmbuf_free_bulk(mbuf, num_tso_mbufs);
			break;
		}
		num_tx++;
		/* free original mbuf */
		rte_pktmbuf_free(mbuf_in);
		/* free tso mbufs */
		if (num_tso_mbufs > 0)
			rte_pktmbuf_free_bulk(mbuf, num_tso_mbufs);
	}

	txq->stats.opackets += num_packets;
	txq->stats.errs += nb_pkts - num_tx;
	txq->stats.obytes += num_tx_bytes;

	return num_tx;
}

static const char *
tap_ioctl_req2str(unsigned long request)
{
	switch (request) {
	case SIOCSIFFLAGS:
		return "SIOCSIFFLAGS";
	case SIOCGIFFLAGS:
		return "SIOCGIFFLAGS";
	case SIOCGIFHWADDR:
		return "SIOCGIFHWADDR";
	case SIOCSIFHWADDR:
		return "SIOCSIFHWADDR";
	case SIOCSIFMTU:
		return "SIOCSIFMTU";
	}
	return "UNKNOWN";
}

static int
tap_ioctl(struct pmd_internals *pmd, unsigned long request,
	  struct ifreq *ifr, int set, enum ioctl_mode mode)
{
	short req_flags = ifr->ifr_flags;
	int remote = pmd->remote_if_index &&
		(mode == REMOTE_ONLY || mode == LOCAL_AND_REMOTE);

	if (!pmd->remote_if_index && mode == REMOTE_ONLY)
		return 0;
	/*
	 * If there is a remote netdevice, apply ioctl on it, then apply it on
	 * the tap netdevice.
	 */
apply:
	if (remote)
		strlcpy(ifr->ifr_name, pmd->remote_iface, IFNAMSIZ);
	else if (mode == LOCAL_ONLY || mode == LOCAL_AND_REMOTE)
		strlcpy(ifr->ifr_name, pmd->name, IFNAMSIZ);
	switch (request) {
	case SIOCSIFFLAGS:
		/* fetch current flags to leave other flags untouched */
		if (ioctl(pmd->ioctl_sock, SIOCGIFFLAGS, ifr) < 0)
			goto error;
		if (set)
			ifr->ifr_flags |= req_flags;
		else
			ifr->ifr_flags &= ~req_flags;
		break;
	case SIOCGIFFLAGS:
	case SIOCGIFHWADDR:
	case SIOCSIFHWADDR:
	case SIOCSIFMTU:
		break;
	default:
		TAP_LOG(WARNING, "%s: ioctl() called with wrong arg",
			pmd->name);
		return -EINVAL;
	}
	if (ioctl(pmd->ioctl_sock, request, ifr) < 0)
		goto error;
	if (remote-- && mode == LOCAL_AND_REMOTE)
		goto apply;
	return 0;

error:
	TAP_LOG(DEBUG, "%s(%s) failed: %s(%d)", ifr->ifr_name,
		tap_ioctl_req2str(request), strerror(errno), errno);
	return -errno;
}

static int
tap_link_set_down(struct rte_eth_dev *dev)
{
	struct pmd_internals *pmd = dev->data->dev_private;
	struct ifreq ifr = { .ifr_flags = IFF_UP };

	dev->data->dev_link.link_status = ETH_LINK_DOWN;
	return tap_ioctl(pmd, SIOCSIFFLAGS, &ifr, 0, LOCAL_ONLY);
}

static int
tap_link_set_up(struct rte_eth_dev *dev)
{
	struct pmd_internals *pmd = dev->data->dev_private;
	struct ifreq ifr = { .ifr_flags = IFF_UP };

	dev->data->dev_link.link_status = ETH_LINK_UP;
	return tap_ioctl(pmd, SIOCSIFFLAGS, &ifr, 1, LOCAL_AND_REMOTE);
}

static int
tap_dev_start(struct rte_eth_dev *dev)
{
	int err, i;

	err = tap_intr_handle_set(dev, 1);
	if (err)
		return err;

	err = tap_link_set_up(dev);
	if (err)
		return err;

	for (i = 0; i < dev->data->nb_tx_queues; i++)
		dev->data->tx_queue_state[i] = RTE_ETH_QUEUE_STATE_STARTED;
	for (i = 0; i < dev->data->nb_rx_queues; i++)
		dev->data->rx_queue_state[i] = RTE_ETH_QUEUE_STATE_STARTED;

	return err;
}

/* This function gets called when the current port gets stopped.
 */
static void
tap_dev_stop(struct rte_eth_dev *dev)
{
	int i;

	for (i = 0; i < dev->data->nb_tx_queues; i++)
		dev->data->tx_queue_state[i] = RTE_ETH_QUEUE_STATE_STOPPED;
	for (i = 0; i < dev->data->nb_rx_queues; i++)
		dev->data->rx_queue_state[i] = RTE_ETH_QUEUE_STATE_STOPPED;

	tap_intr_handle_set(dev, 0);
	tap_link_set_down(dev);
}

static int
tap_dev_configure(struct rte_eth_dev *dev)
{
	struct pmd_internals *pmd = dev->data->dev_private;

	if (dev->data->nb_rx_queues > RTE_PMD_TAP_MAX_QUEUES) {
		TAP_LOG(ERR,
			"%s: number of rx queues %d exceeds max num of queues %d",
			dev->device->name,
			dev->data->nb_rx_queues,
			RTE_PMD_TAP_MAX_QUEUES);
		return -1;
	}
	if (dev->data->nb_tx_queues > RTE_PMD_TAP_MAX_QUEUES) {
		TAP_LOG(ERR,
			"%s: number of tx queues %d exceeds max num of queues %d",
			dev->device->name,
			dev->data->nb_tx_queues,
			RTE_PMD_TAP_MAX_QUEUES);
		return -1;
	}

	TAP_LOG(INFO, "%s: %s: TX configured queues number: %u",
		dev->device->name, pmd->name, dev->data->nb_tx_queues);

	TAP_LOG(INFO, "%s: %s: RX configured queues number: %u",
		dev->device->name, pmd->name, dev->data->nb_rx_queues);

	return 0;
}

static uint32_t
tap_dev_speed_capa(void)
{
	uint32_t speed = pmd_link.link_speed;
	uint32_t capa = 0;

	if (speed >= ETH_SPEED_NUM_10M)
		capa |= ETH_LINK_SPEED_10M;
	if (speed >= ETH_SPEED_NUM_100M)
		capa |= ETH_LINK_SPEED_100M;
	if (speed >= ETH_SPEED_NUM_1G)
		capa |= ETH_LINK_SPEED_1G;
	if (speed >= ETH_SPEED_NUM_5G)
		capa |= ETH_LINK_SPEED_2_5G;
	if (speed >= ETH_SPEED_NUM_5G)
		capa |= ETH_LINK_SPEED_5G;
	if (speed >= ETH_SPEED_NUM_10G)
		capa |= ETH_LINK_SPEED_10G;
	if (speed >= ETH_SPEED_NUM_20G)
		capa |= ETH_LINK_SPEED_20G;
	if (speed >= ETH_SPEED_NUM_25G)
		capa |= ETH_LINK_SPEED_25G;
	if (speed >= ETH_SPEED_NUM_40G)
		capa |= ETH_LINK_SPEED_40G;
	if (speed >= ETH_SPEED_NUM_50G)
		capa |= ETH_LINK_SPEED_50G;
	if (speed >= ETH_SPEED_NUM_56G)
		capa |= ETH_LINK_SPEED_56G;
	if (speed >= ETH_SPEED_NUM_100G)
		capa |= ETH_LINK_SPEED_100G;

	return capa;
}

static int
tap_dev_info(struct rte_eth_dev *dev, struct rte_eth_dev_info *dev_info)
{
	struct pmd_internals *internals = dev->data->dev_private;

	dev_info->if_index = internals->if_index;
	dev_info->max_mac_addrs = 1;
	dev_info->max_rx_pktlen = (uint32_t)RTE_ETHER_MAX_VLAN_FRAME_LEN;
	dev_info->max_rx_queues = RTE_PMD_TAP_MAX_QUEUES;
	dev_info->max_tx_queues = RTE_PMD_TAP_MAX_QUEUES;
	dev_info->min_rx_bufsize = 0;
	dev_info->speed_capa = tap_dev_speed_capa();
	dev_info->rx_queue_offload_capa = tap_rx_offload_get_queue_capa();
	dev_info->rx_offload_capa = tap_rx_offload_get_port_capa() |
				    dev_info->rx_queue_offload_capa;
	dev_info->tx_queue_offload_capa = tap_tx_offload_get_queue_capa();
	dev_info->tx_offload_capa = tap_tx_offload_get_port_capa() |
				    dev_info->tx_queue_offload_capa;
	dev_info->hash_key_size = TAP_RSS_HASH_KEY_SIZE;
	/*
	 * limitation: TAP supports all of IP, UDP and TCP hash
	 * functions together and not in partial combinations
	 */
	dev_info->flow_type_rss_offloads = ~TAP_RSS_HF_MASK;

	return 0;
}

static int
tap_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *tap_stats)
{
	unsigned int i, imax;
	unsigned long rx_total = 0, tx_total = 0, tx_err_total = 0;
	unsigned long rx_bytes_total = 0, tx_bytes_total = 0;
	unsigned long rx_nombuf = 0, ierrors = 0;
	const struct pmd_internals *pmd = dev->data->dev_private;

	/* rx queue statistics */
	imax = (dev->data->nb_rx_queues < RTE_ETHDEV_QUEUE_STAT_CNTRS) ?
		dev->data->nb_rx_queues : RTE_ETHDEV_QUEUE_STAT_CNTRS;
	for (i = 0; i < imax; i++) {
		tap_stats->q_ipackets[i] = pmd->rxq[i].stats.ipackets;
		tap_stats->q_ibytes[i] = pmd->rxq[i].stats.ibytes;
		rx_total += tap_stats->q_ipackets[i];
		rx_bytes_total += tap_stats->q_ibytes[i];
		rx_nombuf += pmd->rxq[i].stats.rx_nombuf;
		ierrors += pmd->rxq[i].stats.ierrors;
	}

	/* tx queue statistics */
	imax = (dev->data->nb_tx_queues < RTE_ETHDEV_QUEUE_STAT_CNTRS) ?
		dev->data->nb_tx_queues : RTE_ETHDEV_QUEUE_STAT_CNTRS;

	for (i = 0; i < imax; i++) {
		tap_stats->q_opackets[i] = pmd->txq[i].stats.opackets;
		tap_stats->q_obytes[i] = pmd->txq[i].stats.obytes;
		tx_total += tap_stats->q_opackets[i];
		tx_err_total += pmd->txq[i].stats.errs;
		tx_bytes_total += tap_stats->q_obytes[i];
	}

	tap_stats->ipackets = rx_total;
	tap_stats->ibytes = rx_bytes_total;
	tap_stats->ierrors = ierrors;
	tap_stats->rx_nombuf = rx_nombuf;
	tap_stats->opackets = tx_total;
	tap_stats->oerrors = tx_err_total;
	tap_stats->obytes = tx_bytes_total;
	return 0;
}

static int
tap_stats_reset(struct rte_eth_dev *dev)
{
	int i;
	struct pmd_internals *pmd = dev->data->dev_private;

	for (i = 0; i < RTE_PMD_TAP_MAX_QUEUES; i++) {
		pmd->rxq[i].stats.ipackets = 0;
		pmd->rxq[i].stats.ibytes = 0;
		pmd->rxq[i].stats.ierrors = 0;
		pmd->rxq[i].stats.rx_nombuf = 0;

		pmd->txq[i].stats.opackets = 0;
		pmd->txq[i].stats.errs = 0;
		pmd->txq[i].stats.obytes = 0;
	}

	return 0;
}

static void
tap_dev_close(struct rte_eth_dev *dev)
{
	int i;
	struct pmd_internals *internals = dev->data->dev_private;
	struct pmd_process_private *process_private = dev->process_private;
	struct rx_queue *rxq;

	tap_link_set_down(dev);
	if (internals->nlsk_fd != -1) {
		tap_flow_flush(dev, NULL);
		tap_flow_implicit_flush(internals, NULL);
		tap_nl_final(internals->nlsk_fd);
		internals->nlsk_fd = -1;
	}

	for (i = 0; i < RTE_PMD_TAP_MAX_QUEUES; i++) {
		if (process_private->rxq_fds[i] != -1) {
			rxq = &internals->rxq[i];
			close(process_private->rxq_fds[i]);
			process_private->rxq_fds[i] = -1;
			rte_pktmbuf_free(rxq->pool);
			rte_free(rxq->iovecs);
			rxq->pool = NULL;
			rxq->iovecs = NULL;
		}
		if (process_private->txq_fds[i] != -1) {
			close(process_private->txq_fds[i]);
			process_private->txq_fds[i] = -1;
		}
	}

	if (internals->remote_if_index) {
		/* Restore initial remote state */
		ioctl(internals->ioctl_sock, SIOCSIFFLAGS,
				&internals->remote_initial_flags);
	}

	if (internals->ka_fd != -1) {
		close(internals->ka_fd);
		internals->ka_fd = -1;
	}
	/*
	 * Since TUN device has no more opened file descriptors
	 * it will be removed from kernel
	 */
}

static void
tap_rx_queue_release(void *queue)
{
	struct rx_queue *rxq = queue;
	struct pmd_process_private *process_private;

	if (!rxq)
		return;
	process_private = rte_eth_devices[rxq->in_port].process_private;
	if (process_private->rxq_fds[rxq->queue_id] > 0) {
		close(process_private->rxq_fds[rxq->queue_id]);
		process_private->rxq_fds[rxq->queue_id] = -1;
		rte_pktmbuf_free(rxq->pool);
		rte_free(rxq->iovecs);
		rxq->pool = NULL;
		rxq->iovecs = NULL;
	}
}

static void
tap_tx_queue_release(void *queue)
{
	struct tx_queue *txq = queue;
	struct pmd_process_private *process_private;

	if (!txq)
		return;
	process_private = rte_eth_devices[txq->out_port].process_private;

	if (process_private->txq_fds[txq->queue_id] > 0) {
		close(process_private->txq_fds[txq->queue_id]);
		process_private->txq_fds[txq->queue_id] = -1;
	}
}

static int
tap_link_update(struct rte_eth_dev *dev, int wait_to_complete __rte_unused)
{
	struct rte_eth_link *dev_link = &dev->data->dev_link;
	struct pmd_internals *pmd = dev->data->dev_private;
	struct ifreq ifr = { .ifr_flags = 0 };

	if (pmd->remote_if_index) {
		tap_ioctl(pmd, SIOCGIFFLAGS, &ifr, 0, REMOTE_ONLY);
		if (!(ifr.ifr_flags & IFF_UP) ||
		    !(ifr.ifr_flags & IFF_RUNNING)) {
			dev_link->link_status = ETH_LINK_DOWN;
			return 0;
		}
	}
	tap_ioctl(pmd, SIOCGIFFLAGS, &ifr, 0, LOCAL_ONLY);
	dev_link->link_status =
		((ifr.ifr_flags & IFF_UP) && (ifr.ifr_flags & IFF_RUNNING) ?
		 ETH_LINK_UP :
		 ETH_LINK_DOWN);
	return 0;
}

static int
tap_promisc_enable(struct rte_eth_dev *dev)
{
	struct pmd_internals *pmd = dev->data->dev_private;
	struct ifreq ifr = { .ifr_flags = IFF_PROMISC };
	int ret;

	ret = tap_ioctl(pmd, SIOCSIFFLAGS, &ifr, 1, LOCAL_AND_REMOTE);
	if (ret != 0)
		return ret;

	if (pmd->remote_if_index && !pmd->flow_isolate) {
		dev->data->promiscuous = 1;
		ret = tap_flow_implicit_create(pmd, TAP_REMOTE_PROMISC);
		if (ret != 0) {
			/* Rollback promisc flag */
			tap_ioctl(pmd, SIOCSIFFLAGS, &ifr, 0, LOCAL_AND_REMOTE);
			/*
			 * rte_eth_dev_promiscuous_enable() rollback
			 * dev->data->promiscuous in the case of failure.
			 */
			return ret;
		}
	}

	return 0;
}

static int
tap_promisc_disable(struct rte_eth_dev *dev)
{
	struct pmd_internals *pmd = dev->data->dev_private;
	struct ifreq ifr = { .ifr_flags = IFF_PROMISC };
	int ret;

	ret = tap_ioctl(pmd, SIOCSIFFLAGS, &ifr, 0, LOCAL_AND_REMOTE);
	if (ret != 0)
		return ret;

	if (pmd->remote_if_index && !pmd->flow_isolate) {
		dev->data->promiscuous = 0;
		ret = tap_flow_implicit_destroy(pmd, TAP_REMOTE_PROMISC);
		if (ret != 0) {
			/* Rollback promisc flag */
			tap_ioctl(pmd, SIOCSIFFLAGS, &ifr, 1, LOCAL_AND_REMOTE);
			/*
			 * rte_eth_dev_promiscuous_disable() rollback
			 * dev->data->promiscuous in the case of failure.
			 */
			return ret;
		}
	}

	return 0;
}

static int
tap_allmulti_enable(struct rte_eth_dev *dev)
{
	struct pmd_internals *pmd = dev->data->dev_private;
	struct ifreq ifr = { .ifr_flags = IFF_ALLMULTI };
	int ret;

	ret = tap_ioctl(pmd, SIOCSIFFLAGS, &ifr, 1, LOCAL_AND_REMOTE);
	if (ret != 0)
		return ret;

	if (pmd->remote_if_index && !pmd->flow_isolate) {
		dev->data->all_multicast = 1;
		ret = tap_flow_implicit_create(pmd, TAP_REMOTE_ALLMULTI);
		if (ret != 0) {
			/* Rollback allmulti flag */
			tap_ioctl(pmd, SIOCSIFFLAGS, &ifr, 0, LOCAL_AND_REMOTE);
			/*
			 * rte_eth_dev_allmulticast_enable() rollback
			 * dev->data->all_multicast in the case of failure.
			 */
			return ret;
		}
	}

	return 0;
}

static int
tap_allmulti_disable(struct rte_eth_dev *dev)
{
	struct pmd_internals *pmd = dev->data->dev_private;
	struct ifreq ifr = { .ifr_flags = IFF_ALLMULTI };
	int ret;

	ret = tap_ioctl(pmd, SIOCSIFFLAGS, &ifr, 0, LOCAL_AND_REMOTE);
	if (ret != 0)
		return ret;

	if (pmd->remote_if_index && !pmd->flow_isolate) {
		dev->data->all_multicast = 0;
		ret = tap_flow_implicit_destroy(pmd, TAP_REMOTE_ALLMULTI);
		if (ret != 0) {
			/* Rollback allmulti flag */
			tap_ioctl(pmd, SIOCSIFFLAGS, &ifr, 1, LOCAL_AND_REMOTE);
			/*
			 * rte_eth_dev_allmulticast_disable() rollback
			 * dev->data->all_multicast in the case of failure.
			 */
			return ret;
		}
	}

	return 0;
}

static int
tap_mac_set(struct rte_eth_dev *dev, struct rte_ether_addr *mac_addr)
{
	struct pmd_internals *pmd = dev->data->dev_private;
	enum ioctl_mode mode = LOCAL_ONLY;
	struct ifreq ifr;
	int ret;

	if (pmd->type == ETH_TUNTAP_TYPE_TUN) {
		TAP_LOG(ERR, "%s: can't MAC address for TUN",
			dev->device->name);
		return -ENOTSUP;
	}

	if (rte_is_zero_ether_addr(mac_addr)) {
		TAP_LOG(ERR, "%s: can't set an empty MAC address",
			dev->device->name);
		return -EINVAL;
	}
	/* Check the actual current MAC address on the tap netdevice */
	ret = tap_ioctl(pmd, SIOCGIFHWADDR, &ifr, 0, LOCAL_ONLY);
	if (ret < 0)
		return ret;
	if (rte_is_same_ether_addr(
			(struct rte_ether_addr *)&ifr.ifr_hwaddr.sa_data,
			mac_addr))
		return 0;
	/* Check the current MAC address on the remote */
	ret = tap_ioctl(pmd, SIOCGIFHWADDR, &ifr, 0, REMOTE_ONLY);
	if (ret < 0)
		return ret;
	if (!rte_is_same_ether_addr(
			(struct rte_ether_addr *)&ifr.ifr_hwaddr.sa_data,
			mac_addr))
		mode = LOCAL_AND_REMOTE;
	ifr.ifr_hwaddr.sa_family = AF_LOCAL;
	rte_memcpy(ifr.ifr_hwaddr.sa_data, mac_addr, RTE_ETHER_ADDR_LEN);
	ret = tap_ioctl(pmd, SIOCSIFHWADDR, &ifr, 1, mode);
	if (ret < 0)
		return ret;
	rte_memcpy(&pmd->eth_addr, mac_addr, RTE_ETHER_ADDR_LEN);
	if (pmd->remote_if_index && !pmd->flow_isolate) {
		/* Replace MAC redirection rule after a MAC change */
		ret = tap_flow_implicit_destroy(pmd, TAP_REMOTE_LOCAL_MAC);
		if (ret < 0) {
			TAP_LOG(ERR,
				"%s: Couldn't delete MAC redirection rule",
				dev->device->name);
			return ret;
		}
		ret = tap_flow_implicit_create(pmd, TAP_REMOTE_LOCAL_MAC);
		if (ret < 0) {
			TAP_LOG(ERR,
				"%s: Couldn't add MAC redirection rule",
				dev->device->name);
			return ret;
		}
	}

	return 0;
}

static int
tap_gso_ctx_setup(struct rte_gso_ctx *gso_ctx, struct rte_eth_dev *dev)
{
	uint32_t gso_types;
	char pool_name[64];

	/*
	 * Create private mbuf pool with TAP_GSO_MBUF_SEG_SIZE bytes
	 * size per mbuf use this pool for both direct and indirect mbufs
	 */

	struct rte_mempool *mp;      /* Mempool for GSO packets */

	/* initialize GSO context */
	gso_types = DEV_TX_OFFLOAD_TCP_TSO;
	snprintf(pool_name, sizeof(pool_name), "mp_%s", dev->device->name);
	mp = rte_mempool_lookup((const char *)pool_name);
	if (!mp) {
		mp = rte_pktmbuf_pool_create(pool_name, TAP_GSO_MBUFS_NUM,
			TAP_GSO_MBUF_CACHE_SIZE, 0,
			RTE_PKTMBUF_HEADROOM + TAP_GSO_MBUF_SEG_SIZE,
			SOCKET_ID_ANY);
		if (!mp) {
			struct pmd_internals *pmd = dev->data->dev_private;

			TAP_LOG(ERR,
				"%s: failed to create mbuf pool for device %s\n",
				pmd->name, dev->device->name);
			return -1;
		}
	}

	gso_ctx->direct_pool = mp;
	gso_ctx->indirect_pool = mp;
	gso_ctx->gso_types = gso_types;
	gso_ctx->gso_size = 0; /* gso_size is set in tx_burst() per packet */
	gso_ctx->flag = 0;

	return 0;
}

static int
tap_setup_queue(struct rte_eth_dev *dev,
		struct pmd_internals *internals,
		uint16_t qid,
		int is_rx)
{
	int ret;
	int *fd;
	int *other_fd;
	const char *dir;
	struct pmd_internals *pmd = dev->data->dev_private;
	struct pmd_process_private *process_private = dev->process_private;
	struct rx_queue *rx = &internals->rxq[qid];
	struct tx_queue *tx = &internals->txq[qid];
	struct rte_gso_ctx *gso_ctx;

	if (is_rx) {
		fd = &process_private->rxq_fds[qid];
		other_fd = &process_private->txq_fds[qid];
		dir = "rx";
		gso_ctx = NULL;
	} else {
		fd = &process_private->txq_fds[qid];
		other_fd = &process_private->rxq_fds[qid];
		dir = "tx";
		gso_ctx = &tx->gso_ctx;
	}
	if (*fd != -1) {
		/* fd for this queue already exists */
		TAP_LOG(DEBUG, "%s: fd %d for %s queue qid %d exists",
			pmd->name, *fd, dir, qid);
		gso_ctx = NULL;
	} else if (*other_fd != -1) {
		/* Only other_fd exists. dup it */
		*fd = dup(*other_fd);
		if (*fd < 0) {
			*fd = -1;
			TAP_LOG(ERR, "%s: dup() failed.", pmd->name);
			return -1;
		}
		TAP_LOG(DEBUG, "%s: dup fd %d for %s queue qid %d (%d)",
			pmd->name, *other_fd, dir, qid, *fd);
	} else {
		/* Both RX and TX fds do not exist (equal -1). Create fd */
		*fd = tun_alloc(pmd, 0);
		if (*fd < 0) {
			*fd = -1; /* restore original value */
			TAP_LOG(ERR, "%s: tun_alloc() failed.", pmd->name);
			return -1;
		}
		TAP_LOG(DEBUG, "%s: add %s queue for qid %d fd %d",
			pmd->name, dir, qid, *fd);
	}

	tx->mtu = &dev->data->mtu;
	rx->rxmode = &dev->data->dev_conf.rxmode;
	if (gso_ctx) {
		ret = tap_gso_ctx_setup(gso_ctx, dev);
		if (ret)
			return -1;
	}

	tx->type = pmd->type;

	return *fd;
}

static int
tap_rx_queue_setup(struct rte_eth_dev *dev,
		   uint16_t rx_queue_id,
		   uint16_t nb_rx_desc,
		   unsigned int socket_id,
		   const struct rte_eth_rxconf *rx_conf __rte_unused,
		   struct rte_mempool *mp)
{
	struct pmd_internals *internals = dev->data->dev_private;
	struct pmd_process_private *process_private = dev->process_private;
	struct rx_queue *rxq = &internals->rxq[rx_queue_id];
	struct rte_mbuf **tmp = &rxq->pool;
	long iov_max = sysconf(_SC_IOV_MAX);

	if (iov_max <= 0) {
		TAP_LOG(WARNING,
			"_SC_IOV_MAX is not defined. Using %d as default",
			TAP_IOV_DEFAULT_MAX);
		iov_max = TAP_IOV_DEFAULT_MAX;
	}
	uint16_t nb_desc = RTE_MIN(nb_rx_desc, iov_max - 1);
	struct iovec (*iovecs)[nb_desc + 1];
	int data_off = RTE_PKTMBUF_HEADROOM;
	int ret = 0;
	int fd;
	int i;

	if (rx_queue_id >= dev->data->nb_rx_queues || !mp) {
		TAP_LOG(WARNING,
			"nb_rx_queues %d too small or mempool NULL",
			dev->data->nb_rx_queues);
		return -1;
	}

	rxq->mp = mp;
	rxq->trigger_seen = 1; /* force initial burst */
	rxq->in_port = dev->data->port_id;
	rxq->queue_id = rx_queue_id;
	rxq->nb_rx_desc = nb_desc;
	iovecs = rte_zmalloc_socket(dev->device->name, sizeof(*iovecs), 0,
				    socket_id);
	if (!iovecs) {
		TAP_LOG(WARNING,
			"%s: Couldn't allocate %d RX descriptors",
			dev->device->name, nb_desc);
		return -ENOMEM;
	}
	rxq->iovecs = iovecs;

	dev->data->rx_queues[rx_queue_id] = rxq;
	fd = tap_setup_queue(dev, internals, rx_queue_id, 1);
	if (fd == -1) {
		ret = fd;
		goto error;
	}

	(*rxq->iovecs)[0].iov_len = sizeof(struct tun_pi);
	(*rxq->iovecs)[0].iov_base = &rxq->pi;

	for (i = 1; i <= nb_desc; i++) {
		*tmp = rte_pktmbuf_alloc(rxq->mp);
		if (!*tmp) {
			TAP_LOG(WARNING,
				"%s: couldn't allocate memory for queue %d",
				dev->device->name, rx_queue_id);
			ret = -ENOMEM;
			goto error;
		}
		(*rxq->iovecs)[i].iov_len = (*tmp)->buf_len - data_off;
		(*rxq->iovecs)[i].iov_base =
			(char *)(*tmp)->buf_addr + data_off;
		data_off = 0;
		tmp = &(*tmp)->next;
	}

	TAP_LOG(DEBUG, "  RX TUNTAP device name %s, qid %d on fd %d",
		internals->name, rx_queue_id,
		process_private->rxq_fds[rx_queue_id]);

	return 0;

error:
	rte_pktmbuf_free(rxq->pool);
	rxq->pool = NULL;
	rte_free(rxq->iovecs);
	rxq->iovecs = NULL;
	return ret;
}

static int
tap_tx_queue_setup(struct rte_eth_dev *dev,
		   uint16_t tx_queue_id,
		   uint16_t nb_tx_desc __rte_unused,
		   unsigned int socket_id __rte_unused,
		   const struct rte_eth_txconf *tx_conf)
{
	struct pmd_internals *internals = dev->data->dev_private;
	struct pmd_process_private *process_private = dev->process_private;
	struct tx_queue *txq;
	int ret;
	uint64_t offloads;

	if (tx_queue_id >= dev->data->nb_tx_queues)
		return -1;
	dev->data->tx_queues[tx_queue_id] = &internals->txq[tx_queue_id];
	txq = dev->data->tx_queues[tx_queue_id];
	txq->out_port = dev->data->port_id;
	txq->queue_id = tx_queue_id;

	offloads = tx_conf->offloads | dev->data->dev_conf.txmode.offloads;
	txq->csum = !!(offloads &
			(DEV_TX_OFFLOAD_IPV4_CKSUM |
			 DEV_TX_OFFLOAD_UDP_CKSUM |
			 DEV_TX_OFFLOAD_TCP_CKSUM));

	ret = tap_setup_queue(dev, internals, tx_queue_id, 0);
	if (ret == -1)
		return -1;
	TAP_LOG(DEBUG,
		"  TX TUNTAP device name %s, qid %d on fd %d csum %s",
		internals->name, tx_queue_id,
		process_private->txq_fds[tx_queue_id],
		txq->csum ? "on" : "off");

	return 0;
}

static int
tap_mtu_set(struct rte_eth_dev *dev, uint16_t mtu)
{
	struct pmd_internals *pmd = dev->data->dev_private;
	struct ifreq ifr = { .ifr_mtu = mtu };
	int err = 0;

	err = tap_ioctl(pmd, SIOCSIFMTU, &ifr, 1, LOCAL_AND_REMOTE);
	if (!err)
		dev->data->mtu = mtu;

	return err;
}

static int
tap_set_mc_addr_list(struct rte_eth_dev *dev __rte_unused,
		     struct rte_ether_addr *mc_addr_set __rte_unused,
		     uint32_t nb_mc_addr __rte_unused)
{
	/*
	 * Nothing to do actually: the tap has no filtering whatsoever, every
	 * packet is received.
	 */
	return 0;
}

static int
tap_nl_msg_handler(struct nlmsghdr *nh, void *arg)
{
	struct rte_eth_dev *dev = arg;
	struct pmd_internals *pmd = dev->data->dev_private;
	struct ifinfomsg *info = NLMSG_DATA(nh);

	if (nh->nlmsg_type != RTM_NEWLINK ||
	    (info->ifi_index != pmd->if_index &&
	     info->ifi_index != pmd->remote_if_index))
		return 0;
	return tap_link_update(dev, 0);
}

static void
tap_dev_intr_handler(void *cb_arg)
{
	struct rte_eth_dev *dev = cb_arg;
	struct pmd_internals *pmd = dev->data->dev_private;

	tap_nl_recv(pmd->intr_handle.fd, tap_nl_msg_handler, dev);
}

static int
tap_lsc_intr_handle_set(struct rte_eth_dev *dev, int set)
{
	struct pmd_internals *pmd = dev->data->dev_private;

	/* In any case, disable interrupt if the conf is no longer there. */
	if (!dev->data->dev_conf.intr_conf.lsc) {
		if (pmd->intr_handle.fd != -1) {
			tap_nl_final(pmd->intr_handle.fd);
			rte_intr_callback_unregister(&pmd->intr_handle,
				tap_dev_intr_handler, dev);
		}
		return 0;
	}
	if (set) {
		pmd->intr_handle.fd = tap_nl_init(RTMGRP_LINK);
		if (unlikely(pmd->intr_handle.fd == -1))
			return -EBADF;
		return rte_intr_callback_register(
			&pmd->intr_handle, tap_dev_intr_handler, dev);
	}
	tap_nl_final(pmd->intr_handle.fd);
	return rte_intr_callback_unregister(&pmd->intr_handle,
					    tap_dev_intr_handler, dev);
}

static int
tap_intr_handle_set(struct rte_eth_dev *dev, int set)
{
	int err;

	err = tap_lsc_intr_handle_set(dev, set);
	if (err < 0) {
		if (!set)
			tap_rx_intr_vec_set(dev, 0);
		return err;
	}
	err = tap_rx_intr_vec_set(dev, set);
	if (err && set)
		tap_lsc_intr_handle_set(dev, 0);
	return err;
}

static const uint32_t*
tap_dev_supported_ptypes_get(struct rte_eth_dev *dev __rte_unused)
{
	static const uint32_t ptypes[] = {
		RTE_PTYPE_INNER_L2_ETHER,
		RTE_PTYPE_INNER_L2_ETHER_VLAN,
		RTE_PTYPE_INNER_L2_ETHER_QINQ,
		RTE_PTYPE_INNER_L3_IPV4,
		RTE_PTYPE_INNER_L3_IPV4_EXT,
		RTE_PTYPE_INNER_L3_IPV6,
		RTE_PTYPE_INNER_L3_IPV6_EXT,
		RTE_PTYPE_INNER_L4_FRAG,
		RTE_PTYPE_INNER_L4_UDP,
		RTE_PTYPE_INNER_L4_TCP,
		RTE_PTYPE_INNER_L4_SCTP,
		RTE_PTYPE_L2_ETHER,
		RTE_PTYPE_L2_ETHER_VLAN,
		RTE_PTYPE_L2_ETHER_QINQ,
		RTE_PTYPE_L3_IPV4,
		RTE_PTYPE_L3_IPV4_EXT,
		RTE_PTYPE_L3_IPV6_EXT,
		RTE_PTYPE_L3_IPV6,
		RTE_PTYPE_L4_FRAG,
		RTE_PTYPE_L4_UDP,
		RTE_PTYPE_L4_TCP,
		RTE_PTYPE_L4_SCTP,
	};

	return ptypes;
}

static int
tap_flow_ctrl_get(struct rte_eth_dev *dev __rte_unused,
		  struct rte_eth_fc_conf *fc_conf)
{
	fc_conf->mode = RTE_FC_NONE;
	return 0;
}

static int
tap_flow_ctrl_set(struct rte_eth_dev *dev __rte_unused,
		  struct rte_eth_fc_conf *fc_conf)
{
	if (fc_conf->mode != RTE_FC_NONE)
		return -ENOTSUP;
	return 0;
}

/**
 * DPDK callback to update the RSS hash configuration.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param[in] rss_conf
 *   RSS configuration data.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
tap_rss_hash_update(struct rte_eth_dev *dev,
		struct rte_eth_rss_conf *rss_conf)
{
	if (rss_conf->rss_hf & TAP_RSS_HF_MASK) {
		rte_errno = EINVAL;
		return -rte_errno;
	}
	if (rss_conf->rss_key && rss_conf->rss_key_len) {
		/*
		 * Currently TAP RSS key is hard coded
		 * and cannot be updated
		 */
		TAP_LOG(ERR,
			"port %u RSS key cannot be updated",
			dev->data->port_id);
		rte_errno = EINVAL;
		return -rte_errno;
	}
	return 0;
}

static int
tap_rx_queue_start(struct rte_eth_dev *dev, uint16_t rx_queue_id)
{
	dev->data->rx_queue_state[rx_queue_id] = RTE_ETH_QUEUE_STATE_STARTED;

	return 0;
}

static int
tap_tx_queue_start(struct rte_eth_dev *dev, uint16_t tx_queue_id)
{
	dev->data->tx_queue_state[tx_queue_id] = RTE_ETH_QUEUE_STATE_STARTED;

	return 0;
}

static int
tap_rx_queue_stop(struct rte_eth_dev *dev, uint16_t rx_queue_id)
{
	dev->data->rx_queue_state[rx_queue_id] = RTE_ETH_QUEUE_STATE_STOPPED;

	return 0;
}

static int
tap_tx_queue_stop(struct rte_eth_dev *dev, uint16_t tx_queue_id)
{
	dev->data->tx_queue_state[tx_queue_id] = RTE_ETH_QUEUE_STATE_STOPPED;

	return 0;
}
static const struct eth_dev_ops ops = {
	.dev_start              = tap_dev_start,
	.dev_stop               = tap_dev_stop,
	.dev_close              = tap_dev_close,
	.dev_configure          = tap_dev_configure,
	.dev_infos_get          = tap_dev_info,
	.rx_queue_setup         = tap_rx_queue_setup,
	.tx_queue_setup         = tap_tx_queue_setup,
	.rx_queue_start         = tap_rx_queue_start,
	.tx_queue_start         = tap_tx_queue_start,
	.rx_queue_stop          = tap_rx_queue_stop,
	.tx_queue_stop          = tap_tx_queue_stop,
	.rx_queue_release       = tap_rx_queue_release,
	.tx_queue_release       = tap_tx_queue_release,
	.flow_ctrl_get          = tap_flow_ctrl_get,
	.flow_ctrl_set          = tap_flow_ctrl_set,
	.link_update            = tap_link_update,
	.dev_set_link_up        = tap_link_set_up,
	.dev_set_link_down      = tap_link_set_down,
	.promiscuous_enable     = tap_promisc_enable,
	.promiscuous_disable    = tap_promisc_disable,
	.allmulticast_enable    = tap_allmulti_enable,
	.allmulticast_disable   = tap_allmulti_disable,
	.mac_addr_set           = tap_mac_set,
	.mtu_set                = tap_mtu_set,
	.set_mc_addr_list       = tap_set_mc_addr_list,
	.stats_get              = tap_stats_get,
	.stats_reset            = tap_stats_reset,
	.dev_supported_ptypes_get = tap_dev_supported_ptypes_get,
	.rss_hash_update        = tap_rss_hash_update,
	.filter_ctrl            = tap_dev_filter_ctrl,
};

static const char *tuntap_types[ETH_TUNTAP_TYPE_MAX] = {
	"UNKNOWN", "TUN", "TAP"
};

static int
eth_dev_tap_create(struct rte_vdev_device *vdev, const char *tap_name,
		   char *remote_iface, struct rte_ether_addr *mac_addr,
		   enum rte_tuntap_type type)
{
	int numa_node = rte_socket_id();
	struct rte_eth_dev *dev;
	struct pmd_internals *pmd;
	struct pmd_process_private *process_private;
	const char *tuntap_name = tuntap_types[type];
	struct rte_eth_dev_data *data;
	struct ifreq ifr;
	int i;

	TAP_LOG(DEBUG, "%s device on numa %u", tuntap_name, rte_socket_id());

	dev = rte_eth_vdev_allocate(vdev, sizeof(*pmd));
	if (!dev) {
		TAP_LOG(ERR, "%s Unable to allocate device struct",
				tuntap_name);
		goto error_exit_nodev;
	}

	process_private = (struct pmd_process_private *)
		rte_zmalloc_socket(tap_name, sizeof(struct pmd_process_private),
			RTE_CACHE_LINE_SIZE, dev->device->numa_node);

	if (process_private == NULL) {
		TAP_LOG(ERR, "Failed to alloc memory for process private");
		return -1;
	}
	pmd = dev->data->dev_private;
	dev->process_private = process_private;
	pmd->dev = dev;
	strlcpy(pmd->name, tap_name, sizeof(pmd->name));
	pmd->type = type;

	pmd->ioctl_sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (pmd->ioctl_sock == -1) {
		TAP_LOG(ERR,
			"%s Unable to get a socket for management: %s",
			tuntap_name, strerror(errno));
		goto error_exit;
	}

	/* Setup some default values */
	data = dev->data;
	data->dev_private = pmd;
	data->dev_flags = RTE_ETH_DEV_INTR_LSC;
	data->numa_node = numa_node;

	data->dev_link = pmd_link;
	data->mac_addrs = &pmd->eth_addr;
	/* Set the number of RX and TX queues */
	data->nb_rx_queues = 0;
	data->nb_tx_queues = 0;

	dev->dev_ops = &ops;
	dev->rx_pkt_burst = pmd_rx_burst;
	dev->tx_pkt_burst = pmd_tx_burst;

	pmd->intr_handle.type = RTE_INTR_HANDLE_EXT;
	pmd->intr_handle.fd = -1;
	dev->intr_handle = &pmd->intr_handle;

	/* Presetup the fds to -1 as being not valid */
	pmd->ka_fd = -1;
	for (i = 0; i < RTE_PMD_TAP_MAX_QUEUES; i++) {
		process_private->rxq_fds[i] = -1;
		process_private->txq_fds[i] = -1;
	}

	if (pmd->type == ETH_TUNTAP_TYPE_TAP) {
		if (rte_is_zero_ether_addr(mac_addr))
			rte_eth_random_addr((uint8_t *)&pmd->eth_addr);
		else
			rte_memcpy(&pmd->eth_addr, mac_addr, sizeof(*mac_addr));
	}

	/*
	 * Allocate a TUN device keep-alive file descriptor that will only be
	 * closed when the TUN device itself is closed or removed.
	 * This keep-alive file descriptor will guarantee that the TUN device
	 * exists even when all of its queues are closed
	 */
	pmd->ka_fd = tun_alloc(pmd, 1);
	if (pmd->ka_fd == -1) {
		TAP_LOG(ERR, "Unable to create %s interface", tuntap_name);
		goto error_exit;
	}
	TAP_LOG(DEBUG, "allocated %s", pmd->name);

	ifr.ifr_mtu = dev->data->mtu;
	if (tap_ioctl(pmd, SIOCSIFMTU, &ifr, 1, LOCAL_AND_REMOTE) < 0)
		goto error_exit;

	if (pmd->type == ETH_TUNTAP_TYPE_TAP) {
		memset(&ifr, 0, sizeof(struct ifreq));
		ifr.ifr_hwaddr.sa_family = AF_LOCAL;
		rte_memcpy(ifr.ifr_hwaddr.sa_data, &pmd->eth_addr,
				RTE_ETHER_ADDR_LEN);
		if (tap_ioctl(pmd, SIOCSIFHWADDR, &ifr, 0, LOCAL_ONLY) < 0)
			goto error_exit;
	}

	/*
	 * Set up everything related to rte_flow:
	 * - netlink socket
	 * - tap / remote if_index
	 * - mandatory QDISCs
	 * - rte_flow actual/implicit lists
	 * - implicit rules
	 */
	pmd->nlsk_fd = tap_nl_init(0);
	if (pmd->nlsk_fd == -1) {
		TAP_LOG(WARNING, "%s: failed to create netlink socket.",
			pmd->name);
		goto disable_rte_flow;
	}
	pmd->if_index = if_nametoindex(pmd->name);
	if (!pmd->if_index) {
		TAP_LOG(ERR, "%s: failed to get if_index.", pmd->name);
		goto disable_rte_flow;
	}
	if (qdisc_create_multiq(pmd->nlsk_fd, pmd->if_index) < 0) {
		TAP_LOG(ERR, "%s: failed to create multiq qdisc.",
			pmd->name);
		goto disable_rte_flow;
	}
	if (qdisc_create_ingress(pmd->nlsk_fd, pmd->if_index) < 0) {
		TAP_LOG(ERR, "%s: failed to create ingress qdisc.",
			pmd->name);
		goto disable_rte_flow;
	}
	LIST_INIT(&pmd->flows);

	if (strlen(remote_iface)) {
		pmd->remote_if_index = if_nametoindex(remote_iface);
		if (!pmd->remote_if_index) {
			TAP_LOG(ERR, "%s: failed to get %s if_index.",
				pmd->name, remote_iface);
			goto error_remote;
		}
		strlcpy(pmd->remote_iface, remote_iface, RTE_ETH_NAME_MAX_LEN);

		/* Save state of remote device */
		tap_ioctl(pmd, SIOCGIFFLAGS, &pmd->remote_initial_flags, 0, REMOTE_ONLY);

		/* Replicate remote MAC address */
		if (tap_ioctl(pmd, SIOCGIFHWADDR, &ifr, 0, REMOTE_ONLY) < 0) {
			TAP_LOG(ERR, "%s: failed to get %s MAC address.",
				pmd->name, pmd->remote_iface);
			goto error_remote;
		}
		rte_memcpy(&pmd->eth_addr, ifr.ifr_hwaddr.sa_data,
			   RTE_ETHER_ADDR_LEN);
		/* The desired MAC is already in ifreq after SIOCGIFHWADDR. */
		if (tap_ioctl(pmd, SIOCSIFHWADDR, &ifr, 0, LOCAL_ONLY) < 0) {
			TAP_LOG(ERR, "%s: failed to get %s MAC address.",
				pmd->name, remote_iface);
			goto error_remote;
		}

		/*
		 * Flush usually returns negative value because it tries to
		 * delete every QDISC (and on a running device, one QDISC at
		 * least is needed). Ignore negative return value.
		 */
		qdisc_flush(pmd->nlsk_fd, pmd->remote_if_index);
		if (qdisc_create_ingress(pmd->nlsk_fd,
					 pmd->remote_if_index) < 0) {
			TAP_LOG(ERR, "%s: failed to create ingress qdisc.",
				pmd->remote_iface);
			goto error_remote;
		}
		LIST_INIT(&pmd->implicit_flows);
		if (tap_flow_implicit_create(pmd, TAP_REMOTE_TX) < 0 ||
		    tap_flow_implicit_create(pmd, TAP_REMOTE_LOCAL_MAC) < 0 ||
		    tap_flow_implicit_create(pmd, TAP_REMOTE_BROADCAST) < 0 ||
		    tap_flow_implicit_create(pmd, TAP_REMOTE_BROADCASTV6) < 0) {
			TAP_LOG(ERR,
				"%s: failed to create implicit rules.",
				pmd->name);
			goto error_remote;
		}
	}

	rte_eth_dev_probing_finish(dev);
	return 0;

disable_rte_flow:
	TAP_LOG(ERR, " Disabling rte flow support: %s(%d)",
		strerror(errno), errno);
	if (strlen(remote_iface)) {
		TAP_LOG(ERR, "Remote feature requires flow support.");
		goto error_exit;
	}
	rte_eth_dev_probing_finish(dev);
	return 0;

error_remote:
	TAP_LOG(ERR, " Can't set up remote feature: %s(%d)",
		strerror(errno), errno);
	tap_flow_implicit_flush(pmd, NULL);

error_exit:
	if (pmd->ioctl_sock > 0)
		close(pmd->ioctl_sock);
	/* mac_addrs must not be freed alone because part of dev_private */
	dev->data->mac_addrs = NULL;
	rte_eth_dev_release_port(dev);

error_exit_nodev:
	TAP_LOG(ERR, "%s Unable to initialize %s",
		tuntap_name, rte_vdev_device_name(vdev));

	return -EINVAL;
}

/* make sure name is a possible Linux network device name */
static bool
is_valid_iface(const char *name)
{
	if (*name == '\0')
		return false;

	if (strnlen(name, IFNAMSIZ) == IFNAMSIZ)
		return false;

	while (*name) {
		if (*name == '/' || *name == ':' || isspace(*name))
			return false;
		name++;
	}
	return true;
}

static int
set_interface_name(const char *key __rte_unused,
		   const char *value,
		   void *extra_args)
{
	char *name = (char *)extra_args;

	if (value) {
		if (!is_valid_iface(value)) {
			TAP_LOG(ERR, "TAP invalid remote interface name (%s)",
				value);
			return -1;
		}
		strlcpy(name, value, RTE_ETH_NAME_MAX_LEN);
	} else {
		/* use tap%d which causes kernel to choose next available */
		strlcpy(name, DEFAULT_TAP_NAME "%d", RTE_ETH_NAME_MAX_LEN);
	}
	return 0;
}

static int
set_remote_iface(const char *key __rte_unused,
		 const char *value,
		 void *extra_args)
{
	char *name = (char *)extra_args;

	if (value) {
		if (!is_valid_iface(value)) {
			TAP_LOG(ERR, "TAP invalid remote interface name (%s)",
				value);
			return -1;
		}
		strlcpy(name, value, RTE_ETH_NAME_MAX_LEN);
	}

	return 0;
}

static int parse_user_mac(struct rte_ether_addr *user_mac,
		const char *value)
{
	unsigned int index = 0;
	char mac_temp[strlen(ETH_TAP_USR_MAC_FMT) + 1], *mac_byte = NULL;

	if (user_mac == NULL || value == NULL)
		return 0;

	strlcpy(mac_temp, value, sizeof(mac_temp));
	mac_byte = strtok(mac_temp, ":");

	while ((mac_byte != NULL) &&
			(strlen(mac_byte) <= 2) &&
			(strlen(mac_byte) == strspn(mac_byte,
					ETH_TAP_CMP_MAC_FMT))) {
		user_mac->addr_bytes[index++] = strtoul(mac_byte, NULL, 16);
		mac_byte = strtok(NULL, ":");
	}

	return index;
}

static int
set_mac_type(const char *key __rte_unused,
	     const char *value,
	     void *extra_args)
{
	struct rte_ether_addr *user_mac = extra_args;

	if (!value)
		return 0;

	if (!strncasecmp(ETH_TAP_MAC_FIXED, value, strlen(ETH_TAP_MAC_FIXED))) {
		static int iface_idx;

		/* fixed mac = 00:64:74:61:70:<iface_idx> */
		memcpy((char *)user_mac->addr_bytes, "\0dtap",
			RTE_ETHER_ADDR_LEN);
		user_mac->addr_bytes[RTE_ETHER_ADDR_LEN - 1] =
			iface_idx++ + '0';
		goto success;
	}

	if (parse_user_mac(user_mac, value) != 6)
		goto error;
success:
	TAP_LOG(DEBUG, "TAP user MAC param (%s)", value);
	return 0;

error:
	TAP_LOG(ERR, "TAP user MAC (%s) is not in format (%s|%s)",
		value, ETH_TAP_MAC_FIXED, ETH_TAP_USR_MAC_FMT);
	return -1;
}

/*
 * Open a TUN interface device. TUN PMD
 * 1) sets tap_type as false
 * 2) intakes iface as argument.
 * 3) as interface is virtual set speed to 10G
 */
static int
rte_pmd_tun_probe(struct rte_vdev_device *dev)
{
	const char *name, *params;
	int ret;
	struct rte_kvargs *kvlist = NULL;
	char tun_name[RTE_ETH_NAME_MAX_LEN];
	char remote_iface[RTE_ETH_NAME_MAX_LEN];
	struct rte_eth_dev *eth_dev;

	name = rte_vdev_device_name(dev);
	params = rte_vdev_device_args(dev);
	memset(remote_iface, 0, RTE_ETH_NAME_MAX_LEN);

	if (rte_eal_process_type() == RTE_PROC_SECONDARY &&
	    strlen(params) == 0) {
		eth_dev = rte_eth_dev_attach_secondary(name);
		if (!eth_dev) {
			TAP_LOG(ERR, "Failed to probe %s", name);
			return -1;
		}
		eth_dev->dev_ops = &ops;
		eth_dev->device = &dev->device;
		rte_eth_dev_probing_finish(eth_dev);
		return 0;
	}

	/* use tun%d which causes kernel to choose next available */
	strlcpy(tun_name, DEFAULT_TUN_NAME "%d", RTE_ETH_NAME_MAX_LEN);

	if (params && (params[0] != '\0')) {
		TAP_LOG(DEBUG, "parameters (%s)", params);

		kvlist = rte_kvargs_parse(params, valid_arguments);
		if (kvlist) {
			if (rte_kvargs_count(kvlist, ETH_TAP_IFACE_ARG) == 1) {
				ret = rte_kvargs_process(kvlist,
					ETH_TAP_IFACE_ARG,
					&set_interface_name,
					tun_name);

				if (ret == -1)
					goto leave;
			}
		}
	}
	pmd_link.link_speed = ETH_SPEED_NUM_10G;

	TAP_LOG(DEBUG, "Initializing pmd_tun for %s", name);

	ret = eth_dev_tap_create(dev, tun_name, remote_iface, 0,
				 ETH_TUNTAP_TYPE_TUN);

leave:
	if (ret == -1) {
		TAP_LOG(ERR, "Failed to create pmd for %s as %s",
			name, tun_name);
	}
	rte_kvargs_free(kvlist);

	return ret;
}

/* Request queue file descriptors from secondary to primary. */
static int
tap_mp_attach_queues(const char *port_name, struct rte_eth_dev *dev)
{
	int ret;
	struct timespec timeout = {.tv_sec = 1, .tv_nsec = 0};
	struct rte_mp_msg request, *reply;
	struct rte_mp_reply replies;
	struct ipc_queues *request_param = (struct ipc_queues *)request.param;
	struct ipc_queues *reply_param;
	struct pmd_process_private *process_private = dev->process_private;
	int queue, fd_iterator;

	/* Prepare the request */
	memset(&request, 0, sizeof(request));
	strlcpy(request.name, TAP_MP_KEY, sizeof(request.name));
	strlcpy(request_param->port_name, port_name,
		sizeof(request_param->port_name));
	request.len_param = sizeof(*request_param);
	/* Send request and receive reply */
	ret = rte_mp_request_sync(&request, &replies, &timeout);
	if (ret < 0 || replies.nb_received != 1) {
		TAP_LOG(ERR, "Failed to request queues from primary: %d",
			rte_errno);
		return -1;
	}
	reply = &replies.msgs[0];
	reply_param = (struct ipc_queues *)reply->param;
	TAP_LOG(DEBUG, "Received IPC reply for %s", reply_param->port_name);

	/* Attach the queues from received file descriptors */
	if (reply_param->rxq_count + reply_param->txq_count != reply->num_fds) {
		TAP_LOG(ERR, "Unexpected number of fds received");
		return -1;
	}

	dev->data->nb_rx_queues = reply_param->rxq_count;
	dev->data->nb_tx_queues = reply_param->txq_count;
	fd_iterator = 0;
	for (queue = 0; queue < reply_param->rxq_count; queue++)
		process_private->rxq_fds[queue] = reply->fds[fd_iterator++];
	for (queue = 0; queue < reply_param->txq_count; queue++)
		process_private->txq_fds[queue] = reply->fds[fd_iterator++];
	free(reply);
	return 0;
}

/* Send the queue file descriptors from the primary process to secondary. */
static int
tap_mp_sync_queues(const struct rte_mp_msg *request, const void *peer)
{
	struct rte_eth_dev *dev;
	struct pmd_process_private *process_private;
	struct rte_mp_msg reply;
	const struct ipc_queues *request_param =
		(const struct ipc_queues *)request->param;
	struct ipc_queues *reply_param =
		(struct ipc_queues *)reply.param;
	uint16_t port_id;
	int queue;
	int ret;

	/* Get requested port */
	TAP_LOG(DEBUG, "Received IPC request for %s", request_param->port_name);
	ret = rte_eth_dev_get_port_by_name(request_param->port_name, &port_id);
	if (ret) {
		TAP_LOG(ERR, "Failed to get port id for %s",
			request_param->port_name);
		return -1;
	}
	dev = &rte_eth_devices[port_id];
	process_private = dev->process_private;

	/* Fill file descriptors for all queues */
	reply.num_fds = 0;
	reply_param->rxq_count = 0;
	if (dev->data->nb_rx_queues + dev->data->nb_tx_queues >
			RTE_MP_MAX_FD_NUM){
		TAP_LOG(ERR, "Number of rx/tx queues exceeds max number of fds");
		return -1;
	}

	for (queue = 0; queue < dev->data->nb_rx_queues; queue++) {
		reply.fds[reply.num_fds++] = process_private->rxq_fds[queue];
		reply_param->rxq_count++;
	}
	RTE_ASSERT(reply_param->rxq_count == dev->data->nb_rx_queues);

	reply_param->txq_count = 0;
	for (queue = 0; queue < dev->data->nb_tx_queues; queue++) {
		reply.fds[reply.num_fds++] = process_private->txq_fds[queue];
		reply_param->txq_count++;
	}
	RTE_ASSERT(reply_param->txq_count == dev->data->nb_tx_queues);

	/* Send reply */
	strlcpy(reply.name, request->name, sizeof(reply.name));
	strlcpy(reply_param->port_name, request_param->port_name,
		sizeof(reply_param->port_name));
	reply.len_param = sizeof(*reply_param);
	if (rte_mp_reply(&reply, peer) < 0) {
		TAP_LOG(ERR, "Failed to reply an IPC request to sync queues");
		return -1;
	}
	return 0;
}

/* Open a TAP interface device.
 */
static int
rte_pmd_tap_probe(struct rte_vdev_device *dev)
{
	const char *name, *params;
	int ret;
	struct rte_kvargs *kvlist = NULL;
	int speed;
	char tap_name[RTE_ETH_NAME_MAX_LEN];
	char remote_iface[RTE_ETH_NAME_MAX_LEN];
	struct rte_ether_addr user_mac = { .addr_bytes = {0} };
	struct rte_eth_dev *eth_dev;
	int tap_devices_count_increased = 0;

	name = rte_vdev_device_name(dev);
	params = rte_vdev_device_args(dev);

	if (rte_eal_process_type() == RTE_PROC_SECONDARY) {
		eth_dev = rte_eth_dev_attach_secondary(name);
		if (!eth_dev) {
			TAP_LOG(ERR, "Failed to probe %s", name);
			return -1;
		}
		eth_dev->dev_ops = &ops;
		eth_dev->device = &dev->device;
		eth_dev->rx_pkt_burst = pmd_rx_burst;
		eth_dev->tx_pkt_burst = pmd_tx_burst;
		if (!rte_eal_primary_proc_alive(NULL)) {
			TAP_LOG(ERR, "Primary process is missing");
			return -1;
		}
		eth_dev->process_private = (struct pmd_process_private *)
			rte_zmalloc_socket(name,
				sizeof(struct pmd_process_private),
				RTE_CACHE_LINE_SIZE,
				eth_dev->device->numa_node);
		if (eth_dev->process_private == NULL) {
			TAP_LOG(ERR,
				"Failed to alloc memory for process private");
			return -1;
		}

		ret = tap_mp_attach_queues(name, eth_dev);
		if (ret != 0)
			return -1;
		rte_eth_dev_probing_finish(eth_dev);
		return 0;
	}

	speed = ETH_SPEED_NUM_10G;

	/* use tap%d which causes kernel to choose next available */
	strlcpy(tap_name, DEFAULT_TAP_NAME "%d", RTE_ETH_NAME_MAX_LEN);
	memset(remote_iface, 0, RTE_ETH_NAME_MAX_LEN);

	if (params && (params[0] != '\0')) {
		TAP_LOG(DEBUG, "parameters (%s)", params);

		kvlist = rte_kvargs_parse(params, valid_arguments);
		if (kvlist) {
			if (rte_kvargs_count(kvlist, ETH_TAP_IFACE_ARG) == 1) {
				ret = rte_kvargs_process(kvlist,
							 ETH_TAP_IFACE_ARG,
							 &set_interface_name,
							 tap_name);
				if (ret == -1)
					goto leave;
			}

			if (rte_kvargs_count(kvlist, ETH_TAP_REMOTE_ARG) == 1) {
				ret = rte_kvargs_process(kvlist,
							 ETH_TAP_REMOTE_ARG,
							 &set_remote_iface,
							 remote_iface);
				if (ret == -1)
					goto leave;
			}

			if (rte_kvargs_count(kvlist, ETH_TAP_MAC_ARG) == 1) {
				ret = rte_kvargs_process(kvlist,
							 ETH_TAP_MAC_ARG,
							 &set_mac_type,
							 &user_mac);
				if (ret == -1)
					goto leave;
			}
		}
	}
	pmd_link.link_speed = speed;

	TAP_LOG(DEBUG, "Initializing pmd_tap for %s", name);

	/* Register IPC feed callback */
	if (!tap_devices_count) {
		ret = rte_mp_action_register(TAP_MP_KEY, tap_mp_sync_queues);
		if (ret < 0 && rte_errno != ENOTSUP) {
			TAP_LOG(ERR, "tap: Failed to register IPC callback: %s",
				strerror(rte_errno));
			goto leave;
		}
	}
	tap_devices_count++;
	tap_devices_count_increased = 1;
	ret = eth_dev_tap_create(dev, tap_name, remote_iface, &user_mac,
		ETH_TUNTAP_TYPE_TAP);

leave:
	if (ret == -1) {
		TAP_LOG(ERR, "Failed to create pmd for %s as %s",
			name, tap_name);
		if (tap_devices_count_increased == 1) {
			if (tap_devices_count == 1)
				rte_mp_action_unregister(TAP_MP_KEY);
			tap_devices_count--;
		}
	}
	rte_kvargs_free(kvlist);

	return ret;
}

/* detach a TUNTAP device.
 */
static int
rte_pmd_tap_remove(struct rte_vdev_device *dev)
{
	struct rte_eth_dev *eth_dev = NULL;
	struct pmd_internals *internals;

	/* find the ethdev entry */
	eth_dev = rte_eth_dev_allocated(rte_vdev_device_name(dev));
	if (!eth_dev)
		return -ENODEV;

	/* mac_addrs must not be freed alone because part of dev_private */
	eth_dev->data->mac_addrs = NULL;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return rte_eth_dev_release_port(eth_dev);

	tap_dev_close(eth_dev);

	internals = eth_dev->data->dev_private;
	TAP_LOG(DEBUG, "Closing %s Ethernet device on numa %u",
		tuntap_types[internals->type], rte_socket_id());

	close(internals->ioctl_sock);
	rte_free(eth_dev->process_private);
	if (tap_devices_count == 1)
		rte_mp_action_unregister(TAP_MP_KEY);
	tap_devices_count--;
	rte_eth_dev_release_port(eth_dev);

	if (internals->ka_fd != -1) {
		close(internals->ka_fd);
		internals->ka_fd = -1;
	}
	return 0;
}

static struct rte_vdev_driver pmd_tun_drv = {
	.probe = rte_pmd_tun_probe,
	.remove = rte_pmd_tap_remove,
};

static struct rte_vdev_driver pmd_tap_drv = {
	.probe = rte_pmd_tap_probe,
	.remove = rte_pmd_tap_remove,
};

RTE_PMD_REGISTER_VDEV(net_tap, pmd_tap_drv);
RTE_PMD_REGISTER_VDEV(net_tun, pmd_tun_drv);
RTE_PMD_REGISTER_ALIAS(net_tap, eth_tap);
RTE_PMD_REGISTER_PARAM_STRING(net_tun,
			      ETH_TAP_IFACE_ARG "=<string> ");
RTE_PMD_REGISTER_PARAM_STRING(net_tap,
			      ETH_TAP_IFACE_ARG "=<string> "
			      ETH_TAP_MAC_ARG "=" ETH_TAP_MAC_ARG_FMT " "
			      ETH_TAP_REMOTE_ARG "=<string>");
int tap_logtype;

RTE_INIT(tap_init_log)
{
	tap_logtype = rte_log_register("pmd.net.tap");
	if (tap_logtype >= 0)
		rte_log_set_level(tap_logtype, RTE_LOG_NOTICE);
}
