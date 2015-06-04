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

#include <string.h>
#include <errno.h>
#include <stdio.h>

#include "cmdline_cirbuf.h"


int
cirbuf_init(struct cirbuf *cbuf, char *buf, unsigned int start, unsigned int maxlen)
{
	if (!cbuf || !buf)
		return -EINVAL;
	cbuf->maxlen = maxlen;
	cbuf->len = 0;
	cbuf->start = start;
	cbuf->end = start;
	cbuf->buf = buf;
	return 0;
}

/* multiple add */

int
cirbuf_add_buf_head(struct cirbuf *cbuf, const char *c, unsigned int n)
{
	unsigned int e;

	if (!cbuf || !c || !n || n > CIRBUF_GET_FREELEN(cbuf))
		return -EINVAL;

	e = CIRBUF_IS_EMPTY(cbuf) ? 1 : 0;

	if (n < cbuf->start + e) {
		dprintf("s[%d] -> d[%d] (%d)\n", 0, cbuf->start - n + e, n);
		memcpy(cbuf->buf + cbuf->start - n + e, c, n);
	}
	else {
		dprintf("s[%d] -> d[%d] (%d)\n", + n - (cbuf->start + e), 0,
			cbuf->start + e);
		dprintf("s[%d] -> d[%d] (%d)\n", cbuf->maxlen - n +
			(cbuf->start + e), 0, n - (cbuf->start + e));
		memcpy(cbuf->buf, c  + n - (cbuf->start + e) , cbuf->start + e);
		memcpy(cbuf->buf + cbuf->maxlen - n + (cbuf->start + e), c,
		       n - (cbuf->start + e));
	}
	cbuf->len += n;
	cbuf->start += (cbuf->maxlen - n + e);
	cbuf->start %= cbuf->maxlen;
	return n;
}

/* multiple add */

int
cirbuf_add_buf_tail(struct cirbuf *cbuf, const char *c, unsigned int n)
{
	unsigned int e;

	if (!cbuf || !c || !n || n > CIRBUF_GET_FREELEN(cbuf))
		return -EINVAL;

	e = CIRBUF_IS_EMPTY(cbuf) ? 1 : 0;

	if (n < cbuf->maxlen - cbuf->end - 1 + e) {
		dprintf("s[%d] -> d[%d] (%d)\n", 0, cbuf->end + !e, n);
		memcpy(cbuf->buf + cbuf->end + !e, c, n);
	}
	else {
		dprintf("s[%d] -> d[%d] (%d)\n", cbuf->end + !e, 0,
			cbuf->maxlen - cbuf->end - 1 + e);
		dprintf("s[%d] -> d[%d] (%d)\n", cbuf->maxlen - cbuf->end - 1 +
			e, 0, n - cbuf->maxlen + cbuf->end + 1 - e);
		memcpy(cbuf->buf + cbuf->end + !e, c, cbuf->maxlen -
		       cbuf->end - 1 + e);
		memcpy(cbuf->buf, c + cbuf->maxlen - cbuf->end - 1 + e,
		       n - cbuf->maxlen + cbuf->end + 1 - e);
	}
	cbuf->len += n;
	cbuf->end += n - e;
	cbuf->end %= cbuf->maxlen;
	return n;
}

/* add at head */

static inline void
__cirbuf_add_head(struct cirbuf * cbuf, char c)
{
	if (!CIRBUF_IS_EMPTY(cbuf)) {
		cbuf->start += (cbuf->maxlen - 1);
		cbuf->start %= cbuf->maxlen;
	}
	cbuf->buf[cbuf->start] = c;
	cbuf->len ++;
}

int
cirbuf_add_head_safe(struct cirbuf * cbuf, char c)
{
	if (cbuf && !CIRBUF_IS_FULL(cbuf)) {
		__cirbuf_add_head(cbuf, c);
		return 0;
	}
	return -EINVAL;
}

void
cirbuf_add_head(struct cirbuf * cbuf, char c)
{
	__cirbuf_add_head(cbuf, c);
}

/* add at tail */

static inline void
__cirbuf_add_tail(struct cirbuf * cbuf, char c)
{
	if (!CIRBUF_IS_EMPTY(cbuf)) {
		cbuf->end ++;
		cbuf->end %= cbuf->maxlen;
	}
	cbuf->buf[cbuf->end] = c;
	cbuf->len ++;
}

int
cirbuf_add_tail_safe(struct cirbuf * cbuf, char c)
{
	if (cbuf && !CIRBUF_IS_FULL(cbuf)) {
		__cirbuf_add_tail(cbuf, c);
		return 0;
	}
	return -EINVAL;
}

void
cirbuf_add_tail(struct cirbuf * cbuf, char c)
{
	__cirbuf_add_tail(cbuf, c);
}


static inline void
__cirbuf_shift_left(struct cirbuf *cbuf)
{
	unsigned int i;
	char tmp = cbuf->buf[cbuf->start];

	for (i=0 ; i<cbuf->len ; i++) {
		cbuf->buf[(cbuf->start+i)%cbuf->maxlen] =
			cbuf->buf[(cbuf->start+i+1)%cbuf->maxlen];
	}
	cbuf->buf[(cbuf->start-1+cbuf->maxlen)%cbuf->maxlen] = tmp;
	cbuf->start += (cbuf->maxlen - 1);
	cbuf->start %= cbuf->maxlen;
	cbuf->end += (cbuf->maxlen - 1);
	cbuf->end %= cbuf->maxlen;
}

static inline void
__cirbuf_shift_right(struct cirbuf *cbuf)
{
	unsigned int i;
	char tmp = cbuf->buf[cbuf->end];

	for (i=0 ; i<cbuf->len ; i++) {
		cbuf->buf[(cbuf->end+cbuf->maxlen-i)%cbuf->maxlen] =
			cbuf->buf[(cbuf->end+cbuf->maxlen-i-1)%cbuf->maxlen];
	}
	cbuf->buf[(cbuf->end+1)%cbuf->maxlen] = tmp;
	cbuf->start += 1;
	cbuf->start %= cbuf->maxlen;
	cbuf->end += 1;
	cbuf->end %= cbuf->maxlen;
}

/* XXX we could do a better algorithm here... */
int
cirbuf_align_left(struct cirbuf * cbuf)
{
	if (!cbuf)
		return -EINVAL;

	if (cbuf->start < cbuf->maxlen/2) {
		while (cbuf->start != 0) {
			__cirbuf_shift_left(cbuf);
		}
	}
	else {
		while (cbuf->start != 0) {
			__cirbuf_shift_right(cbuf);
		}
	}

	return 0;
}

/* XXX we could do a better algorithm here... */
int
cirbuf_align_right(struct cirbuf * cbuf)
{
	if (!cbuf)
		return -EINVAL;

	if (cbuf->start >= cbuf->maxlen/2) {
		while (cbuf->end != cbuf->maxlen-1) {
			__cirbuf_shift_left(cbuf);
		}
	}
	else {
		while (cbuf->start != cbuf->maxlen-1) {
			__cirbuf_shift_right(cbuf);
		}
	}

	return 0;
}

/* buffer del */

int
cirbuf_del_buf_head(struct cirbuf *cbuf, unsigned int size)
{
	if (!cbuf || !size || size > CIRBUF_GET_LEN(cbuf))
		return -EINVAL;

	cbuf->len -= size;
	if (CIRBUF_IS_EMPTY(cbuf)) {
		cbuf->start += size - 1;
		cbuf->start %= cbuf->maxlen;
	}
	else {
		cbuf->start += size;
		cbuf->start %= cbuf->maxlen;
	}
	return 0;
}

/* buffer del */

int
cirbuf_del_buf_tail(struct cirbuf *cbuf, unsigned int size)
{
	if (!cbuf || !size || size > CIRBUF_GET_LEN(cbuf))
		return -EINVAL;

	cbuf->len -= size;
	if (CIRBUF_IS_EMPTY(cbuf)) {
		cbuf->end  += (cbuf->maxlen - size + 1);
		cbuf->end %= cbuf->maxlen;
	}
	else {
		cbuf->end  += (cbuf->maxlen - size);
		cbuf->end %= cbuf->maxlen;
	}
	return 0;
}

/* del at head */

static inline void
__cirbuf_del_head(struct cirbuf * cbuf)
{
	cbuf->len --;
	if (!CIRBUF_IS_EMPTY(cbuf)) {
		cbuf->start ++;
		cbuf->start %= cbuf->maxlen;
	}
}

int
cirbuf_del_head_safe(struct cirbuf * cbuf)
{
	if (cbuf && !CIRBUF_IS_EMPTY(cbuf)) {
		__cirbuf_del_head(cbuf);
		return 0;
	}
	return -EINVAL;
}

void
cirbuf_del_head(struct cirbuf * cbuf)
{
	__cirbuf_del_head(cbuf);
}

/* del at tail */

static inline void
__cirbuf_del_tail(struct cirbuf * cbuf)
{
	cbuf->len --;
	if (!CIRBUF_IS_EMPTY(cbuf)) {
		cbuf->end  += (cbuf->maxlen - 1);
		cbuf->end %= cbuf->maxlen;
	}
}

int
cirbuf_del_tail_safe(struct cirbuf * cbuf)
{
	if (cbuf && !CIRBUF_IS_EMPTY(cbuf)) {
		__cirbuf_del_tail(cbuf);
		return 0;
	}
	return -EINVAL;
}

void
cirbuf_del_tail(struct cirbuf * cbuf)
{
	__cirbuf_del_tail(cbuf);
}

/* convert to buffer */

int
cirbuf_get_buf_head(struct cirbuf *cbuf, char *c, unsigned int size)
{
	unsigned int n;

	if (!cbuf || !c)
		return -EINVAL;

	n = (size < CIRBUF_GET_LEN(cbuf)) ? size : CIRBUF_GET_LEN(cbuf);

	if (!n)
		return 0;

	if (cbuf->start <= cbuf->end) {
		dprintf("s[%d] -> d[%d] (%d)\n", cbuf->start, 0, n);
		memcpy(c, cbuf->buf + cbuf->start , n);
	}
	else {
		/* check if we need to go from end to the beginning */
		if (n <= cbuf->maxlen - cbuf->start) {
			dprintf("s[%d] -> d[%d] (%d)\n", 0, cbuf->start, n);
			memcpy(c, cbuf->buf + cbuf->start , n);
		}
		else {
			dprintf("s[%d] -> d[%d] (%d)\n", cbuf->start, 0,
				cbuf->maxlen - cbuf->start);
			dprintf("s[%d] -> d[%d] (%d)\n", 0, cbuf->maxlen - cbuf->start,
				n - cbuf->maxlen + cbuf->start);
			memcpy(c, cbuf->buf + cbuf->start , cbuf->maxlen - cbuf->start);
			memcpy(c + cbuf->maxlen - cbuf->start, cbuf->buf,
				   n - cbuf->maxlen + cbuf->start);
		}
	}
	return n;
}

/* convert to buffer */

int
cirbuf_get_buf_tail(struct cirbuf *cbuf, char *c, unsigned int size)
{
	unsigned int n;

	if (!cbuf || !c)
		return -EINVAL;

	n = (size < CIRBUF_GET_LEN(cbuf)) ? size : CIRBUF_GET_LEN(cbuf);

	if (!n)
		return 0;

	if (cbuf->start <= cbuf->end) {
		dprintf("s[%d] -> d[%d] (%d)\n", cbuf->end - n + 1, 0, n);
		memcpy(c, cbuf->buf + cbuf->end - n + 1, n);
	}
	else {
		/* check if we need to go from end to the beginning */
		if (n <= cbuf->end + 1) {
			dprintf("s[%d] -> d[%d] (%d)\n", 0, cbuf->end - n + 1, n);
			memcpy(c, cbuf->buf + cbuf->end - n + 1, n);
		}
		else {
			dprintf("s[%d] -> d[%d] (%d)\n", 0,
				cbuf->maxlen - cbuf->start, cbuf->end + 1);
			dprintf("s[%d] -> d[%d] (%d)\n",
				cbuf->maxlen - n + cbuf->end + 1, 0, n - cbuf->end - 1);
			memcpy(c + cbuf->maxlen - cbuf->start,
					       cbuf->buf, cbuf->end + 1);
			memcpy(c, cbuf->buf + cbuf->maxlen - n + cbuf->end +1,
				   n - cbuf->end - 1);
		}
	}
	return n;
}

/* get head or get tail */

char
cirbuf_get_head(struct cirbuf * cbuf)
{
	return cbuf->buf[cbuf->start];
}

/* get head or get tail */

char
cirbuf_get_tail(struct cirbuf * cbuf)
{
	return cbuf->buf[cbuf->end];
}
