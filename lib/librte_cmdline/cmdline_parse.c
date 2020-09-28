/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation.
 * Copyright (c) 2009, Olivier MATZ <zer0@droids-corp.org>
 * All rights reserved.
 */

#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <inttypes.h>
#include <ctype.h>

#include <netinet/in.h>

#include <rte_string_fns.h>

#include "cmdline_private.h"

#ifdef RTE_LIBRTE_CMDLINE_DEBUG
#define debug_printf printf
#else
#define debug_printf(args...) do {} while(0)
#endif

#define CMDLINE_BUFFER_SIZE 64

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

static int
isendofline(char c)
{
	if (c == '\n' ||
	    c == '\r' )
		return 1;
	return 0;
}

static int
iscomment(char c)
{
	if (c == '#')
		return 1;
	return 0;
}

int
cmdline_isendoftoken(char c)
{
	if (!c || iscomment(c) || isblank2(c) || isendofline(c))
		return 1;
	return 0;
}

int
cmdline_isendofcommand(char c)
{
	if (!c || iscomment(c) || isendofline(c))
		return 1;
	return 0;
}

static unsigned int
nb_common_chars(const char * s1, const char * s2)
{
	unsigned int i=0;

	while (*s1==*s2 && *s1) {
		s1++;
		s2++;
		i++;
	}
	return i;
}

/** Retrieve either static or dynamic token at a given index. */
static cmdline_parse_token_hdr_t *
get_token(cmdline_parse_inst_t *inst, unsigned int index)
{
	cmdline_parse_token_hdr_t *token_p;

	/* check presence of static tokens first */
	if (inst->tokens[0] || !inst->f)
		return inst->tokens[index];
	/* generate dynamic token */
	token_p = NULL;
	inst->f(&token_p, NULL, &inst->tokens[index]);
	return token_p;
}

/**
 * try to match the buffer with an instruction (only the first
 * nb_match_token tokens if != 0). Return 0 if we match all the
 * tokens, else the number of matched tokens, else -1.
 */
static int
match_inst(cmdline_parse_inst_t *inst, const char *buf,
	   unsigned int nb_match_token, void *resbuf, unsigned resbuf_size)
{
	cmdline_parse_token_hdr_t *token_p = NULL;
	unsigned int i=0;
	int n = 0;
	struct cmdline_token_hdr token_hdr;

	if (resbuf != NULL)
		memset(resbuf, 0, resbuf_size);
	/* check if we match all tokens of inst */
	while (!nb_match_token || i < nb_match_token) {
		token_p = get_token(inst, i);
		if (!token_p)
			break;
		memcpy(&token_hdr, token_p, sizeof(token_hdr));

		debug_printf("TK\n");
		/* skip spaces */
		while (isblank2(*buf)) {
			buf++;
		}

		/* end of buf */
		if ( isendofline(*buf) || iscomment(*buf) )
			break;

		if (resbuf == NULL) {
			n = token_hdr.ops->parse(token_p, buf, NULL, 0);
		} else {
			unsigned rb_sz;

			if (token_hdr.offset > resbuf_size) {
				printf("Parse error(%s:%d): Token offset(%u) "
					"exceeds maximum size(%u)\n",
					__FILE__, __LINE__,
					token_hdr.offset, resbuf_size);
				return -ENOBUFS;
			}
			rb_sz = resbuf_size - token_hdr.offset;

			n = token_hdr.ops->parse(token_p, buf, (char *)resbuf +
				token_hdr.offset, rb_sz);
		}

		if (n < 0)
			break;

		debug_printf("TK parsed (len=%d)\n", n);
		i++;
		buf += n;
	}

	/* does not match */
	if (i==0)
		return -1;

	/* in case we want to match a specific num of token */
	if (nb_match_token) {
		if (i == nb_match_token) {
			return 0;
		}
		return i;
	}

	/* we don't match all the tokens */
	if (token_p) {
		return i;
	}

	/* are there are some tokens more */
	while (isblank2(*buf)) {
		buf++;
	}

	/* end of buf */
	if ( isendofline(*buf) || iscomment(*buf) )
		return 0;

	/* garbage after inst */
	return i;
}


int
cmdline_parse(struct cmdline *cl, const char * buf)
{
	unsigned int inst_num=0;
	cmdline_parse_inst_t *inst;
	const char *curbuf;
	union {
		char buf[CMDLINE_PARSE_RESULT_BUFSIZE];
		long double align; /* strong alignment constraint for buf */
	} result, tmp_result;
	void (*f)(void *, struct cmdline *, void *) = NULL;
	void *data = NULL;
	int comment = 0;
	int linelen = 0;
	int parse_it = 0;
	int err = CMDLINE_PARSE_NOMATCH;
	int tok;
	cmdline_parse_ctx_t *ctx;
	char *result_buf = result.buf;

	if (!cl || !buf)
		return CMDLINE_PARSE_BAD_ARGS;

	ctx = cl->ctx;

	/*
	 * - look if the buffer contains at least one line
	 * - look if line contains only spaces or comments
	 * - count line length
	 */
	curbuf = buf;
	while (! isendofline(*curbuf)) {
		if ( *curbuf == '\0' ) {
			debug_printf("Incomplete buf (len=%d)\n", linelen);
			return 0;
		}
		if ( iscomment(*curbuf) ) {
			comment = 1;
		}
		if ( ! isblank2(*curbuf) && ! comment) {
			parse_it = 1;
		}
		curbuf++;
		linelen++;
	}

	/* skip all endofline chars */
	while (isendofline(buf[linelen])) {
		linelen++;
	}

	/* empty line */
	if ( parse_it == 0 ) {
		debug_printf("Empty line (len=%d)\n", linelen);
		return linelen;
	}

	debug_printf("Parse line : len=%d, <%.*s>\n",
		     linelen, linelen > 64 ? 64 : linelen, buf);

	/* parse it !! */
	inst = ctx[inst_num];
	while (inst) {
		debug_printf("INST %d\n", inst_num);

		/* fully parsed */
		tok = match_inst(inst, buf, 0, result_buf,
				 CMDLINE_PARSE_RESULT_BUFSIZE);

		if (tok > 0) /* we matched at least one token */
			err = CMDLINE_PARSE_BAD_ARGS;

		else if (!tok) {
			debug_printf("INST fully parsed\n");
			/* skip spaces */
			while (isblank2(*curbuf)) {
				curbuf++;
			}

			/* if end of buf -> there is no garbage after inst */
			if (isendofline(*curbuf) || iscomment(*curbuf)) {
				if (!f) {
					memcpy(&f, &inst->f, sizeof(f));
					memcpy(&data, &inst->data, sizeof(data));
					result_buf = tmp_result.buf;
				}
				else {
					/* more than 1 inst matches */
					err = CMDLINE_PARSE_AMBIGUOUS;
					f=NULL;
					debug_printf("Ambiguous cmd\n");
					break;
				}
			}
		}

		inst_num ++;
		inst = ctx[inst_num];
	}

	/* call func */
	if (f) {
		f(result.buf, cl, data);
	}

	/* no match */
	else {
		debug_printf("No match err=%d\n", err);
		return err;
	}

	return linelen;
}

int
cmdline_complete(struct cmdline *cl, const char *buf, int *state,
		 char *dst, unsigned int size)
{
	const char *partial_tok = buf;
	unsigned int inst_num = 0;
	cmdline_parse_inst_t *inst;
	cmdline_parse_token_hdr_t *token_p;
	struct cmdline_token_hdr token_hdr;
	char tmpbuf[CMDLINE_BUFFER_SIZE], comp_buf[CMDLINE_BUFFER_SIZE];
	unsigned int partial_tok_len;
	int comp_len = -1;
	int tmp_len = -1;
	int nb_token = 0;
	unsigned int i, n;
	int l;
	unsigned int nb_completable;
	unsigned int nb_non_completable;
	int local_state = 0;
	const char *help_str;
	cmdline_parse_ctx_t *ctx;

	if (!cl || !buf || !state || !dst)
		return -1;

	ctx = cl->ctx;

	debug_printf("%s called\n", __func__);
	memset(&token_hdr, 0, sizeof(token_hdr));

	/* count the number of complete token to parse */
	for (i=0 ; buf[i] ; i++) {
		if (!isblank2(buf[i]) && isblank2(buf[i+1]))
			nb_token++;
		if (isblank2(buf[i]) && !isblank2(buf[i+1]))
			partial_tok = buf+i+1;
	}
	partial_tok_len = strnlen(partial_tok, RDLINE_BUF_SIZE);

	/* first call -> do a first pass */
	if (*state <= 0) {
		debug_printf("try complete <%s>\n", buf);
		debug_printf("there is %d complete tokens, <%s> is incomplete\n",
			     nb_token, partial_tok);

		nb_completable = 0;
		nb_non_completable = 0;

		inst = ctx[inst_num];
		while (inst) {
			/* parse the first tokens of the inst */
			if (nb_token &&
			    match_inst(inst, buf, nb_token, NULL, 0))
				goto next;

			debug_printf("instruction match\n");
			token_p = get_token(inst, nb_token);
			if (token_p)
				memcpy(&token_hdr, token_p, sizeof(token_hdr));

			/* non completable */
			if (!token_p ||
			    !token_hdr.ops->complete_get_nb ||
			    !token_hdr.ops->complete_get_elt ||
			    (n = token_hdr.ops->complete_get_nb(token_p)) == 0) {
				nb_non_completable++;
				goto next;
			}

			debug_printf("%d choices for this token\n", n);
			for (i=0 ; i<n ; i++) {
				if (token_hdr.ops->complete_get_elt(token_p, i,
								    tmpbuf,
								    sizeof(tmpbuf)) < 0)
					continue;

				/* we have at least room for one char */
				tmp_len = strnlen(tmpbuf, sizeof(tmpbuf));
				if (tmp_len < CMDLINE_BUFFER_SIZE - 1) {
					tmpbuf[tmp_len] = ' ';
					tmpbuf[tmp_len+1] = 0;
				}

				debug_printf("   choice <%s>\n", tmpbuf);

				/* does the completion match the
				 * beginning of the word ? */
				if (!strncmp(partial_tok, tmpbuf,
					     partial_tok_len)) {
					if (comp_len == -1) {
						strlcpy(comp_buf,
							tmpbuf + partial_tok_len,
							sizeof(comp_buf));
						comp_len =
							strnlen(tmpbuf + partial_tok_len,
									sizeof(tmpbuf) - partial_tok_len);

					}
					else {
						comp_len =
							nb_common_chars(comp_buf,
									tmpbuf+partial_tok_len);
						comp_buf[comp_len] = 0;
					}
					nb_completable++;
				}
			}
		next:
			debug_printf("next\n");
			inst_num ++;
			inst = ctx[inst_num];
		}

		debug_printf("total choices %d for this completion\n",
			     nb_completable);

		/* no possible completion */
		if (nb_completable == 0 && nb_non_completable == 0)
			return 0;

		/* if multichoice is not required */
		if (*state == 0 && partial_tok_len > 0) {
			/* one or several choices starting with the
			   same chars */
			if (comp_len > 0) {
				if ((unsigned)(comp_len + 1) > size)
					return 0;

				strlcpy(dst, comp_buf, size);
				dst[comp_len] = 0;
				return 2;
			}
		}
	}

	/* init state correctly */
	if (*state == -1)
		*state = 0;

	debug_printf("Multiple choice STATE=%d\n", *state);

	inst_num = 0;
	inst = ctx[inst_num];
	while (inst) {
		/* we need to redo it */
		inst = ctx[inst_num];

		if (nb_token &&
		    match_inst(inst, buf, nb_token, NULL, 0))
			goto next2;

		token_p = get_token(inst, nb_token);
		if (token_p)
			memcpy(&token_hdr, token_p, sizeof(token_hdr));

		/* one choice for this token */
		if (!token_p ||
		    !token_hdr.ops->complete_get_nb ||
		    !token_hdr.ops->complete_get_elt ||
		    (n = token_hdr.ops->complete_get_nb(token_p)) == 0) {
			if (local_state < *state) {
				local_state++;
				goto next2;
			}
			(*state)++;
			if (token_p && token_hdr.ops->get_help) {
				token_hdr.ops->get_help(token_p, tmpbuf,
							sizeof(tmpbuf));
				help_str = inst->help_str;
				if (help_str)
					snprintf(dst, size, "[%s]: %s", tmpbuf,
						 help_str);
				else
					snprintf(dst, size, "[%s]: No help",
						 tmpbuf);
			}
			else {
				snprintf(dst, size, "[RETURN]");
			}
			return 1;
		}

		/* several choices */
		for (i=0 ; i<n ; i++) {
			if (token_hdr.ops->complete_get_elt(token_p, i, tmpbuf,
							    sizeof(tmpbuf)) < 0)
				continue;
			/* we have at least room for one char */
			tmp_len = strnlen(tmpbuf, sizeof(tmpbuf));
			if (tmp_len < CMDLINE_BUFFER_SIZE - 1) {
				tmpbuf[tmp_len] = ' ';
				tmpbuf[tmp_len + 1] = 0;
			}

			debug_printf("   choice <%s>\n", tmpbuf);

			/* does the completion match the beginning of
			 * the word ? */
			if (!strncmp(partial_tok, tmpbuf,
				     partial_tok_len)) {
				if (local_state < *state) {
					local_state++;
					continue;
				}
				(*state)++;
				l=strlcpy(dst, tmpbuf, size);
				if (l>=0 && token_hdr.ops->get_help) {
					token_hdr.ops->get_help(token_p, tmpbuf,
								sizeof(tmpbuf));
					help_str = inst->help_str;
					if (help_str)
						snprintf(dst+l, size-l, "[%s]: %s",
							 tmpbuf, help_str);
					else
						snprintf(dst+l, size-l,
							 "[%s]: No help", tmpbuf);
				}

				return 1;
			}
		}
	next2:
		inst_num ++;
		inst = ctx[inst_num];
	}
	return 0;
}
