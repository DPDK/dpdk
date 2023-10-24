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
#include <bpf_def.h>

#ifdef __cplusplus
extern "C" {
#endif

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
 * Input parameters for loading eBPF code.
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
 * Information about compiled into native ISA eBPF code.
 */
struct rte_bpf_jit {
	uint64_t (*func)(void *); /**< JIT-ed native code */
	size_t sz;                /**< size of JIT-ed code */
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
rte_bpf_load(const struct rte_bpf_prm *prm);

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
		const char *sname);
/**
 * Execute given BPF bytecode.
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
 * Execute given BPF bytecode over a set of input contexts.
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
 * Provide information about natively compiled code for given BPF handle.
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
rte_bpf_convert(const struct bpf_program *prog);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_BPF_H_ */
