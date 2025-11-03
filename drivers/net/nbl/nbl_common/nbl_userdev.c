/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2025 Nebulamatrix Technology Co., Ltd.
 */

#include "nbl_userdev.h"
#include <uapi/linux/vfio.h>
#include <rte_vfio.h>

#define NBL_USERDEV_EVENT_CLB_NAME	"nbl_userspace_mem_event_clb"
#define NBL_USERDEV_BAR0_SIZE		65536
#define NBL_USERDEV_DMA_LIMIT		0xFFFFFFFFFFFF

/* Size of the buffer to receive kernel messages */
#define NBL_NL_BUF_SIZE (32 * 1024)
/* Send buffer size for the Netlink socket */
#define NBL_SEND_BUF_SIZE 32768
/* Receive buffer size for the Netlink socket */
#define NBL_RECV_BUF_SIZE 32768

struct nbl_userdev_map_record {
	TAILQ_ENTRY(nbl_userdev_map_record) next;
	u64 vaddr;
	u64 iova;
	u64 len;
};

static int nbl_default_container = -1;
static int nbl_group_count;

TAILQ_HEAD(nbl_adapter_list_head, nbl_adapter);
static struct nbl_adapter_list_head nbl_adapter_list =
	TAILQ_HEAD_INITIALIZER(nbl_adapter_list);

TAILQ_HEAD(nbl_userdev_map_record_head, nbl_userdev_map_record);
static struct nbl_userdev_map_record_head nbl_map_list =
	TAILQ_HEAD_INITIALIZER(nbl_map_list);

static int
nbl_userdev_dma_mem_map(int devfd, uint64_t vaddr, uint64_t iova, uint64_t len)
{
	struct nbl_dev_user_dma_map dma_map;
	int ret = 0;

	memset(&dma_map, 0, sizeof(dma_map));
	dma_map.argsz = sizeof(struct nbl_dev_user_dma_map);
	dma_map.vaddr = vaddr;
	dma_map.size = len;
	dma_map.iova = iova;
	dma_map.flags = NBL_DEV_USER_DMA_MAP_FLAG_READ |
			NBL_DEV_USER_DMA_MAP_FLAG_WRITE;

	ret = ioctl(devfd, NBL_DEV_USER_MAP_DMA, &dma_map);
	if (ret) {
		/**
		 * In case the mapping was already done EEXIST will be
		 * returned from kernel.
		 */
		if (errno == EEXIST) {
			NBL_LOG(ERR,
				"nbl container Memory segment is already mapped, skipping");
			ret = 0;
		} else {
			NBL_LOG(ERR,
				"nbl container cannot set up DMA remapping, error %i (%s), ret %d",
				errno, strerror(errno), ret);
		}
	}

	return ret;
}

static int
nbl_vfio_dma_mem_map(int vfio_container_fd, uint64_t vaddr, uint64_t len, int do_map)
{
	struct vfio_iommu_type1_dma_map dma_map;
	struct vfio_iommu_type1_dma_unmap dma_unmap;
	int ret;

	if (do_map != 0) {
		memset(&dma_map, 0, sizeof(dma_map));
		dma_map.argsz = sizeof(struct vfio_iommu_type1_dma_map);
		dma_map.vaddr = vaddr;
		dma_map.size = len;
		dma_map.iova = vaddr;
		dma_map.flags = VFIO_DMA_MAP_FLAG_READ |
				VFIO_DMA_MAP_FLAG_WRITE;

		ret = ioctl(vfio_container_fd, VFIO_IOMMU_MAP_DMA, &dma_map);
		if (ret) {
			/**
			 * In case the mapping was already done EEXIST will be
			 * returned from kernel.
			 */
			if (errno == EEXIST) {
				NBL_LOG(ERR,
					"Memory segment is already mapped, skipping");
			} else {
				NBL_LOG(ERR,
					"cannot set up DMA remapping, error %i (%s)",
					errno, strerror(errno));
				return -1;
			}
		}
	} else {
		memset(&dma_unmap, 0, sizeof(dma_unmap));
		dma_unmap.argsz = sizeof(struct vfio_iommu_type1_dma_unmap);
		dma_unmap.size = len;
		dma_unmap.iova = vaddr;

		ret = ioctl(vfio_container_fd, VFIO_IOMMU_UNMAP_DMA, &dma_unmap);
		if (ret) {
			NBL_LOG(ERR, "cannot clear DMA remapping, error %i (%s)",
				errno, strerror(errno));
			return -1;
		}
	}

	return 0;
}

static int
vfio_map_contig(const struct rte_memseg_list *msl, const struct rte_memseg *ms,
		size_t len, void *arg)
{
	struct nbl_userdev_map_record *record;
	int *vfio_container_fd = arg;
	int ret;

	if (msl->external)
		return 0;

	ret = nbl_vfio_dma_mem_map(*vfio_container_fd, ms->addr_64, len, 1);
	if (ret)
		goto vfio_dma_mem_map_fail;

	record = malloc(sizeof(*record));
	if (!record) {
		ret = -ENOMEM;
		goto malloc_fail;
	}

	record->vaddr = ms->addr_64;
	record->iova = ms->iova;
	record->len = len;
	TAILQ_INSERT_TAIL(&nbl_map_list, record, next);

	return 0;

malloc_fail:
	nbl_vfio_dma_mem_map(*vfio_container_fd, ms->addr_64, len, 0);
vfio_dma_mem_map_fail:
	return ret;
}

static int
vfio_map(const struct rte_memseg_list *msl, const struct rte_memseg *ms, void *arg)
{
	struct nbl_userdev_map_record *record;
	int *vfio_container_fd = arg;
	int ret;

	/* skip external memory that isn't a heap */
	if (msl->external && !msl->heap)
		return 0;

	/* skip any segments with invalid IOVA addresses */
	if (ms->iova == RTE_BAD_IOVA)
		return 0;

	/* if IOVA mode is VA, we've already mapped the internal segments */
	if (!msl->external && rte_eal_iova_mode() == RTE_IOVA_VA)
		return 0;

	ret = nbl_vfio_dma_mem_map(*vfio_container_fd, ms->addr_64, ms->len, 1);
	if (ret)
		goto vfio_dma_mem_map_fail;

	record = malloc(sizeof(*record));
	if (!record) {
		ret = -ENOMEM;
		goto malloc_fail;
	}

	record->vaddr = ms->addr_64;
	record->iova = ms->iova;
	record->len = ms->len;
	TAILQ_INSERT_TAIL(&nbl_map_list, record, next);

	return 0;

malloc_fail:
	nbl_vfio_dma_mem_map(*vfio_container_fd, ms->addr_64, ms->len, 0);
vfio_dma_mem_map_fail:
	return ret;
}

static int nbl_userdev_vfio_dma_map(int vfio_container_fd)
{
	if (rte_eal_iova_mode() == RTE_IOVA_VA) {
		/**
		 * with IOVA as VA mode, we can get away with mapping contiguous
		 * chunks rather than going page-by-page.
		 */
		int ret = rte_memseg_contig_walk(vfio_map_contig,
				&vfio_container_fd);
		if (ret)
			return ret;
		/**
		 * we have to continue the walk because we've skipped the
		 * external segments during the config walk.
		 */
	}
	return rte_memseg_walk(vfio_map, &vfio_container_fd);
}

static int nbl_userdev_dma_map(struct nbl_adapter *adapter)
{
	struct nbl_common_info *common = &adapter->common;
	struct nbl_userdev_map_record *record;
	rte_iova_t iova;

	rte_mcfg_mem_read_lock();
	TAILQ_FOREACH(record, &nbl_map_list, next) {
		iova = record->iova;
		if (common->dma_set_msb)
			iova |= (1UL << common->dma_limit_msb);
		nbl_userdev_dma_mem_map(common->devfd, record->vaddr, iova, record->len);
	}
	TAILQ_INSERT_TAIL(&nbl_adapter_list, adapter, next);
	rte_mcfg_mem_read_unlock();

	return 0;
}

static void *nbl_userdev_mmap(int devfd, __rte_unused int bar_index, size_t size)
{
	void *addr;

	addr = rte_mem_map(NULL, size, RTE_PROT_READ | RTE_PROT_WRITE,
			   RTE_MAP_SHARED, devfd, 0);
	if (!addr)
		NBL_LOG(ERR, "usermap mmap bar failed");

	return addr;
}

static int nbl_userdev_add_record(u64 vaddr, u64 iova, u64 len)
{
	struct nbl_userdev_map_record *record;
	struct nbl_adapter *adapter;
	u64 dma_iova;
	int ret;

	record = malloc(sizeof(*record));
	if (!record)
		return -ENOMEM;

	ret = nbl_vfio_dma_mem_map(nbl_default_container, vaddr, len, 1);
	if (ret) {
		free(record);
		return ret;
	}

	record->iova = iova;
	record->len = len;
	record->vaddr = vaddr;

	TAILQ_INSERT_TAIL(&nbl_map_list, record, next);
	TAILQ_FOREACH(adapter, &nbl_adapter_list, next) {
		dma_iova = record->iova;
		if (adapter->common.dma_set_msb)
			dma_iova |= (1UL << adapter->common.dma_limit_msb);
		nbl_userdev_dma_mem_map(adapter->common.devfd, record->vaddr,
					dma_iova, record->len);
	}

	return 0;
}

static int nbl_userdev_free_record(u64 vaddr, u64 iova __rte_unused, u64 len __rte_unused)
{
	struct nbl_userdev_map_record *record, *tmp_record;

	RTE_TAILQ_FOREACH_SAFE(record, &nbl_map_list, next, tmp_record) {
		if (record->vaddr != vaddr)
			continue;
		nbl_vfio_dma_mem_map(nbl_default_container, vaddr, record->len, 0);
		TAILQ_REMOVE(&nbl_map_list, record, next);
		free(record);
	}

	return 0;
}

static void nbl_userdev_dma_free(void)
{
	struct nbl_userdev_map_record *record, *tmp_record;

	RTE_TAILQ_FOREACH_SAFE(record, &nbl_map_list, next, tmp_record) {
		TAILQ_REMOVE(&nbl_map_list, record, next);
		free(record);
	}
}

static void
nbl_userdev_mem_event_callback(enum rte_mem_event type, const void *addr, size_t len,
			       void *arg __rte_unused)
{
	rte_iova_t iova_start, iova_expected;
	struct rte_memseg_list *msl;
	struct rte_memseg *ms;
	size_t cur_len = 0;
	u64 va_start;
	u64 vfio_va;

	if (!nbl_group_count)
		return;

	msl = rte_mem_virt2memseg_list(addr);

	/* for IOVA as VA mode, no need to care for IOVA addresses */
	if (rte_eal_iova_mode() == RTE_IOVA_VA && msl->external == 0) {
		vfio_va = (u64)(uintptr_t)addr;
		if (type == RTE_MEM_EVENT_ALLOC)
			nbl_userdev_add_record(vfio_va, vfio_va, len);
		else
			nbl_userdev_free_record(vfio_va, vfio_va, len);
		return;
	}

	/* memsegs are contiguous in memory */
	ms = rte_mem_virt2memseg(addr, msl);

	/**
	 * This memory is not guaranteed to be contiguous, but it still could
	 * be, or it could have some small contiguous chunks. Since the number
	 * of VFIO mappings is limited, and VFIO appears to not concatenate
	 * adjacent mappings, we have to do this ourselves.
	 * So, find contiguous chunks, then map them.
	 */
	va_start = ms->addr_64;
	iova_start = ms->iova;
	iova_expected = ms->iova;
	while (cur_len < len) {
		bool new_contig_area = ms->iova != iova_expected;
		bool last_seg = (len - cur_len) == ms->len;
		bool skip_last = false;

		/* only do mappings when current contiguous area ends */
		if (new_contig_area) {
			if (type == RTE_MEM_EVENT_ALLOC)
				nbl_userdev_add_record(va_start, iova_start,
						       iova_expected - iova_start);
			else
				nbl_userdev_free_record(va_start, iova_start,
							iova_expected - iova_start);
			va_start = ms->addr_64;
			iova_start = ms->iova;
		}
		/* some memory segments may have invalid IOVA */
		if (ms->iova == RTE_BAD_IOVA) {
			NBL_LOG(DEBUG, "Memory segment at %p has bad IOVA, skipping",
				ms->addr);
			skip_last = true;
		}
		iova_expected = ms->iova + ms->len;
		cur_len += ms->len;
		++ms;

		/**
		 * don't count previous segment, and don't attempt to
		 * dereference a potentially invalid pointer.
		 */
		if (skip_last && !last_seg) {
			iova_expected = ms->iova;
			iova_start = ms->iova;
			va_start = ms->addr_64;
		} else if (!skip_last && last_seg) {
			/* this is the last segment and we're not skipping */
			if (type == RTE_MEM_EVENT_ALLOC)
				nbl_userdev_add_record(va_start, iova_start,
						       iova_expected - iova_start);
			else
				nbl_userdev_free_record(va_start, iova_start,
							iova_expected - iova_start);
		}
	}
}

static int nbl_mdev_map_device(struct nbl_adapter *adapter)
{
	const struct rte_pci_device *pci_dev = adapter->pci_dev;
	struct nbl_common_info *common = &adapter->common;
	char dev_name[RTE_DEV_NAME_MAX_LEN] = {0};
	char pathname[PATH_MAX];
	struct vfio_device_info device_info = { .argsz = sizeof(device_info) };
	struct vfio_group_status group_status = {
			.argsz = sizeof(group_status)
	};
	u64 dma_limit = NBL_USERDEV_DMA_LIMIT;
	int ret, container_create = 0, container_set = 0;
	int vfio_group_fd, container = nbl_default_container;

	rte_pci_device_name(&pci_dev->addr, dev_name, RTE_DEV_NAME_MAX_LEN);
	snprintf(pathname, sizeof(pathname),
		 "%s/%s/", rte_pci_get_sysfs_path(), dev_name);

	ret = rte_vfio_get_group_num(pathname, dev_name, &common->iommu_group_num);
	if (ret <= 0) {
		NBL_LOG(ERR, "nbl vfio group number failed");
		return -1;
	}

	NBL_LOG(DEBUG, "nbl vfio group number %d", common->iommu_group_num);
	/* vfio_container */
	if (nbl_default_container < 0) {
		container = rte_vfio_container_create();
		container_create = 1;

		if (container < 0) {
			NBL_LOG(ERR, "nbl vfio container create failed");
			return -1;
		}
	}

	NBL_LOG(DEBUG, "nbl vfio container %d", container);
	vfio_group_fd = rte_vfio_container_group_bind(container, common->iommu_group_num);
	if (vfio_group_fd < 0) {
		NBL_LOG(ERR, "nbl vfio group bind failed, %d", vfio_group_fd);
		goto free_container;
	}

	/* check if the group is viable */
	ret = ioctl(vfio_group_fd, VFIO_GROUP_GET_STATUS, &group_status);
	if (ret) {
		NBL_LOG(ERR, "%s cannot get group status,error %i (%s)", dev_name,
			errno, strerror(errno));
		goto free_group;
	} else if (!(group_status.flags & VFIO_GROUP_FLAGS_VIABLE)) {
		NBL_LOG(ERR, "%s VFIO group is not viable!", dev_name);
		goto free_group;
	}

	if (!(group_status.flags & VFIO_GROUP_FLAGS_CONTAINER_SET)) {
		/* add group to a container */
		ret = ioctl(vfio_group_fd, VFIO_GROUP_SET_CONTAINER, &container);
		if (ret) {
			NBL_LOG(ERR, "%s cannot add VFIO group to container, error %i (%s)",
				dev_name, errno, strerror(errno));
			goto free_group;
		}

		nbl_group_count++;
		container_set = 1;
		/* set an IOMMU type for container */

		if (container_create || nbl_group_count == 1) {
			if (ioctl(container, VFIO_CHECK_EXTENSION, VFIO_TYPE1v2_IOMMU)) {
				ret = ioctl(container, VFIO_SET_IOMMU, VFIO_TYPE1v2_IOMMU);
				if (ret) {
					NBL_LOG(ERR, "Failed to setup VFIO iommu");
					goto unset_container;
				}
			} else {
				NBL_LOG(ERR, "No supported IOMMU available");
				goto unset_container;
			}

			rte_mcfg_mem_read_lock();
			ret = nbl_userdev_vfio_dma_map(container);
			if (ret) {
				rte_mcfg_mem_read_unlock();
				NBL_LOG(WARNING, "nbl vfio container dma map failed, %d", ret);
				goto free_dma_map;
			}
			ret = rte_mem_event_callback_register(NBL_USERDEV_EVENT_CLB_NAME,
							      nbl_userdev_mem_event_callback, NULL);
			rte_mcfg_mem_read_unlock();
			if (ret && rte_errno != ENOTSUP) {
				NBL_LOG(WARNING, "nbl vfio mem event register callback failed, %d",
					ret);
				goto free_dma_map;
			}
		}
	}

	/* get a file descriptor for the device */
	common->devfd = ioctl(vfio_group_fd, VFIO_GROUP_GET_DEVICE_FD, dev_name);
	if (common->devfd < 0) {
		/* if we cannot get a device fd, this implies a problem with
		 * the VFIO group or the container not having IOMMU configured.
		 */

		NBL_LOG(WARNING, "Getting a vfio_dev_fd for %s failed, %d",
			dev_name, common->devfd);
		goto unregister_mem_event;
	}

	if (container_create)
		nbl_default_container = container;

	common->specific_dma = true;
	if (rte_eal_iova_mode() == RTE_IOVA_PA)
		common->dma_set_msb = true;
	ioctl(common->devfd, NBL_DEV_USER_GET_DMA_LIMIT, &dma_limit);
	common->dma_limit_msb = rte_fls_u64(dma_limit) - 1;
	if (common->dma_limit_msb < 38) {
		NBL_LOG(ERR, "iommu dma limit msb %u, low 3-level page table",
			common->dma_limit_msb);
		goto close_fd;
	}

	nbl_userdev_dma_map(adapter);

	return 0;

close_fd:
	close(common->devfd);
unregister_mem_event:
	if (nbl_group_count == 1) {
		rte_mcfg_mem_read_lock();
		rte_mem_event_callback_unregister(NBL_USERDEV_EVENT_CLB_NAME, NULL);
		rte_mcfg_mem_read_unlock();
	}
free_dma_map:
	if (nbl_group_count == 1) {
		rte_mcfg_mem_read_lock();
		nbl_userdev_dma_free();
		rte_mcfg_mem_read_unlock();
	}
unset_container:
	if (container_set) {
		ioctl(vfio_group_fd, VFIO_GROUP_UNSET_CONTAINER, &container);
		nbl_group_count--;
	}
free_group:
	close(vfio_group_fd);
	rte_vfio_clear_group(vfio_group_fd);
free_container:
	if (container_create)
		rte_vfio_container_destroy(container);
	return -1;
}

static int nbl_mdev_unmap_device(struct nbl_adapter *adapter)
{
	struct nbl_common_info *common = &adapter->common;
	int vfio_group_fd, ret;

	close(common->devfd);
	rte_mcfg_mem_read_lock();
	vfio_group_fd = rte_vfio_container_group_bind(nbl_default_container,
						      common->iommu_group_num);
	NBL_LOG(DEBUG, "close vfio_group_fd %d", vfio_group_fd);
	ret = ioctl(vfio_group_fd, VFIO_GROUP_UNSET_CONTAINER, &nbl_default_container);
	if (ret)
		NBL_LOG(ERR, "unset container, error %i (%s) %d",
			errno, strerror(errno), ret);
	nbl_group_count--;
	ret = rte_vfio_container_group_unbind(nbl_default_container, common->iommu_group_num);
	if (ret)
		NBL_LOG(ERR, "vfio container group unbind failed %d", ret);
	if (!nbl_group_count) {
		rte_mem_event_callback_unregister(NBL_USERDEV_EVENT_CLB_NAME, NULL);
		nbl_userdev_dma_free();
	}
	rte_mcfg_mem_read_unlock();

	return 0;
}

static int nbl_userdev_get_ifindex(int devfd)
{
	int ifindex = -1, ret;

	ret = ioctl(devfd, NBL_DEV_USER_GET_IFINDEX, &ifindex);
	if (ret)
		NBL_LOG(ERR, "get ifindex failed %d", ret);

	NBL_LOG(DEBUG, "get ifindex %d", ifindex);

	return ifindex;
}

static int nbl_userdev_nl_init(int protocol)
{
	int fd;
	int sndbuf_size = NBL_SEND_BUF_SIZE;
	int rcvbuf_size = NBL_RECV_BUF_SIZE;
	struct sockaddr_nl local = {
		.nl_family = AF_NETLINK,
	};
	int ret;

	fd = socket(AF_NETLINK, SOCK_RAW | SOCK_CLOEXEC, protocol);
	if (fd == -1) {
		rte_errno = errno;
		return -rte_errno;
	}
	ret = setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &sndbuf_size, sizeof(int));
	if (ret == -1) {
		rte_errno = errno;
		goto error;
	}
	ret = setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf_size, sizeof(int));
	if (ret == -1) {
		rte_errno = errno;
		goto error;
	}
	ret = bind(fd, (struct sockaddr *)&local, sizeof(local));
	if (ret == -1) {
		rte_errno = errno;
		goto error;
	}
	return fd;
error:
	close(fd);
	return -rte_errno;
}

int nbl_userdev_port_config(struct nbl_adapter *adapter, int start)
{
	struct nbl_common_info *common = &adapter->common;
	int ret;

	if (NBL_IS_NOT_COEXISTENCE(common))
		return 0;

	if (common->isolate)
		return 0;

	ret = ioctl(common->devfd, NBL_DEV_USER_SWITCH_NETWORK, &start);
	if (ret) {
		NBL_LOG(ERR, "userspace switch network ret %d", ret);
		return ret;
	}

	common->curr_network = start;
	return ret;
}

int nbl_userdev_port_isolate(struct nbl_adapter *adapter, int set, struct rte_flow_error *error)
{
	struct nbl_common_info *common = &adapter->common;
	int ret = 0, stat = !set;

	if (NBL_IS_NOT_COEXISTENCE(common)) {
		/* special use for isolate: offload mode ignore isolate when pf is in vfio/uio */
		rte_flow_error_set(error, EREMOTEIO,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "nbl isolate switch failed.");
		return -EREMOTEIO;
	}

	if (common->curr_network != stat)
		ret = ioctl(common->devfd, NBL_DEV_USER_SWITCH_NETWORK, &stat);

	if (ret) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "nbl isolate switch failed.");
		return ret;
	}

	common->curr_network = !set;
	common->isolate = set;

	return ret;
}

int nbl_pci_map_device(struct nbl_adapter *adapter)
{
	struct rte_pci_device *pci_dev = adapter->pci_dev;
	const struct rte_pci_addr *loc = &pci_dev->addr;
	struct nbl_common_info *common = &adapter->common;
	char pathname[PATH_MAX];
	int ret = 0, fd;
	enum rte_iova_mode iova_mode;
	size_t bar_size = NBL_USERDEV_BAR0_SIZE;

	NBL_USERDEV_INIT_COMMON(common);
	iova_mode = rte_eal_iova_mode();
	if (iova_mode == RTE_IOVA_PA) {
		/* check iommu disable */
		snprintf(pathname, sizeof(pathname),
			 "/dev/nbl_userdev/" PCI_PRI_FMT, loc->domain,
			 loc->bus, loc->devid, loc->function);
		common->devfd = open(pathname, O_RDWR);
		if (common->devfd >= 0)
			goto mmap;

		NBL_LOG(ERR, "%s char device open failed", pci_dev->device.name);
	}

	/* check iommu translate mode */
	ret = nbl_mdev_map_device(adapter);
	if (ret) {
		ret = rte_pci_map_device(pci_dev);
		if (ret)
			NBL_LOG(ERR, "uio/vfio %s map device failed", pci_dev->device.name);
		return ret;
	}

mmap:
	fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
	if (fd < 0) {
		NBL_LOG(ERR, "nbl userdev get event fd failed");
		ret = -1;
		goto close_fd;
	}

	ret = ioctl(common->devfd, NBL_DEV_USER_SET_EVENTFD, &fd);
	if (ret) {
		NBL_LOG(ERR, "nbl userdev set event fd failed");
		goto close_eventfd;
	}

	common->eventfd = fd;
	ioctl(common->devfd, NBL_DEV_USER_GET_BAR_SIZE, &bar_size);

	if (!ret) {
		pci_dev->mem_resource[0].addr = nbl_userdev_mmap(common->devfd, 0, bar_size);
		pci_dev->mem_resource[0].phys_addr = 0;
		pci_dev->mem_resource[0].len = bar_size;
		pci_dev->mem_resource[2].addr = 0;

		common->ifindex = nbl_userdev_get_ifindex(common->devfd);
		common->nl_socket_route = nbl_userdev_nl_init(NETLINK_ROUTE);
	}

	return ret;

close_eventfd:
	close(fd);
close_fd:
	if (common->specific_dma)
		nbl_mdev_unmap_device(adapter);
	else
		close(common->devfd);

	return ret;
}

void nbl_pci_unmap_device(struct nbl_adapter *adapter)
{
	struct rte_pci_device *pci_dev = adapter->pci_dev;
	struct nbl_common_info *common = &adapter->common;

	if (NBL_IS_NOT_COEXISTENCE(common))
		return rte_pci_unmap_device(pci_dev);

	rte_mem_unmap(pci_dev->mem_resource[0].addr, pci_dev->mem_resource[0].len);
	ioctl(common->devfd, NBL_DEV_USER_CLEAR_EVENTFD, 0);
	close(common->eventfd);
	close(common->nl_socket_route);

	if (!common->specific_dma) {
		close(common->devfd);
		return;
	}

	nbl_mdev_unmap_device(adapter);
}

static int nbl_userdev_ifreq(int ifindex, int req, struct ifreq *ifr)
{
	int sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
	int ret = 0;

	if (sock == -1) {
		rte_errno = errno;
		return -rte_errno;
	}

	if (!if_indextoname(ifindex, &ifr->ifr_name[0]))
		goto error;

	ret = ioctl(sock, req, ifr);
	if (ret == -1) {
		rte_errno = errno;
		goto error;
	}
	close(sock);
	return 0;
error:
	close(sock);
	return -rte_errno;
}

int nbl_userdev_get_mac_addr(struct nbl_common_info *common, u8 *mac)
{
	struct ifreq request;
	int ret;

	ret = nbl_userdev_ifreq(common->ifindex, SIOCGIFHWADDR, &request);
	if (ret) {
		NBL_LOG(ERR, "userdev get mac failed: %d", ret);
		return ret;
	}

	memcpy(mac, request.ifr_hwaddr.sa_data, RTE_ETHER_ADDR_LEN);
	return 0;
}
