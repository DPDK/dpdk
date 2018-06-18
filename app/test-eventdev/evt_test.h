/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Cavium, Inc
 */

#ifndef _EVT_TEST_
#define _EVT_TEST_

#include <string.h>
#include <stdbool.h>
#include <sys/queue.h>

#include <rte_eal.h>

enum evt_test_result {
	EVT_TEST_SUCCESS,
	EVT_TEST_FAILED,
	EVT_TEST_UNSUPPORTED,
};

struct evt_test;
struct evt_options;

typedef bool (*evt_test_capability_check_t)(struct evt_options *opt);
typedef int (*evt_test_options_check_t)(struct evt_options *opt);
typedef void (*evt_test_options_dump_t)(struct evt_options *opt);
typedef int (*evt_test_setup_t)
		(struct evt_test *test, struct evt_options *opt);
typedef int (*evt_test_mempool_setup_t)
		(struct evt_test *test, struct evt_options *opt);
typedef int (*evt_test_ethdev_setup_t)
		(struct evt_test *test, struct evt_options *opt);
typedef int (*evt_test_eventdev_setup_t)
		(struct evt_test *test, struct evt_options *opt);
typedef int (*evt_test_launch_lcores_t)
		(struct evt_test *test, struct evt_options *opt);
typedef int (*evt_test_result_t)
		(struct evt_test *test, struct evt_options *opt);
typedef void (*evt_test_eventdev_destroy_t)
		(struct evt_test *test, struct evt_options *opt);
typedef void (*evt_test_ethdev_destroy_t)
		(struct evt_test *test, struct evt_options *opt);
typedef void (*evt_test_mempool_destroy_t)
		(struct evt_test *test, struct evt_options *opt);
typedef void (*evt_test_destroy_t)
		(struct evt_test *test, struct evt_options *opt);

struct evt_test_ops {
	evt_test_capability_check_t cap_check;
	evt_test_options_check_t opt_check;
	evt_test_options_dump_t opt_dump;
	evt_test_setup_t test_setup;
	evt_test_mempool_setup_t mempool_setup;
	evt_test_ethdev_setup_t ethdev_setup;
	evt_test_eventdev_setup_t eventdev_setup;
	evt_test_launch_lcores_t launch_lcores;
	evt_test_result_t test_result;
	evt_test_eventdev_destroy_t eventdev_destroy;
	evt_test_ethdev_destroy_t ethdev_destroy;
	evt_test_mempool_destroy_t mempool_destroy;
	evt_test_destroy_t test_destroy;
};

struct evt_test {
	const char *name;
	void *test_priv;
	struct evt_test_ops ops;
};

struct evt_test_entry {
	struct evt_test test;

	STAILQ_ENTRY(evt_test_entry) next;
};

void evt_test_register(struct evt_test_entry *test);
void evt_test_dump_names(void);

#define EVT_TEST_REGISTER(nm)                         \
static struct evt_test_entry _evt_test_entry_ ##nm;   \
RTE_INIT(evt_test_ ##nm)                              \
{                                                     \
	_evt_test_entry_ ##nm.test.name = RTE_STR(nm);\
	memcpy(&_evt_test_entry_ ##nm.test.ops, &nm,  \
			sizeof(struct evt_test_ops)); \
	evt_test_register(&_evt_test_entry_ ##nm);    \
}

struct evt_test *evt_test_get(const char *name);

static inline void *
evt_test_priv(struct evt_test *test)
{
	return test->test_priv;
}

#endif /*  _EVT_TEST_ */
