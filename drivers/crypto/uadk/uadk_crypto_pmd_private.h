/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2022-2023 Huawei Technologies Co.,Ltd. All rights reserved.
 * Copyright 2022-2023 Linaro ltd.
 */

#ifndef _UADK_CRYPTO_PMD_PRIVATE_H_
#define _UADK_CRYPTO_PMD_PRIVATE_H_

enum uadk_crypto_version {
	UADK_CRYPTO_V2,
	UADK_CRYPTO_V3,
};

struct uadk_crypto_priv {
	enum uadk_crypto_version version;
} __rte_cache_aligned;

extern int uadk_crypto_logtype;

#define UADK_LOG(level, fmt, ...)  \
	rte_log(RTE_LOG_ ## level, uadk_crypto_logtype,  \
		"%s() line %u: " fmt "\n", __func__, __LINE__,  \
		## __VA_ARGS__)

#endif /* _UADK_CRYPTO_PMD_PRIVATE_H_ */
