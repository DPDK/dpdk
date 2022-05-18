/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/epoll.h>
#include <linux/virtio_net.h>
#include <stdbool.h>

#include <rte_eal_paging.h>
#include <rte_malloc.h>
#include <rte_memory.h>
#include <rte_bus_pci.h>
#include <rte_vhost.h>
#include <rte_vdpa.h>
#include <vdpa_driver.h>
#include <rte_vfio.h>
#include <rte_spinlock.h>
#include <rte_log.h>
#include <rte_kvargs.h>
#include <rte_devargs.h>

#include "base/ifcvf.h"

RTE_LOG_REGISTER(ifcvf_vdpa_logtype, pmd.vdpa.ifcvf, NOTICE);
#define DRV_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, ifcvf_vdpa_logtype, \
		"IFCVF %s(): " fmt "\n", __func__, ##args)

#define IFCVF_USED_RING_LEN(size) \
	((size) * sizeof(struct vring_used_elem) + sizeof(uint16_t) * 3)

#define IFCVF_VDPA_MODE		"vdpa"
#define IFCVF_SW_FALLBACK_LM	"sw-live-migration"

#define THREAD_NAME_LEN	16

static const char * const ifcvf_valid_arguments[] = {
	IFCVF_VDPA_MODE,
	IFCVF_SW_FALLBACK_LM,
	NULL
};

struct ifcvf_internal {
	struct rte_pci_device *pdev;
	struct ifcvf_hw hw;
	int configured;
	int vfio_container_fd;
	int vfio_group_fd;
	int vfio_dev_fd;
	pthread_t tid;	/* thread for notify relay */
	int epfd;
	int vid;
	struct rte_vdpa_device *vdev;
	uint16_t max_queues;
	uint64_t features;
	rte_atomic32_t started;
	rte_atomic32_t dev_attached;
	rte_atomic32_t running;
	rte_spinlock_t lock;
	bool sw_lm;
	bool sw_fallback_running;
	/* mediated vring for sw fallback */
	struct vring m_vring[IFCVF_MAX_QUEUES * 2];
	/* eventfd for used ring interrupt */
	int intr_fd[IFCVF_MAX_QUEUES * 2];
};

struct internal_list {
	TAILQ_ENTRY(internal_list) next;
	struct ifcvf_internal *internal;
};

TAILQ_HEAD(internal_list_head, internal_list);
static struct internal_list_head internal_list =
	TAILQ_HEAD_INITIALIZER(internal_list);

static pthread_mutex_t internal_list_lock = PTHREAD_MUTEX_INITIALIZER;

static void update_used_ring(struct ifcvf_internal *internal, uint16_t qid);

static struct internal_list *
find_internal_resource_by_vdev(struct rte_vdpa_device *vdev)
{
	int found = 0;
	struct internal_list *list;

	pthread_mutex_lock(&internal_list_lock);

	TAILQ_FOREACH(list, &internal_list, next) {
		if (vdev == list->internal->vdev) {
			found = 1;
			break;
		}
	}

	pthread_mutex_unlock(&internal_list_lock);

	if (!found)
		return NULL;

	return list;
}

static struct internal_list *
find_internal_resource_by_dev(struct rte_pci_device *pdev)
{
	int found = 0;
	struct internal_list *list;

	pthread_mutex_lock(&internal_list_lock);

	TAILQ_FOREACH(list, &internal_list, next) {
		if (!rte_pci_addr_cmp(&pdev->addr,
					&list->internal->pdev->addr)) {
			found = 1;
			break;
		}
	}

	pthread_mutex_unlock(&internal_list_lock);

	if (!found)
		return NULL;

	return list;
}

static int
ifcvf_vfio_setup(struct ifcvf_internal *internal)
{
	struct rte_pci_device *dev = internal->pdev;
	char devname[RTE_DEV_NAME_MAX_LEN] = {0};
	int iommu_group_num;
	int i, ret;

	internal->vfio_dev_fd = -1;
	internal->vfio_group_fd = -1;
	internal->vfio_container_fd = -1;

	rte_pci_device_name(&dev->addr, devname, RTE_DEV_NAME_MAX_LEN);
	ret = rte_vfio_get_group_num(rte_pci_get_sysfs_path(), devname,
			&iommu_group_num);
	if (ret <= 0) {
		DRV_LOG(ERR, "%s failed to get IOMMU group", devname);
		return -1;
	}

	internal->vfio_container_fd = rte_vfio_container_create();
	if (internal->vfio_container_fd < 0)
		return -1;

	internal->vfio_group_fd = rte_vfio_container_group_bind(
			internal->vfio_container_fd, iommu_group_num);
	if (internal->vfio_group_fd < 0)
		goto err;

	if (rte_pci_map_device(dev))
		goto err;

	internal->vfio_dev_fd = rte_intr_dev_fd_get(dev->intr_handle);

	for (i = 0; i < RTE_MIN(PCI_MAX_RESOURCE, IFCVF_PCI_MAX_RESOURCE);
			i++) {
		internal->hw.mem_resource[i].addr =
			internal->pdev->mem_resource[i].addr;
		internal->hw.mem_resource[i].phys_addr =
			internal->pdev->mem_resource[i].phys_addr;
		internal->hw.mem_resource[i].len =
			internal->pdev->mem_resource[i].len;
	}

	return 0;

err:
	rte_vfio_container_destroy(internal->vfio_container_fd);
	return -1;
}

static int
ifcvf_dma_map(struct ifcvf_internal *internal, bool do_map)
{
	uint32_t i;
	int ret;
	struct rte_vhost_memory *mem = NULL;
	int vfio_container_fd;

	ret = rte_vhost_get_mem_table(internal->vid, &mem);
	if (ret < 0) {
		DRV_LOG(ERR, "failed to get VM memory layout.");
		goto exit;
	}

	vfio_container_fd = internal->vfio_container_fd;

	for (i = 0; i < mem->nregions; i++) {
		struct rte_vhost_mem_region *reg;

		reg = &mem->regions[i];
		DRV_LOG(INFO, "%s, region %u: HVA 0x%" PRIx64 ", "
			"GPA 0x%" PRIx64 ", size 0x%" PRIx64 ".",
			do_map ? "DMA map" : "DMA unmap", i,
			reg->host_user_addr, reg->guest_phys_addr, reg->size);

		if (do_map) {
			ret = rte_vfio_container_dma_map(vfio_container_fd,
				reg->host_user_addr, reg->guest_phys_addr,
				reg->size);
			if (ret < 0) {
				DRV_LOG(ERR, "DMA map failed.");
				goto exit;
			}
		} else {
			ret = rte_vfio_container_dma_unmap(vfio_container_fd,
				reg->host_user_addr, reg->guest_phys_addr,
				reg->size);
			if (ret < 0) {
				DRV_LOG(ERR, "DMA unmap failed.");
				goto exit;
			}
		}
	}

exit:
	if (mem)
		free(mem);
	return ret;
}

static uint64_t
hva_to_gpa(int vid, uint64_t hva)
{
	struct rte_vhost_memory *mem = NULL;
	struct rte_vhost_mem_region *reg;
	uint32_t i;
	uint64_t gpa = 0;

	if (rte_vhost_get_mem_table(vid, &mem) < 0)
		goto exit;

	for (i = 0; i < mem->nregions; i++) {
		reg = &mem->regions[i];

		if (hva >= reg->host_user_addr &&
				hva < reg->host_user_addr + reg->size) {
			gpa = hva - reg->host_user_addr + reg->guest_phys_addr;
			break;
		}
	}

exit:
	if (mem)
		free(mem);
	return gpa;
}

static int
vdpa_ifcvf_start(struct ifcvf_internal *internal)
{
	struct ifcvf_hw *hw = &internal->hw;
	int i, nr_vring;
	int vid;
	struct rte_vhost_vring vq;
	uint64_t gpa;

	vid = internal->vid;
	nr_vring = rte_vhost_get_vring_num(vid);
	rte_vhost_get_negotiated_features(vid, &hw->req_features);

	for (i = 0; i < nr_vring; i++) {
		rte_vhost_get_vhost_vring(vid, i, &vq);
		gpa = hva_to_gpa(vid, (uint64_t)(uintptr_t)vq.desc);
		if (gpa == 0) {
			DRV_LOG(ERR, "Fail to get GPA for descriptor ring.");
			return -1;
		}
		hw->vring[i].desc = gpa;

		gpa = hva_to_gpa(vid, (uint64_t)(uintptr_t)vq.avail);
		if (gpa == 0) {
			DRV_LOG(ERR, "Fail to get GPA for available ring.");
			return -1;
		}
		hw->vring[i].avail = gpa;

		gpa = hva_to_gpa(vid, (uint64_t)(uintptr_t)vq.used);
		if (gpa == 0) {
			DRV_LOG(ERR, "Fail to get GPA for used ring.");
			return -1;
		}
		hw->vring[i].used = gpa;

		hw->vring[i].size = vq.size;
		rte_vhost_get_vring_base(vid, i, &hw->vring[i].last_avail_idx,
				&hw->vring[i].last_used_idx);
	}
	hw->nr_vring = i;

	return ifcvf_start_hw(&internal->hw);
}

static void
vdpa_ifcvf_stop(struct ifcvf_internal *internal)
{
	struct ifcvf_hw *hw = &internal->hw;
	uint32_t i;
	int vid;
	uint64_t features = 0;
	uint64_t log_base = 0, log_size = 0;
	uint64_t len;

	vid = internal->vid;
	ifcvf_stop_hw(hw);

	for (i = 0; i < hw->nr_vring; i++)
		rte_vhost_set_vring_base(vid, i, hw->vring[i].last_avail_idx,
				hw->vring[i].last_used_idx);

	if (internal->sw_lm)
		return;

	rte_vhost_get_negotiated_features(vid, &features);
	if (RTE_VHOST_NEED_LOG(features)) {
		ifcvf_disable_logging(hw);
		rte_vhost_get_log_base(internal->vid, &log_base, &log_size);
		rte_vfio_container_dma_unmap(internal->vfio_container_fd,
				log_base, IFCVF_LOG_BASE, log_size);
		/*
		 * IFCVF marks dirty memory pages for only packet buffer,
		 * SW helps to mark the used ring as dirty after device stops.
		 */
		for (i = 0; i < hw->nr_vring; i++) {
			len = IFCVF_USED_RING_LEN(hw->vring[i].size);
			rte_vhost_log_used_vring(vid, i, 0, len);
		}
	}
}

#define MSIX_IRQ_SET_BUF_LEN (sizeof(struct vfio_irq_set) + \
		sizeof(int) * (IFCVF_MAX_QUEUES * 2 + 1))
static int
vdpa_enable_vfio_intr(struct ifcvf_internal *internal, bool m_rx)
{
	int ret;
	uint32_t i, nr_vring;
	char irq_set_buf[MSIX_IRQ_SET_BUF_LEN];
	struct vfio_irq_set *irq_set;
	int *fd_ptr;
	struct rte_vhost_vring vring;
	int fd;

	vring.callfd = -1;

	nr_vring = rte_vhost_get_vring_num(internal->vid);
	if (nr_vring > IFCVF_MAX_QUEUES * 2)
		return -1;

	irq_set = (struct vfio_irq_set *)irq_set_buf;
	irq_set->argsz = sizeof(irq_set_buf);
	irq_set->count = nr_vring + 1;
	irq_set->flags = VFIO_IRQ_SET_DATA_EVENTFD |
			 VFIO_IRQ_SET_ACTION_TRIGGER;
	irq_set->index = VFIO_PCI_MSIX_IRQ_INDEX;
	irq_set->start = 0;
	fd_ptr = (int *)&irq_set->data;
	fd_ptr[RTE_INTR_VEC_ZERO_OFFSET] =
		rte_intr_fd_get(internal->pdev->intr_handle);

	for (i = 0; i < nr_vring; i++)
		internal->intr_fd[i] = -1;

	for (i = 0; i < nr_vring; i++) {
		rte_vhost_get_vhost_vring(internal->vid, i, &vring);
		fd_ptr[RTE_INTR_VEC_RXTX_OFFSET + i] = vring.callfd;
		if ((i & 1) == 0 && m_rx == true) {
			fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
			if (fd < 0) {
				DRV_LOG(ERR, "can't setup eventfd: %s",
					strerror(errno));
				return -1;
			}
			internal->intr_fd[i] = fd;
			fd_ptr[RTE_INTR_VEC_RXTX_OFFSET + i] = fd;
		}
	}

	ret = ioctl(internal->vfio_dev_fd, VFIO_DEVICE_SET_IRQS, irq_set);
	if (ret) {
		DRV_LOG(ERR, "Error enabling MSI-X interrupts: %s",
				strerror(errno));
		return -1;
	}

	return 0;
}

static int
vdpa_disable_vfio_intr(struct ifcvf_internal *internal)
{
	int ret;
	uint32_t i, nr_vring;
	char irq_set_buf[MSIX_IRQ_SET_BUF_LEN];
	struct vfio_irq_set *irq_set;

	irq_set = (struct vfio_irq_set *)irq_set_buf;
	irq_set->argsz = sizeof(irq_set_buf);
	irq_set->count = 0;
	irq_set->flags = VFIO_IRQ_SET_DATA_NONE | VFIO_IRQ_SET_ACTION_TRIGGER;
	irq_set->index = VFIO_PCI_MSIX_IRQ_INDEX;
	irq_set->start = 0;

	nr_vring = rte_vhost_get_vring_num(internal->vid);
	for (i = 0; i < nr_vring; i++) {
		if (internal->intr_fd[i] >= 0)
			close(internal->intr_fd[i]);
		internal->intr_fd[i] = -1;
	}

	ret = ioctl(internal->vfio_dev_fd, VFIO_DEVICE_SET_IRQS, irq_set);
	if (ret) {
		DRV_LOG(ERR, "Error disabling MSI-X interrupts: %s",
				strerror(errno));
		return -1;
	}

	return 0;
}

static void *
notify_relay(void *arg)
{
	int i, kickfd, epfd, nfds = 0;
	uint32_t qid, q_num;
	struct epoll_event events[IFCVF_MAX_QUEUES * 2];
	struct epoll_event ev;
	uint64_t buf;
	int nbytes;
	struct rte_vhost_vring vring;
	struct ifcvf_internal *internal = (struct ifcvf_internal *)arg;
	struct ifcvf_hw *hw = &internal->hw;

	q_num = rte_vhost_get_vring_num(internal->vid);

	epfd = epoll_create(IFCVF_MAX_QUEUES * 2);
	if (epfd < 0) {
		DRV_LOG(ERR, "failed to create epoll instance.");
		return NULL;
	}
	internal->epfd = epfd;

	vring.kickfd = -1;
	for (qid = 0; qid < q_num; qid++) {
		ev.events = EPOLLIN | EPOLLPRI;
		rte_vhost_get_vhost_vring(internal->vid, qid, &vring);
		ev.data.u64 = qid | (uint64_t)vring.kickfd << 32;
		if (epoll_ctl(epfd, EPOLL_CTL_ADD, vring.kickfd, &ev) < 0) {
			DRV_LOG(ERR, "epoll add error: %s", strerror(errno));
			return NULL;
		}
	}

	for (;;) {
		nfds = epoll_wait(epfd, events, q_num, -1);
		if (nfds < 0) {
			if (errno == EINTR)
				continue;
			DRV_LOG(ERR, "epoll_wait return fail\n");
			return NULL;
		}

		for (i = 0; i < nfds; i++) {
			qid = events[i].data.u32;
			kickfd = (uint32_t)(events[i].data.u64 >> 32);
			do {
				nbytes = read(kickfd, &buf, 8);
				if (nbytes < 0) {
					if (errno == EINTR ||
					    errno == EWOULDBLOCK ||
					    errno == EAGAIN)
						continue;
					DRV_LOG(INFO, "Error reading "
						"kickfd: %s",
						strerror(errno));
				}
				break;
			} while (1);

			ifcvf_notify_queue(hw, qid);
		}
	}

	return NULL;
}

static int
setup_notify_relay(struct ifcvf_internal *internal)
{
	char name[THREAD_NAME_LEN];
	int ret;

	snprintf(name, sizeof(name), "ifc-notify-%d", internal->vid);
	ret = rte_ctrl_thread_create(&internal->tid, name, NULL, notify_relay,
				     (void *)internal);
	if (ret != 0) {
		DRV_LOG(ERR, "failed to create notify relay pthread.");
		return -1;
	}

	return 0;
}

static int
unset_notify_relay(struct ifcvf_internal *internal)
{
	void *status;

	if (internal->tid) {
		pthread_cancel(internal->tid);
		pthread_join(internal->tid, &status);
	}
	internal->tid = 0;

	if (internal->epfd >= 0)
		close(internal->epfd);
	internal->epfd = -1;

	return 0;
}

static int
update_datapath(struct ifcvf_internal *internal)
{
	int ret;

	rte_spinlock_lock(&internal->lock);

	if (!rte_atomic32_read(&internal->running) &&
	    (rte_atomic32_read(&internal->started) &&
	     rte_atomic32_read(&internal->dev_attached))) {
		ret = ifcvf_dma_map(internal, true);
		if (ret)
			goto err;

		ret = vdpa_enable_vfio_intr(internal, false);
		if (ret)
			goto err;

		ret = vdpa_ifcvf_start(internal);
		if (ret)
			goto err;

		ret = setup_notify_relay(internal);
		if (ret)
			goto err;

		rte_atomic32_set(&internal->running, 1);
	} else if (rte_atomic32_read(&internal->running) &&
		   (!rte_atomic32_read(&internal->started) ||
		    !rte_atomic32_read(&internal->dev_attached))) {
		ret = unset_notify_relay(internal);
		if (ret)
			goto err;

		vdpa_ifcvf_stop(internal);

		ret = vdpa_disable_vfio_intr(internal);
		if (ret)
			goto err;

		ret = ifcvf_dma_map(internal, false);
		if (ret)
			goto err;

		rte_atomic32_set(&internal->running, 0);
	}

	rte_spinlock_unlock(&internal->lock);
	return 0;
err:
	rte_spinlock_unlock(&internal->lock);
	return ret;
}

static int
m_ifcvf_start(struct ifcvf_internal *internal)
{
	struct ifcvf_hw *hw = &internal->hw;
	uint32_t i, nr_vring;
	int vid, ret;
	struct rte_vhost_vring vq;
	void *vring_buf;
	uint64_t m_vring_iova = IFCVF_MEDIATED_VRING;
	uint64_t size;
	uint64_t gpa;

	memset(&vq, 0, sizeof(vq));
	vid = internal->vid;
	nr_vring = rte_vhost_get_vring_num(vid);
	rte_vhost_get_negotiated_features(vid, &hw->req_features);

	for (i = 0; i < nr_vring; i++) {
		rte_vhost_get_vhost_vring(vid, i, &vq);

		size = RTE_ALIGN_CEIL(vring_size(vq.size, rte_mem_page_size()),
				rte_mem_page_size());
		vring_buf = rte_zmalloc("ifcvf", size, rte_mem_page_size());
		vring_init(&internal->m_vring[i], vq.size, vring_buf,
				rte_mem_page_size());

		ret = rte_vfio_container_dma_map(internal->vfio_container_fd,
			(uint64_t)(uintptr_t)vring_buf, m_vring_iova, size);
		if (ret < 0) {
			DRV_LOG(ERR, "mediated vring DMA map failed.");
			goto error;
		}

		gpa = hva_to_gpa(vid, (uint64_t)(uintptr_t)vq.desc);
		if (gpa == 0) {
			DRV_LOG(ERR, "Fail to get GPA for descriptor ring.");
			return -1;
		}
		hw->vring[i].desc = gpa;

		gpa = hva_to_gpa(vid, (uint64_t)(uintptr_t)vq.avail);
		if (gpa == 0) {
			DRV_LOG(ERR, "Fail to get GPA for available ring.");
			return -1;
		}
		hw->vring[i].avail = gpa;

		/* Direct I/O for Tx queue, relay for Rx queue */
		if (i & 1) {
			gpa = hva_to_gpa(vid, (uint64_t)(uintptr_t)vq.used);
			if (gpa == 0) {
				DRV_LOG(ERR, "Fail to get GPA for used ring.");
				return -1;
			}
			hw->vring[i].used = gpa;
		} else {
			hw->vring[i].used = m_vring_iova +
				(char *)internal->m_vring[i].used -
				(char *)internal->m_vring[i].desc;
		}

		hw->vring[i].size = vq.size;

		rte_vhost_get_vring_base(vid, i,
				&internal->m_vring[i].avail->idx,
				&internal->m_vring[i].used->idx);

		rte_vhost_get_vring_base(vid, i, &hw->vring[i].last_avail_idx,
				&hw->vring[i].last_used_idx);

		m_vring_iova += size;
	}
	hw->nr_vring = nr_vring;

	return ifcvf_start_hw(&internal->hw);

error:
	for (i = 0; i < nr_vring; i++)
		if (internal->m_vring[i].desc)
			rte_free(internal->m_vring[i].desc);

	return -1;
}

static int
m_ifcvf_stop(struct ifcvf_internal *internal)
{
	int vid;
	uint32_t i;
	struct rte_vhost_vring vq;
	struct ifcvf_hw *hw = &internal->hw;
	uint64_t m_vring_iova = IFCVF_MEDIATED_VRING;
	uint64_t size, len;

	vid = internal->vid;
	ifcvf_stop_hw(hw);

	for (i = 0; i < hw->nr_vring; i++) {
		/* synchronize remaining new used entries if any */
		if ((i & 1) == 0)
			update_used_ring(internal, i);

		rte_vhost_get_vhost_vring(vid, i, &vq);
		len = IFCVF_USED_RING_LEN(vq.size);
		rte_vhost_log_used_vring(vid, i, 0, len);

		size = RTE_ALIGN_CEIL(vring_size(vq.size, rte_mem_page_size()),
				rte_mem_page_size());
		rte_vfio_container_dma_unmap(internal->vfio_container_fd,
			(uint64_t)(uintptr_t)internal->m_vring[i].desc,
			m_vring_iova, size);

		rte_vhost_set_vring_base(vid, i, hw->vring[i].last_avail_idx,
				hw->vring[i].last_used_idx);
		rte_free(internal->m_vring[i].desc);
		m_vring_iova += size;
	}

	return 0;
}

static void
update_used_ring(struct ifcvf_internal *internal, uint16_t qid)
{
	rte_vdpa_relay_vring_used(internal->vid, qid, &internal->m_vring[qid]);
	rte_vhost_vring_call(internal->vid, qid);
}

static void *
vring_relay(void *arg)
{
	int i, vid, epfd, fd, nfds;
	struct ifcvf_internal *internal = (struct ifcvf_internal *)arg;
	struct rte_vhost_vring vring;
	uint16_t qid, q_num;
	struct epoll_event events[IFCVF_MAX_QUEUES * 4];
	struct epoll_event ev;
	int nbytes;
	uint64_t buf;

	vid = internal->vid;
	q_num = rte_vhost_get_vring_num(vid);

	/* add notify fd and interrupt fd to epoll */
	epfd = epoll_create(IFCVF_MAX_QUEUES * 2);
	if (epfd < 0) {
		DRV_LOG(ERR, "failed to create epoll instance.");
		return NULL;
	}
	internal->epfd = epfd;

	vring.kickfd = -1;
	for (qid = 0; qid < q_num; qid++) {
		ev.events = EPOLLIN | EPOLLPRI;
		rte_vhost_get_vhost_vring(vid, qid, &vring);
		ev.data.u64 = qid << 1 | (uint64_t)vring.kickfd << 32;
		if (epoll_ctl(epfd, EPOLL_CTL_ADD, vring.kickfd, &ev) < 0) {
			DRV_LOG(ERR, "epoll add error: %s", strerror(errno));
			return NULL;
		}
	}

	for (qid = 0; qid < q_num; qid += 2) {
		ev.events = EPOLLIN | EPOLLPRI;
		/* leave a flag to mark it's for interrupt */
		ev.data.u64 = 1 | qid << 1 |
			(uint64_t)internal->intr_fd[qid] << 32;
		if (epoll_ctl(epfd, EPOLL_CTL_ADD, internal->intr_fd[qid], &ev)
				< 0) {
			DRV_LOG(ERR, "epoll add error: %s", strerror(errno));
			return NULL;
		}
		update_used_ring(internal, qid);
	}

	/* start relay with a first kick */
	for (qid = 0; qid < q_num; qid++)
		ifcvf_notify_queue(&internal->hw, qid);

	/* listen to the events and react accordingly */
	for (;;) {
		nfds = epoll_wait(epfd, events, q_num * 2, -1);
		if (nfds < 0) {
			if (errno == EINTR)
				continue;
			DRV_LOG(ERR, "epoll_wait return fail\n");
			return NULL;
		}

		for (i = 0; i < nfds; i++) {
			fd = (uint32_t)(events[i].data.u64 >> 32);
			do {
				nbytes = read(fd, &buf, 8);
				if (nbytes < 0) {
					if (errno == EINTR ||
					    errno == EWOULDBLOCK ||
					    errno == EAGAIN)
						continue;
					DRV_LOG(INFO, "Error reading "
						"kickfd: %s",
						strerror(errno));
				}
				break;
			} while (1);

			qid = events[i].data.u32 >> 1;

			if (events[i].data.u32 & 1)
				update_used_ring(internal, qid);
			else
				ifcvf_notify_queue(&internal->hw, qid);
		}
	}

	return NULL;
}

static int
setup_vring_relay(struct ifcvf_internal *internal)
{
	char name[THREAD_NAME_LEN];
	int ret;

	snprintf(name, sizeof(name), "ifc-vring-%d", internal->vid);
	ret = rte_ctrl_thread_create(&internal->tid, name, NULL, vring_relay,
				     (void *)internal);
	if (ret != 0) {
		DRV_LOG(ERR, "failed to create ring relay pthread.");
		return -1;
	}

	return 0;
}

static int
unset_vring_relay(struct ifcvf_internal *internal)
{
	void *status;

	if (internal->tid) {
		pthread_cancel(internal->tid);
		pthread_join(internal->tid, &status);
	}
	internal->tid = 0;

	if (internal->epfd >= 0)
		close(internal->epfd);
	internal->epfd = -1;

	return 0;
}

static int
ifcvf_sw_fallback_switchover(struct ifcvf_internal *internal)
{
	int ret;
	int vid = internal->vid;

	/* stop the direct IO data path */
	unset_notify_relay(internal);
	vdpa_ifcvf_stop(internal);
	vdpa_disable_vfio_intr(internal);

	ret = rte_vhost_host_notifier_ctrl(vid, RTE_VHOST_QUEUE_ALL, false);
	if (ret && ret != -ENOTSUP)
		goto error;

	/* set up interrupt for interrupt relay */
	ret = vdpa_enable_vfio_intr(internal, true);
	if (ret)
		goto unmap;

	/* config the VF */
	ret = m_ifcvf_start(internal);
	if (ret)
		goto unset_intr;

	/* set up vring relay thread */
	ret = setup_vring_relay(internal);
	if (ret)
		goto stop_vf;

	rte_vhost_host_notifier_ctrl(vid, RTE_VHOST_QUEUE_ALL, true);

	internal->sw_fallback_running = true;

	return 0;

stop_vf:
	m_ifcvf_stop(internal);
unset_intr:
	vdpa_disable_vfio_intr(internal);
unmap:
	ifcvf_dma_map(internal, false);
error:
	return -1;
}

static int
ifcvf_dev_config(int vid)
{
	struct rte_vdpa_device *vdev;
	struct internal_list *list;
	struct ifcvf_internal *internal;

	vdev = rte_vhost_get_vdpa_device(vid);
	list = find_internal_resource_by_vdev(vdev);
	if (list == NULL) {
		DRV_LOG(ERR, "Invalid vDPA device: %p", vdev);
		return -1;
	}

	internal = list->internal;
	internal->vid = vid;
	rte_atomic32_set(&internal->dev_attached, 1);
	update_datapath(internal);

	if (rte_vhost_host_notifier_ctrl(vid, RTE_VHOST_QUEUE_ALL, true) != 0)
		DRV_LOG(NOTICE, "vDPA (%s): software relay is used.",
				vdev->device->name);

	internal->configured = 1;
	return 0;
}

static int
ifcvf_dev_close(int vid)
{
	struct rte_vdpa_device *vdev;
	struct internal_list *list;
	struct ifcvf_internal *internal;

	vdev = rte_vhost_get_vdpa_device(vid);
	list = find_internal_resource_by_vdev(vdev);
	if (list == NULL) {
		DRV_LOG(ERR, "Invalid vDPA device: %p", vdev);
		return -1;
	}

	internal = list->internal;

	if (internal->sw_fallback_running) {
		/* unset ring relay */
		unset_vring_relay(internal);

		/* reset VF */
		m_ifcvf_stop(internal);

		/* remove interrupt setting */
		vdpa_disable_vfio_intr(internal);

		/* unset DMA map for guest memory */
		ifcvf_dma_map(internal, false);

		internal->sw_fallback_running = false;
	} else {
		rte_atomic32_set(&internal->dev_attached, 0);
		update_datapath(internal);
	}

	internal->configured = 0;
	return 0;
}

static int
ifcvf_set_features(int vid)
{
	uint64_t features = 0;
	struct rte_vdpa_device *vdev;
	struct internal_list *list;
	struct ifcvf_internal *internal;
	uint64_t log_base = 0, log_size = 0;

	vdev = rte_vhost_get_vdpa_device(vid);
	list = find_internal_resource_by_vdev(vdev);
	if (list == NULL) {
		DRV_LOG(ERR, "Invalid vDPA device: %p", vdev);
		return -1;
	}

	internal = list->internal;
	rte_vhost_get_negotiated_features(vid, &features);

	if (!RTE_VHOST_NEED_LOG(features))
		return 0;

	if (internal->sw_lm) {
		ifcvf_sw_fallback_switchover(internal);
	} else {
		rte_vhost_get_log_base(vid, &log_base, &log_size);
		rte_vfio_container_dma_map(internal->vfio_container_fd,
				log_base, IFCVF_LOG_BASE, log_size);
		ifcvf_enable_logging(&internal->hw, IFCVF_LOG_BASE, log_size);
	}

	return 0;
}

static int
ifcvf_get_vfio_group_fd(int vid)
{
	struct rte_vdpa_device *vdev;
	struct internal_list *list;

	vdev = rte_vhost_get_vdpa_device(vid);
	list = find_internal_resource_by_vdev(vdev);
	if (list == NULL) {
		DRV_LOG(ERR, "Invalid vDPA device: %p", vdev);
		return -1;
	}

	return list->internal->vfio_group_fd;
}

static int
ifcvf_get_vfio_device_fd(int vid)
{
	struct rte_vdpa_device *vdev;
	struct internal_list *list;

	vdev = rte_vhost_get_vdpa_device(vid);
	list = find_internal_resource_by_vdev(vdev);
	if (list == NULL) {
		DRV_LOG(ERR, "Invalid vDPA device: %p", vdev);
		return -1;
	}

	return list->internal->vfio_dev_fd;
}

static int
ifcvf_get_notify_area(int vid, int qid, uint64_t *offset, uint64_t *size)
{
	struct rte_vdpa_device *vdev;
	struct internal_list *list;
	struct ifcvf_internal *internal;
	struct vfio_region_info reg = { .argsz = sizeof(reg) };
	int ret;

	vdev = rte_vhost_get_vdpa_device(vid);
	list = find_internal_resource_by_vdev(vdev);
	if (list == NULL) {
		DRV_LOG(ERR, "Invalid vDPA device: %p", vdev);
		return -1;
	}

	internal = list->internal;

	reg.index = ifcvf_get_notify_region(&internal->hw);
	ret = ioctl(internal->vfio_dev_fd, VFIO_DEVICE_GET_REGION_INFO, &reg);
	if (ret) {
		DRV_LOG(ERR, "Get not get device region info: %s",
				strerror(errno));
		return -1;
	}

	*offset = ifcvf_get_queue_notify_off(&internal->hw, qid) + reg.offset;
	*size = 0x1000;

	return 0;
}

static int
ifcvf_get_queue_num(struct rte_vdpa_device *vdev, uint32_t *queue_num)
{
	struct internal_list *list;

	list = find_internal_resource_by_vdev(vdev);
	if (list == NULL) {
		DRV_LOG(ERR, "Invalid vDPA device: %p", vdev);
		return -1;
	}

	*queue_num = list->internal->max_queues;

	return 0;
}

static int
ifcvf_get_vdpa_features(struct rte_vdpa_device *vdev, uint64_t *features)
{
	struct internal_list *list;

	list = find_internal_resource_by_vdev(vdev);
	if (list == NULL) {
		DRV_LOG(ERR, "Invalid vDPA device: %p", vdev);
		return -1;
	}

	*features = list->internal->features;

	return 0;
}

#define VDPA_SUPPORTED_PROTOCOL_FEATURES \
		(1ULL << VHOST_USER_PROTOCOL_F_REPLY_ACK | \
		 1ULL << VHOST_USER_PROTOCOL_F_SLAVE_REQ | \
		 1ULL << VHOST_USER_PROTOCOL_F_SLAVE_SEND_FD | \
		 1ULL << VHOST_USER_PROTOCOL_F_HOST_NOTIFIER | \
		 1ULL << VHOST_USER_PROTOCOL_F_LOG_SHMFD | \
		 1ULL << VHOST_USER_PROTOCOL_F_STATUS)
static int
ifcvf_get_protocol_features(struct rte_vdpa_device *vdev, uint64_t *features)
{
	RTE_SET_USED(vdev);

	*features = VDPA_SUPPORTED_PROTOCOL_FEATURES;
	return 0;
}

static int
ifcvf_set_vring_state(int vid, int vring, int state)
{
	struct rte_vdpa_device *vdev;
	struct internal_list *list;
	struct ifcvf_internal *internal;
	struct ifcvf_hw *hw;
	struct ifcvf_pci_common_cfg *cfg;
	int ret = 0;

	vdev = rte_vhost_get_vdpa_device(vid);
	list = find_internal_resource_by_vdev(vdev);
	if (list == NULL) {
		DRV_LOG(ERR, "Invalid vDPA device: %p", vdev);
		return -1;
	}

	internal = list->internal;
	if (vring < 0 || vring >= internal->max_queues * 2) {
		DRV_LOG(ERR, "Vring index %d not correct", vring);
		return -1;
	}

	hw = &internal->hw;
	if (!internal->configured)
		goto exit;

	cfg = hw->common_cfg;
	IFCVF_WRITE_REG16(vring, &cfg->queue_select);
	IFCVF_WRITE_REG16(!!state, &cfg->queue_enable);

	if (!state && hw->vring[vring].enable) {
		ret = vdpa_disable_vfio_intr(internal);
		if (ret)
			return ret;
	}

	if (state && !hw->vring[vring].enable) {
		ret = vdpa_enable_vfio_intr(internal, false);
		if (ret)
			return ret;
	}

exit:
	hw->vring[vring].enable = !!state;
	return 0;
}

static struct rte_vdpa_dev_ops ifcvf_ops = {
	.get_queue_num = ifcvf_get_queue_num,
	.get_features = ifcvf_get_vdpa_features,
	.get_protocol_features = ifcvf_get_protocol_features,
	.dev_conf = ifcvf_dev_config,
	.dev_close = ifcvf_dev_close,
	.set_vring_state = ifcvf_set_vring_state,
	.set_features = ifcvf_set_features,
	.migration_done = NULL,
	.get_vfio_group_fd = ifcvf_get_vfio_group_fd,
	.get_vfio_device_fd = ifcvf_get_vfio_device_fd,
	.get_notify_area = ifcvf_get_notify_area,
};

static inline int
open_int(const char *key __rte_unused, const char *value, void *extra_args)
{
	uint16_t *n = extra_args;

	if (value == NULL || extra_args == NULL)
		return -EINVAL;

	*n = (uint16_t)strtoul(value, NULL, 0);
	if (*n == USHRT_MAX && errno == ERANGE)
		return -1;

	return 0;
}

static int
ifcvf_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
		struct rte_pci_device *pci_dev)
{
	uint64_t features;
	struct ifcvf_internal *internal = NULL;
	struct internal_list *list = NULL;
	int vdpa_mode = 0;
	int sw_fallback_lm = 0;
	struct rte_kvargs *kvlist = NULL;
	int ret = 0;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	if (!pci_dev->device.devargs)
		return 1;

	kvlist = rte_kvargs_parse(pci_dev->device.devargs->args,
			ifcvf_valid_arguments);
	if (kvlist == NULL)
		return 1;

	/* probe only when vdpa mode is specified */
	if (rte_kvargs_count(kvlist, IFCVF_VDPA_MODE) == 0) {
		rte_kvargs_free(kvlist);
		return 1;
	}

	ret = rte_kvargs_process(kvlist, IFCVF_VDPA_MODE, &open_int,
			&vdpa_mode);
	if (ret < 0 || vdpa_mode == 0) {
		rte_kvargs_free(kvlist);
		return 1;
	}

	list = rte_zmalloc("ifcvf", sizeof(*list), 0);
	if (list == NULL)
		goto error;

	internal = rte_zmalloc("ifcvf", sizeof(*internal), 0);
	if (internal == NULL)
		goto error;

	internal->pdev = pci_dev;
	rte_spinlock_init(&internal->lock);

	if (ifcvf_vfio_setup(internal) < 0) {
		DRV_LOG(ERR, "failed to setup device %s", pci_dev->name);
		goto error;
	}

	if (ifcvf_init_hw(&internal->hw, internal->pdev) < 0) {
		DRV_LOG(ERR, "failed to init device %s", pci_dev->name);
		goto error;
	}

	internal->configured = 0;
	internal->max_queues = IFCVF_MAX_QUEUES;
	features = ifcvf_get_features(&internal->hw);
	internal->features = (features &
		~(1ULL << VIRTIO_F_IOMMU_PLATFORM)) |
		(1ULL << VIRTIO_NET_F_GUEST_ANNOUNCE) |
		(1ULL << VIRTIO_NET_F_CTRL_VQ) |
		(1ULL << VIRTIO_NET_F_STATUS) |
		(1ULL << VHOST_USER_F_PROTOCOL_FEATURES) |
		(1ULL << VHOST_F_LOG_ALL);

	list->internal = internal;

	if (rte_kvargs_count(kvlist, IFCVF_SW_FALLBACK_LM)) {
		ret = rte_kvargs_process(kvlist, IFCVF_SW_FALLBACK_LM,
				&open_int, &sw_fallback_lm);
		if (ret < 0)
			goto error;
	}
	internal->sw_lm = sw_fallback_lm;

	internal->vdev = rte_vdpa_register_device(&pci_dev->device, &ifcvf_ops);
	if (internal->vdev == NULL) {
		DRV_LOG(ERR, "failed to register device %s", pci_dev->name);
		goto error;
	}

	pthread_mutex_lock(&internal_list_lock);
	TAILQ_INSERT_TAIL(&internal_list, list, next);
	pthread_mutex_unlock(&internal_list_lock);

	rte_atomic32_set(&internal->started, 1);
	update_datapath(internal);

	rte_kvargs_free(kvlist);
	return 0;

error:
	rte_kvargs_free(kvlist);
	rte_free(list);
	rte_free(internal);
	return -1;
}

static int
ifcvf_pci_remove(struct rte_pci_device *pci_dev)
{
	struct ifcvf_internal *internal;
	struct internal_list *list;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	list = find_internal_resource_by_dev(pci_dev);
	if (list == NULL) {
		DRV_LOG(ERR, "Invalid device: %s", pci_dev->name);
		return -1;
	}

	internal = list->internal;
	rte_atomic32_set(&internal->started, 0);
	update_datapath(internal);

	rte_pci_unmap_device(internal->pdev);
	rte_vfio_container_destroy(internal->vfio_container_fd);
	rte_vdpa_unregister_device(internal->vdev);

	pthread_mutex_lock(&internal_list_lock);
	TAILQ_REMOVE(&internal_list, list, next);
	pthread_mutex_unlock(&internal_list_lock);

	rte_free(list);
	rte_free(internal);

	return 0;
}

/*
 * IFCVF has the same vendor ID and device ID as virtio net PCI
 * device, with its specific subsystem vendor ID and device ID.
 */
static const struct rte_pci_id pci_id_ifcvf_map[] = {
	{ .class_id = RTE_CLASS_ANY_ID,
	  .vendor_id = IFCVF_VENDOR_ID,
	  .device_id = IFCVF_DEVICE_ID,
	  .subsystem_vendor_id = IFCVF_SUBSYS_VENDOR_ID,
	  .subsystem_device_id = IFCVF_SUBSYS_DEVICE_ID,
	},

	{ .vendor_id = 0, /* sentinel */
	},
};

static struct rte_pci_driver rte_ifcvf_vdpa = {
	.id_table = pci_id_ifcvf_map,
	.drv_flags = 0,
	.probe = ifcvf_pci_probe,
	.remove = ifcvf_pci_remove,
};

RTE_PMD_REGISTER_PCI(net_ifcvf, rte_ifcvf_vdpa);
RTE_PMD_REGISTER_PCI_TABLE(net_ifcvf, pci_id_ifcvf_map);
RTE_PMD_REGISTER_KMOD_DEP(net_ifcvf, "* vfio-pci");
