/*-
 *   BSD LICENSE
 * 
 *   Copyright(c) 2010-2012 Intel Corporation. All rights reserved.
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
 * 
 */

#ifndef _RTE_EAL_H_
#define _RTE_EAL_H_

/**
 * @file
 *
 * EAL Configuration API
 */

#include <rte_tailq.h>
#include <rte_memory.h>
#include <rte_memzone.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RTE_VERSION 1 /**< The version of the RTE configuration structure. */
#define RTE_MAGIC 19820526 /**< Magic number written by the main partition when ready. */

/**
 * The lcore role (used in RTE or not).
 */
enum rte_lcore_role_t {
	ROLE_RTE,
	ROLE_OFF,
};

/**
 * The type of process in a linuxapp, multi-process setup
 */
enum rte_proc_type_t {
	RTE_PROC_AUTO = -1,   /* allow auto-detection of primary/secondary */
	RTE_PROC_PRIMARY = 0, /* set to zero, so primary is the default */
	RTE_PROC_SECONDARY,

	RTE_PROC_INVALID
};

/**
 * the structure for the memory configuration for the RTE.
 * Used by the rte_config structure. It is separated out, as for multi-process
 * support, the memory details should be shared across instances
 */
struct rte_mem_config {
	/* memory topology */
	uint32_t nchannel;    /**< Number of channels (0 if unknown). */
	uint32_t nrank;       /**< Number of ranks (0 if unknown). */

	/* memory segments and zones */
	struct rte_memseg memseg[RTE_MAX_MEMSEG];    /**< Physmem descriptors. */
	struct rte_memzone memzone[RTE_MAX_MEMZONE]; /**< Memzone descriptors. */

	struct rte_tailq_head tailq_head[RTE_MAX_TAILQ]; /**< Tailqs for objects */
} __attribute__((__packed__));

/**
 * The global RTE configuration structure.
 */
struct rte_config {
	uint32_t version; /**< Configuration [structure] version. */
	uint32_t magic;   /**< Magic number - Sanity check. */


	uint32_t master_lcore;       /**< Id of the master lcore */
	uint32_t lcore_count;        /**< Number of available logical cores. */
	enum rte_lcore_role_t lcore_role[RTE_MAX_LCORE]; /**< State of cores. */

	/** Primary or secondary configuration */
	enum rte_proc_type_t process_type;

	/**
	 * Pointer to memory configuration, which may be shared across multiple
	 * Intel DPDK instances
	 */
	struct rte_mem_config *mem_config;
} __attribute__((__packed__));

/**
 * Get the global configuration structure.
 *
 * @return
 *   A pointer to the global configuration structure.
 */
struct rte_config *rte_eal_get_configuration(void);

/**
 * Get a lcore's role.
 *
 * @param lcore_id
 *   The identifier of the lcore.
 * @return
 *   The role of the lcore.
 */
enum rte_lcore_role_t rte_eal_lcore_role(unsigned lcore_id);


/**
 * Get the process type in a multi-process setup
 *
 * @return
 *   The process type
 */
enum rte_proc_type_t rte_eal_process_type(void);

/**
 * Initialize the Environment Abstraction Layer (EAL).
 *
 * This function is to be executed on the MASTER lcore only, as soon
 * as possible in the application's main() function.
 *
 * The function finishes the initialization process that was started
 * during boot (in case of baremetal) or before main() is called (in
 * case of linuxapp). It puts the SLAVE lcores in the WAIT state.
 *
 * When the multi-partition feature is supported, depending on the
 * configuration (if CONFIG_RTE_EAL_MAIN_PARTITION is disabled), this
 * function waits to ensure that the magic number is set before
 * returning. See also the rte_eal_get_configuration() function. Note:
 * This behavior may change in the future.
 *
 * @param argc
 *   The argc argument that was given to the main() function.
 * @param argv
 *   The argv argument that was given to the main() function.
 * @return
 *   - On success, the number of parsed arguments, which is greater or
 *     equal to zero. After the call to rte_eal_init(),
 *     all arguments argv[x] with x < ret may be modified and should
 *     not be accessed by the application.
 *   - On failure, a negative error value.
 */
int rte_eal_init(int argc, char **argv);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_EAL_H_ */
