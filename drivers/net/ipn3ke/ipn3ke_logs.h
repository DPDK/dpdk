/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#ifndef _IPN3KE_LOGS_H_
#define _IPN3KE_LOGS_H_

#include <rte_log.h>

extern int ipn3ke_afu_logtype;

#define IPN3KE_AFU_PMD_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, ipn3ke_afu_logtype, "%s(): " fmt "\n", \
		__func__, ##args)

#define IPN3KE_AFU_PMD_FUNC_TRACE() IPN3KE_AFU_PMD_LOG(DEBUG, ">>")

#define IPN3KE_AFU_PMD_DEBUG(fmt, args...) \
	IPN3KE_AFU_PMD_LOG(DEBUG, fmt, ## args)

#define IPN3KE_AFU_PMD_INFO(fmt, args...) \
	IPN3KE_AFU_PMD_LOG(INFO, fmt, ## args)

#define IPN3KE_AFU_PMD_ERR(fmt, args...) \
	IPN3KE_AFU_PMD_LOG(ERR, fmt, ## args)

#define IPN3KE_AFU_PMD_WARN(fmt, args...) \
	IPN3KE_AFU_PMD_LOG(WARNING, fmt, ## args)

#endif /* _IPN3KE_LOGS_H_ */
