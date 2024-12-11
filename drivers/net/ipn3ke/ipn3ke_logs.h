/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#ifndef _IPN3KE_LOGS_H_
#define _IPN3KE_LOGS_H_

#include <rte_log.h>

extern int ipn3ke_afu_logtype;
#define RTE_LOGTYPE_IPN3KE_AFU ipn3ke_afu_logtype

#define IPN3KE_AFU_PMD_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, IPN3KE_AFU, "%s(): ", __func__, __VA_ARGS__)

#define IPN3KE_AFU_PMD_FUNC_TRACE() IPN3KE_AFU_PMD_LOG(DEBUG, ">>")

#define IPN3KE_AFU_PMD_DEBUG(fmt, ...) \
	IPN3KE_AFU_PMD_LOG(DEBUG, fmt, ## __VA_ARGS__)

#define IPN3KE_AFU_PMD_INFO(fmt, ...) \
	IPN3KE_AFU_PMD_LOG(INFO, fmt, ## __VA_ARGS__)

#define IPN3KE_AFU_PMD_ERR(fmt, ...) \
	IPN3KE_AFU_PMD_LOG(ERR, fmt, ## __VA_ARGS__)

#define IPN3KE_AFU_PMD_WARN(fmt, ...) \
	IPN3KE_AFU_PMD_LOG(WARNING, fmt, ## __VA_ARGS__)

#endif /* _IPN3KE_LOGS_H_ */
