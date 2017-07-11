/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2016 Intel Corporation. All rights reserved.
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
#include <sched.h>

#include <rte_per_lcore.h>
#include <rte_config.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RTE_MAGIC 19820526 /**< Magic number written by the main partition when ready. */

/* Maximum thread_name length. */
#define RTE_MAX_THREAD_NAME_LEN 16

/**
 * The lcore role (used in RTE or not).
 */
enum rte_lcore_role_t {
	ROLE_RTE,
	ROLE_OFF,
	ROLE_SERVICE,
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
	uint32_t service_lcore_count;/**< Number of available service cores. */
	enum rte_lcore_role_t lcore_role[RTE_MAX_LCORE]; /**< State of cores. */

	/** Primary or secondary configuration */
	enum rte_proc_type_t process_type;

	/**
	 * Pointer to memory configuration, which may be shared across multiple
	 * DPDK instances
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
 * Request iopl privilege for all RPL.
 *
 * This function should be called by pmds which need access to ioports.

 * @return
 *   - On success, returns 0.
 *   - On failure, returns -1.
 */
int rte_eal_iopl_init(void);

/**
 * Initialize the Environment Abstraction Layer (EAL).
 *
 * This function is to be executed on the MASTER lcore only, as soon
 * as possible in the application's main() function.
 *
 * The function finishes the initialization process before main() is called.
 * It puts the SLAVE lcores in the WAIT state.
 *
 * When the multi-partition feature is supported, depending on the
 * configuration (if CONFIG_RTE_EAL_MAIN_PARTITION is disabled), this
 * function waits to ensure that the magic number is set before
 * returning. See also the rte_eal_get_configuration() function. Note:
 * This behavior may change in the future.
 *
 * @param argc
 *   A non-negative value.  If it is greater than 0, the array members
 *   for argv[0] through argv[argc] (non-inclusive) shall contain pointers
 *   to strings.
 * @param argv
 *   An array of strings.  The contents of the array, as well as the strings
 *   which are pointed to by the array, may be modified by this function.
 * @return
 *   - On success, the number of parsed arguments, which is greater or
 *     equal to zero. After the call to rte_eal_init(),
 *     all arguments argv[x] with x < ret may have been modified by this
 *     function call and should not be further interpreted by the
 *     application.  The EAL does not take any ownership of the memory used
 *     for either the argv array, or its members.
 *   - On failure, -1 and rte_errno is set to a value indicating the cause
 *     for failure.  In some instances, the application will need to be
 *     restarted as part of clearing the issue.
 *
 *   Error codes returned via rte_errno:
 *     EACCES indicates a permissions issue.
 *
 *     EAGAIN indicates either a bus or system resource was not available,
 *            setup may be attempted again.
 *
 *     EALREADY indicates that the rte_eal_init function has already been
 *              called, and cannot be called again.
 *
 *     EFAULT indicates the tailq configuration name was not found in
 *            memory configuration.
 *
 *     EINVAL indicates invalid parameters were passed as argv/argc.
 *
 *     ENOMEM indicates failure likely caused by an out-of-memory condition.
 *
 *     ENODEV indicates memory setup issues.
 *
 *     ENOTSUP indicates that the EAL cannot initialize on this system.
 *
 *     EPROTO indicates that the PCI bus is either not present, or is not
 *            readable by the eal.
 *
 *     ENOEXEC indicates that a service core failed to launch successfully.
 */
int rte_eal_init(int argc, char **argv);

/**
 * Check if a primary process is currently alive
 *
 * This function returns true when a primary process is currently
 * active.
 *
 * @param config_file_path
 *   The config_file_path argument provided should point at the location
 *   that the primary process will create its config file. If NULL, the default
 *   config file path is used.
 *
 * @return
 *  - If alive, returns 1.
 *  - If dead, returns 0.
 */
int rte_eal_primary_proc_alive(const char *config_file_path);

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
 * @param usage_func
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
rte_set_application_usage_hook(rte_usage_hook_t usage_func);

/**
 * macro to get the lock of tailq in mem_config
 */
#define RTE_EAL_TAILQ_RWLOCK         (&rte_eal_get_configuration()->mem_config->qlock)

/**
 * macro to get the multiple lock of mempool shared by mutiple-instance
 */
#define RTE_EAL_MEMPOOL_RWLOCK            (&rte_eal_get_configuration()->mem_config->mplock)

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

/**
 * A wrap API for syscall gettid.
 *
 * @return
 *   On success, returns the thread ID of calling process.
 *   It is always successful.
 */
int rte_sys_gettid(void);

/**
 * Get system unique thread id.
 *
 * @return
 *   On success, returns the thread ID of calling process.
 *   It is always successful.
 */
static inline int rte_gettid(void)
{
	static RTE_DEFINE_PER_LCORE(int, _thread_id) = -1;
	if (RTE_PER_LCORE(_thread_id) == -1)
		RTE_PER_LCORE(_thread_id) = rte_sys_gettid();
	return RTE_PER_LCORE(_thread_id);
}

#define RTE_INIT(func) \
static void __attribute__((constructor, used)) func(void)

#define RTE_INIT_PRIO(func, prio) \
static void __attribute__((constructor(prio), used)) func(void)

#ifdef __cplusplus
}
#endif

#endif /* _RTE_EAL_H_ */
