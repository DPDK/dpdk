/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019-2021 Xilinx, Inc.
 * Copyright(c) 2019 Solarflare Communications Inc.
 *
 * This software was jointly developed between OKTET Labs (under contract
 * for Solarflare) and Solarflare Communications, Inc.
 */

#include <string.h>
#include <rte_log.h>
#include <rte_kvargs.h>
#include <rte_devargs.h>

#include "sfc_efx_log.h"
#include "sfc_efx.h"

uint32_t sfc_efx_logtype;

static int
sfc_efx_kvarg_dev_class_handler(__rte_unused const char *key,
				const char *class_str, void *opaque)
{
	enum sfc_efx_dev_class *dev_class = opaque;

	if (class_str == NULL)
		return *dev_class;

	if (strcmp(class_str, "vdpa") == 0) {
		*dev_class = SFC_EFX_DEV_CLASS_VDPA;
	} else if (strcmp(class_str, "net") == 0) {
		*dev_class = SFC_EFX_DEV_CLASS_NET;
	} else {
		SFC_EFX_LOG(ERR, "Unsupported class %s.", class_str);
		*dev_class = SFC_EFX_DEV_CLASS_INVALID;
	}

	return 0;
}

enum sfc_efx_dev_class
sfc_efx_dev_class_get(struct rte_devargs *devargs)
{
	struct rte_kvargs *kvargs;
	const char *key = SFC_EFX_KVARG_DEV_CLASS;
	enum sfc_efx_dev_class dev_class = SFC_EFX_DEV_CLASS_NET;

	if (devargs == NULL)
		return dev_class;

	kvargs = rte_kvargs_parse(devargs->args, NULL);
	if (kvargs == NULL)
		return dev_class;

	if (rte_kvargs_count(kvargs, key) != 0) {
		rte_kvargs_process(kvargs, key, sfc_efx_kvarg_dev_class_handler,
				   &dev_class);
	}

	rte_kvargs_free(kvargs);

	return dev_class;
}

RTE_INIT(sfc_efx_register_logtype)
{
	int ret;

	ret = rte_log_register_type_and_pick_level("pmd.common.sfc_efx",
						   RTE_LOG_NOTICE);
	sfc_efx_logtype = (ret < 0) ? RTE_LOGTYPE_PMD : ret;
}
