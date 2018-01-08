/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Cavium, Inc
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/queue.h>

#include "evt_test.h"

static STAILQ_HEAD(, evt_test_entry) head = STAILQ_HEAD_INITIALIZER(head);

void
evt_test_register(struct evt_test_entry *entry)
{
	STAILQ_INSERT_TAIL(&head, entry, next);
}

struct evt_test*
evt_test_get(const char *name)
{
	struct evt_test_entry *entry;

	if (!name)
		return NULL;

	STAILQ_FOREACH(entry, &head, next)
		if (!strncmp(entry->test.name, name, strlen(name)))
			return &entry->test;

	return NULL;
}

void
evt_test_dump_names(void)
{
	struct evt_test_entry *entry;

	STAILQ_FOREACH(entry, &head, next)
		if (entry->test.name)
			printf("\t %s\n", entry->test.name);
}
