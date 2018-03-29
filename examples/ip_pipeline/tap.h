/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#ifndef _INCLUDE_TAP_H_
#define _INCLUDE_TAP_H_

#include <sys/queue.h>

#include "common.h"

struct tap {
	TAILQ_ENTRY(tap) node;
	char name[NAME_SIZE];
	int fd;
};

TAILQ_HEAD(tap_list, tap);

int
tap_init(void);

struct tap *
tap_find(const char *name);

struct tap *
tap_create(const char *name);

#endif /* _INCLUDE_TAP_H_ */
