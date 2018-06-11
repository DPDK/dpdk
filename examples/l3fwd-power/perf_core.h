/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#ifndef _PERF_CORE_H_
#define _PERF_CORE_H_

int parse_perf_config(const char *q_arg);
int parse_perf_core_list(const char *corelist);
int update_lcore_params(void);

#endif /* _PERF_CORE_H_ */
