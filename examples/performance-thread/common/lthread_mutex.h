/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015 Intel Corporation
 */


#ifndef LTHREAD_MUTEX_H_
#define LTHREAD_MUTEX_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "lthread_queue.h"


#define MAX_MUTEX_NAME_SIZE 64

struct lthread_mutex {
	struct lthread *owner;
	rte_atomic64_t	count;
	struct lthread_queue *blocked __rte_cache_aligned;
	struct lthread_sched *root_sched;
	char			name[MAX_MUTEX_NAME_SIZE];
	uint64_t		diag_ref; /* optional ref to user diag data */
} __rte_cache_aligned;

#ifdef __cplusplus
}
#endif

#endif /* LTHREAD_MUTEX_H_ */
