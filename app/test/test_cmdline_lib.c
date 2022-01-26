/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <sys/queue.h>

#include <rte_common.h>

#include <cmdline_vt100.h>
#include <cmdline_rdline.h>
#include <cmdline_parse.h>
#include <cmdline_socket.h>
#include <cmdline.h>

#include "test_cmdline.h"

#ifndef RTE_EXEC_ENV_WINDOWS
#define NULL_INPUT "/dev/null"
#else
#define NULL_INPUT "NUL"
#endif

/****************************************************************/
/* static functions required for some tests */
static void
valid_buffer(__rte_unused struct rdline *rdl,
			__rte_unused const char *buf,
			__rte_unused unsigned int size)
{
}

static int
complete_buffer(__rte_unused struct rdline *rdl,
			__rte_unused const char *buf,
			__rte_unused char *dstbuf,
			__rte_unused unsigned int dstsize,
			__rte_unused int *state)
{
	return 0;
}

/****************************************************************/

static int
test_cmdline_parse_fns(void)
{
	struct cmdline *cl;
	cmdline_parse_ctx_t ctx;
	int i = 0;
	char dst[CMDLINE_TEST_BUFSIZE];

	cl = cmdline_new(&ctx, "prompt", -1, -1);
	if (cl == NULL) {
		printf("Error: cannot create cmdline to test parse fns!\n");
		return -1;
	}

	if (cmdline_parse(NULL, "buffer") >= 0)
		goto error;
	if (cmdline_parse(cl, NULL) >= 0)
		goto error;

	if (cmdline_complete(NULL, "buffer", &i, dst, sizeof(dst)) >= 0)
		goto error;
	if (cmdline_complete(cl, NULL, &i, dst, sizeof(dst)) >= 0)
		goto error;
	if (cmdline_complete(cl, "buffer", NULL, dst, sizeof(dst)) >= 0)
		goto error;
	if (cmdline_complete(cl, "buffer", &i, NULL, sizeof(dst)) >= 0)
		goto error;

	cmdline_free(cl);
	return 0;

error:
	printf("Error: function accepted null parameter!\n");
	cmdline_free(cl);
	return -1;
}

static int
test_cmdline_rdline_fns(void)
{
	struct rdline *rdl;
	rdline_write_char_t *wc = &cmdline_write_char;
	rdline_validate_t *v = &valid_buffer;
	rdline_complete_t *c = &complete_buffer;

	rdl = rdline_new(NULL, v, c, NULL);
	if (rdl != NULL)
		goto error;
	rdl = rdline_new(wc, NULL, c, NULL);
	if (rdl != NULL)
		goto error;
	rdl = rdline_new(wc, v, NULL, NULL);
	if (rdl != NULL)
		goto error;
	if (rdline_char_in(NULL, 0) >= 0)
		goto error;
	if (rdline_get_buffer(NULL) != NULL)
		goto error;
	if (rdline_add_history(NULL, "history") >= 0)
		goto error;
	if (rdline_add_history(rdl, NULL) >= 0)
		goto error;
	if (rdline_get_history_item(NULL, 0) != NULL)
		goto error;

	/* void functions */
	rdline_get_history_buffer_size(NULL);
	rdline_get_opaque(NULL);
	rdline_newline(NULL, "prompt");
	rdline_newline(rdl, NULL);
	rdline_stop(NULL);
	rdline_quit(NULL);
	rdline_restart(NULL);
	rdline_redisplay(NULL);
	rdline_reset(NULL);
	rdline_clear_history(NULL);
	rdline_free(NULL);

	rdline_free(rdl);
	return 0;

error:
	printf("Error: function accepted null parameter!\n");
	rdline_free(rdl);
	return -1;
}

static int
test_cmdline_vt100_fns(void)
{
	if (vt100_parser(NULL, 0) >= 0) {
		printf("Error: function accepted null parameter!\n");
		return -1;
	}

	/* void functions */
	vt100_init(NULL);

	return 0;
}

static int
test_cmdline_socket_fns(void)
{
	cmdline_parse_ctx_t ctx;
	struct cmdline *cl;

	cl = cmdline_stdin_new(NULL, "prompt");
	if (cl != NULL)
		goto error;
	cl = cmdline_stdin_new(&ctx, NULL);
	if (cl != NULL)
		goto error;
	cl = cmdline_file_new(NULL, "prompt", NULL_INPUT);
	if (cl != NULL)
		goto error;
	cl = cmdline_file_new(&ctx, NULL, NULL_INPUT);
	if (cl != NULL)
		goto error;
	cl = cmdline_file_new(&ctx, "prompt", NULL);
	if (cl != NULL)
		goto error;
	cl = cmdline_file_new(&ctx, "prompt", "-/invalid/~/path");
	if (cl != NULL) {
		printf("Error: succeeded in opening invalid file for reading!");
		cmdline_free(cl);
		return -1;
	}
	cl = cmdline_file_new(&ctx, "prompt", NULL_INPUT);
	if (cl == NULL) {
		printf("Error: failed to open /dev/null for reading!");
		return -1;
	}
	cmdline_free(cl);
	cl = NULL;

	/* void functions */
	cmdline_stdin_exit(NULL);

	return 0;
error:
	printf("Error: function accepted null parameter!\n");
	cmdline_free(cl);
	return -1;
}

static int
test_cmdline_fns(void)
{
	cmdline_parse_ctx_t ctx;
	struct cmdline *cl;

	memset(&ctx, 0, sizeof(ctx));
	cl = cmdline_new(NULL, "prompt", 0, 0);
	if (cl != NULL)
		goto error;
	cl = cmdline_new(&ctx, NULL, 0, 0);
	if (cl != NULL)
		goto error;
	cl = cmdline_new(&ctx, "test", -1, -1);
	if (cl == NULL)
		goto error;
	if (cmdline_in(NULL, "buffer", CMDLINE_TEST_BUFSIZE) >= 0)
		goto error;
	if (cmdline_in(cl, NULL, CMDLINE_TEST_BUFSIZE) >= 0)
		goto error;
	if (cmdline_write_char(NULL, 0) >= 0)
		goto error;

	/* void functions */
	cmdline_set_prompt(NULL, "prompt");
	cmdline_free(NULL);
	cmdline_printf(NULL, "format");
	cmdline_interact(NULL);
	cmdline_quit(NULL);

	cmdline_free(cl);
	return 0;

error:
	printf("Error: function accepted null parameter!\n");
	if (cl != NULL)
		cmdline_free(cl);
	return -1;
}

/* test library functions. the point of these tests is not so much to test
 * functions' behaviour as it is to make sure there are no segfaults if
 * they are called with invalid parameters.
 */
int
test_cmdline_lib(void)
{
	if (test_cmdline_parse_fns() < 0)
		return -1;
	if (test_cmdline_rdline_fns() < 0)
		return -1;
	if (test_cmdline_vt100_fns() < 0)
		return -1;
	if (test_cmdline_socket_fns() < 0)
		return -1;
	if (test_cmdline_fns() < 0)
		return -1;
	return 0;
}
