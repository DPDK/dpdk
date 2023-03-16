/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 Marvell.
 */

#include "ml_test.h"

static STAILQ_HEAD(, ml_test_entry) head = STAILQ_HEAD_INITIALIZER(head);

void
ml_test_register(struct ml_test_entry *entry)
{
	STAILQ_INSERT_TAIL(&head, entry, next);
}

struct ml_test *
ml_test_get(const char *name)
{
	struct ml_test_entry *entry;

	if (!name)
		return NULL;

	STAILQ_FOREACH(entry, &head, next)
	if (!strncmp(entry->test.name, name, strlen(name)))
		return &entry->test;

	return NULL;
}

void
ml_test_dump_names(void (*f)(const char *name))
{
	struct ml_test_entry *entry;

	STAILQ_FOREACH(entry, &head, next)
	{
		if (entry->test.name)
			printf("\t %s\n", entry->test.name);
		f(entry->test.name);
	}
}
