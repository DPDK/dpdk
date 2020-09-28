/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2020 Dmitry Kozlyuk
 */

#ifndef _CMDLINE_PRIVATE_H_
#define _CMDLINE_PRIVATE_H_

#include <stdarg.h>

#include <rte_common.h>
#ifdef RTE_EXEC_ENV_WINDOWS
#include <rte_windows.h>
#endif

#include <cmdline.h>

#ifdef RTE_EXEC_ENV_WINDOWS
struct terminal {
	DWORD input_mode;
	DWORD output_mode;
	int is_console_input;
	int is_console_output;
};

struct cmdline {
	int s_in;
	int s_out;
	cmdline_parse_ctx_t *ctx;
	struct rdline rdl;
	char prompt[RDLINE_PROMPT_SIZE];
	struct terminal oldterm;
	char repeated_char;
	WORD repeat_count;
};
#endif

/* Disable buffering and echoing, save previous settings to oldterm. */
void terminal_adjust(struct cmdline *cl);

/* Restore terminal settings form oldterm. */
void terminal_restore(const struct cmdline *cl);

/* Check if a single character can be read from input. */
int cmdline_poll_char(struct cmdline *cl);

/* Read one character from input. */
ssize_t cmdline_read_char(struct cmdline *cl, char *c);

/* vdprintf(3) */
__rte_format_printf(2, 0)
int cmdline_vdprintf(int fd, const char *format, va_list op);

#endif
