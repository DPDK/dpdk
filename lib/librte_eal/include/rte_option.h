/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation.
 */

#ifndef __INCLUDE_RTE_OPTION_H__
#define __INCLUDE_RTE_OPTION_H__

/**
 * @file
 *
 * This API offers the ability to register options to the EAL command line and
 * map those options to functions that will be executed at the end of EAL
 * initialization. These options will be available as part of the EAL command
 * line of applications and are dynamically managed.
 *
 * This is used primarily by DPDK libraries offering command line options.
 * Currently, this API is limited to registering options without argument.
 *
 * The register API can be used to resolve circular dependency issues
 * between EAL and the library. The library uses EAL, but is also initialized
 * by EAL. Hence, EAL depends on the init function of the library. The API
 * introduced in rte_option allows us to register the library init with EAL
 * (passing a function pointer) and avoid the circular dependency.
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*rte_option_cb)(void);

/**
 * Structure describing an EAL command line option dynamically registered.
 *
 * Common EAL options are mostly statically defined.
 * Some libraries need additional options to be dynamically added.
 * This structure describes such options.
 */
struct rte_option {
	TAILQ_ENTRY(rte_option) next; /**< Next entry in the list. */
	const char *name; /**< The option name. */
	const char *usage; /**< Option summary string. */
	rte_option_cb cb;          /**< Function called when option is used. */
	int enabled;               /**< Set when the option is used. */
};

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Register an option to the EAL command line.
 * When recognized, the associated function will be executed at the end of EAL
 * initialization.
 *
 * The associated structure must be available the whole time this option is
 * registered (i.e. not stack memory).
 *
 * @param opt
 *  Structure describing the option to parse.
 *
 * @return
 *  0 on success, <0 otherwise.
 */
__rte_experimental
int
rte_option_register(struct rte_option *opt);

#ifdef __cplusplus
}
#endif

#endif
