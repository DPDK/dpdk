/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 StarFive
 * Copyright(c) 2022 SiFive
 * Copyright(c) 2022 Semihalf
 */

#ifndef RTE_MEMCPY_RISCV_H
#define RTE_MEMCPY_RISCV_H

#include <stdint.h>
#include <string.h>

#include "rte_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "generic/rte_memcpy.h"

static inline void
rte_mov16(uint8_t *dst, const uint8_t *src)
{
	memcpy(dst, src, 16);
}

static inline void
rte_mov32(uint8_t *dst, const uint8_t *src)
{
	memcpy(dst, src, 32);
}

static inline void
rte_mov48(uint8_t *dst, const uint8_t *src)
{
	memcpy(dst, src, 48);
}

static inline void
rte_mov64(uint8_t *dst, const uint8_t *src)
{
	memcpy(dst, src, 64);
}

static inline void
rte_mov128(uint8_t *dst, const uint8_t *src)
{
	memcpy(dst, src, 128);
}

static inline void
rte_mov256(uint8_t *dst, const uint8_t *src)
{
	memcpy(dst, src, 256);
}

#define rte_memcpy(d, s, n)	memcpy((d), (s), (n))

#ifdef __cplusplus
}
#endif

#endif /* RTE_MEMCPY_RISCV_H */
