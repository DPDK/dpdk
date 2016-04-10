/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
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

#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <xen/xen-compat.h>
#if __XEN_LATEST_INTERFACE_VERSION__ < 0x00040200
#include <xs.h>
#else
#include <xenstore.h>
#endif
#include <linux/virtio_ring.h>
#include <linux/virtio_pci.h>
#include <linux/virtio_net.h>

#include <rte_ethdev.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_string_fns.h>

#include "virtio-net.h"
#include "xen_vhost.h"

struct virtio_watch {
	struct xs_handle *xs;
	int watch_fd;
};


/* device ops to add/remove device to/from data core. */
static struct virtio_net_device_ops const *notify_ops;

/* root address of the linked list in the configuration core. */
static struct virtio_net_config_ll *ll_root = NULL;

/* root address of VM. */
static struct xen_guestlist guest_root;

static struct virtio_watch watch;

static void
vq_vring_init(struct vhost_virtqueue *vq, unsigned int num, uint8_t *p,
	unsigned long align)
{
	vq->size = num;
	vq->desc = (struct vring_desc *) p;
	vq->avail = (struct vring_avail *) (p +
		num * sizeof(struct vring_desc));
	vq->used = (void *)
		RTE_ALIGN_CEIL( (uintptr_t)(&vq->avail->ring[num]), align);

}

static int
init_watch(void)
{
	struct xs_handle *xs;
	int ret;
	int fd;

	/* get a connection to the daemon */
	xs = xs_daemon_open();
	if (xs == NULL) {
		RTE_LOG(ERR, XENHOST, "xs_daemon_open failed\n");
		return -1;
	}

	ret = xs_watch(xs, "/local/domain", "mytoken");
	if (ret == 0) {
		RTE_LOG(ERR, XENHOST, "%s: xs_watch failed\n", __func__);
		xs_daemon_close(xs);
		return -1;
	}

	/* We are notified of read availability on the watch via the file descriptor. */
	fd = xs_fileno(xs);
	watch.xs = xs;
	watch.watch_fd = fd;

	TAILQ_INIT(&guest_root);
	return 0;
}

static struct xen_guest *
get_xen_guest(int dom_id)
{
	struct xen_guest *guest = NULL;

	TAILQ_FOREACH(guest, &guest_root, next) {
		if(guest->dom_id == dom_id)
			return guest;
	}

	return NULL;
}


static struct xen_guest *
add_xen_guest(int32_t dom_id)
{
	struct xen_guest *guest = NULL;

	if ((guest = get_xen_guest(dom_id)) != NULL)
		return guest;

	guest = calloc(1, sizeof(struct xen_guest));
	if (guest) {
		RTE_LOG(ERR, XENHOST, "  %s: return newly created guest with %d rings\n", __func__, guest->vring_num);
		TAILQ_INSERT_TAIL(&guest_root, guest, next);
		guest->dom_id = dom_id;
	}

	return guest;
}

static void
cleanup_device(struct virtio_net_config_ll *ll_dev)
{
	if (ll_dev == NULL)
		return;
	if (ll_dev->dev.virtqueue_rx) {
		rte_free(ll_dev->dev.virtqueue_rx);
		ll_dev->dev.virtqueue_rx = NULL;
	}
	if (ll_dev->dev.virtqueue_tx) {
		rte_free(ll_dev->dev.virtqueue_tx);
		ll_dev->dev.virtqueue_tx = NULL;
	}
	free(ll_dev);
}

/*
 * Add entry containing a device to the device configuration linked list.
 */
static void
add_config_ll_entry(struct virtio_net_config_ll *new_ll_dev)
{
	struct virtio_net_config_ll *ll_dev = ll_root;

	/* If ll_dev == NULL then this is the first device so go to else */
	if (ll_dev) {
		/* If the 1st device_id != 0 then we insert our device here. */
		if (ll_dev->dev.device_fh != 0)	{
			new_ll_dev->dev.device_fh = 0;
			new_ll_dev->next = ll_dev;
			ll_root = new_ll_dev;
		} else {
			/* increment through the ll until we find un unused device_id,
			 * insert the device at that entry
			 */
			while ((ll_dev->next != NULL) && (ll_dev->dev.device_fh == (ll_dev->next->dev.device_fh - 1)))
				ll_dev = ll_dev->next;

			new_ll_dev->dev.device_fh = ll_dev->dev.device_fh + 1;
			new_ll_dev->next = ll_dev->next;
			ll_dev->next = new_ll_dev;
		}
	} else {
		ll_root = new_ll_dev;
		ll_root->dev.device_fh = 0;
	}
}


/*
 * Remove an entry from the device configuration linked list.
 */
static struct virtio_net_config_ll *
rm_config_ll_entry(struct virtio_net_config_ll *ll_dev, struct virtio_net_config_ll *ll_dev_last)
{
	/* First remove the device and then clean it up. */
	if (ll_dev == ll_root) {
		ll_root = ll_dev->next;
		cleanup_device(ll_dev);
		return ll_root;
	} else {
		ll_dev_last->next = ll_dev->next;
		cleanup_device(ll_dev);
		return ll_dev_last->next;
	}
}

/*
 * Retrieves an entry from the devices configuration linked list.
 */
static struct virtio_net_config_ll *
get_config_ll_entry(unsigned int virtio_idx, unsigned int dom_id)
{
	struct virtio_net_config_ll *ll_dev = ll_root;

	/* Loop through linked list until the dom_id is found. */
	while (ll_dev != NULL) {
		if (ll_dev->dev.dom_id == dom_id && ll_dev->dev.virtio_idx == virtio_idx)
			return ll_dev;
		ll_dev = ll_dev->next;
	}

	return NULL;
}

/*
 * Initialise all variables in device structure.
 */
static void
init_dev(struct virtio_net *dev)
{
	RTE_SET_USED(dev);
}


static struct
virtio_net_config_ll *new_device(unsigned int virtio_idx, struct xen_guest *guest)
{
	struct virtio_net_config_ll *new_ll_dev;
	struct vhost_virtqueue *virtqueue_rx, *virtqueue_tx;
	size_t size, vq_ring_size, vq_size = VQ_DESC_NUM;
	void *vq_ring_virt_mem;
	uint64_t gpa;
	uint32_t i;

	/* Setup device and virtqueues. */
	new_ll_dev   = calloc(1, sizeof(struct virtio_net_config_ll));
	virtqueue_rx = rte_zmalloc(NULL, sizeof(struct vhost_virtqueue), RTE_CACHE_LINE_SIZE);
	virtqueue_tx = rte_zmalloc(NULL, sizeof(struct vhost_virtqueue), RTE_CACHE_LINE_SIZE);
	if (new_ll_dev == NULL || virtqueue_rx == NULL || virtqueue_tx == NULL)
		goto err;

	new_ll_dev->dev.virtqueue_rx = virtqueue_rx;
	new_ll_dev->dev.virtqueue_tx = virtqueue_tx;
	new_ll_dev->dev.dom_id       = guest->dom_id;
	new_ll_dev->dev.virtio_idx   = virtio_idx;
	/* Initialise device and virtqueues. */
	init_dev(&new_ll_dev->dev);

	size = vring_size(vq_size, VIRTIO_PCI_VRING_ALIGN);
	vq_ring_size = RTE_ALIGN_CEIL(size, VIRTIO_PCI_VRING_ALIGN);
	(void)vq_ring_size;

	vq_ring_virt_mem = guest->vring[virtio_idx].rxvring_addr;
	vq_vring_init(virtqueue_rx, vq_size, vq_ring_virt_mem, VIRTIO_PCI_VRING_ALIGN);
	virtqueue_rx->size = vq_size;
	virtqueue_rx->vhost_hlen = sizeof(struct virtio_net_hdr);

	vq_ring_virt_mem = guest->vring[virtio_idx].txvring_addr;
	vq_vring_init(virtqueue_tx, vq_size, vq_ring_virt_mem, VIRTIO_PCI_VRING_ALIGN);
	virtqueue_tx->size = vq_size;
	memcpy(&new_ll_dev->dev.mac_address, &guest->vring[virtio_idx].addr, sizeof(struct ether_addr));

	/* virtio_memory has to be one per domid */
	new_ll_dev->dev.mem = malloc(sizeof(struct virtio_memory) + sizeof(struct virtio_memory_regions) * MAX_XENVIRT_MEMPOOL);
	new_ll_dev->dev.mem->nregions = guest->pool_num;
	for (i = 0; i < guest->pool_num; i++) {
		gpa = new_ll_dev->dev.mem->regions[i].guest_phys_address =
				(uint64_t)((uintptr_t)guest->mempool[i].gva);
		new_ll_dev->dev.mem->regions[i].guest_phys_address_end =
				gpa + guest->mempool[i].mempfn_num * getpagesize();
		new_ll_dev->dev.mem->regions[i].address_offset =
				(uint64_t)((uintptr_t)guest->mempool[i].hva -
					(uintptr_t)gpa);
	}

	new_ll_dev->next = NULL;

	/* Add entry to device configuration linked list. */
	add_config_ll_entry(new_ll_dev);
	return new_ll_dev;
err:
	free(new_ll_dev);
	rte_free(virtqueue_rx);
	rte_free(virtqueue_tx);

	return NULL;
}

static void
destroy_guest(struct xen_guest *guest)
{
	uint32_t i;

	for (i = 0; i < guest->vring_num; i++)
		cleanup_vring(&guest->vring[i]);
	/* clean mempool */
	for (i = 0; i < guest->pool_num; i++)
		cleanup_mempool(&guest->mempool[i]);
	free(guest);

	return;
}

/*
 * This function will cleanup the device and remove it from device configuration linked list.
 */
static void
destroy_device(unsigned int virtio_idx, unsigned int dom_id)
{
	struct virtio_net_config_ll *ll_dev_cur_ctx, *ll_dev_last = NULL;
	struct virtio_net_config_ll *ll_dev_cur = ll_root;

	/* clean virtio device */
	struct xen_guest *guest = NULL;
	guest = get_xen_guest(dom_id);
	if (guest == NULL)
		return;

	/* Find the linked list entry for the device to be removed. */
	ll_dev_cur_ctx = get_config_ll_entry(virtio_idx, dom_id);
	while (ll_dev_cur != NULL) {
		/* If the device is found or a device that doesn't exist is found then it is removed. */
		if  (ll_dev_cur == ll_dev_cur_ctx) {
			if ((ll_dev_cur->dev.flags & VIRTIO_DEV_RUNNING))
				notify_ops->destroy_device(&(ll_dev_cur->dev));
			ll_dev_cur = rm_config_ll_entry(ll_dev_cur, ll_dev_last);
		} else {
			ll_dev_last = ll_dev_cur;
			ll_dev_cur = ll_dev_cur->next;
		}
	}
	RTE_LOG(INFO, XENHOST, "  %s guest:%p vring:%p rxvring:%p txvring:%p flag:%p\n",
		__func__, guest, &guest->vring[virtio_idx], guest->vring[virtio_idx].rxvring_addr, guest->vring[virtio_idx].txvring_addr, guest->vring[virtio_idx].flag);
	cleanup_vring(&guest->vring[virtio_idx]);
	guest->vring[virtio_idx].removed = 1;
	guest->vring_num -= 1;
}




static void
watch_unmap_event(void)
{
	int i;
	struct xen_guest *guest  = NULL;
	bool remove_request;

	TAILQ_FOREACH(guest, &guest_root, next) {
		for (i = 0; i < MAX_VIRTIO; i++) {
			if (guest->vring[i].dom_id && guest->vring[i].removed == 0 && *guest->vring[i].flag == 0) {
				RTE_LOG(INFO, XENHOST, "\n\n");
				RTE_LOG(INFO, XENHOST, "  #####%s:  (%d, %d) to be removed\n",
					__func__,
					guest->vring[i].dom_id,
					i);
				destroy_device(i, guest->dom_id);
				RTE_LOG(INFO, XENHOST, "  %s: DOM %u, vring num: %d\n",
					__func__,
					guest->dom_id,
					guest->vring_num);
			}
		}
	}

_find_next_remove:
	guest = NULL;
	remove_request = false;
	TAILQ_FOREACH(guest, &guest_root, next) {
		if (guest->vring_num == 0) {
			remove_request = true;
			break;
		}
	}
	if (remove_request == true) {
		TAILQ_REMOVE(&guest_root, guest, next);
		RTE_LOG(INFO, XENHOST, "  #####%s: destroy guest (%d)\n", __func__, guest->dom_id);
		destroy_guest(guest);
		goto _find_next_remove;
	}
	return;
}

/*
 * OK, if the guest starts first, it is ok.
 * if host starts first, it is ok.
 * if guest starts, and has run for sometime, and host stops and restarts,
 * then last_used_idx  0? how to solve this. */

static void virtio_init(void)
{
	uint32_t len, e_num;
	uint32_t i,j;
	char **dom;
	char *status;
	int dom_id;
	char path[PATH_MAX];
	char node[PATH_MAX];
	xs_transaction_t th;
	struct xen_guest *guest;
	struct virtio_net_config_ll *net_config;
	char *end;
	int val;

	/* init env for watch the node */
	if (init_watch() < 0)
		return;

	dom = xs_directory(watch.xs, XBT_NULL, "/local/domain", &e_num);

	for (i = 0; i < e_num; i++) {
		errno = 0;
		dom_id = strtol(dom[i], &end, 0);
		if (errno != 0 || end == NULL || dom_id == 0)
			continue;

		for (j = 0; j < RTE_MAX_ETHPORTS; j++) {
			snprintf(node, PATH_MAX, "%s%d", VIRTIO_START, j);
			snprintf(path, PATH_MAX, XEN_VM_NODE_FMT,
					dom_id, node);

			th = xs_transaction_start(watch.xs);
			status = xs_read(watch.xs, th, path, &len);
			xs_transaction_end(watch.xs, th, false);

			if (status == NULL)
				break;

			/* if there's any valid virtio device */
			errno = 0;
			val = strtol(status, &end, 0);
			if (errno != 0 || end == NULL || dom_id == 0)
				val = 0;
			if (val == 1) {
				guest = add_xen_guest(dom_id);
				if (guest == NULL)
					continue;
				RTE_LOG(INFO, XENHOST, "  there's a new virtio existed, new a virtio device\n\n");

				RTE_LOG(INFO, XENHOST, "  parse_vringnode dom_id %d virtioidx %d\n",dom_id,j);
				if (parse_vringnode(guest, j)) {
					RTE_LOG(ERR, XENHOST, "  there is invalid information in xenstore\n");
					TAILQ_REMOVE(&guest_root, guest, next);
					destroy_guest(guest);

					continue;
				}

				/*if pool_num > 0, then mempool has already been parsed*/
				if (guest->pool_num == 0 && parse_mempoolnode(guest)) {
					RTE_LOG(ERR, XENHOST, "  there is error information in xenstore\n");
					TAILQ_REMOVE(&guest_root, guest, next);
					destroy_guest(guest);
					continue;
				}

				net_config = new_device(j, guest);
				/* every thing is ready now, added into data core */
				notify_ops->new_device(&net_config->dev);
			}
		}
	}

	free(dom);
	return;
}

void
virtio_monitor_loop(void)
{
	char **vec;
	xs_transaction_t th;
	char *buf;
	unsigned int len;
	unsigned int dom_id;
	uint32_t virtio_idx;
	struct xen_guest *guest;
	struct virtio_net_config_ll *net_config;
	enum fieldnames {
		FLD_NULL = 0,
		FLD_LOCAL,
		FLD_DOMAIN,
		FLD_ID,
		FLD_CONTROL,
		FLD_DPDK,
		FLD_NODE,
		_NUM_FLD
	};
	char *str_fld[_NUM_FLD];
	char *str;
	char *end;

	virtio_init();
	while (1) {
		watch_unmap_event();

		usleep(50);
		vec = xs_check_watch(watch.xs);

		if (vec == NULL)
			continue;

		th = xs_transaction_start(watch.xs);

		buf = xs_read(watch.xs, th, vec[XS_WATCH_PATH],&len);
		xs_transaction_end(watch.xs, th, false);

		if (buf) {
			/* theres' some node for vhost existed */
			if (rte_strsplit(vec[XS_WATCH_PATH], strnlen(vec[XS_WATCH_PATH], PATH_MAX),
						str_fld, _NUM_FLD, '/') == _NUM_FLD) {
				if (strstr(str_fld[FLD_NODE], VIRTIO_START)) {
					errno = 0;
					str = str_fld[FLD_ID];
					dom_id = strtoul(str, &end, 0);
					if (errno != 0 || end == NULL || end == str ) {
						RTE_LOG(INFO, XENHOST, "invalid domain id\n");
						continue;
					}

					errno = 0;
					str = str_fld[FLD_NODE] + sizeof(VIRTIO_START) - 1;
					virtio_idx = strtoul(str, &end, 0);
					if (errno != 0 || end == NULL || end == str
							|| virtio_idx > MAX_VIRTIO) {
						RTE_LOG(INFO, XENHOST, "invalid virtio idx\n");
						continue;
					}
					RTE_LOG(INFO, XENHOST, "  #####virtio dev (%d, %d) is started\n", dom_id, virtio_idx);

					guest = add_xen_guest(dom_id);
					if (guest == NULL)
						continue;
					guest->dom_id = dom_id;
					if (parse_vringnode(guest, virtio_idx)) {
						RTE_LOG(ERR, XENHOST, "  there is invalid information in xenstore\n");
						/*guest newly created? guest existed ?*/
						TAILQ_REMOVE(&guest_root, guest, next);
						destroy_guest(guest);
						continue;
					}
					/*if pool_num > 0, then mempool has already been parsed*/
					if (guest->pool_num == 0 && parse_mempoolnode(guest)) {
						RTE_LOG(ERR, XENHOST, "  there is error information in xenstore\n");
						TAILQ_REMOVE(&guest_root, guest, next);
						destroy_guest(guest);
						continue;
					}


					net_config = new_device(virtio_idx, guest);
					RTE_LOG(INFO, XENHOST, "  Add to dataplane core\n");
					notify_ops->new_device(&net_config->dev);

				}
			}
		}

		free(vec);
	}
	return;
}

/*
 * Register ops so that we can add/remove device to data core.
 */
int
init_virtio_xen(struct virtio_net_device_ops const *const ops)
{
	notify_ops = ops;
	if (xenhost_init())
		return -1;
	return 0;
}
