/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2026 Intel Corporation
 */

#ifndef _COMMON_INTEL_LOG_H_
#define _COMMON_INTEL_LOG_H_

#include <rte_log.h>

/*
 * Common logging for shared Intel driver code.
 *
 * This header must only be included from driver translation units, where the
 * build system has already defined RTE_COMPONENT_NAME (e.g. "iavf"). It uses
 * that token to reference the per-driver logtype variable that every Intel
 * driver registers (e.g. iavf_logtype_driver), so no additional setup is
 * needed beyond what each driver already provides.
 *
 * Usage: CI_DRV_LOG(DEBUG, "format %s", arg);
 */

#ifndef RTE_COMPONENT_NAME
/* CI_DRV_LOG is a no-op when included outside a driver build context. */
#define CI_DRV_LOG(level, fmt, ...) do { } while (0)
#else /* RTE_COMPONENT_NAME */

/* Resolves to the per-driver logtype variable, e.g. iavf_logtype_driver. */
#define CI_DRV_LOGTYPE RTE_CONCAT(RTE_COMPONENT_NAME, _logtype_driver)

/* Forward-declare the logtype so shared headers need not include driver headers. */
extern int CI_DRV_LOGTYPE;

#define CI_DRV_LOG(level, fmt, ...) \
	rte_log(RTE_LOG_##level, CI_DRV_LOGTYPE, \
		"PMD_INTEL_COMMON: %s(): " fmt "\n", \
		__func__, ##__VA_ARGS__)

#endif /* RTE_COMPONENT_NAME */

#endif /* _COMMON_INTEL_LOG_H_ */
