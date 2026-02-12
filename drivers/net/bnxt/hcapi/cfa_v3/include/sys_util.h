/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023-2024 Broadcom
 * All rights reserved.
 */

#ifndef _SYS_UTIL_H_
#define _SYS_UTIL_H_

#include <stdint.h>

#define INVALID_U64 UINT64_MAX
#define INVALID_U32 UINT32_MAX
#define INVALID_U16 UINT16_MAX
#define INVALID_U8 UINT8_MAX

#ifndef ALIGN
#define ALIGN(x, a) (((x) + (a) - (1)) & ~((a) - (1)))
#endif

#define ALIGN_256(x) ALIGN(x, 256)
#define ALIGN_128(x) ALIGN(x, 128)
#define ALIGN_64(x) ALIGN(x, 64)
#define ALIGN_32(x) ALIGN(x, 32)
#define ALIGN_16(x) ALIGN(x, 16)
#define ALIGN_8(x) ALIGN(x, 8)
#define ALIGN_4(x) ALIGN(x, 4)

#define NUM_ALIGN_UNITS(x, unit) (((x) + (unit) - (1)) / (unit))
#define IS_POWER_2(x) (((x) != 0) && (((x) & ((x) - (1))) == 0))

#define NUM_WORDS_ALIGN_32BIT(x) (ALIGN_32(x) / BITS_PER_WORD)
#define NUM_WORDS_ALIGN_64BIT(x) (ALIGN_64(x) / BITS_PER_WORD)
#define NUM_WORDS_ALIGN_128BIT(x) (ALIGN_128(x) / BITS_PER_WORD)
#define NUM_WORDS_ALIGN_256BIT(x) (ALIGN_256(x) / BITS_PER_WORD)

#ifndef MAX
#define MAX(A, B) ((A) > (B) ? (A) : (B))
#endif

#ifndef MIN
#define MIN(A, B) ((A) < (B) ? (A) : (B))
#endif

#ifndef STRINGIFY
#define STRINGIFY(X) #X
#endif

#ifndef ARRAY_SIZE
#define ELEM_SIZE(ARRAY) sizeof((ARRAY)[0])
#define ARRAY_SIZE(ARRAY) (sizeof(ARRAY) / ELEM_SIZE(ARRAY))
#endif

#ifndef BITS_PER_BYTE
#define BITS_PER_BYTE (8)
#endif

#ifndef BITS_PER_WORD
#define BITS_PER_WORD (sizeof(uint32_t) * BITS_PER_BYTE)
#endif

#ifndef BITS_PER_DWORD
#define BITS_PER_DWORD (sizeof(uint64_t) * BITS_PER_BYTE)
#endif

/* Helper macros to get/set/clear Nth bit in a uint8_t bitmap */
#define BMP_GETBIT(BMP, N)                                                     \
	((*((uint8_t *)(BMP) + ((N) / 8)) >> ((N) % 8)) & 0x1)
#define BMP_SETBIT(BMP, N)                                                     \
	do {                                                                   \
		uint32_t n = (N);                                              \
		*((uint8_t *)(BMP) + (n / 8)) |= (0x1U << (n % 8));            \
	} while (0)
#define BMP_CLRBIT(BMP, N)                                                     \
	do {                                                                   \
		uint32_t n = (N);                                              \
		*((uint8_t *)(BMP) + (n / 8)) &=                               \
			(uint8_t)(~(0x1U << (n % 8)));                         \
	} while (0)

#ifndef STATIC_ASSERT
#ifndef NETXTREME_UT_SUPPORT
#define STATIC_ASSERT_TYPE_DEFINE(cntr) STATIC_ASSERT_TYPE##cntr
#define STATIC_ASSERT(x)                                                       \
	typedef int STATIC_ASSERT_TYPE_DEFINE(__COUNTER__)[(x) ? 1 : -1]
#else
#define STATIC_ASSERT_TYPE_DEFINE(cntr)
#define STATIC_ASSERT(x)
#endif
#endif

#endif /* _SYS_UTIL_H_ */
