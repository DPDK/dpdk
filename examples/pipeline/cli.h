/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

#ifndef __INCLUDE_CLI_H__
#define __INCLUDE_CLI_H__

#include <stddef.h>

void
cli_process(char *in, char *out, size_t out_size, void *arg);

int
cli_script_process(const char *file_name,
	size_t msg_in_len_max,
	size_t msg_out_len_max,
	void *arg);

#endif
