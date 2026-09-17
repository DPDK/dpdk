/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Huawei Technologies Co., Ltd
 */

#include <rte_bpf.h>
#include <rte_log.h>


extern int validate_bpf_logtype;
#define RTE_LOGTYPE_VALIDATE_BPF validate_bpf_logtype
#define VALIDATE_BPF_LOG(level, ...) \
	RTE_LOG_LINE(level, VALIDATE_BPF, "" __VA_ARGS__)

struct rte_bpf_validate_debug;

/**
 * List of pointers to allocations, to keep track of things to free.
 *
 * May contain NULLs. May contain itself directly or via an outer struct.
 */
struct alloc_list {
	size_t count;
	void **ptrs;
};

/** Add new pointer to the alloc list, return its index. */
size_t alloc_list_append(struct alloc_list *alloc_list, void *ptr);

/** Replace pointer at the specified index in the alloc list. */
void alloc_list_replace(struct alloc_list *alloc_list, size_t index, void *ptr);

/** Free all allocations in the alloc list */
void alloc_list_free_all(struct alloc_list *alloc_list);


/** Parsed program arguments */
struct args {
	/* Allocations list, args.c internal use only. */
	struct alloc_list _alloc_list;

	/* Set if command line contains --help */
	bool show_help;

	/* BPF load parameters. */
	struct rte_bpf_prm_ex bpf_prm;

	/* Size of the mbuf data buffer. */
	size_t mbuf_buf_size;
};

/** Print program usage information */
void
print_usage(const char *program_name);

/** Print program defaults. */
void
print_defaults(void);

/**
 * Parse command-line arguments
 *
 * @param argc
 *   Command-line arguments count, as received by main.
 * @param argv
 *   Command-line arguments array, as received by main.
 *   Modified during the call due to the use of getopt_long.
 *   Modifying it after the call invalidates the return value.
 * @return
 *   - parse results in case of success;
 *   - NULL in case of an error (diagnostic will be printed to stderr)
 */
struct args *
args_parse(int argc, char *argv[]);

/** Destroy args struct returned by args_parse */
void
args_destroy(struct args *args);


/** Value for the rte_eal_init argc argument */
int
get_eal_init_argc(void);

/**
 * Value for the rte_eal_init argv argument
 *
 * @param prog_name
 *   Program name, can be obtained from argv[0]
 */
char **
get_eal_init_argv(char *prog_name);


/** Print types supported in command-line. */
void
print_supported_types(void);

/**
 * Parse text into arg.
 * @param arg
 *   Pointer to a variable to put parse result to.
 *   Member `buf_size` of types related to `struct rte_mbuf` is set to
 *   `RTE_MBUF_DEFAULT_BUF_SIZE`, adjust using `adjust_arg_buf_size` if needed.
 * @param text
 *   Text representation of the type, e.g. "struct rte_mbuf *".
 * @param alloc_list
 *   Alloc list to use for new allocations.
 * @return
 *   0 on success
 *   -1 on failure
 */
int
parse_arg(struct rte_bpf_arg *arg, const char *text);

/**
 * Parse text into xsym.
 * @param arg
 *   Pointer to a variable to put parse result to.
 *   Member `buf_size` of types related to `struct rte_mbuf` is set to
 *   `RTE_MBUF_DEFAULT_BUF_SIZE`, adjust using `adjust_xsym_buf_size` if needed.
 * @param text
 *   Text representation of the external symbol, e.g. "void exit(uint32_t)".
 * @param alloc_list
 *   Alloc list to use for new allocations.
 * @return
 *   0 on success
 *   -1 on failure
 */
int
parse_xsym(struct rte_bpf_xsym *xsym, const char *text,
	struct alloc_list *alloc_list);

/** If arg->buf_size is non-zero, set it to mbuf_buf_size */
void
adjust_arg_buf_size(struct rte_bpf_arg *arg, size_t mbuf_buf_size);

/** If buf_size members are non-zero, set them to mbuf_buf_size */
void
adjust_xsym_buf_size(struct rte_bpf_xsym *xsym, size_t mbuf_buf_size);

/** Create and set up global debugging session. */
struct rte_bpf_validate_debug *
debug_create(void);

/** Clear and destroy global debugging session. */
void
debug_destroy(struct rte_bpf_validate_debug *debug);

/** Tells caller if validation should be re-tried. */
bool
debug_validate_again(void);
