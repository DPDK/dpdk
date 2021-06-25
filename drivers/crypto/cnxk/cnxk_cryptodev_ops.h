/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef _CNXK_CRYPTODEV_OPS_H_
#define _CNXK_CRYPTODEV_OPS_H_

#include <rte_cryptodev.h>

#define CNXK_CPT_MIN_HEADROOM_REQ 24

int cnxk_cpt_dev_config(struct rte_cryptodev *dev,
			struct rte_cryptodev_config *conf);

int cnxk_cpt_dev_start(struct rte_cryptodev *dev);

void cnxk_cpt_dev_stop(struct rte_cryptodev *dev);

int cnxk_cpt_dev_close(struct rte_cryptodev *dev);

void cnxk_cpt_dev_info_get(struct rte_cryptodev *dev,
			   struct rte_cryptodev_info *info);

#endif /* _CNXK_CRYPTODEV_OPS_H_ */
