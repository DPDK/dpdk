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

#ifndef _CMDLINE_PARSE_H_
#define _CMDLINE_PARSE_H_

#ifdef __cplusplus
extern "C" {
#endif

#ifndef offsetof
#define offsetof(type, field)  ((size_t) &( ((type *)0)->field) )
#endif

/* return status for parsing */
#define CMDLINE_PARSE_SUCCESS        0
#define CMDLINE_PARSE_AMBIGUOUS     -1
#define CMDLINE_PARSE_NOMATCH       -2
#define CMDLINE_PARSE_BAD_ARGS      -3

/* return status for completion */
#define CMDLINE_PARSE_COMPLETE_FINISHED 0
#define CMDLINE_PARSE_COMPLETE_AGAIN    1
#define CMDLINE_PARSE_COMPLETED_BUFFER  2

/* maximum buffer size for parsed result */
#define CMDLINE_PARSE_RESULT_BUFSIZE 8192

/* maximum number of dynamic tokens */
#define CMDLINE_PARSE_DYNAMIC_TOKENS 128

/**
 * Stores a pointer to the ops struct, and the offset: the place to
 * write the parsed result in the destination structure.
 */
struct cmdline_token_hdr {
	struct cmdline_token_ops *ops;
	unsigned int offset;
};
typedef struct cmdline_token_hdr cmdline_parse_token_hdr_t;

/**
 * A token is defined by this structure.
 *
 * parse() takes the token as first argument, then the source buffer
 * starting at the token we want to parse. The 3rd arg is a pointer
 * where we store the parsed data (as binary). It returns the number of
 * parsed chars on success and a negative value on error.
 *
 * complete_get_nb() returns the number of possible values for this
 * token if completion is possible. If it is NULL or if it returns 0,
 * no completion is possible.
 *
 * complete_get_elt() copy in dstbuf (the size is specified in the
 * parameter) the i-th possible completion for this token.  returns 0
 * on success or and a negative value on error.
 *
 * get_help() fills the dstbuf with the help for the token. It returns
 * -1 on error and 0 on success.
 */
struct cmdline_token_ops {
	/** parse(token ptr, buf, res pts, buf len) */
	int (*parse)(cmdline_parse_token_hdr_t *, const char *, void *,
		unsigned int);
	/** return the num of possible choices for this token */
	int (*complete_get_nb)(cmdline_parse_token_hdr_t *);
	/** return the elt x for this token (token, idx, dstbuf, size) */
	int (*complete_get_elt)(cmdline_parse_token_hdr_t *, int, char *,
		unsigned int);
	/** get help for this token (token, dstbuf, size) */
	int (*get_help)(cmdline_parse_token_hdr_t *, char *, unsigned int);
};

struct cmdline;
/**
 * Store a instruction, which is a pointer to a callback function and
 * its parameter that is called when the instruction is parsed, a help
 * string, and a list of token composing this instruction.
 *
 * When no tokens are defined (tokens[0] == NULL), they are retrieved
 * dynamically by calling f() as follows:
 *
 *  f((struct cmdline_token_hdr **)&token_hdr,
 *    NULL,
 *    (struct cmdline_token_hdr *[])tokens));
 *
 * The address of the resulting token is expected at the location pointed by
 * the first argument. Can be set to NULL to end the list.
 *
 * The cmdline argument (struct cmdline *) is always NULL.
 *
 * The last argument points to the NULL-terminated list of dynamic tokens
 * defined so far. Since token_hdr points to an index of that list, the
 * current index can be derived as follows:
 *
 *  int index = token_hdr - &(*tokens)[0];
 */
struct cmdline_inst {
	/* f(parsed_struct, data) */
	void (*f)(void *, struct cmdline *, void *);
	void *data;
	const char *help_str;
	cmdline_parse_token_hdr_t *tokens[];
};
typedef struct cmdline_inst cmdline_parse_inst_t;

/**
 * A context is identified by its name, and contains a list of
 * instruction
 *
 */
typedef cmdline_parse_inst_t *cmdline_parse_ctx_t;

/**
 * Try to parse a buffer according to the specified context. The
 * argument buf must ends with "\n\0". The function returns
 * CMDLINE_PARSE_AMBIGUOUS, CMDLINE_PARSE_NOMATCH or
 * CMDLINE_PARSE_BAD_ARGS on error. Else it calls the associated
 * function (defined in the context) and returns 0
 * (CMDLINE_PARSE_SUCCESS).
 */
int cmdline_parse(struct cmdline *cl, const char *buf);

/**
 * complete() must be called with *state==0 (try to complete) or
 * with *state==-1 (just display choices), then called without
 * modifying *state until it returns CMDLINE_PARSE_COMPLETED_BUFFER or
 * CMDLINE_PARSE_COMPLETED_BUFFER.
 *
 * It returns < 0 on error.
 *
 * Else it returns:
 *   - CMDLINE_PARSE_COMPLETED_BUFFER on completion (one possible
 *     choice). In this case, the chars are appended in dst buffer.
 *   - CMDLINE_PARSE_COMPLETE_AGAIN if there is several possible
 *     choices. In this case, you must call the function again,
 *     keeping the value of state intact.
 *   - CMDLINE_PARSE_COMPLETED_BUFFER when the iteration is
 *     finished. The dst is not valid for this last call.
 *
 * The returned dst buf ends with \0.
 */
int cmdline_complete(struct cmdline *cl, const char *buf, int *state,
		     char *dst, unsigned int size);


/* return true if(!c || iscomment(c) || isblank(c) ||
 * isendofline(c)) */
int cmdline_isendoftoken(char c);

/* return true if(!c || iscomment(c) || isendofline(c)) */
int cmdline_isendofcommand(char c);

#ifdef __cplusplus
}
#endif

#endif /* _CMDLINE_PARSE_H_ */
