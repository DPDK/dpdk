/*-
 * Copyright (c) 2016 Solarflare Communications Inc.
 * All rights reserved.
 *
 * This software was jointly developed between OKTET Labs (under contract
 * for Solarflare) and Solarflare Communications, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <rte_dev.h>
#include <rte_ethdev.h>
#include <rte_pci.h>

#include "efx.h"

#include "sfc.h"
#include "sfc_debug.h"
#include "sfc_log.h"
#include "sfc_kvargs.h"


static void
sfc_dev_infos_get(struct rte_eth_dev *dev, struct rte_eth_dev_info *dev_info)
{
	struct sfc_adapter *sa = dev->data->dev_private;

	sfc_log_init(sa, "entry");

	dev_info->pci_dev = RTE_DEV_TO_PCI(dev->device);
}

static const struct eth_dev_ops sfc_eth_dev_ops = {
	.dev_infos_get			= sfc_dev_infos_get,
};

static int
sfc_eth_dev_init(struct rte_eth_dev *dev)
{
	struct sfc_adapter *sa = dev->data->dev_private;
	struct rte_pci_device *pci_dev = SFC_DEV_TO_PCI(dev);
	int rc;
	const efx_nic_cfg_t *encp;
	const struct ether_addr *from;

	/* Required for logging */
	sa->eth_dev = dev;

	/* Copy PCI device info to the dev->data */
	rte_eth_copy_pci_info(dev, pci_dev);

	rc = sfc_kvargs_parse(sa);
	if (rc != 0)
		goto fail_kvargs_parse;

	rc = sfc_kvargs_process(sa, SFC_KVARG_DEBUG_INIT,
				sfc_kvarg_bool_handler, &sa->debug_init);
	if (rc != 0)
		goto fail_kvarg_debug_init;

	sfc_log_init(sa, "entry");

	dev->data->mac_addrs = rte_zmalloc("sfc", ETHER_ADDR_LEN, 0);
	if (dev->data->mac_addrs == NULL) {
		rc = ENOMEM;
		goto fail_mac_addrs;
	}

	sfc_adapter_lock_init(sa);
	sfc_adapter_lock(sa);

	sfc_log_init(sa, "attaching");
	rc = sfc_attach(sa);
	if (rc != 0)
		goto fail_attach;

	encp = efx_nic_cfg_get(sa->nic);

	/*
	 * The arguments are really reverse order in comparison to
	 * Linux kernel. Copy from NIC config to Ethernet device data.
	 */
	from = (const struct ether_addr *)(encp->enc_mac_addr);
	ether_addr_copy(from, &dev->data->mac_addrs[0]);

	dev->dev_ops = &sfc_eth_dev_ops;

	sfc_adapter_unlock(sa);

	sfc_log_init(sa, "done");
	return 0;

fail_attach:
	sfc_adapter_unlock(sa);
	sfc_adapter_lock_fini(sa);
	rte_free(dev->data->mac_addrs);
	dev->data->mac_addrs = NULL;

fail_mac_addrs:
fail_kvarg_debug_init:
	sfc_kvargs_cleanup(sa);

fail_kvargs_parse:
	sfc_log_init(sa, "failed %d", rc);
	SFC_ASSERT(rc > 0);
	return -rc;
}

static int
sfc_eth_dev_uninit(struct rte_eth_dev *dev)
{
	struct sfc_adapter *sa = dev->data->dev_private;

	sfc_log_init(sa, "entry");

	sfc_adapter_lock(sa);

	sfc_detach(sa);

	rte_free(dev->data->mac_addrs);
	dev->data->mac_addrs = NULL;

	dev->dev_ops = NULL;

	sfc_kvargs_cleanup(sa);

	sfc_adapter_unlock(sa);
	sfc_adapter_lock_fini(sa);

	sfc_log_init(sa, "done");

	/* Required for logging, so cleanup last */
	sa->eth_dev = NULL;
	return 0;
}

static const struct rte_pci_id pci_id_sfc_efx_map[] = {
	{ RTE_PCI_DEVICE(EFX_PCI_VENID_SFC, EFX_PCI_DEVID_FARMINGDALE) },
	{ RTE_PCI_DEVICE(EFX_PCI_VENID_SFC, EFX_PCI_DEVID_GREENPORT) },
	{ RTE_PCI_DEVICE(EFX_PCI_VENID_SFC, EFX_PCI_DEVID_MEDFORD) },
	{ .vendor_id = 0 /* sentinel */ }
};

static struct eth_driver sfc_efx_pmd = {
	.pci_drv = {
		.id_table = pci_id_sfc_efx_map,
		.drv_flags =
			RTE_PCI_DRV_NEED_MAPPING,
		.probe = rte_eth_dev_pci_probe,
		.remove = rte_eth_dev_pci_remove,
	},
	.eth_dev_init = sfc_eth_dev_init,
	.eth_dev_uninit = sfc_eth_dev_uninit,
	.dev_private_size = sizeof(struct sfc_adapter),
};

RTE_PMD_REGISTER_PCI(net_sfc_efx, sfc_efx_pmd.pci_drv);
RTE_PMD_REGISTER_PCI_TABLE(net_sfc_efx, pci_id_sfc_efx_map);
RTE_PMD_REGISTER_PARAM_STRING(net_sfc_efx,
	SFC_KVARG_DEBUG_INIT "=" SFC_KVARG_VALUES_BOOL);
