/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Huawei Technologies Co., Ltd
 */

#include "internal.h"


#define APP_EAL_INIT_ARG_SIZE_MAX sizeof("--log-level=lib.eal:warning")

static const char APP_EAL_INIT_ARGS[][APP_EAL_INIT_ARG_SIZE_MAX] = {
	"--log-level=lib.eal:warning",
	"--no-huge",
	"--no-pci",
	"--no-hpet",
	"--no-shconf",
};

#define APP_EAL_INIT_ARGC (/* program name */ 1 + RTE_DIM(APP_EAL_INIT_ARGS))

int
get_eal_init_argc(void)
{
	return APP_EAL_INIT_ARGC;
}

/*
 * We cannot just return literal strings here, because rte_eal_init accepts
 * an array of mutable pointers to mutable strings, so literals won't work.
 * Instead we build a copy of APP_EAL_INIT_ARGS in a static mutable area.
 */
char**
get_eal_init_argv(char *prog_name)
{
	/* Static arrays for mutable copies of actual args */
	static char mutable_args[RTE_DIM(APP_EAL_INIT_ARGS)]
		[APP_EAL_INIT_ARG_SIZE_MAX];
	/*
	 * Static array for pointers to args. First element will hold pointer
	 * to the prog_name, last will be set to NULL, the rest will point to
	 * elements of mutable_args with index one smaller.
	 */
	static char *mutable_ptrs[APP_EAL_INIT_ARGC + /* terminating NULL */ 1];

	mutable_ptrs[0] = prog_name;
	for (int argi = 0; argi != RTE_DIM(APP_EAL_INIT_ARGS); ++argi) {
		const char * const const_arg = APP_EAL_INIT_ARGS[argi];
		char * const mutable_arg = mutable_args[argi];
		const int snprintf_rc = snprintf(mutable_arg,
			APP_EAL_INIT_ARG_SIZE_MAX, "%s", const_arg);
		RTE_VERIFY(snprintf_rc >= 0 &&
			(size_t)snprintf_rc < APP_EAL_INIT_ARG_SIZE_MAX);
		mutable_ptrs[1 + argi] = mutable_arg;
	}
	mutable_ptrs[RTE_DIM(mutable_ptrs) - 1] = NULL;

	return mutable_ptrs;
}
