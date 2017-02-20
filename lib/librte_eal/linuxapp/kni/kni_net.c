/*-
 * GPL LICENSE SUMMARY
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of version 2 of the GNU General Public License as
 *   published by the Free Software Foundation.
 *
 *   This program is distributed in the hope that it will be useful, but
 *   WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *   The full GNU General Public License is included in this distribution
 *   in the file called LICENSE.GPL.
 *
 *   Contact Information:
 *   Intel Corporation
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
#include <linux/skbuff.h>
#include <linux/kthread.h>
#include <linux/delay.h>

#include <exec-env/rte_kni_common.h>
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
			(unsigned long)m->buf_physaddr);
	return va;
}

/* mbuf data kernel virtual address from mbuf kernel virtual address */
static void *
kva2data_kva(struct rte_kni_mbuf *m)
{
	return phys_to_virt(m->buf_physaddr + m->data_off);
}

/* virtual address to physical address */
static void *
va2pa(void *va, struct rte_kni_mbuf *m)
{
	void *pa;

	pa = (void *)((unsigned long)va -
			((unsigned long)m->buf_addr -
			 (unsigned long)m->buf_physaddr));
	return pa;
}

/*
 * It can be called to process the request.
 */
static int
kni_net_process_request(struct kni_dev *kni, struct rte_kni_request *req)
{
	int ret = -1;
	void *resp_va;
	uint32_t num;
	int ret_val;

	if (!kni || !req) {
		pr_err("No kni instance or request\n");
		return -EINVAL;
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
	ret = 0;

fail:
	mutex_unlock(&kni->sync_lock);
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
	struct kni_dev *kni = netdev_priv(dev);

	netif_start_queue(dev);

	memset(&req, 0, sizeof(req));
	req.req_id = RTE_KNI_REQ_CFG_NETWORK_IF;

	/* Setting if_up to non-zero means up */
	req.if_up = 1;
	ret = kni_net_process_request(kni, &req);

	return (ret == 0) ? req.result : ret;
}

static int
kni_net_release(struct net_device *dev)
{
	int ret;
	struct rte_kni_request req;
	struct kni_dev *kni = netdev_priv(dev);

	netif_stop_queue(dev); /* can't transmit any more */

	memset(&req, 0, sizeof(req));
	req.req_id = RTE_KNI_REQ_CFG_NETWORK_IF;

	/* Setting if_up to 0 means down */
	req.if_up = 0;
	ret = kni_net_process_request(kni, &req);

	return (ret == 0) ? req.result : ret;
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

		pkt_kva = pa2kva(pkt_pa);
		data_kva = kva2data_kva(pkt_kva);
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
	kni->stats.tx_bytes += len;
	kni->stats.tx_packets++;

	return NETDEV_TX_OK;

drop:
	/* Free skb and update statistics */
	dev_kfree_skb(skb);
	kni->stats.tx_dropped++;

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
	struct rte_kni_mbuf *kva;
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
		kva = pa2kva(kni->pa[i]);
		len = kva->pkt_len;
		data_kva = kva2data_kva(kva);
		kni->va[i] = pa2va(kni->pa[i], kva);

		skb = dev_alloc_skb(len + 2);
		if (!skb) {
			/* Update statistics */
			kni->stats.rx_dropped++;
			continue;
		}

		/* Align IP on 16B boundary */
		skb_reserve(skb, 2);

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

				kva = pa2kva(va2pa(kva->next, kva));
				data_kva = kva2data_kva(kva);
			}
		}

		skb->dev = dev;
		skb->protocol = eth_type_trans(skb, dev);
		skb->ip_summed = CHECKSUM_UNNECESSARY;

		/* Call netif interface */
		netif_rx_ni(skb);

		/* Update statistics */
		kni->stats.rx_bytes += len;
		kni->stats.rx_packets++;
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
	struct rte_kni_mbuf *kva;
	void *data_kva;
	struct rte_kni_mbuf *alloc_kva;
	void *alloc_data_kva;

	/* Get the number of entries in rx_q */
	num_rq = kni_fifo_count(kni->rx_q);

	/* Get the number of free entrie in tx_q */
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
			kva = pa2kva(kni->pa[i]);
			len = kva->pkt_len;
			data_kva = kva2data_kva(kva);
			kni->va[i] = pa2va(kni->pa[i], kva);

			alloc_kva = pa2kva(kni->alloc_pa[i]);
			alloc_data_kva = kva2data_kva(alloc_kva);
			kni->alloc_va[i] = pa2va(kni->alloc_pa[i], alloc_kva);

			memcpy(alloc_data_kva, data_kva, len);
			alloc_kva->pkt_len = len;
			alloc_kva->data_len = len;

			kni->stats.tx_bytes += len;
			kni->stats.rx_bytes += len;
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
	kni->stats.tx_packets += num;
	kni->stats.rx_packets += num;
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
	struct rte_kni_mbuf *kva;
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
		kva = pa2kva(kni->pa[i]);
		len = kva->pkt_len;
		data_kva = kva2data_kva(kva);
		kni->va[i] = pa2va(kni->pa[i], kva);

		skb = dev_alloc_skb(len + 2);
		if (skb) {
			/* Align IP on 16B boundary */
			skb_reserve(skb, 2);
			memcpy(skb_put(skb, len), data_kva, len);
			skb->dev = dev;
			skb->ip_summed = CHECKSUM_UNNECESSARY;
			dev_kfree_skb(skb);
		}

		/* Simulate real usage, allocate/copy skb twice */
		skb = dev_alloc_skb(len + 2);
		if (skb == NULL) {
			kni->stats.rx_dropped++;
			continue;
		}

		/* Align IP on 16B boundary */
		skb_reserve(skb, 2);

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

				kva = pa2kva(va2pa(kva->next, kva));
				data_kva = kva2data_kva(kva);
			}
		}

		skb->dev = dev;
		skb->ip_summed = CHECKSUM_UNNECESSARY;

		kni->stats.rx_bytes += len;
		kni->stats.rx_packets++;

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
static void
kni_net_tx_timeout(struct net_device *dev)
{
	struct kni_dev *kni = netdev_priv(dev);

	pr_debug("Transmit timeout at %ld, latency %ld\n", jiffies,
			jiffies - dev_trans_start(dev));

	kni->stats.tx_errors++;
	netif_wake_queue(dev);
}

/*
 * Ioctl commands
 */
static int
kni_net_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	pr_debug("kni_net_ioctl group:%d cmd:%d\n",
		((struct kni_dev *)netdev_priv(dev))->group_id, cmd);

	return 0;
}

static void
kni_net_set_rx_mode(struct net_device *dev)
{
}

static int
kni_net_change_mtu(struct net_device *dev, int new_mtu)
{
	int ret;
	struct rte_kni_request req;
	struct kni_dev *kni = netdev_priv(dev);

	pr_debug("kni_net_change_mtu new mtu %d to be set\n", new_mtu);

	memset(&req, 0, sizeof(req));
	req.req_id = RTE_KNI_REQ_CHANGE_MTU;
	req.new_mtu = new_mtu;
	ret = kni_net_process_request(kni, &req);
	if (ret == 0 && req.result == 0)
		dev->mtu = new_mtu;

	return (ret == 0) ? req.result : ret;
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
 * Return statistics to the caller
 */
static struct net_device_stats *
kni_net_stats(struct net_device *dev)
{
	struct kni_dev *kni = netdev_priv(dev);

	return &kni->stats;
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
	struct sockaddr *addr = p;

	if (!is_valid_ether_addr((unsigned char *)(addr->sa_data)))
		return -EADDRNOTAVAIL;
	memcpy(netdev->dev_addr, addr->sa_data, netdev->addr_len);
	return 0;
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
#ifdef HAVE_REBUILD_HEADER
	.rebuild = kni_net_rebuild_header,
#endif /* < 4.1.0  */
	.cache   = NULL,  /* disable caching */
};

static const struct net_device_ops kni_net_netdev_ops = {
	.ndo_open = kni_net_open,
	.ndo_stop = kni_net_release,
	.ndo_set_config = kni_net_config,
	.ndo_start_xmit = kni_net_tx,
	.ndo_change_mtu = kni_net_change_mtu,
	.ndo_do_ioctl = kni_net_ioctl,
	.ndo_set_rx_mode = kni_net_set_rx_mode,
	.ndo_get_stats = kni_net_stats,
	.ndo_tx_timeout = kni_net_tx_timeout,
	.ndo_set_mac_address = kni_net_set_mac,
#ifdef HAVE_CHANGE_CARRIER_CB
	.ndo_change_carrier = kni_net_change_carrier,
#endif
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
	} else
		pr_debug("Incognizant parameter, loopback disabled");
}
