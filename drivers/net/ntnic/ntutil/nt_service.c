/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Napatech A/S
 */

#include <rte_service.h>
#include <rte_cycles.h>

#include "nt_service.h"
#include "ntlog.h"

#define NT_SERVICE_UNKNOWN_ID (-1)

static struct nt_service g_nt_services[RTE_NTNIC_SERVICE_MAX] = {
	[RTE_NTNIC_SERVICE_FLM_UPDATE] = {
		.tag = RTE_NTNIC_SERVICE_FLM_UPDATE,
		.id = NT_SERVICE_UNKNOWN_ID,
		.lcore = RTE_MAX_LCORE,
		.initialized = false,
	},
	[RTE_NTNIC_SERVICE_STAT] = {
		.tag = RTE_NTNIC_SERVICE_STAT,
		.id = NT_SERVICE_UNKNOWN_ID,
		.lcore = RTE_MAX_LCORE,
		.initialized = false,
	},
	[RTE_NTNIC_SERVICE_PORT_0_EVENT] = {
		.tag = RTE_NTNIC_SERVICE_PORT_0_EVENT,
		.id = NT_SERVICE_UNKNOWN_ID,
		.lcore = RTE_MAX_LCORE,
		.initialized = false,
	},
	[RTE_NTNIC_SERVICE_PORT_1_EVENT] = {
		.tag = RTE_NTNIC_SERVICE_PORT_1_EVENT,
		.id = NT_SERVICE_UNKNOWN_ID,
		.lcore = RTE_MAX_LCORE,
		.initialized = false,
	},
};

inline struct nt_service *nthw_service_get_info(const enum rte_ntnic_service_tag tag)
{
	if (tag < 0 || tag >= RTE_NTNIC_SERVICE_MAX)
		return NULL;

	return &g_nt_services[tag];
}

int nthw_service_add(struct rte_service_spec *srv_spec, const enum rte_ntnic_service_tag tag)
{
	if (srv_spec == NULL || tag < 0 || tag >= RTE_NTNIC_SERVICE_MAX) {
		NT_LOG(ERR, NTNIC, "Invalid service specification or service tag");
		return -1;
	}

	int ret = rte_service_component_register(srv_spec, &g_nt_services[tag].id);
	if (ret < 0) {
		NT_LOG(ERR, NTNIC, "Failed to register service %s: error: %d",
			   srv_spec->name, ret);
		return ret;
	}

	const uint32_t service_id = g_nt_services[tag].id;

	NT_LOG(DBG, NTNIC, "Service %s registered with ID %u",
		   srv_spec->name, service_id);

	rte_service_component_runstate_set(service_id, 1);

	ret = rte_service_runstate_set(service_id, 1);
	if (ret < 0) {
		NT_LOG(ERR, NTNIC, "Failed to start service %s: error: %d",
			   srv_spec->name, ret);
		rte_service_component_unregister(service_id);
		return ret;
	}

	return 0;
}

int nthw_service_del(const enum rte_ntnic_service_tag tag)
{
	if (tag < 0 || tag >= RTE_NTNIC_SERVICE_MAX) {
		NT_LOG(ERR, NTNIC, "Invalid service tag");
		return -1;
	}

	struct nt_service *info = &g_nt_services[tag];

	const char *service_name = rte_service_get_name(info->id);

	rte_service_component_runstate_set(info->id, 0);

	const uint32_t timeout_count = 10000;

	for (uint32_t i = 0; i < timeout_count; i++) {
		if (rte_service_may_be_active(info->id) == 0)
			break;
		rte_delay_ms(1);
	}

	int ret = rte_service_runstate_set(info->id, 0);
	if (ret < 0) {
		NT_LOG(ERR, NTNIC, "Failed to stop service %s: error: %d",
			   service_name, ret);
		return ret;
	}

	ret = rte_service_component_unregister(info->id);
	if (ret < 0) {
		NT_LOG(ERR, NTNIC, "Failed to unregister service %s: error: %d",
			   service_name, ret);
		return ret;
	}

	NT_LOG(DBG, NTNIC, "Service ID %d unregistered", info->id);

	g_nt_services[tag].id = NT_SERVICE_UNKNOWN_ID;

	return 0;
}
