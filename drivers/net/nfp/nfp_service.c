/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Corigine, Inc.
 * All rights reserved.
 */

#include "nfp_service.h"

#include <errno.h>
#include <rte_cycles.h>

#include "nfp_logs.h"

/* Disable service and try to get service status */
#define NFP_SERVICE_DISABLE_WAIT_COUNT 3000

int
nfp_service_enable(const struct rte_service_spec *service_spec,
		struct nfp_service_info *info)
{
	int ret;
	int32_t lcore_count;

	lcore_count = rte_service_lcore_count();
	if (lcore_count == 0)
		return -ENOTSUP;

	/* Register the service */
	ret = rte_service_component_register(service_spec, &info->id);
	if (ret != 0) {
		PMD_DRV_LOG(DEBUG, "Could not register %s.", service_spec->name);
		return -EINVAL;
	}

	/* Set the NFP service runstate of a component. */
	rte_service_component_runstate_set(info->id, 1);

	PMD_DRV_LOG(DEBUG, "Enable service %s successfully.", service_spec->name);

	return 0;
}

int
nfp_service_disable(struct nfp_service_info *info)
{
	uint32_t i;
	const char *service_name;

	service_name = rte_service_get_name(info->id);
	if (service_name == NULL) {
		PMD_DRV_LOG(ERR, "Could not find service %u.", info->id);
		return -EINVAL;
	}

	rte_service_component_runstate_set(info->id, 0);

	for (i = 0; i < NFP_SERVICE_DISABLE_WAIT_COUNT; i++) {
		if (rte_service_may_be_active(info->id) == 0)
			break;
		rte_delay_ms(1);
	}

	if (i == NFP_SERVICE_DISABLE_WAIT_COUNT)
		PMD_DRV_LOG(ERR, "Could not stop service %s.", service_name);

	rte_service_component_unregister(info->id);

	return 0;
}
