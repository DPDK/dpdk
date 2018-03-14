/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2017 Intel Corporation
 */

#include <stdint.h>
#include <unistd.h>
#include <stdbool.h>
#include <signal.h>
#include <assert.h>
#include <semaphore.h>
#include <linux/virtio_scsi.h>
#include <linux/virtio_ring.h>

#include <rte_atomic.h>
#include <rte_cycles.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_vhost.h>

#include "vhost_scsi.h"
#include "scsi_spec.h"

#define VIRTIO_SCSI_FEATURES ((1 << VIRTIO_F_NOTIFY_ON_EMPTY) |\
			      (1 << VIRTIO_SCSI_F_INOUT) |\
			      (1 << VIRTIO_SCSI_F_CHANGE))

/* Path to folder where character device will be created. Can be set by user. */
static char dev_pathname[PATH_MAX] = "";

static struct vhost_scsi_ctrlr *g_vhost_ctrlr;
static int g_should_stop;
static sem_t exit_sem;

static struct vhost_scsi_ctrlr *
vhost_scsi_ctrlr_find(__rte_unused const char *ctrlr_name)
{
	/* currently we only support 1 socket file fd */
	return g_vhost_ctrlr;
}

static uint64_t gpa_to_vva(int vid, uint64_t gpa, uint64_t *len)
{
	char path[PATH_MAX];
	struct vhost_scsi_ctrlr *ctrlr;
	int ret = 0;

	ret = rte_vhost_get_ifname(vid, path, PATH_MAX);
	if (ret) {
		fprintf(stderr, "Cannot get socket name\n");
		assert(ret != 0);
	}

	ctrlr = vhost_scsi_ctrlr_find(path);
	if (!ctrlr) {
		fprintf(stderr, "Controller is not ready\n");
		assert(ctrlr != NULL);
	}

	assert(ctrlr->mem != NULL);

	return rte_vhost_va_from_guest_pa(ctrlr->mem, gpa, len);
}

static struct vring_desc *
descriptor_get_next(struct vring_desc *vq_desc, struct vring_desc *cur_desc)
{
	return &vq_desc[cur_desc->next];
}

static bool
descriptor_has_next(struct vring_desc *cur_desc)
{
	return !!(cur_desc->flags & VRING_DESC_F_NEXT);
}

static bool
descriptor_is_wr(struct vring_desc *cur_desc)
{
	return !!(cur_desc->flags & VRING_DESC_F_WRITE);
}

static void
submit_completion(struct vhost_scsi_task *task, uint32_t q_idx)
{
	struct rte_vhost_vring *vq;
	struct vring_used *used;

	vq = task->vq;
	used = vq->used;
	/* Fill out the next entry in the "used" ring.  id = the
	 * index of the descriptor that contained the SCSI request.
	 * len = the total amount of data transferred for the SCSI
	 * request. We must report the correct len, for variable
	 * length SCSI CDBs, where we may return less data than
	 * allocated by the guest VM.
	 */
	used->ring[used->idx & (vq->size - 1)].id = task->req_idx;
	used->ring[used->idx & (vq->size - 1)].len = task->data_len;
	used->idx++;

	/* Send an interrupt back to the guest VM so that it knows
	 * a completion is ready to be processed.
	 */
	rte_vhost_vring_call(task->bdev->vid, q_idx);
}

static void
vhost_process_read_payload_chain(struct vhost_scsi_task *task)
{
	void *data;
	uint64_t chunck_len;

	task->iovs_cnt = 0;
	chunck_len = task->desc->len;
	task->resp = (void *)(uintptr_t)gpa_to_vva(task->bdev->vid,
						   task->desc->addr,
						   &chunck_len);
	if (!task->resp || chunck_len != task->desc->len) {
		fprintf(stderr, "failed to translate desc address.\n");
		return;
	}

	while (descriptor_has_next(task->desc)) {
		task->desc = descriptor_get_next(task->vq->desc, task->desc);
		chunck_len = task->desc->len;
		data = (void *)(uintptr_t)gpa_to_vva(task->bdev->vid,
						     task->desc->addr,
							 &chunck_len);
		if (!data || chunck_len != task->desc->len) {
			fprintf(stderr, "failed to translate desc address.\n");
			return;
		}

		task->iovs[task->iovs_cnt].iov_base = data;
		task->iovs[task->iovs_cnt].iov_len = task->desc->len;
		task->data_len += task->desc->len;
		task->iovs_cnt++;
	}
}

static void
vhost_process_write_payload_chain(struct vhost_scsi_task *task)
{
	void *data;
	uint64_t chunck_len;

	task->iovs_cnt = 0;

	do {
		chunck_len = task->desc->len;
		data = (void *)(uintptr_t)gpa_to_vva(task->bdev->vid,
						     task->desc->addr,
							 &chunck_len);
		if (!data || chunck_len != task->desc->len) {
			fprintf(stderr, "failed to translate desc address.\n");
			return;
		}

		task->iovs[task->iovs_cnt].iov_base = data;
		task->iovs[task->iovs_cnt].iov_len = task->desc->len;
		task->data_len += task->desc->len;
		task->iovs_cnt++;
		task->desc = descriptor_get_next(task->vq->desc, task->desc);
	} while (descriptor_has_next(task->desc));

	chunck_len = task->desc->len;
	task->resp = (void *)(uintptr_t)gpa_to_vva(task->bdev->vid,
						   task->desc->addr,
						   &chunck_len);
	if (!task->resp || chunck_len != task->desc->len)
		fprintf(stderr, "failed to translate desc address.\n");
}

static struct vhost_block_dev *
vhost_scsi_bdev_construct(const char *bdev_name, const char *bdev_serial,
			  uint32_t blk_size, uint64_t blk_cnt,
			  bool wce_enable)
{
	struct vhost_block_dev *bdev;

	bdev = rte_zmalloc(NULL, sizeof(*bdev), RTE_CACHE_LINE_SIZE);
	if (!bdev)
		return NULL;

	strncpy(bdev->name, bdev_name, sizeof(bdev->name));
	strncpy(bdev->product_name, bdev_serial, sizeof(bdev->product_name));
	bdev->blocklen = blk_size;
	bdev->blockcnt = blk_cnt;
	bdev->write_cache = wce_enable;

	/* use memory as disk storage space */
	bdev->data = rte_zmalloc(NULL, blk_cnt * blk_size, 0);
	if (!bdev->data) {
		fprintf(stderr, "no enough reseverd huge memory for disk\n");
		return NULL;
	}

	return bdev;
}

static void
process_requestq(struct vhost_scsi_ctrlr *ctrlr, uint32_t q_idx)
{
	int ret;
	struct vhost_scsi_queue *scsi_vq;
	struct rte_vhost_vring *vq;

	scsi_vq = &ctrlr->bdev->queues[q_idx];
	vq = &scsi_vq->vq;
	ret = rte_vhost_get_vhost_vring(ctrlr->bdev->vid, q_idx, vq);
	assert(ret == 0);

	while (vq->avail->idx != scsi_vq->last_used_idx) {
		int req_idx;
		uint16_t last_idx;
		struct vhost_scsi_task *task;
		uint64_t chunck_len;

		last_idx = scsi_vq->last_used_idx & (vq->size - 1);
		req_idx = vq->avail->ring[last_idx];

		task = rte_zmalloc(NULL, sizeof(*task), 0);
		assert(task != NULL);

		task->ctrlr = ctrlr;
		task->bdev = ctrlr->bdev;
		task->vq = vq;
		task->req_idx = req_idx;
		task->desc = &task->vq->desc[task->req_idx];

		/* does not support indirect descriptors */
		assert((task->desc->flags & VRING_DESC_F_INDIRECT) == 0);
		scsi_vq->last_used_idx++;

		chunck_len = task->desc->len;
		task->req = (void *)(uintptr_t)gpa_to_vva(task->bdev->vid,
							  task->desc->addr,
							  &chunck_len);
		if (!task->req || chunck_len != task->desc->len) {
			fprintf(stderr, "failed to translate desc address.\n");
			return;
		}

		task->desc = descriptor_get_next(task->vq->desc, task->desc);
		if (!descriptor_has_next(task->desc)) {
			task->dxfer_dir = SCSI_DIR_NONE;
			chunck_len = task->desc->len;
			task->resp = (void *)(uintptr_t)
					      gpa_to_vva(task->bdev->vid,
							 task->desc->addr,
							 &chunck_len);
			if (!task->resp || chunck_len != task->desc->len) {
				fprintf(stderr, "failed to translate desc address.\n");
				return;
			}
		} else if (!descriptor_is_wr(task->desc)) {
			task->dxfer_dir = SCSI_DIR_TO_DEV;
			vhost_process_write_payload_chain(task);
		} else {
			task->dxfer_dir = SCSI_DIR_FROM_DEV;
			vhost_process_read_payload_chain(task);
		}

		ret = vhost_bdev_process_scsi_commands(ctrlr->bdev, task);
		if (ret) {
			/* invalid response */
			task->resp->response = VIRTIO_SCSI_S_BAD_TARGET;
		} else {
			/* successfully */
			task->resp->response = VIRTIO_SCSI_S_OK;
			task->resp->status = 0;
			task->resp->resid = 0;
		}
		submit_completion(task, q_idx);
		rte_free(task);
	}
}

/* Main framework for processing IOs */
static void *
ctrlr_worker(void *arg)
{
	uint32_t idx, num;
	struct vhost_scsi_ctrlr *ctrlr = (struct vhost_scsi_ctrlr *)arg;
	cpu_set_t cpuset;
	pthread_t thread;

	thread = pthread_self();
	CPU_ZERO(&cpuset);
	CPU_SET(0, &cpuset);
	pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);

	num =  rte_vhost_get_vring_num(ctrlr->bdev->vid);
	fprintf(stdout, "Ctrlr Worker Thread Started with %u Vring\n", num);

	if (num != NUM_OF_SCSI_QUEUES) {
		fprintf(stderr, "Only 1 IO queue are supported\n");
		exit(0);
	}

	while (!g_should_stop && ctrlr->bdev != NULL) {
		/* At least 3 vrings, currently only can support 1 IO queue
		 * Queue 2 for IO queue, does not support TMF and hotplug
		 * for the example application now
		 */
		for (idx = 2; idx < num; idx++)
			process_requestq(ctrlr, idx);
	}

	fprintf(stdout, "Ctrlr Worker Thread Exiting\n");
	sem_post(&exit_sem);
	return NULL;
}

static int
new_device(int vid)
{
	char path[PATH_MAX];
	struct vhost_scsi_ctrlr *ctrlr;
	struct vhost_scsi_queue *scsi_vq;
	struct rte_vhost_vring *vq;
	pthread_t tid;
	int i, ret;

	ret = rte_vhost_get_ifname(vid, path, PATH_MAX);
	if (ret) {
		fprintf(stderr, "Cannot get socket name\n");
		return -1;
	}

	ctrlr = vhost_scsi_ctrlr_find(path);
	if (!ctrlr) {
		fprintf(stderr, "Controller is not ready\n");
		return -1;
	}

	ret = rte_vhost_get_mem_table(vid, &ctrlr->mem);
	if (ret) {
		fprintf(stderr, "Get Controller memory region failed\n");
		return -1;
	}
	assert(ctrlr->mem != NULL);

	/* hardcoded block device information with 128MiB */
	ctrlr->bdev = vhost_scsi_bdev_construct("malloc0", "vhost_scsi_malloc0",
						4096, 32768, 0);
	if (!ctrlr->bdev)
		return -1;

	ctrlr->bdev->vid = vid;

	/* Disable Notifications */
	for (i = 0; i < NUM_OF_SCSI_QUEUES; i++) {
		rte_vhost_enable_guest_notification(vid, i, 0);
		/* restore used index */
		scsi_vq = &ctrlr->bdev->queues[i];
		vq = &scsi_vq->vq;
		ret = rte_vhost_get_vhost_vring(ctrlr->bdev->vid, i, vq);
		assert(ret == 0);
		scsi_vq->last_used_idx = vq->used->idx;
		scsi_vq->last_avail_idx = vq->used->idx;
	}

	g_should_stop = 0;
	fprintf(stdout, "New Device %s, Device ID %d\n", path, vid);
	if (pthread_create(&tid, NULL, &ctrlr_worker, ctrlr) < 0) {
		fprintf(stderr, "Worker Thread Started Failed\n");
		return -1;
	}
	pthread_detach(tid);
	return 0;
}

static void
destroy_device(int vid)
{
	char path[PATH_MAX];
	struct vhost_scsi_ctrlr *ctrlr;

	rte_vhost_get_ifname(vid, path, PATH_MAX);
	fprintf(stdout, "Destroy %s Device ID %d\n", path, vid);
	ctrlr = vhost_scsi_ctrlr_find(path);
	if (!ctrlr) {
		fprintf(stderr, "Destroy Ctrlr Failed\n");
		return;
	}
	ctrlr->bdev = NULL;
	g_should_stop = 1;

	sem_wait(&exit_sem);
}

static const struct vhost_device_ops vhost_scsi_device_ops = {
	.new_device =  new_device,
	.destroy_device = destroy_device,
};

static struct vhost_scsi_ctrlr *
vhost_scsi_ctrlr_construct(const char *ctrlr_name)
{
	int ret;
	struct vhost_scsi_ctrlr *ctrlr;
	char *path;
	char cwd[PATH_MAX];

	/* always use current directory */
	path = getcwd(cwd, PATH_MAX);
	if (!path) {
		fprintf(stderr, "Cannot get current working directory\n");
		return NULL;
	}
	snprintf(dev_pathname, sizeof(dev_pathname), "%s/%s", path, ctrlr_name);

	if (access(dev_pathname, F_OK) != -1) {
		if (unlink(dev_pathname) != 0)
			rte_exit(EXIT_FAILURE, "Cannot remove %s.\n",
				 dev_pathname);
	}

	if (rte_vhost_driver_register(dev_pathname, 0) != 0) {
		fprintf(stderr, "socket %s already exists\n", dev_pathname);
		return NULL;
	}

	fprintf(stdout, "socket file: %s created\n", dev_pathname);

	ret = rte_vhost_driver_set_features(dev_pathname, VIRTIO_SCSI_FEATURES);
	if (ret != 0) {
		fprintf(stderr, "Set vhost driver features failed\n");
		return NULL;
	}

	ctrlr = rte_zmalloc(NULL, sizeof(*ctrlr), RTE_CACHE_LINE_SIZE);
	if (!ctrlr)
		return NULL;

	rte_vhost_driver_callback_register(dev_pathname,
					   &vhost_scsi_device_ops);

	return ctrlr;
}

static void
signal_handler(__rte_unused int signum)
{

	if (access(dev_pathname, F_OK) == 0)
		unlink(dev_pathname);
	exit(0);
}

int main(int argc, char *argv[])
{
	int ret;

	signal(SIGINT, signal_handler);

	/* init EAL */
	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");

	g_vhost_ctrlr = vhost_scsi_ctrlr_construct("vhost.socket");
	if (g_vhost_ctrlr == NULL) {
		fprintf(stderr, "Construct vhost scsi controller failed\n");
		return 0;
	}

	if (sem_init(&exit_sem, 0, 0) < 0) {
		fprintf(stderr, "Error init exit_sem\n");
		return -1;
	}

	rte_vhost_driver_start(dev_pathname);

	/* loop for exit the application */
	while (1)
		sleep(1);

	return 0;
}

