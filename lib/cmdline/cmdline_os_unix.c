/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2020 Dmitry Kozlyuk
 */

#include <poll.h>
#include <string.h>
#include <unistd.h>

#include "cmdline_private.h"

void
terminal_adjust(struct cmdline *cl)
{
	struct termios term;

	tcgetattr(0, &cl->oldterm);

	memcpy(&term, &cl->oldterm, sizeof(term));
	term.c_lflag &= ~(ICANON | ECHO | ISIG);
	tcsetattr(0, TCSANOW, &term);

	setbuf(stdin, NULL);
}

void
terminal_restore(const struct cmdline *cl)
{
	tcsetattr(fileno(stdin), TCSANOW, &cl->oldterm);
}

ssize_t
cmdline_read_char(struct cmdline *cl, char *c)
{
	return read(cl->s_in, c, 1);
}

int
cmdline_vdprintf(int fd, const char *format, va_list op)
{
	return vdprintf(fd, format, op);
}

/* This function is not needed on Linux, instead use sigaction() */
void
cmdline_cancel(__rte_unused struct cmdline *cl)
{
}
