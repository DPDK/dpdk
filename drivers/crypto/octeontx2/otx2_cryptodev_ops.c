/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2019 Marvell International Ltd.
 */

#include <rte_cryptodev_pmd.h>

#include "otx2_cryptodev.h"
#include "otx2_cryptodev_hw_access.h"
#include "otx2_cryptodev_mbox.h"
#include "otx2_cryptodev_ops.h"
#include "otx2_mbox.h"

#include "cpt_hw_types.h"
#include "cpt_pmd_logs.h"

/* PMD ops */

static int
otx2_cpt_dev_config(struct rte_cryptodev *dev,
		    struct rte_cryptodev_config *conf)
{
	struct otx2_cpt_vf *vf = dev->data->dev_private;
	int ret;

	if (conf->nb_queue_pairs > vf->max_queues) {
		CPT_LOG_ERR("Invalid number of queue pairs requested");
		return -EINVAL;
	}

	dev->feature_flags &= ~conf->ff_disable;

	/* Unregister error interrupts */
	if (vf->err_intr_registered)
		otx2_cpt_err_intr_unregister(dev);

	/* Detach queues */
	if (vf->nb_queues) {
		ret = otx2_cpt_queues_detach(dev);
		if (ret) {
			CPT_LOG_ERR("Could not detach CPT queues");
			return ret;
		}
	}

	/* Attach queues */
	ret = otx2_cpt_queues_attach(dev, conf->nb_queue_pairs);
	if (ret) {
		CPT_LOG_ERR("Could not attach CPT queues");
		return -ENODEV;
	}

	ret = otx2_cpt_msix_offsets_get(dev);
	if (ret) {
		CPT_LOG_ERR("Could not get MSI-X offsets");
		goto queues_detach;
	}

	/* Register error interrupts */
	ret = otx2_cpt_err_intr_register(dev);
	if (ret) {
		CPT_LOG_ERR("Could not register error interrupts");
		goto queues_detach;
	}

	rte_mb();
	return 0;

queues_detach:
	otx2_cpt_queues_detach(dev);
	return ret;
}

static int
otx2_cpt_dev_start(struct rte_cryptodev *dev)
{
	RTE_SET_USED(dev);

	CPT_PMD_INIT_FUNC_TRACE();

	return 0;
}

static void
otx2_cpt_dev_stop(struct rte_cryptodev *dev)
{
	RTE_SET_USED(dev);

	CPT_PMD_INIT_FUNC_TRACE();
}

static int
otx2_cpt_dev_close(struct rte_cryptodev *dev)
{
	struct otx2_cpt_vf *vf = dev->data->dev_private;
	int ret = 0;

	/* Unregister error interrupts */
	if (vf->err_intr_registered)
		otx2_cpt_err_intr_unregister(dev);

	/* Detach queues */
	if (vf->nb_queues) {
		ret = otx2_cpt_queues_detach(dev);
		if (ret)
			CPT_LOG_ERR("Could not detach CPT queues");
	}

	return ret;
}

static void
otx2_cpt_dev_info_get(struct rte_cryptodev *dev,
		      struct rte_cryptodev_info *info)
{
	struct otx2_cpt_vf *vf = dev->data->dev_private;

	if (info != NULL) {
		info->max_nb_queue_pairs = vf->max_queues;
		info->feature_flags = dev->feature_flags;
		info->capabilities = NULL;
		info->sym.max_nb_sessions = 0;
		info->driver_id = otx2_cryptodev_driver_id;
		info->min_mbuf_headroom_req = OTX2_CPT_MIN_HEADROOM_REQ;
		info->min_mbuf_tailroom_req = OTX2_CPT_MIN_TAILROOM_REQ;
	}
}

struct rte_cryptodev_ops otx2_cpt_ops = {
	/* Device control ops */
	.dev_configure = otx2_cpt_dev_config,
	.dev_start = otx2_cpt_dev_start,
	.dev_stop = otx2_cpt_dev_stop,
	.dev_close = otx2_cpt_dev_close,
	.dev_infos_get = otx2_cpt_dev_info_get,

	.stats_get = NULL,
	.stats_reset = NULL,
	.queue_pair_setup = NULL,
	.queue_pair_release = NULL,
	.queue_pair_count = NULL,

	/* Symmetric crypto ops */
	.sym_session_get_size = NULL,
	.sym_session_configure = NULL,
	.sym_session_clear = NULL,
};
