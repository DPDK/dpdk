/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */
#include <isa-l.h>

#include <rte_common.h>
#include <rte_compressdev_pmd.h>
#include <rte_malloc.h>

#include "isal_compress_pmd_private.h"

static const struct rte_compressdev_capabilities isal_pmd_capabilities[] = {
	{
		.algo = RTE_COMP_ALGO_DEFLATE,
		.comp_feature_flags =	RTE_COMP_FF_OOP_SGL_IN_SGL_OUT |
					RTE_COMP_FF_OOP_SGL_IN_LB_OUT |
					RTE_COMP_FF_OOP_LB_IN_SGL_OUT |
					RTE_COMP_FF_SHAREABLE_PRIV_XFORM |
					RTE_COMP_FF_HUFFMAN_FIXED |
					RTE_COMP_FF_HUFFMAN_DYNAMIC |
					RTE_COMP_FF_CRC32_CHECKSUM |
					RTE_COMP_FF_ADLER32_CHECKSUM,
		.window_size = {
			.min = 15,
			.max = 15,
			.increment = 0
		},
	},
	RTE_COMP_END_OF_CAPABILITIES_LIST()
};

/** Configure device */
static int
isal_comp_pmd_config(struct rte_compressdev *dev,
		struct rte_compressdev_config *config)
{
	int ret = 0;
	unsigned int n;
	char mp_name[RTE_COMPRESSDEV_NAME_MAX_LEN];
	unsigned int elt_size = sizeof(struct isal_priv_xform);
	struct isal_comp_private *internals = dev->data->dev_private;

	n = snprintf(mp_name, sizeof(mp_name), "compdev_%d_xform_mp",
			dev->data->dev_id);
	if (n > sizeof(mp_name)) {
		ISAL_PMD_LOG(ERR,
			"Unable to create unique name for xform mempool");
		return -ENOMEM;
	}

	internals->priv_xform_mp = rte_mempool_lookup(mp_name);

	if (internals->priv_xform_mp != NULL) {
		if (((internals->priv_xform_mp)->elt_size != elt_size) ||
				((internals->priv_xform_mp)->size <
					config->max_nb_priv_xforms)) {

			ISAL_PMD_LOG(ERR, "%s mempool already exists with different"
				" initialization parameters", mp_name);
			internals->priv_xform_mp = NULL;
			return -ENOMEM;
		}
	} else { /* First time configuration */
		internals->priv_xform_mp = rte_mempool_create(
				mp_name, /* mempool name */
				/* number of elements*/
				config->max_nb_priv_xforms,
				elt_size, /* element size*/
				0, /* Cache size*/
				0, /* private data size */
				NULL, /* obj initialization constructor */
				NULL, /* obj initialization constructor arg */
				NULL, /**< obj constructor*/
				NULL, /* obj constructor arg */
				config->socket_id, /* socket id */
				0); /* flags */
	}

	if (internals->priv_xform_mp == NULL) {
		ISAL_PMD_LOG(ERR, "%s mempool allocation failed", mp_name);
		return -ENOMEM;
	}

	dev->data->dev_private = internals;

	return ret;
}

/** Start device */
static int
isal_comp_pmd_start(__rte_unused struct rte_compressdev *dev)
{
	return 0;
}

/** Stop device */
static void
isal_comp_pmd_stop(__rte_unused struct rte_compressdev *dev)
{
}

/** Close device */
static int
isal_comp_pmd_close(struct rte_compressdev *dev)
{
	/* Free private data */
	struct isal_comp_private *internals = dev->data->dev_private;

	rte_mempool_free(internals->priv_xform_mp);
	return 0;
}

/** Get device statistics */
static void
isal_comp_pmd_stats_get(struct rte_compressdev *dev,
		struct rte_compressdev_stats *stats)
{
	uint16_t qp_id;

	for (qp_id = 0; qp_id < dev->data->nb_queue_pairs; qp_id++) {
		struct isal_comp_qp *qp = dev->data->queue_pairs[qp_id];

		stats->enqueued_count += qp->qp_stats.enqueued_count;
		stats->dequeued_count += qp->qp_stats.dequeued_count;

		stats->enqueue_err_count += qp->qp_stats.enqueue_err_count;
		stats->dequeue_err_count += qp->qp_stats.dequeue_err_count;
	}
}

/** Get device info */
static void
isal_comp_pmd_info_get(struct rte_compressdev *dev __rte_unused,
		struct rte_compressdev_info *dev_info)
{
	if (dev_info != NULL) {
		dev_info->capabilities = isal_pmd_capabilities;

		/* Check CPU for supported vector instruction and set
		 * feature_flags
		 */
		if (rte_cpu_get_flag_enabled(RTE_CPUFLAG_AVX512F))
			dev_info->feature_flags |= RTE_COMPDEV_FF_CPU_AVX512;
		else if (rte_cpu_get_flag_enabled(RTE_CPUFLAG_AVX2))
			dev_info->feature_flags |= RTE_COMPDEV_FF_CPU_AVX2;
		else if (rte_cpu_get_flag_enabled(RTE_CPUFLAG_AVX))
			dev_info->feature_flags |= RTE_COMPDEV_FF_CPU_AVX;
		else
			dev_info->feature_flags |= RTE_COMPDEV_FF_CPU_SSE;
	}
}

/** Reset device statistics */
static void
isal_comp_pmd_stats_reset(struct rte_compressdev *dev)
{
	uint16_t qp_id;

	for (qp_id = 0; qp_id < dev->data->nb_queue_pairs; qp_id++) {
		struct isal_comp_qp *qp = dev->data->queue_pairs[qp_id];
		memset(&qp->qp_stats, 0, sizeof(qp->qp_stats));
	}
}

/** Release queue pair */
static int
isal_comp_pmd_qp_release(struct rte_compressdev *dev, uint16_t qp_id)
{
	struct isal_comp_qp *qp = dev->data->queue_pairs[qp_id];

	if (qp == NULL)
		return -EINVAL;

	if (qp->stream)
		rte_free(qp->stream->level_buf);

	rte_free(qp->state);
	rte_ring_free(qp->processed_pkts);
	rte_free(qp->stream);
	rte_free(qp);
	dev->data->queue_pairs[qp_id] = NULL;

	return 0;
}

/** Create a ring to place process packets on */
static struct rte_ring *
isal_comp_pmd_qp_create_processed_pkts_ring(struct isal_comp_qp *qp,
		unsigned int ring_size, int socket_id)
{
	struct rte_ring *r;

	r = rte_ring_lookup(qp->name);
	if (r) {
		if (rte_ring_get_size(r) >= ring_size) {
			ISAL_PMD_LOG(DEBUG,
				"Reusing existing ring %s for processed packets",
				qp->name);
			return r;
		}

			ISAL_PMD_LOG(ERR,
				"Unable to reuse existing ring %s"
				" for processed packets",
			 qp->name);
		return NULL;
	}

	return rte_ring_create(qp->name, ring_size, socket_id,
			RING_F_SP_ENQ | RING_F_SC_DEQ);
}

/** set a unique name for the queue pair based on its name, dev_id and qp_id */
static int
isal_comp_pmd_qp_set_unique_name(struct rte_compressdev *dev,
struct isal_comp_qp *qp)
{
	unsigned int n = snprintf(qp->name, sizeof(qp->name),
			"isal_comp_pmd_%u_qp_%u",
			dev->data->dev_id, qp->id);

	if (n >= sizeof(qp->name))
		return -1;

	return 0;
}

/* Setup a queue pair */
static int
isal_comp_pmd_qp_setup(struct rte_compressdev *dev, uint16_t qp_id,
		uint32_t max_inflight_ops, int socket_id)
{
	struct isal_comp_qp *qp = NULL;
	int retval;

	/* Free memory prior to re-allocation if needed. */
	if (dev->data->queue_pairs[qp_id] != NULL)
		isal_comp_pmd_qp_release(dev, qp_id);

	/* Allocate the queue pair data structure. */
	qp = rte_zmalloc_socket("Isa-l compression PMD Queue Pair", sizeof(*qp),
					RTE_CACHE_LINE_SIZE, socket_id);
	if (qp == NULL) {
		ISAL_PMD_LOG(ERR, "Failed to allocate queue pair memory");
		return (-ENOMEM);
	}

	/* Initialize memory for compression stream structure */
	qp->stream = rte_zmalloc_socket("Isa-l compression stream ",
			sizeof(struct isal_zstream),  RTE_CACHE_LINE_SIZE,
			socket_id);
	if (qp->stream == NULL) {
		ISAL_PMD_LOG(ERR, "Failed to allocate compression stream memory");
		goto qp_setup_cleanup;
	}
	/* Initialize memory for compression level buffer */
	qp->stream->level_buf = rte_zmalloc_socket("Isa-l compression lev_buf",
			ISAL_DEF_LVL3_DEFAULT, RTE_CACHE_LINE_SIZE,
			socket_id);
	if (qp->stream->level_buf == NULL) {
		ISAL_PMD_LOG(ERR, "Failed to allocate compression level_buf memory");
		goto qp_setup_cleanup;
	}

	/* Initialize memory for decompression state structure */
	qp->state = rte_zmalloc_socket("Isa-l decompression state",
			sizeof(struct inflate_state), RTE_CACHE_LINE_SIZE,
			socket_id);
	if (qp->state == NULL) {
		ISAL_PMD_LOG(ERR, "Failed to allocate decompression state memory");
		goto qp_setup_cleanup;
	}

	qp->id = qp_id;
	dev->data->queue_pairs[qp_id] = qp;

	retval = isal_comp_pmd_qp_set_unique_name(dev, qp);
	if (retval) {
		ISAL_PMD_LOG(ERR, "Failed to create unique name for isal "
				"compression device");
		goto qp_setup_cleanup;
	}

	qp->processed_pkts = isal_comp_pmd_qp_create_processed_pkts_ring(qp,
			max_inflight_ops, socket_id);
	if (qp->processed_pkts == NULL) {
		ISAL_PMD_LOG(ERR, "Failed to create unique name for isal "
				"compression device");
		goto qp_setup_cleanup;
	}

	qp->num_free_elements = rte_ring_free_count(qp->processed_pkts);

	memset(&qp->qp_stats, 0, sizeof(qp->qp_stats));
	return 0;

qp_setup_cleanup:
	if (qp->stream)
		rte_free(qp->stream->level_buf);
	rte_free(qp->stream);
	rte_free(qp->state);
	rte_free(qp);

	return -1;
}

/** Set private xform data*/
static int
isal_comp_pmd_priv_xform_create(struct rte_compressdev *dev,
			const struct rte_comp_xform *xform, void **priv_xform)
{
	int ret;
	struct isal_comp_private *internals = dev->data->dev_private;

	if (xform == NULL) {
		ISAL_PMD_LOG(ERR, "Invalid Xform struct");
		return -EINVAL;
	}

	if (rte_mempool_get(internals->priv_xform_mp, priv_xform)) {
		ISAL_PMD_LOG(ERR,
			"Couldn't get object from private xform mempool");
		return -ENOMEM;
	}

	ret = isal_comp_set_priv_xform_parameters(*priv_xform, xform);
	if (ret != 0) {
		ISAL_PMD_LOG(ERR, "Failed to configure private xform parameters");

		/* Return private xform to mempool */
		rte_mempool_put(internals->priv_xform_mp, priv_xform);
		return ret;
	}
	return 0;
}

/** Clear memory of the private xform so it doesn't leave key material behind */
static int
isal_comp_pmd_priv_xform_free(struct rte_compressdev *dev, void *priv_xform)
{
	struct isal_comp_private *internals = dev->data->dev_private;

	/* Zero out the whole structure */
	if (priv_xform) {
		memset(priv_xform, 0, sizeof(struct isal_priv_xform));
		rte_mempool_put(internals->priv_xform_mp, priv_xform);
	}
	return 0;
}

struct rte_compressdev_ops isal_pmd_ops = {
		.dev_configure		= isal_comp_pmd_config,
		.dev_start		= isal_comp_pmd_start,
		.dev_stop		= isal_comp_pmd_stop,
		.dev_close		= isal_comp_pmd_close,

		.stats_get		= isal_comp_pmd_stats_get,
		.stats_reset		= isal_comp_pmd_stats_reset,

		.dev_infos_get		= isal_comp_pmd_info_get,

		.queue_pair_setup	= isal_comp_pmd_qp_setup,
		.queue_pair_release	= isal_comp_pmd_qp_release,

		.private_xform_create	= isal_comp_pmd_priv_xform_create,
		.private_xform_free	= isal_comp_pmd_priv_xform_free,
};

struct rte_compressdev_ops *isal_compress_pmd_ops = &isal_pmd_ops;
