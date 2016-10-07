/*-
 *   BSD LICENSE
 *
 *   Copyright 2012-2015 6WIND S.A.
 *   Copyright 2012 Mellanox.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of 6WIND S.A. nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef RTE_PMD_MLX4_H_
#define RTE_PMD_MLX4_H_

#include <stddef.h>
#include <stdint.h>
#include <limits.h>

/*
 * Maximum number of simultaneous MAC addresses supported.
 *
 * According to ConnectX's Programmer Reference Manual:
 *   The L2 Address Match is implemented by comparing a MAC/VLAN combination
 *   of 128 MAC addresses and 127 VLAN values, comprising 128x127 possible
 *   L2 addresses.
 */
#define MLX4_MAX_MAC_ADDRESSES 128

/* Maximum number of simultaneous VLAN filters supported. See above. */
#define MLX4_MAX_VLAN_IDS 127

/* Request send completion once in every 64 sends, might be less. */
#define MLX4_PMD_TX_PER_COMP_REQ 64

/* Maximum number of Scatter/Gather Elements per Work Request. */
#ifndef MLX4_PMD_SGE_WR_N
#define MLX4_PMD_SGE_WR_N 4
#endif

/* Maximum size for inline data. */
#ifndef MLX4_PMD_MAX_INLINE
#define MLX4_PMD_MAX_INLINE 0
#endif

/*
 * Maximum number of cached Memory Pools (MPs) per TX queue. Each RTE MP
 * from which buffers are to be transmitted will have to be mapped by this
 * driver to their own Memory Region (MR). This is a slow operation.
 *
 * This value is always 1 for RX queues.
 */
#ifndef MLX4_PMD_TX_MP_CACHE
#define MLX4_PMD_TX_MP_CACHE 8
#endif

/*
 * If defined, only use software counters. The PMD will never ask the hardware
 * for these, and many of them won't be available.
 */
#ifndef MLX4_PMD_SOFT_COUNTERS
#define MLX4_PMD_SOFT_COUNTERS 1
#endif

/* Alarm timeout. */
#define MLX4_ALARM_TIMEOUT_US 100000

enum {
	PCI_VENDOR_ID_MELLANOX = 0x15b3,
};

enum {
	PCI_DEVICE_ID_MELLANOX_CONNECTX3 = 0x1003,
	PCI_DEVICE_ID_MELLANOX_CONNECTX3VF = 0x1004,
	PCI_DEVICE_ID_MELLANOX_CONNECTX3PRO = 0x1007,
};

#define MLX4_DRIVER_NAME "net_mlx4"

/* Bit-field manipulation. */
#define BITFIELD_DECLARE(bf, type, size)				\
	type bf[(((size_t)(size) / (sizeof(type) * CHAR_BIT)) +		\
		 !!((size_t)(size) % (sizeof(type) * CHAR_BIT)))]
#define BITFIELD_DEFINE(bf, type, size)					\
	BITFIELD_DECLARE((bf), type, (size)) = { 0 }
#define BITFIELD_SET(bf, b)						\
	(assert((size_t)(b) < (sizeof(bf) * CHAR_BIT)),			\
	 (void)((bf)[((b) / (sizeof((bf)[0]) * CHAR_BIT))] |=		\
		((size_t)1 << ((b) % (sizeof((bf)[0]) * CHAR_BIT)))))
#define BITFIELD_RESET(bf, b)						\
	(assert((size_t)(b) < (sizeof(bf) * CHAR_BIT)),			\
	 (void)((bf)[((b) / (sizeof((bf)[0]) * CHAR_BIT))] &=		\
		~((size_t)1 << ((b) % (sizeof((bf)[0]) * CHAR_BIT)))))
#define BITFIELD_ISSET(bf, b)						\
	(assert((size_t)(b) < (sizeof(bf) * CHAR_BIT)),			\
	 !!(((bf)[((b) / (sizeof((bf)[0]) * CHAR_BIT))] &		\
	     ((size_t)1 << ((b) % (sizeof((bf)[0]) * CHAR_BIT))))))

/* Number of elements in array. */
#define elemof(a) (sizeof(a) / sizeof((a)[0]))

/* Cast pointer p to structure member m to its parent structure of type t. */
#define containerof(p, t, m) ((t *)((uint8_t *)(p) - offsetof(t, m)))

/* Branch prediction helpers. */
#ifndef likely
#define likely(c) __builtin_expect(!!(c), 1)
#endif
#ifndef unlikely
#define unlikely(c) __builtin_expect(!!(c), 0)
#endif

/* Debugging */
#ifndef NDEBUG
#include <stdio.h>
#define DEBUG__(m, ...)						\
	(fprintf(stderr, "%s:%d: %s(): " m "%c",		\
		 __FILE__, __LINE__, __func__, __VA_ARGS__),	\
	 fflush(stderr),					\
	 (void)0)
/*
 * Save/restore errno around DEBUG__().
 * XXX somewhat undefined behavior, but works.
 */
#define DEBUG_(...)				\
	(errno = ((int []){			\
		*(volatile int *)&errno,	\
		(DEBUG__(__VA_ARGS__), 0)	\
	})[0])
#define DEBUG(...) DEBUG_(__VA_ARGS__, '\n')
#define claim_zero(...) assert((__VA_ARGS__) == 0)
#define claim_nonzero(...) assert((__VA_ARGS__) != 0)
#define claim_positive(...) assert((__VA_ARGS__) >= 0)
#else /* NDEBUG */
/* No-ops. */
#define DEBUG(...) (void)0
#define claim_zero(...) (__VA_ARGS__)
#define claim_nonzero(...) (__VA_ARGS__)
#define claim_positive(...) (__VA_ARGS__)
#endif /* NDEBUG */

#endif /* RTE_PMD_MLX4_H_ */
