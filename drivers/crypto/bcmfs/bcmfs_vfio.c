/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Broadcom.
 * All rights reserved.
 */

#include <errno.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <rte_vfio.h>

#include "bcmfs_device.h"
#include "bcmfs_logs.h"
#include "bcmfs_vfio.h"

#ifdef VFIO_PRESENT
static int
vfio_map_dev_obj(const char *path, const char *dev_obj,
		 uint32_t *size, void **addr, int *dev_fd)
{
	int32_t ret;
	struct vfio_group_status status = { .argsz = sizeof(status) };

	struct vfio_device_info d_info = { .argsz = sizeof(d_info) };
	struct vfio_region_info reg_info = { .argsz = sizeof(reg_info) };

	ret = rte_vfio_setup_device(path, dev_obj, dev_fd, &d_info);
	if (ret) {
		BCMFS_LOG(ERR, "VFIO Setting for device failed");
		return ret;
	}

	/* getting device region info*/
	ret = ioctl(*dev_fd, VFIO_DEVICE_GET_REGION_INFO, &reg_info);
	if (ret < 0) {
		BCMFS_LOG(ERR, "Error in VFIO getting REGION_INFO");
		goto map_failed;
	}

	*addr = mmap(NULL, reg_info.size,
		     PROT_WRITE | PROT_READ, MAP_SHARED,
		     *dev_fd, reg_info.offset);
	if (*addr == MAP_FAILED) {
		BCMFS_LOG(ERR, "Error mapping region (errno = %d)", errno);
		ret = errno;
		goto map_failed;
	}
	*size = reg_info.size;

	return 0;

map_failed:
	rte_vfio_release_device(path, dev_obj, *dev_fd);

	return ret;
}

int
bcmfs_attach_vfio(struct bcmfs_device *dev)
{
	int ret;
	int vfio_dev_fd;
	void  *v_addr = NULL;
	uint32_t size = 0;

	ret = vfio_map_dev_obj(dev->dirname, dev->name,
			       &size, &v_addr, &vfio_dev_fd);
	if (ret)
		return -1;

	dev->mmap_size = size;
	dev->mmap_addr = v_addr;
	dev->vfio_dev_fd = vfio_dev_fd;

	return 0;
}

void
bcmfs_release_vfio(struct bcmfs_device *dev)
{
	int ret;

	if (dev == NULL)
		return;

	/* unmap the addr */
	munmap(dev->mmap_addr, dev->mmap_size);
	/* release the device */
	ret = rte_vfio_release_device(dev->dirname, dev->name,
				      dev->vfio_dev_fd);
	if (ret < 0) {
		BCMFS_LOG(ERR, "cannot release device");
		return;
	}
}
#else
int
bcmfs_attach_vfio(struct bcmfs_device *dev __rte_unused)
{
	return -1;
}

void
bcmfs_release_vfio(struct bcmfs_device *dev __rte_unused)
{
}
#endif
