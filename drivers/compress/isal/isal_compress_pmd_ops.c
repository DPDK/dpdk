/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include <rte_common.h>
#include <rte_compressdev_pmd.h>

#include "isal_compress_pmd_private.h"

static const struct rte_compressdev_capabilities isal_pmd_capabilities[] = {
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

/** Get device info */
static void
isal_comp_pmd_info_get(struct rte_compressdev *dev __rte_unused,
		struct rte_compressdev_info *dev_info)
{
	if (dev_info != NULL) {
		dev_info->capabilities = isal_pmd_capabilities;
		dev_info->feature_flags = RTE_COMPDEV_FF_CPU_AVX512 |
				RTE_COMPDEV_FF_CPU_AVX2 |
				RTE_COMPDEV_FF_CPU_AVX |
				RTE_COMPDEV_FF_CPU_SSE;
	}
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

		.stats_get		= NULL,
		.stats_reset		= NULL,

		.dev_infos_get		= isal_comp_pmd_info_get,

		.queue_pair_setup	= NULL,
		.queue_pair_release	= NULL,

		.private_xform_create	= isal_comp_pmd_priv_xform_create,
		.private_xform_free	= isal_comp_pmd_priv_xform_free,
};

struct rte_compressdev_ops *isal_compress_pmd_ops = &isal_pmd_ops;
