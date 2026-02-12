/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2024 ZTE Corporation
 */

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <bus_pci_driver.h>
#include <rte_atomic.h>
#include <rte_common.h>
#include <rte_dev.h>
#include <rte_eal_paging.h>
#include <rte_errno.h>
#include <rte_lcore.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_memzone.h>
#include <rte_pci.h>
#include <rte_rawdev.h>
#include <rte_rawdev_pmd.h>
#include <rte_spinlock.h>
#include <rte_branch_prediction.h>

#include "gdtc_rawdev.h"

/*
 * User define:
 * ep_id-bit[15:12] vfunc_num-bit[11:4] func_num-bit[3:1] vfunc_active-bit0
 * host ep_id:5~8   zf ep_id:9
 */
#define ZXDH_GDMA_ZF_USER                       0x9000      /* ep4 pf0 */
#define ZXDH_GDMA_PF_NUM_SHIFT                  1
#define ZXDH_GDMA_VF_NUM_SHIFT                  4
#define ZXDH_GDMA_EP_ID_SHIFT                   12
#define ZXDH_GDMA_VF_EN                         1
#define ZXDH_GDMA_EPID_OFFSET                   5

/* Register offset */
#define ZXDH_GDMA_BASE_OFFSET                   0x100000
#define ZXDH_GDMA_EXT_ADDR_OFFSET               0x218
#define ZXDH_GDMA_SAR_LOW_OFFSET                0x200
#define ZXDH_GDMA_DAR_LOW_OFFSET                0x204
#define ZXDH_GDMA_SAR_HIGH_OFFSET               0x234
#define ZXDH_GDMA_DAR_HIGH_OFFSET               0x238
#define ZXDH_GDMA_XFERSIZE_OFFSET               0x208
#define ZXDH_GDMA_CONTROL_OFFSET                0x230
#define ZXDH_GDMA_TC_STATUS_OFFSET              0x0
#define ZXDH_GDMA_STATUS_CLEAN_OFFSET           0x80
#define ZXDH_GDMA_LLI_L_OFFSET                  0x21c
#define ZXDH_GDMA_LLI_H_OFFSET                  0x220
#define ZXDH_GDMA_CHAN_CONTINUE_OFFSET          0x224
#define ZXDH_GDMA_TC_CNT_OFFSET                 0x23c
#define ZXDH_GDMA_LLI_USER_OFFSET               0x228

/* Control register */
#define ZXDH_GDMA_CHAN_ENABLE                   0x1
#define ZXDH_GDMA_CHAN_DISABLE                  0
#define ZXDH_GDMA_SOFT_CHAN                     0x2
#define ZXDH_GDMA_TC_INTR_ENABLE                0x10
#define ZXDH_GDMA_ALL_INTR_ENABLE               0x30
#define ZXDH_GDMA_SBS_SHIFT                     6           /* src burst size */
#define ZXDH_GDMA_SBL_SHIFT                     9           /* src burst length */
#define ZXDH_GDMA_DBS_SHIFT                     13          /* dest burst size */
#define ZXDH_GDMA_BURST_SIZE_MIN                0x1         /* 1 byte */
#define ZXDH_GDMA_BURST_SIZE_MEDIUM             0x4         /* 4 word */
#define ZXDH_GDMA_BURST_SIZE_MAX                0x6         /* 16 word */
#define ZXDH_GDMA_DEFAULT_BURST_LEN             0xf         /* 16 beats */
#define ZXDH_GDMA_TC_CNT_ENABLE                 RTE_BIT32(27)
#define ZXDH_GDMA_CHAN_FORCE_CLOSE              RTE_BIT32(31)

/* TC count & Error interrupt status register */
#define ZXDH_GDMA_SRC_LLI_ERR                   RTE_BIT32(16)
#define ZXDH_GDMA_SRC_DATA_ERR                  RTE_BIT32(17)
#define ZXDH_GDMA_DST_ADDR_ERR                  RTE_BIT32(18)
#define ZXDH_GDMA_ERR_STATUS                    RTE_BIT32(19)
#define ZXDH_GDMA_ERR_INTR_ENABLE               RTE_BIT32(20)
#define ZXDH_GDMA_TC_CNT_CLEAN                  (1)

#define ZXDH_GDMA_CHAN_SHIFT                    0x80
#define ZXDH_GDMA_LINK_END_NODE                 RTE_BIT32(30)
#define ZXDH_GDMA_CHAN_CONTINUE                 (1)

#define LOW32_MASK                              0xffffffff
#define LOW16_MASK                              0xffff

#define ZXDH_GDMA_TC_CNT_MAX                    0x10000

#define IDX_TO_ADDR(addr, idx, t) \
	((t)((uintptr_t)(addr) + (idx) * sizeof(struct zxdh_gdma_buff_desc)))

static int zxdh_gdma_queue_init(struct rte_rawdev *dev, uint16_t queue_id);
static int zxdh_gdma_queue_free(struct rte_rawdev *dev, uint16_t queue_id);

char zxdh_gdma_driver_name[] = "rawdev_zxdh_gdma";
char dev_name[] = "zxdh_gdma";

static inline struct zxdh_gdma_rawdev *
zxdh_gdma_rawdev_get_priv(const struct rte_rawdev *rawdev)
{
	return rawdev->dev_private;
}

static inline struct zxdh_gdma_queue *
zxdh_gdma_get_queue(struct rte_rawdev *dev, uint16_t queue_id)
{
	struct zxdh_gdma_rawdev *gdmadev = zxdh_gdma_rawdev_get_priv(dev);

	if (queue_id >= ZXDH_GDMA_TOTAL_CHAN_NUM) {
		ZXDH_PMD_LOG(ERR, "queue id %d is invalid", queue_id);
		return NULL;
	}

	return &(gdmadev->vqs[queue_id]);
}

static uint32_t
zxdh_gdma_read_reg(struct rte_rawdev *dev, uint16_t queue_id, uint32_t offset)
{
	struct zxdh_gdma_rawdev *gdmadev = zxdh_gdma_rawdev_get_priv(dev);
	uint32_t addr = 0;
	uint32_t val = 0;

	addr = offset + queue_id * ZXDH_GDMA_CHAN_SHIFT;
	val = *(uint32_t *)(gdmadev->base_addr + addr);

	return val;
}

static void
zxdh_gdma_write_reg(struct rte_rawdev *dev, uint16_t queue_id, uint32_t offset, uint32_t val)
{
	struct zxdh_gdma_rawdev *gdmadev = zxdh_gdma_rawdev_get_priv(dev);
	uint32_t addr = 0;

	addr = offset + queue_id * ZXDH_GDMA_CHAN_SHIFT;
	*(uint32_t *)(gdmadev->base_addr + addr) = val;
}

static int
zxdh_gdma_rawdev_info_get(struct rte_rawdev *dev,
		__rte_unused rte_rawdev_obj_t dev_info,
		__rte_unused size_t dev_info_size)
{
	if (dev == NULL)
		return -EINVAL;

	return 0;
}

static int
zxdh_gdma_rawdev_configure(const struct rte_rawdev *dev,
		rte_rawdev_obj_t config,
		size_t config_size)
{
	struct zxdh_gdma_config *gdma_config = NULL;

	if ((dev == NULL) ||
		(config == NULL) ||
		(config_size != sizeof(struct zxdh_gdma_config)))
		return -EINVAL;

	gdma_config = (struct zxdh_gdma_config *)config;
	if (gdma_config->max_vqs > ZXDH_GDMA_TOTAL_CHAN_NUM) {
		ZXDH_PMD_LOG(ERR, "gdma supports up to %d queues", ZXDH_GDMA_TOTAL_CHAN_NUM);
		return -EINVAL;
	}

	return 0;
}

static int
zxdh_gdma_rawdev_start(struct rte_rawdev *dev)
{
	struct zxdh_gdma_rawdev *gdmadev = NULL;

	if (dev == NULL)
		return -EINVAL;

	gdmadev = zxdh_gdma_rawdev_get_priv(dev);
	gdmadev->device_state = ZXDH_GDMA_DEV_RUNNING;

	return 0;
}

static void
zxdh_gdma_rawdev_stop(struct rte_rawdev *dev)
{
	struct zxdh_gdma_rawdev *gdmadev = NULL;

	if (dev == NULL)
		return;

	gdmadev = zxdh_gdma_rawdev_get_priv(dev);
	gdmadev->device_state = ZXDH_GDMA_DEV_STOPPED;
}

static int
zxdh_gdma_rawdev_reset(struct rte_rawdev *dev)
{
	if (dev == NULL)
		return -EINVAL;

	return 0;
}

static int
zxdh_gdma_rawdev_close(struct rte_rawdev *dev)
{
	struct zxdh_gdma_rawdev *gdmadev = NULL;
	struct zxdh_gdma_queue *queue = NULL;
	uint16_t queue_id = 0;

	if (dev == NULL)
		return -EINVAL;

	for (queue_id = 0; queue_id < ZXDH_GDMA_TOTAL_CHAN_NUM; queue_id++) {
		queue = zxdh_gdma_get_queue(dev, queue_id);
		if ((queue == NULL) || (queue->enable == 0))
			continue;

		zxdh_gdma_queue_free(dev, queue_id);
	}
	gdmadev = zxdh_gdma_rawdev_get_priv(dev);
	gdmadev->device_state = ZXDH_GDMA_DEV_STOPPED;

	return 0;
}

static int
zxdh_gdma_rawdev_queue_setup(struct rte_rawdev *dev,
		uint16_t queue_id,
		rte_rawdev_obj_t queue_conf,
		size_t conf_size)
{
	struct zxdh_gdma_rawdev *gdmadev = NULL;
	struct zxdh_gdma_queue *queue = NULL;
	struct zxdh_gdma_queue_config *qconfig = NULL;
	struct zxdh_gdma_rbp *rbp = NULL;
	uint16_t i = 0;
	uint8_t is_txq = 0;
	uint32_t src_user = 0;
	uint32_t dst_user = 0;

	if (dev == NULL)
		return -EINVAL;

	if ((queue_conf == NULL) || (conf_size != sizeof(struct zxdh_gdma_queue_config)))
		return -EINVAL;

	gdmadev = zxdh_gdma_rawdev_get_priv(dev);
	qconfig = (struct zxdh_gdma_queue_config *)queue_conf;

	for (i = 0; i < ZXDH_GDMA_TOTAL_CHAN_NUM; i++) {
		if (gdmadev->vqs[i].enable == 0)
			break;
	}
	if (i >= ZXDH_GDMA_TOTAL_CHAN_NUM) {
		ZXDH_PMD_LOG(ERR, "Failed to setup queue, no avail queues");
		return -1;
	}
	queue_id = i;
	if (zxdh_gdma_queue_init(dev, queue_id) != 0) {
		ZXDH_PMD_LOG(ERR, "Failed to init queue");
		return -1;
	}
	queue = &(gdmadev->vqs[queue_id]);

	rbp = qconfig->rbp;
	if ((rbp->srbp != 0) && (rbp->drbp == 0)) {
		is_txq = 0;
		dst_user = ZXDH_GDMA_ZF_USER;
		src_user = ((rbp->spfid << ZXDH_GDMA_PF_NUM_SHIFT) |
				((rbp->sportid + ZXDH_GDMA_EPID_OFFSET) << ZXDH_GDMA_EP_ID_SHIFT));

		if (rbp->svfid != 0)
			src_user |= (ZXDH_GDMA_VF_EN |
					((rbp->svfid - 1) << ZXDH_GDMA_VF_NUM_SHIFT));

		ZXDH_PMD_LOG(DEBUG, "rxq->qidx:%d setup src_user(ep:%d pf:%d vf:%d) success",
				queue_id, (uint8_t)rbp->sportid, (uint8_t)rbp->spfid,
				(uint8_t)rbp->svfid);
	} else if ((rbp->srbp == 0) && (rbp->drbp != 0)) {
		is_txq = 1;
		src_user = ZXDH_GDMA_ZF_USER;
		dst_user = ((rbp->dpfid << ZXDH_GDMA_PF_NUM_SHIFT) |
				((rbp->dportid + ZXDH_GDMA_EPID_OFFSET) << ZXDH_GDMA_EP_ID_SHIFT));

		if (rbp->dvfid != 0)
			dst_user |= (ZXDH_GDMA_VF_EN |
					((rbp->dvfid - 1) << ZXDH_GDMA_VF_NUM_SHIFT));

		ZXDH_PMD_LOG(DEBUG, "txq->qidx:%d setup dst_user(ep:%d pf:%d vf:%d) success",
				queue_id, (uint8_t)rbp->dportid, (uint8_t)rbp->dpfid,
				(uint8_t)rbp->dvfid);
	} else {
		ZXDH_PMD_LOG(ERR, "Failed to setup queue, srbp/drbp is invalid");
		return -EINVAL;
	}
	queue->is_txq = is_txq;

	/* setup queue user info */
	queue->user = (src_user & LOW16_MASK) | (dst_user << 16);

	zxdh_gdma_write_reg(dev, queue_id, ZXDH_GDMA_EXT_ADDR_OFFSET, queue->user);
	gdmadev->used_num++;

	return queue_id;
}

static int
zxdh_gdma_rawdev_queue_release(struct rte_rawdev *dev, uint16_t queue_id)
{
	struct zxdh_gdma_queue *queue = NULL;

	if (dev == NULL)
		return -EINVAL;

	queue = zxdh_gdma_get_queue(dev, queue_id);
	if ((queue == NULL) || (queue->enable == 0))
		return -EINVAL;

	zxdh_gdma_queue_free(dev, queue_id);

	return 0;
}

static int
zxdh_gdma_rawdev_get_attr(struct rte_rawdev *dev,
				__rte_unused const char *attr_name,
				uint64_t *attr_value)
{
	struct zxdh_gdma_rawdev *gdmadev = NULL;
	struct zxdh_gdma_attr *gdma_attr = NULL;

	if ((dev == NULL) || (attr_value == NULL))
		return -EINVAL;

	gdmadev   = zxdh_gdma_rawdev_get_priv(dev);
	gdma_attr = (struct zxdh_gdma_attr *)attr_value;
	gdma_attr->num_hw_queues = gdmadev->used_num;

	return 0;
}

static inline void
zxdh_gdma_control_cal(uint32_t *val, uint8_t tc_enable)
{
	*val = (ZXDH_GDMA_CHAN_ENABLE |
			ZXDH_GDMA_SOFT_CHAN |
			(ZXDH_GDMA_DEFAULT_BURST_LEN << ZXDH_GDMA_SBL_SHIFT) |
			(ZXDH_GDMA_BURST_SIZE_MAX << ZXDH_GDMA_SBS_SHIFT) |
			(ZXDH_GDMA_BURST_SIZE_MAX << ZXDH_GDMA_DBS_SHIFT));

	if (tc_enable != 0)
		*val |= ZXDH_GDMA_TC_CNT_ENABLE;
}

static inline uint32_t
zxdh_gdma_user_get(struct zxdh_gdma_queue *queue, struct zxdh_gdma_job *job)
{
	uint32_t src_user = 0;
	uint32_t dst_user = 0;

	if ((job->flags & ZXDH_GDMA_JOB_DIR_MASK) == 0) {
		ZXDH_PMD_LOG(DEBUG, "job flags:0x%x default user:0x%x",
				job->flags, queue->user);
		return queue->user;
	} else if ((job->flags & ZXDH_GDMA_JOB_DIR_TX) != 0) {
		src_user = ZXDH_GDMA_ZF_USER;
		dst_user = ((job->pf_id << ZXDH_GDMA_PF_NUM_SHIFT) |
				((job->ep_id + ZXDH_GDMA_EPID_OFFSET) << ZXDH_GDMA_EP_ID_SHIFT));

		if (job->vf_id != 0)
			dst_user |= (ZXDH_GDMA_VF_EN |
					((job->vf_id - 1) << ZXDH_GDMA_VF_NUM_SHIFT));
	} else {
		dst_user = ZXDH_GDMA_ZF_USER;
		src_user = ((job->pf_id << ZXDH_GDMA_PF_NUM_SHIFT) |
				((job->ep_id + ZXDH_GDMA_EPID_OFFSET) << ZXDH_GDMA_EP_ID_SHIFT));

		if (job->vf_id != 0)
			src_user |= (ZXDH_GDMA_VF_EN |
					((job->vf_id - 1) << ZXDH_GDMA_VF_NUM_SHIFT));
	}
	ZXDH_PMD_LOG(DEBUG, "job flags:0x%x ep_id:%u, pf_id:%u, vf_id:%u, user:0x%x",
			job->flags, job->ep_id, job->pf_id, job->vf_id,
			(src_user & LOW16_MASK) | (dst_user << 16));

	return (src_user & LOW16_MASK) | (dst_user << 16);
}

static inline void
zxdh_gdma_fill_bd(struct zxdh_gdma_queue *queue, struct zxdh_gdma_job *job)
{
	struct zxdh_gdma_buff_desc *bd = NULL;
	uint32_t val = 0;
	uint64_t next_bd_addr = 0;
	uint16_t avail_idx = 0;

	avail_idx = queue->ring.avail_idx;
	bd = &(queue->ring.desc[avail_idx]);
	memset(bd, 0, sizeof(struct zxdh_gdma_buff_desc));

	/* data bd */
	if (job != NULL) {
		zxdh_gdma_control_cal(&val, 1);
		next_bd_addr   = IDX_TO_ADDR(queue->ring.ring_mem,
				(avail_idx + 1) % ZXDH_GDMA_RING_SIZE, uint64_t);
		bd->SrcAddr_L  = job->src & LOW32_MASK;
		bd->DstAddr_L  = job->dest & LOW32_MASK;
		bd->SrcAddr_H  = (job->src >> 32) & LOW32_MASK;
		bd->DstAddr_H  = (job->dest >> 32) & LOW32_MASK;
		bd->Xpara      = job->len;
		bd->ExtAddr    = zxdh_gdma_user_get(queue, job);
		bd->LLI_Addr_L = (next_bd_addr >> 6) & LOW32_MASK;
		bd->LLI_Addr_H = next_bd_addr >> 38;
		bd->LLI_User   = ZXDH_GDMA_ZF_USER;
		bd->Control    = val;
	} else {
		zxdh_gdma_control_cal(&val, 0);
		next_bd_addr   = IDX_TO_ADDR(queue->ring.ring_mem, avail_idx, uint64_t);
		bd->ExtAddr    = queue->user;
		bd->LLI_User   = ZXDH_GDMA_ZF_USER;
		bd->Control    = val;
		bd->LLI_Addr_L = (next_bd_addr >> 6) & LOW32_MASK;
		bd->LLI_Addr_H = (next_bd_addr >> 38) | ZXDH_GDMA_LINK_END_NODE;
		if (queue->flag != 0) {
			bd = IDX_TO_ADDR(queue->ring.desc,
					queue->ring.last_avail_idx,
					struct zxdh_gdma_buff_desc*);
			next_bd_addr = IDX_TO_ADDR(queue->ring.ring_mem,
					(queue->ring.last_avail_idx + 1) % ZXDH_GDMA_RING_SIZE,
					uint64_t);
			bd->LLI_Addr_L  = (next_bd_addr >> 6) & LOW32_MASK;
			bd->LLI_Addr_H  = next_bd_addr >> 38;
			rte_wmb();
			bd->LLI_Addr_H &= ~ZXDH_GDMA_LINK_END_NODE;
		}
		/* Record the index of empty bd for dynamic chaining */
		queue->ring.last_avail_idx = avail_idx;
	}

	if (++avail_idx >= ZXDH_GDMA_RING_SIZE)
		avail_idx -= ZXDH_GDMA_RING_SIZE;

	queue->ring.avail_idx = avail_idx;
}

static int
zxdh_gdma_rawdev_enqueue_bufs(struct rte_rawdev *dev,
				__rte_unused struct rte_rawdev_buf **buffers,
				uint32_t count,
				rte_rawdev_obj_t context)
{
	struct zxdh_gdma_rawdev *gdmadev = NULL;
	struct zxdh_gdma_queue *queue = NULL;
	struct zxdh_gdma_enqdeq *e_context = NULL;
	struct zxdh_gdma_job *job = NULL;
	uint16_t queue_id = 0;
	uint32_t val = 0;
	uint16_t i = 0;
	uint16_t free_cnt = 0;

	if (dev == NULL)
		return -EINVAL;

	if (unlikely((count < 1) || (context == NULL)))
		return -EINVAL;

	gdmadev = zxdh_gdma_rawdev_get_priv(dev);
	if (gdmadev->device_state == ZXDH_GDMA_DEV_STOPPED) {
		ZXDH_PMD_LOG(ERR, "gdma dev is stop");
		return 0;
	}

	e_context = (struct zxdh_gdma_enqdeq *)context;
	queue_id = e_context->vq_id;
	queue = zxdh_gdma_get_queue(dev, queue_id);
	if ((queue == NULL) || (queue->enable == 0))
		return -EINVAL;

	free_cnt = queue->sw_ring.free_cnt;
	if (free_cnt == 0) {
		ZXDH_PMD_LOG(ERR, "queue %u is full, enq_idx:%u deq_idx:%u used_idx:%u",
				queue_id, queue->sw_ring.enq_idx,
				queue->sw_ring.deq_idx, queue->sw_ring.used_idx);
		return 0;
	} else if (free_cnt < count) {
		ZXDH_PMD_LOG(DEBUG, "job num %u > free_cnt, change to %u", count, free_cnt);
		count = free_cnt;
	}

	rte_spinlock_lock(&queue->enqueue_lock);

	/* Build bd list, the last bd is empty bd */
	for (i = 0; i < count; i++) {
		job = e_context->job[i];
		zxdh_gdma_fill_bd(queue, job);
	}
	zxdh_gdma_fill_bd(queue, NULL);

	if (unlikely(queue->flag == 0)) {
		zxdh_gdma_write_reg(dev, queue_id, ZXDH_GDMA_LLI_L_OFFSET,
				(queue->ring.ring_mem >> 6) & LOW32_MASK);
		zxdh_gdma_write_reg(dev, queue_id, ZXDH_GDMA_LLI_H_OFFSET,
				queue->ring.ring_mem >> 38);
		/* Start hardware handling */
		zxdh_gdma_write_reg(dev, queue_id, ZXDH_GDMA_XFERSIZE_OFFSET, 0);
		zxdh_gdma_control_cal(&val, 0);
		zxdh_gdma_write_reg(dev, queue_id, ZXDH_GDMA_CONTROL_OFFSET, val);
		queue->flag = 1;
	} else {
		val = ZXDH_GDMA_CHAN_CONTINUE;
		zxdh_gdma_write_reg(dev, queue->vq_id, ZXDH_GDMA_CHAN_CONTINUE_OFFSET, val);
	}

    /* job enqueue */
	for (i = 0; i < count; i++) {
		queue->sw_ring.job[queue->sw_ring.enq_idx] = e_context->job[i];
		if (++queue->sw_ring.enq_idx >= queue->queue_size)
			queue->sw_ring.enq_idx -= queue->queue_size;

		free_cnt--;
	}
	queue->sw_ring.free_cnt = free_cnt;
	queue->sw_ring.pend_cnt += count;
	rte_spinlock_unlock(&queue->enqueue_lock);

	return count;
}

static inline void
zxdh_gdma_used_idx_update(struct zxdh_gdma_queue *queue, uint16_t cnt, uint8_t data_bd_err)
{
	uint16_t idx = 0;

	if (queue->sw_ring.used_idx + cnt < queue->queue_size)
		queue->sw_ring.used_idx += cnt;
	else
		queue->sw_ring.used_idx = queue->sw_ring.used_idx + cnt - queue->queue_size;

	if (data_bd_err == 1) {
		/* Update job status, the last job status is error */
		if (queue->sw_ring.used_idx == 0)
			idx = queue->queue_size - 1;
		else
			idx = queue->sw_ring.used_idx - 1;

		queue->sw_ring.job[idx]->status = 1;
	}
}

static int
zxdh_gdma_rawdev_dequeue_bufs(struct rte_rawdev *dev,
		__rte_unused struct rte_rawdev_buf **buffers,
		uint32_t count,
		rte_rawdev_obj_t context)
{
	struct zxdh_gdma_queue *queue = NULL;
	struct zxdh_gdma_enqdeq *e_context = NULL;
	uint16_t queue_id = 0;
	uint32_t val = 0;
	uint16_t tc_cnt = 0;
	uint16_t diff_cnt = 0;
	uint16_t i = 0;
	uint16_t bd_idx = 0;
	uint64_t next_bd_addr = 0;
	uint8_t data_bd_err = 0;

	if ((dev == NULL) || (context == NULL))
		return -EINVAL;

	e_context = (struct zxdh_gdma_enqdeq *)context;
	queue_id = e_context->vq_id;
	queue = zxdh_gdma_get_queue(dev, queue_id);
	if ((queue == NULL) || (queue->enable == 0))
		return -EINVAL;

	if (queue->sw_ring.pend_cnt == 0)
		goto deq_job;

	/* Get data transmit count */
	val = zxdh_gdma_read_reg(dev, queue_id, ZXDH_GDMA_TC_CNT_OFFSET);
	tc_cnt = val & LOW16_MASK;
	if (tc_cnt >= queue->tc_cnt)
		diff_cnt = tc_cnt - queue->tc_cnt;
	else
		diff_cnt = tc_cnt + ZXDH_GDMA_TC_CNT_MAX - queue->tc_cnt;

	queue->tc_cnt = tc_cnt;

	/* Data transmit error, channel stopped */
	if ((val & ZXDH_GDMA_ERR_STATUS) != 0) {
		next_bd_addr  = zxdh_gdma_read_reg(dev, queue_id, ZXDH_GDMA_LLI_L_OFFSET);
		next_bd_addr |= ((uint64_t)zxdh_gdma_read_reg(dev, queue_id,
				ZXDH_GDMA_LLI_H_OFFSET) << 32);
		next_bd_addr  = next_bd_addr << 6;
		bd_idx = (next_bd_addr - queue->ring.ring_mem) / sizeof(struct zxdh_gdma_buff_desc);
		if ((val & ZXDH_GDMA_SRC_DATA_ERR) || (val & ZXDH_GDMA_DST_ADDR_ERR)) {
			diff_cnt++;
			data_bd_err = 1;
		}
		ZXDH_PMD_LOG(INFO, "queue%d is err(0x%x) next_bd_idx:%u ll_addr:0x%"PRIx64" def user:0x%x",
				queue_id, val, bd_idx, next_bd_addr, queue->user);

		ZXDH_PMD_LOG(INFO, "Clean up error status");
		val = ZXDH_GDMA_ERR_STATUS | ZXDH_GDMA_ERR_INTR_ENABLE;
		zxdh_gdma_write_reg(dev, queue_id, ZXDH_GDMA_TC_CNT_OFFSET, val);

		ZXDH_PMD_LOG(INFO, "Restart channel");
		zxdh_gdma_write_reg(dev, queue_id, ZXDH_GDMA_XFERSIZE_OFFSET, 0);
		zxdh_gdma_control_cal(&val, 0);
		zxdh_gdma_write_reg(dev, queue_id, ZXDH_GDMA_CONTROL_OFFSET, val);
	}

	if (diff_cnt != 0) {
		zxdh_gdma_used_idx_update(queue, diff_cnt, data_bd_err);
		queue->sw_ring.deq_cnt += diff_cnt;
		queue->sw_ring.pend_cnt -= diff_cnt;
	}

deq_job:
	if (queue->sw_ring.deq_cnt == 0)
		return 0;
	else if (queue->sw_ring.deq_cnt < count)
		count = queue->sw_ring.deq_cnt;

	queue->sw_ring.deq_cnt -= count;

	for (i = 0; i < count; i++) {
		e_context->job[i] = queue->sw_ring.job[queue->sw_ring.deq_idx];
		queue->sw_ring.job[queue->sw_ring.deq_idx] = NULL;
		if (++queue->sw_ring.deq_idx >= queue->queue_size)
			queue->sw_ring.deq_idx -= queue->queue_size;
	}
	queue->sw_ring.free_cnt += count;

	return count;
}

static const struct rte_rawdev_ops zxdh_gdma_rawdev_ops = {
	.dev_info_get = zxdh_gdma_rawdev_info_get,
	.dev_configure = zxdh_gdma_rawdev_configure,
	.dev_start = zxdh_gdma_rawdev_start,
	.dev_stop = zxdh_gdma_rawdev_stop,
	.dev_close = zxdh_gdma_rawdev_close,
	.dev_reset = zxdh_gdma_rawdev_reset,

	.queue_setup = zxdh_gdma_rawdev_queue_setup,
	.queue_release = zxdh_gdma_rawdev_queue_release,

	.attr_get = zxdh_gdma_rawdev_get_attr,

	.enqueue_bufs = zxdh_gdma_rawdev_enqueue_bufs,
	.dequeue_bufs = zxdh_gdma_rawdev_dequeue_bufs,
};

static int
zxdh_gdma_queue_init(struct rte_rawdev *dev, uint16_t queue_id)
{
	char name[RTE_MEMZONE_NAMESIZE];
	struct zxdh_gdma_queue *queue = NULL;
	const struct rte_memzone *mz = NULL;
	uint32_t size = 0;
	int ret = 0;

	queue = zxdh_gdma_get_queue(dev, queue_id);
	if (queue == NULL)
		return -EINVAL;

	queue->enable = 1;
	queue->vq_id  = queue_id;
	queue->flag   = 0;
	queue->tc_cnt = 0;

	/* Init sw_ring */
	queue->sw_ring.job = rte_calloc(NULL, queue->queue_size, sizeof(struct zxdh_gdma_job *), 0);
	if (queue->sw_ring.job == NULL) {
		ZXDH_PMD_LOG(ERR, "can not allocate sw_ring");
		ret = -ENOMEM;
		goto free_queue;
	}

	/* Cache up to size-1 job in the ring to prevent overwriting hardware prefetching */
	queue->sw_ring.free_cnt = queue->queue_size - 1;
	queue->sw_ring.deq_cnt  = 0;
	queue->sw_ring.pend_cnt = 0;
	queue->sw_ring.enq_idx  = 0;
	queue->sw_ring.deq_idx  = 0;
	queue->sw_ring.used_idx = 0;

	/* Init ring */
	snprintf(name, RTE_MEMZONE_NAMESIZE, "gdma_vq%d_ring", queue_id);
	size = ZXDH_GDMA_RING_SIZE * sizeof(struct zxdh_gdma_buff_desc);
	mz = rte_memzone_reserve_aligned(name, size, rte_socket_id(),
			RTE_MEMZONE_IOVA_CONTIG, size);
	if (mz == NULL) {
		if (rte_errno == EEXIST)
			mz = rte_memzone_lookup(name);
		if (mz == NULL) {
			ZXDH_PMD_LOG(ERR, "can not allocate ring %s", name);
			ret = -ENOMEM;
			goto free_queue;
		}
	}
	memset(mz->addr, 0, size);
	queue->ring.ring_mz   = mz;
	queue->ring.desc      = (struct zxdh_gdma_buff_desc *)(mz->addr);
	queue->ring.ring_mem  = mz->iova;
	queue->ring.avail_idx = 0;
	ZXDH_PMD_LOG(INFO, "queue%u ring phy addr:0x%"PRIx64" virt addr:%p",
			queue_id, mz->iova, mz->addr);

	/* Configure the hardware channel to the initial state */
	zxdh_gdma_write_reg(dev, queue_id, ZXDH_GDMA_CONTROL_OFFSET,
			ZXDH_GDMA_CHAN_FORCE_CLOSE);
	zxdh_gdma_write_reg(dev, queue_id, ZXDH_GDMA_TC_CNT_OFFSET,
			ZXDH_GDMA_ERR_INTR_ENABLE | ZXDH_GDMA_ERR_STATUS | ZXDH_GDMA_TC_CNT_CLEAN);
	zxdh_gdma_write_reg(dev, queue_id, ZXDH_GDMA_LLI_USER_OFFSET,
			ZXDH_GDMA_ZF_USER);

	return 0;

free_queue:
	zxdh_gdma_queue_free(dev, queue_id);
	return ret;
}

static int
zxdh_gdma_queue_free(struct rte_rawdev *dev, uint16_t queue_id)
{
	struct zxdh_gdma_rawdev *gdmadev = NULL;
	struct zxdh_gdma_queue *queue = NULL;
	uint32_t val = 0;

	queue = zxdh_gdma_get_queue(dev, queue_id);
	if (queue == NULL)
		return -EINVAL;

	gdmadev = zxdh_gdma_rawdev_get_priv(dev);
	gdmadev->used_num--;

	/* disable gdma channel */
	val = ZXDH_GDMA_CHAN_FORCE_CLOSE;
	zxdh_gdma_write_reg(dev, queue_id, ZXDH_GDMA_CONTROL_OFFSET, val);

	queue->enable           = 0;
	queue->is_txq           = 0;
	queue->flag             = 0;
	queue->user             = 0;
	queue->tc_cnt           = 0;
	queue->ring.avail_idx   = 0;
	queue->sw_ring.free_cnt = 0;
	queue->sw_ring.deq_cnt  = 0;
	queue->sw_ring.pend_cnt = 0;
	queue->sw_ring.enq_idx  = 0;
	queue->sw_ring.deq_idx  = 0;
	queue->sw_ring.used_idx = 0;
	rte_free(queue->sw_ring.job);
	rte_memzone_free(queue->ring.ring_mz);

	return 0;
}

static int
zxdh_gdma_map_resource(struct rte_pci_device *dev)
{
	int fd = -1;
	char devname[PATH_MAX];
	void *mapaddr = NULL;
	struct rte_pci_addr *loc;

	loc = &dev->addr;
	snprintf(devname, sizeof(devname), "%s/" PCI_PRI_FMT "/resource0",
		rte_pci_get_sysfs_path(),
		loc->domain, loc->bus, loc->devid,
		loc->function);

		fd = open(devname, O_RDWR);
		if (fd < 0) {
			ZXDH_PMD_LOG(ERR, "Cannot open %s: %s", devname, strerror(errno));
			return -1;
		}

	/* Map the PCI memory resource of device */
	mapaddr = rte_mem_map(NULL, (size_t)dev->mem_resource[0].len,
			RTE_PROT_READ | RTE_PROT_WRITE,
			RTE_MAP_SHARED, fd, 0);
	if (mapaddr == NULL) {
		ZXDH_PMD_LOG(ERR, "cannot map resource(%d, 0x%zx): %s (%p)",
				fd, (size_t)dev->mem_resource[0].len,
				rte_strerror(rte_errno), mapaddr);
		close(fd);
		return -1;
	}

	close(fd);
	dev->mem_resource[0].addr = mapaddr;

	return 0;
}

static void
zxdh_gdma_unmap_resource(void *requested_addr, size_t size)
{
	if (requested_addr == NULL)
		return;

	/* Unmap the PCI memory resource of device */
	if (rte_mem_unmap(requested_addr, size))
		ZXDH_PMD_LOG(ERR, "cannot mem unmap(%p, %#zx): %s",
				requested_addr, size, rte_strerror(rte_errno));
	else
		ZXDH_PMD_LOG(DEBUG, "PCI memory unmapped at %p", requested_addr);
}

static int
zxdh_gdma_rawdev_probe(struct rte_pci_driver *pci_drv __rte_unused,
		struct rte_pci_device *pci_dev)
{
	struct rte_rawdev *dev = NULL;
	struct zxdh_gdma_rawdev *gdmadev = NULL;
	struct zxdh_gdma_queue *queue = NULL;
	uint8_t i = 0;
	int ret;

	if (pci_dev->mem_resource[0].phys_addr == 0) {
		ZXDH_PMD_LOG(ERR, "PCI bar0 resource is invalid");
		return -1;
	}

	ret = zxdh_gdma_map_resource(pci_dev);
	if (ret != 0) {
		ZXDH_PMD_LOG(ERR, "Failed to mmap pci device(%s)", pci_dev->name);
		return -1;
	}
	ZXDH_PMD_LOG(INFO, "%s bar0 0x%"PRIx64" mapped at %p",
			pci_dev->name, pci_dev->mem_resource[0].phys_addr,
			pci_dev->mem_resource[0].addr);

	dev = rte_rawdev_pmd_allocate(dev_name, sizeof(struct zxdh_gdma_rawdev), rte_socket_id());
	if (dev == NULL) {
		ZXDH_PMD_LOG(ERR, "Unable to allocate gdma rawdev");
		goto err_out;
	}
	ZXDH_PMD_LOG(INFO, "Init %s on NUMA node %d, dev_id is %d",
			dev_name, rte_socket_id(), dev->dev_id);

	dev->dev_ops = &zxdh_gdma_rawdev_ops;
	dev->device = &pci_dev->device;
	dev->driver_name = zxdh_gdma_driver_name;
	gdmadev = zxdh_gdma_rawdev_get_priv(dev);
	gdmadev->device_state = ZXDH_GDMA_DEV_STOPPED;
	gdmadev->rawdev = dev;
	gdmadev->queue_num = ZXDH_GDMA_TOTAL_CHAN_NUM;
	gdmadev->used_num = 0;
	gdmadev->base_addr = (uintptr_t)pci_dev->mem_resource[0].addr + ZXDH_GDMA_BASE_OFFSET;

	for (i = 0; i < ZXDH_GDMA_TOTAL_CHAN_NUM; i++) {
		queue = &(gdmadev->vqs[i]);
		queue->enable = 0;
		queue->queue_size = ZXDH_GDMA_QUEUE_SIZE;
		rte_spinlock_init(&(queue->enqueue_lock));
	}

	return 0;

err_out:
	zxdh_gdma_unmap_resource(pci_dev->mem_resource[0].addr,
			(size_t)pci_dev->mem_resource[0].len);
	return -1;
}

static int
zxdh_gdma_rawdev_remove(struct rte_pci_device *pci_dev)
{
	struct rte_rawdev *dev = NULL;
	int ret = 0;

	dev = rte_rawdev_pmd_get_named_dev(dev_name);
	if (dev == NULL)
		return -EINVAL;

	/* rte_rawdev_close is called by pmd_release */
	ret = rte_rawdev_pmd_release(dev);
	if (ret != 0) {
		ZXDH_PMD_LOG(ERR, "Device cleanup failed");
		return -1;
	}

	zxdh_gdma_unmap_resource(pci_dev->mem_resource[0].addr,
			(size_t)pci_dev->mem_resource[0].len);

	ZXDH_PMD_LOG(DEBUG, "rawdev %s remove done!", dev_name);

	return ret;
}

static const struct rte_pci_id zxdh_gdma_rawdev_map[] = {
	{ RTE_PCI_DEVICE(ZXDH_GDMA_VENDORID, ZXDH_GDMA_DEVICEID) },
	{ .vendor_id = 0, /* sentinel */ },
};

static struct rte_pci_driver zxdh_gdma_rawdev_pmd = {
	.id_table = zxdh_gdma_rawdev_map,
	.drv_flags = 0,
	.probe = zxdh_gdma_rawdev_probe,
	.remove = zxdh_gdma_rawdev_remove,
};

RTE_PMD_REGISTER_PCI(zxdh_gdma_rawdev_pci_driver, zxdh_gdma_rawdev_pmd);
RTE_PMD_REGISTER_PCI_TABLE(zxdh_gdma_rawdev_pci_driver, zxdh_gdma_rawdev_map);
RTE_LOG_REGISTER_DEFAULT(zxdh_gdma_rawdev_logtype, NOTICE);
