/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2018 Intel Corporation
 */

#ifndef _QAT_LOGS_H_
#define _QAT_LOGS_H_

extern int qat_gen_logtype;

#define PMD_DRV_LOG(level, fmt, args...)			\
	rte_log(RTE_LOG_ ## level, qat_gen_logtype,		\
			"%s(): " fmt "\n", __func__, ## args)

#endif /* _QAT_LOGS_H_ */
