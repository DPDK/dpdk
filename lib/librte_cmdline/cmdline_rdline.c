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
 *  version: DPDK.L.1.2.3-3
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
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

#include "cmdline_cirbuf.h"
#include "cmdline_rdline.h"

static void rdline_puts(struct rdline *rdl, const char *buf);
static void rdline_miniprintf(struct rdline *rdl,
			      const char *buf, unsigned int val);

#ifndef NO_RDLINE_HISTORY
static void rdline_remove_old_history_item(struct rdline *rdl);
static void rdline_remove_first_history_item(struct rdline *rdl);
static unsigned int rdline_get_history_size(struct rdline *rdl);
#endif /* !NO_RDLINE_HISTORY */


/* isblank() needs _XOPEN_SOURCE >= 600 || _ISOC99_SOURCE, so use our
 * own. */
static int
isblank2(char c)
{
	if (c == ' ' ||
	    c == '\t' )
		return 1;
	return 0;
}

void
rdline_init(struct rdline *rdl,
		 rdline_write_char_t *write_char,
		 rdline_validate_t *validate,
		 rdline_complete_t *complete)
{
	memset(rdl, 0, sizeof(*rdl));
	rdl->validate = validate;
	rdl->complete = complete;
	rdl->write_char = write_char;
	rdl->status = RDLINE_INIT;
#ifndef NO_RDLINE_HISTORY
	cirbuf_init(&rdl->history, rdl->history_buf, 0, RDLINE_HISTORY_BUF_SIZE);
#endif /* !NO_RDLINE_HISTORY */
}

void
rdline_newline(struct rdline *rdl, const char *prompt)
{
	unsigned int i;

	vt100_init(&rdl->vt100);
	cirbuf_init(&rdl->left, rdl->left_buf, 0, RDLINE_BUF_SIZE);
	cirbuf_init(&rdl->right, rdl->right_buf, 0, RDLINE_BUF_SIZE);

	if (prompt != rdl->prompt)
		memcpy(rdl->prompt, prompt, sizeof(rdl->prompt)-1);
	rdl->prompt_size = strnlen(prompt, RDLINE_PROMPT_SIZE);

	for (i=0 ; i<rdl->prompt_size ; i++)
		rdl->write_char(rdl, rdl->prompt[i]);
	rdl->status = RDLINE_RUNNING;

#ifndef NO_RDLINE_HISTORY
	rdl->history_cur_line = -1;
#endif /* !NO_RDLINE_HISTORY */
}

void
rdline_stop(struct rdline *rdl)
{
	rdl->status = RDLINE_INIT;
}

void
rdline_quit(struct rdline *rdl)
{
	rdl->status = RDLINE_EXITED;
}

void
rdline_restart(struct rdline *rdl)
{
	rdl->status = RDLINE_RUNNING;
}

void
rdline_reset(struct rdline *rdl)
{
	vt100_init(&rdl->vt100);
	cirbuf_init(&rdl->left, rdl->left_buf, 0, RDLINE_BUF_SIZE);
	cirbuf_init(&rdl->right, rdl->right_buf, 0, RDLINE_BUF_SIZE);

	rdl->status = RDLINE_RUNNING;

#ifndef NO_RDLINE_HISTORY
	rdl->history_cur_line = -1;
#endif	/* !NO_RDLINE_HISTORY */
}

const char *
rdline_get_buffer(struct rdline *rdl)
{
	unsigned int len_l, len_r;
	cirbuf_align_left(&rdl->left);
	cirbuf_align_left(&rdl->right);

	len_l = CIRBUF_GET_LEN(&rdl->left);
	len_r = CIRBUF_GET_LEN(&rdl->right);
	memcpy(rdl->left_buf+len_l, rdl->right_buf, len_r);

	rdl->left_buf[len_l + len_r] = '\n';
	rdl->left_buf[len_l + len_r + 1] = '\0';
	return rdl->left_buf;
}

static void
display_right_buffer(struct rdline *rdl, int force)
{
	unsigned int i;
	char tmp;

	if (!force && CIRBUF_IS_EMPTY(&rdl->right))
		return;

	rdline_puts(rdl, vt100_clear_right);
	CIRBUF_FOREACH(&rdl->right, i, tmp) {
		rdl->write_char(rdl, tmp);
	}
	if (!CIRBUF_IS_EMPTY(&rdl->right))
		rdline_miniprintf(rdl, vt100_multi_left,
				  CIRBUF_GET_LEN(&rdl->right));
}

void
rdline_redisplay(struct rdline *rdl)
{
	unsigned int i;
	char tmp;

	rdline_puts(rdl, vt100_home);
	for (i=0 ; i<rdl->prompt_size ; i++)
		rdl->write_char(rdl, rdl->prompt[i]);
	CIRBUF_FOREACH(&rdl->left, i, tmp) {
		rdl->write_char(rdl, tmp);
	}
	display_right_buffer(rdl, 1);
}

int
rdline_char_in(struct rdline *rdl, char c)
{
	unsigned int i;
	int cmd;
	char tmp;
#ifndef NO_RDLINE_HISTORY
	char *buf;
#endif

	if (rdl->status == RDLINE_EXITED)
		return RDLINE_RES_EXITED;
	if (rdl->status != RDLINE_RUNNING)
		return RDLINE_RES_NOT_RUNNING;

	cmd = vt100_parser(&rdl->vt100, c);
	if (cmd == -2)
		return RDLINE_RES_SUCCESS;

	if (cmd >= 0) {
		switch (cmd) {
		case CMDLINE_KEY_CTRL_B:
		case CMDLINE_KEY_LEFT_ARR:
			if (CIRBUF_IS_EMPTY(&rdl->left))
				break;
			tmp = cirbuf_get_tail(&rdl->left);
			cirbuf_del_tail(&rdl->left);
			cirbuf_add_head(&rdl->right, tmp);
			rdline_puts(rdl, vt100_left_arr);
			break;

		case CMDLINE_KEY_CTRL_F:
		case CMDLINE_KEY_RIGHT_ARR:
			if (CIRBUF_IS_EMPTY(&rdl->right))
				break;
			tmp = cirbuf_get_head(&rdl->right);
			cirbuf_del_head(&rdl->right);
			cirbuf_add_tail(&rdl->left, tmp);
			rdline_puts(rdl, vt100_right_arr);
			break;

		case CMDLINE_KEY_WLEFT:
			while (! CIRBUF_IS_EMPTY(&rdl->left) &&
			       (tmp = cirbuf_get_tail(&rdl->left)) &&
			       isblank2(tmp)) {
				rdline_puts(rdl, vt100_left_arr);
				cirbuf_del_tail(&rdl->left);
				cirbuf_add_head(&rdl->right, tmp);
			}
			while (! CIRBUF_IS_EMPTY(&rdl->left) &&
			       (tmp = cirbuf_get_tail(&rdl->left)) &&
			       !isblank2(tmp)) {
				rdline_puts(rdl, vt100_left_arr);
				cirbuf_del_tail(&rdl->left);
				cirbuf_add_head(&rdl->right, tmp);
			}
			break;

		case CMDLINE_KEY_WRIGHT:
			while (! CIRBUF_IS_EMPTY(&rdl->right) &&
			       (tmp = cirbuf_get_head(&rdl->right)) &&
			       isblank2(tmp)) {
				rdline_puts(rdl, vt100_right_arr);
				cirbuf_del_head(&rdl->right);
				cirbuf_add_tail(&rdl->left, tmp);
			}
			while (! CIRBUF_IS_EMPTY(&rdl->right) &&
			       (tmp = cirbuf_get_head(&rdl->right)) &&
			       !isblank2(tmp)) {
				rdline_puts(rdl, vt100_right_arr);
				cirbuf_del_head(&rdl->right);
				cirbuf_add_tail(&rdl->left, tmp);
			}
			break;

		case CMDLINE_KEY_BKSPACE:
			if(!cirbuf_del_tail_safe(&rdl->left)) {
				rdline_puts(rdl, vt100_bs);
				display_right_buffer(rdl, 1);
			}
			break;

		case CMDLINE_KEY_META_BKSPACE:
		case CMDLINE_KEY_CTRL_W:
			while (! CIRBUF_IS_EMPTY(&rdl->left) && isblank2(cirbuf_get_tail(&rdl->left))) {
				rdline_puts(rdl, vt100_bs);
				cirbuf_del_tail(&rdl->left);
			}
			while (! CIRBUF_IS_EMPTY(&rdl->left) && !isblank2(cirbuf_get_tail(&rdl->left))) {
				rdline_puts(rdl, vt100_bs);
				cirbuf_del_tail(&rdl->left);
			}
			display_right_buffer(rdl, 1);
			break;

		case CMDLINE_KEY_META_D:
			while (! CIRBUF_IS_EMPTY(&rdl->right) && isblank2(cirbuf_get_head(&rdl->right)))
				cirbuf_del_head(&rdl->right);
			while (! CIRBUF_IS_EMPTY(&rdl->right) && !isblank2(cirbuf_get_head(&rdl->right)))
				cirbuf_del_head(&rdl->right);
			display_right_buffer(rdl, 1);
			break;

		case CMDLINE_KEY_SUPPR:
		case CMDLINE_KEY_CTRL_D:
			if (cmd == CMDLINE_KEY_CTRL_D &&
			    CIRBUF_IS_EMPTY(&rdl->left) &&
			    CIRBUF_IS_EMPTY(&rdl->right)) {
				return RDLINE_RES_EOF;
			}
			if (!cirbuf_del_head_safe(&rdl->right)) {
				display_right_buffer(rdl, 1);
			}
			break;

		case CMDLINE_KEY_CTRL_A:
			if (CIRBUF_IS_EMPTY(&rdl->left))
				break;
			rdline_miniprintf(rdl, vt100_multi_left,
					    CIRBUF_GET_LEN(&rdl->left));
			while (! CIRBUF_IS_EMPTY(&rdl->left)) {
				tmp = cirbuf_get_tail(&rdl->left);
				cirbuf_del_tail(&rdl->left);
				cirbuf_add_head(&rdl->right, tmp);
			}
			break;

		case CMDLINE_KEY_CTRL_E:
			if (CIRBUF_IS_EMPTY(&rdl->right))
				break;
			rdline_miniprintf(rdl, vt100_multi_right,
					    CIRBUF_GET_LEN(&rdl->right));
			while (! CIRBUF_IS_EMPTY(&rdl->right)) {
				tmp = cirbuf_get_head(&rdl->right);
				cirbuf_del_head(&rdl->right);
				cirbuf_add_tail(&rdl->left, tmp);
			}
			break;

#ifndef NO_RDLINE_KILL_BUF
		case CMDLINE_KEY_CTRL_K:
			cirbuf_get_buf_head(&rdl->right, rdl->kill_buf, RDLINE_BUF_SIZE);
			rdl->kill_size = CIRBUF_GET_LEN(&rdl->right);
			cirbuf_del_buf_head(&rdl->right, rdl->kill_size);
			rdline_puts(rdl, vt100_clear_right);
			break;

		case CMDLINE_KEY_CTRL_Y:
			i=0;
			while(CIRBUF_GET_LEN(&rdl->right) + CIRBUF_GET_LEN(&rdl->left) <
			      RDLINE_BUF_SIZE &&
			      i < rdl->kill_size) {
				cirbuf_add_tail(&rdl->left, rdl->kill_buf[i]);
				rdl->write_char(rdl, rdl->kill_buf[i]);
				i++;
			}
			display_right_buffer(rdl, 0);
			break;
#endif /* !NO_RDLINE_KILL_BUF */

		case CMDLINE_KEY_CTRL_C:
			rdline_puts(rdl, "\r\n");
			rdline_newline(rdl, rdl->prompt);
			break;

		case CMDLINE_KEY_CTRL_L:
			rdline_redisplay(rdl);
			break;

		case CMDLINE_KEY_TAB:
		case CMDLINE_KEY_HELP:
			cirbuf_align_left(&rdl->left);
			rdl->left_buf[CIRBUF_GET_LEN(&rdl->left)] = '\0';
			if (rdl->complete) {
				char tmp_buf[BUFSIZ];
				int complete_state;
				int ret;
				unsigned int tmp_size;

				if (cmd == CMDLINE_KEY_TAB)
					complete_state = 0;
				else
					complete_state = -1;

				/* see in parse.h for help on complete() */
				ret = rdl->complete(rdl, rdl->left_buf,
						    tmp_buf, sizeof(tmp_buf),
						    &complete_state);
				/* no completion or error */
				if (ret <= 0) {
					return RDLINE_RES_COMPLETE;
				}

				tmp_size = strnlen(tmp_buf, sizeof(tmp_buf));
				/* add chars */
				if (ret == RDLINE_RES_COMPLETE) {
					i=0;
					while(CIRBUF_GET_LEN(&rdl->right) + CIRBUF_GET_LEN(&rdl->left) <
					      RDLINE_BUF_SIZE &&
					      i < tmp_size) {
						cirbuf_add_tail(&rdl->left, tmp_buf[i]);
						rdl->write_char(rdl, tmp_buf[i]);
						i++;
					}
					display_right_buffer(rdl, 1);
					return RDLINE_RES_COMPLETE; /* ?? */
				}

				/* choice */
				rdline_puts(rdl, "\r\n");
				while (ret) {
					rdl->write_char(rdl, ' ');
					for (i=0 ; tmp_buf[i] ; i++)
						rdl->write_char(rdl, tmp_buf[i]);
					rdline_puts(rdl, "\r\n");
					ret = rdl->complete(rdl, rdl->left_buf,
							    tmp_buf, sizeof(tmp_buf),
							    &complete_state);
				}

				rdline_redisplay(rdl);
			}
			return RDLINE_RES_COMPLETE;

		case CMDLINE_KEY_RETURN:
		case CMDLINE_KEY_RETURN2:
			rdline_get_buffer(rdl);
			rdl->status = RDLINE_INIT;
			rdline_puts(rdl, "\r\n");
#ifndef NO_RDLINE_HISTORY
			if (rdl->history_cur_line != -1)
				rdline_remove_first_history_item(rdl);
#endif

			if (rdl->validate)
				rdl->validate(rdl, rdl->left_buf, CIRBUF_GET_LEN(&rdl->left)+2);
			/* user may have stopped rdline */
			if (rdl->status == RDLINE_EXITED)
				return RDLINE_RES_EXITED;
			return RDLINE_RES_VALIDATED;

#ifndef NO_RDLINE_HISTORY
		case CMDLINE_KEY_UP_ARR:
		case CMDLINE_KEY_CTRL_P:
			if (rdl->history_cur_line == 0) {
				rdline_remove_first_history_item(rdl);
			}
			if (rdl->history_cur_line <= 0) {
				rdline_add_history(rdl, rdline_get_buffer(rdl));
				rdl->history_cur_line = 0;
			}

			buf = rdline_get_history_item(rdl, rdl->history_cur_line + 1);
			if (!buf)
				break;

			rdl->history_cur_line ++;
			vt100_init(&rdl->vt100);
			cirbuf_init(&rdl->left, rdl->left_buf, 0, RDLINE_BUF_SIZE);
			cirbuf_init(&rdl->right, rdl->right_buf, 0, RDLINE_BUF_SIZE);
			cirbuf_add_buf_tail(&rdl->left, buf, strnlen(buf, RDLINE_BUF_SIZE));
			rdline_redisplay(rdl);
			break;

		case CMDLINE_KEY_DOWN_ARR:
		case CMDLINE_KEY_CTRL_N:
			if (rdl->history_cur_line - 1 < 0)
				break;

			rdl->history_cur_line --;
			buf = rdline_get_history_item(rdl, rdl->history_cur_line);
			if (!buf)
				break;
			vt100_init(&rdl->vt100);
			cirbuf_init(&rdl->left, rdl->left_buf, 0, RDLINE_BUF_SIZE);
			cirbuf_init(&rdl->right, rdl->right_buf, 0, RDLINE_BUF_SIZE);
			cirbuf_add_buf_tail(&rdl->left, buf, strnlen(buf, RDLINE_BUF_SIZE));
			rdline_redisplay(rdl);

			break;
#endif /* !NO_RDLINE_HISTORY */


		default:
			break;
		}

		return RDLINE_RES_SUCCESS;
	}

	if (!isprint((int)c))
		return RDLINE_RES_SUCCESS;

	/* standard chars */
	if (CIRBUF_GET_LEN(&rdl->left) + CIRBUF_GET_LEN(&rdl->right) >= RDLINE_BUF_SIZE)
		return RDLINE_RES_SUCCESS;

	if (cirbuf_add_tail_safe(&rdl->left, c))
		return RDLINE_RES_SUCCESS;

	rdl->write_char(rdl, c);
	display_right_buffer(rdl, 0);

	return RDLINE_RES_SUCCESS;
}


/* HISTORY */

#ifndef NO_RDLINE_HISTORY
static void
rdline_remove_old_history_item(struct rdline * rdl)
{
	char tmp;

	while (! CIRBUF_IS_EMPTY(&rdl->history) ) {
		tmp = cirbuf_get_head(&rdl->history);
		cirbuf_del_head(&rdl->history);
		if (!tmp)
			break;
	}
}

static void
rdline_remove_first_history_item(struct rdline * rdl)
{
	char tmp;

	if ( CIRBUF_IS_EMPTY(&rdl->history) ) {
		return;
	}
	else {
		cirbuf_del_tail(&rdl->history);
	}

	while (! CIRBUF_IS_EMPTY(&rdl->history) ) {
		tmp = cirbuf_get_tail(&rdl->history);
		if (!tmp)
			break;
		cirbuf_del_tail(&rdl->history);
	}
}

static unsigned int
rdline_get_history_size(struct rdline * rdl)
{
	unsigned int i, tmp, ret=0;

	CIRBUF_FOREACH(&rdl->history, i, tmp) {
		if (tmp == 0)
			ret ++;
	}

	return ret;
}

char *
rdline_get_history_item(struct rdline * rdl, unsigned int idx)
{
	unsigned int len, i, tmp;

	len = rdline_get_history_size(rdl);
	if ( idx >= len ) {
		return NULL;
	}

	cirbuf_align_left(&rdl->history);

	CIRBUF_FOREACH(&rdl->history, i, tmp) {
		if ( idx == len - 1) {
			return rdl->history_buf + i;
		}
		if (tmp == 0)
			len --;
	}

	return NULL;
}

int
rdline_add_history(struct rdline * rdl, const char * buf)
{
	unsigned int len, i;

	len = strnlen(buf, RDLINE_BUF_SIZE);
	for (i=0; i<len ; i++) {
		if (buf[i] == '\n') {
			len = i;
			break;
		}
	}

	if ( len >= RDLINE_HISTORY_BUF_SIZE )
		return -1;

	while ( len >= CIRBUF_GET_FREELEN(&rdl->history) ) {
		rdline_remove_old_history_item(rdl);
	}

	cirbuf_add_buf_tail(&rdl->history, buf, len);
	cirbuf_add_tail(&rdl->history, 0);

	return 0;
}

void
rdline_clear_history(struct rdline * rdl)
{
	cirbuf_init(&rdl->history, rdl->history_buf, 0, RDLINE_HISTORY_BUF_SIZE);
}

#else /* !NO_RDLINE_HISTORY */

int rdline_add_history(struct rdline * rdl, const char * buf) {return -1;}
void rdline_clear_history(struct rdline * rdl) {}
char * rdline_get_history_item(struct rdline * rdl, unsigned int i) {return NULL;}


#endif /* !NO_RDLINE_HISTORY */


/* STATIC USEFUL FUNCS */

static void
rdline_puts(struct rdline * rdl, const char * buf)
{
	char c;
	while ( (c = *(buf++)) != '\0' ) {
		rdl->write_char(rdl, c);
	}
}

/* a very very basic printf with one arg and one format 'u' */
static void
rdline_miniprintf(struct rdline *rdl, const char * buf, unsigned int val)
{
	char c, started=0, div=100;

	while ( (c=*(buf++)) ) {
		if (c != '%') {
			rdl->write_char(rdl, c);
			continue;
		}
		c = *(buf++);
		if (c != 'u') {
			rdl->write_char(rdl, '%');
			rdl->write_char(rdl, c);
			continue;
		}
		/* val is never more than 255 */
		while (div) {
			c = (char)(val / div);
			if (c || started) {
				rdl->write_char(rdl, (char)(c+'0'));
				started = 1;
			}
			val %= div;
			div /= 10;
		}
	}
}

