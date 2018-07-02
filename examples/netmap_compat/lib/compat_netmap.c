/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <errno.h>
#include <inttypes.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <net/if.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/mman.h>

#include <rte_common.h>
#include <rte_errno.h>
#include <rte_ethdev.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>
#include <rte_spinlock.h>
#include <rte_string_fns.h>

#include "compat_netmap.h"

struct netmap_port {
	struct rte_mempool   *pool;
	struct netmap_if     *nmif;
	struct rte_eth_conf   eth_conf;
	struct rte_eth_txconf tx_conf;
	struct rte_eth_rxconf rx_conf;
	int32_t  socket_id;
	uint16_t nr_tx_rings;
	uint16_t nr_rx_rings;
	uint32_t nr_tx_slots;
	uint32_t nr_rx_slots;
	uint16_t tx_burst;
	uint16_t rx_burst;
	uint32_t fd;
};

struct fd_port {
	uint32_t port;
};

#ifndef POLLRDNORM
#define POLLRDNORM	0x0040
#endif

#ifndef POLLWRNORM
#define POLLWRNORM	0x0100
#endif

#define	FD_PORT_FREE	UINT32_MAX
#define	FD_PORT_RSRV	(FD_PORT_FREE - 1)

struct netmap_state {
	struct rte_netmap_conf conf;
	uintptr_t buf_start;
	void     *mem;
	uint32_t  mem_sz;
	uint32_t  netif_memsz;
};


#define COMPAT_NETMAP_MAX_NOFILE	(2 * RTE_MAX_ETHPORTS)
#define COMPAT_NETMAP_MAX_BURST		64
#define COMPAT_NETMAP_MAX_PKT_PER_SYNC	(2 * COMPAT_NETMAP_MAX_BURST)

static struct netmap_port ports[RTE_MAX_ETHPORTS];
static struct netmap_state netmap;

static struct fd_port fd_port[COMPAT_NETMAP_MAX_NOFILE];
static const int next_fd_start = RLIMIT_NOFILE + 1;
static rte_spinlock_t netmap_lock;

#define	IDX_TO_FD(x)	((x) + next_fd_start)
#define	FD_TO_IDX(x)	((x) - next_fd_start)
#define	FD_VALID(x)	((x) >= next_fd_start && \
	(x) < (typeof (x))(RTE_DIM(fd_port) + next_fd_start))

#define	PORT_NUM_RINGS	(2 * netmap.conf.max_rings)
#define	PORT_NUM_SLOTS	(PORT_NUM_RINGS * netmap.conf.max_slots)

#define	BUF_IDX(port, ring, slot)            \
	(((port) * PORT_NUM_RINGS + (ring)) * netmap.conf.max_slots + \
	(slot))

#define NETMAP_IF_RING_OFS(rid, rings, slots)   ({\
	struct netmap_if *_if;                    \
	struct netmap_ring *_rg;                  \
	sizeof(*_if) +                            \
	(rings) * sizeof(_if->ring_ofs[0]) +      \
	(rid) * sizeof(*_rg) +                    \
	(slots) * sizeof(_rg->slot[0]);           \
	})

static void netmap_unregif(uint32_t idx, uint32_t port);


static int32_t
ifname_to_portid(const char *ifname, uint16_t *port)
{
	char *endptr;
	uint64_t portid;

	errno = 0;
	portid = strtoul(ifname, &endptr, 10);
	if (endptr == ifname || *endptr != '\0' ||
			portid >= RTE_DIM(ports) || errno != 0)
		return -EINVAL;

	*port = portid;
	return 0;
}

/**
 * Given a dpdk mbuf, fill in the Netmap slot in ring r and its associated
 * buffer with the data held by the mbuf.
 * Note that mbuf chains are not supported.
 */
static void
mbuf_to_slot(struct rte_mbuf *mbuf, struct netmap_ring *r, uint32_t index)
{
	char *data;
	uint16_t length;

	data   = rte_pktmbuf_mtod(mbuf, char *);
	length = rte_pktmbuf_data_len(mbuf);

	if (length > r->nr_buf_size)
		length = 0;

	r->slot[index].len = length;
	rte_memcpy(NETMAP_BUF(r, r->slot[index].buf_idx), data, length);
}

/**
 * Given a Netmap ring and a slot index for that ring, construct a dpdk mbuf
 * from the data held in the buffer associated with the slot.
 * Allocation/deallocation of the dpdk mbuf are the responsibility of the
 * caller.
 * Note that mbuf chains are not supported.
 */
static void
slot_to_mbuf(struct netmap_ring *r, uint32_t index, struct rte_mbuf *mbuf)
{
	char *data;
	uint16_t length;

	rte_pktmbuf_reset(mbuf);
	length = r->slot[index].len;
	data = rte_pktmbuf_append(mbuf, length);

	if (data != NULL)
	    rte_memcpy(data, NETMAP_BUF(r, r->slot[index].buf_idx), length);
}

static int32_t
fd_reserve(void)
{
	uint32_t i;

	for (i = 0; i != RTE_DIM(fd_port) && fd_port[i].port != FD_PORT_FREE;
			i++)
		;

	if (i == RTE_DIM(fd_port))
		return -ENOMEM;

	fd_port[i].port = FD_PORT_RSRV;
	return IDX_TO_FD(i);
}

static int32_t
fd_release(int32_t fd)
{
	uint32_t idx, port;

	idx = FD_TO_IDX(fd);

	if (!FD_VALID(fd) || (port = fd_port[idx].port) == FD_PORT_FREE)
		return -EINVAL;

	/* if we still have a valid port attached, release the port */
	if (port < RTE_DIM(ports) && ports[port].fd == idx) {
		netmap_unregif(idx, port);
	}

	fd_port[idx].port = FD_PORT_FREE;
	return 0;
}

static int
check_nmreq(struct nmreq *req, uint16_t *port)
{
	int32_t rc;
	uint16_t portid;

	if (req == NULL)
		return -EINVAL;

	if (req->nr_version != NETMAP_API) {
		req->nr_version = NETMAP_API;
		return -EINVAL;
	}

	if ((rc = ifname_to_portid(req->nr_name, &portid)) != 0) {
	    	RTE_LOG(ERR, USER1, "Invalid interface name:\"%s\" "
			"in NIOCGINFO call\n", req->nr_name);
		return rc;
	}

	if (ports[portid].pool == NULL) {
		RTE_LOG(ERR, USER1, "Misconfigured portid %u\n", portid);
		return -EINVAL;
	}

	*port = portid;
	return 0;
}

/**
 * Simulate a Netmap NIOCGINFO ioctl: given a struct nmreq holding an interface
 * name (a port number in our case), fill the struct nmreq in with advisory
 * information about the interface: number of rings and their size, total memory
 * required in the map, ...
 * Those are preconfigured using rte_eth_{,tx,rx}conf and
 * rte_netmap_port_conf structures
 * and calls to rte_netmap_init_port() in the Netmap application.
 */
static int
ioctl_niocginfo(__rte_unused int fd, void * param)
{
	uint16_t portid;
	struct nmreq *req;
	int32_t rc;

	req = (struct nmreq *)param;
	if ((rc = check_nmreq(req, &portid)) != 0)
		return rc;

	req->nr_tx_rings = (uint16_t)(ports[portid].nr_tx_rings - 1);
	req->nr_rx_rings = (uint16_t)(ports[portid].nr_rx_rings - 1);
	req->nr_tx_slots = ports[portid].nr_tx_slots;
	req->nr_rx_slots = ports[portid].nr_rx_slots;

	/* in current implementation we have all NETIFs shared aone region. */
	req->nr_memsize = netmap.mem_sz;
	req->nr_offset = 0;

	return 0;
}

static void
netmap_ring_setup(struct netmap_ring *ring, uint16_t port, uint32_t ringid,
	uint32_t num_slots)
{
	uint32_t j;

	ring->buf_ofs = netmap.buf_start - (uintptr_t)ring;
	ring->num_slots = num_slots;
	ring->cur = 0;
	ring->reserved = 0;
	ring->nr_buf_size = netmap.conf.max_bufsz;
	ring->flags = 0;
	ring->ts.tv_sec = 0;
	ring->ts.tv_usec = 0;

	for (j = 0; j < ring->num_slots; j++) {
		ring->slot[j].buf_idx = BUF_IDX(port, ringid, j);
		ring->slot[j].len = 0;
		ring->flags = 0;
	}
}

static int
netmap_regif(struct nmreq *req, uint32_t idx, uint16_t port)
{
	struct netmap_if *nmif;
	struct netmap_ring *ring;
	uint32_t i, slots, start_ring;
	int32_t rc;

	if (ports[port].fd < RTE_DIM(fd_port)) {
		RTE_LOG(ERR, USER1, "port %u already in use by fd: %u\n",
			port, IDX_TO_FD(ports[port].fd));
		return -EBUSY;
	}
	if (fd_port[idx].port != FD_PORT_RSRV) {
	    	RTE_LOG(ERR, USER1, "fd: %u is misconfigured\n",
			IDX_TO_FD(idx));
		return -EBUSY;
	}

	nmif = ports[port].nmif;

	/* setup netmap_if fields. */
	memset(nmif, 0, netmap.netif_memsz);

	/* only ALL rings supported right now. */
	if (req->nr_ringid != 0)
		return -EINVAL;

	snprintf(nmif->ni_name, sizeof(nmif->ni_name), "%s", req->nr_name);
	nmif->ni_version  = req->nr_version;

	/* Netmap uses ni_(r|t)x_rings + 1 */
	nmif->ni_rx_rings = ports[port].nr_rx_rings - 1;
	nmif->ni_tx_rings = ports[port].nr_tx_rings - 1;

	/*
	 * Setup TX rings and slots.
	 * Refer to the comments in netmap.h for details
	 */

	slots = 0;
	for (i = 0; i < nmif->ni_tx_rings + 1; i++) {

		nmif->ring_ofs[i] = NETMAP_IF_RING_OFS(i,
			PORT_NUM_RINGS, slots);

		ring = NETMAP_TXRING(nmif, i);
		netmap_ring_setup(ring, port, i, ports[port].nr_tx_slots);
		ring->avail = ring->num_slots;

		slots += ports[port].nr_tx_slots;
	}

	/*
	 * Setup  RX rings and slots.
	 * Refer to the comments in netmap.h for details
	 */

	start_ring = i;

	for (; i < nmif->ni_rx_rings + 1 + start_ring; i++) {

		nmif->ring_ofs[i] = NETMAP_IF_RING_OFS(i,
			PORT_NUM_RINGS, slots);

		ring = NETMAP_RXRING(nmif, (i - start_ring));
		netmap_ring_setup(ring, port, i, ports[port].nr_rx_slots);
		ring->avail = 0;

		slots += ports[port].nr_rx_slots;
	}

	if ((rc = rte_eth_dev_start(port)) < 0) {
		RTE_LOG(ERR, USER1,
			"Couldn't start ethernet device %s (error %d)\n",
			req->nr_name, rc);
	    return rc;
	}

	/* setup fdi <--> port relationtip. */
	ports[port].fd = idx;
	fd_port[idx].port = port;

	req->nr_memsize = netmap.mem_sz;
	req->nr_offset = (uintptr_t)nmif - (uintptr_t)netmap.mem;

	return 0;
}

/**
 * Simulate a Netmap NIOCREGIF ioctl:
 */
static int
ioctl_niocregif(int32_t fd, void * param)
{
	uint16_t portid;
	int32_t rc;
	uint32_t idx;
	struct nmreq *req;

	req = (struct nmreq *)param;
	if ((rc = check_nmreq(req, &portid)) != 0)
		return rc;

	idx = FD_TO_IDX(fd);

	rte_spinlock_lock(&netmap_lock);
	rc = netmap_regif(req, idx, portid);
	rte_spinlock_unlock(&netmap_lock);

	return rc;
}

static void
netmap_unregif(uint32_t idx, uint32_t port)
{
	fd_port[idx].port = FD_PORT_RSRV;
	ports[port].fd = UINT32_MAX;
	rte_eth_dev_stop(port);
}

/**
 * Simulate a Netmap NIOCUNREGIF ioctl: put an interface running in Netmap
 * mode back in "normal" mode. In our case, we just stop the port associated
 * with this file descriptor.
 */
static int
ioctl_niocunregif(int fd)
{
	uint32_t idx, port;
	int32_t rc;

	idx = FD_TO_IDX(fd);

	rte_spinlock_lock(&netmap_lock);

	port = fd_port[idx].port;
	if (port < RTE_DIM(ports) && ports[port].fd == idx) {
		netmap_unregif(idx, port);
		rc = 0;
	} else {
		RTE_LOG(ERR, USER1,
			"%s: %d is not associated with valid port\n",
			__func__, fd);
		rc = -EINVAL;
	}

	rte_spinlock_unlock(&netmap_lock);
	return rc;
}

/**
 * A call to rx_sync_ring will try to fill a Netmap RX ring with as many
 * packets as it can hold coming from its dpdk port.
 */
static inline int
rx_sync_ring(struct netmap_ring *ring, uint16_t port, uint16_t ring_number,
	uint16_t max_burst)
{
	int32_t i, n_rx;
	uint16_t burst_size;
	uint32_t cur_slot, n_free_slots;
	struct rte_mbuf *rx_mbufs[COMPAT_NETMAP_MAX_BURST];

	n_free_slots = ring->num_slots - (ring->avail + ring->reserved);
	n_free_slots = RTE_MIN(n_free_slots, max_burst);
	cur_slot = (ring->cur + ring->avail) & (ring->num_slots - 1);

	while (n_free_slots) {
		burst_size = (uint16_t)RTE_MIN(n_free_slots, RTE_DIM(rx_mbufs));

		/* receive up to burst_size packets from the NIC's queue */
		n_rx = rte_eth_rx_burst(port, ring_number, rx_mbufs,
			burst_size);

		if (n_rx == 0)
			return 0;
		if (unlikely(n_rx < 0))
			return -1;

		/* Put those n_rx packets in the Netmap structures */
		for (i = 0; i < n_rx ; i++) {
			mbuf_to_slot(rx_mbufs[i], ring, cur_slot);
			rte_pktmbuf_free(rx_mbufs[i]);
			cur_slot = NETMAP_RING_NEXT(ring, cur_slot);
		}

		/* Update the Netmap ring structure to reflect the change */
		ring->avail += n_rx;
		n_free_slots -= n_rx;
	}

	return 0;
}

static inline int
rx_sync_if(uint32_t port)
{
	uint16_t burst;
	uint32_t i, rc;
	struct netmap_if *nifp;
	struct netmap_ring *r;

	nifp = ports[port].nmif;
	burst = ports[port].rx_burst;
	rc = 0;

	for (i = 0; i < nifp->ni_rx_rings + 1; i++) {
		r = NETMAP_RXRING(nifp, i);
		rx_sync_ring(r, port, (uint16_t)i, burst);
		rc += r->avail;
	}

	return rc;
}

/**
 * Simulate a Netmap NIOCRXSYNC ioctl:
 */
static int
ioctl_niocrxsync(int fd)
{
	uint32_t idx, port;

	idx = FD_TO_IDX(fd);
	if ((port = fd_port[idx].port) < RTE_DIM(ports) &&
			ports[port].fd == idx) {
		return rx_sync_if(fd_port[idx].port);
	} else  {
		return -EINVAL;
	}
}

/**
 * A call to tx_sync_ring will try to empty a Netmap TX ring by converting its
 * buffers into rte_mbufs and sending them out on the rings's dpdk port.
 */
static int
tx_sync_ring(struct netmap_ring *ring, uint16_t port, uint16_t ring_number,
	struct rte_mempool *pool, uint16_t max_burst)
{
	uint32_t i, n_tx;
	uint16_t burst_size;
	uint32_t cur_slot, n_used_slots;
	struct rte_mbuf *tx_mbufs[COMPAT_NETMAP_MAX_BURST];

	n_used_slots = ring->num_slots - ring->avail;
	n_used_slots = RTE_MIN(n_used_slots, max_burst);
	cur_slot = (ring->cur + ring->avail) & (ring->num_slots - 1);

	while (n_used_slots) {
		burst_size = (uint16_t)RTE_MIN(n_used_slots, RTE_DIM(tx_mbufs));

		for (i = 0; i < burst_size; i++) {
			tx_mbufs[i] = rte_pktmbuf_alloc(pool);
			if (tx_mbufs[i] == NULL)
				goto err;

			slot_to_mbuf(ring, cur_slot, tx_mbufs[i]);
			cur_slot = NETMAP_RING_NEXT(ring, cur_slot);
		}

		n_tx = rte_eth_tx_burst(port, ring_number, tx_mbufs,
			burst_size);

		/* Update the Netmap ring structure to reflect the change */
		ring->avail += n_tx;
		n_used_slots -= n_tx;

		/* Return the mbufs that failed to transmit to their pool */
		if (unlikely(n_tx != burst_size)) {
			for (i = n_tx; i < burst_size; i++)
				rte_pktmbuf_free(tx_mbufs[i]);
	        	break;
		}
	}

	return 0;

err:
	for (; i == 0; --i)
		rte_pktmbuf_free(tx_mbufs[i]);

	RTE_LOG(ERR, USER1,
		"Couldn't get mbuf from mempool is the mempool too small?\n");
	return -1;
}

static int
tx_sync_if(uint32_t port)
{
	uint16_t burst;
	uint32_t i, rc;
	struct netmap_if *nifp;
	struct netmap_ring *r;
	struct rte_mempool *mp;

	nifp = ports[port].nmif;
	mp = ports[port].pool;
	burst = ports[port].tx_burst;
	rc = 0;

	for (i = 0; i < nifp->ni_tx_rings + 1; i++) {
		r = NETMAP_TXRING(nifp, i);
		tx_sync_ring(r, port, (uint16_t)i, mp, burst);
		rc += r->avail;
	}

	return rc;
}

/**
 * Simulate a Netmap NIOCTXSYNC ioctl:
 */
static inline int
ioctl_nioctxsync(int fd)
{
	uint32_t idx, port;

	idx = FD_TO_IDX(fd);
	if ((port = fd_port[idx].port) < RTE_DIM(ports) &&
			ports[port].fd == idx) {
		return tx_sync_if(fd_port[idx].port);
	} else  {
		return -EINVAL;
	}
}

/**
 * Give the library a mempool of rte_mbufs with which it can do the
 * rte_mbuf <--> netmap slot conversions.
 */
int
rte_netmap_init(const struct rte_netmap_conf *conf)
{
	size_t buf_ofs, nmif_sz, sz;
	size_t port_rings, port_slots, port_bufs;
	uint32_t i, port_num;

	port_num = RTE_MAX_ETHPORTS;
	port_rings = 2 * conf->max_rings;
	port_slots = port_rings * conf->max_slots;
	port_bufs = port_slots;

	nmif_sz = NETMAP_IF_RING_OFS(port_rings, port_rings, port_slots);
	sz = nmif_sz * port_num;

	buf_ofs = RTE_ALIGN_CEIL(sz, RTE_CACHE_LINE_SIZE);
	sz = buf_ofs + port_bufs * conf->max_bufsz * port_num;

	if (sz > UINT32_MAX ||
			(netmap.mem = rte_zmalloc_socket(__func__, sz,
			RTE_CACHE_LINE_SIZE, conf->socket_id)) == NULL) {
		RTE_LOG(ERR, USER1, "%s: failed to allocate %zu bytes\n",
			__func__, sz);
		return -ENOMEM;
	}

	netmap.mem_sz = sz;
	netmap.netif_memsz = nmif_sz;
	netmap.buf_start = (uintptr_t)netmap.mem + buf_ofs;
	netmap.conf = *conf;

	rte_spinlock_init(&netmap_lock);

	/* Mark all ports as unused and set NETIF pointer. */
	for (i = 0; i != RTE_DIM(ports); i++) {
		ports[i].fd = UINT32_MAX;
		ports[i].nmif = (struct netmap_if *)
			((uintptr_t)netmap.mem + nmif_sz * i);
	}

	/* Mark all fd_ports as unused. */
	for (i = 0; i != RTE_DIM(fd_port); i++) {
		fd_port[i].port = FD_PORT_FREE;
	}

	return 0;
}


int
rte_netmap_init_port(uint16_t portid, const struct rte_netmap_port_conf *conf)
{
	int32_t ret;
	uint16_t i;
	uint16_t rx_slots, tx_slots;
	struct rte_eth_rxconf rxq_conf;
	struct rte_eth_txconf txq_conf;
	struct rte_eth_dev_info dev_info;

	if (conf == NULL ||
			portid >= RTE_DIM(ports) ||
			conf->nr_tx_rings > netmap.conf.max_rings ||
			conf->nr_rx_rings > netmap.conf.max_rings) {
		RTE_LOG(ERR, USER1, "%s(%u): invalid parameters\n",
			__func__, portid);
		return -EINVAL;
	}

	rx_slots = (uint16_t)rte_align32pow2(conf->nr_rx_slots);
	tx_slots = (uint16_t)rte_align32pow2(conf->nr_tx_slots);

	if (tx_slots > netmap.conf.max_slots ||
			rx_slots > netmap.conf.max_slots) {
		RTE_LOG(ERR, USER1, "%s(%u): invalid parameters\n",
			__func__, portid);
		return -EINVAL;
	}

	rte_eth_dev_info_get(portid, &dev_info);
	if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_MBUF_FAST_FREE)
		conf->eth_conf->txmode.offloads |=
			DEV_TX_OFFLOAD_MBUF_FAST_FREE;
	ret = rte_eth_dev_configure(portid, conf->nr_rx_rings,
		conf->nr_tx_rings, conf->eth_conf);

	if (ret < 0) {
		RTE_LOG(ERR, USER1, "Couldn't configure port %u\n", portid);
		return ret;
	}

	ret = rte_eth_dev_adjust_nb_rx_tx_desc(portid, &rx_slots, &tx_slots);

	if (ret < 0) {
		RTE_LOG(ERR, USER1,
			"Couldn't ot adjust number of descriptors for port %u\n",
			portid);
		return ret;
	}

	rxq_conf = dev_info.default_rxconf;
	rxq_conf.offloads = conf->eth_conf->rxmode.offloads;
	txq_conf = dev_info.default_txconf;
	txq_conf.offloads = conf->eth_conf->txmode.offloads;
	for (i = 0; i < conf->nr_tx_rings; i++) {
		ret = rte_eth_tx_queue_setup(portid, i, tx_slots,
			conf->socket_id, &txq_conf);

		if (ret < 0) {
			RTE_LOG(ERR, USER1,
				"fail to configure TX queue %u of port %u\n",
				i, portid);
			return ret;
		}

		ret = rte_eth_rx_queue_setup(portid, i, rx_slots,
			conf->socket_id, &rxq_conf, conf->pool);

		if (ret < 0) {
			RTE_LOG(ERR, USER1,
				"fail to configure RX queue %u of port %u\n",
				i, portid);
			return ret;
		}
	}

	/* copy config to the private storage. */
	ports[portid].eth_conf = conf->eth_conf[0];
	ports[portid].pool = conf->pool;
	ports[portid].socket_id = conf->socket_id;
	ports[portid].nr_tx_rings = conf->nr_tx_rings;
	ports[portid].nr_rx_rings = conf->nr_rx_rings;
	ports[portid].nr_tx_slots = tx_slots;
	ports[portid].nr_rx_slots = rx_slots;
	ports[portid].tx_burst = conf->tx_burst;
	ports[portid].rx_burst = conf->rx_burst;

	return 0;
}

int
rte_netmap_close(__rte_unused int fd)
{
	int32_t rc;

	rte_spinlock_lock(&netmap_lock);
	rc = fd_release(fd);
	rte_spinlock_unlock(&netmap_lock);

	if (rc < 0) {
		errno =-rc;
		rc = -1;
	}
	return rc;
}

int rte_netmap_ioctl(int fd, uint32_t op, void *param)
{
	int ret;

	if (!FD_VALID(fd)) {
	    errno = EBADF;
	    return -1;
	}

	switch (op) {

	    case NIOCGINFO:
	        ret = ioctl_niocginfo(fd, param);
	        break;

	    case NIOCREGIF:
	        ret = ioctl_niocregif(fd, param);
	        break;

	    case NIOCUNREGIF:
	        ret = ioctl_niocunregif(fd);
	        break;

	    case NIOCRXSYNC:
	        ret = ioctl_niocrxsync(fd);
	        break;

	    case NIOCTXSYNC:
	        ret = ioctl_nioctxsync(fd);
	        break;

	    default:
	        ret = -ENOTTY;
	}

	if (ret < 0) {
		errno = -ret;
		ret = -1;
	} else {
		ret = 0;
	}

	return ret;
}

void *
rte_netmap_mmap(void *addr, size_t length,
	int prot, int flags, int fd, off_t offset)
{
	static const int cprot = PROT_WRITE | PROT_READ;

	if (!FD_VALID(fd) || length + offset > netmap.mem_sz ||
			(prot & cprot) != cprot ||
			((flags & MAP_FIXED) != 0 && addr != NULL)) {

		errno = EINVAL;
		return MAP_FAILED;
	}

	return (void *)((uintptr_t)netmap.mem + (uintptr_t)offset);
}

/**
 * Return a "fake" file descriptor with a value above RLIMIT_NOFILE so that
 * any attempt to use that file descriptor with the usual API will fail.
 */
int
rte_netmap_open(__rte_unused const char *pathname, __rte_unused int flags)
{
	int fd;

	rte_spinlock_lock(&netmap_lock);
	fd = fd_reserve();
	rte_spinlock_unlock(&netmap_lock);

	if (fd < 0) {
		errno = -fd;
		fd = -1;
	}
	return fd;
}

/**
 * Doesn't support timeout other than 0 or infinite (negative) timeout
 */
int
rte_netmap_poll(struct pollfd *fds, nfds_t nfds, int timeout)
{
	int32_t count_it, ret;
	uint32_t i, idx, port;
	uint32_t want_rx, want_tx;

	if (timeout > 0)
		return -1;

	ret = 0;
	do {
		for (i = 0; i < nfds; i++) {

			count_it = 0;

			if (!FD_VALID(fds[i].fd) || fds[i].events == 0) {
				fds[i].revents = 0;
				continue;
			}

			idx = FD_TO_IDX(fds[i].fd);
			if ((port = fd_port[idx].port) >= RTE_DIM(ports) ||
		ports[port].fd != idx) {

				fds[i].revents |= POLLERR;
				ret++;
				continue;
			}

			want_rx = fds[i].events & (POLLIN  | POLLRDNORM);
			want_tx = fds[i].events & (POLLOUT | POLLWRNORM);

			if (want_rx && rx_sync_if(port) > 0) {
				fds[i].revents = (uint16_t)
					(fds[i].revents | want_rx);
				count_it = 1;
			}
			if (want_tx && tx_sync_if(port) > 0) {
				fds[i].revents = (uint16_t)
					(fds[i].revents | want_tx);
				count_it = 1;
			}

			ret += count_it;
		}
	}
	while ((ret == 0 && timeout < 0) || timeout);

	return ret;
}
