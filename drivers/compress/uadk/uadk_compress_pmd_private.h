/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2024-2025 Huawei Technologies Co.,Ltd. All rights reserved.
 * Copyright 2024-2025 Linaro ltd.
 */

#ifndef _UADK_COMPRESS_PMD_PRIVATE_H_
#define _UADK_COMPRESS_PMD_PRIVATE_H_

struct uadk_compress_priv {
	bool env_init;
};

extern int uadk_compress_logtype;

#define UADK_LOG(level, fmt, ...)  \
	rte_log(RTE_LOG_ ## level, uadk_compress_logtype,  \
		"%s() line %u: " fmt "\n", __func__, __LINE__,  \
		## __VA_ARGS__)

#endif /* _UADK_COMPRESS_PMD_PRIVATE_H_ */
