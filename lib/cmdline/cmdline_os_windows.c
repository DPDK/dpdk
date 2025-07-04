/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2020 Dmitry Kozlyuk
 */

#include <io.h>

#include "cmdline_private.h"

/* Missing from some MinGW-w64 distributions. */
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif

#ifndef ENABLE_VIRTUAL_TERMINAL_INPUT
#define ENABLE_VIRTUAL_TERMINAL_INPUT 0x0200
#endif

void
terminal_adjust(struct cmdline *cl)
{
	HANDLE handle;
	DWORD mode;

	ZeroMemory(&cl->oldterm, sizeof(cl->oldterm));

	/* Detect console input, set it up and make it emulate VT100. */
	handle = GetStdHandle(STD_INPUT_HANDLE);
	if (GetConsoleMode(handle, &mode)) {
		cl->oldterm.is_console_input = 1;
		cl->oldterm.input_mode = mode;

		mode &= ~(
			ENABLE_LINE_INPUT |      /* no line buffering */
			ENABLE_ECHO_INPUT |      /* no echo */
			ENABLE_PROCESSED_INPUT | /* pass Ctrl+C to program */
			ENABLE_MOUSE_INPUT |     /* no mouse events */
			ENABLE_WINDOW_INPUT);    /* no window resize events */
		mode |= ENABLE_VIRTUAL_TERMINAL_INPUT;
		SetConsoleMode(handle, mode);
	}

	/* Detect console output and make it emulate VT100. */
	handle = GetStdHandle(STD_OUTPUT_HANDLE);
	if (GetConsoleMode(handle, &mode)) {
		cl->oldterm.is_console_output = 1;
		cl->oldterm.output_mode = mode;

		mode &= ~ENABLE_WRAP_AT_EOL_OUTPUT;
		mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
		SetConsoleMode(handle, mode);
	}
}

void
terminal_restore(const struct cmdline *cl)
{
	if (cl->oldterm.is_console_input) {
		HANDLE handle = GetStdHandle(STD_INPUT_HANDLE);
		SetConsoleMode(handle, cl->oldterm.input_mode);
	}

	if (cl->oldterm.is_console_output) {
		HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
		SetConsoleMode(handle, cl->oldterm.output_mode);
	}
}

static int
cmdline_is_key_down(const INPUT_RECORD *record)
{
	return (record->EventType == KEY_EVENT) &&
		record->Event.KeyEvent.bKeyDown;
}

ssize_t
cmdline_read_char(struct cmdline *cl, char *c)
{
	HANDLE handle;
	INPUT_RECORD record;
	KEY_EVENT_RECORD *key;
	DWORD events;

	if (!cl->oldterm.is_console_input)
		return _read(cl->s_in, c, 1);

	/* Return repeated strokes from previous event. */
	if (cl->repeat_count > 0) {
		*c = cl->repeated_char;
		cl->repeat_count--;
		return 1;
	}

	handle = (HANDLE)_get_osfhandle(cl->s_in);
	key = &record.Event.KeyEvent;
	do {
		if (!ReadConsoleInput(handle, &record, 1, &events)) {
			if (GetLastError() == ERROR_HANDLE_EOF) {
				*c = EOF;
				return 0;
			}
			return -1;
		}
	} while (!cmdline_is_key_down(&record));

	*c = key->uChar.AsciiChar;

	/* Save repeated strokes from a single event. */
	if (key->wRepeatCount > 1) {
		cl->repeated_char = *c;
		cl->repeat_count = key->wRepeatCount - 1;
	}

	return 1;
}

int
cmdline_vdprintf(int fd, const char *format, va_list op)
{
	int copy, ret;
	FILE *file;

	copy = _dup(fd);
	if (copy < 0)
		return -1;

	file = _fdopen(copy, "a");
	if (file == NULL) {
		_close(copy);
		return -1;
	}

	ret = vfprintf(file, format, op);

	fclose(file); /* also closes copy */

	return ret;
}

void
cmdline_cancel(struct cmdline *cl)
{
	if (!cl)
		return;

	/* force the outstanding read on console to exit */
	if (cl->oldterm.is_console_input) {
		HANDLE handle = (HANDLE)_get_osfhandle(cl->s_in);

		CancelIoEx(handle, NULL);
	}
}
