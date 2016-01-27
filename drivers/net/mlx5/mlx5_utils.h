/*-
 *   BSD LICENSE
 *
 *   Copyright 2015 6WIND S.A.
 *   Copyright 2015 Mellanox.
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

#ifndef RTE_PMD_MLX5_UTILS_H_
#define RTE_PMD_MLX5_UTILS_H_

#include <stddef.h>
#include <stdio.h>
#include <limits.h>
#include <assert.h>
#include <errno.h>

#include "mlx5_defs.h"

/* Bit-field manipulation. */
#define BITFIELD_DECLARE(bf, type, size) \
	type bf[(((size_t)(size) / (sizeof(type) * CHAR_BIT)) + \
		 !!((size_t)(size) % (sizeof(type) * CHAR_BIT)))]
#define BITFIELD_DEFINE(bf, type, size) \
	BITFIELD_DECLARE((bf), type, (size)) = { 0 }
#define BITFIELD_SET(bf, b) \
	(assert((size_t)(b) < (sizeof(bf) * CHAR_BIT)), \
	 (void)((bf)[((b) / (sizeof((bf)[0]) * CHAR_BIT))] |= \
		((size_t)1 << ((b) % (sizeof((bf)[0]) * CHAR_BIT)))))
#define BITFIELD_RESET(bf, b) \
	(assert((size_t)(b) < (sizeof(bf) * CHAR_BIT)), \
	 (void)((bf)[((b) / (sizeof((bf)[0]) * CHAR_BIT))] &= \
		~((size_t)1 << ((b) % (sizeof((bf)[0]) * CHAR_BIT)))))
#define BITFIELD_ISSET(bf, b) \
	(assert((size_t)(b) < (sizeof(bf) * CHAR_BIT)), \
	 !!(((bf)[((b) / (sizeof((bf)[0]) * CHAR_BIT))] & \
	     ((size_t)1 << ((b) % (sizeof((bf)[0]) * CHAR_BIT))))))

/* Save and restore errno around argument evaluation. */
#define ERRNO_SAFE(x) ((errno = (int []){ errno, ((x), 0) }[0]))

/*
 * Helper macros to work around __VA_ARGS__ limitations in a C99 compliant
 * manner.
 */
#define PMD_DRV_LOG_STRIP(a, b) a
#define PMD_DRV_LOG_OPAREN (
#define PMD_DRV_LOG_CPAREN )
#define PMD_DRV_LOG_COMMA ,

/* Return the file name part of a path. */
static inline const char *
pmd_drv_log_basename(const char *s)
{
	const char *n = s;

	while (*n)
		if (*(n++) == '/')
			s = n;
	return s;
}

/*
 * When debugging is enabled (NDEBUG not defined), file, line and function
 * information replace the driver name (MLX5_DRIVER_NAME) in log messages.
 */
#ifndef NDEBUG

#define PMD_DRV_LOG___(level, ...) \
	ERRNO_SAFE(RTE_LOG(level, PMD, __VA_ARGS__))
#define PMD_DRV_LOG__(level, ...) \
	PMD_DRV_LOG___(level, "%s:%u: %s(): " __VA_ARGS__)
#define PMD_DRV_LOG_(level, s, ...) \
	PMD_DRV_LOG__(level, \
		s "\n" PMD_DRV_LOG_COMMA \
		pmd_drv_log_basename(__FILE__) PMD_DRV_LOG_COMMA \
		__LINE__ PMD_DRV_LOG_COMMA \
		__func__, \
		__VA_ARGS__)

#else /* NDEBUG */

#define PMD_DRV_LOG___(level, ...) \
	ERRNO_SAFE(RTE_LOG(level, PMD, MLX5_DRIVER_NAME ": " __VA_ARGS__))
#define PMD_DRV_LOG__(level, ...) \
	PMD_DRV_LOG___(level, __VA_ARGS__)
#define PMD_DRV_LOG_(level, s, ...) \
	PMD_DRV_LOG__(level, s "\n", __VA_ARGS__)

#endif /* NDEBUG */

/* Generic printf()-like logging macro with automatic line feed. */
#define PMD_DRV_LOG(level, ...) \
	PMD_DRV_LOG_(level, \
		__VA_ARGS__ PMD_DRV_LOG_STRIP PMD_DRV_LOG_OPAREN, \
		PMD_DRV_LOG_CPAREN)

/*
 * Like assert(), DEBUG() becomes a no-op and claim_zero() does not perform
 * any check when debugging is disabled.
 */
#ifndef NDEBUG

#define DEBUG(...) PMD_DRV_LOG(DEBUG, __VA_ARGS__)
#define claim_zero(...) assert((__VA_ARGS__) == 0)

#else /* NDEBUG */

#define DEBUG(...) (void)0
#define claim_zero(...) (__VA_ARGS__)

#endif /* NDEBUG */

#define INFO(...) PMD_DRV_LOG(INFO, __VA_ARGS__)
#define WARN(...) PMD_DRV_LOG(WARNING, __VA_ARGS__)
#define ERROR(...) PMD_DRV_LOG(ERR, __VA_ARGS__)

/* Convenience macros for accessing mbuf fields. */
#define NEXT(m) ((m)->next)
#define DATA_LEN(m) ((m)->data_len)
#define PKT_LEN(m) ((m)->pkt_len)
#define DATA_OFF(m) ((m)->data_off)
#define SET_DATA_OFF(m, o) ((m)->data_off = (o))
#define NB_SEGS(m) ((m)->nb_segs)
#define PORT(m) ((m)->port)

/* Transpose flags. Useful to convert IBV to DPDK flags. */
#define TRANSPOSE(val, from, to) \
	(((from) >= (to)) ? \
	 (((val) & (from)) / ((from) / (to))) : \
	 (((val) & (from)) * ((to) / (from))))

/* Allocate a buffer on the stack and fill it with a printf format string. */
#define MKSTR(name, ...) \
	char name[snprintf(NULL, 0, __VA_ARGS__) + 1]; \
	\
	snprintf(name, sizeof(name), __VA_ARGS__)

/**
 * Return nearest power of two above input value.
 *
 * @param v
 *   Input value.
 *
 * @return
 *   Nearest power of two above input value.
 */
static inline unsigned int
log2above(unsigned int v)
{
	unsigned int l;
	unsigned int r;

	for (l = 0, r = 0; (v >> 1); ++l, v >>= 1)
		r |= (v & 1);
	return l + r;
}

#endif /* RTE_PMD_MLX5_UTILS_H_ */
