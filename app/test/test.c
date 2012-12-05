/*-
 *   BSD LICENSE
 * 
 *   Copyright(c) 2010-2012 Intel Corporation. All rights reserved.
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
 * 
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

#include <cmdline_rdline.h>
#include <cmdline_parse.h>
#include <cmdline_socket.h>
#include <cmdline.h>

#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_tailq.h>
#include <rte_eal.h>
#include <rte_timer.h>
#include <rte_string_fns.h>

#include "test.h"

const char *prgname; /* to be set to argv[0] */

#ifndef RTE_EXEC_ENV_BAREMETAL
static const char *recursive_call; /* used in linuxapp for MP and other tests */

static int
no_action(void){ return 0; }

static int
do_recursive_call(void)
{
	unsigned i;
	struct {
		const char *env_var;
		int (*action_fn)(void);
	} actions[] =  {
			{ "run_secondary_instances", test_mp_secondary },
			{ "test_missing_c_flag", no_action },
			{ "test_missing_n_flag", no_action },
			{ "test_no_hpet_flag", no_action },
			{ "test_invalid_b_flag", no_action },
			{ "test_invalid_r_flag", no_action },
			{ "test_misc_flags", no_action },
			{ "test_memory_flags", no_action },
			{ "test_file_prefix", no_action },
			{ "test_no_huge_flag", no_action },
	};

	if (recursive_call == NULL)
		return -1;
	for (i = 0; i < sizeof(actions)/sizeof(actions[0]); i++) {
		if (strcmp(actions[i].env_var, recursive_call) == 0)
			return (actions[i].action_fn)();
	}
	return -1;
}
#endif

void
test_hexdump(const char *title, const void *buf, unsigned int len)
{
	unsigned int i, out, ofs;
	const unsigned char *data = buf;
#define LINE_LEN 80
	char line[LINE_LEN];	/* space needed 8+16*3+3+16 == 75 */

	printf("%s at [%p], len=%u\n", title, data, len);
	ofs = 0;
	while (ofs < len) {
		/* format 1 line in the buffer, then use printf to print them */
		out = rte_snprintf(line, LINE_LEN, "%08X", ofs);
		for (i = 0; ofs+i < len && i < 16; i++)
			out += rte_snprintf(line+out, LINE_LEN - out, " %02X",
					data[ofs+i]&0xff);
		for(; i <= 16; i++)
			out += rte_snprintf(line+out, LINE_LEN - out, "   ");
		for(i = 0; ofs < len && i < 16; i++, ofs++) {
			unsigned char c = data[ofs];
			if (!isascii(c) || !isprint(c))
				c = '.';
			out += rte_snprintf(line+out, LINE_LEN - out, "%c", c);
		}
		printf("%s\n", line);
	}
}

int
main(int argc, char **argv)
{
	struct cmdline *cl;
	int ret;

	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		return -1;

	rte_timer_subsystem_init();

	argv += ret;

	prgname = argv[0];

#ifndef RTE_EXEC_ENV_BAREMETAL
	if ((recursive_call = getenv(RECURSIVE_ENV_VAR)) != NULL)
		return do_recursive_call();
#endif

	cl = cmdline_stdin_new(main_ctx, "RTE>>");
	if (cl == NULL) {
		return -1;
	}
	cmdline_interact(cl);
	cmdline_stdin_exit(cl);

	return 0;
}
