/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation.
 * Copyright (c) 2009, Olivier MATZ <zer0@droids-corp.org>
 * All rights reserved.
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

#include "cmdline_vt100.h"

const char *cmdline_vt100_commands[] = {
	vt100_up_arr,
	vt100_down_arr,
	vt100_right_arr,
	vt100_left_arr,
	"\177",
	"\n",
	"\001",
	"\005",
	"\013",
	"\031",
	"\003",
	"\006",
	"\002",
	vt100_suppr,
	vt100_tab,
	"\004",
	"\014",
	"\r",
	"\033\177",
	vt100_word_left,
	vt100_word_right,
	"?",
	"\027",
	"\020",
	"\016",
	"\033\144",
	vt100_bs,
};

void
vt100_init(struct cmdline_vt100 *vt)
{
	if (!vt)
		return;
	vt->state = CMDLINE_VT100_INIT;
}


static int
match_command(char *buf, unsigned int size)
{
	const char *cmd;
	size_t cmdlen;
	unsigned int i = 0;

	for (i=0 ; i<sizeof(cmdline_vt100_commands)/sizeof(const char *) ; i++) {
		cmd = *(cmdline_vt100_commands + i);

		cmdlen = strnlen(cmd, CMDLINE_VT100_BUF_SIZE);
		if (size == cmdlen &&
		    !strncmp(buf, cmd, cmdlen)) {
			return i;
		}
	}

	return -1;
}

int
vt100_parser(struct cmdline_vt100 *vt, char ch)
{
	unsigned int size;
	uint8_t c = (uint8_t) ch;

	if (!vt)
		return -1;

	if (vt->bufpos >= CMDLINE_VT100_BUF_SIZE) {
		vt->state = CMDLINE_VT100_INIT;
		vt->bufpos = 0;
	}

	vt->buf[vt->bufpos++] = c;
	size = vt->bufpos;

	switch (vt->state) {
	case CMDLINE_VT100_INIT:
		if (c == 033) {
			vt->state = CMDLINE_VT100_ESCAPE;
		}
		else {
			vt->bufpos = 0;
			goto match_command;
		}
		break;

	case CMDLINE_VT100_ESCAPE:
		if (c == 0133) {
			vt->state = CMDLINE_VT100_ESCAPE_CSI;
		}
		else if (c >= 060 && c <= 0177) { /* XXX 0177 ? */
			vt->bufpos = 0;
			vt->state = CMDLINE_VT100_INIT;
			goto match_command;
		}
		break;

	case CMDLINE_VT100_ESCAPE_CSI:
		if (c >= 0100 && c <= 0176) {
			vt->bufpos = 0;
			vt->state = CMDLINE_VT100_INIT;
			goto match_command;
		}
		break;

	default:
		vt->bufpos = 0;
		break;
	}

	return -2;

 match_command:
	return match_command(vt->buf, size);
}
