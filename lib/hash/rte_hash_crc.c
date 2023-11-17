/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <rte_cpuflags.h>
#include <rte_log.h>

#include "rte_hash_crc.h"

RTE_LOG_REGISTER_SUFFIX(hash_crc_logtype, crc, INFO);
#define RTE_LOGTYPE_HASH_CRC hash_crc_logtype
#define HASH_CRC_LOG(level, ...) \
	RTE_LOG_LINE(level, HASH_CRC, "" __VA_ARGS__)

uint8_t rte_hash_crc32_alg = CRC32_SW;

/**
 * Allow or disallow use of SSE4.2/ARMv8 intrinsics for CRC32 hash
 * calculation.
 *
 * @param alg
 *   An OR of following flags:
 *   - (CRC32_SW) Don't use SSE4.2/ARMv8 intrinsics (default non-[x86/ARMv8])
 *   - (CRC32_SSE42) Use SSE4.2 intrinsics if available
 *   - (CRC32_SSE42_x64) Use 64-bit SSE4.2 intrinsic if available (default x86)
 *   - (CRC32_ARM64) Use ARMv8 CRC intrinsic if available (default ARMv8)
 *
 */
void
rte_hash_crc_set_alg(uint8_t alg)
{
	rte_hash_crc32_alg = CRC32_SW;

	if (alg == CRC32_SW)
		return;

#if defined RTE_ARCH_X86
	if (!(alg & CRC32_SSE42_x64))
		HASH_CRC_LOG(WARNING,
			"Unsupported CRC32 algorithm requested using CRC32_x64/CRC32_SSE42");
	if (!rte_cpu_get_flag_enabled(RTE_CPUFLAG_EM64T) || alg == CRC32_SSE42)
		rte_hash_crc32_alg = CRC32_SSE42;
	else
		rte_hash_crc32_alg = CRC32_SSE42_x64;
#endif

#if defined RTE_ARCH_ARM64
	if (!(alg & CRC32_ARM64))
		HASH_CRC_LOG(WARNING,
			"Unsupported CRC32 algorithm requested using CRC32_ARM64");
	if (rte_cpu_get_flag_enabled(RTE_CPUFLAG_CRC32))
		rte_hash_crc32_alg = CRC32_ARM64;
#endif

	if (rte_hash_crc32_alg == CRC32_SW)
		HASH_CRC_LOG(WARNING,
			"Unsupported CRC32 algorithm requested using CRC32_SW");
}

/* Setting the best available algorithm */
RTE_INIT(rte_hash_crc_init_alg)
{
#if defined(RTE_ARCH_X86)
	rte_hash_crc_set_alg(CRC32_SSE42_x64);
#elif defined(RTE_ARCH_ARM64) && defined(__ARM_FEATURE_CRC32)
	rte_hash_crc_set_alg(CRC32_ARM64);
#else
	rte_hash_crc_set_alg(CRC32_SW);
#endif
}
