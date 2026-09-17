/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Huawei Technologies Co., Ltd
 */

#include "internal.h"

#include <ctype.h>
#include <getopt.h>
#include <stdlib.h>

#include <rte_errno.h>


/* Values to be used in getopt_long option.val. */
enum app_args {
	ARG_UNRECOGNIZED = '?',
	ARG_AUTO = 0,	/* Value set by getopt_long when flag is non-NULL. */
	ARG_HELP,	/* Keep this one in the beginning for UX reasons. */
	ARG_DEBUG,
	ARG_MBUF_BUF_SIZE,
	ARG_NO_PROG_ARGS,
	ARG_PROG_ARG,
	ARG_SECTION,
	ARG_XSYM,
};

/* Program options. */
static struct option app_options[] = {
	{
		.name = "help",
		.val = ARG_HELP,
	},
	{
		.name = "debug",
		.val = ARG_DEBUG,
	},
	{
		.name = "mbuf-buf-size",
		.has_arg = required_argument,
		.val = ARG_MBUF_BUF_SIZE,
	},
	{
		.name = "no-prog-arg",
		.val = ARG_NO_PROG_ARGS,
	},
	{
		.name = "no-prog-args",
		.val = ARG_NO_PROG_ARGS,
	},
	{
		.name = "prog-arg",
		.has_arg = required_argument,
		.val = ARG_PROG_ARG,
	},
	{
		.name = "section",
		.has_arg = required_argument,
		.val = ARG_SECTION,
	},
	{
		.name = "xsym",
		.has_arg = required_argument,
		.val = ARG_XSYM,
	},
	{ /* terminating zero record */ }
};

/* Default args value. */
static const struct args args_default = {
	.bpf_prm = {
		.sz = sizeof(struct rte_bpf_prm_ex),
		.origin = RTE_BPF_ORIGIN_ELF_FILE,
		.elf_file.section = ".text",
	},
	.mbuf_buf_size = RTE_MBUF_DEFAULT_BUF_SIZE,
};

/* Default --prog-arg argument value. */
static const char * const prog_arg_default = "struct rte_mbuf *";


void
print_usage(const char *program_name)
{
	static const char *const options[][2] = {
		{ "--help", "Display help and exit." },
		{ "--debug", "Enable interactive debug mode." },
		{ "--mbuf-buf-size=MBUF_BUF_SIZE", "Size of the mbuf data buffer (in bytes)." },
		{ "--prog-arg=TYPE", "Expected type of the next BPF program argument (up to 5)." },
		{ "--no-prog-args", "BPF program does not take any arguments." },
		{ "--section=SECTION", "ELF section name in the BPF object file to load." },
		{ "--xsym='TYPE NAME[(TYPE, ...)]'", "External symbol BPF program can access." },
	};

	printf("USAGE: %s [OPTIONS]... BPF_PATH\n", program_name);
	printf("OPTIONS:\n");
	for (int oi = 0; oi != RTE_DIM(options); ++oi)
		printf("\t%-31s %s\n", options[oi][0], options[oi][1]);
}

void
print_defaults(void)
{
	printf("DEFAULTS:"
			" --mbuf-buf-size=%#zx"
			" --prog-arg='%s'"
			" --section='%s'\n",
		(size_t)RTE_MBUF_DEFAULT_BUF_SIZE,
		prog_arg_default,
		args_default.bpf_prm.elf_file.section);
}

static int
parse_size(size_t *result, const char *text)
{
	char *parse_end = NULL;

	errno = 0;
	const unsigned long strtoul_result = strtoul(text, &parse_end, 0);
	if (errno != 0 || *parse_end != '\0' || strtoul_result == 0)
		return -1;

	*result = strtoul_result;
	return 0;
}

struct args *
args_parse(int argc, char *argv[])
{
	int val;
	size_t xsym_alloc_index;
	struct rte_bpf_xsym *xsym = NULL;
	const char * const program_name = argv[0];

	/* Allocate args and set aliases for some of its members. */
	struct args * const args = malloc(sizeof(*args));
	RTE_VERIFY(args != NULL);
	struct alloc_list * const alloc_list = &args->_alloc_list;
	struct rte_bpf_prm_ex * const bpf_prm = &args->bpf_prm;

	/* Set default values. */
	*args = args_default;
	RTE_VERIFY(parse_arg(&bpf_prm->prog_arg[bpf_prm->nb_prog_arg++],
		prog_arg_default) >= 0);
	bool default_prog_args = true;

	/* Reserve space for xsym in alloc_list. */
	xsym_alloc_index = alloc_list_append(alloc_list, xsym);

	while ((val = getopt_long(argc, argv, "", app_options, NULL)) != EOF) {
		int rc = 0;
		switch (val) {
		case ARG_AUTO:
			/* getopt_long made the assignment, nothing to do */
			break;
		case ARG_HELP:
			args->show_help = true;
			break;
		case ARG_DEBUG:
			if (bpf_prm->debug == NULL)
				bpf_prm->debug = debug_create();
			if (bpf_prm->debug == NULL) {
				rc = -rte_errno;
				fprintf(stderr,
					"%s: error %d creating debug session\n",
					program_name, -rc);
			}
			break;
		case ARG_MBUF_BUF_SIZE:
			rc = parse_size(&args->mbuf_buf_size, optarg);
			if (rc < 0)
				fprintf(stderr,
					"%s: invalid mbuf buf size '%s'\n",
					program_name, optarg);
			break;
		case ARG_NO_PROG_ARGS:
			bpf_prm->nb_prog_arg = 0;
			default_prog_args = false;
			break;
		case ARG_PROG_ARG:
			if (default_prog_args) {
				bpf_prm->nb_prog_arg = 0;
				default_prog_args = false;
			}

			if (bpf_prm->nb_prog_arg == RTE_DIM(bpf_prm->prog_arg)) {
				fprintf(stderr,
					"%s: at most %d program arguments allowed\n",
					program_name,
					(int)RTE_DIM(bpf_prm->prog_arg));
				rc = -EINVAL;
				break;
			}

			rc = parse_arg(&bpf_prm->prog_arg[bpf_prm->nb_prog_arg++],
				optarg);
			if (rc < 0)
				fprintf(stderr,
					"%s: unrecognized prog arg '%s'\n",
					program_name, optarg);

			break;
		case ARG_SECTION:
			bpf_prm->elf_file.section = optarg;
			break;
		case ARG_XSYM:
			bpf_prm->xsym = xsym = realloc(xsym,
				sizeof(xsym[0]) * (bpf_prm->nb_xsym + 1));
			RTE_VERIFY(xsym != NULL);
			alloc_list_replace(alloc_list, xsym_alloc_index, xsym);
			rc = parse_xsym(&xsym[bpf_prm->nb_xsym++], optarg,
					alloc_list);
			if (rc < 0)
				fprintf(stderr, "%s: invalid xsym '%s'\n",
					program_name, optarg);
			break;
		case ARG_UNRECOGNIZED:
			args_destroy(args);
			return NULL;
		default:
			rte_panic("Unexpected getopt_long return value %d\n",
				val);
		}
		if (rc < 0) {
			args_destroy(args);
			return NULL;
		}
	}

	/* Set buf_size to the value specified in command line arguments. */
	adjust_arg_buf_size(bpf_prm->prog_arg, args->mbuf_buf_size);
	for (size_t xsymi = 0; xsymi != bpf_prm->nb_xsym; ++xsymi)
		adjust_xsym_buf_size(&xsym[xsymi], args->mbuf_buf_size);

	/* getopt_long moves all non-options to the end, starting at optind. */
	const int nb_bpf_path = argc - optind;
	switch (nb_bpf_path) {
	case 0:
		break;
	case 1:
		bpf_prm->elf_file.path = argv[optind];
		break;
	default:
		fprintf(stderr, "%s: too many positional arguments\n",
			program_name);
		args_destroy(args);
		return NULL;
	}

	return args;
}

void
args_destroy(struct args *args)
{
	if (args == NULL)
		return;
	if (args->bpf_prm.debug != NULL)
		debug_destroy(args->bpf_prm.debug);
	alloc_list_free_all(&args->_alloc_list);
	free(args);
}
