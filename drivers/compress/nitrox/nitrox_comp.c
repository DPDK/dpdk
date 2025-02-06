/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2024 Marvell.
 */

#include <rte_compressdev_pmd.h>
#include <rte_comp.h>
#include <rte_errno.h>
#include <rte_malloc.h>

#include "nitrox_comp.h"
#include "nitrox_device.h"
#include "nitrox_logs.h"
#include "nitrox_comp_reqmgr.h"
#include "nitrox_qp.h"

static const char nitrox_comp_drv_name[] = RTE_STR(COMPRESSDEV_NAME_NITROX_PMD);
static const struct rte_driver nitrox_rte_comp_drv = {
	.name = nitrox_comp_drv_name,
	.alias = nitrox_comp_drv_name
};

static int nitrox_comp_queue_pair_release(struct rte_compressdev *dev,
					  uint16_t qp_id);

static const struct rte_compressdev_capabilities
				nitrox_comp_pmd_capabilities[] = {
	{	.algo = RTE_COMP_ALGO_DEFLATE,
		.comp_feature_flags = RTE_COMP_FF_HUFFMAN_FIXED |
				      RTE_COMP_FF_HUFFMAN_DYNAMIC |
				      RTE_COMP_FF_CRC32_CHECKSUM |
				      RTE_COMP_FF_ADLER32_CHECKSUM |
				      RTE_COMP_FF_SHAREABLE_PRIV_XFORM |
				      RTE_COMP_FF_OOP_SGL_IN_SGL_OUT |
				      RTE_COMP_FF_OOP_SGL_IN_LB_OUT |
				      RTE_COMP_FF_OOP_LB_IN_SGL_OUT |
				      RTE_COMP_FF_STATEFUL_COMPRESSION |
				      RTE_COMP_FF_STATEFUL_DECOMPRESSION,
		.window_size = {
			.min = NITROX_COMP_WINDOW_SIZE_MIN,
			.max = NITROX_COMP_WINDOW_SIZE_MAX,
			.increment = 1
		},
	},
	RTE_COMP_END_OF_CAPABILITIES_LIST()
};

static int nitrox_comp_dev_configure(struct rte_compressdev *dev,
				     struct rte_compressdev_config *config)
{
	struct nitrox_comp_device *comp_dev = dev->data->dev_private;
	struct nitrox_device *ndev = comp_dev->ndev;
	uint32_t xform_cnt;
	char name[RTE_MEMPOOL_NAMESIZE];

	if (config->nb_queue_pairs > ndev->nr_queues) {
		NITROX_LOG_LINE(ERR, "Invalid queue pairs, max supported %d",
			   ndev->nr_queues);
		return -EINVAL;
	}

	xform_cnt = config->max_nb_priv_xforms + config->max_nb_streams;
	if (unlikely(xform_cnt == 0)) {
		NITROX_LOG_LINE(ERR, "Invalid configuration with 0 xforms");
		return -EINVAL;
	}

	snprintf(name, sizeof(name), "%s_xform", dev->data->name);
	comp_dev->xform_pool = rte_mempool_create(name,
			xform_cnt, sizeof(struct nitrox_comp_xform),
			0, 0, NULL, NULL, NULL, NULL,
			config->socket_id, 0);
	if (comp_dev->xform_pool == NULL) {
		NITROX_LOG_LINE(ERR, "Failed to create xform pool, err %d",
			   rte_errno);
		return -rte_errno;
	}

	return 0;
}

static int nitrox_comp_dev_start(struct rte_compressdev *dev)
{
	RTE_SET_USED(dev);
	return 0;
}

static void nitrox_comp_dev_stop(struct rte_compressdev *dev)
{
	RTE_SET_USED(dev);
}

static int nitrox_comp_dev_close(struct rte_compressdev *dev)
{
	int i, ret;
	struct nitrox_comp_device *comp_dev = dev->data->dev_private;

	for (i = 0; i < dev->data->nb_queue_pairs; i++) {
		ret = nitrox_comp_queue_pair_release(dev, i);
		if (ret)
			return ret;
	}

	rte_mempool_free(comp_dev->xform_pool);
	comp_dev->xform_pool = NULL;
	return 0;
}

static void nitrox_comp_stats_get(struct rte_compressdev *dev,
				  struct rte_compressdev_stats *stats)
{
	int qp_id;

	for (qp_id = 0; qp_id < dev->data->nb_queue_pairs; qp_id++) {
		struct nitrox_qp *qp = dev->data->queue_pairs[qp_id];

		if (!qp)
			continue;

		stats->enqueued_count += qp->stats.enqueued_count;
		stats->dequeued_count += qp->stats.dequeued_count;
		stats->enqueue_err_count += qp->stats.enqueue_err_count;
		stats->dequeue_err_count += qp->stats.dequeue_err_count;
	}
}

static void nitrox_comp_stats_reset(struct rte_compressdev *dev)
{
	int qp_id;

	for (qp_id = 0; qp_id < dev->data->nb_queue_pairs; qp_id++) {
		struct nitrox_qp *qp = dev->data->queue_pairs[qp_id];

		if (!qp)
			continue;

		memset(&qp->stats, 0, sizeof(qp->stats));
	}
}

static void nitrox_comp_dev_info_get(struct rte_compressdev *dev,
				     struct rte_compressdev_info *info)
{
	struct nitrox_comp_device *comp_dev = dev->data->dev_private;
	struct nitrox_device *ndev = comp_dev->ndev;

	if (!info)
		return;

	info->max_nb_queue_pairs = ndev->nr_queues;
	info->feature_flags = dev->feature_flags;
	info->capabilities = nitrox_comp_pmd_capabilities;
}

static int nitrox_comp_queue_pair_setup(struct rte_compressdev *dev,
					uint16_t qp_id,
					uint32_t max_inflight_ops, int socket_id)
{
	struct nitrox_comp_device *comp_dev = dev->data->dev_private;
	struct nitrox_device *ndev = comp_dev->ndev;
	struct nitrox_qp *qp = NULL;
	int err;

	NITROX_LOG_LINE(DEBUG, "queue %d", qp_id);
	if (qp_id >= ndev->nr_queues) {
		NITROX_LOG_LINE(ERR, "queue %u invalid, max queues supported %d",
			   qp_id, ndev->nr_queues);
		return -EINVAL;
	}

	if (dev->data->queue_pairs[qp_id]) {
		err = nitrox_comp_queue_pair_release(dev, qp_id);
		if (err)
			return err;
	}

	qp = rte_zmalloc_socket("nitrox PMD qp", sizeof(*qp),
				RTE_CACHE_LINE_SIZE,
				socket_id);
	if (!qp) {
		NITROX_LOG_LINE(ERR, "Failed to allocate nitrox qp");
		return -ENOMEM;
	}

	qp->type = NITROX_QUEUE_ZIP;
	qp->qno = qp_id;
	err = nitrox_qp_setup(qp, ndev->bar_addr, dev->data->name,
			      max_inflight_ops, ZIP_INSTR_SIZE,
			      socket_id);
	if (unlikely(err))
		goto qp_setup_err;

	qp->sr_mp = nitrox_comp_req_pool_create(dev, qp->count, qp_id,
						socket_id);
	if (unlikely(!qp->sr_mp))
		goto req_pool_err;

	dev->data->queue_pairs[qp_id] = qp;
	NITROX_LOG_LINE(DEBUG, "queue %d setup done", qp_id);
	return 0;

req_pool_err:
	nitrox_qp_release(qp, ndev->bar_addr);
qp_setup_err:
	rte_free(qp);
	return err;
}

static int nitrox_comp_queue_pair_release(struct rte_compressdev *dev,
					  uint16_t qp_id)
{
	struct nitrox_comp_device *comp_dev = dev->data->dev_private;
	struct nitrox_device *ndev = comp_dev->ndev;
	struct nitrox_qp *qp;
	int err;

	NITROX_LOG_LINE(DEBUG, "queue %d", qp_id);
	if (qp_id >= ndev->nr_queues) {
		NITROX_LOG_LINE(ERR, "queue %u invalid, max queues supported %d",
			   qp_id, ndev->nr_queues);
		return -EINVAL;
	}

	qp = dev->data->queue_pairs[qp_id];
	if (!qp) {
		NITROX_LOG_LINE(DEBUG, "queue %u already freed", qp_id);
		return 0;
	}

	if (!nitrox_qp_is_empty(qp)) {
		NITROX_LOG_LINE(ERR, "queue %d not empty", qp_id);
		return -EAGAIN;
	}

	dev->data->queue_pairs[qp_id] = NULL;
	err = nitrox_qp_release(qp, ndev->bar_addr);
	nitrox_comp_req_pool_free(qp->sr_mp);
	rte_free(qp);
	NITROX_LOG_LINE(DEBUG, "queue %d release done", qp_id);
	return err;
}

static int nitrox_comp_private_xform_create(struct rte_compressdev *dev,
					    const struct rte_comp_xform *xform,
					    void **private_xform)
{
	struct nitrox_comp_device *comp_dev = dev->data->dev_private;
	struct nitrox_comp_xform *nxform;
	enum rte_comp_checksum_type chksum_type;
	int ret;

	if (unlikely(comp_dev->xform_pool == NULL)) {
		NITROX_LOG_LINE(ERR, "private xform pool not yet created");
		return -EINVAL;
	}

	if (rte_mempool_get(comp_dev->xform_pool, private_xform)) {
		NITROX_LOG_LINE(ERR, "Failed to get from private xform pool");
		return -ENOMEM;
	}

	nxform = (struct nitrox_comp_xform *)*private_xform;
	memset(nxform, 0, sizeof(*nxform));
	if (xform->type == RTE_COMP_COMPRESS) {
		enum rte_comp_huffman algo;
		int level;

		nxform->op = NITROX_COMP_OP_COMPRESS;
		if (xform->compress.algo != RTE_COMP_ALGO_DEFLATE) {
			NITROX_LOG_LINE(ERR, "Only deflate is supported");
			ret = -ENOTSUP;
			goto err_exit;
		}

		algo = xform->compress.deflate.huffman;
		if (algo == RTE_COMP_HUFFMAN_DEFAULT)
			nxform->algo = NITROX_COMP_ALGO_DEFLATE_DEFAULT;
		else if (algo == RTE_COMP_HUFFMAN_FIXED)
			nxform->algo = NITROX_COMP_ALGO_DEFLATE_FIXEDHUFF;
		else if (algo == RTE_COMP_HUFFMAN_DYNAMIC)
			nxform->algo = NITROX_COMP_ALGO_DEFLATE_DYNHUFF;
		else {
			NITROX_LOG_LINE(ERR, "Invalid deflate algorithm %d", algo);
			ret = -EINVAL;
			goto err_exit;
		}

		level = xform->compress.level;
		if (level == RTE_COMP_LEVEL_PMD_DEFAULT) {
			nxform->level = NITROX_COMP_LEVEL_MEDIUM;
		} else if (level >= NITROX_COMP_LEVEL_LOWEST_START &&
			   level <= NITROX_COMP_LEVEL_LOWEST_END) {
			nxform->level = NITROX_COMP_LEVEL_LOWEST;
		} else if (level >= NITROX_COMP_LEVEL_LOWER_START &&
			   level <= NITROX_COMP_LEVEL_LOWER_END) {
			nxform->level = NITROX_COMP_LEVEL_LOWER;
		} else if (level >= NITROX_COMP_LEVEL_MEDIUM_START &&
			   level <= NITROX_COMP_LEVEL_MEDIUM_END) {
			nxform->level = NITROX_COMP_LEVEL_MEDIUM;
		} else if (level >= NITROX_COMP_LEVEL_BEST_START &&
			   level <= NITROX_COMP_LEVEL_BEST_END) {
			nxform->level = NITROX_COMP_LEVEL_BEST;
		} else {
			NITROX_LOG_LINE(ERR, "Unsupported compression level %d",
				   xform->compress.level);
			ret = -ENOTSUP;
			goto err_exit;
		}

		chksum_type = xform->compress.chksum;
	} else if (xform->type == RTE_COMP_DECOMPRESS) {
		nxform->op = NITROX_COMP_OP_DECOMPRESS;
		if (xform->decompress.algo != RTE_COMP_ALGO_DEFLATE) {
			NITROX_LOG_LINE(ERR, "Only deflate is supported");
			ret = -ENOTSUP;
			goto err_exit;
		}

		nxform->algo = NITROX_COMP_ALGO_DEFLATE_DEFAULT;
		nxform->level = NITROX_COMP_LEVEL_BEST;
		chksum_type = xform->decompress.chksum;
	} else {
		ret = -EINVAL;
		goto err_exit;
	}

	if (chksum_type == RTE_COMP_CHECKSUM_NONE)
		nxform->chksum_type = NITROX_CHKSUM_TYPE_NONE;
	else if (chksum_type == RTE_COMP_CHECKSUM_CRC32)
		nxform->chksum_type = NITROX_CHKSUM_TYPE_CRC32;
	else if (chksum_type == RTE_COMP_CHECKSUM_ADLER32)
		nxform->chksum_type = NITROX_CHKSUM_TYPE_ADLER32;
	else {
		NITROX_LOG_LINE(ERR, "Unsupported checksum type %d",
			   chksum_type);
		ret = -ENOTSUP;
		goto err_exit;
	}

	nxform->context = NULL;
	nxform->history_window = NULL;
	nxform->window_size = 0;
	nxform->hlen = 0;
	nxform->exn = 0;
	nxform->exbits = 0;
	nxform->bf = true;
	return 0;
err_exit:
	memset(nxform, 0, sizeof(*nxform));
	rte_mempool_put(comp_dev->xform_pool, nxform);
	return ret;
}

static int nitrox_comp_private_xform_free(struct rte_compressdev *dev,
					  void *private_xform)
{
	struct nitrox_comp_xform *nxform = private_xform;
	struct rte_mempool *mp = rte_mempool_from_obj(nxform);

	RTE_SET_USED(dev);
	if (unlikely(nxform == NULL))
		return -EINVAL;

	memset(nxform, 0, sizeof(*nxform));
	mp = rte_mempool_from_obj(nxform);
	rte_mempool_put(mp, nxform);
	return 0;
}

static int nitrox_comp_stream_free(struct rte_compressdev *dev, void *stream)
{
	struct nitrox_comp_xform *nxform = stream;

	if (unlikely(nxform == NULL))
		return -EINVAL;

	rte_free(nxform->history_window);
	nxform->history_window = NULL;
	rte_free(nxform->context);
	nxform->context = NULL;
	return nitrox_comp_private_xform_free(dev, stream);
}

static int nitrox_comp_stream_create(struct rte_compressdev *dev,
			const struct rte_comp_xform *xform, void **stream)
{
	int err;
	struct nitrox_comp_xform *nxform;
	struct nitrox_comp_device *comp_dev = dev->data->dev_private;

	err = nitrox_comp_private_xform_create(dev, xform, stream);
	if (unlikely(err))
		return err;

	nxform = *stream;
	if (xform->type == RTE_COMP_COMPRESS) {
		uint8_t window_size = xform->compress.window_size;

		if (unlikely(window_size < NITROX_COMP_WINDOW_SIZE_MIN ||
			      window_size > NITROX_COMP_WINDOW_SIZE_MAX)) {
			NITROX_LOG_LINE(ERR, "Invalid window size %d",
				   window_size);
			return -EINVAL;
		}

		if (window_size == NITROX_COMP_WINDOW_SIZE_MAX)
			nxform->window_size = NITROX_CONSTANTS_MAX_SEARCH_DEPTH;
		else
			nxform->window_size = RTE_BIT32(window_size);
	} else {
		nxform->window_size = NITROX_DEFAULT_DEFLATE_SEARCH_DEPTH;
	}

	nxform->history_window = rte_zmalloc_socket(NULL, nxform->window_size,
					8, comp_dev->xform_pool->socket_id);
	if (unlikely(nxform->history_window == NULL)) {
		err = -ENOMEM;
		goto err_exit;
	}

	if (xform->type == RTE_COMP_COMPRESS)
		return 0;

	nxform->context = rte_zmalloc_socket(NULL,
					NITROX_DECOMP_CTX_SIZE, 8,
					comp_dev->xform_pool->socket_id);
	if (unlikely(nxform->context == NULL)) {
		err = -ENOMEM;
		goto err_exit;
	}

	return 0;
err_exit:
	nitrox_comp_stream_free(dev, *stream);
	return err;
}

static int nitrox_enq_single_op(struct nitrox_qp *qp, struct rte_comp_op *op)
{
	struct nitrox_softreq *sr;
	int err;

	if (unlikely(rte_mempool_get(qp->sr_mp, (void **)&sr)))
		return -ENOMEM;

	err = nitrox_process_comp_req(op, sr);
	if (unlikely(err)) {
		rte_mempool_put(qp->sr_mp, sr);
		return err;
	}

	if (op->status == RTE_COMP_OP_STATUS_SUCCESS)
		err = nitrox_qp_enqueue_sr(qp, sr);
	else
		nitrox_qp_enqueue(qp, nitrox_comp_instr_addr(sr), sr);

	return err;
}

static uint16_t nitrox_comp_dev_enq_burst(void *queue_pair,
					  struct rte_comp_op **ops,
					  uint16_t nb_ops)
{
	struct nitrox_qp *qp = queue_pair;
	uint16_t free_slots = 0;
	uint16_t cnt = 0;
	uint16_t dbcnt = 0;
	bool err = false;

	free_slots = nitrox_qp_free_count(qp);
	if (nb_ops > free_slots)
		nb_ops = free_slots;

	for (cnt = 0; cnt < nb_ops; cnt++) {
		if (unlikely(nitrox_enq_single_op(qp, ops[cnt]))) {
			err = true;
			break;
		}

		if (ops[cnt]->status != RTE_COMP_OP_STATUS_SUCCESS)
			dbcnt++;
	}

	nitrox_ring_dbell(qp, dbcnt);
	qp->stats.enqueued_count += cnt;
	if (unlikely(err))
		qp->stats.enqueue_err_count++;

	return cnt;
}

static int nitrox_deq_single_op(struct nitrox_qp *qp,
				struct rte_comp_op **op_ptr)
{
	struct nitrox_softreq *sr;
	int err;

	sr = nitrox_qp_get_softreq(qp);
	err = nitrox_check_comp_req(sr, op_ptr);
	if (err == -EAGAIN)
		return err;

	nitrox_qp_dequeue(qp);
	rte_mempool_put(qp->sr_mp, sr);
	if (err == 0)
		qp->stats.dequeued_count++;
	else
		qp->stats.dequeue_err_count++;

	return 0;
}

static uint16_t nitrox_comp_dev_deq_burst(void *queue_pair,
					  struct rte_comp_op **ops,
					  uint16_t nb_ops)
{
	struct nitrox_qp *qp = queue_pair;
	uint16_t filled_slots = nitrox_qp_used_count(qp);
	int cnt = 0;

	if (nb_ops > filled_slots)
		nb_ops = filled_slots;

	for (cnt = 0; cnt < nb_ops; cnt++)
		if (nitrox_deq_single_op(qp, &ops[cnt]))
			break;

	return cnt;
}

static struct rte_compressdev_ops nitrox_compressdev_ops = {
		.dev_configure		= nitrox_comp_dev_configure,
		.dev_start		= nitrox_comp_dev_start,
		.dev_stop		= nitrox_comp_dev_stop,
		.dev_close		= nitrox_comp_dev_close,

		.stats_get		= nitrox_comp_stats_get,
		.stats_reset		= nitrox_comp_stats_reset,

		.dev_infos_get		= nitrox_comp_dev_info_get,

		.queue_pair_setup	= nitrox_comp_queue_pair_setup,
		.queue_pair_release	= nitrox_comp_queue_pair_release,

		.private_xform_create	= nitrox_comp_private_xform_create,
		.private_xform_free	= nitrox_comp_private_xform_free,
		.stream_create		= nitrox_comp_stream_create,
		.stream_free		= nitrox_comp_stream_free,
};

int
nitrox_comp_pmd_create(struct nitrox_device *ndev)
{
	char name[RTE_COMPRESSDEV_NAME_MAX_LEN];
	struct rte_compressdev_pmd_init_params init_params = {
			.name = "",
			.socket_id = ndev->pdev->device.numa_node,
	};
	struct rte_compressdev *cdev;

	rte_pci_device_name(&ndev->pdev->addr, name, sizeof(name));
	snprintf(name + strlen(name),
		 RTE_COMPRESSDEV_NAME_MAX_LEN - strlen(name),
		 "_n5comp");
	ndev->rte_comp_dev.driver = &nitrox_rte_comp_drv;
	ndev->rte_comp_dev.numa_node = ndev->pdev->device.numa_node;
	ndev->rte_comp_dev.devargs = NULL;
	cdev = rte_compressdev_pmd_create(name,
					  &ndev->rte_comp_dev,
					  sizeof(struct nitrox_comp_device),
					  &init_params);
	if (!cdev) {
		NITROX_LOG_LINE(ERR, "Cryptodev '%s' creation failed", name);
		return -ENODEV;
	}

	cdev->dev_ops = &nitrox_compressdev_ops;
	cdev->enqueue_burst = nitrox_comp_dev_enq_burst;
	cdev->dequeue_burst = nitrox_comp_dev_deq_burst;
	cdev->feature_flags = RTE_COMPDEV_FF_HW_ACCELERATED;

	ndev->comp_dev = cdev->data->dev_private;
	ndev->comp_dev->cdev = cdev;
	ndev->comp_dev->ndev = ndev;
	ndev->comp_dev->xform_pool = NULL;
	NITROX_LOG_LINE(DEBUG, "Created compressdev '%s', dev_id %d",
		   cdev->data->name, cdev->data->dev_id);
	return 0;
}

int
nitrox_comp_pmd_destroy(struct nitrox_device *ndev)
{
	int err;

	if (ndev->comp_dev == NULL)
		return 0;

	err = rte_compressdev_pmd_destroy(ndev->comp_dev->cdev);
	if (err)
		return err;

	ndev->comp_dev = NULL;
	return 0;
}

static struct nitrox_driver comp_drv = {
	.create = nitrox_comp_pmd_create,
	.destroy = nitrox_comp_pmd_destroy,
};
NITROX_REGISTER_DRIVER(comp_drv);
