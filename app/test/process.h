/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef _PROCESS_H_
#define _PROCESS_H_

#include <stdint.h>

/*
 * launches a second copy of the test process using the given argv parameters,
 * which should include argv[0] as the process name. To identify in the
 * subprocess the source of the call, the env_value parameter is set in the
 * environment as $RTE_TEST
 */
int process_dup(const char *const argv[], int numargs, const char *env_value);

/*
 * Return a --file-prefix=XXXX argument
 * Note: only Linux supports file prefixes.
 */
const char *file_prefix_arg(void);

#ifdef RTE_EXEC_ENV_LINUX
/* Get current hugepage file prefix */
char *get_current_prefix(char *prefix, int size);
#endif

#endif /* _PROCESS_H_ */
