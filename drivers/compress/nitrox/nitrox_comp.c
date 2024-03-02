/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2024 Marvell.
 */

#include <rte_compressdev_pmd.h>
#include <rte_comp.h>
#include <rte_errno.h>

#include "nitrox_comp.h"
#include "nitrox_device.h"
#include "nitrox_logs.h"
#include "nitrox_comp_reqmgr.h"

static const char nitrox_comp_drv_name[] = RTE_STR(COMPRESSDEV_NAME_NITROX_PMD);
static const struct rte_driver nitrox_rte_comp_drv = {
	.name = nitrox_comp_drv_name,
	.alias = nitrox_comp_drv_name
};

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
				      RTE_COMP_FF_OOP_LB_IN_SGL_OUT,
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
		NITROX_LOG(ERR, "Invalid queue pairs, max supported %d\n",
			   ndev->nr_queues);
		return -EINVAL;
	}

	xform_cnt = config->max_nb_priv_xforms + config->max_nb_streams;
	if (unlikely(xform_cnt == 0)) {
		NITROX_LOG(ERR, "Invalid configuration with 0 xforms\n");
		return -EINVAL;
	}

	snprintf(name, sizeof(name), "%s_xform", dev->data->name);
	comp_dev->xform_pool = rte_mempool_create(name,
			xform_cnt, sizeof(struct nitrox_comp_xform),
			0, 0, NULL, NULL, NULL, NULL,
			config->socket_id, 0);
	if (comp_dev->xform_pool == NULL) {
		NITROX_LOG(ERR, "Failed to create xform pool, err %d\n",
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
	struct nitrox_comp_device *comp_dev = dev->data->dev_private;

	rte_mempool_free(comp_dev->xform_pool);
	comp_dev->xform_pool = NULL;
	return 0;
}

static void nitrox_comp_stats_get(struct rte_compressdev *dev,
				  struct rte_compressdev_stats *stats)
{
	RTE_SET_USED(dev);
	RTE_SET_USED(stats);
}

static void nitrox_comp_stats_reset(struct rte_compressdev *dev)
{
	RTE_SET_USED(dev);
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
	RTE_SET_USED(dev);
	RTE_SET_USED(qp_id);
	RTE_SET_USED(max_inflight_ops);
	RTE_SET_USED(socket_id);
	return -1;
}

static int nitrox_comp_queue_pair_release(struct rte_compressdev *dev,
					  uint16_t qp_id)
{
	RTE_SET_USED(dev);
	RTE_SET_USED(qp_id);
	return 0;
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
		NITROX_LOG(ERR, "private xform pool not yet created\n");
		return -EINVAL;
	}

	if (rte_mempool_get(comp_dev->xform_pool, private_xform)) {
		NITROX_LOG(ERR, "Failed to get from private xform pool\n");
		return -ENOMEM;
	}

	nxform = (struct nitrox_comp_xform *)*private_xform;
	memset(nxform, 0, sizeof(*nxform));
	if (xform->type == RTE_COMP_COMPRESS) {
		enum rte_comp_huffman algo;
		int level;

		nxform->op = NITROX_COMP_OP_COMPRESS;
		if (xform->compress.algo != RTE_COMP_ALGO_DEFLATE) {
			NITROX_LOG(ERR, "Only deflate is supported\n");
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
			NITROX_LOG(ERR, "Invalid deflate algorithm %d\n", algo);
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
			NITROX_LOG(ERR, "Unsupported compression level %d\n",
				   xform->compress.level);
			ret = -ENOTSUP;
			goto err_exit;
		}

		chksum_type = xform->compress.chksum;
	} else if (xform->type == RTE_COMP_DECOMPRESS) {
		nxform->op = NITROX_COMP_OP_DECOMPRESS;
		if (xform->decompress.algo != RTE_COMP_ALGO_DEFLATE) {
			NITROX_LOG(ERR, "Only deflate is supported\n");
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
		NITROX_LOG(ERR, "Unsupported checksum type %d\n",
			   chksum_type);
		ret = -ENOTSUP;
		goto err_exit;
	}

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

static uint16_t nitrox_comp_dev_enq_burst(void *qp,
					  struct rte_comp_op **ops,
					  uint16_t nb_ops)
{
	RTE_SET_USED(qp);
	RTE_SET_USED(ops);
	RTE_SET_USED(nb_ops);
	return 0;
}

static uint16_t nitrox_comp_dev_deq_burst(void *qp,
					  struct rte_comp_op **ops,
					  uint16_t nb_ops)
{
	RTE_SET_USED(qp);
	RTE_SET_USED(ops);
	RTE_SET_USED(nb_ops);
	return 0;
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
		.stream_create		= NULL,
		.stream_free		= NULL
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
		NITROX_LOG(ERR, "Cryptodev '%s' creation failed\n", name);
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
	NITROX_LOG(DEBUG, "Created compressdev '%s', dev_id %d\n",
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
