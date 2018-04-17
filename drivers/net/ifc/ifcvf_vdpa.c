/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/epoll.h>

#include <rte_malloc.h>
#include <rte_memory.h>
#include <rte_bus_pci.h>
#include <rte_vhost.h>
#include <rte_vdpa.h>
#include <rte_vfio.h>
#include <rte_spinlock.h>
#include <rte_log.h>

#include "base/ifcvf.h"

#define DRV_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, ifcvf_vdpa_logtype, \
		"%s(): " fmt "\n", __func__, ##args)

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif

static int ifcvf_vdpa_logtype;

struct ifcvf_internal {
	struct rte_vdpa_dev_addr dev_addr;
	struct rte_pci_device *pdev;
	struct ifcvf_hw hw;
	int vfio_container_fd;
	int vfio_group_fd;
	int vfio_dev_fd;
	pthread_t tid;	/* thread for notify relay */
	int epfd;
	int vid;
	int did;
	uint16_t max_queues;
	uint64_t features;
	rte_atomic32_t started;
	rte_atomic32_t dev_attached;
	rte_atomic32_t running;
	rte_spinlock_t lock;
};

struct internal_list {
	TAILQ_ENTRY(internal_list) next;
	struct ifcvf_internal *internal;
};

TAILQ_HEAD(internal_list_head, internal_list);
static struct internal_list_head internal_list =
	TAILQ_HEAD_INITIALIZER(internal_list);

static pthread_mutex_t internal_list_lock = PTHREAD_MUTEX_INITIALIZER;

static struct internal_list *
find_internal_resource_by_did(int did)
{
	int found = 0;
	struct internal_list *list;

	pthread_mutex_lock(&internal_list_lock);

	TAILQ_FOREACH(list, &internal_list, next) {
		if (did == list->internal->did) {
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
		if (pdev == list->internal->pdev) {
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
	int ret = 0;
	int i;

	internal->vfio_dev_fd = -1;
	internal->vfio_group_fd = -1;
	internal->vfio_container_fd = -1;

	rte_pci_device_name(&dev->addr, devname, RTE_DEV_NAME_MAX_LEN);
	rte_vfio_get_group_num(rte_pci_get_sysfs_path(), devname,
			&iommu_group_num);

	internal->vfio_container_fd = rte_vfio_container_create();
	if (internal->vfio_container_fd < 0)
		return -1;

	internal->vfio_group_fd = rte_vfio_container_group_bind(
			internal->vfio_container_fd, iommu_group_num);
	if (internal->vfio_group_fd < 0)
		goto err;

	if (rte_pci_map_device(dev))
		goto err;

	internal->vfio_dev_fd = dev->intr_handle.vfio_dev_fd;

	for (i = 0; i < RTE_MIN(PCI_MAX_RESOURCE, IFCVF_PCI_MAX_RESOURCE);
			i++) {
		internal->hw.mem_resource[i].addr =
			internal->pdev->mem_resource[i].addr;
		internal->hw.mem_resource[i].phys_addr =
			internal->pdev->mem_resource[i].phys_addr;
		internal->hw.mem_resource[i].len =
			internal->pdev->mem_resource[i].len;
	}
	ret = ifcvf_init_hw(&internal->hw, internal->pdev);

	return ret;

err:
	rte_vfio_container_destroy(internal->vfio_container_fd);
	return -1;
}

static int
ifcvf_dma_map(struct ifcvf_internal *internal, int do_map)
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
qva_to_gpa(int vid, uint64_t qva)
{
	struct rte_vhost_memory *mem = NULL;
	struct rte_vhost_mem_region *reg;
	uint32_t i;
	uint64_t gpa = 0;

	if (rte_vhost_get_mem_table(vid, &mem) < 0)
		goto exit;

	for (i = 0; i < mem->nregions; i++) {
		reg = &mem->regions[i];

		if (qva >= reg->host_user_addr &&
				qva < reg->host_user_addr + reg->size) {
			gpa = qva - reg->host_user_addr + reg->guest_phys_addr;
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
		gpa = qva_to_gpa(vid, (uint64_t)(uintptr_t)vq.desc);
		if (gpa == 0) {
			DRV_LOG(ERR, "Fail to get GPA for descriptor ring.");
			return -1;
		}
		hw->vring[i].desc = gpa;

		gpa = qva_to_gpa(vid, (uint64_t)(uintptr_t)vq.avail);
		if (gpa == 0) {
			DRV_LOG(ERR, "Fail to get GPA for available ring.");
			return -1;
		}
		hw->vring[i].avail = gpa;

		gpa = qva_to_gpa(vid, (uint64_t)(uintptr_t)vq.used);
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

	vid = internal->vid;
	ifcvf_stop_hw(hw);

	for (i = 0; i < hw->nr_vring; i++)
		rte_vhost_set_vring_base(vid, i, hw->vring[i].last_avail_idx,
				hw->vring[i].last_used_idx);
}

#define MSIX_IRQ_SET_BUF_LEN (sizeof(struct vfio_irq_set) + \
		sizeof(int) * (IFCVF_MAX_QUEUES * 2 + 1))
static int
vdpa_enable_vfio_intr(struct ifcvf_internal *internal)
{
	int ret;
	uint32_t i, nr_vring;
	char irq_set_buf[MSIX_IRQ_SET_BUF_LEN];
	struct vfio_irq_set *irq_set;
	int *fd_ptr;
	struct rte_vhost_vring vring;

	nr_vring = rte_vhost_get_vring_num(internal->vid);

	irq_set = (struct vfio_irq_set *)irq_set_buf;
	irq_set->argsz = sizeof(irq_set_buf);
	irq_set->count = nr_vring + 1;
	irq_set->flags = VFIO_IRQ_SET_DATA_EVENTFD |
			 VFIO_IRQ_SET_ACTION_TRIGGER;
	irq_set->index = VFIO_PCI_MSIX_IRQ_INDEX;
	irq_set->start = 0;
	fd_ptr = (int *)&irq_set->data;
	fd_ptr[RTE_INTR_VEC_ZERO_OFFSET] = internal->pdev->intr_handle.fd;

	for (i = 0; i < nr_vring; i++) {
		rte_vhost_get_vhost_vring(internal->vid, i, &vring);
		fd_ptr[RTE_INTR_VEC_RXTX_OFFSET + i] = vring.callfd;
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
	char irq_set_buf[MSIX_IRQ_SET_BUF_LEN];
	struct vfio_irq_set *irq_set;

	irq_set = (struct vfio_irq_set *)irq_set_buf;
	irq_set->argsz = sizeof(irq_set_buf);
	irq_set->count = 0;
	irq_set->flags = VFIO_IRQ_SET_DATA_NONE | VFIO_IRQ_SET_ACTION_TRIGGER;
	irq_set->index = VFIO_PCI_MSIX_IRQ_INDEX;
	irq_set->start = 0;

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
	int ret;

	ret = pthread_create(&internal->tid, NULL, notify_relay,
			(void *)internal);
	if (ret) {
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
		ret = ifcvf_dma_map(internal, 1);
		if (ret)
			goto err;

		ret = vdpa_enable_vfio_intr(internal);
		if (ret)
			goto err;

		ret = setup_notify_relay(internal);
		if (ret)
			goto err;

		ret = vdpa_ifcvf_start(internal);
		if (ret)
			goto err;

		rte_atomic32_set(&internal->running, 1);
	} else if (rte_atomic32_read(&internal->running) &&
		   (!rte_atomic32_read(&internal->started) ||
		    !rte_atomic32_read(&internal->dev_attached))) {
		vdpa_ifcvf_stop(internal);

		ret = unset_notify_relay(internal);
		if (ret)
			goto err;

		ret = vdpa_disable_vfio_intr(internal);
		if (ret)
			goto err;

		ret = ifcvf_dma_map(internal, 0);
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
ifcvf_dev_config(int vid)
{
	int did;
	struct internal_list *list;
	struct ifcvf_internal *internal;

	did = rte_vhost_get_vdpa_device_id(vid);
	list = find_internal_resource_by_did(did);
	if (list == NULL) {
		DRV_LOG(ERR, "Invalid device id: %d", did);
		return -1;
	}

	internal = list->internal;
	internal->vid = vid;
	rte_atomic32_set(&internal->dev_attached, 1);
	update_datapath(internal);

	return 0;
}

static int
ifcvf_dev_close(int vid)
{
	int did;
	struct internal_list *list;
	struct ifcvf_internal *internal;

	did = rte_vhost_get_vdpa_device_id(vid);
	list = find_internal_resource_by_did(did);
	if (list == NULL) {
		DRV_LOG(ERR, "Invalid device id: %d", did);
		return -1;
	}

	internal = list->internal;
	rte_atomic32_set(&internal->dev_attached, 0);
	update_datapath(internal);

	return 0;
}

static int
ifcvf_get_vfio_group_fd(int vid)
{
	int did;
	struct internal_list *list;

	did = rte_vhost_get_vdpa_device_id(vid);
	list = find_internal_resource_by_did(did);
	if (list == NULL) {
		DRV_LOG(ERR, "Invalid device id: %d", did);
		return -1;
	}

	return list->internal->vfio_group_fd;
}

static int
ifcvf_get_vfio_device_fd(int vid)
{
	int did;
	struct internal_list *list;

	did = rte_vhost_get_vdpa_device_id(vid);
	list = find_internal_resource_by_did(did);
	if (list == NULL) {
		DRV_LOG(ERR, "Invalid device id: %d", did);
		return -1;
	}

	return list->internal->vfio_dev_fd;
}

static int
ifcvf_get_notify_area(int vid, int qid, uint64_t *offset, uint64_t *size)
{
	int did;
	struct internal_list *list;
	struct ifcvf_internal *internal;
	struct vfio_region_info reg = { .argsz = sizeof(reg) };
	int ret;

	did = rte_vhost_get_vdpa_device_id(vid);
	list = find_internal_resource_by_did(did);
	if (list == NULL) {
		DRV_LOG(ERR, "Invalid device id: %d", did);
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
ifcvf_get_queue_num(int did, uint32_t *queue_num)
{
	struct internal_list *list;

	list = find_internal_resource_by_did(did);
	if (list == NULL) {
		DRV_LOG(ERR, "Invalid device id: %d", did);
		return -1;
	}

	*queue_num = list->internal->max_queues;

	return 0;
}

static int
ifcvf_get_vdpa_features(int did, uint64_t *features)
{
	struct internal_list *list;

	list = find_internal_resource_by_did(did);
	if (list == NULL) {
		DRV_LOG(ERR, "Invalid device id: %d", did);
		return -1;
	}

	*features = list->internal->features;

	return 0;
}

#define VDPA_SUPPORTED_PROTOCOL_FEATURES \
		(1ULL << VHOST_USER_PROTOCOL_F_REPLY_ACK | \
		 1ULL << VHOST_USER_PROTOCOL_F_LOG_SHMFD)
static int
ifcvf_get_protocol_features(int did __rte_unused, uint64_t *features)
{
	*features = VDPA_SUPPORTED_PROTOCOL_FEATURES;
	return 0;
}

struct rte_vdpa_dev_ops ifcvf_ops = {
	.get_queue_num = ifcvf_get_queue_num,
	.get_features = ifcvf_get_vdpa_features,
	.get_protocol_features = ifcvf_get_protocol_features,
	.dev_conf = ifcvf_dev_config,
	.dev_close = ifcvf_dev_close,
	.set_vring_state = NULL,
	.set_features = NULL,
	.migration_done = NULL,
	.get_vfio_group_fd = ifcvf_get_vfio_group_fd,
	.get_vfio_device_fd = ifcvf_get_vfio_device_fd,
	.get_notify_area = ifcvf_get_notify_area,
};

static int
ifcvf_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
		struct rte_pci_device *pci_dev)
{
	uint64_t features;
	struct ifcvf_internal *internal = NULL;
	struct internal_list *list = NULL;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	list = rte_zmalloc("ifcvf", sizeof(*list), 0);
	if (list == NULL)
		goto error;

	internal = rte_zmalloc("ifcvf", sizeof(*internal), 0);
	if (internal == NULL)
		goto error;

	internal->pdev = pci_dev;
	rte_spinlock_init(&internal->lock);
	if (ifcvf_vfio_setup(internal) < 0)
		return -1;

	internal->max_queues = IFCVF_MAX_QUEUES;
	features = ifcvf_get_features(&internal->hw);
	internal->features = (features &
		~(1ULL << VIRTIO_F_IOMMU_PLATFORM)) |
		(1ULL << VHOST_USER_F_PROTOCOL_FEATURES);

	internal->dev_addr.pci_addr = pci_dev->addr;
	internal->dev_addr.type = PCI_ADDR;
	list->internal = internal;

	pthread_mutex_lock(&internal_list_lock);
	TAILQ_INSERT_TAIL(&internal_list, list, next);
	pthread_mutex_unlock(&internal_list_lock);

	internal->did = rte_vdpa_register_device(&internal->dev_addr,
				&ifcvf_ops);
	if (internal->did < 0)
		goto error;

	rte_atomic32_set(&internal->started, 1);
	update_datapath(internal);

	return 0;

error:
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
	rte_vdpa_unregister_device(internal->did);

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

RTE_INIT(ifcvf_vdpa_init_log);
static void
ifcvf_vdpa_init_log(void)
{
	ifcvf_vdpa_logtype = rte_log_register("pmd.net.ifcvf_vdpa");
	if (ifcvf_vdpa_logtype >= 0)
		rte_log_set_level(ifcvf_vdpa_logtype, RTE_LOG_NOTICE);
}
