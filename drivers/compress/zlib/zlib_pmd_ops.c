/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Cavium Networks
 */

#include <string.h>

#include <rte_common.h>
#include <rte_malloc.h>

#include "zlib_pmd_private.h"

static const struct rte_compressdev_capabilities zlib_pmd_capabilities[] = {
	{   /* Deflate */
		.algo = RTE_COMP_ALGO_DEFLATE,
		.comp_feature_flags = (RTE_COMP_FF_NONCOMPRESSED_BLOCKS |
					RTE_COMP_FF_HUFFMAN_FIXED |
					RTE_COMP_FF_HUFFMAN_DYNAMIC),
		.window_size = {
			.min = 8,
			.max = 15,
			.increment = 1
		},
	},

	RTE_COMP_END_OF_CAPABILITIES_LIST()

};

/** Configure device */
static int
zlib_pmd_config(struct rte_compressdev *dev,
		struct rte_compressdev_config *config)
{
	struct rte_mempool *mp;
	char mp_name[RTE_MEMPOOL_NAMESIZE];
	struct zlib_private *internals = dev->data->dev_private;

	snprintf(mp_name, RTE_MEMPOOL_NAMESIZE,
			"stream_mp_%u", dev->data->dev_id);
	mp = internals->mp;
	if (mp == NULL) {
		mp = rte_mempool_create(mp_name,
				config->max_nb_priv_xforms +
				config->max_nb_streams,
				sizeof(struct zlib_priv_xform),
				0, 0, NULL, NULL, NULL,
				NULL, config->socket_id,
				0);
		if (mp == NULL) {
			ZLIB_PMD_ERR("Cannot create private xform pool on "
			"socket %d\n", config->socket_id);
			return -ENOMEM;
		}
		internals->mp = mp;
	}
	return 0;
}

/** Start device */
static int
zlib_pmd_start(__rte_unused struct rte_compressdev *dev)
{
	return 0;
}

/** Stop device */
static void
zlib_pmd_stop(__rte_unused struct rte_compressdev *dev)
{
}

/** Close device */
static int
zlib_pmd_close(struct rte_compressdev *dev)
{
	struct zlib_private *internals = dev->data->dev_private;
	rte_mempool_free(internals->mp);
	internals->mp = NULL;
	return 0;
}

/** Get device statistics */
static void
zlib_pmd_stats_get(struct rte_compressdev *dev,
		struct rte_compressdev_stats *stats)
{
	int qp_id;

	for (qp_id = 0; qp_id < dev->data->nb_queue_pairs; qp_id++) {
		struct zlib_qp *qp = dev->data->queue_pairs[qp_id];

		stats->enqueued_count += qp->qp_stats.enqueued_count;
		stats->dequeued_count += qp->qp_stats.dequeued_count;

		stats->enqueue_err_count += qp->qp_stats.enqueue_err_count;
		stats->dequeue_err_count += qp->qp_stats.dequeue_err_count;
	}
}

/** Reset device statistics */
static void
zlib_pmd_stats_reset(struct rte_compressdev *dev)
{
	int qp_id;

	for (qp_id = 0; qp_id < dev->data->nb_queue_pairs; qp_id++) {
		struct zlib_qp *qp = dev->data->queue_pairs[qp_id];

		memset(&qp->qp_stats, 0, sizeof(qp->qp_stats));
	}
}

/** Get device info */
static void
zlib_pmd_info_get(struct rte_compressdev *dev,
		struct rte_compressdev_info *dev_info)
{
	if (dev_info != NULL) {
		dev_info->driver_name = dev->device->name;
		dev_info->feature_flags = dev->feature_flags;
		dev_info->capabilities = zlib_pmd_capabilities;
	}
}

/** Release queue pair */
static int
zlib_pmd_qp_release(struct rte_compressdev *dev, uint16_t qp_id)
{
	struct zlib_qp *qp = dev->data->queue_pairs[qp_id];

	if (qp != NULL) {
		rte_ring_free(qp->processed_pkts);
		rte_free(qp);
		dev->data->queue_pairs[qp_id] = NULL;
	}
	return 0;
}

/** set a unique name for the queue pair based on its name, dev_id and qp_id */
static int
zlib_pmd_qp_set_unique_name(struct rte_compressdev *dev,
		struct zlib_qp *qp)
{
	unsigned int n = snprintf(qp->name, sizeof(qp->name),
				"zlib_pmd_%u_qp_%u",
				dev->data->dev_id, qp->id);

	if (n >= sizeof(qp->name))
		return -1;

	return 0;
}

/** Create a ring to place process packets on */
static struct rte_ring *
zlib_pmd_qp_create_processed_pkts_ring(struct zlib_qp *qp,
		unsigned int ring_size, int socket_id)
{
	struct rte_ring *r = qp->processed_pkts;

	if (r) {
		if (rte_ring_get_size(r) >= ring_size) {
			ZLIB_PMD_INFO("Reusing existing ring %s for processed"
					" packets", qp->name);
			return r;
		}

		ZLIB_PMD_ERR("Unable to reuse existing ring %s for processed"
				" packets", qp->name);
		return NULL;
	}

	return rte_ring_create(qp->name, ring_size, socket_id,
						RING_F_EXACT_SZ);
}

/** Setup a queue pair */
static int
zlib_pmd_qp_setup(struct rte_compressdev *dev, uint16_t qp_id,
		uint32_t max_inflight_ops, int socket_id)
{
	struct zlib_qp *qp = NULL;

	/* Free memory prior to re-allocation if needed. */
	if (dev->data->queue_pairs[qp_id] != NULL)
		zlib_pmd_qp_release(dev, qp_id);

	/* Allocate the queue pair data structure. */
	qp = rte_zmalloc_socket("ZLIB PMD Queue Pair", sizeof(*qp),
					RTE_CACHE_LINE_SIZE, socket_id);
	if (qp == NULL)
		return (-ENOMEM);

	qp->id = qp_id;
	dev->data->queue_pairs[qp_id] = qp;

	if (zlib_pmd_qp_set_unique_name(dev, qp))
		goto qp_setup_cleanup;

	qp->processed_pkts = zlib_pmd_qp_create_processed_pkts_ring(qp,
			max_inflight_ops, socket_id);
	if (qp->processed_pkts == NULL)
		goto qp_setup_cleanup;

	memset(&qp->qp_stats, 0, sizeof(qp->qp_stats));
	return 0;

qp_setup_cleanup:
	if (qp) {
		rte_free(qp);
		qp = NULL;
	}
	return -1;
}

struct rte_compressdev_ops zlib_pmd_ops = {
		.dev_configure		= zlib_pmd_config,
		.dev_start		= zlib_pmd_start,
		.dev_stop		= zlib_pmd_stop,
		.dev_close		= zlib_pmd_close,

		.stats_get		= zlib_pmd_stats_get,
		.stats_reset		= zlib_pmd_stats_reset,

		.dev_infos_get		= zlib_pmd_info_get,

		.queue_pair_setup	= zlib_pmd_qp_setup,
		.queue_pair_release	= zlib_pmd_qp_release,

		.private_xform_create	= NULL,
		.private_xform_free	= NULL,

		.stream_create	= NULL,
		.stream_free	= NULL
};

struct rte_compressdev_ops *rte_zlib_pmd_ops = &zlib_pmd_ops;
