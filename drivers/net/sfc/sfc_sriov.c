/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019-2020 Xilinx, Inc.
 * Copyright(c) 2019 Solarflare Communications Inc.
 *
 * This software was jointly developed between OKTET Labs (under contract
 * for Solarflare) and Solarflare Communications, Inc.
 */

#include <rte_common.h>
#include <rte_bus_pci.h>

#include "sfc.h"
#include "sfc_log.h"

#include "efx.h"


/*
 * Check if a MAC address is already assigned to one of previously
 * configured vPorts (either PF itself or one of already configured VFs).
 *
 * Typically the first vPort which corresponds to PF has globally
 * administered unicast address, but it still could be locally
 * administered if user assigned it or in the case of unconfigured NIC.
 * So, it is safer to include it as well in uniqueness check.
 */
static bool
sriov_mac_addr_assigned(const efx_vport_config_t *vport_config,
			unsigned int num, const uint8_t *mac_addr)
{
	unsigned int i;

	/* Check PF's MAC address as well as explained above */
	for (i = 0; i < num; ++i) {
		if (memcmp(mac_addr, vport_config[i].evc_mac_addr,
			   sizeof(vport_config[i].evc_mac_addr)) == 0)
			return true;
	}

	return false;
}

int
sfc_sriov_attach(struct sfc_adapter *sa)
{
	const struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(sa->eth_dev);
	struct sfc_sriov *sriov = &sa->sriov;
	efx_vport_config_t *vport_config;
	unsigned int i;
	int rc;

	sfc_log_init(sa, "entry");

	sriov->num_vfs = pci_dev->max_vfs;
	if (sriov->num_vfs == 0)
		goto done;

	vport_config = calloc(sriov->num_vfs + 1, sizeof(*vport_config));
	if (vport_config == NULL) {
		rc = ENOMEM;
		goto fail_alloc_vport_config;
	}

	vport_config[0].evc_function = 0xffff;
	vport_config[0].evc_vid = EFX_VF_VID_DEFAULT;
	vport_config[0].evc_vlan_restrict = B_FALSE;

	for (i = 1; i <= sriov->num_vfs; ++i) {
		vport_config[i].evc_function = i - 1;
		vport_config[i].evc_vid = EFX_VF_VID_DEFAULT;
		vport_config[i].evc_vlan_restrict = B_FALSE;
		do {
			rte_eth_random_addr(vport_config[i].evc_mac_addr);
		} while (sriov_mac_addr_assigned(vport_config, i,
						 vport_config[i].evc_mac_addr));
	}

	sriov->vport_config = vport_config;

done:
	sfc_log_init(sa, "done");
	return 0;

fail_alloc_vport_config:
	sriov->num_vfs = 0;
	return rc;
}

void
sfc_sriov_detach(struct sfc_adapter *sa)
{
	struct sfc_sriov *sriov = &sa->sriov;

	sfc_log_init(sa, "entry");

	free(sriov->vport_config);
	sriov->vport_config = NULL;
	sriov->num_vfs = 0;

	sfc_log_init(sa, "done");
}

int
sfc_sriov_vswitch_create(struct sfc_adapter *sa)
{
	struct sfc_sriov *sriov = &sa->sriov;
	efx_vport_config_t *vport_config = sriov->vport_config;
	int rc;

	sfc_log_init(sa, "entry");

	if (sriov->num_vfs == 0) {
		sfc_log_init(sa, "no VFs enabled");
		goto done;
	}

	rc = efx_evb_init(sa->nic);
	if (rc != 0) {
		sfc_err(sa, "EVB init failed %d", rc);
		goto fail_evb_init;
	}

	RTE_BUILD_BUG_ON(sizeof(sa->port.default_mac_addr) !=
			 sizeof(vport_config[0].evc_mac_addr));
	rte_ether_addr_copy(&sa->port.default_mac_addr,
		(struct rte_ether_addr *)vport_config[0].evc_mac_addr);

	rc = efx_evb_vswitch_create(sa->nic, sriov->num_vfs + 1,
				    vport_config, &sriov->vswitch);
	if (rc != 0) {
		sfc_err(sa, "EVB vSwitch create failed %d", rc);
		goto fail_evb_vswitch_create;
	}

done:
	sfc_log_init(sa, "done");
	return 0;

fail_evb_vswitch_create:
	efx_evb_fini(sa->nic);

fail_evb_init:
	return rc;
}

void
sfc_sriov_vswitch_destroy(struct sfc_adapter *sa)
{
	struct sfc_sriov *sriov = &sa->sriov;
	int rc;

	sfc_log_init(sa, "entry");

	if (sriov->num_vfs == 0)
		goto done;

	rc = efx_evb_vswitch_destroy(sa->nic, sriov->vswitch);
	if (rc != 0)
		sfc_err(sa, "efx_evb_vswitch_destroy() failed %d", rc);

	sriov->vswitch = NULL;

	efx_evb_fini(sa->nic);

done:
	sfc_log_init(sa, "done");
}
