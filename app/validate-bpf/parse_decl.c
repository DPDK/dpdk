/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Huawei Technologies Co., Ltd
 */

#include "internal.h"

#include <ctype.h>
#include <stdlib.h>

#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_mbuf_core.h>
#include <rte_tcp.h>
#include <rte_udp.h>

#define RETURN_TEXT_ERROR(text, text_start, message, ...) do {                \
	const int _offset = (text) - (text_start);                            \
	VALIDATE_BPF_LOG(ERR, "at offset %d: " message,                       \
		_offset, ## __VA_ARGS__);                                     \
	VALIDATE_BPF_LOG(NOTICE, "%s", text_start);                           \
	VALIDATE_BPF_LOG(NOTICE, "%*c", _offset + 1, '^');                    \
	return -1;                                                            \
} while (0)

/* Used in place of any signature, this is not really being checked. */
static uint64_t
dummy_function(uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4,
	uint64_t arg5)
{
	RTE_SET_USED(arg1);
	RTE_SET_USED(arg2);
	RTE_SET_USED(arg3);
	RTE_SET_USED(arg4);
	RTE_SET_USED(arg5);
	return 0;
}


/* TOKENS AND TYPES */

enum token {
	TOKEN_UNRECOGNIZED = -1,
	TOKEN_END = 0,

	TOKEN_ASTERISK,
	TOKEN_BRACKET_CLOSE,
	TOKEN_BRACKET_OPEN,
	TOKEN_COMMA,
	TOKEN_PARENTHESIS_CLOSE,
	TOKEN_PARENTHESIS_OPEN,
	TOKEN_STRUCT,

	TOKEN_TYPES_BEGIN,

	TOKEN_TYPE_CHAR,
	TOKEN_TYPE_ETHER_HEADER,
	TOKEN_TYPE_INT32_T,
	TOKEN_TYPE_IP_HEADER,
	TOKEN_TYPE_IP_HEADERS,
	TOKEN_TYPE_RTE_MBUF,
	TOKEN_TYPE_TCP_HEADER,
	TOKEN_TYPE_TCP_HEADERS,
	TOKEN_TYPE_UDP_HEADER,
	TOKEN_TYPE_UDP_HEADERS,
	TOKEN_TYPE_UINT32_T,
	TOKEN_TYPE_UINT64_T,
	TOKEN_TYPE_UINTPTR_T,
	TOKEN_TYPE_VOID,

	TOKEN_TYPES_END,
};

/* Return true if the token is a type token */
static bool
is_type_token(enum token token)
{
	return token > TOKEN_TYPES_BEGIN && token < TOKEN_TYPES_END;
}

struct text_token {
	const char *text;
	unsigned int length;  /* Not size_t to fit this struct in 2 regs. */
	enum token token;
};

#define TEXT_TOKEN_DEF(text_, token_) {                                       \
	.text = text_,                                                        \
	.length = sizeof(text_) - 1,                                          \
	.token = token_,                                                      \
}

/* Token search table, MUST BE SORTED! */
const struct text_token text_tokens[] = {
	TEXT_TOKEN_DEF("(",			TOKEN_PARENTHESIS_OPEN),
	TEXT_TOKEN_DEF(")",			TOKEN_PARENTHESIS_CLOSE),
	TEXT_TOKEN_DEF("*",			TOKEN_ASTERISK),
	TEXT_TOKEN_DEF(",",			TOKEN_COMMA),
	TEXT_TOKEN_DEF("[",			TOKEN_BRACKET_OPEN),
	TEXT_TOKEN_DEF("]",			TOKEN_BRACKET_CLOSE),
	TEXT_TOKEN_DEF("char",			TOKEN_TYPE_CHAR),
	TEXT_TOKEN_DEF("ether_header",		TOKEN_TYPE_ETHER_HEADER),
	TEXT_TOKEN_DEF("int32_t",		TOKEN_TYPE_INT32_T),
	TEXT_TOKEN_DEF("ip_header",		TOKEN_TYPE_IP_HEADER),
	TEXT_TOKEN_DEF("ip_headers",		TOKEN_TYPE_IP_HEADERS),
	TEXT_TOKEN_DEF("rte_ether_hdr",		TOKEN_TYPE_ETHER_HEADER),
	TEXT_TOKEN_DEF("rte_ipv4_hdr",		TOKEN_TYPE_IP_HEADER),
	TEXT_TOKEN_DEF("rte_mbuf",		TOKEN_TYPE_RTE_MBUF),
	TEXT_TOKEN_DEF("rte_tcp_hdr",		TOKEN_TYPE_TCP_HEADER),
	TEXT_TOKEN_DEF("rte_udp_hdr",		TOKEN_TYPE_UDP_HEADER),
	TEXT_TOKEN_DEF("struct",		TOKEN_STRUCT),
	TEXT_TOKEN_DEF("tcp_header",		TOKEN_TYPE_TCP_HEADER),
	TEXT_TOKEN_DEF("tcp_headers",		TOKEN_TYPE_TCP_HEADERS),
	TEXT_TOKEN_DEF("udp_header",		TOKEN_TYPE_UDP_HEADER),
	TEXT_TOKEN_DEF("udp_headers",		TOKEN_TYPE_UDP_HEADERS),
	TEXT_TOKEN_DEF("uint32_t",		TOKEN_TYPE_UINT32_T),
	TEXT_TOKEN_DEF("uint64_t",		TOKEN_TYPE_UINT64_T),
	TEXT_TOKEN_DEF("uintptr_t",		TOKEN_TYPE_UINTPTR_T),
	TEXT_TOKEN_DEF("void",			TOKEN_TYPE_VOID),
};

const struct text_token text_token_unrecognized = { "", 0, TOKEN_UNRECOGNIZED };
const struct text_token text_token_end = { "", 0, TOKEN_END };

/* IP header maximum possible size with options. */
#define IP_HEADER_MAX_SIZE 60
#define IP_HEADERS_MAX_SIZE (sizeof(struct rte_ether_hdr) + IP_HEADER_MAX_SIZE)
#define TCP_HEADERS_MAX_SIZE (IP_HEADERS_MAX_SIZE + sizeof(struct rte_tcp_hdr))
#define UDP_HEADERS_MAX_SIZE (IP_HEADERS_MAX_SIZE + sizeof(struct rte_udp_hdr))

#define POINTER_SIZE (sizeof(char *))

const size_t type_token_size[] = {
	[TOKEN_TYPE_CHAR]		= sizeof(char),
	[TOKEN_TYPE_ETHER_HEADER]	= sizeof(struct rte_ether_hdr),
	[TOKEN_TYPE_INT32_T]		= sizeof(int32_t),
	[TOKEN_TYPE_IP_HEADERS]		= IP_HEADERS_MAX_SIZE,
	[TOKEN_TYPE_IP_HEADER]		= sizeof(struct rte_ipv4_hdr),
	[TOKEN_TYPE_TCP_HEADERS]	= TCP_HEADERS_MAX_SIZE,
	[TOKEN_TYPE_TCP_HEADER]		= sizeof(struct rte_tcp_hdr),
	[TOKEN_TYPE_UDP_HEADERS]	= UDP_HEADERS_MAX_SIZE,
	[TOKEN_TYPE_UDP_HEADER]		= sizeof(struct rte_udp_hdr),
	[TOKEN_TYPE_UINT32_T]		= sizeof(uint32_t),
	[TOKEN_TYPE_UINT64_T]		= sizeof(uint64_t),
	[TOKEN_TYPE_UINTPTR_T]		= sizeof(uintptr_t),
};

/*
 * Struct rte_bpf_arg augmented with a type of pointer to it, since rte_bpf_arg
 * by itself may not contain enough information to determine its pointer type.
 */
struct arg_info {
	struct rte_bpf_arg value;
	enum rte_bpf_arg_type ptr_type;
	bool is_array;
};

/* Return arg_info struct described by the specified type token. */
static struct arg_info
get_type_token_arg(enum token type)
{
	RTE_ASSERT(is_type_token(type));
	switch (type) {
	case TOKEN_TYPE_VOID:
		return (struct arg_info){
			.value = { .type = RTE_BPF_ARG_UNDEF },
			.ptr_type = RTE_BPF_ARG_PTR,
		};
	case TOKEN_TYPE_RTE_MBUF:
		return (struct arg_info){
			.value = {
				.type = RTE_BPF_ARG_RAW,
				.size = sizeof(struct rte_mbuf),
				.buf_size = RTE_MBUF_DEFAULT_BUF_SIZE,
			},
			.ptr_type = RTE_BPF_ARG_PTR_MBUF,
		};
	default:
		/* Should only reach here for normal sized types. */
		RTE_ASSERT((size_t)type < RTE_DIM(type_token_size) &&
			type_token_size[type] != 0);
		return (struct arg_info){
			.value = {
				.type = RTE_BPF_ARG_RAW,
				.size = type_token_size[type],
			},
			.ptr_type = RTE_BPF_ARG_PTR,
		};
	}
}


/* PARSING TEXT INTO TOKENS */

/* Return true if character matches [a-zA-Z0-9_] in regex */
static bool
iswordchar(char character)
{
	return isalnum(character) || character == '_';
}

/*
 * Compare pointer to text with pointer to struct text_token to determine
 * if text starts with the specified token.
 */
static int
text_and_text_token_cmp(const void *text_void_ptr, const void *text_token_void_ptr)
{
	int result;
	const char * const text = text_void_ptr;
	const struct text_token * const text_token = text_token_void_ptr;

	result = strncmp(text, text_token->text, text_token->length);

	/* Checking the case of a partial word match. */
	if (result == 0 && iswordchar(text[text_token->length - 1]) &&
			iswordchar(text[text_token->length]))
		/* Text word is longer than the token. */
		result = 1;

	return result;
}

/* Advance pointed character pointer to the first non-space character. */
static void
skip_space(const char **text_ptr)
{
	while (isspace(**text_ptr))
		++*text_ptr;
}

/*
 * Recognize and return a token starting text.
 * Return text_token_end if text is empty.
 * Return text_token_unrecognized if text starts with unknown token.
 */
static struct text_token
peek_text_token(const char *text)
{
	if (*text == '\0')
		return text_token_end;
	const struct text_token * const text_token = bsearch(text, text_tokens,
		RTE_DIM(text_tokens), sizeof(text_tokens[0]),
		text_and_text_token_cmp);
	if (text_token == NULL)
		return text_token_unrecognized;
	return *text_token;
}

/*
 * Advance pointed text starting with the specified token to the first
 * non-space character after it.
 * Do nothing if called for text_token_end or text_token_unrecognized.
 */
static void
consume_text_token(const char **text_ptr, struct text_token text_token)
{
	RTE_ASSERT(memcmp(*text_ptr, text_token.text, text_token.length) == 0);
	*text_ptr += text_token.length;
	skip_space(text_ptr);
}

/* Return length of the word starting at `text`. */
static size_t
find_word_length(const char *text)
{
	size_t word_length = 0;
	while (iswordchar(text[word_length]))
		++word_length;
	return word_length;
}

/* Create a copy of the word starting text, advance to next token. */
static const char *
take_name(const char **text_ptr, struct alloc_list *alloc_list)
{
	const size_t word_length = find_word_length(*text_ptr);
	if (word_length == 0)
		/* Text does not start with a word. */
		return NULL;

	/* Allocate memory for the word and add it to the alloc_list */
	char *word = malloc(word_length + 1);
	RTE_VERIFY(word != NULL);
	alloc_list_append(alloc_list, word);

	/* Copy and terminate word contents. */
	memcpy(word, *text_ptr, word_length);
	word[word_length] = '\0';

	/* Advance text pointer. */
	*text_ptr += word_length;
	skip_space(text_ptr);

	return word;
}

/* Read a number starting text, advance to next token. */
static int
take_number(size_t *number, const char **text_ptr)
{
	/* Read a word and let strtoull to decide if it's a number. */
	const size_t word_length = find_word_length(*text_ptr);
	if (word_length == 0)
		/* Text does not start with a word. */
		return -ENOENT;

	errno = 0;
	char *number_end;
	unsigned long long_number = strtoul(*text_ptr, &number_end, 0);
	if (errno > 0)
		return -errno;
	if (number_end != *text_ptr + word_length)
		/* Could not parse whole word. */
		return -EINVAL;

	*number = long_number;

	/* Advance text pointer. */
	*text_ptr += word_length;
	skip_space(text_ptr);

	return 0;
}


/* PARSING DECLARATION PARTS */

/* Change arg into a reference. */
static void
change_into_reference(struct arg_info *arg)
{
	if (RTE_BPF_ARG_PTR_TYPE(arg->value.type) != 0) {
		VALIDATE_BPF_LOG(WARNING,
			"After taking reference to a pointer the latter "
			"will be described as an opaque pointer-size blob.");
		RTE_ASSERT(arg->ptr_type == RTE_BPF_ARG_PTR);
		arg->value.size = POINTER_SIZE;
	}
	arg->value.type = arg->ptr_type;
	arg->ptr_type = RTE_BPF_ARG_PTR;
	arg->is_array = false;
}

/*
 * Recognize and consume arg starting text, advance to next token.
 */
static int
take_arg(struct arg_info *arg, const char **text_ptr, const char *text_start)
{
	struct text_token next = peek_text_token(*text_ptr);

	if (next.token == TOKEN_STRUCT) {
		consume_text_token(text_ptr, next);
		next = peek_text_token(*text_ptr);
	}

	if (!is_type_token(next.token))
		RETURN_TEXT_ERROR(*text_ptr, text_start, "expect type");
	const enum token type = next.token;
	consume_text_token(text_ptr, next);
	next = peek_text_token(*text_ptr);

	*arg = get_type_token_arg(type);

	while (next.token == TOKEN_ASTERISK) {
		consume_text_token(text_ptr, next);
		next = peek_text_token(*text_ptr);
		change_into_reference(arg);
	}

	while (next.token == TOKEN_BRACKET_OPEN) {
		consume_text_token(text_ptr, next);

		/* Initialize to zero to avoid spurious compiler warnings. */
		size_t array_length = 0;
		if (take_number(&array_length, text_ptr) < 0)
			RETURN_TEXT_ERROR(*text_ptr, text_start, "expect length");
		next = peek_text_token(*text_ptr);

		if (arg->value.size != 0 &&
				array_length > SIZE_MAX / arg->value.size)
			RETURN_TEXT_ERROR(*text_ptr, text_start, "type too big");

		if (next.token != TOKEN_BRACKET_CLOSE)
			RETURN_TEXT_ERROR(*text_ptr, text_start, "expect ']'");
		consume_text_token(text_ptr, next);
		next = peek_text_token(*text_ptr);

		change_into_reference(arg);
		arg->value.size *= array_length;
		arg->is_array = true;
	}

	return 0;
}

/* Fill struct rte_bpf_arg within xsym trying not to unzero the padding. */
static void
fill_xsym_arg(struct rte_bpf_arg *target, struct rte_bpf_arg source)
{
	/* Copy fields individually to try and prevent copying the padding. */
	target->type = source.type;
	target->size = source.size;
	target->buf_size = source.buf_size;
}

/* Build and return struct rte_bpf_arg of type RTE_BPF_XTYPE_VAR */
static int
fill_var_xsym(struct rte_bpf_xsym *xsym, struct arg_info arg, const char *name,
	struct alloc_list *alloc_list)
{
	/* Variables are passed by reference, except for arrays. */
	if (!arg.is_array) {
		if (RTE_BPF_ARG_PTR_TYPE(arg.value.type) != 0)
			VALIDATE_BPF_LOG(WARNING,
				"External pointers may not work as expected "
				"because all external variables are passed by "
				"reference but there is currently no way to "
				"describe double pointer to the validator.");
		change_into_reference(&arg);
	}

	/* Allocate something to assign to a val pointer. */
	void * const val = calloc(1, RTE_MAX(1u, arg.value.size));
	if (val == NULL) {
		VALIDATE_BPF_LOG(ERR, "could not allocate enough memory");
		return -1;
	}
	alloc_list_append(alloc_list, val);

	/* Need all padding and unused fields to be zero-filled. */
	memset(xsym, 0, sizeof(*xsym));
	xsym->name = name;
	xsym->type = RTE_BPF_XTYPE_VAR;
	xsym->var.val = val;
	fill_xsym_arg(&xsym->var.desc, arg.value);

	return 0;
}

/* Build and return struct rte_bpf_arg of type RTE_BPF_XTYPE_FUNC */
static void
fill_func_xsym(struct rte_bpf_xsym *xsym, struct arg_info arg, const char *name)
{
	/* Need all padding and unused fields to be zero-filled. */
	memset(xsym, 0, sizeof(*xsym));
	xsym->name = name;
	xsym->type = RTE_BPF_XTYPE_FUNC;
	xsym->func.val = &dummy_function;
	fill_xsym_arg(&xsym->func.ret, arg.value);
}

/*
 * Parse and store function arguments, advance to next token after ')'.
 * Value of *text_ptr should point to the next token after '('.
 */
static int
take_func_xsym_args(struct rte_bpf_xsym *xsym, const char **text_ptr,
	const char *text_start)
{
	struct arg_info arg;
	struct text_token delimiter = peek_text_token(*text_ptr);

	while (delimiter.token != TOKEN_PARENTHESIS_CLOSE) {
		if (xsym->func.nb_args == RTE_BPF_FUNC_MAX_ARGS)
			RETURN_TEXT_ERROR(*text_ptr, text_start,
				"too many arguments, maximum %d allowed",
				RTE_BPF_FUNC_MAX_ARGS);

		if (take_arg(&arg, text_ptr, text_start) < 0)
			return -1;
		if (arg.value.type == RTE_BPF_ARG_UNDEF &&
				xsym->func.nb_args != 0)
			RETURN_TEXT_ERROR(*text_ptr, text_start,
				"arguments of type void are not allowed");
		fill_xsym_arg(&xsym->func.args[xsym->func.nb_args++],
			arg.value);

		delimiter = peek_text_token(*text_ptr);
		switch (delimiter.token) {
		case TOKEN_COMMA:
			consume_text_token(text_ptr, delimiter);
			continue;
		case TOKEN_PARENTHESIS_CLOSE:
			break;
		default:
			RETURN_TEXT_ERROR(*text_ptr, text_start,
				"expect ')' or ','");
		}
	}
	consume_text_token(text_ptr, delimiter);

	/* Special case of single void argument. */
	if (xsym->func.nb_args == 1 &&
			xsym->func.args[0].type == RTE_BPF_ARG_UNDEF) {
		xsym->func.nb_args = 0;
		/* Need all padding and unused fields to be zero-filled. */
		fill_xsym_arg(&xsym->func.args[0], (struct rte_bpf_arg){});
	}

	return 0;
}

/* Make sure text has ended. */
static int
ensure_end(const char *text, const char *text_start)
{
	if (peek_text_token(text).token != TOKEN_END)
		RETURN_TEXT_ERROR(text, text_start, "trailing garbage");
	return 0;
}

/* Parse and store xsym, advance to next token. */
static int
take_xsym(struct rte_bpf_xsym *xsym, const char **text_ptr,
	const char *text_start, struct alloc_list *alloc_list)
{
	struct arg_info arg;

	if (take_arg(&arg, text_ptr, text_start) < 0)
		return -1;

	const char * const name = take_name(text_ptr, alloc_list);
	if (name == NULL)
		RETURN_TEXT_ERROR(*text_ptr, text_start, "expect name");

	const struct text_token next = peek_text_token(*text_ptr);
	switch (next.token) {
	case TOKEN_END:
		if (fill_var_xsym(xsym, arg, name, alloc_list) < 0)
			return -1;
		break;
	case TOKEN_PARENTHESIS_OPEN:
		consume_text_token(text_ptr, next);
		fill_func_xsym(xsym, arg, name);
		if (take_func_xsym_args(xsym, text_ptr, text_start) < 0)
			return -1;
		break;
	default:
		RETURN_TEXT_ERROR(*text_ptr, text_start,
			"expect '(' or text end");
	}
	return 0;
}


/* PUBLIC FUNCTIONS */

void
print_supported_types(void)
{
	printf("TYPE: [struct] BASIC_TYPE [*]... [[N]]...\n");
	printf("BASIC_TYPE: one of\n");
	for (int tti = 0; tti != RTE_DIM(text_tokens); ++tti) {
		if (is_type_token(text_tokens[tti].token))
			printf("\t%s\n", text_tokens[tti].text);
	}
}

int
parse_arg(struct rte_bpf_arg *arg, const char *text)
{
	struct arg_info arg_info;
	const char * const text_start = text;
	skip_space(&text);
	if (take_arg(&arg_info, &text, text_start) < 0)
		return -1;
	if (ensure_end(text, text_start) < 0)
		return -1;
	if (arg_info.ptr_type != RTE_BPF_ARG_PTR)
		VALIDATE_BPF_LOG(WARNING,
			"`%s` has a special pointer type which was left unused; "
			"argument was set to an opaque blob.",
			text_start);
	*arg = arg_info.value;
	return 0;
}

int
parse_xsym(struct rte_bpf_xsym *xsym, const char *text,
	struct alloc_list *alloc_list)
{
	const char * const text_start = text;
	skip_space(&text);
	if (take_xsym(xsym, &text, text_start, alloc_list) < 0)
		return -1;
	if (ensure_end(text, text_start) < 0)
		return -1;
	return 0;
}

void
adjust_arg_buf_size(struct rte_bpf_arg *arg, size_t mbuf_buf_size)
{
	if (arg->buf_size != 0)
		/* Non-zero buf_size indicates mbuf or a pointer to it. */
		arg->buf_size = mbuf_buf_size;
}

void
adjust_xsym_buf_size(struct rte_bpf_xsym *xsym, size_t mbuf_buf_size)
{
	switch (xsym->type) {
	case RTE_BPF_XTYPE_FUNC:
		for (uint32_t argi = 0; argi != xsym->func.nb_args; ++argi)
			adjust_arg_buf_size(&xsym->func.args[argi],
				mbuf_buf_size);
		adjust_arg_buf_size(&xsym->func.ret, mbuf_buf_size);
		break;
	case RTE_BPF_XTYPE_VAR:
		adjust_arg_buf_size(&xsym->var.desc, mbuf_buf_size);
		break;
	default:
		rte_panic("Unexpected xsym type %d\n", xsym->type);
	}
}
