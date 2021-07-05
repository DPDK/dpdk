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
	enum sfc_efx_dev_class dev_class = SFC_EFX_DEV_CLASS_NET;

	if (devargs == NULL)
		return dev_class;

	kvargs = rte_kvargs_parse(devargs->args, NULL);
	if (kvargs == NULL)
		return dev_class;

	if (rte_kvargs_count(kvargs, RTE_DEVARGS_KEY_CLASS) != 0) {
		rte_kvargs_process(kvargs, RTE_DEVARGS_KEY_CLASS,
				   sfc_efx_kvarg_dev_class_handler, &dev_class);
	}

	rte_kvargs_free(kvargs);

	return dev_class;
}

static efx_rc_t
sfc_efx_find_mem_bar(efsys_pci_config_t *configp, int bar_index,
		     efsys_bar_t *barp)
{
	efsys_bar_t result;
	struct rte_pci_device *dev;

	memset(&result, 0, sizeof(result));

	if (bar_index < 0 || bar_index >= PCI_MAX_RESOURCE)
		return -EINVAL;

	dev = configp->espc_dev;

	result.esb_rid = bar_index;
	result.esb_dev = dev;
	result.esb_base = dev->mem_resource[bar_index].addr;

	*barp = result;

	return 0;
}

static efx_rc_t
sfc_efx_pci_config_readd(efsys_pci_config_t *configp, uint32_t offset,
			 efx_dword_t *edp)
{
	int rc;

	rc = rte_pci_read_config(configp->espc_dev, edp->ed_u32, sizeof(*edp),
				 offset);

	return (rc < 0 || rc != sizeof(*edp)) ? EIO : 0;
}

int
sfc_efx_family(struct rte_pci_device *pci_dev,
	       efx_bar_region_t *mem_ebrp, efx_family_t *family)
{
	static const efx_pci_ops_t ops = {
		.epo_config_readd = sfc_efx_pci_config_readd,
		.epo_find_mem_bar = sfc_efx_find_mem_bar,
	};

	efsys_pci_config_t espcp;
	int rc;

	espcp.espc_dev = pci_dev;

	rc = efx_family_probe_bar(pci_dev->id.vendor_id,
				  pci_dev->id.device_id,
				  &espcp, &ops, family, mem_ebrp);

	return rc;
}

RTE_INIT(sfc_efx_register_logtype)
{
	int ret;

	ret = rte_log_register_type_and_pick_level("pmd.common.sfc_efx",
						   RTE_LOG_NOTICE);
	sfc_efx_logtype = (ret < 0) ? RTE_LOGTYPE_PMD : ret;
}
