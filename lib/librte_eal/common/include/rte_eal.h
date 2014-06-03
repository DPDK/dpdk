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

#ifndef _RTE_EAL_H_
#define _RTE_EAL_H_

/**
 * @file
 *
 * EAL Configuration API
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

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
 * The global RTE configuration structure.
 */
struct rte_config {
	uint32_t master_lcore;       /**< Id of the master lcore */
	uint32_t lcore_count;        /**< Number of available logical cores. */
	enum rte_lcore_role_t lcore_role[RTE_MAX_LCORE]; /**< State of cores. */

	/** Primary or secondary configuration */
	enum rte_proc_type_t process_type;

	/** A set of general status flags */
	unsigned flags;

	/**
	 * Pointer to memory configuration, which may be shared across multiple
	 * Intel DPDK instances
	 */
	struct rte_mem_config *mem_config;
} __attribute__((__packed__));

/* Flag definitions for rte_config flags */
#define EAL_FLG_HIGH_IOPL 1 /**< indicates high IO privilege in a linux env */

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
/**
 * Usage function typedef used by the application usage function.
 *
 * Use this function typedef to define and call rte_set_applcation_usage_hook()
 * routine.
 */
typedef void	(*rte_usage_hook_t)(const char * prgname);

/**
 * Add application usage routine callout from the eal_usage() routine.
 *
 * This function allows the application to include its usage message
 * in the EAL system usage message. The routine rte_set_application_usage_hook()
 * needs to be called before the rte_eal_init() routine in the application.
 *
 * This routine is optional for the application and will behave as if the set
 * routine was never called as the default behavior.
 *
 * @param func
 *   The func argument is a function pointer to the application usage routine.
 *   Called function is defined using rte_usage_hook_t typedef, which is of
 *   the form void rte_usage_func(const char * prgname).
 *
 *   Calling this routine with a NULL value will reset the usage hook routine and
 *   return the current value, which could be NULL.
 * @return
 *   - Returns the current value of the rte_application_usage pointer to allow
 *     the caller to daisy chain the usage routines if needing more then one.
 */
rte_usage_hook_t
rte_set_application_usage_hook( rte_usage_hook_t usage_func );

/**
 * macro to get the lock of tailq in mem_config
 */
#define RTE_EAL_TAILQ_RWLOCK         (&rte_eal_get_configuration()->mem_config->qlock)

/**
 * macro to get the multiple lock of mempool shared by mutiple-instance
 */
#define RTE_EAL_MEMPOOL_RWLOCK            (&rte_eal_get_configuration()->mem_config->mplock)


/**
 * Utility macro to do a thread-safe tailq 'INSERT' of rte_mem_config
 *
 * @param idx
 *   a kind of tailq define in enum rte_tailq_t
 *
 * @param type
 *   type of list(tailq head)
 *
 * @param elm
 *   The element will be added into the list
 *
 */
#define RTE_EAL_TAILQ_INSERT_TAIL(idx, type, elm) do {	\
	struct type *list;                                      \
	list = RTE_TAILQ_LOOKUP_BY_IDX(idx, type);              \
	rte_rwlock_write_lock(RTE_EAL_TAILQ_RWLOCK);            \
	TAILQ_INSERT_TAIL(list, elm, next);                     \
	rte_rwlock_write_unlock(RTE_EAL_TAILQ_RWLOCK);          \
} while (0)

/**
 * Utility macro to do a thread-safe tailq 'REMOVE' of rte_mem_config
 *
 * @param idx
 *   a kind of tailq define in enum rte_tailq_t
 *
 * @param type
 *   type of list(tailq head)
 *
 * @param elm
 *   The element will be remove from the list
 *
 */
#define RTE_EAL_TAILQ_REMOVE(idx, type, elm) do {	\
	struct type *list;                                      \
	list = RTE_TAILQ_LOOKUP_BY_IDX(idx, type);              \
	rte_rwlock_write_lock(RTE_EAL_TAILQ_RWLOCK);            \
	TAILQ_REMOVE(list, elm, next);                          \
	rte_rwlock_write_unlock(RTE_EAL_TAILQ_RWLOCK);          \
} while (0)                                                     \


/**
 *  macro to check TAILQ exist
 *
 *  @param idx
 *    a kind of tailq define in enum rte_tailq_t
 *
 */
#define RTE_EAL_TAILQ_EXIST_CHECK(idx) do {   \
	if (RTE_TAILQ_LOOKUP_BY_IDX(idx, rte_tailq_head) == NULL){      \
		rte_errno = E_RTE_NO_TAILQ;				\
		return NULL;						\
	}								\
} while(0)

/**
 * Whether EAL is using huge pages (disabled by --no-huge option).
 * The no-huge mode cannot be used with UIO poll-mode drivers like igb/ixgbe.
 * It is useful for NIC drivers (e.g. librte_pmd_mlx4, librte_pmd_vmxnet3) or
 * crypto drivers (e.g. librte_crypto_nitrox) provided by third-parties such
 * as 6WIND.
 *
 * @return
 *   Nonzero if hugepages are enabled.
 */
int rte_eal_has_hugepages(void);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_EAL_H_ */
