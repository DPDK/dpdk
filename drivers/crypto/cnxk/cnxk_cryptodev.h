/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef _CNXK_CRYPTODEV_H_
#define _CNXK_CRYPTODEV_H_

#include <rte_cryptodev.h>

#include "roc_cpt.h"

/**
 * Device private data
 */
struct cnxk_cpt_vf {
	struct roc_cpt cpt;
};

int cnxk_cpt_eng_grp_add(struct roc_cpt *roc_cpt);

#endif /* _CNXK_CRYPTODEV_H_ */
