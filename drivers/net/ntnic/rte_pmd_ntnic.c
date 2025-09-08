/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Napatech A/S
 */

#include <eal_export.h>
#include <rte_service.h>
#include <rte_pmd_ntnic.h>

#include <stdint.h>

#include "nt_service.h"
#include "ntlog.h"

static int nthw_service_is_mapped_to_lcore(enum rte_ntnic_service_tag tag)
{
	struct nt_service *service = nthw_service_get_info(tag);

	uint32_t cores[RTE_MAX_LCORE] = {0};
	int32_t lcore_count = rte_service_lcore_list(cores, RTE_MAX_LCORE);

	for (int32_t i = 0; i < lcore_count; i++) {
		if (rte_service_map_lcore_get(service->id, cores[i]))
			return cores[i];
	}
	return RTE_MAX_LCORE;
}


RTE_EXPORT_SYMBOL(rte_pmd_ntnic_service_set_lcore)
int rte_pmd_ntnic_service_set_lcore(enum rte_ntnic_service_tag tag, uint32_t lcore_id)
{
	if (tag < 0 || tag >= RTE_NTNIC_SERVICE_MAX || lcore_id >= RTE_MAX_LCORE) {
		NT_LOG(ERR, NTNIC, "Invalid service tag or lcore ID");
		return -1;
	}

	struct nt_service *srv = nthw_service_get_info(tag);
	const char *service_name = rte_service_get_name(srv->id);

	uint32_t service_core = nthw_service_is_mapped_to_lcore(tag);

	if (service_core != RTE_MAX_LCORE) {
		NT_LOG(WRN, NTNIC, "Service %s[id=%u] is already mapped to lcore: %u", service_name,
			srv->id, service_core);
		/* Mapping by application has higher priority then 1:1 default mapping.
		 * Disable previous mapping and do remapping to new lcore.
		 */

		rte_service_runstate_set(srv->id, 0);

		NT_SERVICE_SET_STATE(srv, false);

		int timeout_count = 10000;

		while (rte_service_may_be_active(srv->id) != 0) {
			if (--timeout_count <= 0) {
				NT_LOG(ERR, NTNIC, "Failed to stop service %s[id=%d] on lcore %u",
					service_name, srv->id, service_core);
				return -1;
			}
			rte_delay_ms(1);
		}

		rte_service_map_lcore_set(srv->id, service_core, 0);
		rte_service_runstate_set(srv->id, 1);
	}

	int ret = rte_service_lcore_add(lcore_id);
	if (ret < 0 && ret != -EALREADY) {
		NT_LOG(ERR, NTNIC, "Failed to add service lcore %u for service %s: error: %d",
			   lcore_id, service_name, ret);
		return ret;
	}

	ret = rte_service_map_lcore_set(srv->id, lcore_id, 1);
	if (ret < 0) {
		NT_LOG(ERR, NTNIC, "Failed to map service %s to lcore %u: error: %d",
			   service_name, lcore_id, ret);
		return ret;
	}

	NT_LOG(DBG, NTNIC, "Service %s[id=%d] is mapped to lcore %u", service_name, srv->id,
		lcore_id);

	return 0;
}

RTE_EXPORT_SYMBOL(rte_pmd_ntnic_service_get_id)
int rte_pmd_ntnic_service_get_id(enum rte_ntnic_service_tag tag)
{
	if (tag < 0 || tag >= RTE_NTNIC_SERVICE_MAX) {
		NT_LOG(ERR, NTNIC, "Invalid service tag");
		return -1;
	}

	struct nt_service *service = nthw_service_get_info(tag);
	if (service == NULL) {
		NT_LOG(ERR, NTNIC, "Service with tag %d not found", tag);
		return -1;
	}

	return service->id;
}
