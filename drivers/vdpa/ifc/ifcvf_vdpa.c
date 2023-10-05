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
#include <bus_pci_driver.h>
#include <rte_vhost.h>
#include <rte_vdpa.h>
#include <vdpa_driver.h>
#include <rte_vfio.h>
#include <rte_spinlock.h>
#include <rte_log.h>
#include <rte_kvargs.h>
#include <rte_devargs.h>

#include "base/ifcvf.h"

/*
 * RTE_MIN() cannot be used since braced-group within expression allowed
 * only inside a function.
 */
#define MIN(v1, v2)	((v1) < (v2) ? (v1) : (v2))

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
	rte_thread_t tid; /* thread for notify relay */
	rte_thread_t intr_tid; /* thread for config space change interrupt relay */
	int epfd;
	int csc_epfd;
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

/* vdpa device info includes device features and devcic operation. */
struct rte_vdpa_dev_info {
	uint64_t features;
	struct rte_vdpa_dev_ops *ops;
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
find_internal_resource_by_pci_dev(struct rte_pci_device *pdev)
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

static struct internal_list *
find_internal_resource_by_rte_dev(struct rte_device *rte_dev)
{
	int found = 0;
	struct internal_list *list;

	pthread_mutex_lock(&internal_list_lock);

	TAILQ_FOREACH(list, &internal_list, next) {
		if (rte_dev == &list->internal->pdev->device) {
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
		if (!hw->vring[i].enable)
			continue;
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
	u32 ring_state = 0;

	vid = internal->vid;

	/* to make sure no packet is lost for blk device
	 * do not stop until last_avail_idx == last_used_idx
	 */
	if (internal->hw.device_type == IFCVF_BLK) {
		for (i = 0; i < hw->nr_vring; i++) {
			do {
				if (hw->lm_cfg != NULL)
					ring_state = *(u32 *)(hw->lm_cfg +
						IFCVF_LM_RING_STATE_OFFSET +
						i * IFCVF_LM_CFG_SIZE);
				hw->vring[i].last_avail_idx =
					(u16)(ring_state & IFCVF_16_BIT_MASK);
				hw->vring[i].last_used_idx =
					(u16)(ring_state >> 16);
				usleep(10);
			} while (hw->vring[i].last_avail_idx !=
				hw->vring[i].last_used_idx);
		}
	}

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
	/* The first interrupt is for the configure space change notification */
	fd_ptr[RTE_INTR_VEC_ZERO_OFFSET] =
		rte_intr_fd_get(internal->pdev->intr_handle);

	for (i = 0; i < nr_vring; i++)
		internal->intr_fd[i] = -1;

	for (i = 0; i < nr_vring; i++) {
		rte_vhost_get_vhost_vring(internal->vid, i, &vring);
		fd_ptr[RTE_INTR_VEC_RXTX_OFFSET + i] = vring.callfd;
		if (m_rx == true &&
			((i & 1) == 0 || internal->hw.device_type == IFCVF_BLK)) {
			/* For the net we only need to relay rx queue,
			 * which will change the mem of VM.
			 * For the blk we need to relay all the read cmd
			 * of each queue
			 */
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

static uint32_t
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
		return 1;
	}
	internal->epfd = epfd;

	vring.kickfd = -1;
	for (qid = 0; qid < q_num; qid++) {
		if (!hw->vring[qid].enable)
			continue;
		ev.events = EPOLLIN | EPOLLPRI;
		rte_vhost_get_vhost_vring(internal->vid, qid, &vring);
		ev.data.u64 = qid | (uint64_t)vring.kickfd << 32;
		if (epoll_ctl(epfd, EPOLL_CTL_ADD, vring.kickfd, &ev) < 0) {
			DRV_LOG(ERR, "epoll add error: %s", strerror(errno));
			return 1;
		}
	}

	for (;;) {
		nfds = epoll_wait(epfd, events, q_num, -1);
		if (nfds < 0) {
			if (errno == EINTR)
				continue;
			DRV_LOG(ERR, "epoll_wait return fail\n");
			return 1;
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

	return 0;
}

static int
setup_notify_relay(struct ifcvf_internal *internal)
{
	char name[RTE_THREAD_INTERNAL_NAME_SIZE];
	int ret;

	snprintf(name, sizeof(name), "ifc-noti%d", internal->vid);
	ret = rte_thread_create_internal_control(&internal->tid, name,
			notify_relay, internal);
	if (ret != 0) {
		DRV_LOG(ERR, "failed to create notify relay pthread.");
		return -1;
	}

	return 0;
}

static int
unset_notify_relay(struct ifcvf_internal *internal)
{
	if (internal->tid.opaque_id != 0) {
		pthread_cancel((pthread_t)internal->tid.opaque_id);
		rte_thread_join(internal->tid, NULL);
	}
	internal->tid.opaque_id = 0;

	if (internal->epfd >= 0)
		close(internal->epfd);
	internal->epfd = -1;

	return 0;
}

static void
virtio_interrupt_handler(struct ifcvf_internal *internal)
{
	int vid = internal->vid;
	int ret;

	ret = rte_vhost_backend_config_change(vid, 1);
	if (ret)
		DRV_LOG(ERR, "failed to notify the guest about configuration space change.");
}

static uint32_t
intr_relay(void *arg)
{
	struct ifcvf_internal *internal = (struct ifcvf_internal *)arg;
	struct epoll_event csc_event;
	struct epoll_event ev;
	uint64_t buf;
	int nbytes;
	int csc_epfd, csc_val = 0;

	csc_epfd = epoll_create(1);
	if (csc_epfd < 0) {
		DRV_LOG(ERR, "failed to create epoll for config space change.");
		return 1;
	}

	ev.events = EPOLLIN | EPOLLPRI | EPOLLRDHUP | EPOLLHUP;
	ev.data.fd = rte_intr_fd_get(internal->pdev->intr_handle);
	if (epoll_ctl(csc_epfd, EPOLL_CTL_ADD,
		rte_intr_fd_get(internal->pdev->intr_handle), &ev) < 0) {
		DRV_LOG(ERR, "epoll add error: %s", strerror(errno));
		goto out;
	}

	internal->csc_epfd = csc_epfd;

	for (;;) {
		csc_val = epoll_wait(csc_epfd, &csc_event, 1, -1);
		if (csc_val < 0) {
			if (errno == EINTR)
				continue;
			DRV_LOG(ERR, "epoll_wait return fail.");
			goto out;
		} else if (csc_val == 0) {
			continue;
		} else {
			/* csc_val > 0 */
			nbytes = read(csc_event.data.fd, &buf, 8);
			if (nbytes < 0) {
				if (errno == EINTR ||
				    errno == EWOULDBLOCK ||
				    errno == EAGAIN)
					continue;
				DRV_LOG(ERR, "Error reading from file descriptor %d: %s\n",
					csc_event.data.fd,
					strerror(errno));
				goto out;
			} else if (nbytes == 0) {
				DRV_LOG(ERR, "Read nothing from file descriptor %d\n",
					csc_event.data.fd);
				continue;
			} else {
				virtio_interrupt_handler(internal);
			}
		}
	}

out:
	if (csc_epfd >= 0)
		close(csc_epfd);
	internal->csc_epfd = -1;

	return 0;
}

static int
setup_intr_relay(struct ifcvf_internal *internal)
{
	char name[RTE_THREAD_INTERNAL_NAME_SIZE];
	int ret;

	snprintf(name, sizeof(name), "ifc-int%d", internal->vid);
	ret = rte_thread_create_internal_control(&internal->intr_tid, name,
			intr_relay, (void *)internal);
	if (ret) {
		DRV_LOG(ERR, "failed to create notify relay pthread.");
		return -1;
	}
	return 0;
}

static void
unset_intr_relay(struct ifcvf_internal *internal)
{
	if (internal->intr_tid.opaque_id != 0) {
		pthread_cancel((pthread_t)internal->intr_tid.opaque_id);
		rte_thread_join(internal->intr_tid, NULL);
	}
	internal->intr_tid.opaque_id = 0;

	if (internal->csc_epfd >= 0)
		close(internal->csc_epfd);
	internal->csc_epfd = -1;
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

		ret = setup_intr_relay(internal);
		if (ret)
			goto err;

		rte_atomic32_set(&internal->running, 1);
	} else if (rte_atomic32_read(&internal->running) &&
		   (!rte_atomic32_read(&internal->started) ||
		    !rte_atomic32_read(&internal->dev_attached))) {
		unset_intr_relay(internal);

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

		/* NET: Direct I/O for Tx queue, relay for Rx queue
		 * BLK: relay every queue
		 */
		if ((internal->hw.device_type == IFCVF_NET) && (i & 1)) {
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
	u32 ring_state = 0;

	vid = internal->vid;

	/* to make sure no packet is lost for blk device
	 * do not stop until last_avail_idx == last_used_idx
	 */
	if (internal->hw.device_type == IFCVF_BLK) {
		for (i = 0; i < hw->nr_vring; i++) {
			do {
				if (hw->lm_cfg != NULL)
					ring_state = *(u32 *)(hw->lm_cfg +
						IFCVF_LM_RING_STATE_OFFSET +
						i * IFCVF_LM_CFG_SIZE);
				hw->vring[i].last_avail_idx =
					(u16)(ring_state & IFCVF_16_BIT_MASK);
				hw->vring[i].last_used_idx =
					(u16)(ring_state >> 16);
				usleep(10);
			} while (hw->vring[i].last_avail_idx !=
				hw->vring[i].last_used_idx);
		}
	}

	ifcvf_stop_hw(hw);

	for (i = 0; i < hw->nr_vring; i++) {
		/* synchronize remaining new used entries if any */
		if (internal->hw.device_type == IFCVF_NET) {
			if ((i & 1) == 0)
				update_used_ring(internal, i);
		} else if (internal->hw.device_type == IFCVF_BLK) {
			update_used_ring(internal, i);
		}

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

static uint32_t
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
		return 1;
	}
	internal->epfd = epfd;

	vring.kickfd = -1;
	for (qid = 0; qid < q_num; qid++) {
		ev.events = EPOLLIN | EPOLLPRI;
		rte_vhost_get_vhost_vring(vid, qid, &vring);
		ev.data.u64 = qid << 1 | (uint64_t)vring.kickfd << 32;
		if (epoll_ctl(epfd, EPOLL_CTL_ADD, vring.kickfd, &ev) < 0) {
			DRV_LOG(ERR, "epoll add error: %s", strerror(errno));
			return 1;
		}
	}

	for (qid = 0; qid < q_num; qid += 1) {
		if ((internal->hw.device_type == IFCVF_NET) && (qid & 1))
			continue;
		ev.events = EPOLLIN | EPOLLPRI;
		/* leave a flag to mark it's for interrupt */
		ev.data.u64 = 1 | qid << 1 |
			(uint64_t)internal->intr_fd[qid] << 32;
		if (epoll_ctl(epfd, EPOLL_CTL_ADD, internal->intr_fd[qid], &ev)
				< 0) {
			DRV_LOG(ERR, "epoll add error: %s", strerror(errno));
			return 1;
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
			DRV_LOG(ERR, "epoll_wait return fail.");
			return 1;
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

	return 0;
}

static int
setup_vring_relay(struct ifcvf_internal *internal)
{
	char name[RTE_THREAD_INTERNAL_NAME_SIZE];
	int ret;

	snprintf(name, sizeof(name), "ifc-ring%d", internal->vid);
	ret = rte_thread_create_internal_control(&internal->tid, name,
			vring_relay, internal);
	if (ret != 0) {
		DRV_LOG(ERR, "failed to create ring relay pthread.");
		return -1;
	}

	return 0;
}

static int
unset_vring_relay(struct ifcvf_internal *internal)
{
	if (internal->tid.opaque_id != 0) {
		pthread_cancel((pthread_t)internal->tid.opaque_id);
		rte_thread_join(internal->tid, NULL);
	}
	internal->tid.opaque_id = 0;

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

	unset_intr_relay(internal);

	vdpa_disable_vfio_intr(internal);

	rte_atomic32_set(&internal->running, 0);

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
	struct ifcvf_hw *hw;
	uint16_t i;

	vdev = rte_vhost_get_vdpa_device(vid);
	list = find_internal_resource_by_vdev(vdev);
	if (list == NULL) {
		DRV_LOG(ERR, "Invalid vDPA device: %p", vdev);
		return -1;
	}

	internal = list->internal;
	internal->vid = vid;
	rte_atomic32_set(&internal->dev_attached, 1);
	if (update_datapath(internal) < 0) {
		DRV_LOG(ERR, "failed to update datapath for vDPA device %s",
			vdev->device->name);
		rte_atomic32_set(&internal->dev_attached, 0);
		return -1;
	}

	hw = &internal->hw;
	for (i = 0; i < hw->nr_vring; i++) {
		if (!hw->vring[i].enable)
			continue;
		if (rte_vhost_host_notifier_ctrl(vid, i, true) != 0)
			DRV_LOG(NOTICE, "vDPA (%s): software relay is used.",
				vdev->device->name);
	}

	internal->configured = 1;
	DRV_LOG(INFO, "vDPA device %s is configured", vdev->device->name);
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
		if (update_datapath(internal) < 0) {
			DRV_LOG(ERR, "failed to update datapath for vDPA device %s",
				vdev->device->name);
			internal->configured = 0;
			return -1;
		}
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
		 1ULL << VHOST_USER_PROTOCOL_F_BACKEND_REQ | \
		 1ULL << VHOST_USER_PROTOCOL_F_BACKEND_SEND_FD | \
		 1ULL << VHOST_USER_PROTOCOL_F_HOST_NOTIFIER | \
		 1ULL << VHOST_USER_PROTOCOL_F_LOG_SHMFD | \
		 1ULL << VHOST_USER_PROTOCOL_F_MQ | \
		 1ULL << VHOST_USER_PROTOCOL_F_STATUS)

#define VDPA_BLK_PROTOCOL_FEATURES \
		(1ULL << VHOST_USER_PROTOCOL_F_CONFIG)

static int
ifcvf_get_protocol_features(struct rte_vdpa_device *vdev, uint64_t *features)
{
	RTE_SET_USED(vdev);

	*features = VDPA_SUPPORTED_PROTOCOL_FEATURES;
	return 0;
}

static int
ifcvf_config_vring(struct ifcvf_internal *internal, int vring)
{
	struct ifcvf_hw *hw = &internal->hw;
	int vid = internal->vid;
	struct rte_vhost_vring vq;
	uint64_t gpa;

	if (hw->vring[vring].enable) {
		rte_vhost_get_vhost_vring(vid, vring, &vq);
		gpa = hva_to_gpa(vid, (uint64_t)(uintptr_t)vq.desc);
		if (gpa == 0) {
			DRV_LOG(ERR, "Fail to get GPA for descriptor ring.");
			return -1;
		}
		hw->vring[vring].desc = gpa;

		gpa = hva_to_gpa(vid, (uint64_t)(uintptr_t)vq.avail);
		if (gpa == 0) {
			DRV_LOG(ERR, "Fail to get GPA for available ring.");
			return -1;
		}
		hw->vring[vring].avail = gpa;

		gpa = hva_to_gpa(vid, (uint64_t)(uintptr_t)vq.used);
		if (gpa == 0) {
			DRV_LOG(ERR, "Fail to get GPA for used ring.");
			return -1;
		}
		hw->vring[vring].used = gpa;

		hw->vring[vring].size = vq.size;
		rte_vhost_get_vring_base(vid, vring,
				&hw->vring[vring].last_avail_idx,
				&hw->vring[vring].last_used_idx);
		ifcvf_enable_vring_hw(&internal->hw, vring);
	} else {
		ifcvf_disable_vring_hw(&internal->hw, vring);
		rte_vhost_set_vring_base(vid, vring,
				hw->vring[vring].last_avail_idx,
				hw->vring[vring].last_used_idx);
	}

	return 0;
}

static int
ifcvf_set_vring_state(int vid, int vring, int state)
{
	struct rte_vdpa_device *vdev;
	struct internal_list *list;
	struct ifcvf_internal *internal;
	struct ifcvf_hw *hw;
	bool enable = !!state;
	int ret = 0;

	vdev = rte_vhost_get_vdpa_device(vid);
	list = find_internal_resource_by_vdev(vdev);
	if (list == NULL) {
		DRV_LOG(ERR, "Invalid vDPA device: %p", vdev);
		return -1;
	}

	DRV_LOG(INFO, "%s queue %d of vDPA device %s",
		enable ? "enable" : "disable", vring, vdev->device->name);

	internal = list->internal;
	if (vring < 0 || vring >= internal->max_queues * 2) {
		DRV_LOG(ERR, "Vring index %d not correct", vring);
		return -1;
	}

	hw = &internal->hw;
	hw->vring[vring].enable = enable;

	if (!internal->configured)
		return 0;

	unset_notify_relay(internal);

	ret = vdpa_enable_vfio_intr(internal, false);
	if (ret) {
		DRV_LOG(ERR, "failed to set vfio interrupt of vDPA device %s",
			vdev->device->name);
		return ret;
	}

	ret = ifcvf_config_vring(internal, vring);
	if (ret) {
		DRV_LOG(ERR, "failed to configure queue %d of vDPA device %s",
			vring, vdev->device->name);
		return ret;
	}

	ret = setup_notify_relay(internal);
	if (ret) {
		DRV_LOG(ERR, "failed to setup notify relay of vDPA device %s",
			vdev->device->name);
		return ret;
	}

	ret = rte_vhost_host_notifier_ctrl(vid, vring, enable);
	if (ret) {
		DRV_LOG(ERR, "vDPA device %s queue %d host notifier ctrl fail",
			vdev->device->name, vring);
		return ret;
	}

	return 0;
}

static int
ifcvf_get_device_type(struct rte_vdpa_device *vdev,
	uint32_t *type)
{
	struct ifcvf_internal *internal;
	struct internal_list *list;
	struct rte_device *rte_dev = vdev->device;

	list = find_internal_resource_by_rte_dev(rte_dev);
	if (list == NULL) {
		DRV_LOG(ERR, "Invalid rte device: %p", rte_dev);
		return -1;
	}

	internal = list->internal;

	if (internal->hw.device_type == IFCVF_BLK)
		*type = RTE_VHOST_VDPA_DEVICE_TYPE_BLK;
	else
		*type = RTE_VHOST_VDPA_DEVICE_TYPE_NET;

	return 0;
}

static struct rte_vdpa_dev_ops ifcvf_net_ops = {
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
	.get_dev_type = ifcvf_get_device_type,
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

static int16_t
ifcvf_pci_get_device_type(struct rte_pci_device *pci_dev)
{
	uint16_t pci_device_id = pci_dev->id.device_id;
	uint16_t device_id;

	if (pci_device_id < 0x1000 || pci_device_id > 0x107f) {
		DRV_LOG(ERR, "Probe device is not a virtio device\n");
		return -1;
	}

	if (pci_device_id < 0x1040) {
		/* Transitional devices: use the PCI subsystem device id as
		 * virtio device id, same as legacy driver always did.
		 */
		device_id = pci_dev->id.subsystem_device_id;
	} else {
		/* Modern devices: simply use PCI device id,
		 * but start from 0x1040.
		 */
		device_id = pci_device_id - 0x1040;
	}

	return device_id;
}

static int
ifcvf_blk_get_config(int vid, uint8_t *config, uint32_t size)
{
	struct virtio_blk_config *dev_cfg;
	struct ifcvf_internal *internal;
	struct rte_vdpa_device *vdev;
	struct internal_list *list;
	uint32_t i;
	uint64_t capacity = 0;
	uint8_t *byte;

	if (size < sizeof(struct virtio_blk_config)) {
		DRV_LOG(ERR, "Invalid len: %u, required: %u",
			size, (uint32_t)sizeof(struct virtio_blk_config));
		return -1;
	}

	vdev = rte_vhost_get_vdpa_device(vid);
	if (vdev == NULL) {
		DRV_LOG(ERR, "Invalid vDPA device vid: %d", vid);
		return -1;
	}

	list = find_internal_resource_by_vdev(vdev);
	if (list == NULL) {
		DRV_LOG(ERR, "Invalid vDPA device: %p", vdev);
		return -1;
	}

	internal = list->internal;

	for (i = 0; i < sizeof(struct virtio_blk_config); i++)
		config[i] = *((u8 *)internal->hw.blk_cfg + i);

	dev_cfg = (struct virtio_blk_config *)internal->hw.blk_cfg;

	/* cannot read 64-bit register in one attempt, so read byte by byte. */
	for (i = 0; i < sizeof(internal->hw.blk_cfg->capacity); i++) {
		byte = (uint8_t *)&internal->hw.blk_cfg->capacity + i;
		capacity |= (uint64_t)*byte << (i * 8);
	}
	/* The capacity is number of sectors in 512-byte.
	 * So right shift 1 bit  we get in K,
	 * another right shift 10 bits we get in M,
	 * right shift 10 more bits, we get in G.
	 * To show capacity in G, we right shift 21 bits in total.
	 */
	DRV_LOG(DEBUG, "capacity  : %"PRIu64"G", capacity >> 21);

	DRV_LOG(DEBUG, "size_max  : 0x%08x", dev_cfg->size_max);
	DRV_LOG(DEBUG, "seg_max   : 0x%08x", dev_cfg->seg_max);
	DRV_LOG(DEBUG, "blk_size  : 0x%08x", dev_cfg->blk_size);
	DRV_LOG(DEBUG, "geometry");
	DRV_LOG(DEBUG, "      cylinders: %u", dev_cfg->geometry.cylinders);
	DRV_LOG(DEBUG, "      heads    : %u", dev_cfg->geometry.heads);
	DRV_LOG(DEBUG, "      sectors  : %u", dev_cfg->geometry.sectors);
	DRV_LOG(DEBUG, "num_queues: 0x%08x", dev_cfg->num_queues);

	DRV_LOG(DEBUG, "config: [%x] [%x] [%x] [%x] [%x] [%x] [%x] [%x]\n",
		config[0], config[1], config[2], config[3], config[4],
		config[5], config[6], config[7]);
	return 0;
}

static int
ifcvf_blk_get_protocol_features(struct rte_vdpa_device *vdev,
	uint64_t *features)
{
	RTE_SET_USED(vdev);

	*features = VDPA_SUPPORTED_PROTOCOL_FEATURES;
	*features |= VDPA_BLK_PROTOCOL_FEATURES;
	return 0;
}

static struct rte_vdpa_dev_ops ifcvf_blk_ops = {
	.get_queue_num = ifcvf_get_queue_num,
	.get_features = ifcvf_get_vdpa_features,
	.set_features = ifcvf_set_features,
	.get_protocol_features = ifcvf_blk_get_protocol_features,
	.dev_conf = ifcvf_dev_config,
	.dev_close = ifcvf_dev_close,
	.set_vring_state = ifcvf_set_vring_state,
	.migration_done = NULL,
	.get_vfio_group_fd = ifcvf_get_vfio_group_fd,
	.get_vfio_device_fd = ifcvf_get_vfio_device_fd,
	.get_notify_area = ifcvf_get_notify_area,
	.get_config = ifcvf_blk_get_config,
	.get_dev_type = ifcvf_get_device_type,
};

struct rte_vdpa_dev_info dev_info[] = {
	{
		.features = (1ULL << VIRTIO_NET_F_GUEST_ANNOUNCE) |
			    (1ULL << VIRTIO_NET_F_CTRL_VQ) |
			    (1ULL << VIRTIO_NET_F_STATUS) |
			    (1ULL << VHOST_USER_F_PROTOCOL_FEATURES) |
			    (1ULL << VHOST_F_LOG_ALL),
		.ops = &ifcvf_net_ops,
	},
	{
		.features = (1ULL << VHOST_USER_F_PROTOCOL_FEATURES) |
			    (1ULL << VHOST_F_LOG_ALL),
		.ops = &ifcvf_blk_ops,
	},
};

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
	int16_t device_id;
	uint64_t capacity = 0;
	uint8_t *byte;
	uint32_t i;
	uint16_t queue_pairs;

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
	features = ifcvf_get_features(&internal->hw);

	device_id = ifcvf_pci_get_device_type(pci_dev);
	if (device_id < 0) {
		DRV_LOG(ERR, "failed to get device %s type", pci_dev->name);
		goto error;
	}

	if (device_id == VIRTIO_ID_NET) {
		internal->hw.device_type = IFCVF_NET;
		/*
		 * ifc device always has CTRL_VQ,
		 * and supports VIRTIO_NET_F_CTRL_VQ feature.
		 */
		queue_pairs = (internal->hw.common_cfg->num_queues - 1) / 2;
		DRV_LOG(INFO, "%s support %u queue pairs", pci_dev->name,
			queue_pairs);
		internal->max_queues = MIN(IFCVF_MAX_QUEUES, queue_pairs);
		internal->features = features &
					~(1ULL << VIRTIO_F_IOMMU_PLATFORM);
		internal->features |= dev_info[IFCVF_NET].features;
	} else if (device_id == VIRTIO_ID_BLOCK) {
		internal->hw.device_type = IFCVF_BLK;
		internal->features = features &
					~(1ULL << VIRTIO_F_IOMMU_PLATFORM);
		internal->features |= dev_info[IFCVF_BLK].features;

		/* cannot read 64-bit register in one attempt,
		 * so read byte by byte.
		 */
		for (i = 0; i < sizeof(internal->hw.blk_cfg->capacity); i++) {
			byte = (uint8_t *)&internal->hw.blk_cfg->capacity + i;
			capacity |= (uint64_t)*byte << (i * 8);
		}
		/* The capacity is number of sectors in 512-byte.
		 * So right shift 1 bit  we get in K,
		 * another right shift 10 bits we get in M,
		 * right shift 10 more bits, we get in G.
		 * To show capacity in G, we right shift 21 bits in total.
		 */
		DRV_LOG(DEBUG, "capacity  : %"PRIu64"G", capacity >> 21);

		DRV_LOG(DEBUG, "size_max  : 0x%08x",
			internal->hw.blk_cfg->size_max);
		DRV_LOG(DEBUG, "seg_max   : 0x%08x",
			internal->hw.blk_cfg->seg_max);
		DRV_LOG(DEBUG, "blk_size  : 0x%08x",
			internal->hw.blk_cfg->blk_size);
		DRV_LOG(DEBUG, "geometry");
		DRV_LOG(DEBUG, "    cylinders: %u",
			internal->hw.blk_cfg->geometry.cylinders);
		DRV_LOG(DEBUG, "    heads    : %u",
			internal->hw.blk_cfg->geometry.heads);
		DRV_LOG(DEBUG, "    sectors  : %u",
			internal->hw.blk_cfg->geometry.sectors);
		DRV_LOG(DEBUG, "num_queues: 0x%08x",
			internal->hw.blk_cfg->num_queues);

		internal->max_queues = MIN(IFCVF_MAX_QUEUES,
			internal->hw.blk_cfg->num_queues);
	}

	list->internal = internal;

	if (rte_kvargs_count(kvlist, IFCVF_SW_FALLBACK_LM)) {
		ret = rte_kvargs_process(kvlist, IFCVF_SW_FALLBACK_LM,
				&open_int, &sw_fallback_lm);
		if (ret < 0)
			goto error;
	}
	internal->sw_lm = sw_fallback_lm;
	if (!internal->sw_lm && !internal->hw.lm_cfg) {
		DRV_LOG(ERR, "Device %s does not support HW assist live migration, please enable sw-live-migration!",
			pci_dev->name);
		goto error;
	}

	pthread_mutex_lock(&internal_list_lock);
	TAILQ_INSERT_TAIL(&internal_list, list, next);
	pthread_mutex_unlock(&internal_list_lock);

	internal->vdev = rte_vdpa_register_device(&pci_dev->device,
				dev_info[internal->hw.device_type].ops);
	if (internal->vdev == NULL) {
		DRV_LOG(ERR, "failed to register device %s", pci_dev->name);
		pthread_mutex_lock(&internal_list_lock);
		TAILQ_REMOVE(&internal_list, list, next);
		pthread_mutex_unlock(&internal_list_lock);
		goto error;
	}

	rte_atomic32_set(&internal->started, 1);
	if (update_datapath(internal) < 0) {
		DRV_LOG(ERR, "failed to update datapath %s", pci_dev->name);
		rte_atomic32_set(&internal->started, 0);
		rte_vdpa_unregister_device(internal->vdev);
		pthread_mutex_lock(&internal_list_lock);
		TAILQ_REMOVE(&internal_list, list, next);
		pthread_mutex_unlock(&internal_list_lock);
		goto error;
	}

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

	list = find_internal_resource_by_pci_dev(pci_dev);
	if (list == NULL) {
		DRV_LOG(ERR, "Invalid device: %s", pci_dev->name);
		return -1;
	}

	internal = list->internal;
	rte_atomic32_set(&internal->started, 0);
	if (update_datapath(internal) < 0)
		DRV_LOG(ERR, "failed to update datapath %s", pci_dev->name);

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
	  .device_id = IFCVF_NET_MODERN_DEVICE_ID,
	  .subsystem_vendor_id = IFCVF_SUBSYS_VENDOR_ID,
	  .subsystem_device_id = IFCVF_SUBSYS_DEVICE_ID,
	},

	{ .class_id = RTE_CLASS_ANY_ID,
	  .vendor_id = IFCVF_VENDOR_ID,
	  .device_id = IFCVF_NET_TRANSITIONAL_DEVICE_ID,
	  .subsystem_vendor_id = IFCVF_SUBSYS_VENDOR_ID,
	  .subsystem_device_id = IFCVF_SUBSYS_NET_DEVICE_ID,
	},

	{ .class_id = RTE_CLASS_ANY_ID,
	  .vendor_id = IFCVF_VENDOR_ID,
	  .device_id = IFCVF_BLK_TRANSITIONAL_DEVICE_ID,
	  .subsystem_vendor_id = IFCVF_SUBSYS_VENDOR_ID,
	  .subsystem_device_id = IFCVF_SUBSYS_BLK_DEVICE_ID,
	},

	{ .class_id = RTE_CLASS_ANY_ID,
	  .vendor_id = IFCVF_VENDOR_ID,
	  .device_id = IFCVF_BLK_MODERN_DEVICE_ID,
	  .subsystem_vendor_id = IFCVF_SUBSYS_VENDOR_ID,
	  .subsystem_device_id = IFCVF_SUBSYS_BLK_DEVICE_ID,
	},

	{ .class_id = RTE_CLASS_ANY_ID,
	  .vendor_id = IFCVF_VENDOR_ID,
	  .device_id = IFCVF_BLK_MODERN_DEVICE_ID,
	  .subsystem_vendor_id = IFCVF_SUBSYS_VENDOR_ID,
	  .subsystem_device_id = IFCVF_SUBSYS_DEFAULT_DEVICE_ID,
	}, /* virtio-blk devices with default subsystem IDs */

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
