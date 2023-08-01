/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation.
 * Copyright (c) 2009, Olivier MATZ <zer0@droids-corp.org>
 * All rights reserved.
 */

#ifndef _CMDLINE_H_
#define _CMDLINE_H_

#include <rte_common.h>
#include <rte_compat.h>

#include <cmdline_rdline.h>
#include <cmdline_parse.h>

/**
 * @file
 *
 * Command line API
 */

#ifdef __cplusplus
extern "C" {
#endif

enum rdline_status {
	RDLINE_INIT,
	RDLINE_RUNNING,
	RDLINE_EXITED
};

struct cmdline;

struct cmdline *cmdline_new(cmdline_parse_ctx_t *ctx, const char *prompt, int s_in, int s_out);
void cmdline_set_prompt(struct cmdline *cl, const char *prompt);
void cmdline_free(struct cmdline *cl);
void cmdline_printf(const struct cmdline *cl, const char *fmt, ...)
	__rte_format_printf(2, 3);
int cmdline_in(struct cmdline *cl, const char *buf, int size);
int cmdline_write_char(struct rdline *rdl, char c);

__rte_experimental
struct rdline *
cmdline_get_rdline(struct cmdline *cl);


void cmdline_interact(struct cmdline *cl);
void cmdline_quit(struct cmdline *cl);

#ifdef __cplusplus
}
#endif

#endif /* _CMDLINE_SOCKET_H_ */
