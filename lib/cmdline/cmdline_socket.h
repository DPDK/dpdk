/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation.
 * Copyright (c) 2009, Olivier MATZ <zer0@droids-corp.org>
 * All rights reserved.
 */

#ifndef _CMDLINE_SOCKET_H_
#define _CMDLINE_SOCKET_H_

#include <cmdline_parse.h>

#ifdef __cplusplus
extern "C" {
#endif

struct cmdline *cmdline_file_new(cmdline_parse_ctx_t *ctx, const char *prompt, const char *path);
struct cmdline *cmdline_stdin_new(cmdline_parse_ctx_t *ctx, const char *prompt);
void cmdline_stdin_exit(struct cmdline *cl);

#ifdef __cplusplus
}
#endif

#endif /* _CMDLINE_SOCKET_H_ */
