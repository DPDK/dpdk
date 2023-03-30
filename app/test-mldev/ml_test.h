/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 Marvell.
 */

#ifndef ML_TEST_H
#define ML_TEST_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>

#include <rte_common.h>

#include "ml_options.h"

#define ML_TEST_MAX_POOL_SIZE 256

enum ml_test_result {
	ML_TEST_SUCCESS,
	ML_TEST_FAILED,
	ML_TEST_UNSUPPORTED,
};

struct ml_test;

typedef bool (*ml_test_capability_check_t)(struct ml_options *opt);
typedef int (*ml_test_options_check_t)(struct ml_options *opt);
typedef void (*ml_test_options_dump_t)(struct ml_options *opt);
typedef int (*ml_test_setup_t)(struct ml_test *test, struct ml_options *opt);
typedef void (*ml_test_destroy_t)(struct ml_test *test, struct ml_options *opt);
typedef int (*ml_test_driver_t)(struct ml_test *test, struct ml_options *opt);
typedef int (*ml_test_result_t)(struct ml_test *test, struct ml_options *opt);

struct ml_test_ops {
	ml_test_capability_check_t cap_check;
	ml_test_options_check_t opt_check;
	ml_test_options_dump_t opt_dump;
	ml_test_setup_t test_setup;
	ml_test_destroy_t test_destroy;
	ml_test_driver_t test_driver;
	ml_test_result_t test_result;
};

struct ml_test {
	const char *name;
	void *test_priv;
	struct ml_test_ops ops;
};

struct ml_test_entry {
	struct ml_test test;

	STAILQ_ENTRY(ml_test_entry) next;
};

static inline void *
ml_test_priv(struct ml_test *test)
{
	return test->test_priv;
}

struct ml_test *ml_test_get(const char *name);
void ml_test_register(struct ml_test_entry *test);
void ml_test_dump_names(void (*f)(const char *));

#define ML_TEST_REGISTER(nm) \
	static struct ml_test_entry _ml_test_entry_##nm; \
	RTE_INIT(ml_test_##nm) \
	{ \
		_ml_test_entry_##nm.test.name = RTE_STR(nm); \
		memcpy(&_ml_test_entry_##nm.test.ops, &nm, sizeof(struct ml_test_ops)); \
		ml_test_register(&_ml_test_entry_##nm); \
	}

#endif /* ML_TEST_H */
