/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include <rte_cryptodev.h>
#include <rte_cryptodev_pmd.h>
#include <rte_errno.h>

#include "roc_cpt.h"

#include "cnxk_cryptodev.h"
#include "cnxk_cryptodev_ops.h"

int
cnxk_cpt_dev_config(struct rte_cryptodev *dev,
		    struct rte_cryptodev_config *conf)
{
	struct cnxk_cpt_vf *vf = dev->data->dev_private;
	struct roc_cpt *roc_cpt = &vf->cpt;
	uint16_t nb_lf_avail, nb_lf;
	int ret;

	dev->feature_flags &= ~conf->ff_disable;

	nb_lf_avail = roc_cpt->nb_lf_avail;
	nb_lf = conf->nb_queue_pairs;

	if (nb_lf > nb_lf_avail)
		return -ENOTSUP;

	ret = roc_cpt_dev_configure(roc_cpt, nb_lf);
	if (ret) {
		plt_err("Could not configure device");
		return ret;
	}

	return 0;
}

int
cnxk_cpt_dev_start(struct rte_cryptodev *dev)
{
	RTE_SET_USED(dev);

	return 0;
}

void
cnxk_cpt_dev_stop(struct rte_cryptodev *dev)
{
	RTE_SET_USED(dev);
}

int
cnxk_cpt_dev_close(struct rte_cryptodev *dev)
{
	struct cnxk_cpt_vf *vf = dev->data->dev_private;

	roc_cpt_dev_clear(&vf->cpt);

	return 0;
}

void
cnxk_cpt_dev_info_get(struct rte_cryptodev *dev,
		      struct rte_cryptodev_info *info)
{
	struct cnxk_cpt_vf *vf = dev->data->dev_private;
	struct roc_cpt *roc_cpt = &vf->cpt;

	info->max_nb_queue_pairs = roc_cpt->nb_lf_avail;
	info->feature_flags = dev->feature_flags;
	info->capabilities = NULL;
	info->sym.max_nb_sessions = 0;
	info->min_mbuf_headroom_req = CNXK_CPT_MIN_HEADROOM_REQ;
	info->min_mbuf_tailroom_req = 0;
}
