/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 2009, Olivier MATZ <zer0@droids-corp.org>
 * All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the University of California, Berkeley nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE REGENTS AND CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <termios.h>

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
