/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2015 6WIND S.A.
 * Copyright 2015 Mellanox Technologies, Ltd
 */

#ifndef RTE_PMD_MLX5_UTILS_H_
#define RTE_PMD_MLX5_UTILS_H_

#include <stddef.h>
#include <stdint.h>
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

/* Convert a bit number to the corresponding 64-bit mask */
#define MLX5_BITSHIFT(v) (UINT64_C(1) << (v))

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

extern int mlx5_logtype;

#define PMD_DRV_LOG___(level, ...) \
	rte_log(RTE_LOG_ ## level, \
		mlx5_logtype, \
		RTE_FMT(MLX5_DRIVER_NAME ": " \
			RTE_FMT_HEAD(__VA_ARGS__,), \
		RTE_FMT_TAIL(__VA_ARGS__,)))

/*
 * When debugging is enabled (NDEBUG not defined), file, line and function
 * information replace the driver name (MLX5_DRIVER_NAME) in log messages.
 */
#ifndef NDEBUG

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
#define PMD_DRV_LOG__(level, ...) \
	PMD_DRV_LOG___(level, __VA_ARGS__)
#define PMD_DRV_LOG_(level, s, ...) \
	PMD_DRV_LOG__(level, s "\n", __VA_ARGS__)

#endif /* NDEBUG */

/* Generic printf()-like logging macro with automatic line feed. */
#define DRV_LOG(level, ...) \
	PMD_DRV_LOG_(level, \
		__VA_ARGS__ PMD_DRV_LOG_STRIP PMD_DRV_LOG_OPAREN, \
		PMD_DRV_LOG_CPAREN)

/* claim_zero() does not perform any check when debugging is disabled. */
#ifndef NDEBUG

#define DEBUG(...) DRV_LOG(DEBUG, __VA_ARGS__)
#define claim_zero(...) assert((__VA_ARGS__) == 0)
#define claim_nonzero(...) assert((__VA_ARGS__) != 0)

#else /* NDEBUG */

#define DEBUG(...) (void)0
#define claim_zero(...) (__VA_ARGS__)
#define claim_nonzero(...) (__VA_ARGS__)

#endif /* NDEBUG */

#define INFO(...) DRV_LOG(INFO, __VA_ARGS__)
#define WARN(...) DRV_LOG(WARNING, __VA_ARGS__)
#define ERROR(...) DRV_LOG(ERR, __VA_ARGS__)

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
