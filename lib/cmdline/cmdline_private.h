/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2020 Dmitry Kozlyuk
 */

#ifndef _CMDLINE_PRIVATE_H_
#define _CMDLINE_PRIVATE_H_

#include <stdarg.h>

#include <rte_common.h>
#include <rte_os_shim.h>
#ifdef RTE_EXEC_ENV_WINDOWS
#include <rte_windows.h>
#else
#include <termios.h>
#endif

#include <cmdline.h>

#define RDLINE_BUF_SIZE 512
#define RDLINE_PROMPT_SIZE  32
#define RDLINE_VT100_BUF_SIZE  8
#define RDLINE_HISTORY_BUF_SIZE BUFSIZ
#define RDLINE_HISTORY_MAX_LINE 64

struct rdline {
	volatile enum rdline_status status;
	/* rdline bufs */
	struct cirbuf left;
	struct cirbuf right;
	char left_buf[RDLINE_BUF_SIZE+2]; /* reserve 2 chars for the \n\0 */
	char right_buf[RDLINE_BUF_SIZE];

	char prompt[RDLINE_PROMPT_SIZE];
	unsigned int prompt_size;

	char kill_buf[RDLINE_BUF_SIZE];
	unsigned int kill_size;

	/* history */
	struct cirbuf history;
	char history_buf[RDLINE_HISTORY_BUF_SIZE];
	int history_cur_line;

	/* callbacks and func pointers */
	rdline_write_char_t *write_char;
	rdline_validate_t *validate;
	rdline_complete_t *complete;

	/* vt100 parser */
	struct cmdline_vt100 vt100;

	/* opaque pointer */
	void *opaque;
};

#ifdef RTE_EXEC_ENV_WINDOWS
struct terminal {
	DWORD input_mode;
	DWORD output_mode;
	int is_console_input;
	int is_console_output;
};
#endif

struct cmdline {
	int s_in;
	int s_out;
	cmdline_parse_ctx_t *ctx;
	struct rdline rdl;
	char prompt[RDLINE_PROMPT_SIZE];
#ifdef RTE_EXEC_ENV_WINDOWS
	struct terminal oldterm;
	char repeated_char;
	WORD repeat_count;
#else
	struct termios oldterm;
#endif
};

/* Disable buffering and echoing, save previous settings to oldterm. */
void terminal_adjust(struct cmdline *cl);

/* Restore terminal settings form oldterm. */
void terminal_restore(const struct cmdline *cl);

/* Read one character from input. */
ssize_t cmdline_read_char(struct cmdline *cl, char *c);

/* Force current cmdline read to unblock. */
void cmdline_cancel(struct cmdline *cl);

/* vdprintf(3) */
__rte_format_printf(2, 0)
int cmdline_vdprintf(int fd, const char *format, va_list op);

int rdline_init(struct rdline *rdl,
		rdline_write_char_t *write_char,
		rdline_validate_t *validate,
		rdline_complete_t *complete,
		void *opaque);

#endif
