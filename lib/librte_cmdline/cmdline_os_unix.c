/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2020 Dmitry Kozlyuk
 */

#include <string.h>

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
