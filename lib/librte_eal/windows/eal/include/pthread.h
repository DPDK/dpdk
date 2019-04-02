/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#ifndef _PTHREAD_H_
#define _PTHREAD_H_

/**
 * This file is required to support the common code in eal_common_proc.c,
 * eal_common_thread.c and common\include\rte_per_lcore.h as Microsoft libc
 * does not contain pthread.h. This may be removed in future releases.
 */
#ifdef __cplusplus
extern "C" {
#endif

/* defining pthread_t type on Windows since there is no in Microsoft libc*/
typedef uintptr_t pthread_t;

/* defining pthread_attr_t type on Windows since there is no in Microsoft libc*/
typedef void *pthread_attr_t;

#ifdef __cplusplus
}
#endif

#endif /* _PTHREAD_H_ */
