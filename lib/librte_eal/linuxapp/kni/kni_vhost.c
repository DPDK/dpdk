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

#include <linux/module.h>
#include <linux/net.h>
#include <net/sock.h>
#include <linux/virtio_net.h>
#include <linux/wait.h>
#include <linux/mm.h>
#include <linux/nsproxy.h>
#include <linux/sched.h>
#include <linux/if_tun.h>
#include <linux/version.h>

#include "compat.h"
#include "kni_dev.h"
#include "kni_fifo.h"

#define RX_BURST_SZ 4

extern void put_unused_fd(unsigned int fd);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,7,0)
extern struct file*
sock_alloc_file(struct socket *sock,
		int flags, const char *dname);

extern int get_unused_fd_flags(unsigned flags);

extern void fd_install(unsigned int fd, struct file *file);

static int kni_sock_map_fd(struct socket *sock)
{
	struct file *file;
	int fd = get_unused_fd_flags(0);
	if (fd < 0)
		return fd;

	file = sock_alloc_file(sock, 0, NULL);
	if (IS_ERR(file)) {
		put_unused_fd(fd);
		return PTR_ERR(file);
	}
	fd_install(fd, file);
	return fd;
}
#else
#define kni_sock_map_fd(s)             sock_map_fd(s, 0)
#endif

static struct proto kni_raw_proto = {
	.name = "kni_vhost",
	.owner = THIS_MODULE,
	.obj_size = sizeof(struct kni_vhost_queue),
};

static inline int
kni_vhost_net_tx(struct kni_dev *kni, struct iovec *iov,
		 unsigned offset, unsigned len)
{
	struct rte_kni_mbuf *pkt_kva = NULL;
	struct rte_kni_mbuf *pkt_va = NULL;
	int ret;

	KNI_DBG_TX("tx offset=%d, len=%d, iovlen=%d\n",
		   offset, len, (int)iov->iov_len);

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
	ret = kni_fifo_get(kni->alloc_q, (void **)&pkt_va, 1);
	if (likely(ret == 1)) {
		void *data_kva;

		pkt_kva = (void *)pkt_va - kni->mbuf_va + kni->mbuf_kva;
		data_kva = pkt_kva->buf_addr + pkt_kva->data_off
		           - kni->mbuf_va + kni->mbuf_kva;

		memcpy_fromiovecend(data_kva, iov, offset, len);
		if (unlikely(len < ETH_ZLEN)) {
			memset(data_kva + len, 0, ETH_ZLEN - len);
			len = ETH_ZLEN;
		}
		pkt_kva->pkt_len = len;
		pkt_kva->data_len = len;

		/* enqueue mbuf into tx_q */
		ret = kni_fifo_put(kni->tx_q, (void **)&pkt_va, 1);
		if (unlikely(ret != 1)) {
			/* Failing should not happen */
			KNI_ERR("Fail to enqueue mbuf into tx_q\n");
			goto drop;
		}
	} else {
		/* Failing should not happen */
		KNI_ERR("Fail to dequeue mbuf from alloc_q\n");
		goto drop;
	}

	/* update statistics */
	kni->stats.tx_bytes += len;
	kni->stats.tx_packets++;

	return 0;

drop:
	/* update statistics */
	kni->stats.tx_dropped++;

	return 0;
}

static inline int
kni_vhost_net_rx(struct kni_dev *kni, struct iovec *iov,
		 unsigned offset, unsigned len)
{
	uint32_t pkt_len;
	struct rte_kni_mbuf *kva;
	struct rte_kni_mbuf *va;
	void * data_kva;
	struct sk_buff *skb;
	struct kni_vhost_queue *q = kni->vhost_queue;

	if (unlikely(q == NULL))
		return 0;

	/* ensure at least one entry in free_q */
	if (unlikely(kni_fifo_free_count(kni->free_q) == 0))
		return 0;

	skb = skb_dequeue(&q->sk.sk_receive_queue);
	if (unlikely(skb == NULL))
		return 0;

	kva = (struct rte_kni_mbuf*)skb->data;

	/* free skb to cache */
	skb->data = NULL;
	if (unlikely(1 != kni_fifo_put(q->fifo, (void **)&skb, 1)))
		/* Failing should not happen */
		KNI_ERR("Fail to enqueue entries into rx cache fifo\n");

	pkt_len = kva->data_len;
	if (unlikely(pkt_len > len))
		goto drop;

	KNI_DBG_RX("rx offset=%d, len=%d, pkt_len=%d, iovlen=%d\n",
		   offset, len, pkt_len, (int)iov->iov_len);

	data_kva = kva->buf_addr + kva->data_off - kni->mbuf_va + kni->mbuf_kva;
	if (unlikely(memcpy_toiovecend(iov, data_kva, offset, pkt_len)))
		goto drop;

	/* Update statistics */
	kni->stats.rx_bytes += pkt_len;
	kni->stats.rx_packets++;

	/* enqueue mbufs into free_q */
	va = (void*)kva - kni->mbuf_kva + kni->mbuf_va;
	if (unlikely(1 != kni_fifo_put(kni->free_q, (void **)&va, 1)))
		/* Failing should not happen */
		KNI_ERR("Fail to enqueue entries into free_q\n");

	KNI_DBG_RX("receive done %d\n", pkt_len);

	return pkt_len;

drop:
	/* Update drop statistics */
	kni->stats.rx_dropped++;

	return 0;
}

static unsigned int
kni_sock_poll(struct file *file, struct socket *sock, poll_table * wait)
{
	struct kni_vhost_queue *q =
		container_of(sock->sk, struct kni_vhost_queue, sk);
	struct kni_dev *kni;
	unsigned int mask = 0;

	if (unlikely(q == NULL || q->kni == NULL))
		return POLLERR;

	kni = q->kni;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35)
	KNI_DBG("start kni_poll on group %d, wq 0x%16llx\n",
		  kni->group_id, (uint64_t)sock->wq);
#else
	KNI_DBG("start kni_poll on group %d, wait at 0x%16llx\n",
		  kni->group_id, (uint64_t)&sock->wait);
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35)
	poll_wait(file, &sock->wq->wait, wait);
#else
	poll_wait(file, &sock->wait, wait);
#endif

	if (kni_fifo_count(kni->rx_q) > 0)
		mask |= POLLIN | POLLRDNORM;

	if (sock_writeable(&q->sk) ||
	    (!test_and_set_bit(SOCK_ASYNC_NOSPACE, &q->sock->flags) &&
	     sock_writeable(&q->sk)))
		mask |= POLLOUT | POLLWRNORM;

	return mask;
}

static inline void
kni_vhost_enqueue(struct kni_dev *kni, struct kni_vhost_queue *q,
		  struct sk_buff *skb, struct rte_kni_mbuf *va)
{
	struct rte_kni_mbuf *kva;

	kva = (void *)(va) - kni->mbuf_va + kni->mbuf_kva;
	(skb)->data = (unsigned char*)kva;
	(skb)->len = kva->data_len;
	skb_queue_tail(&q->sk.sk_receive_queue, skb);
}

static inline void
kni_vhost_enqueue_burst(struct kni_dev *kni, struct kni_vhost_queue *q,
	  struct sk_buff **skb, struct rte_kni_mbuf **va)
{
	int i;
	for (i = 0; i < RX_BURST_SZ; skb++, va++, i++)
		kni_vhost_enqueue(kni, q, *skb, *va);
}

int
kni_chk_vhost_rx(struct kni_dev *kni)
{
	struct kni_vhost_queue *q = kni->vhost_queue;
	unsigned nb_in, nb_mbuf, nb_skb;
	const unsigned BURST_MASK = RX_BURST_SZ - 1;
	unsigned nb_burst, nb_backlog, i;
	struct sk_buff *skb[RX_BURST_SZ];
	struct rte_kni_mbuf *va[RX_BURST_SZ];

	if (unlikely(BE_STOP & kni->vq_status)) {
		kni->vq_status |= BE_FINISH;
		return 0;
	}

	if (unlikely(q == NULL))
		return 0;

	nb_skb = kni_fifo_count(q->fifo);
	nb_mbuf = kni_fifo_count(kni->rx_q);

	nb_in = min(nb_mbuf, nb_skb);
	nb_in = min(nb_in, (unsigned)RX_BURST_SZ);
	nb_burst   = (nb_in & ~BURST_MASK);
	nb_backlog = (nb_in & BURST_MASK);

	/* enqueue skb_queue per BURST_SIZE bulk */
	if (0 != nb_burst) {
		if (unlikely(RX_BURST_SZ != kni_fifo_get(
				     kni->rx_q, (void **)&va,
				     RX_BURST_SZ)))
			goto except;

		if (unlikely(RX_BURST_SZ != kni_fifo_get(
				     q->fifo, (void **)&skb,
				     RX_BURST_SZ)))
			goto except;

		kni_vhost_enqueue_burst(kni, q, skb, va);
	}

	/* all leftover, do one by one */
	for (i = 0; i < nb_backlog; ++i) {
		if (unlikely(1 != kni_fifo_get(
				     kni->rx_q,(void **)&va, 1)))
			goto except;

		if (unlikely(1 != kni_fifo_get(
				     q->fifo, (void **)&skb, 1)))
			goto except;

		kni_vhost_enqueue(kni, q, *skb, *va);
	}

	/* Ondemand wake up */
	if ((nb_in == RX_BURST_SZ) || (nb_skb == 0) ||
	    ((nb_mbuf < RX_BURST_SZ) && (nb_mbuf != 0))) {
		wake_up_interruptible_poll(sk_sleep(&q->sk),
				   POLLIN | POLLRDNORM | POLLRDBAND);
		KNI_DBG_RX("RX CHK KICK nb_mbuf %d, nb_skb %d, nb_in %d\n",
			   nb_mbuf, nb_skb, nb_in);
	}

	return 0;

except:
	/* Failing should not happen */
	KNI_ERR("Fail to enqueue fifo, it shouldn't happen \n");
	BUG_ON(1);

	return 0;
}

static int
kni_sock_sndmsg(struct kiocb *iocb, struct socket *sock,
	   struct msghdr *m, size_t total_len)
{
 	struct kni_vhost_queue *q =
		container_of(sock->sk, struct kni_vhost_queue, sk);
	int vnet_hdr_len = 0;
	unsigned long len = total_len;

	if (unlikely(q == NULL || q->kni == NULL))
		return 0;

	KNI_DBG_TX("kni_sndmsg len %ld, flags 0x%08x, nb_iov %d\n",
		   len, q->flags, (int)m->msg_iovlen);

#ifdef RTE_KNI_VHOST_VNET_HDR_EN
	if (likely(q->flags & IFF_VNET_HDR)) {
		vnet_hdr_len = q->vnet_hdr_sz;
		if (unlikely(len < vnet_hdr_len))
			return -EINVAL;
		len -= vnet_hdr_len;
	}
#endif

	if (unlikely(len < ETH_HLEN + q->vnet_hdr_sz))
		return -EINVAL;

	return kni_vhost_net_tx(q->kni, m->msg_iov, vnet_hdr_len, len);
}

static int
kni_sock_rcvmsg(struct kiocb *iocb, struct socket *sock,
	   struct msghdr *m, size_t len, int flags)
{
	int vnet_hdr_len = 0;
	int pkt_len = 0;
	struct kni_vhost_queue *q =
		container_of(sock->sk, struct kni_vhost_queue, sk);
	static struct virtio_net_hdr
		__attribute__ ((unused)) vnet_hdr = {
		.flags = 0,
		.gso_type = VIRTIO_NET_HDR_GSO_NONE
	};

	if (unlikely(q == NULL || q->kni == NULL))
		return 0;

#ifdef RTE_KNI_VHOST_VNET_HDR_EN
	if (likely(q->flags & IFF_VNET_HDR)) {
		vnet_hdr_len = q->vnet_hdr_sz;
		if ((len -= vnet_hdr_len) < 0)
			return -EINVAL;
	}
#endif

	if (unlikely(0 == (pkt_len = kni_vhost_net_rx(q->kni,
		m->msg_iov, vnet_hdr_len, len))))
		return 0;

#ifdef RTE_KNI_VHOST_VNET_HDR_EN
	/* no need to copy hdr when no pkt received */
	if (unlikely(memcpy_toiovecend(m->msg_iov,
		(void *)&vnet_hdr, 0, vnet_hdr_len)))
		return -EFAULT;
#endif
	KNI_DBG_RX("kni_rcvmsg expect_len %ld, flags 0x%08x, pkt_len %d\n",
		   (unsigned long)len, q->flags, pkt_len);

	return (pkt_len + vnet_hdr_len);
}

/* dummy tap like ioctl */
static int
kni_sock_ioctl(struct socket *sock, unsigned int cmd,
	      unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	struct ifreq __user *ifr = argp;
	unsigned int __user *up = argp;
	struct kni_vhost_queue *q =
		container_of(sock->sk, struct kni_vhost_queue, sk);
	struct kni_dev *kni;
	unsigned int u;
	int __user *sp = argp;
	int s;
	int ret;

	KNI_DBG("tap ioctl cmd 0x%08x\n", cmd);

	switch (cmd) {
	case TUNSETIFF:
		KNI_DBG("TUNSETIFF\n");
		/* ignore the name, just look at flags */
		if (get_user(u, &ifr->ifr_flags))
			return -EFAULT;

		ret = 0;
		if ((u & ~IFF_VNET_HDR) != (IFF_NO_PI | IFF_TAP))
			ret = -EINVAL;
		else
			q->flags = u;

		return ret;

	case TUNGETIFF:
		KNI_DBG("TUNGETIFF\n");
		rcu_read_lock_bh();
		kni = rcu_dereference_bh(q->kni);
		if (kni)
			dev_hold(kni->net_dev);
		rcu_read_unlock_bh();

		if (!kni)
			return -ENOLINK;

		ret = 0;
		if (copy_to_user(&ifr->ifr_name, kni->net_dev->name, IFNAMSIZ) ||
		    put_user(q->flags, &ifr->ifr_flags))
			ret = -EFAULT;
		dev_put(kni->net_dev);
		return ret;

	case TUNGETFEATURES:
		KNI_DBG("TUNGETFEATURES\n");
		u = IFF_TAP | IFF_NO_PI;
#ifdef RTE_KNI_VHOST_VNET_HDR_EN
		u |= IFF_VNET_HDR;
#endif
		if (put_user(u, up))
			return -EFAULT;
		return 0;

	case TUNSETSNDBUF:
		KNI_DBG("TUNSETSNDBUF\n");
		if (get_user(u, up))
			return -EFAULT;

		q->sk.sk_sndbuf = u;
		return 0;

	case TUNGETVNETHDRSZ:
		s = q->vnet_hdr_sz;
		if (put_user(s, sp))
			return -EFAULT;
		KNI_DBG("TUNGETVNETHDRSZ %d\n", s);
		return 0;

	case TUNSETVNETHDRSZ:
		if (get_user(s, sp))
			return -EFAULT;
		if (s < (int)sizeof(struct virtio_net_hdr))
			return -EINVAL;

		KNI_DBG("TUNSETVNETHDRSZ %d\n", s);
		q->vnet_hdr_sz = s;
		return 0;

	case TUNSETOFFLOAD:
		KNI_DBG("TUNSETOFFLOAD %lx\n", arg);
#ifdef RTE_KNI_VHOST_VNET_HDR_EN
		/* not support any offload yet */
		if (!(q->flags & IFF_VNET_HDR))
			return  -EINVAL;

		return 0;
#else
		return -EINVAL;
#endif

	default:
		KNI_DBG("NOT SUPPORT\n");
		return -EINVAL;
	}
}

static int
kni_sock_compat_ioctl(struct socket *sock, unsigned int cmd,
		     unsigned long arg)
{
	/* 32 bits app on 64 bits OS to be supported later */
	KNI_PRINT("Not implemented.\n");

	return -EINVAL;
}

#define KNI_VHOST_WAIT_WQ_SAFE()                        \
do {		                                	\
	while ((BE_FINISH | BE_STOP) == kni->vq_status) \
		msleep(1);                              \
}while(0)                                               \


static int
kni_sock_release(struct socket *sock)
{
	struct kni_vhost_queue *q =
		container_of(sock->sk, struct kni_vhost_queue, sk);
	struct kni_dev *kni;

	if (q == NULL)
		return 0;

	if (NULL != (kni = q->kni)) {
		kni->vq_status = BE_STOP;
		KNI_VHOST_WAIT_WQ_SAFE();
		kni->vhost_queue = NULL;
		q->kni = NULL;
	}

	if (q->sockfd != -1)
		q->sockfd = -1;

	sk_set_socket(&q->sk, NULL);
	sock->sk = NULL;

	sock_put(&q->sk);

	KNI_DBG("dummy sock release done\n");

	return 0;
}

int
kni_sock_getname (struct socket *sock,
		  struct sockaddr *addr,
		  int *sockaddr_len, int peer)
{
	KNI_DBG("dummy sock getname\n");
	((struct sockaddr_ll*)addr)->sll_family = AF_PACKET;
	return 0;
}

static const struct proto_ops kni_socket_ops = {
	.getname = kni_sock_getname,
	.sendmsg = kni_sock_sndmsg,
	.recvmsg = kni_sock_rcvmsg,
	.release = kni_sock_release,
	.poll    = kni_sock_poll,
	.ioctl   = kni_sock_ioctl,
	.compat_ioctl = kni_sock_compat_ioctl,
};

static void
kni_sk_write_space(struct sock *sk)
{
	wait_queue_head_t *wqueue;

	if (!sock_writeable(sk) ||
	    !test_and_clear_bit(SOCK_ASYNC_NOSPACE,
				&sk->sk_socket->flags))
		return;
	wqueue = sk_sleep(sk);
	if (wqueue && waitqueue_active(wqueue))
		wake_up_interruptible_poll(
			wqueue, POLLOUT | POLLWRNORM | POLLWRBAND);
}

static void
kni_sk_destruct(struct sock *sk)
{
	struct kni_vhost_queue *q =
		container_of(sk, struct kni_vhost_queue, sk);

	if (!q)
		return;

	/* make sure there's no packet in buffer */
	while (skb_dequeue(&sk->sk_receive_queue) != NULL)
	       ;

	mb();

	if (q->fifo != NULL) {
		kfree(q->fifo);
		q->fifo = NULL;
	}

	if (q->cache != NULL) {
		kfree(q->cache);
		q->cache = NULL;
	}
}

static int
kni_vhost_backend_init(struct kni_dev *kni)
{
	struct kni_vhost_queue *q;
	struct net *net = current->nsproxy->net_ns;
	int err, i, sockfd;
	struct rte_kni_fifo *fifo;
	struct sk_buff *elem;

	if (kni->vhost_queue != NULL)
		return -1;

	if (!(q = (struct kni_vhost_queue *)sk_alloc(
		      net, AF_UNSPEC, GFP_KERNEL, &kni_raw_proto)))
		return -ENOMEM;

	err = sock_create_lite(AF_UNSPEC, SOCK_RAW, IPPROTO_RAW, &q->sock);
	if (err)
		goto free_sk;

	sockfd = kni_sock_map_fd(q->sock);
	if (sockfd < 0) {
		err = sockfd;
		goto free_sock;
	}

	/* cache init */
	q->cache = (struct sk_buff*)
		kzalloc(RTE_KNI_VHOST_MAX_CACHE_SIZE * sizeof(struct sk_buff),
			GFP_KERNEL);
	if (!q->cache)
		goto free_fd;

	fifo = (struct rte_kni_fifo*)
		kzalloc(RTE_KNI_VHOST_MAX_CACHE_SIZE * sizeof(void *)
			+ sizeof(struct rte_kni_fifo), GFP_KERNEL);
	if (!fifo)
		goto free_cache;

	kni_fifo_init(fifo, RTE_KNI_VHOST_MAX_CACHE_SIZE);

	for (i = 0; i < RTE_KNI_VHOST_MAX_CACHE_SIZE; i++) {
		elem = &q->cache[i];
		kni_fifo_put(fifo, (void**)&elem, 1);
	}
	q->fifo = fifo;

	/* store sockfd in vhost_queue */
	q->sockfd = sockfd;

	/* init socket */
	q->sock->type = SOCK_RAW;
	q->sock->state = SS_CONNECTED;
	q->sock->ops = &kni_socket_ops;
	sock_init_data(q->sock, &q->sk);

	/* init sock data */
	q->sk.sk_write_space = kni_sk_write_space;
	q->sk.sk_destruct = kni_sk_destruct;
	q->flags = IFF_NO_PI | IFF_TAP;
	q->vnet_hdr_sz = sizeof(struct virtio_net_hdr);
#ifdef RTE_KNI_VHOST_VNET_HDR_EN
	q->flags |= IFF_VNET_HDR;
#endif

	/* bind kni_dev with vhost_queue */
	q->kni = kni;
	kni->vhost_queue = q;

	wmb();

	kni->vq_status = BE_START;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35)
	KNI_DBG("backend init sockfd=%d, sock->wq=0x%16llx,"
		  "sk->sk_wq=0x%16llx",
		  q->sockfd, (uint64_t)q->sock->wq,
		  (uint64_t)q->sk.sk_wq);
#else
	KNI_DBG("backend init sockfd=%d, sock->wait at 0x%16llx,"
		  "sk->sk_sleep=0x%16llx",
		  q->sockfd, (uint64_t)&q->sock->wait,
		  (uint64_t)q->sk.sk_sleep);
#endif

	return 0;

free_cache:
	kfree(q->cache);
	q->cache = NULL;

free_fd:
	put_unused_fd(sockfd);

free_sock:
	q->kni = NULL;
	kni->vhost_queue = NULL;
	kni->vq_status |= BE_FINISH;
	sock_release(q->sock);
	q->sock->ops = NULL;
	q->sock = NULL;

free_sk:
	sk_free((struct sock*)q);

	return err;
}

/* kni vhost sock sysfs */
static ssize_t
show_sock_fd(struct device *dev, struct device_attribute *attr,
	     char *buf)
{
	struct net_device *net_dev = container_of(dev, struct net_device, dev);
	struct kni_dev *kni = netdev_priv(net_dev);
	int sockfd = -1;
	if (kni->vhost_queue != NULL)
		sockfd = kni->vhost_queue->sockfd;
	return snprintf(buf, 10, "%d\n", sockfd);
}

static ssize_t
show_sock_en(struct device *dev, struct device_attribute *attr,
	     char *buf)
{
	struct net_device *net_dev = container_of(dev, struct net_device, dev);
	struct kni_dev *kni = netdev_priv(net_dev);
	return snprintf(buf, 10, "%u\n", (kni->vhost_queue == NULL ? 0 : 1));
}

static ssize_t
set_sock_en(struct device *dev, struct device_attribute *attr,
	      const char *buf, size_t count)
{
	struct net_device *net_dev = container_of(dev, struct net_device, dev);
	struct kni_dev *kni = netdev_priv(net_dev);
	unsigned long en;
	int err = 0;

	if (0 != kstrtoul(buf, 0, &en))
		return -EINVAL;

	if (en)
		err = kni_vhost_backend_init(kni);

	return err ? err : count;
}

static DEVICE_ATTR(sock_fd, S_IRUGO | S_IRUSR, show_sock_fd, NULL);
static DEVICE_ATTR(sock_en, S_IRUGO | S_IWUSR, show_sock_en, set_sock_en);
static struct attribute *dev_attrs[] = {
	&dev_attr_sock_fd.attr,
	&dev_attr_sock_en.attr,
        NULL,
};

static const struct attribute_group dev_attr_grp = {
	.attrs = dev_attrs,
};

int
kni_vhost_backend_release(struct kni_dev *kni)
{
	struct kni_vhost_queue *q = kni->vhost_queue;

	if (q == NULL)
		return 0;

	/* dettach from kni */
	q->kni = NULL;

	KNI_DBG("release backend done\n");

	return 0;
}

int
kni_vhost_init(struct kni_dev *kni)
{
	struct net_device *dev = kni->net_dev;

	if (sysfs_create_group(&dev->dev.kobj, &dev_attr_grp))
		sysfs_remove_group(&dev->dev.kobj, &dev_attr_grp);

	kni->vq_status = BE_STOP;

	KNI_DBG("kni_vhost_init done\n");

	return 0;
}

