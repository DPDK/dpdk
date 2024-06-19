/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Corigine, Inc.
 * All rights reserved.
 */

#ifndef __NFP_SERVICE_H__
#define __NFP_SERVICE_H__

#include <rte_service_component.h>

struct nfp_service_info {
	uint32_t id;
};

int nfp_service_disable(struct nfp_service_info *info);
int nfp_service_enable(const struct rte_service_spec *service_spec,
		struct nfp_service_info *info);

#endif /* __NFP_SERVICE_H__ */
