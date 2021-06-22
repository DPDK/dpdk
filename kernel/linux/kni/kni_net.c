// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright(c) 2010-2014 Intel Corporation.
 */

/*
 * This code is inspired from the book "Linux Device Drivers" by
 * Alessandro Rubini and Jonathan Corbet, published by O'Reilly & Associates
 */

#include <linux/device.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h> /* eth_type_trans */
#include <linux/ethtool.h>
#include <linux/skbuff.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/rtnetlink.h>

#include <rte_kni_common.h>
#include <kni_fifo.h>

#include "compat.h"
#include "kni_dev.h"

#define WD_TIMEOUT 5 /*jiffies */

#define KNI_WAIT_RESPONSE_TIMEOUT 300 /* 3 seconds */

/* typedef for rx function */
typedef void (*kni_net_rx_t)(struct kni_dev *kni);

static void kni_net_rx_normal(struct kni_dev *kni);

/* kni rx function pointer, with default to normal rx */
static kni_net_rx_t kni_net_rx_func = kni_net_rx_normal;

#ifdef HAVE_IOVA_TO_KVA_MAPPING_SUPPORT
/* iova to kernel virtual address */
static inline void *
iova2kva(struct kni_dev *kni, void *iova)
{
	return phys_to_virt(iova_to_phys(kni->usr_tsk, (unsigned long)iova));
}

static inline void *
iova2data_kva(struct kni_dev *kni, struct rte_kni_mbuf *m)
{
	return phys_to_virt(iova_to_phys(kni->usr_tsk, m->buf_iova) +
			    m->data_off);
}
#endif

/* physical address to kernel virtual address */
static void *
pa2kva(void *pa)
{
	return phys_to_virt((unsigned long)pa);
}

/* physical address to virtual address */
static void *
pa2va(void *pa, struct rte_kni_mbuf *m)
{
	void *va;

	va = (void *)((unsigned long)pa +
			(unsigned long)m->buf_addr -
			(unsigned long)m->buf_iova);
	return va;
}

/* mbuf data kernel virtual address from mbuf kernel virtual address */
static void *
kva2data_kva(struct rte_kni_mbuf *m)
{
	return phys_to_virt(m->buf_iova + m->data_off);
}

static inline void *
get_kva(struct kni_dev *kni, void *pa)
{
#ifdef HAVE_IOVA_TO_KVA_MAPPING_SUPPORT
	if (kni->iova_mode == 1)
		return iova2kva(kni, pa);
#endif
	return pa2kva(pa);
}

static inline void *
get_data_kva(struct kni_dev *kni, void *pkt_kva)
{
#ifdef HAVE_IOVA_TO_KVA_MAPPING_SUPPORT
	if (kni->iova_mode == 1)
		return iova2data_kva(kni, pkt_kva);
#endif
	return kva2data_kva(pkt_kva);
}

/*
 * It can be called to process the request.
 */
static int
kni_net_process_request(struct net_device *dev, struct rte_kni_request *req)
{
	struct kni_dev *kni = netdev_priv(dev);
	int ret = -1;
	void *resp_va;
	uint32_t num;
	int ret_val;

	ASSERT_RTNL();

	/* If we need to wait and RTNL mutex is held
	 * drop the mutex and hold reference to keep device
	 */
	if (req->async == 0) {
		dev_hold(dev);
		rtnl_unlock();
	}

	mutex_lock(&kni->sync_lock);

	/* Construct data */
	memcpy(kni->sync_kva, req, sizeof(struct rte_kni_request));
	num = kni_fifo_put(kni->req_q, &kni->sync_va, 1);
	if (num < 1) {
		pr_err("Cannot send to req_q\n");
		ret = -EBUSY;
		goto fail;
	}

	/* No result available since request is handled
	 * asynchronously. set response to success.
	 */
	if (req->async != 0) {
		req->result = 0;
		goto async;
	}

	ret_val = wait_event_interruptible_timeout(kni->wq,
			kni_fifo_count(kni->resp_q), 3 * HZ);
	if (signal_pending(current) || ret_val <= 0) {
		ret = -ETIME;
		goto fail;
	}
	num = kni_fifo_get(kni->resp_q, (void **)&resp_va, 1);
	if (num != 1 || resp_va != kni->sync_va) {
		/* This should never happen */
		pr_err("No data in resp_q\n");
		ret = -ENODATA;
		goto fail;
	}

	memcpy(req, kni->sync_kva, sizeof(struct rte_kni_request));
async:
	ret = 0;

fail:
	mutex_unlock(&kni->sync_lock);
	if (req->async == 0) {
		rtnl_lock();
		dev_put(dev);
	}
	return ret;
}

/*
 * Open and close
 */
static int
kni_net_open(struct net_device *dev)
{
	int ret;
	struct rte_kni_request req;

	netif_start_queue(dev);
	if (kni_dflt_carrier == 1)
		netif_carrier_on(dev);
	else
		netif_carrier_off(dev);

	memset(&req, 0, sizeof(req));
	req.req_id = RTE_KNI_REQ_CFG_NETWORK_IF;

	/* Setting if_up to non-zero means up */
	req.if_up = 1;
	ret = kni_net_process_request(dev, &req);

	return (ret == 0) ? req.result : ret;
}

static int
kni_net_release(struct net_device *dev)
{
	int ret;
	struct rte_kni_request req;

	netif_stop_queue(dev); /* can't transmit any more */
	netif_carrier_off(dev);

	memset(&req, 0, sizeof(req));
	req.req_id = RTE_KNI_REQ_CFG_NETWORK_IF;

	/* Setting if_up to 0 means down */
	req.if_up = 0;

	/* request async because of the deadlock problem */
	req.async = 1;

	ret = kni_net_process_request(dev, &req);

	return (ret == 0) ? req.result : ret;
}

static void
kni_fifo_trans_pa2va(struct kni_dev *kni,
	struct rte_kni_fifo *src_pa, struct rte_kni_fifo *dst_va)
{
	uint32_t ret, i, num_dst, num_rx;
	struct rte_kni_mbuf *kva, *prev_kva;
	int nb_segs;
	int kva_nb_segs;

	do {
		num_dst = kni_fifo_free_count(dst_va);
		if (num_dst == 0)
			return;

		num_rx = min_t(uint32_t, num_dst, MBUF_BURST_SZ);

		num_rx = kni_fifo_get(src_pa, kni->pa, num_rx);
		if (num_rx == 0)
			return;

		for (i = 0; i < num_rx; i++) {
			kva = get_kva(kni, kni->pa[i]);
			kni->va[i] = pa2va(kni->pa[i], kva);

			kva_nb_segs = kva->nb_segs;
			for (nb_segs = 0; nb_segs < kva_nb_segs; nb_segs++) {
				if (!kva->next)
					break;

				prev_kva = kva;
				kva = get_kva(kni, kva->next);
				/* Convert physical address to virtual address */
				prev_kva->next = pa2va(prev_kva->next, kva);
			}
		}

		ret = kni_fifo_put(dst_va, kni->va, num_rx);
		if (ret != num_rx) {
			/* Failing should not happen */
			pr_err("Fail to enqueue entries into dst_va\n");
			return;
		}
	} while (1);
}

/* Try to release mbufs when kni release */
void kni_net_release_fifo_phy(struct kni_dev *kni)
{
	/* release rx_q first, because it can't release in userspace */
	kni_fifo_trans_pa2va(kni, kni->rx_q, kni->free_q);
	/* release alloc_q for speeding up kni release in userspace */
	kni_fifo_trans_pa2va(kni, kni->alloc_q, kni->free_q);
}

/*
 * Configuration changes (passed on by ifconfig)
 */
static int
kni_net_config(struct net_device *dev, struct ifmap *map)
{
	if (dev->flags & IFF_UP) /* can't act on a running interface */
		return -EBUSY;

	/* ignore other fields */
	return 0;
}

/*
 * Transmit a packet (called by the kernel)
 */
static int
kni_net_tx(struct sk_buff *skb, struct net_device *dev)
{
	int len = 0;
	uint32_t ret;
	struct kni_dev *kni = netdev_priv(dev);
	struct rte_kni_mbuf *pkt_kva = NULL;
	void *pkt_pa = NULL;
	void *pkt_va = NULL;

	/* save the timestamp */
#ifdef HAVE_TRANS_START_HELPER
	netif_trans_update(dev);
#else
	dev->trans_start = jiffies;
#endif

	/* Check if the length of skb is less than mbuf size */
	if (skb->len > kni->mbuf_size)
		goto drop;

	/**
	 * Check if it has at least one free entry in tx_q and
	 * one entry in alloc_q.
	 */
	if (kni_fifo_free_count(kni->tx_q) == 0 ||
			kni_fifo_count(kni->alloc_q) == 0) {
		/**
		 * If no free entry in tx_q or no entry in alloc_q,
		 * drops skb and goes out.
		 */
		goto drop;
	}

	/* dequeue a mbuf from alloc_q */
	ret = kni_fifo_get(kni->alloc_q, &pkt_pa, 1);
	if (likely(ret == 1)) {
		void *data_kva;

		pkt_kva = get_kva(kni, pkt_pa);
		data_kva = get_data_kva(kni, pkt_kva);
		pkt_va = pa2va(pkt_pa, pkt_kva);

		len = skb->len;
		memcpy(data_kva, skb->data, len);
		if (unlikely(len < ETH_ZLEN)) {
			memset(data_kva + len, 0, ETH_ZLEN - len);
			len = ETH_ZLEN;
		}
		pkt_kva->pkt_len = len;
		pkt_kva->data_len = len;

		/* enqueue mbuf into tx_q */
		ret = kni_fifo_put(kni->tx_q, &pkt_va, 1);
		if (unlikely(ret != 1)) {
			/* Failing should not happen */
			pr_err("Fail to enqueue mbuf into tx_q\n");
			goto drop;
		}
	} else {
		/* Failing should not happen */
		pr_err("Fail to dequeue mbuf from alloc_q\n");
		goto drop;
	}

	/* Free skb and update statistics */
	dev_kfree_skb(skb);
	dev->stats.tx_bytes += len;
	dev->stats.tx_packets++;

	return NETDEV_TX_OK;

drop:
	/* Free skb and update statistics */
	dev_kfree_skb(skb);
	dev->stats.tx_dropped++;

	return NETDEV_TX_OK;
}

/*
 * RX: normal working mode
 */
static void
kni_net_rx_normal(struct kni_dev *kni)
{
	uint32_t ret;
	uint32_t len;
	uint32_t i, num_rx, num_fq;
	struct rte_kni_mbuf *kva, *prev_kva;
	void *data_kva;
	struct sk_buff *skb;
	struct net_device *dev = kni->net_dev;

	/* Get the number of free entries in free_q */
	num_fq = kni_fifo_free_count(kni->free_q);
	if (num_fq == 0) {
		/* No room on the free_q, bail out */
		return;
	}

	/* Calculate the number of entries to dequeue from rx_q */
	num_rx = min_t(uint32_t, num_fq, MBUF_BURST_SZ);

	/* Burst dequeue from rx_q */
	num_rx = kni_fifo_get(kni->rx_q, kni->pa, num_rx);
	if (num_rx == 0)
		return;

	/* Transfer received packets to netif */
	for (i = 0; i < num_rx; i++) {
		kva = get_kva(kni, kni->pa[i]);
		len = kva->pkt_len;
		data_kva = get_data_kva(kni, kva);
		kni->va[i] = pa2va(kni->pa[i], kva);

		skb = netdev_alloc_skb(dev, len);
		if (!skb) {
			/* Update statistics */
			dev->stats.rx_dropped++;
			continue;
		}

		if (kva->nb_segs == 1) {
			memcpy(skb_put(skb, len), data_kva, len);
		} else {
			int nb_segs;
			int kva_nb_segs = kva->nb_segs;

			for (nb_segs = 0; nb_segs < kva_nb_segs; nb_segs++) {
				memcpy(skb_put(skb, kva->data_len),
					data_kva, kva->data_len);

				if (!kva->next)
					break;

				prev_kva = kva;
				kva = get_kva(kni, kva->next);
				data_kva = kva2data_kva(kva);
				/* Convert physical address to virtual address */
				prev_kva->next = pa2va(prev_kva->next, kva);
			}
		}

		skb->protocol = eth_type_trans(skb, dev);
		skb->ip_summed = CHECKSUM_UNNECESSARY;

		/* Call netif interface */
		netif_rx_ni(skb);

		/* Update statistics */
		dev->stats.rx_bytes += len;
		dev->stats.rx_packets++;
	}

	/* Burst enqueue mbufs into free_q */
	ret = kni_fifo_put(kni->free_q, kni->va, num_rx);
	if (ret != num_rx)
		/* Failing should not happen */
		pr_err("Fail to enqueue entries into free_q\n");
}

/*
 * RX: loopback with enqueue/dequeue fifos.
 */
static void
kni_net_rx_lo_fifo(struct kni_dev *kni)
{
	uint32_t ret;
	uint32_t len;
	uint32_t i, num, num_rq, num_tq, num_aq, num_fq;
	struct rte_kni_mbuf *kva, *next_kva;
	void *data_kva;
	struct rte_kni_mbuf *alloc_kva;
	void *alloc_data_kva;
	struct net_device *dev = kni->net_dev;

	/* Get the number of entries in rx_q */
	num_rq = kni_fifo_count(kni->rx_q);

	/* Get the number of free entries in tx_q */
	num_tq = kni_fifo_free_count(kni->tx_q);

	/* Get the number of entries in alloc_q */
	num_aq = kni_fifo_count(kni->alloc_q);

	/* Get the number of free entries in free_q */
	num_fq = kni_fifo_free_count(kni->free_q);

	/* Calculate the number of entries to be dequeued from rx_q */
	num = min(num_rq, num_tq);
	num = min(num, num_aq);
	num = min(num, num_fq);
	num = min_t(uint32_t, num, MBUF_BURST_SZ);

	/* Return if no entry to dequeue from rx_q */
	if (num == 0)
		return;

	/* Burst dequeue from rx_q */
	ret = kni_fifo_get(kni->rx_q, kni->pa, num);
	if (ret == 0)
		return; /* Failing should not happen */

	/* Dequeue entries from alloc_q */
	ret = kni_fifo_get(kni->alloc_q, kni->alloc_pa, num);
	if (ret) {
		num = ret;
		/* Copy mbufs */
		for (i = 0; i < num; i++) {
			kva = get_kva(kni, kni->pa[i]);
			len = kva->data_len;
			data_kva = get_data_kva(kni, kva);
			kni->va[i] = pa2va(kni->pa[i], kva);

			while (kva->next) {
				next_kva = get_kva(kni, kva->next);
				/* Convert physical address to virtual address */
				kva->next = pa2va(kva->next, next_kva);
				kva = next_kva;
			}

			alloc_kva = get_kva(kni, kni->alloc_pa[i]);
			alloc_data_kva = get_data_kva(kni, alloc_kva);
			kni->alloc_va[i] = pa2va(kni->alloc_pa[i], alloc_kva);

			memcpy(alloc_data_kva, data_kva, len);
			alloc_kva->pkt_len = len;
			alloc_kva->data_len = len;

			dev->stats.tx_bytes += len;
			dev->stats.rx_bytes += len;
		}

		/* Burst enqueue mbufs into tx_q */
		ret = kni_fifo_put(kni->tx_q, kni->alloc_va, num);
		if (ret != num)
			/* Failing should not happen */
			pr_err("Fail to enqueue mbufs into tx_q\n");
	}

	/* Burst enqueue mbufs into free_q */
	ret = kni_fifo_put(kni->free_q, kni->va, num);
	if (ret != num)
		/* Failing should not happen */
		pr_err("Fail to enqueue mbufs into free_q\n");

	/**
	 * Update statistic, and enqueue/dequeue failure is impossible,
	 * as all queues are checked at first.
	 */
	dev->stats.tx_packets += num;
	dev->stats.rx_packets += num;
}

/*
 * RX: loopback with enqueue/dequeue fifos and sk buffer copies.
 */
static void
kni_net_rx_lo_fifo_skb(struct kni_dev *kni)
{
	uint32_t ret;
	uint32_t len;
	uint32_t i, num_rq, num_fq, num;
	struct rte_kni_mbuf *kva, *prev_kva;
	void *data_kva;
	struct sk_buff *skb;
	struct net_device *dev = kni->net_dev;

	/* Get the number of entries in rx_q */
	num_rq = kni_fifo_count(kni->rx_q);

	/* Get the number of free entries in free_q */
	num_fq = kni_fifo_free_count(kni->free_q);

	/* Calculate the number of entries to dequeue from rx_q */
	num = min(num_rq, num_fq);
	num = min_t(uint32_t, num, MBUF_BURST_SZ);

	/* Return if no entry to dequeue from rx_q */
	if (num == 0)
		return;

	/* Burst dequeue mbufs from rx_q */
	ret = kni_fifo_get(kni->rx_q, kni->pa, num);
	if (ret == 0)
		return;

	/* Copy mbufs to sk buffer and then call tx interface */
	for (i = 0; i < num; i++) {
		kva = get_kva(kni, kni->pa[i]);
		len = kva->pkt_len;
		data_kva = get_data_kva(kni, kva);
		kni->va[i] = pa2va(kni->pa[i], kva);

		skb = netdev_alloc_skb(dev, len);
		if (skb) {
			memcpy(skb_put(skb, len), data_kva, len);
			skb->ip_summed = CHECKSUM_UNNECESSARY;
			dev_kfree_skb(skb);
		}

		/* Simulate real usage, allocate/copy skb twice */
		skb = netdev_alloc_skb(dev, len);
		if (skb == NULL) {
			dev->stats.rx_dropped++;
			continue;
		}

		if (kva->nb_segs == 1) {
			memcpy(skb_put(skb, len), data_kva, len);
		} else {
			int nb_segs;
			int kva_nb_segs = kva->nb_segs;

			for (nb_segs = 0; nb_segs < kva_nb_segs; nb_segs++) {
				memcpy(skb_put(skb, kva->data_len),
					data_kva, kva->data_len);

				if (!kva->next)
					break;

				prev_kva = kva;
				kva = get_kva(kni, kva->next);
				data_kva = get_data_kva(kni, kva);
				/* Convert physical address to virtual address */
				prev_kva->next = pa2va(prev_kva->next, kva);
			}
		}

		skb->ip_summed = CHECKSUM_UNNECESSARY;

		dev->stats.rx_bytes += len;
		dev->stats.rx_packets++;

		/* call tx interface */
		kni_net_tx(skb, dev);
	}

	/* enqueue all the mbufs from rx_q into free_q */
	ret = kni_fifo_put(kni->free_q, kni->va, num);
	if (ret != num)
		/* Failing should not happen */
		pr_err("Fail to enqueue mbufs into free_q\n");
}

/* rx interface */
void
kni_net_rx(struct kni_dev *kni)
{
	/**
	 * It doesn't need to check if it is NULL pointer,
	 * as it has a default value
	 */
	(*kni_net_rx_func)(kni);
}

/*
 * Deal with a transmit timeout.
 */
#ifdef HAVE_TX_TIMEOUT_TXQUEUE
static void
kni_net_tx_timeout(struct net_device *dev, unsigned int txqueue)
#else
static void
kni_net_tx_timeout(struct net_device *dev)
#endif
{
	pr_debug("Transmit timeout at %ld, latency %ld\n", jiffies,
			jiffies - dev_trans_start(dev));

	dev->stats.tx_errors++;
	netif_wake_queue(dev);
}

static int
kni_net_change_mtu(struct net_device *dev, int new_mtu)
{
	int ret;
	struct rte_kni_request req;

	pr_debug("kni_net_change_mtu new mtu %d to be set\n", new_mtu);

	memset(&req, 0, sizeof(req));
	req.req_id = RTE_KNI_REQ_CHANGE_MTU;
	req.new_mtu = new_mtu;
	ret = kni_net_process_request(dev, &req);
	if (ret == 0 && req.result == 0)
		dev->mtu = new_mtu;

	return (ret == 0) ? req.result : ret;
}

static void
kni_net_change_rx_flags(struct net_device *netdev, int flags)
{
	struct rte_kni_request req;

	memset(&req, 0, sizeof(req));

	if (flags & IFF_ALLMULTI) {
		req.req_id = RTE_KNI_REQ_CHANGE_ALLMULTI;

		if (netdev->flags & IFF_ALLMULTI)
			req.allmulti = 1;
		else
			req.allmulti = 0;
	}

	if (flags & IFF_PROMISC) {
		req.req_id = RTE_KNI_REQ_CHANGE_PROMISC;

		if (netdev->flags & IFF_PROMISC)
			req.promiscusity = 1;
		else
			req.promiscusity = 0;
	}

	kni_net_process_request(netdev, &req);
}

/*
 * Checks if the user space application provided the resp message
 */
void
kni_net_poll_resp(struct kni_dev *kni)
{
	if (kni_fifo_count(kni->resp_q))
		wake_up_interruptible(&kni->wq);
}

/*
 *  Fill the eth header
 */
static int
kni_net_header(struct sk_buff *skb, struct net_device *dev,
		unsigned short type, const void *daddr,
		const void *saddr, uint32_t len)
{
	struct ethhdr *eth = (struct ethhdr *) skb_push(skb, ETH_HLEN);

	memcpy(eth->h_source, saddr ? saddr : dev->dev_addr, dev->addr_len);
	memcpy(eth->h_dest,   daddr ? daddr : dev->dev_addr, dev->addr_len);
	eth->h_proto = htons(type);

	return dev->hard_header_len;
}

/*
 * Re-fill the eth header
 */
#ifdef HAVE_REBUILD_HEADER
static int
kni_net_rebuild_header(struct sk_buff *skb)
{
	struct net_device *dev = skb->dev;
	struct ethhdr *eth = (struct ethhdr *) skb->data;

	memcpy(eth->h_source, dev->dev_addr, dev->addr_len);
	memcpy(eth->h_dest, dev->dev_addr, dev->addr_len);

	return 0;
}
#endif /* < 4.1.0  */

/**
 * kni_net_set_mac - Change the Ethernet Address of the KNI NIC
 * @netdev: network interface device structure
 * @p: pointer to an address structure
 *
 * Returns 0 on success, negative on failure
 **/
static int
kni_net_set_mac(struct net_device *netdev, void *p)
{
	int ret;
	struct rte_kni_request req;
	struct sockaddr *addr = p;

	memset(&req, 0, sizeof(req));
	req.req_id = RTE_KNI_REQ_CHANGE_MAC_ADDR;

	if (!is_valid_ether_addr((unsigned char *)(addr->sa_data)))
		return -EADDRNOTAVAIL;

	memcpy(req.mac_addr, addr->sa_data, netdev->addr_len);
	memcpy(netdev->dev_addr, addr->sa_data, netdev->addr_len);

	ret = kni_net_process_request(netdev, &req);

	return (ret == 0 ? req.result : ret);
}

#ifdef HAVE_CHANGE_CARRIER_CB
static int
kni_net_change_carrier(struct net_device *dev, bool new_carrier)
{
	if (new_carrier)
		netif_carrier_on(dev);
	else
		netif_carrier_off(dev);
	return 0;
}
#endif

static const struct header_ops kni_net_header_ops = {
	.create  = kni_net_header,
	.parse   = eth_header_parse,
#ifdef HAVE_REBUILD_HEADER
	.rebuild = kni_net_rebuild_header,
#endif /* < 4.1.0  */
	.cache   = NULL,  /* disable caching */
};

static const struct net_device_ops kni_net_netdev_ops = {
	.ndo_open = kni_net_open,
	.ndo_stop = kni_net_release,
	.ndo_set_config = kni_net_config,
	.ndo_change_rx_flags = kni_net_change_rx_flags,
	.ndo_start_xmit = kni_net_tx,
	.ndo_change_mtu = kni_net_change_mtu,
	.ndo_tx_timeout = kni_net_tx_timeout,
	.ndo_set_mac_address = kni_net_set_mac,
#ifdef HAVE_CHANGE_CARRIER_CB
	.ndo_change_carrier = kni_net_change_carrier,
#endif
};

static void kni_get_drvinfo(struct net_device *dev,
			    struct ethtool_drvinfo *info)
{
	strlcpy(info->version, KNI_VERSION, sizeof(info->version));
	strlcpy(info->driver, "kni", sizeof(info->driver));
}

static const struct ethtool_ops kni_net_ethtool_ops = {
	.get_drvinfo	= kni_get_drvinfo,
	.get_link	= ethtool_op_get_link,
};

void
kni_net_init(struct net_device *dev)
{
	struct kni_dev *kni = netdev_priv(dev);

	init_waitqueue_head(&kni->wq);
	mutex_init(&kni->sync_lock);

	ether_setup(dev); /* assign some of the fields */
	dev->netdev_ops      = &kni_net_netdev_ops;
	dev->header_ops      = &kni_net_header_ops;
	dev->ethtool_ops     = &kni_net_ethtool_ops;
	dev->watchdog_timeo = WD_TIMEOUT;
}

void
kni_net_config_lo_mode(char *lo_str)
{
	if (!lo_str) {
		pr_debug("loopback disabled");
		return;
	}

	if (!strcmp(lo_str, "lo_mode_none"))
		pr_debug("loopback disabled");
	else if (!strcmp(lo_str, "lo_mode_fifo")) {
		pr_debug("loopback mode=lo_mode_fifo enabled");
		kni_net_rx_func = kni_net_rx_lo_fifo;
	} else if (!strcmp(lo_str, "lo_mode_fifo_skb")) {
		pr_debug("loopback mode=lo_mode_fifo_skb enabled");
		kni_net_rx_func = kni_net_rx_lo_fifo_skb;
	} else {
		pr_debug("Unknown loopback parameter, disabled");
	}
}
