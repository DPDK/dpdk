/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2019 Intel Corporation
 */

#include <stdint.h>
#include <unistd.h>
#include <stdbool.h>
#include <signal.h>
#include <assert.h>
#include <semaphore.h>
#include <linux/virtio_blk.h>
#include <linux/virtio_ring.h>

#include <rte_atomic.h>
#include <rte_cycles.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_vhost.h>

#include "vhost_blk.h"
#include "blk_spec.h"

#define VIRTQ_DESC_F_NEXT	1
#define VIRTQ_DESC_F_AVAIL	(1 << 7)
#define VIRTQ_DESC_F_USED	(1 << 15)

#define MAX_TASK		12

#define VHOST_BLK_FEATURES ((1ULL << VIRTIO_F_RING_PACKED) | \
			    (1ULL << VIRTIO_F_VERSION_1) |\
			    (1ULL << VIRTIO_F_NOTIFY_ON_EMPTY) | \
			    (1ULL << VHOST_USER_F_PROTOCOL_FEATURES))
#define CTRLR_NAME		"vhost.socket"

enum CTRLR_WORKER_STATUS {
	WORKER_STATE_START = 0,
	WORKER_STATE_STOP,
};

struct vhost_blk_ctrlr *g_vhost_ctrlr;

/* Path to folder where character device will be created. Can be set by user. */
static char dev_pathname[PATH_MAX] = "";
static sem_t exit_sem;
static enum CTRLR_WORKER_STATUS worker_thread_status;

struct vhost_blk_ctrlr *
vhost_blk_ctrlr_find(const char *ctrlr_name)
{
	if (ctrlr_name == NULL)
		return NULL;

	/* currently we only support 1 socket file fd */
	return g_vhost_ctrlr;
}

static uint64_t
gpa_to_vva(struct vhost_blk_ctrlr *ctrlr, uint64_t gpa, uint64_t *len)
{
	assert(ctrlr->mem != NULL);

	return rte_vhost_va_from_guest_pa(ctrlr->mem, gpa, len);
}

static void
enqueue_task(struct vhost_blk_task *task)
{
	struct vhost_blk_queue *vq = task->vq;
	struct vring_used *used = vq->vring.used;

	rte_vhost_set_last_inflight_io_split(task->ctrlr->vid,
		vq->id, task->req_idx);

	/* Fill out the next entry in the "used" ring.  id = the
	 * index of the descriptor that contained the blk request.
	 * len = the total amount of data transferred for the blk
	 * request. We must report the correct len, for variable
	 * length blk CDBs, where we may return less data than
	 * allocated by the guest VM.
	 */
	used->ring[used->idx & (vq->vring.size - 1)].id = task->req_idx;
	used->ring[used->idx & (vq->vring.size - 1)].len = task->data_len;
	rte_smp_mb();
	used->idx++;
	rte_smp_mb();

	rte_vhost_clr_inflight_desc_split(task->ctrlr->vid,
		vq->id, used->idx, task->req_idx);

	/* Send an interrupt back to the guest VM so that it knows
	 * a completion is ready to be processed.
	 */
	rte_vhost_vring_call(task->ctrlr->vid, vq->id);
}

static void
enqueue_task_packed(struct vhost_blk_task *task)
{
	struct vhost_blk_queue *vq = task->vq;
	struct vring_packed_desc *desc;

	rte_vhost_set_last_inflight_io_packed(task->ctrlr->vid, vq->id,
					    task->inflight_idx);

	desc = &vq->vring.desc_packed[vq->last_used_idx];
	desc->id = task->buffer_id;
	desc->addr = 0;

	rte_smp_mb();
	if (vq->used_wrap_counter)
		desc->flags |= VIRTQ_DESC_F_AVAIL | VIRTQ_DESC_F_USED;
	else
		desc->flags &= ~(VIRTQ_DESC_F_AVAIL | VIRTQ_DESC_F_USED);
	rte_smp_mb();

	rte_vhost_clr_inflight_desc_packed(task->ctrlr->vid, vq->id,
					   task->inflight_idx);

	vq->last_used_idx += task->chain_num;
	if (vq->last_used_idx >= vq->vring.size) {
		vq->last_used_idx -= vq->vring.size;
		vq->used_wrap_counter = !vq->used_wrap_counter;
	}

	/* Send an interrupt back to the guest VM so that it knows
	 * a completion is ready to be processed.
	 */
	rte_vhost_vring_call(task->ctrlr->vid, vq->id);
}

static bool
descriptor_has_next_packed(struct vring_packed_desc *cur_desc)
{
	return !!(cur_desc->flags & VRING_DESC_F_NEXT);
}

static bool
descriptor_has_next_split(struct vring_desc *cur_desc)
{
	return !!(cur_desc->flags & VRING_DESC_F_NEXT);
}

static int
desc_payload_to_iovs(struct vhost_blk_ctrlr *ctrlr, struct iovec *iovs,
		     uint32_t *iov_index, uintptr_t payload, uint64_t remaining)
{
	void *vva;
	uint64_t len;

	do {
		if (*iov_index >= VHOST_BLK_MAX_IOVS) {
			fprintf(stderr, "VHOST_BLK_MAX_IOVS reached\n");
			return -1;
		}
		len = remaining;
		vva = (void *)(uintptr_t)gpa_to_vva(ctrlr,
				 payload, &len);
		if (!vva || !len) {
			fprintf(stderr, "failed to translate desc address.\n");
			return -1;
		}

		iovs[*iov_index].iov_base = vva;
		iovs[*iov_index].iov_len = len;
		payload += len;
		remaining -= len;
		(*iov_index)++;
	} while (remaining);

	return 0;
}

static struct vring_desc *
vring_get_next_desc(struct vhost_blk_queue *vq, struct vring_desc *desc)
{
	if (descriptor_has_next_split(desc))
		return &vq->vring.desc[desc->next];

	return NULL;
}

static struct vring_packed_desc *
vring_get_next_desc_packed(struct vhost_blk_queue *vq, uint16_t *req_idx)
{
	if (descriptor_has_next_packed(&vq->vring.desc_packed[*req_idx])) {
		*req_idx = (*req_idx + 1) % vq->vring.size;
		return &vq->vring.desc_packed[*req_idx];
	}

	return NULL;
}

static struct rte_vhost_inflight_desc_packed *
vring_get_next_inflight_desc(struct vhost_blk_queue *vq,
			struct rte_vhost_inflight_desc_packed *desc)
{
	if (!!(desc->flags & VRING_DESC_F_NEXT))
		return &vq->inflight_ring.inflight_packed->desc[desc->next];

	return NULL;
}

static int
setup_iovs_from_descs_split(struct vhost_blk_ctrlr *ctrlr,
			    struct vhost_blk_queue *vq, uint16_t req_idx,
			    struct iovec *iovs, uint32_t *iovs_idx,
			    uint32_t *payload)
{
	struct vring_desc *desc = &vq->vring.desc[req_idx];

	do {
		/* does not support indirect descriptors */
		assert((desc->flags & VRING_DESC_F_INDIRECT) == 0);

		if (*iovs_idx >= VHOST_BLK_MAX_IOVS) {
			fprintf(stderr, "Reach VHOST_BLK_MAX_IOVS\n");
			return -1;
		}

		if (desc_payload_to_iovs(ctrlr, iovs, iovs_idx,
			desc->addr, desc->len) != 0) {
			fprintf(stderr, "Failed to convert desc payload to iovs\n");
			return -1;
		}

		*payload += desc->len;

		desc = vring_get_next_desc(vq, desc);
	} while (desc != NULL);

	return 0;
}

static int
setup_iovs_from_descs_packed(struct vhost_blk_ctrlr *ctrlr,
			     struct vhost_blk_queue *vq, uint16_t req_idx,
			     struct iovec *iovs, uint32_t *iovs_idx,
			     uint32_t *payload)
{
	struct vring_packed_desc *desc = &vq->vring.desc_packed[req_idx];

	do {
		/* does not support indirect descriptors */
		assert((desc->flags & VRING_DESC_F_INDIRECT) == 0);

		if (*iovs_idx >= VHOST_BLK_MAX_IOVS) {
			fprintf(stderr, "Reach VHOST_BLK_MAX_IOVS\n");
			return -1;
		}

		if (desc_payload_to_iovs(ctrlr, iovs, iovs_idx,
			desc->addr, desc->len) != 0) {
			fprintf(stderr, "Failed to convert desc payload to iovs\n");
			return -1;
		}

		*payload += desc->len;

		desc = vring_get_next_desc_packed(vq, &req_idx);
	} while (desc != NULL);

	return 0;
}

static int
setup_iovs_from_inflight_desc(struct vhost_blk_ctrlr *ctrlr,
			      struct vhost_blk_queue *vq, uint16_t req_idx,
			      struct iovec *iovs, uint32_t *iovs_idx,
			      uint32_t *payload)
{
	struct rte_vhost_ring_inflight *inflight_vq;
	struct rte_vhost_inflight_desc_packed *desc;

	inflight_vq = &vq->inflight_ring;
	desc = &inflight_vq->inflight_packed->desc[req_idx];

	do {
		/* does not support indirect descriptors */
		assert((desc->flags & VRING_DESC_F_INDIRECT) == 0);

		if (*iovs_idx >= VHOST_BLK_MAX_IOVS) {
			fprintf(stderr, "Reach VHOST_BLK_MAX_IOVS\n");
			return -1;
		}

		if (desc_payload_to_iovs(ctrlr, iovs, iovs_idx,
			desc->addr, desc->len) != 0) {
			fprintf(stderr, "Failed to convert desc payload to iovs\n");
			return -1;
		}

		*payload += desc->len;

		desc = vring_get_next_inflight_desc(vq, desc);
	} while (desc != NULL);

	return 0;
}

static void
process_blk_task(struct vhost_blk_task *task)
{
	uint32_t payload = 0;

	if (task->vq->packed_ring) {
		struct rte_vhost_ring_inflight *inflight_ring;
		struct rte_vhost_resubmit_info *resubmit_inflight;

		inflight_ring = &task->vq->inflight_ring;
		resubmit_inflight = inflight_ring->resubmit_inflight;

		if (resubmit_inflight != NULL &&
		    resubmit_inflight->resubmit_list != NULL) {
			if (setup_iovs_from_inflight_desc(task->ctrlr, task->vq,
				task->req_idx, task->iovs, &task->iovs_cnt,
				&payload)) {
				fprintf(stderr, "Failed to setup iovs\n");
				return;
			}
		} else {
			if (setup_iovs_from_descs_packed(task->ctrlr, task->vq,
				task->req_idx, task->iovs, &task->iovs_cnt,
				&payload)) {
				fprintf(stderr, "Failed to setup iovs\n");
				return;
			}
		}
	} else {
		if (setup_iovs_from_descs_split(task->ctrlr, task->vq,
			task->req_idx, task->iovs, &task->iovs_cnt, &payload)) {
			fprintf(stderr, "Failed to setup iovs\n");
			return;
		}
	}

	/* First IOV must be the req head. */
	task->req = (struct virtio_blk_outhdr *)task->iovs[0].iov_base;
	assert(sizeof(*task->req) == task->iovs[0].iov_len);

	/* Last IOV must be the status tail. */
	task->status = (uint8_t *)task->iovs[task->iovs_cnt - 1].iov_base;
	assert(sizeof(*task->status) == task->iovs[task->iovs_cnt - 1].iov_len);

	/* Transport data len */
	task->data_len = payload - task->iovs[0].iov_len -
		task->iovs[task->iovs_cnt - 1].iov_len;

	if (vhost_bdev_process_blk_commands(task->ctrlr->bdev, task))
		/* invalid response */
		*task->status = VIRTIO_BLK_S_IOERR;
	else
		/* successfully */
		*task->status = VIRTIO_BLK_S_OK;

	if (task->vq->packed_ring)
		enqueue_task_packed(task);
	else
		enqueue_task(task);
}

static void
blk_task_init(struct vhost_blk_task *task)
{
	task->iovs_cnt = 0;
	task->data_len = 0;
	task->req = NULL;
	task->status = NULL;
}

static void
submit_inflight_vq(struct vhost_blk_queue *vq)
{
	struct rte_vhost_ring_inflight *inflight_ring;
	struct rte_vhost_resubmit_info *resubmit_inflight;
	struct vhost_blk_task *task;

	inflight_ring = &vq->inflight_ring;
	resubmit_inflight = inflight_ring->resubmit_inflight;

	if (resubmit_inflight == NULL ||
	    resubmit_inflight->resubmit_num == 0)
		return;

	fprintf(stdout, "Resubmit inflight num is %d\n",
		resubmit_inflight->resubmit_num);

	while (resubmit_inflight->resubmit_num-- > 0) {
		uint16_t desc_idx;

		desc_idx = resubmit_inflight->resubmit_list[
					resubmit_inflight->resubmit_num].index;

		if (vq->packed_ring) {
			uint16_t task_idx;
			struct rte_vhost_inflight_desc_packed *desc;

			desc = inflight_ring->inflight_packed->desc;
			task_idx = desc[desc[desc_idx].last].id;
			task = &vq->tasks[task_idx];

			task->req_idx = desc_idx;
			task->chain_num = desc[desc_idx].num;
			task->buffer_id = task_idx;
			task->inflight_idx = desc_idx;

			vq->last_avail_idx += desc[desc_idx].num;
			if (vq->last_avail_idx >= vq->vring.size) {
				vq->last_avail_idx -= vq->vring.size;
				vq->avail_wrap_counter =
					!vq->avail_wrap_counter;
			}
		} else
			/* In split ring, the desc_idx is the req_id
			 * which was initialized when allocated the task pool.
			 */
			task = &vq->tasks[desc_idx];

		blk_task_init(task);
		process_blk_task(task);
	}

	free(resubmit_inflight->resubmit_list);
	resubmit_inflight->resubmit_list = NULL;
}

/* Use the buffer_id as the task_idx */
static uint16_t
vhost_blk_vq_get_desc_chain_buffer_id(struct vhost_blk_queue *vq,
				      uint16_t *req_head, uint16_t *num)
{
	struct vring_packed_desc *desc = &vq->vring.desc_packed[
						vq->last_avail_idx];

	*req_head = vq->last_avail_idx;
	*num = 1;

	while (descriptor_has_next_packed(desc)) {
		vq->last_avail_idx = (vq->last_avail_idx + 1) % vq->vring.size;
		desc = &vq->vring.desc_packed[vq->last_avail_idx];
		*num += 1;
	}

	/* Point to next desc */
	vq->last_avail_idx = (vq->last_avail_idx + 1) % vq->vring.size;
	if (vq->last_avail_idx < *req_head)
		vq->avail_wrap_counter = !vq->avail_wrap_counter;

	return desc->id;
}

static uint16_t
vq_get_desc_idx(struct vhost_blk_queue *vq)
{
	uint16_t desc_idx;
	uint16_t last_avail_idx;

	last_avail_idx = vq->last_avail_idx & (vq->vring.size - 1);
	desc_idx = vq->vring.avail->ring[last_avail_idx];
	vq->last_avail_idx++;

	return desc_idx;
}

static int
vhost_blk_vq_is_avail(struct vhost_blk_queue *vq)
{
	if (vq->packed_ring) {
		uint16_t flags = vq->vring.desc_packed[
					vq->last_avail_idx].flags;
		bool avail_wrap_counter = vq->avail_wrap_counter;

		return (!!(flags & VIRTQ_DESC_F_AVAIL) == avail_wrap_counter &&
			!!(flags & VIRTQ_DESC_F_USED) != avail_wrap_counter);
	} else {
		if (vq->vring.avail->idx != vq->last_avail_idx)
			return 1;

		return 0;
	}
}

static void
process_vq(struct vhost_blk_queue *vq)
{
	struct vhost_blk_task *task;

	if (vq->packed_ring) {
		while (vhost_blk_vq_is_avail(vq)) {
			uint16_t task_idx, req_idx, last_idx, chain_num;

			task_idx = vhost_blk_vq_get_desc_chain_buffer_id(vq,
					&req_idx, &chain_num);
			task = &vq->tasks[task_idx];

			blk_task_init(task);
			task->req_idx = req_idx;
			task->chain_num = chain_num;
			task->buffer_id = task_idx;
			last_idx = (req_idx + chain_num - 1) % vq->vring.size;

			rte_vhost_set_inflight_desc_packed(task->ctrlr->vid,
							   vq->id,
							   task->req_idx,
							   last_idx,
							   &task->inflight_idx);

			process_blk_task(task);
		}
	} else {
		while (vhost_blk_vq_is_avail(vq)) {
			uint16_t desc_idx;

			desc_idx = vq_get_desc_idx(vq);
			task = &vq->tasks[desc_idx];

			blk_task_init(task);
			rte_vhost_set_inflight_desc_split(task->ctrlr->vid,
							  vq->id,
							  task->req_idx);
			process_blk_task(task);
		}
	}
}

static void *
ctrlr_worker(void *arg)
{
	struct vhost_blk_ctrlr *ctrlr = (struct vhost_blk_ctrlr *)arg;
	cpu_set_t cpuset;
	pthread_t thread;
	int i;

	fprintf(stdout, "Ctrlr Worker Thread start\n");

	if (ctrlr == NULL || ctrlr->bdev == NULL) {
		fprintf(stderr,
			"%s: Error, invalid argument passed to worker thread\n",
			__func__);
		exit(0);
	}

	thread = pthread_self();
	CPU_ZERO(&cpuset);
	CPU_SET(0, &cpuset);
	pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);

	for (i = 0; i < NUM_OF_BLK_QUEUES; i++)
		submit_inflight_vq(&ctrlr->queues[i]);

	while (worker_thread_status != WORKER_STATE_STOP)
		for (i = 0; i < NUM_OF_BLK_QUEUES; i++)
			process_vq(&ctrlr->queues[i]);

	fprintf(stdout, "Ctrlr Worker Thread Exiting\n");
	sem_post(&exit_sem);
	return NULL;
}

static int
alloc_task_pool(struct vhost_blk_ctrlr *ctrlr)
{
	struct vhost_blk_queue *vq;
	int i, j;

	for (i = 0; i < NUM_OF_BLK_QUEUES; i++) {
		vq = &ctrlr->queues[i];

		vq->tasks = rte_zmalloc(NULL,
			sizeof(struct vhost_blk_task) * vq->vring.size, 0);
		if (!vq->tasks) {
			fprintf(stderr, "Failed to allocate task memory\n");
			return -1;
		}

		for (j = 0; j < vq->vring.size; j++) {
			vq->tasks[j].req_idx = j;
			vq->tasks[j].ctrlr = ctrlr;
			vq->tasks[j].vq = vq;
		}
	}

	return 0;
}

static void
free_task_pool(struct vhost_blk_ctrlr *ctrlr)
{
	int i;

	for (i = 0; i < NUM_OF_BLK_QUEUES; i++)
		rte_free(ctrlr->queues[i].tasks);
}

static int
new_device(int vid)
{
	struct vhost_blk_ctrlr *ctrlr;
	struct vhost_blk_queue *vq;
	char path[PATH_MAX];
	uint64_t features;
	pthread_t tid;
	int i, ret;
	bool packed_ring;

	ret = rte_vhost_get_ifname(vid, path, PATH_MAX);
	if (ret) {
		fprintf(stderr, "Failed to get the socket path\n");
		return -1;
	}

	ctrlr = vhost_blk_ctrlr_find(path);
	if (!ctrlr) {
		fprintf(stderr, "Failed to find controller\n");
		return -1;
	}

	if (ctrlr->started)
		return 0;

	ctrlr->vid = vid;
	ret = rte_vhost_get_negotiated_features(vid, &features);
	if (ret) {
		fprintf(stderr, "Failed to get the negotiated features\n");
		return -1;
	}
	packed_ring = !!(features & (1ULL << VIRTIO_F_RING_PACKED));

	/* Disable Notifications and init last idx */
	for (i = 0; i < NUM_OF_BLK_QUEUES; i++) {
		vq = &ctrlr->queues[i];
		vq->id = i;

		assert(rte_vhost_get_vhost_vring(ctrlr->vid, i,
						 &vq->vring) == 0);
		assert(rte_vhost_get_vring_base(ctrlr->vid, i,
					       &vq->last_avail_idx,
					       &vq->last_used_idx) == 0);
		assert(rte_vhost_get_vhost_ring_inflight(ctrlr->vid, i,
						&vq->inflight_ring) == 0);

		if (packed_ring) {
			/* for the reconnection */
			assert(rte_vhost_get_vring_base_from_inflight(
				ctrlr->vid, i,
				&vq->last_avail_idx,
				&vq->last_used_idx) == 0);

			vq->avail_wrap_counter = vq->last_avail_idx &
				(1 << 15);
			vq->last_avail_idx = vq->last_avail_idx &
				0x7fff;
			vq->used_wrap_counter = vq->last_used_idx &
				(1 << 15);
			vq->last_used_idx = vq->last_used_idx &
				0x7fff;
		}

		vq->packed_ring = packed_ring;
		rte_vhost_enable_guest_notification(vid, i, 0);
	}

	assert(rte_vhost_get_mem_table(vid, &ctrlr->mem) == 0);
	assert(ctrlr->mem != NULL);
	assert(alloc_task_pool(ctrlr) == 0);

	/* start polling vring */
	worker_thread_status = WORKER_STATE_START;
	fprintf(stdout, "New Device %s, Device ID %d\n", path, vid);
	if (pthread_create(&tid, NULL, &ctrlr_worker, ctrlr) < 0) {
		fprintf(stderr, "Worker Thread Started Failed\n");
		return -1;
	}

	/* device has been started */
	ctrlr->started = 1;
	pthread_detach(tid);
	return 0;
}

static void
destroy_device(int vid)
{
	char path[PATH_MAX];
	struct vhost_blk_ctrlr *ctrlr;
	struct vhost_blk_queue *vq;
	int i, ret;

	ret = rte_vhost_get_ifname(vid, path, PATH_MAX);
	if (ret) {
		fprintf(stderr, "Destroy Ctrlr Failed\n");
		return;
	}

	fprintf(stdout, "Destroy %s Device ID %d\n", path, vid);
	ctrlr = vhost_blk_ctrlr_find(path);
	if (!ctrlr) {
		fprintf(stderr, "Destroy Ctrlr Failed\n");
		return;
	}

	if (!ctrlr->started)
		return;

	worker_thread_status = WORKER_STATE_STOP;
	sem_wait(&exit_sem);

	for (i = 0; i < NUM_OF_BLK_QUEUES; i++) {
		vq = &ctrlr->queues[i];
		if (vq->packed_ring) {
			vq->last_avail_idx |= (vq->avail_wrap_counter <<
				15);
			vq->last_used_idx |= (vq->used_wrap_counter <<
				15);
		}

		rte_vhost_set_vring_base(ctrlr->vid, i,
					 vq->last_avail_idx,
					 vq->last_used_idx);
	}

	free_task_pool(ctrlr);
	free(ctrlr->mem);

	ctrlr->started = 0;
}

static int
new_connection(int vid)
{
	/* extend the proper features for block device */
	vhost_session_install_rte_compat_hooks(vid);

	return 0;
}

struct vhost_device_ops vhost_blk_device_ops = {
	.new_device =  new_device,
	.destroy_device = destroy_device,
	.new_connection = new_connection,
};

static struct vhost_block_dev *
vhost_blk_bdev_construct(const char *bdev_name,
	const char *bdev_serial, uint32_t blk_size, uint64_t blk_cnt,
	bool wce_enable)
{
	struct vhost_block_dev *bdev;

	bdev = rte_zmalloc(NULL, sizeof(*bdev), RTE_CACHE_LINE_SIZE);
	if (!bdev)
		return NULL;

	snprintf(bdev->name, sizeof(bdev->name), "%s", bdev_name);
	snprintf(bdev->product_name, sizeof(bdev->product_name), "%s",
		 bdev_serial);
	bdev->blocklen = blk_size;
	bdev->blockcnt = blk_cnt;
	bdev->write_cache = wce_enable;

	fprintf(stdout, "Blocklen=%d, blockcnt=%"PRIx64"\n", bdev->blocklen,
		bdev->blockcnt);

	/* use memory as disk storage space */
	bdev->data = rte_zmalloc(NULL, blk_cnt * blk_size, 0);
	if (!bdev->data) {
		fprintf(stderr, "No enough reserved huge memory for disk\n");
		free(bdev);
		return NULL;
	}

	return bdev;
}

static struct vhost_blk_ctrlr *
vhost_blk_ctrlr_construct(const char *ctrlr_name)
{
	int ret;
	struct vhost_blk_ctrlr *ctrlr;
	char *path;
	char cwd[PATH_MAX];

	/* always use current directory */
	path = getcwd(cwd, PATH_MAX);
	if (!path) {
		fprintf(stderr, "Cannot get current working directory\n");
		return NULL;
	}
	snprintf(dev_pathname, sizeof(dev_pathname), "%s/%s", path, ctrlr_name);

	unlink(dev_pathname);

	if (rte_vhost_driver_register(dev_pathname, 0) != 0) {
		fprintf(stderr, "Socket %s already exists\n", dev_pathname);
		return NULL;
	}

	ret = rte_vhost_driver_set_features(dev_pathname, VHOST_BLK_FEATURES);
	if (ret != 0) {
		fprintf(stderr, "Set vhost driver features failed\n");
		rte_vhost_driver_unregister(dev_pathname);
		return NULL;
	}

	/* set vhost user protocol features */
	vhost_dev_install_rte_compat_hooks(dev_pathname);

	ctrlr = rte_zmalloc(NULL, sizeof(*ctrlr), RTE_CACHE_LINE_SIZE);
	if (!ctrlr) {
		rte_vhost_driver_unregister(dev_pathname);
		return NULL;
	}

	/* hardcoded block device information with 128MiB */
	ctrlr->bdev = vhost_blk_bdev_construct("malloc0", "vhost_blk_malloc0",
						4096, 32768, 0);
	if (!ctrlr->bdev) {
		rte_free(ctrlr);
		rte_vhost_driver_unregister(dev_pathname);
		return NULL;
	}

	rte_vhost_driver_callback_register(dev_pathname,
					   &vhost_blk_device_ops);

	return ctrlr;
}

static void
vhost_blk_ctrlr_destroy(struct vhost_blk_ctrlr *ctrlr)
{
	if (ctrlr->bdev != NULL) {
		if (ctrlr->bdev->data != NULL)
			rte_free(ctrlr->bdev->data);

		rte_free(ctrlr->bdev);
	}
	rte_free(ctrlr);

	rte_vhost_driver_unregister(dev_pathname);
}

static void
signal_handler(__rte_unused int signum)
{
	struct vhost_blk_ctrlr *ctrlr;

	ctrlr = vhost_blk_ctrlr_find(dev_pathname);
	if (ctrlr == NULL)
		return;

	if (ctrlr->started)
		destroy_device(ctrlr->vid);

	vhost_blk_ctrlr_destroy(ctrlr);
	exit(0);
}

int main(int argc, char *argv[])
{
	int ret;

	/* init EAL */
	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");

	g_vhost_ctrlr = vhost_blk_ctrlr_construct(CTRLR_NAME);
	if (g_vhost_ctrlr == NULL) {
		fprintf(stderr, "Construct vhost blk controller failed\n");
		return 0;
	}

	if (sem_init(&exit_sem, 0, 0) < 0) {
		fprintf(stderr, "Error init exit_sem\n");
		return -1;
	}

	signal(SIGINT, signal_handler);

	rte_vhost_driver_start(dev_pathname);

	/* loop for exit the application */
	while (1)
		sleep(1);

	return 0;
}
