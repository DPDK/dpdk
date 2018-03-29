/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#ifndef _INCLUDE_THREAD_H_
#define _INCLUDE_THREAD_H_

#include <stdint.h>

int
thread_pipeline_enable(uint32_t thread_id,
	const char *pipeline_name);

int
thread_pipeline_disable(uint32_t thread_id,
	const char *pipeline_name);

int
thread_init(void);

int
thread_main(void *arg);

#endif /* _INCLUDE_THREAD_H_ */
