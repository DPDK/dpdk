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

/* Error type database */
struct cnxk_ml_error_db ml_etype_db[] = {
	{ML_CNXK_ETYPE_NO_ERROR, "NO_ERROR"},	     {ML_CNXK_ETYPE_FW_NONFATAL, "FW_NON_FATAL"},
	{ML_CNXK_ETYPE_HW_NONFATAL, "HW_NON_FATAL"}, {ML_CNXK_ETYPE_HW_FATAL, "HW_FATAL"},
	{ML_CNXK_ETYPE_HW_WARNING, "HW_WARNING"},    {ML_CNXK_ETYPE_DRIVER, "DRIVER_ERROR"},
	{ML_CNXK_ETYPE_UNKNOWN, "UNKNOWN_ERROR"},
};
