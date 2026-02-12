/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Marvell.
 */

#include <rte_mldev.h>
#include <rte_mldev_pmd.h>

#include "cnxk_ml_dev.h"

/* Device status */
int cnxk_ml_dev_initialized;

/* Dummy operations for ML device */
struct rte_ml_dev_ops ml_dev_dummy_ops = {0};
