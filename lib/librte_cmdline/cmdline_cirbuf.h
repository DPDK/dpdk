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

#ifndef _CIRBUF_H_
#define _CIRBUF_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * This structure is the header of a cirbuf type.
 */
struct cirbuf {
	unsigned int maxlen;    /**< total len of the fifo (number of elements) */
	unsigned int start;     /**< indice of the first elt */
	unsigned int end;       /**< indice of the last elt */
	unsigned int len;       /**< current len of fifo */
	char *buf;
};

#ifdef RTE_LIBRTE_CMDLINE_DEBUG
#define dprintf_(fmt, ...) printf("line %3.3d - " fmt "%.0s", __LINE__, __VA_ARGS__)
#define dprintf(...) dprintf_(__VA_ARGS__, "dummy")
#else
#define dprintf(...) (void)0
#endif


/**
 * Init the circular buffer
 */
int cirbuf_init(struct cirbuf *cbuf, char *buf, unsigned int start, unsigned int maxlen);


/**
 * Return 1 if the circular buffer is full
 */
#define CIRBUF_IS_FULL(cirbuf) ((cirbuf)->maxlen == (cirbuf)->len)

/**
 * Return 1 if the circular buffer is empty
 */
#define CIRBUF_IS_EMPTY(cirbuf) ((cirbuf)->len == 0)

/**
 * return current size of the circular buffer (number of used elements)
 */
#define CIRBUF_GET_LEN(cirbuf) ((cirbuf)->len)

/**
 * return size of the circular buffer (used + free elements)
 */
#define CIRBUF_GET_MAXLEN(cirbuf) ((cirbuf)->maxlen)

/**
 * return the number of free elts
 */
#define CIRBUF_GET_FREELEN(cirbuf) ((cirbuf)->maxlen - (cirbuf)->len)

/**
 * Iterator for a circular buffer
 *   c: struct cirbuf pointer
 *   i: an integer type internally used in the macro
 *   e: char that takes the value for each iteration
 */
#define CIRBUF_FOREACH(c, i, e)                                 \
	for ( i=0, e=(c)->buf[(c)->start] ;                     \
		i<((c)->len) ;                                  \
		i ++,  e=(c)->buf[((c)->start+i)%((c)->maxlen)])


/**
 * Add a character at head of the circular buffer. Return 0 on success, or
 * a negative value on error.
 */
int cirbuf_add_head_safe(struct cirbuf *cbuf, char c);

/**
 * Add a character at head of the circular buffer. You _must_ check that you
 * have enough free space in the buffer before calling this func.
 */
void cirbuf_add_head(struct cirbuf *cbuf, char c);

/**
 * Add a character at tail of the circular buffer. Return 0 on success, or
 * a negative value on error.
 */
int cirbuf_add_tail_safe(struct cirbuf *cbuf, char c);

/**
 * Add a character at tail of the circular buffer. You _must_ check that you
 * have enough free space in the buffer before calling this func.
 */
void cirbuf_add_tail(struct cirbuf *cbuf, char c);

/**
 * Remove a char at the head of the circular buffer. Return 0 on
 * success, or a negative value on error.
 */
int cirbuf_del_head_safe(struct cirbuf *cbuf);

/**
 * Remove a char at the head of the circular buffer. You _must_ check
 * that buffer is not empty before calling the function.
 */
void cirbuf_del_head(struct cirbuf *cbuf);

/**
 * Remove a char at the tail of the circular buffer. Return 0 on
 * success, or a negative value on error.
 */
int cirbuf_del_tail_safe(struct cirbuf *cbuf);

/**
 * Remove a char at the tail of the circular buffer. You _must_ check
 * that buffer is not empty before calling the function.
 */
void cirbuf_del_tail(struct cirbuf *cbuf);

/**
 * Return the head of the circular buffer. You _must_ check that
 * buffer is not empty before calling the function.
 */
char cirbuf_get_head(struct cirbuf *cbuf);

/**
 * Return the tail of the circular buffer. You _must_ check that
 * buffer is not empty before calling the function.
 */
char cirbuf_get_tail(struct cirbuf *cbuf);

/**
 * Add a buffer at head of the circular buffer. 'c' is a pointer to a
 * buffer, and n is the number of char to add. Return the number of
 * copied bytes on success, or a negative value on error.
 */
int cirbuf_add_buf_head(struct cirbuf *cbuf, const char *c, unsigned int n);

/**
 * Add a buffer at tail of the circular buffer. 'c' is a pointer to a
 * buffer, and n is the number of char to add. Return the number of
 * copied bytes on success, or a negative value on error.
 */
int cirbuf_add_buf_tail(struct cirbuf *cbuf, const char *c, unsigned int n);

/**
 * Remove chars at the head of the circular buffer. Return 0 on
 * success, or a negative value on error.
 */
int cirbuf_del_buf_head(struct cirbuf *cbuf, unsigned int size);

/**
 * Remove chars at the tail of the circular buffer. Return 0 on
 * success, or a negative value on error.
 */
int cirbuf_del_buf_tail(struct cirbuf *cbuf, unsigned int size);

/**
 * Copy a maximum of 'size' characters from the head of the circular
 * buffer to a flat one pointed by 'c'. Return the number of copied
 * chars.
 */
int cirbuf_get_buf_head(struct cirbuf *cbuf, char *c, unsigned int size);

/**
 * Copy a maximum of 'size' characters from the tail of the circular
 * buffer to a flat one pointed by 'c'. Return the number of copied
 * chars.
 */
int cirbuf_get_buf_tail(struct cirbuf *cbuf, char *c, unsigned int size);


/**
 * Set the start of the data to the index 0 of the internal buffer.
 */
int cirbuf_align_left(struct cirbuf *cbuf);

/**
 * Set the end of the data to the last index of the internal buffer.
 */
int cirbuf_align_right(struct cirbuf *cbuf);

#ifdef __cplusplus
}
#endif

#endif /* _CIRBUF_H_ */
