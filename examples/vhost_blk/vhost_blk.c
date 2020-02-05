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

struct vhost_blk_ctrlr *g_vhost_ctrlr;

/* Path to folder where character device will be created. Can be set by user. */
static char dev_pathname[PATH_MAX] = "";
static sem_t exit_sem;
static int g_should_stop = -1;

struct vhost_blk_ctrlr *
vhost_blk_ctrlr_find(const char *ctrlr_name)
{
	if (ctrlr_name == NULL)
		return NULL;

	/* currently we only support 1 socket file fd */
	return g_vhost_ctrlr;
}

static uint64_t gpa_to_vva(int vid, uint64_t gpa, uint64_t *len)
{
	char path[PATH_MAX];
	struct vhost_blk_ctrlr *ctrlr;
	int ret = 0;

	ret = rte_vhost_get_ifname(vid, path, PATH_MAX);
	if (ret) {
		fprintf(stderr, "Cannot get socket name\n");
		assert(ret != 0);
	}

	ctrlr = vhost_blk_ctrlr_find(path);
	if (!ctrlr) {
		fprintf(stderr, "Controller is not ready\n");
		assert(ctrlr != NULL);
	}

	assert(ctrlr->mem != NULL);

	return rte_vhost_va_from_guest_pa(ctrlr->mem, gpa, len);
}

static struct vring_packed_desc *
descriptor_get_next_packed(struct rte_vhost_vring *vq,
			     uint16_t *idx)
{
	if (vq->desc_packed[*idx % vq->size].flags & VIRTQ_DESC_F_NEXT) {
		*idx += 1;
		return &vq->desc_packed[*idx % vq->size];
	}

	return NULL;
}

static bool
descriptor_has_next_packed(struct vring_packed_desc *cur_desc)
{
	return !!(cur_desc->flags & VRING_DESC_F_NEXT);
}

static bool
descriptor_is_wr_packed(struct vring_packed_desc *cur_desc)
{
	return !!(cur_desc->flags & VRING_DESC_F_WRITE);
}

static struct rte_vhost_inflight_desc_packed *
inflight_desc_get_next(struct rte_vhost_inflight_info_packed *inflight_packed,
			       struct rte_vhost_inflight_desc_packed *cur_desc)
{
	if (!!(cur_desc->flags & VIRTQ_DESC_F_NEXT))
		return &inflight_packed->desc[cur_desc->next];

	return NULL;
}

static bool
inflight_desc_has_next(struct rte_vhost_inflight_desc_packed *cur_desc)
{
	return !!(cur_desc->flags & VRING_DESC_F_NEXT);
}

static bool
inflight_desc_is_wr(struct rte_vhost_inflight_desc_packed *cur_desc)
{
	return !!(cur_desc->flags & VRING_DESC_F_WRITE);
}

static void
inflight_process_payload_chain_packed(struct inflight_blk_task *task)
{
	void *data;
	uint64_t chunck_len;
	struct vhost_blk_task *blk_task;
	struct rte_vhost_inflight_desc_packed *desc;

	blk_task = &task->blk_task;
	blk_task->iovs_cnt = 0;

	do {
		desc = task->inflight_desc;
		chunck_len = desc->len;
		data = (void *)(uintptr_t)gpa_to_vva(blk_task->bdev->vid,
						     desc->addr,
						     &chunck_len);
		if (!data || chunck_len != desc->len) {
			fprintf(stderr, "failed to translate desc address.\n");
			return;
		}

		blk_task->iovs[blk_task->iovs_cnt].iov_base = data;
		blk_task->iovs[blk_task->iovs_cnt].iov_len = desc->len;
		blk_task->data_len += desc->len;
		blk_task->iovs_cnt++;
		task->inflight_desc = inflight_desc_get_next(
					task->inflight_packed, desc);
	} while (inflight_desc_has_next(task->inflight_desc));

	chunck_len = task->inflight_desc->len;
	blk_task->status = (void *)(uintptr_t)gpa_to_vva(
		blk_task->bdev->vid, task->inflight_desc->addr, &chunck_len);
	if (!blk_task->status || chunck_len != task->inflight_desc->len)
		fprintf(stderr, "failed to translate desc address.\n");
}

static void
inflight_submit_completion_packed(struct inflight_blk_task *task,
					      uint32_t q_idx, uint16_t *used_id,
					      bool *used_wrap_counter)
{
	struct vhost_blk_ctrlr *ctrlr;
	struct rte_vhost_vring *vq;
	struct vring_packed_desc *desc;
	int ret;

	ctrlr = vhost_blk_ctrlr_find(dev_pathname);
	vq = task->blk_task.vq;

	ret = rte_vhost_set_last_inflight_io_packed(ctrlr->bdev->vid, q_idx,
						    task->blk_task.head_idx);
	if (ret != 0)
		fprintf(stderr, "failed to set last inflight io\n");

	desc = &vq->desc_packed[*used_id];
	desc->id = task->blk_task.buffer_id;
	rte_smp_mb();
	if (*used_wrap_counter)
		desc->flags |= VIRTQ_DESC_F_AVAIL | VIRTQ_DESC_F_USED;
	else
		desc->flags &= ~(VIRTQ_DESC_F_AVAIL | VIRTQ_DESC_F_USED);
	rte_smp_mb();

	*used_id += task->blk_task.iovs_cnt + 2;
	if (*used_id >= vq->size) {
		*used_id -= vq->size;
		*used_wrap_counter = !(*used_wrap_counter);
	}

	ret = rte_vhost_clr_inflight_desc_packed(ctrlr->bdev->vid, q_idx,
						 task->blk_task.head_idx);
	if (ret != 0)
		fprintf(stderr, "failed to clear inflight io\n");

	/* Send an interrupt back to the guest VM so that it knows
	 * a completion is ready to be processed.
	 */
	rte_vhost_vring_call(task->blk_task.bdev->vid, q_idx);
}

static void
submit_completion_packed(struct vhost_blk_task *task, uint32_t q_idx,
				  uint16_t *used_id, bool *used_wrap_counter)
{
	struct vhost_blk_ctrlr *ctrlr;
	struct rte_vhost_vring *vq;
	struct vring_packed_desc *desc;
	int ret;

	ctrlr = vhost_blk_ctrlr_find(dev_pathname);
	vq = task->vq;

	ret = rte_vhost_set_last_inflight_io_packed(ctrlr->bdev->vid, q_idx,
						    task->inflight_idx);
	if (ret != 0)
		fprintf(stderr, "failed to set last inflight io\n");

	desc = &vq->desc_packed[*used_id];
	desc->id = task->buffer_id;
	rte_smp_mb();
	if (*used_wrap_counter)
		desc->flags |= VIRTQ_DESC_F_AVAIL | VIRTQ_DESC_F_USED;
	else
		desc->flags &= ~(VIRTQ_DESC_F_AVAIL | VIRTQ_DESC_F_USED);
	rte_smp_mb();

	*used_id += task->iovs_cnt + 2;
	if (*used_id >= vq->size) {
		*used_id -= vq->size;
		*used_wrap_counter = !(*used_wrap_counter);
	}

	ret = rte_vhost_clr_inflight_desc_packed(ctrlr->bdev->vid, q_idx,
						 task->inflight_idx);
	if (ret != 0)
		fprintf(stderr, "failed to clear inflight io\n");

	/* Send an interrupt back to the guest VM so that it knows
	 * a completion is ready to be processed.
	 */
	rte_vhost_vring_call(task->bdev->vid, q_idx);
}

static void
vhost_process_payload_chain_packed(struct vhost_blk_task *task,
	uint16_t *idx)
{
	void *data;
	uint64_t chunck_len;

	task->iovs_cnt = 0;

	do {
		chunck_len = task->desc_packed->len;
		data = (void *)(uintptr_t)gpa_to_vva(task->bdev->vid,
						     task->desc_packed->addr,
							 &chunck_len);
		if (!data || chunck_len != task->desc_packed->len) {
			fprintf(stderr, "failed to translate desc address.\n");
			return;
		}

		task->iovs[task->iovs_cnt].iov_base = data;
		task->iovs[task->iovs_cnt].iov_len = task->desc_packed->len;
		task->data_len += task->desc_packed->len;
		task->iovs_cnt++;
		task->desc_packed = descriptor_get_next_packed(task->vq, idx);
	} while (descriptor_has_next_packed(task->desc_packed));

	task->last_idx = *idx % task->vq->size;
	chunck_len = task->desc_packed->len;
	task->status = (void *)(uintptr_t)gpa_to_vva(task->bdev->vid,
						   task->desc_packed->addr,
						   &chunck_len);
	if (!task->status || chunck_len != task->desc_packed->len)
		fprintf(stderr, "failed to translate desc address.\n");
}


static int
descriptor_is_available(struct rte_vhost_vring *vring, uint16_t idx,
					bool avail_wrap_counter)
{
	uint16_t flags = vring->desc_packed[idx].flags;

	return ((!!(flags & VIRTQ_DESC_F_AVAIL) == avail_wrap_counter) &&
		(!!(flags & VIRTQ_DESC_F_USED) != avail_wrap_counter));
}

static void
process_requestq_packed(struct vhost_blk_ctrlr *ctrlr, uint32_t q_idx)
{
	bool avail_wrap_counter, used_wrap_counter;
	uint16_t avail_idx, used_idx;
	int ret;
	uint64_t chunck_len;
	struct vhost_blk_queue *blk_vq;
	struct rte_vhost_vring *vq;
	struct vhost_blk_task *task;

	blk_vq = &ctrlr->bdev->queues[q_idx];
	vq = &blk_vq->vq;

	avail_idx = blk_vq->last_avail_idx;
	avail_wrap_counter = blk_vq->avail_wrap_counter;
	used_idx = blk_vq->last_used_idx;
	used_wrap_counter = blk_vq->used_wrap_counter;

	task = rte_zmalloc(NULL, sizeof(*task), 0);
	assert(task != NULL);
	task->vq = vq;
	task->bdev = ctrlr->bdev;

	while (descriptor_is_available(vq, avail_idx, avail_wrap_counter)) {
		task->head_idx = avail_idx;
		task->desc_packed = &task->vq->desc_packed[task->head_idx];
		task->iovs_cnt = 0;
		task->data_len = 0;
		task->req = NULL;
		task->status = NULL;

		/* does not support indirect descriptors */
		assert((task->desc_packed->flags & VRING_DESC_F_INDIRECT) == 0);

		chunck_len = task->desc_packed->len;
		task->req = (void *)(uintptr_t)gpa_to_vva(task->bdev->vid,
			task->desc_packed->addr, &chunck_len);
		if (!task->req || chunck_len != task->desc_packed->len) {
			fprintf(stderr, "failed to translate desc address.\n");
			rte_free(task);
			return;
		}

		task->desc_packed = descriptor_get_next_packed(task->vq,
								&avail_idx);
		assert(task->desc_packed != NULL);
		if (!descriptor_has_next_packed(task->desc_packed)) {
			task->dxfer_dir = BLK_DIR_NONE;
			task->last_idx = avail_idx % vq->size;
			chunck_len = task->desc_packed->len;
			task->status = (void *)(uintptr_t)
					      gpa_to_vva(task->bdev->vid,
							task->desc_packed->addr,
							&chunck_len);
			if (!task->status ||
				chunck_len != task->desc_packed->len) {
				fprintf(stderr,
					"failed to translate desc address.\n");
				rte_free(task);
				return;
			}
		} else {
			task->readtype = descriptor_is_wr_packed(
							task->desc_packed);
			vhost_process_payload_chain_packed(task, &avail_idx);
		}
		task->buffer_id = vq->desc_packed[task->last_idx].id;
		rte_vhost_set_inflight_desc_packed(ctrlr->bdev->vid, q_idx,
						   task->head_idx,
						   task->last_idx,
						   &task->inflight_idx);

		if (++avail_idx >= vq->size) {
			avail_idx -= vq->size;
			avail_wrap_counter = !avail_wrap_counter;
		}
		blk_vq->last_avail_idx = avail_idx;
		blk_vq->avail_wrap_counter = avail_wrap_counter;

		ret = vhost_bdev_process_blk_commands(ctrlr->bdev, task);
		if (ret) {
			/* invalid response */
			*task->status = VIRTIO_BLK_S_IOERR;
		} else {
			/* successfully */
			*task->status = VIRTIO_BLK_S_OK;
		}

		submit_completion_packed(task, q_idx, &used_idx,
						&used_wrap_counter);
		blk_vq->last_used_idx = used_idx;
		blk_vq->used_wrap_counter = used_wrap_counter;
	}

	rte_free(task);
}

static void
submit_inflight_vq_packed(struct vhost_blk_ctrlr *ctrlr,
	uint16_t q_idx)
{
	bool used_wrap_counter;
	int req_idx, ret;
	uint16_t used_idx;
	uint64_t chunck_len;
	struct vhost_blk_queue *blk_vq;
	struct rte_vhost_ring_inflight *inflight_vq;
	struct rte_vhost_resubmit_info *resubmit_info;
	struct rte_vhost_vring *vq;
	struct inflight_blk_task *task;
	struct vhost_blk_task *blk_task;
	struct rte_vhost_inflight_info_packed *inflight_info;

	blk_vq = &ctrlr->bdev->queues[q_idx];
	vq = &blk_vq->vq;
	inflight_vq = &blk_vq->inflight_vq;
	resubmit_info = inflight_vq->resubmit_inflight;
	inflight_info = inflight_vq->inflight_packed;
	used_idx = blk_vq->last_used_idx;
	used_wrap_counter = blk_vq->used_wrap_counter;

	task = rte_malloc(NULL, sizeof(*task), 0);
	if (!task) {
		fprintf(stderr, "failed to allocate memory\n");
		return;
	}
	blk_task = &task->blk_task;
	blk_task->vq = vq;
	blk_task->bdev = ctrlr->bdev;
	task->inflight_packed = inflight_vq->inflight_packed;

	while (resubmit_info->resubmit_num-- > 0) {
		req_idx = resubmit_info->resubmit_num;
		blk_task->head_idx =
			resubmit_info->resubmit_list[req_idx].index;
		task->inflight_desc =
			&inflight_info->desc[blk_task->head_idx];
		task->blk_task.iovs_cnt = 0;
		task->blk_task.data_len = 0;
		task->blk_task.req = NULL;
		task->blk_task.status = NULL;

		/* update the avail idx too
		 * as it's initial value equals to used idx
		 */
		blk_vq->last_avail_idx += task->inflight_desc->num;
		if (blk_vq->last_avail_idx >= vq->size) {
			blk_vq->last_avail_idx -= vq->size;
			blk_vq->avail_wrap_counter =
				!blk_vq->avail_wrap_counter;
		}

		/* does not support indirect descriptors */
		assert(task->inflight_desc != NULL);
		assert((task->inflight_desc->flags &
			VRING_DESC_F_INDIRECT) == 0);

		chunck_len = task->inflight_desc->len;
		blk_task->req = (void *)(uintptr_t)
				     gpa_to_vva(blk_task->bdev->vid,
						task->inflight_desc->addr,
						&chunck_len);
		if (!blk_task->req ||
			chunck_len != task->inflight_desc->len) {
			fprintf(stderr, "failed to translate desc address.\n");
			rte_free(task);
			return;
		}

		task->inflight_desc = inflight_desc_get_next(
			task->inflight_packed, task->inflight_desc);
		assert(task->inflight_desc != NULL);
		if (!inflight_desc_has_next(task->inflight_desc)) {
			blk_task->dxfer_dir = BLK_DIR_NONE;
			chunck_len = task->inflight_desc->len;
			blk_task->status = (void *)(uintptr_t)
				gpa_to_vva(blk_task->bdev->vid,
						task->inflight_desc->addr,
						&chunck_len);
			if (!blk_task->status ||
			    chunck_len != task->inflight_desc->len) {
				fprintf(stderr,
					"failed to translate desc address.\n");
				rte_free(task);
				return;
			}
		} else {
			blk_task->readtype =
			inflight_desc_is_wr(task->inflight_desc);
			inflight_process_payload_chain_packed(task);
		}

		blk_task->buffer_id = task->inflight_desc->id;

		ret = vhost_bdev_process_blk_commands(ctrlr->bdev, blk_task);
		if (ret)
			/* invalid response */
			*blk_task->status = VIRTIO_BLK_S_IOERR;
		else
			/* successfully */
			*blk_task->status = VIRTIO_BLK_S_OK;

		inflight_submit_completion_packed(task, q_idx, &used_idx,
						  &used_wrap_counter);

		blk_vq->last_used_idx = used_idx;
		blk_vq->used_wrap_counter = used_wrap_counter;
	}

	rte_free(task);
}

static struct vring_desc *
descriptor_get_next_split(struct vring_desc *vq_desc,
				   struct vring_desc *cur_desc)
{
	return &vq_desc[cur_desc->next];
}

static bool
descriptor_has_next_split(struct vring_desc *cur_desc)
{
	return !!(cur_desc->flags & VRING_DESC_F_NEXT);
}

static bool
descriptor_is_wr_split(struct vring_desc *cur_desc)
{
	return !!(cur_desc->flags & VRING_DESC_F_WRITE);
}

static void
vhost_process_payload_chain_split(struct vhost_blk_task *task)
{
	void *data;
	uint64_t chunck_len;

	task->iovs_cnt = 0;

	do {
		chunck_len = task->desc_split->len;
		data = (void *)(uintptr_t)gpa_to_vva(task->bdev->vid,
						     task->desc_split->addr,
						     &chunck_len);
		if (!data || chunck_len != task->desc_split->len) {
			fprintf(stderr, "failed to translate desc address.\n");
			return;
		}

		task->iovs[task->iovs_cnt].iov_base = data;
		task->iovs[task->iovs_cnt].iov_len = task->desc_split->len;
		task->data_len += task->desc_split->len;
		task->iovs_cnt++;
		task->desc_split =
		descriptor_get_next_split(task->vq->desc, task->desc_split);
	} while (descriptor_has_next_split(task->desc_split));

	chunck_len = task->desc_split->len;
	task->status = (void *)(uintptr_t)gpa_to_vva(task->bdev->vid,
						     task->desc_split->addr,
						     &chunck_len);
	if (!task->status || chunck_len != task->desc_split->len)
		fprintf(stderr, "failed to translate desc address.\n");
}

static void
submit_completion_split(struct vhost_blk_task *task, uint32_t vid,
	uint32_t q_idx)
{
	struct rte_vhost_vring *vq;
	struct vring_used *used;

	vq = task->vq;
	used = vq->used;

	rte_vhost_set_last_inflight_io_split(vid, q_idx, task->req_idx);

	/* Fill out the next entry in the "used" ring.  id = the
	 * index of the descriptor that contained the blk request.
	 * len = the total amount of data transferred for the blk
	 * request. We must report the correct len, for variable
	 * length blk CDBs, where we may return less data than
	 * allocated by the guest VM.
	 */
	used->ring[used->idx & (vq->size - 1)].id = task->req_idx;
	used->ring[used->idx & (vq->size - 1)].len = task->data_len;
	rte_smp_mb();
	used->idx++;
	rte_smp_mb();

	rte_vhost_clr_inflight_desc_split(vid, q_idx, used->idx, task->req_idx);

	/* Send an interrupt back to the guest VM so that it knows
	 * a completion is ready to be processed.
	 */
	rte_vhost_vring_call(task->bdev->vid, q_idx);
}

static void
submit_inflight_vq_split(struct vhost_blk_ctrlr *ctrlr,
	uint32_t q_idx)
{
	struct vhost_blk_queue *blk_vq;
	struct rte_vhost_ring_inflight *inflight_vq;
	struct rte_vhost_resubmit_info *resubmit_inflight;
	struct rte_vhost_resubmit_desc *resubmit_list;
	struct vhost_blk_task *task;
	int req_idx;
	uint64_t chunck_len;
	int ret;

	blk_vq = &ctrlr->bdev->queues[q_idx];
	inflight_vq = &blk_vq->inflight_vq;
	resubmit_inflight = inflight_vq->resubmit_inflight;
	resubmit_list = resubmit_inflight->resubmit_list;

	task = rte_zmalloc(NULL, sizeof(*task), 0);
	assert(task != NULL);

	task->ctrlr = ctrlr;
	task->bdev = ctrlr->bdev;
	task->vq = &blk_vq->vq;

	while (resubmit_inflight->resubmit_num-- > 0) {
		req_idx = resubmit_list[resubmit_inflight->resubmit_num].index;
		task->req_idx = req_idx;
		task->desc_split = &task->vq->desc[task->req_idx];
		task->iovs_cnt = 0;
		task->data_len = 0;
		task->req = NULL;
		task->status = NULL;

		/* does not support indirect descriptors */
		assert(task->desc_split != NULL);
		assert((task->desc_split->flags & VRING_DESC_F_INDIRECT) == 0);

		chunck_len = task->desc_split->len;
		task->req = (void *)(uintptr_t)gpa_to_vva(task->bdev->vid,
				task->desc_split->addr, &chunck_len);
		if (!task->req || chunck_len != task->desc_split->len) {
			fprintf(stderr, "failed to translate desc address.\n");
			rte_free(task);
			return;
		}

		task->desc_split = descriptor_get_next_split(task->vq->desc,
							     task->desc_split);
		if (!descriptor_has_next_split(task->desc_split)) {
			task->dxfer_dir = BLK_DIR_NONE;
			chunck_len = task->desc_split->len;
			task->status = (void *)(uintptr_t)
				       gpa_to_vva(task->bdev->vid,
						  task->desc_split->addr,
						  &chunck_len);
			if (!task->status ||
				chunck_len != task->desc_split->len) {
				fprintf(stderr,
					"failed to translate desc address.\n");
				rte_free(task);
				return;
			}
		} else {
			task->readtype =
				descriptor_is_wr_split(task->desc_split);
			vhost_process_payload_chain_split(task);
		}

		ret = vhost_bdev_process_blk_commands(ctrlr->bdev, task);
		if (ret) {
			/* invalid response */
			*task->status = VIRTIO_BLK_S_IOERR;
		} else {
			/* successfully */
			*task->status = VIRTIO_BLK_S_OK;
		}
		submit_completion_split(task, ctrlr->bdev->vid, q_idx);
	}

	rte_free(task);
}

static void
process_requestq_split(struct vhost_blk_ctrlr *ctrlr, uint32_t q_idx)
{
	int ret;
	int req_idx;
	uint16_t last_idx;
	uint64_t chunck_len;
	struct vhost_blk_queue *blk_vq;
	struct rte_vhost_vring *vq;
	struct vhost_blk_task *task;

	blk_vq = &ctrlr->bdev->queues[q_idx];
	vq = &blk_vq->vq;

	task = rte_zmalloc(NULL, sizeof(*task), 0);
	assert(task != NULL);
	task->ctrlr = ctrlr;
	task->bdev = ctrlr->bdev;
	task->vq = vq;

	while (vq->avail->idx != blk_vq->last_avail_idx) {
		last_idx = blk_vq->last_avail_idx & (vq->size - 1);
		req_idx = vq->avail->ring[last_idx];
		task->req_idx = req_idx;
		task->desc_split = &task->vq->desc[task->req_idx];
		task->iovs_cnt = 0;
		task->data_len = 0;
		task->req = NULL;
		task->status = NULL;

		rte_vhost_set_inflight_desc_split(ctrlr->bdev->vid, q_idx,
							task->req_idx);

		/* does not support indirect descriptors */
		assert((task->desc_split->flags & VRING_DESC_F_INDIRECT) == 0);

		chunck_len = task->desc_split->len;
		task->req = (void *)(uintptr_t)gpa_to_vva(task->bdev->vid,
				task->desc_split->addr, &chunck_len);
		if (!task->req || chunck_len != task->desc_split->len) {
			fprintf(stderr, "failed to translate desc address.\n");
			rte_free(task);
			return;
		}

		task->desc_split = descriptor_get_next_split(task->vq->desc,
							     task->desc_split);
		if (!descriptor_has_next_split(task->desc_split)) {
			task->dxfer_dir = BLK_DIR_NONE;
			chunck_len = task->desc_split->len;
			task->status = (void *)(uintptr_t)
					      gpa_to_vva(task->bdev->vid,
							 task->desc_split->addr,
							 &chunck_len);
			if (!task->status ||
				chunck_len != task->desc_split->len) {
				fprintf(stderr,
					"failed to translate desc address.\n");
				rte_free(task);
				return;
			}
		} else {
			task->readtype =
				descriptor_is_wr_split(task->desc_split);
			vhost_process_payload_chain_split(task);
		}
		blk_vq->last_avail_idx++;

		ret = vhost_bdev_process_blk_commands(ctrlr->bdev, task);
		if (ret) {
			/* invalid response */
			*task->status = VIRTIO_BLK_S_IOERR;
		} else {
			/* successfully */
			*task->status = VIRTIO_BLK_S_OK;
		}

		submit_completion_split(task, ctrlr->bdev->vid, q_idx);
	}

	rte_free(task);
}

static void *
ctrlr_worker(void *arg)
{
	struct vhost_blk_ctrlr *ctrlr = (struct vhost_blk_ctrlr *)arg;
	struct vhost_blk_queue *blk_vq;
	struct rte_vhost_ring_inflight *inflight_vq;
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

	for (i = 0; i < NUM_OF_BLK_QUEUES; i++) {
		blk_vq = &ctrlr->bdev->queues[i];
		inflight_vq = &blk_vq->inflight_vq;
		if (inflight_vq->resubmit_inflight != NULL &&
		    inflight_vq->resubmit_inflight->resubmit_num != 0) {
			if (ctrlr->packed_ring)
				submit_inflight_vq_packed(ctrlr, i);
			else
				submit_inflight_vq_split(ctrlr, i);
		}
	}

	while (!g_should_stop && ctrlr->bdev != NULL) {
		for (i = 0; i < NUM_OF_BLK_QUEUES; i++) {
			if (ctrlr->packed_ring)
				process_requestq_packed(ctrlr, i);
			else
				process_requestq_split(ctrlr, i);
		}
	}

	g_should_stop = 2;
	fprintf(stdout, "Ctrlr Worker Thread Exiting\n");
	sem_post(&exit_sem);
	return NULL;
}

static int
new_device(int vid)
{
	struct vhost_blk_ctrlr *ctrlr;
	struct vhost_blk_queue *blk_vq;
	struct rte_vhost_vring *vq;
	uint64_t features;
	pthread_t tid;
	int i, ret;

	ctrlr = vhost_blk_ctrlr_find(dev_pathname);
	if (!ctrlr) {
		fprintf(stderr, "Controller is not ready\n");
		return -1;
	}

	if (ctrlr->started)
		return 0;

	ctrlr->bdev->vid = vid;
	ret = rte_vhost_get_negotiated_features(vid, &features);
	if (ret) {
		fprintf(stderr, "failed to get the negotiated features\n");
		return -1;
	}
	ctrlr->packed_ring = !!(features & (1ULL << VIRTIO_F_RING_PACKED));

	ret = rte_vhost_get_mem_table(vid, &ctrlr->mem);
	if (ret)
		fprintf(stderr, "Get Controller memory region failed\n");
	assert(ctrlr->mem != NULL);

	/* Disable Notifications and init last idx */
	for (i = 0; i < NUM_OF_BLK_QUEUES; i++) {
		blk_vq = &ctrlr->bdev->queues[i];
		vq = &blk_vq->vq;

		ret = rte_vhost_get_vhost_vring(ctrlr->bdev->vid, i, vq);
		assert(ret == 0);

		ret = rte_vhost_get_vring_base(ctrlr->bdev->vid, i,
					       &blk_vq->last_avail_idx,
					       &blk_vq->last_used_idx);
		assert(ret == 0);

		ret = rte_vhost_get_vhost_ring_inflight(ctrlr->bdev->vid, i,
							&blk_vq->inflight_vq);
		assert(ret == 0);

		if (ctrlr->packed_ring) {
			/* for the reconnection */
			ret = rte_vhost_get_vring_base_from_inflight(
				ctrlr->bdev->vid, i,
				&blk_vq->last_avail_idx,
				&blk_vq->last_used_idx);
			assert(ret == 0);

			blk_vq->avail_wrap_counter = blk_vq->last_avail_idx &
				(1 << 15);
			blk_vq->last_avail_idx = blk_vq->last_avail_idx &
				0x7fff;
			blk_vq->used_wrap_counter = blk_vq->last_used_idx &
				(1 << 15);
			blk_vq->last_used_idx = blk_vq->last_used_idx &
				0x7fff;
		}

		rte_vhost_enable_guest_notification(vid, i, 0);
	}

	/* start polling vring */
	g_should_stop = 0;
	fprintf(stdout, "New Device %s, Device ID %d\n", dev_pathname, vid);
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
	struct vhost_blk_queue *blk_vq;
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

	g_should_stop = 1;
	while (g_should_stop != 2)
		;

	for (i = 0; i < NUM_OF_BLK_QUEUES; i++) {
		blk_vq = &ctrlr->bdev->queues[i];
		if (ctrlr->packed_ring) {
			blk_vq->last_avail_idx |= (blk_vq->avail_wrap_counter <<
				15);
			blk_vq->last_used_idx |= (blk_vq->used_wrap_counter <<
				15);
		}
		rte_vhost_set_vring_base(ctrlr->bdev->vid, i,
					 blk_vq->last_avail_idx,
					 blk_vq->last_used_idx);
	}

	free(ctrlr->mem);

	ctrlr->started = 0;
	sem_wait(&exit_sem);
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

	strncpy(bdev->name, bdev_name, sizeof(bdev->name));
	strncpy(bdev->product_name, bdev_serial, sizeof(bdev->product_name));
	bdev->blocklen = blk_size;
	bdev->blockcnt = blk_cnt;
	bdev->write_cache = wce_enable;

	fprintf(stdout, "blocklen=%d, blockcnt=%"PRIx64"\n", bdev->blocklen,
		bdev->blockcnt);

	/* use memory as disk storage space */
	bdev->data = rte_zmalloc(NULL, blk_cnt * blk_size, 0);
	if (!bdev->data) {
		fprintf(stderr, "no enough reserved huge memory for disk\n");
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
		fprintf(stderr, "socket %s already exists\n", dev_pathname);
		return NULL;
	}

	ret = rte_vhost_driver_set_features(dev_pathname, VHOST_BLK_FEATURES);
	if (ret != 0) {
		fprintf(stderr, "Set vhost driver features failed\n");
		rte_vhost_driver_unregister(dev_pathname);
		return NULL;
	}

	/* set proper features */
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
signal_handler(__rte_unused int signum)
{
	struct vhost_blk_ctrlr *ctrlr;

	unlink(dev_pathname);

	if (g_should_stop != -1) {
		g_should_stop = 1;
		while (g_should_stop != 2)
			;
	}

	ctrlr = vhost_blk_ctrlr_find(dev_pathname);
	if (ctrlr != NULL) {
		if (ctrlr->bdev != NULL) {
			rte_free(ctrlr->bdev->data);
			rte_free(ctrlr->bdev);
		}
		rte_free(ctrlr);
	}

	rte_vhost_driver_unregister(dev_pathname);
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

	g_vhost_ctrlr = vhost_blk_ctrlr_construct("vhost.socket");
	if (g_vhost_ctrlr == NULL) {
		fprintf(stderr, "Construct vhost blk controller failed\n");
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
