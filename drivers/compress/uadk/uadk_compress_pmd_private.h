/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2024-2025 Huawei Technologies Co.,Ltd. All rights reserved.
 * Copyright 2024-2025 Linaro ltd.
 */

#ifndef _UADK_COMPRESS_PMD_PRIVATE_H_
#define _UADK_COMPRESS_PMD_PRIVATE_H_

struct uadk_compress_priv {
	bool env_init;
};

struct __rte_cache_aligned uadk_compress_qp {
	/* Ring for placing process packets */
	struct rte_ring *processed_pkts;
	/* Queue pair statistics */
	struct rte_compressdev_stats qp_stats;
	/* Queue Pair Identifier */
	uint16_t id;
	/* Unique Queue Pair Name */
	char name[RTE_COMPRESSDEV_NAME_MAX_LEN];
};

struct  uadk_compress_xform {
	handle_t handle;
	enum rte_comp_xform_type type;
};

extern int uadk_compress_logtype;

#define UADK_LOG(level, fmt, ...)  \
	rte_log(RTE_LOG_ ## level, uadk_compress_logtype,  \
		"%s() line %u: " fmt "\n", __func__, __LINE__,  \
		## __VA_ARGS__)

#endif /* _UADK_COMPRESS_PMD_PRIVATE_H_ */
