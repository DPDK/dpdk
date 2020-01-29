/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2019 Mellanox Technologies, Ltd
 */

#ifndef RTE_PMD_MLX5_COMMON_H_
#define RTE_PMD_MLX5_COMMON_H_

#include <assert.h>
#include <stdio.h>

#include <rte_pci.h>
#include <rte_log.h>


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

#define PMD_DRV_LOG___(level, type, name, ...) \
	rte_log(RTE_LOG_ ## level, \
		type, \
		RTE_FMT(name ": " \
			RTE_FMT_HEAD(__VA_ARGS__,), \
		RTE_FMT_TAIL(__VA_ARGS__,)))

/*
 * When debugging is enabled (NDEBUG not defined), file, line and function
 * information replace the driver name (MLX5_DRIVER_NAME) in log messages.
 */
#ifndef NDEBUG

#define PMD_DRV_LOG__(level, type, name, ...) \
	PMD_DRV_LOG___(level, type, name, "%s:%u: %s(): " __VA_ARGS__)
#define PMD_DRV_LOG_(level, type, name, s, ...) \
	PMD_DRV_LOG__(level, type, name,\
		s "\n" PMD_DRV_LOG_COMMA \
		pmd_drv_log_basename(__FILE__) PMD_DRV_LOG_COMMA \
		__LINE__ PMD_DRV_LOG_COMMA \
		__func__, \
		__VA_ARGS__)

#else /* NDEBUG */
#define PMD_DRV_LOG__(level, type, name, ...) \
	PMD_DRV_LOG___(level, type, name, __VA_ARGS__)
#define PMD_DRV_LOG_(level, type, name, s, ...) \
	PMD_DRV_LOG__(level, type, name, s "\n", __VA_ARGS__)

#endif /* NDEBUG */

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

/* Allocate a buffer on the stack and fill it with a printf format string. */
#define MKSTR(name, ...) \
	int mkstr_size_##name = snprintf(NULL, 0, "" __VA_ARGS__); \
	char name[mkstr_size_##name + 1]; \
	\
	snprintf(name, sizeof(name), "" __VA_ARGS__)

int mlx5_dev_to_pci_addr(const char *dev_path, struct rte_pci_addr *pci_addr);

#endif /* RTE_PMD_MLX5_COMMON_H_ */
