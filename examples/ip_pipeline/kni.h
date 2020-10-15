/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#ifndef _INCLUDE_KNI_H_
#define _INCLUDE_KNI_H_

#include <stdint.h>
#include <sys/queue.h>

#ifdef RTE_LIB_KNI
#include <rte_kni.h>
#endif

#include "common.h"

struct kni {
	TAILQ_ENTRY(kni) node;
	char name[NAME_SIZE];
#ifdef RTE_LIB_KNI
	struct rte_kni *k;
#endif
};

TAILQ_HEAD(kni_list, kni);

int
kni_init(void);

struct kni *
kni_find(const char *name);

struct kni_params {
	const char *link_name;
	const char *mempool_name;
	int force_bind;
	uint32_t thread_id;
};

struct kni *
kni_create(const char *name, struct kni_params *params);

void
kni_handle_request(void);

#endif /* _INCLUDE_KNI_H_ */
