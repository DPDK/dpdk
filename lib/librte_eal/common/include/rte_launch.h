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

#ifndef _RTE_LAUNCH_H_
#define _RTE_LAUNCH_H_

/**
 * @file
 *
 * Launch tasks on other lcores
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * State of an lcore.
 */
enum rte_lcore_state_t {
	WAIT,       /**< waiting a new command */
	RUNNING,    /**< executing command */
	FINISHED,   /**< command executed */
};

/**
 * Definition of a remote launch function.
 */
typedef int (lcore_function_t)(void *);

/**
 * Launch a function on another lcore.
 *
 * To be executed on the MASTER lcore only.
 *
 * Sends a message to a slave lcore (identified by the slave_id) that
 * is in the WAIT state (this is true after the first call to
 * rte_eal_init()). This can be checked by first calling
 * rte_eal_wait_lcore(slave_id).
 *
 * When the remote lcore receives the message, it switches to
 * the RUNNING state, then calls the function f with argument arg. Once the
 * execution is done, the remote lcore switches to a FINISHED state and
 * the return value of f is stored in a local variable to be read using
 * rte_eal_wait_lcore().
 *
 * The MASTER lcore returns as soon as the message is sent and knows
 * nothing about the completion of f.
 *
 * Note: This function is not designed to offer optimum
 * performance. It is just a practical way to launch a function on
 * another lcore at initialization time.
 *
 * @param f
 *   The function to be called.
 * @param arg
 *   The argument for the function.
 * @param slave_id
 *   The identifier of the lcore on which the function should be executed.
 * @return
 *   - 0: Success. Execution of function f started on the remote lcore.
 *   - (-EBUSY): The remote lcore is not in a WAIT state.
 */
int rte_eal_remote_launch(lcore_function_t *f, void *arg, unsigned slave_id);

/**
 * This enum indicates whether the master core must execute the handler
 * launched on all logical cores.
 */
enum rte_rmt_call_master_t {
	SKIP_MASTER = 0, /**< lcore handler not executed by master core. */
	CALL_MASTER,     /**< lcore handler executed by master core. */
};

/**
 * Launch a function on all lcores.
 *
 * Check that each SLAVE lcore is in a WAIT state, then call
 * rte_eal_remote_launch() for each lcore.
 *
 * @param f
 *   The function to be called.
 * @param arg
 *   The argument for the function.
 * @param call_master
 *   If call_master set to SKIP_MASTER, the MASTER lcore does not call
 *   the function. If call_master is set to CALL_MASTER, the function
 *   is also called on master before returning. In any case, the master
 *   lcore returns as soon as it finished its job and knows nothing
 *   about the completion of f on the other lcores.
 * @return
 *   - 0: Success. Execution of function f started on all remote lcores.
 *   - (-EBUSY): At least one remote lcore is not in a WAIT state. In this
 *     case, no message is sent to any of the lcores.
 */
int rte_eal_mp_remote_launch(lcore_function_t *f, void *arg,
			     enum rte_rmt_call_master_t call_master);

/**
 * Get the state of the lcore identified by slave_id.
 *
 * To be executed on the MASTER lcore only.
 *
 * @param slave_id
 *   The identifier of the lcore.
 * @return
 *   The state of the lcore.
 */
enum rte_lcore_state_t rte_eal_get_lcore_state(unsigned slave_id);

/**
 * Wait until an lcore finishes its job.
 *
 * To be executed on the MASTER lcore only.
 *
 * If the slave lcore identified by the slave_id is in a FINISHED state,
 * switch to the WAIT state. If the lcore is in RUNNING state, wait until
 * the lcore finishes its job and moves to the FINISHED state.
 *
 * @param slave_id
 *   The identifier of the lcore.
 * @return
 *   - 0: If the lcore identified by the slave_id is in a WAIT state.
 *   - The value that was returned by the previous remote launch
 *     function call if the lcore identified by the slave_id was in a
 *     FINISHED or RUNNING state. In this case, it changes the state
 *     of the lcore to WAIT.
 */
int rte_eal_wait_lcore(unsigned slave_id);

/**
 * Wait until all lcores finish their jobs.
 *
 * To be executed on the MASTER lcore only. Issue an
 * rte_eal_wait_lcore() for every lcore. The return values are
 * ignored.
 *
 * After a call to rte_eal_mp_wait_lcore(), the caller can assume
 * that all slave lcores are in a WAIT state.
 */
void rte_eal_mp_wait_lcore(void);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_LAUNCH_H_ */
