/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>
#include <errno.h>
#include <termios.h>
#include <ctype.h>
#include <sys/queue.h>

#include <cmdline_vt100.h>
#include <cmdline_rdline.h>
#include <cmdline_parse.h>
#include <cmdline_socket.h>
#include <cmdline.h>

#include "test_cmdline.h"

/****************************************************************/
/* static functions required for some tests */
static void
valid_buffer(__attribute__((unused))struct rdline *rdl,
			__attribute__((unused))const char *buf,
			__attribute__((unused)) unsigned int size)
{
}

static int
complete_buffer(__attribute__((unused)) struct rdline *rdl,
			__attribute__((unused)) const char *buf,
			__attribute__((unused)) char *dstbuf,
			__attribute__((unused)) unsigned int dstsize,
			__attribute__((unused)) int *state)
{
	return 0;
}

/****************************************************************/

static int
test_cmdline_parse_fns(void)
{
	struct cmdline cl;
	int i = 0;
	char dst[CMDLINE_TEST_BUFSIZE];

	if (cmdline_parse(NULL, "buffer") >= 0)
		goto error;
	if (cmdline_parse(&cl, NULL) >= 0)
		goto error;

	if (cmdline_complete(NULL, "buffer", &i, dst, sizeof(dst)) >= 0)
		goto error;
	if (cmdline_complete(&cl, NULL, &i, dst, sizeof(dst)) >= 0)
		goto error;
	if (cmdline_complete(&cl, "buffer", NULL, dst, sizeof(dst)) >= 0)
		goto error;
	if (cmdline_complete(&cl, "buffer", &i, NULL, sizeof(dst)) >= 0)
		goto error;

	return 0;

error:
	printf("Error: function accepted null parameter!\n");
	return -1;
}

static int
test_cmdline_rdline_fns(void)
{
	struct rdline rdl;
	rdline_write_char_t *wc = &cmdline_write_char;
	rdline_validate_t *v = &valid_buffer;
	rdline_complete_t *c = &complete_buffer;

	if (rdline_init(NULL, wc, v, c) >= 0)
		goto error;
	if (rdline_init(&rdl, NULL, v, c) >= 0)
		goto error;
	if (rdline_init(&rdl, wc, NULL, c) >= 0)
		goto error;
	if (rdline_init(&rdl, wc, v, NULL) >= 0)
		goto error;
	if (rdline_char_in(NULL, 0) >= 0)
		goto error;
	if (rdline_get_buffer(NULL) != NULL)
		goto error;
	if (rdline_add_history(NULL, "history") >= 0)
		goto error;
	if (rdline_add_history(&rdl, NULL) >= 0)
		goto error;
	if (rdline_get_history_item(NULL, 0) != NULL)
		goto error;

	/* void functions */
	rdline_newline(NULL, "prompt");
	rdline_newline(&rdl, NULL);
	rdline_stop(NULL);
	rdline_quit(NULL);
	rdline_restart(NULL);
	rdline_redisplay(NULL);
	rdline_reset(NULL);
	rdline_clear_history(NULL);

	return 0;

error:
	printf("Error: function accepted null parameter!\n");
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

	if (cmdline_stdin_new(NULL, "prompt") != NULL)
		goto error;
	if (cmdline_stdin_new(&ctx, NULL) != NULL)
		goto error;
	if (cmdline_file_new(NULL, "prompt", "/dev/null") != NULL)
		goto error;
	if (cmdline_file_new(&ctx, NULL, "/dev/null") != NULL)
		goto error;
	if (cmdline_file_new(&ctx, "prompt", NULL) != NULL)
		goto error;
	if (cmdline_file_new(&ctx, "prompt", "-/invalid/~/path") != NULL) {
		printf("Error: succeeded in opening invalid file for reading!");
		return -1;
	}
	if (cmdline_file_new(&ctx, "prompt", "/dev/null") == NULL) {
		printf("Error: failed to open /dev/null for reading!");
		return -1;
	}

	/* void functions */
	cmdline_stdin_exit(NULL);

	return 0;
error:
	printf("Error: function accepted null parameter!\n");
	return -1;
}

static int
test_cmdline_fns(void)
{
	cmdline_parse_ctx_t ctx;
	struct cmdline cl, *tmp;

	memset(&ctx, 0, sizeof(ctx));
	tmp = cmdline_new(&ctx, "test", -1, -1);
	if (tmp == NULL)
		goto error;

	if (cmdline_new(NULL, "prompt", 0, 0) != NULL)
		goto error;
	if (cmdline_new(&ctx, NULL, 0, 0) != NULL)
		goto error;
	if (cmdline_in(NULL, "buffer", CMDLINE_TEST_BUFSIZE) >= 0)
		goto error;
	if (cmdline_in(&cl, NULL, CMDLINE_TEST_BUFSIZE) >= 0)
		goto error;
	if (cmdline_write_char(NULL, 0) >= 0)
		goto error;

	/* void functions */
	cmdline_set_prompt(NULL, "prompt");
	cmdline_free(NULL);
	cmdline_printf(NULL, "format");
	/* this should fail as stream handles are invalid */
	cmdline_printf(tmp, "format");
	cmdline_interact(NULL);
	cmdline_quit(NULL);

	/* check if void calls change anything when they should fail */
	cl = *tmp;

	cmdline_printf(&cl, NULL);
	if (memcmp(&cl, tmp, sizeof(cl))) goto mismatch;
	cmdline_set_prompt(&cl, NULL);
	if (memcmp(&cl, tmp, sizeof(cl))) goto mismatch;
	cmdline_in(&cl, NULL, CMDLINE_TEST_BUFSIZE);
	if (memcmp(&cl, tmp, sizeof(cl))) goto mismatch;

	cmdline_free(tmp);

	return 0;

error:
	printf("Error: function accepted null parameter!\n");
	return -1;
mismatch:
	printf("Error: data changed!\n");
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
