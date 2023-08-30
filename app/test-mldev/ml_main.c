/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 Marvell.
 */

#include <rte_debug.h>
#include <rte_eal.h>
#include <rte_mldev.h>

#include "ml_common.h"
#include "ml_test.h"

struct ml_options opt;
struct ml_test *test;

int
main(int argc, char **argv)
{
	uint16_t mldevs;
	int ret;

	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_panic("invalid EAL arguments\n");
	argc -= ret;
	argv += ret;

	mldevs = rte_ml_dev_count();
	if (!mldevs) {
		ml_err("no mldev devices found\n");
		goto error;
	}

	/* set default values for options */
	ml_options_default(&opt);

	/* parse the command line arguments */
	ret = ml_options_parse(&opt, argc, argv);
	if (ret) {
		ml_err("parsing one or more user options failed");
		goto error;
	}

	/* get test struct from name */
	test = ml_test_get(opt.test_name);
	if (test == NULL) {
		ml_err("failed to find requested test: %s", opt.test_name);
		goto error;
	}

	if (test->ops.test_result == NULL) {
		ml_err("%s: ops.test_result not found", opt.test_name);
		goto error;
	}

	/* check test options */
	if (test->ops.opt_check) {
		if (test->ops.opt_check(&opt)) {
			ml_err("invalid command line argument");
			goto error;
		}
	}

	/* check the device capability */
	if (test->ops.cap_check) {
		if (test->ops.cap_check(&opt) == false) {
			ml_info("unsupported test: %s", opt.test_name);
			ret = ML_TEST_UNSUPPORTED;
			goto no_cap;
		}
	}

	/* dump options */
	if (opt.debug) {
		if (test->ops.opt_dump)
			test->ops.opt_dump(&opt);
	}

	/* test specific setup */
	if (test->ops.test_setup) {
		if (test->ops.test_setup(test, &opt)) {
			ml_err("failed to setup test: %s", opt.test_name);
			goto error;
		}
	}

	/* test driver */
	if (test->ops.test_driver)
		test->ops.test_driver(test, &opt);

	/* get result */
	if (test->ops.test_result)
		ret = test->ops.test_result(test, &opt);

	if (test->ops.test_destroy)
		test->ops.test_destroy(test, &opt);

no_cap:
	if (ret == ML_TEST_SUCCESS) {
		printf("Result: " CLGRN "%s" CLNRM "\n", "Success");
	} else if (ret == ML_TEST_FAILED) {
		printf("Result: " CLRED "%s" CLNRM "\n", "Failed");
		return EXIT_FAILURE;
	} else if (ret == ML_TEST_UNSUPPORTED) {
		printf("Result: " CLYEL "%s" CLNRM "\n", "Unsupported");
	}

	rte_eal_cleanup();

	return 0;

error:
	rte_eal_cleanup();

	return EXIT_FAILURE;
}
