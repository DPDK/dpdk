/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018-2019 Cisco Systems, Inc.  All rights reserved.
 */

#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <errno.h>

#include <rte_version.h>
#include <rte_mbuf.h>
#include <rte_ether.h>
#include <rte_ethdev_driver.h>
#include <rte_ethdev_vdev.h>
#include <rte_malloc.h>
#include <rte_kvargs.h>
#include <rte_bus_vdev.h>
#include <rte_hash.h>
#include <rte_jhash.h>
#include <rte_string_fns.h>

#include "rte_eth_memif.h"
#include "memif_socket.h"

static void memif_intr_handler(void *arg);

static ssize_t
memif_msg_send(int fd, memif_msg_t *msg, int afd)
{
	struct msghdr mh = { 0 };
	struct iovec iov[1];
	struct cmsghdr *cmsg;
	char ctl[CMSG_SPACE(sizeof(int))];

	iov[0].iov_base = msg;
	iov[0].iov_len = sizeof(memif_msg_t);
	mh.msg_iov = iov;
	mh.msg_iovlen = 1;

	if (afd > 0) {
		memset(&ctl, 0, sizeof(ctl));
		mh.msg_control = ctl;
		mh.msg_controllen = sizeof(ctl);
		cmsg = CMSG_FIRSTHDR(&mh);
		cmsg->cmsg_len = CMSG_LEN(sizeof(int));
		cmsg->cmsg_level = SOL_SOCKET;
		cmsg->cmsg_type = SCM_RIGHTS;
		rte_memcpy(CMSG_DATA(cmsg), &afd, sizeof(int));
	}

	return sendmsg(fd, &mh, 0);
}

static int
memif_msg_send_from_queue(struct memif_control_channel *cc)
{
	ssize_t size;
	int ret = 0;
	struct memif_msg_queue_elt *e;

	e = TAILQ_FIRST(&cc->msg_queue);
	if (e == NULL)
		return 0;

	size = memif_msg_send(cc->intr_handle.fd, &e->msg, e->fd);
	if (size != sizeof(memif_msg_t)) {
		MIF_LOG(ERR, "sendmsg fail: %s.", strerror(errno));
		ret = -1;
	} else {
		MIF_LOG(DEBUG, "Sent msg type %u.", e->msg.type);
	}
	TAILQ_REMOVE(&cc->msg_queue, e, next);
	rte_free(e);

	return ret;
}

static struct memif_msg_queue_elt *
memif_msg_enq(struct memif_control_channel *cc)
{
	struct memif_msg_queue_elt *e;

	e = rte_zmalloc("memif_msg", sizeof(struct memif_msg_queue_elt), 0);
	if (e == NULL) {
		MIF_LOG(ERR, "Failed to allocate control message.");
		return NULL;
	}

	e->fd = -1;
	TAILQ_INSERT_TAIL(&cc->msg_queue, e, next);

	return e;
}

void
memif_msg_enq_disconnect(struct memif_control_channel *cc, const char *reason,
			 int err_code)
{
	struct memif_msg_queue_elt *e;
	struct pmd_internals *pmd;
	memif_msg_disconnect_t *d;

	if (cc == NULL) {
		MIF_LOG(DEBUG, "Missing control channel.");
		return;
	}

	e = memif_msg_enq(cc);
	if (e == NULL) {
		MIF_LOG(WARNING, "Failed to enqueue disconnect message.");
		return;
	}

	d = &e->msg.disconnect;

	e->msg.type = MEMIF_MSG_TYPE_DISCONNECT;
	d->code = err_code;

	if (reason != NULL) {
		strlcpy((char *)d->string, reason, sizeof(d->string));
		if (cc->dev != NULL) {
			pmd = cc->dev->data->dev_private;
			strlcpy(pmd->local_disc_string, reason,
				sizeof(pmd->local_disc_string));
		}
	}
}

static int
memif_msg_enq_hello(struct memif_control_channel *cc)
{
	struct memif_msg_queue_elt *e = memif_msg_enq(cc);
	memif_msg_hello_t *h;

	if (e == NULL)
		return -1;

	h = &e->msg.hello;

	e->msg.type = MEMIF_MSG_TYPE_HELLO;
	h->min_version = MEMIF_VERSION;
	h->max_version = MEMIF_VERSION;
	h->max_s2m_ring = ETH_MEMIF_MAX_NUM_Q_PAIRS;
	h->max_m2s_ring = ETH_MEMIF_MAX_NUM_Q_PAIRS;
	h->max_region = ETH_MEMIF_MAX_REGION_NUM - 1;
	h->max_log2_ring_size = ETH_MEMIF_MAX_LOG2_RING_SIZE;

	strlcpy((char *)h->name, rte_version(), sizeof(h->name));

	return 0;
}

static int
memif_msg_receive_hello(struct rte_eth_dev *dev, memif_msg_t *msg)
{
	struct pmd_internals *pmd = dev->data->dev_private;
	memif_msg_hello_t *h = &msg->hello;

	if (h->min_version > MEMIF_VERSION || h->max_version < MEMIF_VERSION) {
		memif_msg_enq_disconnect(pmd->cc, "Incompatible memif version", 0);
		return -1;
	}

	/* Set parameters for active connection */
	pmd->run.num_s2m_rings = RTE_MIN(h->max_s2m_ring + 1,
					   pmd->cfg.num_s2m_rings);
	pmd->run.num_m2s_rings = RTE_MIN(h->max_m2s_ring + 1,
					   pmd->cfg.num_m2s_rings);
	pmd->run.log2_ring_size = RTE_MIN(h->max_log2_ring_size,
					    pmd->cfg.log2_ring_size);
	pmd->run.pkt_buffer_size = pmd->cfg.pkt_buffer_size;

	strlcpy(pmd->remote_name, (char *)h->name, sizeof(pmd->remote_name));

	MIF_LOG(DEBUG, "Connecting to %s.", pmd->remote_name);

	return 0;
}

static int
memif_msg_receive_init(struct memif_control_channel *cc, memif_msg_t *msg)
{
	memif_msg_init_t *i = &msg->init;
	struct memif_socket_dev_list_elt *elt;
	struct pmd_internals *pmd;
	struct rte_eth_dev *dev;

	if (i->version != MEMIF_VERSION) {
		memif_msg_enq_disconnect(cc, "Incompatible memif version", 0);
		return -1;
	}

	if (cc->socket == NULL) {
		memif_msg_enq_disconnect(cc, "Device error", 0);
		return -1;
	}

	/* Find device with requested ID */
	TAILQ_FOREACH(elt, &cc->socket->dev_queue, next) {
		dev = elt->dev;
		pmd = dev->data->dev_private;
		if (((pmd->flags & ETH_MEMIF_FLAG_DISABLED) == 0) &&
		    (pmd->id == i->id) && (pmd->role == MEMIF_ROLE_MASTER)) {
			if (pmd->flags & (ETH_MEMIF_FLAG_CONNECTING |
					   ETH_MEMIF_FLAG_CONNECTED)) {
				memif_msg_enq_disconnect(cc,
							 "Already connected", 0);
				return -1;
			}

			/* assign control channel to device */
			cc->dev = dev;
			pmd->cc = cc;

			if (i->mode != MEMIF_INTERFACE_MODE_ETHERNET) {
				memif_msg_enq_disconnect(pmd->cc,
							 "Only ethernet mode supported",
							 0);
				return -1;
			}

			strlcpy(pmd->remote_name, (char *)i->name,
				sizeof(pmd->remote_name));

			if (*pmd->secret != '\0') {
				if (*i->secret == '\0') {
					memif_msg_enq_disconnect(pmd->cc,
								 "Secret required", 0);
					return -1;
				}
				if (strncmp(pmd->secret, (char *)i->secret,
						ETH_MEMIF_SECRET_SIZE) != 0) {
					memif_msg_enq_disconnect(pmd->cc,
								 "Incorrect secret", 0);
					return -1;
				}
			}

			pmd->flags |= ETH_MEMIF_FLAG_CONNECTING;
			return 0;
		}
	}

	/* ID not found on this socket */
	MIF_LOG(DEBUG, "ID %u not found.", i->id);
	memif_msg_enq_disconnect(cc, "ID not found", 0);
	return -1;
}

static int
memif_msg_receive_add_region(struct rte_eth_dev *dev, memif_msg_t *msg,
			     int fd)
{
	struct pmd_internals *pmd = dev->data->dev_private;
	struct pmd_process_private *proc_private = dev->process_private;
	memif_msg_add_region_t *ar = &msg->add_region;
	struct memif_region *r;

	if (fd < 0) {
		memif_msg_enq_disconnect(pmd->cc, "Missing region fd", 0);
		return -1;
	}

	if (ar->index >= ETH_MEMIF_MAX_REGION_NUM ||
			ar->index != proc_private->regions_num ||
			proc_private->regions[ar->index] != NULL) {
		memif_msg_enq_disconnect(pmd->cc, "Invalid region index", 0);
		return -1;
	}

	r = rte_zmalloc("region", sizeof(struct memif_region), 0);
	if (r == NULL) {
		memif_msg_enq_disconnect(pmd->cc, "Failed to alloc memif region.", 0);
		return -ENOMEM;
	}

	r->fd = fd;
	r->region_size = ar->size;
	r->addr = NULL;

	proc_private->regions[ar->index] = r;
	proc_private->regions_num++;

	return 0;
}

static int
memif_msg_receive_add_ring(struct rte_eth_dev *dev, memif_msg_t *msg, int fd)
{
	struct pmd_internals *pmd = dev->data->dev_private;
	memif_msg_add_ring_t *ar = &msg->add_ring;
	struct memif_queue *mq;

	if (fd < 0) {
		memif_msg_enq_disconnect(pmd->cc, "Missing interrupt fd", 0);
		return -1;
	}

	/* check if we have enough queues */
	if (ar->flags & MEMIF_MSG_ADD_RING_FLAG_S2M) {
		if (ar->index >= pmd->cfg.num_s2m_rings) {
			memif_msg_enq_disconnect(pmd->cc, "Invalid ring index", 0);
			return -1;
		}
		pmd->run.num_s2m_rings++;
	} else {
		if (ar->index >= pmd->cfg.num_m2s_rings) {
			memif_msg_enq_disconnect(pmd->cc, "Invalid ring index", 0);
			return -1;
		}
		pmd->run.num_m2s_rings++;
	}

	mq = (ar->flags & MEMIF_MSG_ADD_RING_FLAG_S2M) ?
	    dev->data->rx_queues[ar->index] : dev->data->tx_queues[ar->index];

	mq->intr_handle.fd = fd;
	mq->log2_ring_size = ar->log2_ring_size;
	mq->region = ar->region;
	mq->ring_offset = ar->offset;

	return 0;
}

static int
memif_msg_receive_connect(struct rte_eth_dev *dev, memif_msg_t *msg)
{
	struct pmd_internals *pmd = dev->data->dev_private;
	memif_msg_connect_t *c = &msg->connect;
	int ret;

	ret = memif_connect(dev);
	if (ret < 0)
		return ret;

	strlcpy(pmd->remote_if_name, (char *)c->if_name,
		sizeof(pmd->remote_if_name));
	MIF_LOG(INFO, "Remote interface %s connected.", pmd->remote_if_name);

	return 0;
}

static int
memif_msg_receive_connected(struct rte_eth_dev *dev, memif_msg_t *msg)
{
	struct pmd_internals *pmd = dev->data->dev_private;
	memif_msg_connected_t *c = &msg->connected;
	int ret;

	ret = memif_connect(dev);
	if (ret < 0)
		return ret;

	strlcpy(pmd->remote_if_name, (char *)c->if_name,
		sizeof(pmd->remote_if_name));
	MIF_LOG(INFO, "Remote interface %s connected.", pmd->remote_if_name);

	return 0;
}

static int
memif_msg_receive_disconnect(struct rte_eth_dev *dev, memif_msg_t *msg)
{
	struct pmd_internals *pmd = dev->data->dev_private;
	memif_msg_disconnect_t *d = &msg->disconnect;

	memset(pmd->remote_disc_string, 0, sizeof(pmd->remote_disc_string));
	strlcpy(pmd->remote_disc_string, (char *)d->string,
		sizeof(pmd->remote_disc_string));

	MIF_LOG(INFO, "Disconnect received: %s", pmd->remote_disc_string);

	memset(pmd->local_disc_string, 0, 96);
	memif_disconnect(dev);
	return 0;
}

static int
memif_msg_enq_ack(struct rte_eth_dev *dev)
{
	struct pmd_internals *pmd = dev->data->dev_private;
	struct memif_msg_queue_elt *e = memif_msg_enq(pmd->cc);
	if (e == NULL)
		return -1;

	e->msg.type = MEMIF_MSG_TYPE_ACK;

	return 0;
}

static int
memif_msg_enq_init(struct rte_eth_dev *dev)
{
	struct pmd_internals *pmd = dev->data->dev_private;
	struct memif_msg_queue_elt *e = memif_msg_enq(pmd->cc);
	memif_msg_init_t *i = &e->msg.init;

	if (e == NULL)
		return -1;

	i = &e->msg.init;
	e->msg.type = MEMIF_MSG_TYPE_INIT;
	i->version = MEMIF_VERSION;
	i->id = pmd->id;
	i->mode = MEMIF_INTERFACE_MODE_ETHERNET;

	strlcpy((char *)i->name, rte_version(), sizeof(i->name));

	if (*pmd->secret != '\0')
		strlcpy((char *)i->secret, pmd->secret, sizeof(i->secret));

	return 0;
}

static int
memif_msg_enq_add_region(struct rte_eth_dev *dev, uint8_t idx)
{
	struct pmd_internals *pmd = dev->data->dev_private;
	struct pmd_process_private *proc_private = dev->process_private;
	struct memif_msg_queue_elt *e = memif_msg_enq(pmd->cc);
	memif_msg_add_region_t *ar;
	struct memif_region *mr = proc_private->regions[idx];

	if (e == NULL)
		return -1;

	ar = &e->msg.add_region;
	e->msg.type = MEMIF_MSG_TYPE_ADD_REGION;
	e->fd = mr->fd;
	ar->index = idx;
	ar->size = mr->region_size;

	return 0;
}

static int
memif_msg_enq_add_ring(struct rte_eth_dev *dev, uint8_t idx,
		       memif_ring_type_t type)
{
	struct pmd_internals *pmd = dev->data->dev_private;
	struct memif_msg_queue_elt *e = memif_msg_enq(pmd->cc);
	struct memif_queue *mq;
	memif_msg_add_ring_t *ar;

	if (e == NULL)
		return -1;

	ar = &e->msg.add_ring;
	mq = (type == MEMIF_RING_S2M) ? dev->data->tx_queues[idx] :
	    dev->data->rx_queues[idx];

	e->msg.type = MEMIF_MSG_TYPE_ADD_RING;
	e->fd = mq->intr_handle.fd;
	ar->index = idx;
	ar->offset = mq->ring_offset;
	ar->region = mq->region;
	ar->log2_ring_size = mq->log2_ring_size;
	ar->flags = (type == MEMIF_RING_S2M) ? MEMIF_MSG_ADD_RING_FLAG_S2M : 0;
	ar->private_hdr_size = 0;

	return 0;
}

static int
memif_msg_enq_connect(struct rte_eth_dev *dev)
{
	struct pmd_internals *pmd = dev->data->dev_private;
	struct memif_msg_queue_elt *e = memif_msg_enq(pmd->cc);
	memif_msg_connect_t *c;

	if (e == NULL)
		return -1;

	c = &e->msg.connect;
	e->msg.type = MEMIF_MSG_TYPE_CONNECT;
	strlcpy((char *)c->if_name, dev->data->name, sizeof(c->if_name));

	return 0;
}

static int
memif_msg_enq_connected(struct rte_eth_dev *dev)
{
	struct pmd_internals *pmd = dev->data->dev_private;
	struct memif_msg_queue_elt *e = memif_msg_enq(pmd->cc);
	memif_msg_connected_t *c;

	if (e == NULL)
		return -1;

	c = &e->msg.connected;
	e->msg.type = MEMIF_MSG_TYPE_CONNECTED;
	strlcpy((char *)c->if_name, dev->data->name, sizeof(c->if_name));

	return 0;
}

static void
memif_intr_unregister_handler(struct rte_intr_handle *intr_handle, void *arg)
{
	struct memif_msg_queue_elt *elt;
	struct memif_control_channel *cc = arg;

	/* close control channel fd */
	close(intr_handle->fd);
	/* clear message queue */
	while ((elt = TAILQ_FIRST(&cc->msg_queue)) != NULL) {
		TAILQ_REMOVE(&cc->msg_queue, elt, next);
		rte_free(elt);
	}
	/* free control channel */
	rte_free(cc);
}

void
memif_disconnect(struct rte_eth_dev *dev)
{
	struct pmd_internals *pmd = dev->data->dev_private;
	struct memif_msg_queue_elt *elt, *next;
	struct memif_queue *mq;
	struct rte_intr_handle *ih;
	int i;
	int ret;

	dev->data->dev_link.link_status = ETH_LINK_DOWN;
	pmd->flags &= ~ETH_MEMIF_FLAG_CONNECTING;
	pmd->flags &= ~ETH_MEMIF_FLAG_CONNECTED;

	rte_spinlock_lock(&pmd->cc_lock);
	if (pmd->cc != NULL) {
		/* Clear control message queue (except disconnect message if any). */
		for (elt = TAILQ_FIRST(&pmd->cc->msg_queue); elt != NULL; elt = next) {
			next = TAILQ_NEXT(elt, next);
			if (elt->msg.type != MEMIF_MSG_TYPE_DISCONNECT) {
				TAILQ_REMOVE(&pmd->cc->msg_queue, elt, next);
				rte_free(elt);
			}
		}
		/* send disconnect message (if there is any in queue) */
		memif_msg_send_from_queue(pmd->cc);

		/* at this point, there should be no more messages in queue */
		if (TAILQ_FIRST(&pmd->cc->msg_queue) != NULL) {
			MIF_LOG(WARNING,
				"Unexpected message(s) in message queue.");
		}

		ih = &pmd->cc->intr_handle;
		if (ih->fd > 0) {
			ret = rte_intr_callback_unregister(ih,
							memif_intr_handler,
							pmd->cc);
			/*
			 * If callback is active (disconnecting based on
			 * received control message).
			 */
			if (ret == -EAGAIN) {
				ret = rte_intr_callback_unregister_pending(ih,
							memif_intr_handler,
							pmd->cc,
							memif_intr_unregister_handler);
			} else if (ret > 0) {
				close(ih->fd);
				rte_free(pmd->cc);
			}
			pmd->cc = NULL;
			if (ret <= 0)
				MIF_LOG(WARNING,
					"Failed to unregister control channel callback.");
		}
	}
	rte_spinlock_unlock(&pmd->cc_lock);

	/* unconfig interrupts */
	for (i = 0; i < pmd->cfg.num_s2m_rings; i++) {
		if (pmd->role == MEMIF_ROLE_SLAVE) {
			if (dev->data->tx_queues != NULL)
				mq = dev->data->tx_queues[i];
			else
				continue;
		} else {
			if (dev->data->rx_queues != NULL)
				mq = dev->data->rx_queues[i];
			else
				continue;
		}
		if (mq->intr_handle.fd > 0) {
			close(mq->intr_handle.fd);
			mq->intr_handle.fd = -1;
		}
	}
	for (i = 0; i < pmd->cfg.num_m2s_rings; i++) {
		if (pmd->role == MEMIF_ROLE_MASTER) {
			if (dev->data->tx_queues != NULL)
				mq = dev->data->tx_queues[i];
			else
				continue;
		} else {
			if (dev->data->rx_queues != NULL)
				mq = dev->data->rx_queues[i];
			else
				continue;
		}
		if (mq->intr_handle.fd > 0) {
			close(mq->intr_handle.fd);
			mq->intr_handle.fd = -1;
		}
	}

	memif_free_regions(dev);

	/* reset connection configuration */
	memset(&pmd->run, 0, sizeof(pmd->run));

	MIF_LOG(DEBUG, "Disconnected, id: %d, role: %s.", pmd->id,
		(pmd->role == MEMIF_ROLE_MASTER) ? "master" : "slave");
}

static int
memif_msg_receive(struct memif_control_channel *cc)
{
	char ctl[CMSG_SPACE(sizeof(int)) +
		 CMSG_SPACE(sizeof(struct ucred))] = { 0 };
	struct msghdr mh = { 0 };
	struct iovec iov[1];
	memif_msg_t msg = { 0 };
	ssize_t size;
	int ret = 0;
	struct ucred *cr __rte_unused;
	cr = 0;
	struct cmsghdr *cmsg;
	int afd = -1;
	int i;
	struct pmd_internals *pmd;
	struct pmd_process_private *proc_private;

	iov[0].iov_base = (void *)&msg;
	iov[0].iov_len = sizeof(memif_msg_t);
	mh.msg_iov = iov;
	mh.msg_iovlen = 1;
	mh.msg_control = ctl;
	mh.msg_controllen = sizeof(ctl);

	size = recvmsg(cc->intr_handle.fd, &mh, 0);
	if (size != sizeof(memif_msg_t)) {
		MIF_LOG(DEBUG, "Invalid message size = %zd", size);
		if (size > 0)
			/* 0 means end-of-file, negative size means error,
			 * don't send further disconnect message in such cases.
			 */
			memif_msg_enq_disconnect(cc, "Invalid message size", 0);
		return -1;
	}
	MIF_LOG(DEBUG, "Received msg type: %u.", msg.type);

	cmsg = CMSG_FIRSTHDR(&mh);
	while (cmsg) {
		if (cmsg->cmsg_level == SOL_SOCKET) {
			if (cmsg->cmsg_type == SCM_CREDENTIALS)
				cr = (struct ucred *)CMSG_DATA(cmsg);
			else if (cmsg->cmsg_type == SCM_RIGHTS)
				rte_memcpy(&afd, CMSG_DATA(cmsg), sizeof(int));
		}
		cmsg = CMSG_NXTHDR(&mh, cmsg);
	}

	if (cc->dev == NULL && msg.type != MEMIF_MSG_TYPE_INIT) {
		MIF_LOG(DEBUG, "Unexpected message.");
		memif_msg_enq_disconnect(cc, "Unexpected message", 0);
		return -1;
	}

	/* get device from hash data */
	switch (msg.type) {
	case MEMIF_MSG_TYPE_ACK:
		break;
	case MEMIF_MSG_TYPE_HELLO:
		ret = memif_msg_receive_hello(cc->dev, &msg);
		if (ret < 0)
			goto exit;
		ret = memif_init_regions_and_queues(cc->dev);
		if (ret < 0)
			goto exit;
		ret = memif_msg_enq_init(cc->dev);
		if (ret < 0)
			goto exit;
		pmd = cc->dev->data->dev_private;
		proc_private = cc->dev->process_private;
		for (i = 0; i < proc_private->regions_num; i++) {
			ret = memif_msg_enq_add_region(cc->dev, i);
			if (ret < 0)
				goto exit;
		}
		for (i = 0; i < pmd->run.num_s2m_rings; i++) {
			ret = memif_msg_enq_add_ring(cc->dev, i,
						     MEMIF_RING_S2M);
			if (ret < 0)
				goto exit;
		}
		for (i = 0; i < pmd->run.num_m2s_rings; i++) {
			ret = memif_msg_enq_add_ring(cc->dev, i,
						     MEMIF_RING_M2S);
			if (ret < 0)
				goto exit;
		}
		ret = memif_msg_enq_connect(cc->dev);
		if (ret < 0)
			goto exit;
		break;
	case MEMIF_MSG_TYPE_INIT:
		/*
		 * This cc does not have an interface asociated with it.
		 * If suitable interface is found it will be assigned here.
		 */
		ret = memif_msg_receive_init(cc, &msg);
		if (ret < 0)
			goto exit;
		ret = memif_msg_enq_ack(cc->dev);
		if (ret < 0)
			goto exit;
		break;
	case MEMIF_MSG_TYPE_ADD_REGION:
		ret = memif_msg_receive_add_region(cc->dev, &msg, afd);
		if (ret < 0)
			goto exit;
		ret = memif_msg_enq_ack(cc->dev);
		if (ret < 0)
			goto exit;
		break;
	case MEMIF_MSG_TYPE_ADD_RING:
		ret = memif_msg_receive_add_ring(cc->dev, &msg, afd);
		if (ret < 0)
			goto exit;
		ret = memif_msg_enq_ack(cc->dev);
		if (ret < 0)
			goto exit;
		break;
	case MEMIF_MSG_TYPE_CONNECT:
		ret = memif_msg_receive_connect(cc->dev, &msg);
		if (ret < 0)
			goto exit;
		ret = memif_msg_enq_connected(cc->dev);
		if (ret < 0)
			goto exit;
		break;
	case MEMIF_MSG_TYPE_CONNECTED:
		ret = memif_msg_receive_connected(cc->dev, &msg);
		break;
	case MEMIF_MSG_TYPE_DISCONNECT:
		ret = memif_msg_receive_disconnect(cc->dev, &msg);
		if (ret < 0)
			goto exit;
		break;
	default:
		memif_msg_enq_disconnect(cc, "Unknown message type", 0);
		ret = -1;
		goto exit;
	}

 exit:
	return ret;
}

static void
memif_intr_handler(void *arg)
{
	struct memif_control_channel *cc = arg;
	int ret;

	ret = memif_msg_receive(cc);
	/* if driver failed to assign device */
	if (cc->dev == NULL) {
		memif_msg_send_from_queue(cc);
		ret = rte_intr_callback_unregister_pending(&cc->intr_handle,
							   memif_intr_handler,
							   cc,
							   memif_intr_unregister_handler);
		if (ret < 0)
			MIF_LOG(WARNING,
				"Failed to unregister control channel callback.");
		return;
	}
	/* if memif_msg_receive failed */
	if (ret < 0)
		goto disconnect;

	ret = memif_msg_send_from_queue(cc);
	if (ret < 0)
		goto disconnect;

	return;

 disconnect:
	if (cc->dev == NULL) {
		MIF_LOG(WARNING, "eth dev not allocated");
		return;
	}
	memif_disconnect(cc->dev);
}

static void
memif_listener_handler(void *arg)
{
	struct memif_socket *socket = arg;
	int sockfd;
	int addr_len;
	struct sockaddr_un client;
	struct memif_control_channel *cc;
	int ret;

	addr_len = sizeof(client);
	sockfd = accept(socket->intr_handle.fd, (struct sockaddr *)&client,
			(socklen_t *)&addr_len);
	if (sockfd < 0) {
		MIF_LOG(ERR,
			"Failed to accept connection request on socket fd %d",
			socket->intr_handle.fd);
		return;
	}

	MIF_LOG(DEBUG, "%s: Connection request accepted.", socket->filename);

	cc = rte_zmalloc("memif-cc", sizeof(struct memif_control_channel), 0);
	if (cc == NULL) {
		MIF_LOG(ERR, "Failed to allocate control channel.");
		goto error;
	}

	cc->intr_handle.fd = sockfd;
	cc->intr_handle.type = RTE_INTR_HANDLE_EXT;
	cc->socket = socket;
	cc->dev = NULL;
	TAILQ_INIT(&cc->msg_queue);

	ret = rte_intr_callback_register(&cc->intr_handle, memif_intr_handler, cc);
	if (ret < 0) {
		MIF_LOG(ERR, "Failed to register control channel callback.");
		goto error;
	}

	ret = memif_msg_enq_hello(cc);
	if (ret < 0) {
		MIF_LOG(ERR, "Failed to enqueue hello message.");
		goto error;
	}
	ret = memif_msg_send_from_queue(cc);
	if (ret < 0)
		goto error;

	return;

 error:
	if (sockfd >= 0) {
		close(sockfd);
		sockfd = -1;
	}
	if (cc != NULL)
		rte_free(cc);
}

static struct memif_socket *
memif_socket_create(char *key, uint8_t listener)
{
	struct memif_socket *sock;
	struct sockaddr_un un;
	int sockfd;
	int ret;
	int on = 1;

	sock = rte_zmalloc("memif-socket", sizeof(struct memif_socket), 0);
	if (sock == NULL) {
		MIF_LOG(ERR, "Failed to allocate memory for memif socket");
		return NULL;
	}

	sock->listener = listener;
	strlcpy(sock->filename, key, MEMIF_SOCKET_UN_SIZE);
	TAILQ_INIT(&sock->dev_queue);

	if (listener != 0) {
		sockfd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
		if (sockfd < 0)
			goto error;

		un.sun_family = AF_UNIX;
		strlcpy(un.sun_path, sock->filename, MEMIF_SOCKET_UN_SIZE);

		ret = setsockopt(sockfd, SOL_SOCKET, SO_PASSCRED, &on,
				 sizeof(on));
		if (ret < 0)
			goto error;

		ret = bind(sockfd, (struct sockaddr *)&un, sizeof(un));
		if (ret < 0)
			goto error;

		ret = listen(sockfd, 1);
		if (ret < 0)
			goto error;

		MIF_LOG(DEBUG, "Memif listener socket %s created.", sock->filename);

		sock->intr_handle.fd = sockfd;
		sock->intr_handle.type = RTE_INTR_HANDLE_EXT;
		ret = rte_intr_callback_register(&sock->intr_handle,
						 memif_listener_handler, sock);
		if (ret < 0) {
			MIF_LOG(ERR, "Failed to register interrupt "
				"callback for listener socket");
			return NULL;
		}
	}

	return sock;

 error:
	MIF_LOG(ERR, "Failed to setup socket %s: %s", key, strerror(errno));
	if (sock != NULL)
		rte_free(sock);
	if (sockfd >= 0)
		close(sockfd);
	return NULL;
}

static struct rte_hash *
memif_create_socket_hash(void)
{
	struct rte_hash_parameters params = { 0 };

	params.name = MEMIF_SOCKET_HASH_NAME;
	params.entries = 256;
	params.key_len = MEMIF_SOCKET_UN_SIZE;
	params.hash_func = rte_jhash;
	params.hash_func_init_val = 0;
	return rte_hash_create(&params);
}

int
memif_socket_init(struct rte_eth_dev *dev, const char *socket_filename)
{
	struct pmd_internals *pmd = dev->data->dev_private;
	struct memif_socket *socket = NULL;
	struct memif_socket_dev_list_elt *elt;
	struct pmd_internals *tmp_pmd;
	struct rte_hash *hash;
	int ret;
	char key[MEMIF_SOCKET_UN_SIZE];

	hash = rte_hash_find_existing(MEMIF_SOCKET_HASH_NAME);
	if (hash == NULL) {
		hash = memif_create_socket_hash();
		if (hash == NULL) {
			MIF_LOG(ERR, "Failed to create memif socket hash.");
			return -1;
		}
	}

	memset(key, 0, MEMIF_SOCKET_UN_SIZE);
	strlcpy(key, socket_filename, MEMIF_SOCKET_UN_SIZE);
	ret = rte_hash_lookup_data(hash, key, (void **)&socket);
	if (ret < 0) {
		socket = memif_socket_create(key,
					     (pmd->role == MEMIF_ROLE_SLAVE) ? 0 : 1);
		if (socket == NULL)
			return -1;
		ret = rte_hash_add_key_data(hash, key, socket);
		if (ret < 0) {
			MIF_LOG(ERR, "Failed to add socket to socket hash.");
			return ret;
		}
	}
	pmd->socket_filename = socket->filename;

	TAILQ_FOREACH(elt, &socket->dev_queue, next) {
		tmp_pmd = elt->dev->data->dev_private;
		if (tmp_pmd->id == pmd->id && tmp_pmd->role == pmd->role) {
			MIF_LOG(ERR, "Two interfaces with the same id (%d) can "
				"not have the same role.", pmd->id);
			return -1;
		}
	}

	elt = rte_malloc("pmd-queue", sizeof(struct memif_socket_dev_list_elt), 0);
	if (elt == NULL) {
		MIF_LOG(ERR, "Failed to add device to socket device list.");
		return -1;
	}
	elt->dev = dev;
	TAILQ_INSERT_TAIL(&socket->dev_queue, elt, next);

	return 0;
}

void
memif_socket_remove_device(struct rte_eth_dev *dev)
{
	struct pmd_internals *pmd = dev->data->dev_private;
	struct memif_socket *socket = NULL;
	struct memif_socket_dev_list_elt *elt, *next;
	struct rte_hash *hash;
	int ret;

	hash = rte_hash_find_existing(MEMIF_SOCKET_HASH_NAME);
	if (hash == NULL)
		return;

	if (pmd->socket_filename == NULL)
		return;

	if (rte_hash_lookup_data(hash, pmd->socket_filename, (void **)&socket) < 0)
		return;

	for (elt = TAILQ_FIRST(&socket->dev_queue); elt != NULL; elt = next) {
		next = TAILQ_NEXT(elt, next);
		if (elt->dev == dev) {
			TAILQ_REMOVE(&socket->dev_queue, elt, next);
			rte_free(elt);
			pmd->socket_filename = NULL;
		}
	}

	/* remove socket, if this was the last device using it */
	if (TAILQ_EMPTY(&socket->dev_queue)) {
		rte_hash_del_key(hash, socket->filename);
		if (socket->listener) {
			/* remove listener socket file,
			 * so we can create new one later.
			 */
			ret = remove(socket->filename);
			if (ret < 0)
				MIF_LOG(ERR, "Failed to remove socket file: %s",
					socket->filename);
		}
		rte_free(socket);
	}
}

int
memif_connect_master(struct rte_eth_dev *dev)
{
	struct pmd_internals *pmd = dev->data->dev_private;

	memset(pmd->local_disc_string, 0, ETH_MEMIF_DISC_STRING_SIZE);
	memset(pmd->remote_disc_string, 0, ETH_MEMIF_DISC_STRING_SIZE);
	pmd->flags &= ~ETH_MEMIF_FLAG_DISABLED;
	return 0;
}

int
memif_connect_slave(struct rte_eth_dev *dev)
{
	int sockfd;
	int ret;
	struct sockaddr_un sun;
	struct pmd_internals *pmd = dev->data->dev_private;

	memset(pmd->local_disc_string, 0, ETH_MEMIF_DISC_STRING_SIZE);
	memset(pmd->remote_disc_string, 0, ETH_MEMIF_DISC_STRING_SIZE);
	pmd->flags &= ~ETH_MEMIF_FLAG_DISABLED;

	sockfd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
	if (sockfd < 0) {
		MIF_LOG(ERR, "Failed to open socket.");
		return -1;
	}

	sun.sun_family = AF_UNIX;

	memcpy(sun.sun_path, pmd->socket_filename, sizeof(sun.sun_path) - 1);

	ret = connect(sockfd, (struct sockaddr *)&sun,
		      sizeof(struct sockaddr_un));
	if (ret < 0) {
		MIF_LOG(ERR, "Failed to connect socket: %s.", pmd->socket_filename);
		goto error;
	}

	MIF_LOG(DEBUG, "Memif socket: %s connected.", pmd->socket_filename);

	pmd->cc = rte_zmalloc("memif-cc",
			      sizeof(struct memif_control_channel), 0);
	if (pmd->cc == NULL) {
		MIF_LOG(ERR, "Failed to allocate control channel.");
		goto error;
	}

	pmd->cc->intr_handle.fd = sockfd;
	pmd->cc->intr_handle.type = RTE_INTR_HANDLE_EXT;
	pmd->cc->socket = NULL;
	pmd->cc->dev = dev;
	TAILQ_INIT(&pmd->cc->msg_queue);

	ret = rte_intr_callback_register(&pmd->cc->intr_handle,
					 memif_intr_handler, pmd->cc);
	if (ret < 0) {
		MIF_LOG(ERR, "Failed to register interrupt callback for control fd");
		goto error;
	}

	return 0;

 error:
	if (sockfd >= 0) {
		close(sockfd);
		sockfd = -1;
	}
	if (pmd->cc != NULL) {
		rte_free(pmd->cc);
		pmd->cc = NULL;
	}
	return -1;
}
