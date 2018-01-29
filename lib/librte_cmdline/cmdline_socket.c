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
#include <inttypes.h>
#include <fcntl.h>
#include <termios.h>

#include "cmdline_parse.h"
#include "cmdline_rdline.h"
#include "cmdline_socket.h"
#include "cmdline.h"

struct cmdline *
cmdline_file_new(cmdline_parse_ctx_t *ctx, const char *prompt, const char *path)
{
	int fd;

	/* everything else is checked in cmdline_new() */
	if (!path)
		return NULL;

	fd = open(path, O_RDONLY, 0);
	if (fd < 0) {
		dprintf("open() failed\n");
		return NULL;
	}
	return cmdline_new(ctx, prompt, fd, -1);
}

struct cmdline *
cmdline_stdin_new(cmdline_parse_ctx_t *ctx, const char *prompt)
{
	struct cmdline *cl;
	struct termios oldterm, term;

	tcgetattr(0, &oldterm);
	memcpy(&term, &oldterm, sizeof(term));
	term.c_lflag &= ~(ICANON | ECHO | ISIG);
	tcsetattr(0, TCSANOW, &term);
	setbuf(stdin, NULL);

	cl = cmdline_new(ctx, prompt, 0, 1);

	if (cl)
		memcpy(&cl->oldterm, &oldterm, sizeof(term));

	return cl;
}

void
cmdline_stdin_exit(struct cmdline *cl)
{
	if (!cl)
		return;

	tcsetattr(fileno(stdin), TCSANOW, &cl->oldterm);
}
