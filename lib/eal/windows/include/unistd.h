/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#ifndef _UNISTD_H_
#define _UNISTD_H_
/**
 * This file is added to support common code in eal_common_lcore.c
 * as Microsoft libc does not contain unistd.h. This may be removed
 * in future releases.
 */

#include <io.h>

/*
 * Windows appears to be missing STD*_FILENO macros, so define here.
 * For simplicity, assume that if STDIN_FILENO is missing, all are,
 * rather than checking each individually.
 */
#ifndef STDIN_FILENO
#define STDIN_FILENO _fileno(stdin)
#define STDOUT_FILENO _fileno(stdout)
#define STDERR_FILENO _fileno(stderr)
#endif

#endif /* _UNISTD_H_ */
