/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020-2021 Xilinx, Inc.
 */

#include <rte_errno.h>
#include <rte_ether.h>
#include <rte_kvargs.h>

#include "efx.h"
#include "efx_impl.h"
#include "sfc_vdpa.h"

static inline int
sfc_vdpa_get_eth_addr(const char *key __rte_unused,
		      const char *value, void *extra_args)
{
	struct rte_ether_addr *mac_addr = extra_args;

	if (value == NULL || extra_args == NULL)
		return -EINVAL;

	/* Convert string with Ethernet address to an ether_addr */
	rte_ether_unformat_addr(value, mac_addr);

	return 0;
}

static int
sfc_vdpa_set_mac_filter(efx_nic_t *nic, efx_filter_spec_t *spec,
			int qid, uint8_t *eth_addr)
{
	int rc;

	if (nic == NULL || spec == NULL)
		return -1;

	spec->efs_priority = EFX_FILTER_PRI_MANUAL;
	spec->efs_flags = EFX_FILTER_FLAG_RX;
	spec->efs_dmaq_id = qid;

	if (eth_addr == NULL)
		rc = efx_filter_spec_set_mc_def(spec);
	else
		rc = efx_filter_spec_set_eth_local(spec,
						   EFX_FILTER_SPEC_VID_UNSPEC,
						   eth_addr);
	if (rc != 0)
		return rc;

	rc = efx_filter_insert(nic, spec);
	if (rc != 0)
		return rc;

	return rc;
}

int sfc_vdpa_filter_config(struct sfc_vdpa_ops_data *ops_data)
{
	int rc;
	int qid;
	efx_nic_t *nic;
	struct rte_ether_addr bcast_eth_addr;
	struct rte_ether_addr ucast_eth_addr;
	struct sfc_vdpa_adapter *sva = ops_data->dev_handle;
	efx_filter_spec_t *spec;

	sfc_vdpa_log_init(sva, "entry");

	nic = sva->nic;

	sfc_vdpa_log_init(sva, "process kvarg");

	/* skip MAC filter configuration if mac address is not provided */
	if (rte_kvargs_count(sva->kvargs, SFC_VDPA_MAC_ADDR) == 0) {
		sfc_vdpa_warn(sva,
			      "MAC address is not provided, skipping MAC Filter Config");
		return -1;
	}

	rc = rte_kvargs_process(sva->kvargs, SFC_VDPA_MAC_ADDR,
				&sfc_vdpa_get_eth_addr,
				&ucast_eth_addr);
	if (rc < 0)
		return -1;

	/* create filters on the base queue */
	qid = SFC_VDPA_GET_VI_INDEX(0);

	sfc_vdpa_log_init(sva, "insert broadcast mac filter");

	EFX_MAC_BROADCAST_ADDR_SET(bcast_eth_addr.addr_bytes);
	spec = &sva->filters.spec[SFC_VDPA_BCAST_MAC_FILTER];

	rc = sfc_vdpa_set_mac_filter(nic, spec, qid,
				     bcast_eth_addr.addr_bytes);
	if (rc != 0)
		sfc_vdpa_err(ops_data->dev_handle,
			     "broadcast MAC filter insertion failed: %s",
			     rte_strerror(rc));
	else
		sva->filters.filter_cnt++;

	sfc_vdpa_log_init(sva, "insert unicast mac filter");
	spec = &sva->filters.spec[SFC_VDPA_UCAST_MAC_FILTER];

	rc = sfc_vdpa_set_mac_filter(nic, spec, qid,
				     ucast_eth_addr.addr_bytes);
	if (rc != 0)
		sfc_vdpa_err(sva, "unicast MAC filter insertion failed: %s",
			     rte_strerror(rc));
	else
		sva->filters.filter_cnt++;

	sfc_vdpa_log_init(sva, "insert unknown mcast filter");
	spec = &sva->filters.spec[SFC_VDPA_MCAST_DST_FILTER];

	rc = sfc_vdpa_set_mac_filter(nic, spec, qid, NULL);
	if (rc != 0)
		sfc_vdpa_err(sva,
			     "mcast filter insertion failed: %s",
			     rte_strerror(rc));
	else
		sva->filters.filter_cnt++;

	sfc_vdpa_log_init(sva, "done");

	return rc;
}

int sfc_vdpa_filter_remove(struct sfc_vdpa_ops_data *ops_data)
{
	int i, rc = 0;
	struct sfc_vdpa_adapter *sva = ops_data->dev_handle;
	efx_nic_t *nic;

	nic = sva->nic;

	for (i = 0; i < sva->filters.filter_cnt; i++) {
		rc = efx_filter_remove(nic, &(sva->filters.spec[i]));
		if (rc != 0)
			sfc_vdpa_err(sva,
				     "remove HW filter failed for entry %d: %s",
				     i, rte_strerror(rc));
	}

	sva->filters.filter_cnt = 0;

	return rc;
}
