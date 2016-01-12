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

#ifndef _RDLINE_H_
#define _RDLINE_H_

/**
 * This file is a small equivalent to the GNU readline library, but it
 * was originally designed for small systems, like Atmel AVR
 * microcontrollers (8 bits). Indeed, we don't use any malloc that is
 * sometimes not implemented (or just not recommended) on such
 * systems.
 *
 * Obviously, it does not support as many things as the GNU readline,
 * but at least it supports some interesting features like a kill
 * buffer and a command history.
 *
 * It also have a feature that does not have the GNU readline (as far
 * as I know): we can have several instances of it running at the same
 * time, even on a monothread program, since it works with callbacks.
 *
 * The lib is designed for a client-side or a server-side use:
 * - server-side: the server receives all data from a socket, including
 *   control chars, like arrows, tabulations, ... The client is
 *   very simple, it can be a telnet or a minicom through a serial line.
 * - client-side: the client receives its data through its stdin for
 *   instance.
 */

#include <stdio.h>
#include <cmdline_cirbuf.h>
#include <cmdline_vt100.h>

#ifdef __cplusplus
extern "C" {
#endif

/* configuration */
#define RDLINE_BUF_SIZE 512
#define RDLINE_PROMPT_SIZE  32
#define RDLINE_VT100_BUF_SIZE  8
#define RDLINE_HISTORY_BUF_SIZE BUFSIZ
#define RDLINE_HISTORY_MAX_LINE 64

enum rdline_status {
	RDLINE_INIT,
	RDLINE_RUNNING,
	RDLINE_EXITED
};

struct rdline;

typedef int (rdline_write_char_t)(struct rdline *rdl, char);
typedef void (rdline_validate_t)(struct rdline *rdl,
				 const char *buf, unsigned int size);
typedef int (rdline_complete_t)(struct rdline *rdl, const char *buf,
				char *dstbuf, unsigned int dstsize,
				int *state);

struct rdline {
	enum rdline_status status;
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

/**
 * Init fields for a struct rdline. Call this only once at the beginning
 * of your program.
 * \param rdl A pointer to an uninitialized struct rdline
 * \param write_char The function used by the function to write a character
 * \param validate A pointer to the function to execute when the
 *                 user validates the buffer.
 * \param complete A pointer to the function to execute when the
 *                 user completes the buffer.
 */
int rdline_init(struct rdline *rdl,
		 rdline_write_char_t *write_char,
		 rdline_validate_t *validate,
		 rdline_complete_t *complete);


/**
 * Init the current buffer, and display a prompt.
 * \param rdl A pointer to a struct rdline
 * \param prompt A string containing the prompt
 */
void rdline_newline(struct rdline *rdl, const char *prompt);

/**
 * Call it and all received chars will be ignored.
 * \param rdl A pointer to a struct rdline
 */
void rdline_stop(struct rdline *rdl);

/**
 * Same than rdline_stop() except that next calls to rdline_char_in()
 * will return RDLINE_RES_EXITED.
 * \param rdl A pointer to a struct rdline
 */
void rdline_quit(struct rdline *rdl);

/**
 * Restart after a call to rdline_stop() or rdline_quit()
 * \param rdl A pointer to a struct rdline
 */
void rdline_restart(struct rdline *rdl);

/**
 * Redisplay the current buffer
 * \param rdl A pointer to a struct rdline
 */
void rdline_redisplay(struct rdline *rdl);

/**
 * Reset the current buffer and setup for a new line.
 *  \param rdl A pointer to a struct rdline
 */
void rdline_reset(struct rdline *rdl);


/* return status for rdline_char_in() */
#define RDLINE_RES_SUCCESS       0
#define RDLINE_RES_VALIDATED     1
#define RDLINE_RES_COMPLETE      2
#define RDLINE_RES_NOT_RUNNING  -1
#define RDLINE_RES_EOF          -2
#define RDLINE_RES_EXITED       -3

/**
 * append a char to the readline buffer.
 * Return RDLINE_RES_VALIDATE when the line has been validated.
 * Return RDLINE_RES_COMPLETE when the user asked to complete the buffer.
 * Return RDLINE_RES_NOT_RUNNING if it is not running.
 * Return RDLINE_RES_EOF if EOF (ctrl-d on an empty line).
 * Else return RDLINE_RES_SUCCESS.
 * XXX error case when the buffer is full ?
 *
 * \param rdl A pointer to a struct rdline
 * \param c The character to append
 */
int rdline_char_in(struct rdline *rdl, char c);

/**
 * Return the current buffer, terminated by '\0'.
 * \param rdl A pointer to a struct rdline
 */
const char *rdline_get_buffer(struct rdline *rdl);


/**
 * Add the buffer to history.
 * return < 0 on error.
 * \param rdl A pointer to a struct rdline
 * \param buf A buffer that is terminated by '\0'
 */
int rdline_add_history(struct rdline *rdl, const char *buf);

/**
 * Clear current history
 * \param rdl A pointer to a struct rdline
 */
void rdline_clear_history(struct rdline *rdl);

/**
 * Get the i-th history item
 */
char *rdline_get_history_item(struct rdline *rdl, unsigned int i);

#ifdef __cplusplus
}
#endif

#endif /* _RDLINE_H_ */
