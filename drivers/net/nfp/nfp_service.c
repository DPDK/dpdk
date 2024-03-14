/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Corigine, Inc.
 * All rights reserved.
 */

#include "nfp_service.h"

#include "nfpcore/nfp_cpp.h"
#include "nfp_logs.h"

/* Disable service and try to get service status */
#define NFP_SERVICE_DISABLE_WAIT_COUNT 3000

static int
nfp_map_service(struct nfp_service_info *info)
{
	int32_t ret;
	uint32_t slcore = 0;
	int32_t slcore_count;
	uint8_t service_count;
	const char *service_name;
	uint32_t slcore_array[RTE_MAX_LCORE];
	uint8_t min_service_count = UINT8_MAX;

	slcore_count = rte_service_lcore_list(slcore_array, RTE_MAX_LCORE);
	if (slcore_count <= 0) {
		PMD_DRV_LOG(DEBUG, "No service cores found");
		return -ENOENT;
	}

	/*
	 * Find a service core with the least number of services already
	 * registered to it
	 */
	while (slcore_count--) {
		service_count = rte_service_lcore_count_services(slcore_array[slcore_count]);
		if (service_count < min_service_count) {
			slcore = slcore_array[slcore_count];
			min_service_count = service_count;
		}
	}

	service_name = rte_service_get_name(info->id);
	PMD_INIT_LOG(INFO, "Mapping service %s to core %u", service_name, slcore);

	ret = rte_service_map_lcore_set(info->id, slcore, 1);
	if (ret != 0) {
		PMD_DRV_LOG(DEBUG, "Could not map flower service");
		return -ENOENT;
	}

	rte_service_runstate_set(info->id, 1);
	rte_service_component_runstate_set(info->id, 1);
	rte_service_lcore_start(slcore);
	if (rte_service_may_be_active(slcore))
		PMD_DRV_LOG(INFO, "The service %s is running", service_name);
	else
		PMD_DRV_LOG(ERR, "The service %s is not running", service_name);

	info->lcore = slcore;

	return 0;
}

int
nfp_service_enable(const struct rte_service_spec *service_spec,
		struct nfp_service_info *info)
{
	int ret;

	/* Register the service */
	ret = rte_service_component_register(service_spec, &info->id);
	if (ret != 0) {
		PMD_DRV_LOG(DEBUG, "Could not register %s", service_spec->name);
		return -EINVAL;
	}

	/* Map it to available service core */
	ret = nfp_map_service(info);
	if (ret != 0) {
		PMD_DRV_LOG(DEBUG, "Could not map %s", service_spec->name);
		return -EINVAL;
	}

	PMD_DRV_LOG(DEBUG, "Enable service %s successfully", service_spec->name);

	return 0;
}

int
nfp_service_disable(struct nfp_service_info *info)
{
	int ret;
	uint32_t i;
	const char *service_name;

	service_name = rte_service_get_name(info->id);
	if (service_name == NULL) {
		PMD_DRV_LOG(ERR, "Could not find service %u", info->id);
		return -EINVAL;
	}

	rte_service_runstate_set(info->id, 0);
	rte_service_component_runstate_set(info->id, 0);

	for (i = 0; i < NFP_SERVICE_DISABLE_WAIT_COUNT; i++) {
		if (rte_service_may_be_active(info->id) == 0)
			break;
		rte_delay_ms(1);
	}

	if (i == NFP_SERVICE_DISABLE_WAIT_COUNT)
		PMD_DRV_LOG(ERR, "Could not stop service %s", service_name);

	ret = rte_service_map_lcore_set(info->id, info->lcore, 0);
	if (ret != 0) {
		PMD_DRV_LOG(DEBUG, "Could not unmap flower service");
		return -ENOENT;
	}

	rte_service_component_unregister(info->id);

	return 0;
}
