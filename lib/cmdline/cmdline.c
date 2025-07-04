/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation.
 * Copyright (c) 2009, Olivier MATZ <zer0@droids-corp.org>
 * All rights reserved.
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>

#include <eal_export.h>
#include <rte_string_fns.h>

#include "cmdline_private.h"

static void
cmdline_valid_buffer(struct rdline *rdl, const char *buf,
		     __rte_unused unsigned int size)
{
	struct cmdline *cl = rdl->opaque;
	int ret;
	ret = cmdline_parse(cl, buf);
	if (ret == CMDLINE_PARSE_AMBIGUOUS)
		cmdline_printf(cl, "Ambiguous command\n");
	else if (ret == CMDLINE_PARSE_NOMATCH)
		cmdline_printf(cl, "Command not found\n");
	else if (ret == CMDLINE_PARSE_BAD_ARGS)
		cmdline_printf(cl, "Bad arguments\n");
}

static int
cmdline_complete_buffer(struct rdline *rdl, const char *buf,
			char *dstbuf, unsigned int dstsize,
			int *state)
{
	struct cmdline *cl = rdl->opaque;
	return cmdline_complete(cl, buf, state, dstbuf, dstsize);
}

RTE_EXPORT_SYMBOL(cmdline_write_char)
int
cmdline_write_char(struct rdline *rdl, char c)
{
	int ret = -1;
	struct cmdline *cl;

	if (!rdl)
		return -1;

	cl = rdl->opaque;

	if (cl->s_out >= 0)
		ret = write(cl->s_out, &c, 1);

	return ret;
}


RTE_EXPORT_SYMBOL(cmdline_set_prompt)
void
cmdline_set_prompt(struct cmdline *cl, const char *prompt)
{
	if (!cl || !prompt)
		return;
	strlcpy(cl->prompt, prompt, sizeof(cl->prompt));
}

RTE_EXPORT_SYMBOL(cmdline_new)
struct cmdline *
cmdline_new(cmdline_parse_ctx_t *ctx, const char *prompt, int s_in, int s_out)
{
	struct cmdline *cl;
	int ret;

	if (!ctx || !prompt)
		return NULL;

	cl = malloc(sizeof(struct cmdline));
	if (cl == NULL)
		return NULL;
	memset(cl, 0, sizeof(struct cmdline));
	cl->s_in = s_in;
	cl->s_out = s_out;
	cl->ctx = ctx;

	ret = rdline_init(&cl->rdl, cmdline_write_char, cmdline_valid_buffer,
			cmdline_complete_buffer, cl);
	if (ret != 0) {
		free(cl);
		return NULL;
	}

	cmdline_set_prompt(cl, prompt);
	rdline_newline(&cl->rdl, cl->prompt);

	return cl;
}

RTE_EXPORT_SYMBOL(cmdline_get_rdline)
struct rdline*
cmdline_get_rdline(struct cmdline *cl)
{
	return &cl->rdl;
}

RTE_EXPORT_SYMBOL(cmdline_free)
void
cmdline_free(struct cmdline *cl)
{
	dprintf("called\n");

	if (!cl)
		return;

	if (cl->s_in > 2)
		close(cl->s_in);
	if (cl->s_out != cl->s_in && cl->s_out > 2)
		close(cl->s_out);
	free(cl);
}

RTE_EXPORT_SYMBOL(cmdline_printf)
void
cmdline_printf(const struct cmdline *cl, const char *fmt, ...)
{
	va_list ap;

	if (!cl || !fmt)
		return;

	if (cl->s_out < 0)
		return;
	va_start(ap, fmt);
	cmdline_vdprintf(cl->s_out, fmt, ap);
	va_end(ap);
}

RTE_EXPORT_SYMBOL(cmdline_in)
int
cmdline_in(struct cmdline *cl, const char *buf, int size)
{
	const char *history, *buffer;
	size_t histlen, buflen;
	int ret = 0;
	int i, same;

	if (!cl || !buf)
		return -1;

	for (i=0; i<size; i++) {
		ret = rdline_char_in(&cl->rdl, buf[i]);

		if (ret == RDLINE_RES_VALIDATED) {
			buffer = rdline_get_buffer(&cl->rdl);
			history = rdline_get_history_item(&cl->rdl, 0);
			if (history) {
				histlen = strnlen(history, RDLINE_BUF_SIZE);
				same = !memcmp(buffer, history, histlen) &&
					buffer[histlen] == '\n';
			}
			else
				same = 0;
			buflen = strnlen(buffer, RDLINE_BUF_SIZE);
			if (buflen > 1 && !same)
				rdline_add_history(&cl->rdl, buffer);
			rdline_newline(&cl->rdl, cl->prompt);
		}
		else if (ret == RDLINE_RES_EOF)
			return -1;
		else if (ret == RDLINE_RES_EXITED)
			return -1;
	}
	return i;
}

RTE_EXPORT_SYMBOL(cmdline_quit)
void
cmdline_quit(struct cmdline *cl)
{
	if (!cl)
		return;
	cmdline_cancel(cl);
	rdline_quit(&cl->rdl);
}

RTE_EXPORT_SYMBOL(cmdline_interact)
void
cmdline_interact(struct cmdline *cl)
{
	char c;

	if (!cl)
		return;

	c = -1;
	while (1) {
		if (cmdline_read_char(cl, &c) <= 0)
			break;
		if (cmdline_in(cl, &c, 1) < 0)
			break;
	}
}
