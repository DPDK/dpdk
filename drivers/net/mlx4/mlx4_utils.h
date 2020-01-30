/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2017 6WIND S.A.
 * Copyright 2017 Mellanox Technologies, Ltd
 */

#ifndef MLX4_UTILS_H_
#define MLX4_UTILS_H_

#include <stddef.h>
#include <stdio.h>

#include <rte_common.h>
#include <rte_log.h>

#include "mlx4.h"

/*
 * Compilation workaround for PPC64 when AltiVec is fully enabled, e.g. std=c11.
 * Otherwise there would be a type conflict between stdbool and altivec.
 */
#if defined(__PPC64__) && !defined(__APPLE_ALTIVEC__)
#undef bool
/* redefine as in stdbool.h */
#define bool _Bool
#endif

extern int mlx4_logtype;

#ifdef RTE_LIBRTE_MLX4_DEBUG

/*
 * When debugging is enabled (MLX4_DEBUG is defined), file, line and function
 * information replace the driver name (MLX4_DRIVER_NAME) in log messages.
 */

/** Return the file name part of a path. */
static inline const char *
pmd_drv_log_basename(const char *s)
{
	const char *n = s;

	while (*n)
		if (*(n++) == '/')
			s = n;
	return s;
}

#define PMD_DRV_LOG(level, ...) \
	rte_log(RTE_LOG_ ## level, mlx4_logtype, \
		RTE_FMT("%s:%u: %s(): " RTE_FMT_HEAD(__VA_ARGS__,) "\n", \
			pmd_drv_log_basename(__FILE__), \
			__LINE__, \
			__func__, \
			RTE_FMT_TAIL(__VA_ARGS__,)))
#define DEBUG(...) PMD_DRV_LOG(DEBUG, __VA_ARGS__)
#define MLX4_ASSERT(exp) RTE_VERIFY(exp)
#define claim_zero(...) MLX4_ASSERT((__VA_ARGS__) == 0)

#else /* RTE_LIBRTE_MLX4_DEBUG */

/*
 * Like MLX4_ASSERT(), DEBUG() becomes a no-op and claim_zero() does not perform
 * any check when debugging is disabled.
 */

#define PMD_DRV_LOG(level, ...) \
	rte_log(RTE_LOG_ ## level, mlx4_logtype, \
		RTE_FMT(MLX4_DRIVER_NAME ": " \
			RTE_FMT_HEAD(__VA_ARGS__,) "\n", \
		RTE_FMT_TAIL(__VA_ARGS__,)))
#define DEBUG(...) (void)0
#define MLX4_ASSERT(exp) RTE_ASSERT(exp)
#define claim_zero(...) (__VA_ARGS__)

#endif /* RTE_LIBRTE_MLX4_DEBUG */

#define INFO(...) PMD_DRV_LOG(INFO, __VA_ARGS__)
#define WARN(...) PMD_DRV_LOG(WARNING, __VA_ARGS__)
#define ERROR(...) PMD_DRV_LOG(ERR, __VA_ARGS__)

/** Allocate a buffer on the stack and fill it with a printf format string. */
#define MKSTR(name, ...) \
	int mkstr_size_##name = snprintf(NULL, 0, "" __VA_ARGS__); \
	char name[mkstr_size_##name + 1]; \
	\
	snprintf(name, sizeof(name), "" __VA_ARGS__)

/** Generate a string out of the provided arguments. */
#define MLX4_STR(...) # __VA_ARGS__

/** Similar to MLX4_STR() with enclosed macros expanded first. */
#define MLX4_STR_EXPAND(...) MLX4_STR(__VA_ARGS__)

/** Object description used with mlx4_mallocv() and similar functions. */
struct mlx4_malloc_vec {
	size_t align; /**< Alignment constraint (power of 2), 0 if unknown. */
	size_t size; /**< Object size. */
	void **addr; /**< Storage for allocation address. */
};

/* mlx4_utils.c */

int mlx4_fd_set_non_blocking(int fd);
size_t mlx4_mallocv(const char *type, const struct mlx4_malloc_vec *vec,
		    unsigned int cnt);
size_t mlx4_zmallocv(const char *type, const struct mlx4_malloc_vec *vec,
		     unsigned int cnt);
size_t mlx4_mallocv_socket(const char *type, const struct mlx4_malloc_vec *vec,
			   unsigned int cnt, int socket);
size_t mlx4_zmallocv_socket(const char *type, const struct mlx4_malloc_vec *vec,
			    unsigned int cnt, int socket);

#endif /* MLX4_UTILS_H_ */
