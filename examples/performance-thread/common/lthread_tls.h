/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015 Intel Corporation
 */

#ifndef LTHREAD_TLS_H_
#define LTHREAD_TLS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "lthread_api.h"

#define RTE_PER_LTHREAD_SECTION_SIZE \
(&__stop_per_lt - &__start_per_lt)

struct lthread_key {
	tls_destructor_func destructor;
};

struct lthread_tls {
	void *data[LTHREAD_MAX_KEYS];
	int  nb_keys_inuse;
	struct lthread_sched *root_sched;
};

void _lthread_tls_destroy(struct lthread *lt);
void _lthread_key_pool_init(void);
void _lthread_tls_alloc(struct lthread *lt);

#ifdef __cplusplus
}
#endif

#endif				/* LTHREAD_TLS_H_ */
