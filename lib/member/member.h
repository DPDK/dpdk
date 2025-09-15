/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Red Hat, Inc.
 */

#include <rte_log.h>

extern int librte_member_logtype;
#define RTE_LOGTYPE_MEMBER librte_member_logtype

#define MEMBER_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, MEMBER, \
		"%s(): ", __func__, __VA_ARGS__)

/* Hash function used by membership library. */
#if defined(RTE_ARCH_X86) || defined(__ARM_FEATURE_CRC32)
#include <rte_hash_crc.h>
#define MEMBER_HASH_FUNC       rte_hash_crc
#else
#include <rte_jhash.h>
#define MEMBER_HASH_FUNC       rte_jhash
#endif
