/*-
 *   BSD LICENSE
 *
 *   Copyright 2016 6WIND S.A.
 *   Copyright 2016 Mellanox.
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
 *     * Neither the name of 6WIND S.A. nor the names of its
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

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <inttypes.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>

#include <rte_common.h>
#include <rte_ethdev.h>
#include <rte_byteorder.h>
#include <cmdline_parse.h>
#include <rte_flow.h>

#include "testpmd.h"

/** Parser token indices. */
enum index {
	/* Special tokens. */
	ZERO = 0,
	END,

	/* Common tokens. */
	INTEGER,
	UNSIGNED,
	PORT_ID,
	GROUP_ID,

	/* Top-level command. */
	FLOW,

	/* Sub-level commands. */
	LIST,

	/* List arguments. */
	LIST_GROUP,
};

/** Maximum number of subsequent tokens and arguments on the stack. */
#define CTX_STACK_SIZE 16

/** Parser context. */
struct context {
	/** Stack of subsequent token lists to process. */
	const enum index *next[CTX_STACK_SIZE];
	/** Arguments for stacked tokens. */
	const void *args[CTX_STACK_SIZE];
	enum index curr; /**< Current token index. */
	enum index prev; /**< Index of the last token seen. */
	int next_num; /**< Number of entries in next[]. */
	int args_num; /**< Number of entries in args[]. */
	uint32_t reparse:1; /**< Start over from the beginning. */
	uint32_t eol:1; /**< EOL has been detected. */
	uint32_t last:1; /**< No more arguments. */
	uint16_t port; /**< Current port ID (for completions). */
	void *object; /**< Address of current object for relative offsets. */
};

/** Token argument. */
struct arg {
	uint32_t hton:1; /**< Use network byte ordering. */
	uint32_t sign:1; /**< Value is signed. */
	uint32_t offset; /**< Relative offset from ctx->object. */
	uint32_t size; /**< Field size. */
};

/** Parser token definition. */
struct token {
	/** Type displayed during completion (defaults to "TOKEN"). */
	const char *type;
	/** Help displayed during completion (defaults to token name). */
	const char *help;
	/**
	 * Lists of subsequent tokens to push on the stack. Each call to the
	 * parser consumes the last entry of that stack.
	 */
	const enum index *const *next;
	/** Arguments stack for subsequent tokens that need them. */
	const struct arg *const *args;
	/**
	 * Token-processing callback, returns -1 in case of error, the
	 * length of the matched string otherwise. If NULL, attempts to
	 * match the token name.
	 *
	 * If buf is not NULL, the result should be stored in it according
	 * to context. An error is returned if not large enough.
	 */
	int (*call)(struct context *ctx, const struct token *token,
		    const char *str, unsigned int len,
		    void *buf, unsigned int size);
	/**
	 * Callback that provides possible values for this token, used for
	 * completion. Returns -1 in case of error, the number of possible
	 * values otherwise. If NULL, the token name is used.
	 *
	 * If buf is not NULL, entry index ent is written to buf and the
	 * full length of the entry is returned (same behavior as
	 * snprintf()).
	 */
	int (*comp)(struct context *ctx, const struct token *token,
		    unsigned int ent, char *buf, unsigned int size);
	/** Mandatory token name, no default value. */
	const char *name;
};

/** Static initializer for the next field. */
#define NEXT(...) (const enum index *const []){ __VA_ARGS__, NULL, }

/** Static initializer for a NEXT() entry. */
#define NEXT_ENTRY(...) (const enum index []){ __VA_ARGS__, ZERO, }

/** Static initializer for the args field. */
#define ARGS(...) (const struct arg *const []){ __VA_ARGS__, NULL, }

/** Static initializer for ARGS() to target a field. */
#define ARGS_ENTRY(s, f) \
	(&(const struct arg){ \
		.offset = offsetof(s, f), \
		.size = sizeof(((s *)0)->f), \
	})

/** Static initializer for ARGS() to target a pointer. */
#define ARGS_ENTRY_PTR(s, f) \
	(&(const struct arg){ \
		.size = sizeof(*((s *)0)->f), \
	})

/** Parser output buffer layout expected by cmd_flow_parsed(). */
struct buffer {
	enum index command; /**< Flow command. */
	uint16_t port; /**< Affected port ID. */
	union {
		struct {
			uint32_t *group;
			uint32_t group_n;
		} list; /**< List arguments. */
	} args; /**< Command arguments. */
};

static const enum index next_list_attr[] = {
	LIST_GROUP,
	END,
	ZERO,
};

static int parse_init(struct context *, const struct token *,
		      const char *, unsigned int,
		      void *, unsigned int);
static int parse_list(struct context *, const struct token *,
		      const char *, unsigned int,
		      void *, unsigned int);
static int parse_int(struct context *, const struct token *,
		     const char *, unsigned int,
		     void *, unsigned int);
static int parse_port(struct context *, const struct token *,
		      const char *, unsigned int,
		      void *, unsigned int);
static int comp_none(struct context *, const struct token *,
		     unsigned int, char *, unsigned int);
static int comp_port(struct context *, const struct token *,
		     unsigned int, char *, unsigned int);

/** Token definitions. */
static const struct token token_list[] = {
	/* Special tokens. */
	[ZERO] = {
		.name = "ZERO",
		.help = "null entry, abused as the entry point",
		.next = NEXT(NEXT_ENTRY(FLOW)),
	},
	[END] = {
		.name = "",
		.type = "RETURN",
		.help = "command may end here",
	},
	/* Common tokens. */
	[INTEGER] = {
		.name = "{int}",
		.type = "INTEGER",
		.help = "integer value",
		.call = parse_int,
		.comp = comp_none,
	},
	[UNSIGNED] = {
		.name = "{unsigned}",
		.type = "UNSIGNED",
		.help = "unsigned integer value",
		.call = parse_int,
		.comp = comp_none,
	},
	[PORT_ID] = {
		.name = "{port_id}",
		.type = "PORT ID",
		.help = "port identifier",
		.call = parse_port,
		.comp = comp_port,
	},
	[GROUP_ID] = {
		.name = "{group_id}",
		.type = "GROUP ID",
		.help = "group identifier",
		.call = parse_int,
		.comp = comp_none,
	},
	/* Top-level command. */
	[FLOW] = {
		.name = "flow",
		.type = "{command} {port_id} [{arg} [...]]",
		.help = "manage ingress/egress flow rules",
		.next = NEXT(NEXT_ENTRY(LIST)),
		.call = parse_init,
	},
	/* Sub-level commands. */
	[LIST] = {
		.name = "list",
		.help = "list existing flow rules",
		.next = NEXT(next_list_attr, NEXT_ENTRY(PORT_ID)),
		.args = ARGS(ARGS_ENTRY(struct buffer, port)),
		.call = parse_list,
	},
	/* List arguments. */
	[LIST_GROUP] = {
		.name = "group",
		.help = "specify a group",
		.next = NEXT(next_list_attr, NEXT_ENTRY(GROUP_ID)),
		.args = ARGS(ARGS_ENTRY_PTR(struct buffer, args.list.group)),
		.call = parse_list,
	},
};

/** Remove and return last entry from argument stack. */
static const struct arg *
pop_args(struct context *ctx)
{
	return ctx->args_num ? ctx->args[--ctx->args_num] : NULL;
}

/** Add entry on top of the argument stack. */
static int
push_args(struct context *ctx, const struct arg *arg)
{
	if (ctx->args_num == CTX_STACK_SIZE)
		return -1;
	ctx->args[ctx->args_num++] = arg;
	return 0;
}

/** Default parsing function for token name matching. */
static int
parse_default(struct context *ctx, const struct token *token,
	      const char *str, unsigned int len,
	      void *buf, unsigned int size)
{
	(void)ctx;
	(void)buf;
	(void)size;
	if (strncmp(str, token->name, len))
		return -1;
	return len;
}

/** Parse flow command, initialize output buffer for subsequent tokens. */
static int
parse_init(struct context *ctx, const struct token *token,
	   const char *str, unsigned int len,
	   void *buf, unsigned int size)
{
	struct buffer *out = buf;

	/* Token name must match. */
	if (parse_default(ctx, token, str, len, NULL, 0) < 0)
		return -1;
	/* Nothing else to do if there is no buffer. */
	if (!out)
		return len;
	/* Make sure buffer is large enough. */
	if (size < sizeof(*out))
		return -1;
	/* Initialize buffer. */
	memset(out, 0x00, sizeof(*out));
	memset((uint8_t *)out + sizeof(*out), 0x22, size - sizeof(*out));
	ctx->object = out;
	return len;
}

/** Parse tokens for list command. */
static int
parse_list(struct context *ctx, const struct token *token,
	   const char *str, unsigned int len,
	   void *buf, unsigned int size)
{
	struct buffer *out = buf;

	/* Token name must match. */
	if (parse_default(ctx, token, str, len, NULL, 0) < 0)
		return -1;
	/* Nothing else to do if there is no buffer. */
	if (!out)
		return len;
	if (!out->command) {
		if (ctx->curr != LIST)
			return -1;
		if (sizeof(*out) > size)
			return -1;
		out->command = ctx->curr;
		ctx->object = out;
		out->args.list.group =
			(void *)RTE_ALIGN_CEIL((uintptr_t)(out + 1),
					       sizeof(double));
		return len;
	}
	if (((uint8_t *)(out->args.list.group + out->args.list.group_n) +
	     sizeof(*out->args.list.group)) > (uint8_t *)out + size)
		return -1;
	ctx->object = out->args.list.group + out->args.list.group_n++;
	return len;
}

/**
 * Parse signed/unsigned integers 8 to 64-bit long.
 *
 * Last argument (ctx->args) is retrieved to determine integer type and
 * storage location.
 */
static int
parse_int(struct context *ctx, const struct token *token,
	  const char *str, unsigned int len,
	  void *buf, unsigned int size)
{
	const struct arg *arg = pop_args(ctx);
	uintmax_t u;
	char *end;

	(void)token;
	/* Argument is expected. */
	if (!arg)
		return -1;
	errno = 0;
	u = arg->sign ?
		(uintmax_t)strtoimax(str, &end, 0) :
		strtoumax(str, &end, 0);
	if (errno || (size_t)(end - str) != len)
		goto error;
	if (!ctx->object)
		return len;
	buf = (uint8_t *)ctx->object + arg->offset;
	size = arg->size;
	switch (size) {
	case sizeof(uint8_t):
		*(uint8_t *)buf = u;
		break;
	case sizeof(uint16_t):
		*(uint16_t *)buf = arg->hton ? rte_cpu_to_be_16(u) : u;
		break;
	case sizeof(uint32_t):
		*(uint32_t *)buf = arg->hton ? rte_cpu_to_be_32(u) : u;
		break;
	case sizeof(uint64_t):
		*(uint64_t *)buf = arg->hton ? rte_cpu_to_be_64(u) : u;
		break;
	default:
		goto error;
	}
	return len;
error:
	push_args(ctx, arg);
	return -1;
}

/** Parse port and update context. */
static int
parse_port(struct context *ctx, const struct token *token,
	   const char *str, unsigned int len,
	   void *buf, unsigned int size)
{
	struct buffer *out = &(struct buffer){ .port = 0 };
	int ret;

	if (buf)
		out = buf;
	else {
		ctx->object = out;
		size = sizeof(*out);
	}
	ret = parse_int(ctx, token, str, len, out, size);
	if (ret >= 0)
		ctx->port = out->port;
	if (!buf)
		ctx->object = NULL;
	return ret;
}

/** No completion. */
static int
comp_none(struct context *ctx, const struct token *token,
	  unsigned int ent, char *buf, unsigned int size)
{
	(void)ctx;
	(void)token;
	(void)ent;
	(void)buf;
	(void)size;
	return 0;
}

/** Complete available ports. */
static int
comp_port(struct context *ctx, const struct token *token,
	  unsigned int ent, char *buf, unsigned int size)
{
	unsigned int i = 0;
	portid_t p;

	(void)ctx;
	(void)token;
	FOREACH_PORT(p, ports) {
		if (buf && i == ent)
			return snprintf(buf, size, "%u", p);
		++i;
	}
	if (buf)
		return -1;
	return i;
}

/** Internal context. */
static struct context cmd_flow_context;

/** Global parser instance (cmdline API). */
cmdline_parse_inst_t cmd_flow;

/** Initialize context. */
static void
cmd_flow_context_init(struct context *ctx)
{
	/* A full memset() is not necessary. */
	ctx->curr = ZERO;
	ctx->prev = ZERO;
	ctx->next_num = 0;
	ctx->args_num = 0;
	ctx->reparse = 0;
	ctx->eol = 0;
	ctx->last = 0;
	ctx->port = 0;
	ctx->object = NULL;
}

/** Parse a token (cmdline API). */
static int
cmd_flow_parse(cmdline_parse_token_hdr_t *hdr, const char *src, void *result,
	       unsigned int size)
{
	struct context *ctx = &cmd_flow_context;
	const struct token *token;
	const enum index *list;
	int len;
	int i;

	(void)hdr;
	/* Restart as requested. */
	if (ctx->reparse)
		cmd_flow_context_init(ctx);
	token = &token_list[ctx->curr];
	/* Check argument length. */
	ctx->eol = 0;
	ctx->last = 1;
	for (len = 0; src[len]; ++len)
		if (src[len] == '#' || isspace(src[len]))
			break;
	if (!len)
		return -1;
	/* Last argument and EOL detection. */
	for (i = len; src[i]; ++i)
		if (src[i] == '#' || src[i] == '\r' || src[i] == '\n')
			break;
		else if (!isspace(src[i])) {
			ctx->last = 0;
			break;
		}
	for (; src[i]; ++i)
		if (src[i] == '\r' || src[i] == '\n') {
			ctx->eol = 1;
			break;
		}
	/* Initialize context if necessary. */
	if (!ctx->next_num) {
		if (!token->next)
			return 0;
		ctx->next[ctx->next_num++] = token->next[0];
	}
	/* Process argument through candidates. */
	ctx->prev = ctx->curr;
	list = ctx->next[ctx->next_num - 1];
	for (i = 0; list[i]; ++i) {
		const struct token *next = &token_list[list[i]];
		int tmp;

		ctx->curr = list[i];
		if (next->call)
			tmp = next->call(ctx, next, src, len, result, size);
		else
			tmp = parse_default(ctx, next, src, len, result, size);
		if (tmp == -1 || tmp != len)
			continue;
		token = next;
		break;
	}
	if (!list[i])
		return -1;
	--ctx->next_num;
	/* Push subsequent tokens if any. */
	if (token->next)
		for (i = 0; token->next[i]; ++i) {
			if (ctx->next_num == RTE_DIM(ctx->next))
				return -1;
			ctx->next[ctx->next_num++] = token->next[i];
		}
	/* Push arguments if any. */
	if (token->args)
		for (i = 0; token->args[i]; ++i) {
			if (ctx->args_num == RTE_DIM(ctx->args))
				return -1;
			ctx->args[ctx->args_num++] = token->args[i];
		}
	return len;
}

/** Return number of completion entries (cmdline API). */
static int
cmd_flow_complete_get_nb(cmdline_parse_token_hdr_t *hdr)
{
	struct context *ctx = &cmd_flow_context;
	const struct token *token = &token_list[ctx->curr];
	const enum index *list;
	int i;

	(void)hdr;
	/* Tell cmd_flow_parse() that context must be reinitialized. */
	ctx->reparse = 1;
	/* Count number of tokens in current list. */
	if (ctx->next_num)
		list = ctx->next[ctx->next_num - 1];
	else
		list = token->next[0];
	for (i = 0; list[i]; ++i)
		;
	if (!i)
		return 0;
	/*
	 * If there is a single token, use its completion callback, otherwise
	 * return the number of entries.
	 */
	token = &token_list[list[0]];
	if (i == 1 && token->comp) {
		/* Save index for cmd_flow_get_help(). */
		ctx->prev = list[0];
		return token->comp(ctx, token, 0, NULL, 0);
	}
	return i;
}

/** Return a completion entry (cmdline API). */
static int
cmd_flow_complete_get_elt(cmdline_parse_token_hdr_t *hdr, int index,
			  char *dst, unsigned int size)
{
	struct context *ctx = &cmd_flow_context;
	const struct token *token = &token_list[ctx->curr];
	const enum index *list;
	int i;

	(void)hdr;
	/* Tell cmd_flow_parse() that context must be reinitialized. */
	ctx->reparse = 1;
	/* Count number of tokens in current list. */
	if (ctx->next_num)
		list = ctx->next[ctx->next_num - 1];
	else
		list = token->next[0];
	for (i = 0; list[i]; ++i)
		;
	if (!i)
		return -1;
	/* If there is a single token, use its completion callback. */
	token = &token_list[list[0]];
	if (i == 1 && token->comp) {
		/* Save index for cmd_flow_get_help(). */
		ctx->prev = list[0];
		return token->comp(ctx, token, index, dst, size) < 0 ? -1 : 0;
	}
	/* Otherwise make sure the index is valid and use defaults. */
	if (index >= i)
		return -1;
	token = &token_list[list[index]];
	snprintf(dst, size, "%s", token->name);
	/* Save index for cmd_flow_get_help(). */
	ctx->prev = list[index];
	return 0;
}

/** Populate help strings for current token (cmdline API). */
static int
cmd_flow_get_help(cmdline_parse_token_hdr_t *hdr, char *dst, unsigned int size)
{
	struct context *ctx = &cmd_flow_context;
	const struct token *token = &token_list[ctx->prev];

	(void)hdr;
	/* Tell cmd_flow_parse() that context must be reinitialized. */
	ctx->reparse = 1;
	if (!size)
		return -1;
	/* Set token type and update global help with details. */
	snprintf(dst, size, "%s", (token->type ? token->type : "TOKEN"));
	if (token->help)
		cmd_flow.help_str = token->help;
	else
		cmd_flow.help_str = token->name;
	return 0;
}

/** Token definition template (cmdline API). */
static struct cmdline_token_hdr cmd_flow_token_hdr = {
	.ops = &(struct cmdline_token_ops){
		.parse = cmd_flow_parse,
		.complete_get_nb = cmd_flow_complete_get_nb,
		.complete_get_elt = cmd_flow_complete_get_elt,
		.get_help = cmd_flow_get_help,
	},
	.offset = 0,
};

/** Populate the next dynamic token. */
static void
cmd_flow_tok(cmdline_parse_token_hdr_t **hdr,
	     cmdline_parse_token_hdr_t *(*hdrs)[])
{
	struct context *ctx = &cmd_flow_context;

	/* Always reinitialize context before requesting the first token. */
	if (!(hdr - *hdrs))
		cmd_flow_context_init(ctx);
	/* Return NULL when no more tokens are expected. */
	if (!ctx->next_num && ctx->curr) {
		*hdr = NULL;
		return;
	}
	/* Determine if command should end here. */
	if (ctx->eol && ctx->last && ctx->next_num) {
		const enum index *list = ctx->next[ctx->next_num - 1];
		int i;

		for (i = 0; list[i]; ++i) {
			if (list[i] != END)
				continue;
			*hdr = NULL;
			return;
		}
	}
	*hdr = &cmd_flow_token_hdr;
}

/** Dispatch parsed buffer to function calls. */
static void
cmd_flow_parsed(const struct buffer *in)
{
	switch (in->command) {
	case LIST:
		port_flow_list(in->port, in->args.list.group_n,
			       in->args.list.group);
		break;
	default:
		break;
	}
}

/** Token generator and output processing callback (cmdline API). */
static void
cmd_flow_cb(void *arg0, struct cmdline *cl, void *arg2)
{
	if (cl == NULL)
		cmd_flow_tok(arg0, arg2);
	else
		cmd_flow_parsed(arg0);
}

/** Global parser instance (cmdline API). */
cmdline_parse_inst_t cmd_flow = {
	.f = cmd_flow_cb,
	.data = NULL, /**< Unused. */
	.help_str = NULL, /**< Updated by cmd_flow_get_help(). */
	.tokens = {
		NULL,
	}, /**< Tokens are returned by cmd_flow_tok(). */
};
