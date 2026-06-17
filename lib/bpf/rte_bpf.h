/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#ifndef _RTE_BPF_H_
#define _RTE_BPF_H_

/**
 * @file rte_bpf.h
 *
 * RTE BPF support.
 *
 * librte_bpf provides a framework to load and execute eBPF bytecode
 * inside user-space dpdk based applications.
 * It supports basic set of features from eBPF spec
 * (https://www.kernel.org/doc/Documentation/networking/filter.txt).
 */

#include <rte_common.h>
#include <rte_mbuf.h>
#include <rte_malloc.h>
#include <bpf_def.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RTE_BPF_EXEC_FLAG_JIT	RTE_BIT64(0)	/**< use JIT-compiled version */

/** Mask with all supported `RTE_BPF_EXEC_FLAG_*` flags set. */
#define RTE_BPF_EXEC_FLAG_MASK  RTE_BPF_EXEC_FLAG_JIT

/**
 * Possible types for function/BPF program arguments.
 */
enum rte_bpf_arg_type {
	RTE_BPF_ARG_UNDEF,      /**< undefined */
	RTE_BPF_ARG_RAW,        /**< scalar value */
	RTE_BPF_ARG_PTR = 0x10, /**< pointer to data buffer */
	RTE_BPF_ARG_PTR_MBUF,   /**< pointer to rte_mbuf */
	RTE_BPF_ARG_RESERVED    /**< reserved for internal use */
};

/**
 * function argument information
 */
struct rte_bpf_arg {
	enum rte_bpf_arg_type type;
	/**
	 * for ptr type - max size of data buffer it points to
	 * for raw type - the size (in bytes) of the value
	 */
	size_t size;
	size_t buf_size;
	/**< for mbuf ptr type, max size of rte_mbuf data buffer */
};

/**
 * determine is argument a pointer
 */
#define RTE_BPF_ARG_PTR_TYPE(x)	((x) & RTE_BPF_ARG_PTR)

/**
 * Possible types for external symbols.
 */
enum rte_bpf_xtype {
	RTE_BPF_XTYPE_FUNC, /**< function */
	RTE_BPF_XTYPE_VAR   /**< variable */
};

/**
 * Definition for external symbols available in the BPF program.
 */
struct rte_bpf_xsym {
	const char *name;        /**< name */
	enum rte_bpf_xtype type; /**< type */
	union {
		struct {
			uint64_t (*val)(uint64_t, uint64_t, uint64_t,
				uint64_t, uint64_t);
			uint32_t nb_args;
			struct rte_bpf_arg args[EBPF_FUNC_MAX_ARGS];
			/**< Function arguments descriptions. */
			struct rte_bpf_arg ret; /**< function return value. */
		} func;
		struct {
			void *val; /**< actual memory location */
			struct rte_bpf_arg desc; /**< type, size, etc. */
		} var; /**< external variable */
	};
};

/**
 * Possible origins of eBPF program code.
 */
enum rte_bpf_origin {
	RTE_BPF_ORIGIN_RAW,		/**< code loaded from raw array */
	RTE_BPF_ORIGIN_RESERVED,	/**< reserved for cBPF */
	RTE_BPF_ORIGIN_ELF_FILE,	/**< code loaded from elf_file */
};

/**
 * Input parameters for loading eBPF code, extensible version.
 *
 * Follows libbpf conventions for extensible structs.
 */
struct rte_bpf_prm_ex {
	size_t sz;  /**< size of this struct for backward compatibility */

	uint32_t flags;  /**< flags controlling eBPF load and other options */

	enum rte_bpf_origin origin;  /**< origin of eBPF program code */

	/** program origin parameters, member in use depends on origin */
	union {
		struct {
			const struct ebpf_insn *ins;  /**< eBPF instructions */
			uint32_t nb_ins;  /**< number of instructions in ins */
		} raw;
		struct {
			const char *path;  /**< path to the ELF file */
			const char *section;  /**< ELF section with the code */
		} elf_file;
	};

	const struct rte_bpf_xsym *xsym;
	/**< array of external symbols that eBPF code is allowed to reference */
	uint32_t nb_xsym;  /**< number of elements in xsym */

	struct rte_bpf_arg prog_arg[EBPF_FUNC_MAX_ARGS];  /**< program arguments */
	uint32_t nb_prog_arg;  /**< program argument count */
};

/**
 * Input parameters for loading eBPF code, legacy version.
 */
struct rte_bpf_prm {
	const struct ebpf_insn *ins; /**< array of eBPF instructions */
	uint32_t nb_ins;            /**< number of instructions in ins */
	const struct rte_bpf_xsym *xsym;
	/**< array of external symbols that eBPF code is allowed to reference */
	uint32_t nb_xsym; /**< number of elements in xsym */
	struct rte_bpf_arg prog_arg; /**< eBPF program input arg description */
};

/**
 * Information about compiled into native ISA eBPF code accepting 1 argument.
 */
struct rte_bpf_jit {
	uint64_t (*func)(void *); /**< JIT-ed native code */
	size_t sz;                /**< size of JIT-ed code */
};

union rte_bpf_func_arg {
	uint64_t u64;
	void *ptr;
};

typedef uint64_t (*rte_bpf_jit_func0_t)(void);
typedef uint64_t (*rte_bpf_jit_func1_t)(union rte_bpf_func_arg);
typedef uint64_t (*rte_bpf_jit_func2_t)(union rte_bpf_func_arg, union rte_bpf_func_arg);
typedef uint64_t (*rte_bpf_jit_func3_t)(union rte_bpf_func_arg, union rte_bpf_func_arg,
	union rte_bpf_func_arg);
typedef uint64_t (*rte_bpf_jit_func4_t)(union rte_bpf_func_arg, union rte_bpf_func_arg,
	union rte_bpf_func_arg, union rte_bpf_func_arg);
typedef uint64_t (*rte_bpf_jit_func5_t)(union rte_bpf_func_arg, union rte_bpf_func_arg,
	union rte_bpf_func_arg, union rte_bpf_func_arg, union rte_bpf_func_arg);

/**
 * JIT-ed native code, member depends on number of program arguments.
 */
struct rte_bpf_jit_ex {
	union {
		void *raw;
		rte_bpf_jit_func0_t func0;  /* nullary function */
		rte_bpf_jit_func1_t func1;  /* unary function */
		rte_bpf_jit_func2_t func2;  /* binary function */
		rte_bpf_jit_func3_t func3;  /* ternary function */
		rte_bpf_jit_func4_t func4;  /* quaternary function */
		rte_bpf_jit_func5_t func5;  /* quinary function */
	};
	size_t sz;
};

/* Tuple of eBPF program arguments. */
struct rte_bpf_prog_ctx {
	union rte_bpf_func_arg arg[EBPF_FUNC_MAX_ARGS];
};

struct rte_bpf;

/**
 * De-allocate all memory used by this eBPF execution context.
 *
 * @param bpf
 *   BPF handle to destroy.
 */
void
rte_bpf_destroy(struct rte_bpf *bpf);

/**
 * @warning
 * @b EXPERIMENTAL: This API may change, or be removed, without prior notice.
 *
 * Create a new eBPF execution context, load code from specified origin into it.
 *
 * @param prm
 *   Parameters used to create and initialise the BPF execution context.
 *
 *   Member sz must be set to the struct size as known to the application.
 *   If it exceeds the size known to the library, and the extra part has
 *   non-zero bytes, parameter is rejected. If it's smaller than the size known
 *   to the library, defaults are used for the members that are not present.
 * @return
 *   BPF handle that is used in future BPF operations,
 *   or NULL on error, with error code set in rte_errno.
 *   Possible rte_errno errors include:
 *   - EINVAL  - invalid parameter passed to function
 *   - ENOMEM  - can't reserve enough memory
 *   - ENOTSUP - requested feature is not supported (e.g. no libelf to load ELF)
 */
__rte_experimental
struct rte_bpf *
rte_bpf_load_ex(const struct rte_bpf_prm_ex *prm)
	__rte_malloc __rte_dealloc(rte_bpf_destroy, 1);

/**
 * Create a new eBPF execution context and load given BPF code into it.
 *
 * @param prm
 *  Parameters used to create and initialise the BPF execution context.
 * @return
 *   BPF handle that is used in future BPF operations,
 *   or NULL on error, with error code set in rte_errno.
 *   Possible rte_errno errors include:
 *   - EINVAL - invalid parameter passed to function
 *   - ENOMEM - can't reserve enough memory
 */
struct rte_bpf *
rte_bpf_load(const struct rte_bpf_prm *prm)
	__rte_malloc __rte_dealloc(rte_bpf_destroy, 1);

/**
 * Create a new eBPF execution context and load BPF code from given ELF
 * file into it.
 * Note that if the function will encounter EBPF_PSEUDO_CALL instruction
 * that references external symbol, it will treat is as standard BPF_CALL
 * to the external helper function.
 *
 * @param prm
 *  Parameters used to create and initialise the BPF execution context.
 * @param fname
 *  Pathname for a ELF file.
 * @param sname
 *  Name of the executable section within the file to load.
 * @return
 *   BPF handle that is used in future BPF operations,
 *   or NULL on error, with error code set in rte_errno.
 *   Possible rte_errno errors include:
 *   - EINVAL - invalid parameter passed to function
 *   - ENOMEM - can't reserve enough memory
 */
struct rte_bpf *
rte_bpf_elf_load(const struct rte_bpf_prm *prm, const char *fname,
		const char *sname)
	__rte_malloc __rte_dealloc(rte_bpf_destroy, 1);

/**
 * Execute given BPF bytecode accepting 1 argument.
 *
 * @param bpf
 *   handle for the BPF code to execute.
 * @param ctx
 *   pointer to input context.
 * @return
 *   BPF execution return value.
 */
uint64_t
rte_bpf_exec(const struct rte_bpf *bpf, void *ctx);

/**
 * @warning
 * @b EXPERIMENTAL: This API may change, or be removed, without prior notice.
 *
 * Execute given BPF bytecode accepting any number of arguments.
 *
 * @param bpf
 *   handle for the BPF code to execute.
 * @param ctx
 *   program arguments tuple.
 * @param flags
 *   bitwise OR of `RTE_BPF_EXEC_FLAG_*` values controlling execution.
 *   Flag RTE_BPF_EXEC_FLAG_JIT requires presence of JIT version (can be checked
 *   with rte_bpf_get_jit_ex).
 * @return
 *   BPF execution return value.
 */
__rte_experimental
uint64_t
rte_bpf_exec_ex(const struct rte_bpf *bpf, const struct rte_bpf_prog_ctx *ctx,
		uint64_t flags);

/**
 * Execute given BPF bytecode accepting 1 argument over a set of input contexts.
 *
 * @param bpf
 *   handle for the BPF code to execute.
 * @param ctx
 *   array of pointers to the input contexts.
 * @param rc
 *   array of return values (one per input).
 * @param num
 *   number of elements in ctx[] (and rc[]).
 * @return
 *   number of successfully processed inputs.
 */
uint32_t
rte_bpf_exec_burst(const struct rte_bpf *bpf, void *ctx[], uint64_t rc[],
		uint32_t num);

/**
 * @warning
 * @b EXPERIMENTAL: This API may change, or be removed, without prior notice.
 *
 * Execute given BPF program accepting any number of arguments over a set of
 * input contexts.
 *
 * @param bpf
 *   handle for the BPF code to execute.
 * @param ctx
 *   pointer to array of program argument tuples, can be NULL for nullary programs.
 * @param rc
 *   array of return values (one per input).
 * @param num
 *   number executions, number of elements in arrays ctx and rc[].
 * @param flags
 *   bitwise OR of `RTE_BPF_EXEC_FLAG_*` values controlling execution.
 *   Flag RTE_BPF_EXEC_FLAG_JIT requires presence of JIT version (can be checked
 *   with rte_bpf_get_jit_ex).
 * @return
 *   number of successfully processed inputs.
 */
__rte_experimental
uint32_t
rte_bpf_exec_burst_ex(const struct rte_bpf *bpf, const struct rte_bpf_prog_ctx *ctx,
		uint64_t rc[], uint32_t num, uint64_t flags);

/**
 * Provide information about natively compiled code for given BPF program
 * accepting 1 argument.
 *
 * @param bpf
 *   handle for the BPF code.
 * @param jit
 *   pointer to the rte_bpf_jit structure to be filled with related data.
 * @return
 *   - -EINVAL if the parameters are invalid.
 *   - Zero if operation completed successfully.
 */
int
rte_bpf_get_jit(const struct rte_bpf *bpf, struct rte_bpf_jit *jit);

/**
 * @warning
 * @b EXPERIMENTAL: This API may change, or be removed, without prior notice.
 *
 * Get function JIT-compiled from the BPF program.
 *
 * @param bpf
 *   handle for the BPF code.
 * @param jit
 *   pointer to the struct rte_bpf_jit_ex.
 * @return
 *   - -EINVAL if the parameters are invalid.
 *   - -ENOENT if there is no JIT-compiled version.
 *   - Zero if operation completed successfully.
 */
__rte_experimental
int
rte_bpf_get_jit_ex(const struct rte_bpf *bpf, struct rte_bpf_jit_ex *jit);

/**
 * Dump epf instructions to a file.
 *
 * @param f
 *   A pointer to a file for output
 * @param buf
 *   A pointer to BPF instructions
 * @param len
 *   Number of BPF instructions to dump.
 */
void
rte_bpf_dump(FILE *f, const struct ebpf_insn *buf, uint32_t len);

struct bpf_program;

/**
 * Convert a Classic BPF program from libpcap into a DPDK BPF code.
 *
 * @param prog
 *  Classic BPF program from pcap_compile().
 * @return
 *   Pointer to BPF program (allocated with *rte_malloc*)
 *   that is used in future BPF operations,
 *   or NULL on error, with error code set in rte_errno.
 *   Possible rte_errno errors include:
 *   - EINVAL - invalid parameter passed to function
 *   - ENOMEM - can't reserve enough memory
 *   - ENOTSUP - operation not supported
 */
struct rte_bpf_prm *
rte_bpf_convert(const struct bpf_program *prog)
	__rte_malloc __rte_dealloc_free;

#ifdef __cplusplus
}
#endif

#endif /* _RTE_BPF_H_ */
