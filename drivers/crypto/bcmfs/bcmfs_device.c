/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Broadcom.
 * All rights reserved.
 */

#include <dirent.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/queue.h>

#include <rte_malloc.h>
#include <rte_string_fns.h>

#include "bcmfs_device.h"
#include "bcmfs_logs.h"
#include "bcmfs_qp.h"
#include "bcmfs_vfio.h"
#include "bcmfs_sym_pmd.h"

struct bcmfs_device_attr {
	const char name[BCMFS_MAX_PATH_LEN];
	const char suffix[BCMFS_DEV_NAME_LEN];
	const enum bcmfs_device_type type;
	const uint32_t offset;
	const uint32_t version;
};

/* BCMFS supported devices */
static struct bcmfs_device_attr dev_table[] = {
	{
		.name = "fs4",
		.suffix = "crypto_mbox",
		.type = BCMFS_SYM_FS4,
		.offset = 0,
		.version = BCMFS_SYM_FS4_VERSION
	},
	{
		.name = "fs5",
		.suffix = "mbox",
		.type = BCMFS_SYM_FS5,
		.offset = 0,
		.version = BCMFS_SYM_FS5_VERSION
	},
	{
		/* sentinel */
	}
};

struct bcmfs_hw_queue_pair_ops_table bcmfs_hw_queue_pair_ops_table = {
	.tl =  RTE_SPINLOCK_INITIALIZER,
	.num_ops = 0
};

int bcmfs_hw_queue_pair_register_ops(const struct bcmfs_hw_queue_pair_ops *h)
{
	struct bcmfs_hw_queue_pair_ops *ops;
	int16_t ops_index;

	rte_spinlock_lock(&bcmfs_hw_queue_pair_ops_table.tl);

	if (h->enq_one_req == NULL || h->dequeue == NULL ||
	    h->ring_db == NULL || h->startq == NULL || h->stopq == NULL) {
		rte_spinlock_unlock(&bcmfs_hw_queue_pair_ops_table.tl);
		BCMFS_LOG(ERR,
			  "Missing callback while registering device ops");
		return -EINVAL;
	}

	if (strlen(h->name) >= sizeof(ops->name) - 1) {
		rte_spinlock_unlock(&bcmfs_hw_queue_pair_ops_table.tl);
		BCMFS_LOG(ERR, "%s(): fs device_ops <%s>: name too long",
				__func__, h->name);
		return -EEXIST;
	}

	ops_index = bcmfs_hw_queue_pair_ops_table.num_ops++;
	ops = &bcmfs_hw_queue_pair_ops_table.qp_ops[ops_index];
	strlcpy(ops->name, h->name, sizeof(ops->name));
	ops->enq_one_req = h->enq_one_req;
	ops->dequeue = h->dequeue;
	ops->ring_db = h->ring_db;
	ops->startq = h->startq;
	ops->stopq = h->stopq;

	rte_spinlock_unlock(&bcmfs_hw_queue_pair_ops_table.tl);

	return ops_index;
}

TAILQ_HEAD(fsdev_list, bcmfs_device);
static struct fsdev_list fsdev_list = TAILQ_HEAD_INITIALIZER(fsdev_list);

static struct bcmfs_device *
fsdev_allocate_one_dev(struct rte_vdev_device *vdev,
		       char *dirpath,
		       char *devname,
		       enum bcmfs_device_type dev_type __rte_unused)
{
	struct bcmfs_device *fsdev;
	uint32_t i;

	fsdev = rte_calloc(__func__, 1, sizeof(*fsdev), 0);
	if (!fsdev)
		return NULL;

	if (strlen(dirpath) > sizeof(fsdev->dirname)) {
		BCMFS_LOG(ERR, "dir path name is too long");
		goto cleanup;
	}

	if (strlen(devname) > sizeof(fsdev->name)) {
		BCMFS_LOG(ERR, "devname is too long");
		goto cleanup;
	}

	/* check if registered ops name is present in directory path */
	for (i = 0; i < bcmfs_hw_queue_pair_ops_table.num_ops; i++)
		if (strstr(dirpath,
			   bcmfs_hw_queue_pair_ops_table.qp_ops[i].name))
			fsdev->sym_hw_qp_ops =
				&bcmfs_hw_queue_pair_ops_table.qp_ops[i];
	if (!fsdev->sym_hw_qp_ops)
		goto cleanup;

	strcpy(fsdev->dirname, dirpath);
	strcpy(fsdev->name, devname);

	fsdev->vdev = vdev;

	/* attach to VFIO */
	if (bcmfs_attach_vfio(fsdev))
		goto cleanup;

	/* Maximum number of QPs supported */
	fsdev->max_hw_qps = fsdev->mmap_size / BCMFS_HW_QUEUE_IO_ADDR_LEN;

	TAILQ_INSERT_TAIL(&fsdev_list, fsdev, next);

	return fsdev;

cleanup:
	free(fsdev);

	return NULL;
}

static struct bcmfs_device *
find_fsdev(struct rte_vdev_device *vdev)
{
	struct bcmfs_device *fsdev;

	TAILQ_FOREACH(fsdev, &fsdev_list, next)
		if (fsdev->vdev == vdev)
			return fsdev;

	return NULL;
}

static void
fsdev_release(struct bcmfs_device *fsdev)
{
	if (fsdev == NULL)
		return;

	TAILQ_REMOVE(&fsdev_list, fsdev, next);
	free(fsdev);
}

static int
cmprator(const void *a, const void *b)
{
	return (*(const unsigned int *)a - *(const unsigned int *)b);
}

static int
fsdev_find_all_devs(const char *path, const char *search,
		    uint32_t *devs)
{
	DIR *dir;
	struct dirent *entry;
	int count = 0;
	char addr[BCMFS_MAX_NODES][BCMFS_MAX_PATH_LEN];
	int i;

	dir = opendir(path);
	if (dir == NULL) {
		BCMFS_LOG(ERR, "Unable to open directory");
		return 0;
	}

	while ((entry = readdir(dir)) != NULL) {
		if (strstr(entry->d_name, search)) {
			strlcpy(addr[count], entry->d_name,
				BCMFS_MAX_PATH_LEN);
			count++;
		}
	}

	closedir(dir);

	for (i = 0 ; i < count; i++)
		devs[i] = (uint32_t)strtoul(addr[i], NULL, 16);
	/* sort the devices based on IO addresses */
	qsort(devs, count, sizeof(uint32_t), cmprator);

	return count;
}

static bool
fsdev_find_sub_dir(char *path, const char *search, char *output)
{
	DIR *dir;
	struct dirent *entry;

	dir = opendir(path);
	if (dir == NULL) {
		BCMFS_LOG(ERR, "Unable to open directory");
		return -ENODEV;
	}

	while ((entry = readdir(dir)) != NULL) {
		if (!strcmp(entry->d_name, search)) {
			strlcpy(output, entry->d_name, BCMFS_MAX_PATH_LEN);
			closedir(dir);
			return true;
		}
	}

	closedir(dir);

	return false;
}


static int
bcmfs_vdev_probe(struct rte_vdev_device *vdev)
{
	struct bcmfs_device *fsdev;
	char top_dirpath[BCMFS_MAX_PATH_LEN];
	char sub_dirpath[BCMFS_MAX_PATH_LEN];
	char out_dirpath[BCMFS_MAX_PATH_LEN];
	char out_dirname[BCMFS_MAX_PATH_LEN];
	uint32_t fsdev_dev[BCMFS_MAX_NODES];
	enum bcmfs_device_type dtype;
	int err;
	int i = 0;
	int dev_idx;
	int count = 0;
	bool found = false;

	sprintf(top_dirpath, "%s", SYSFS_BCM_PLTFORM_DEVICES);
	while (strlen(dev_table[i].name)) {
		found = fsdev_find_sub_dir(top_dirpath,
					   dev_table[i].name,
					   sub_dirpath);
		if (found)
			break;
		i++;
	}
	if (!found) {
		BCMFS_LOG(ERR, "No supported bcmfs dev found");
		return -ENODEV;
	}

	dev_idx = i;
	dtype = dev_table[i].type;

	snprintf(out_dirpath, sizeof(out_dirpath), "%s/%s",
		 top_dirpath, sub_dirpath);
	count = fsdev_find_all_devs(out_dirpath,
				    dev_table[dev_idx].suffix,
				    fsdev_dev);
	if (!count) {
		BCMFS_LOG(ERR, "No supported bcmfs dev found");
		return -ENODEV;
	}

	i = 0;
	while (count) {
		/* format the device name present in the patch */
		snprintf(out_dirname, sizeof(out_dirname), "%x.%s",
			 fsdev_dev[i], dev_table[dev_idx].suffix);
		fsdev = fsdev_allocate_one_dev(vdev, out_dirpath,
					       out_dirname, dtype);
		if (!fsdev) {
			count--;
			i++;
			continue;
		}
		break;
	}
	if (fsdev == NULL) {
		BCMFS_LOG(ERR, "All supported devs busy");
		return -ENODEV;
	}

	err = bcmfs_sym_dev_create(fsdev);
	if (err) {
		BCMFS_LOG(WARNING,
			  "Failed to create BCMFS SYM PMD for device %s",
			  fsdev->name);
		goto pmd_create_fail;
	}

	return 0;

pmd_create_fail:
	fsdev_release(fsdev);

	return err;
}

static int
bcmfs_vdev_remove(struct rte_vdev_device *vdev)
{
	struct bcmfs_device *fsdev;

	fsdev = find_fsdev(vdev);
	if (fsdev == NULL)
		return -ENODEV;

	fsdev_release(fsdev);
	return 0;
}

/* Register with vdev */
static struct rte_vdev_driver rte_bcmfs_pmd = {
	.probe = bcmfs_vdev_probe,
	.remove = bcmfs_vdev_remove
};

RTE_PMD_REGISTER_VDEV(bcmfs_pmd,
		      rte_bcmfs_pmd);
