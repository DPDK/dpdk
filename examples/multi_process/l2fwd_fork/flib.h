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

#ifndef __FLIB_H
#define __FLIB_H

/* callback function pointer when specific slave leaves */
typedef void (slave_exit_notify)(unsigned slaveid, int stat);

enum slave_stat{
	ST_FREEZE = 1,
	ST_IDLE,
	ST_RUN,
	ST_ZOMBIE,	/* Not implemented yet */
};

/**
 * Initialize the fork lib.
 *
 * @return
 *    - 0 : fork lib initialized successfully
 *    - -1 : fork lib initialized failed
 */
int flib_init(void);

/**
 * Check that every SLAVE lcores are in WAIT state, then call
 * flib_remote_launch() for all of them. If call_master is true
 * (set to CALL_MASTER), also call the function on the master lcore.
 *
 * @param f:
 *	function pointer need to run
 * @param arg:
 *	argument for f to carry
 * @param call_master
 *	- SKIP_MASTER : only launch function on slave lcores
 *	- CALL_MASTER : launch function on master and slave lcores
 * @return
 *    - 0 : function  execute successfully
 *    - -1 :  function  execute  failed
 */
int flib_mp_remote_launch(lcore_function_t *f,
		void *arg, enum rte_rmt_call_master_t call_master);

/**
 * Send a message to a slave lcore identified by slave_id to call a
 * function f with argument arg.
 *
 * @param f:
 *	function pointer need to run
 * @param arg:
 *	argument for f to carry
 * @param slave_id
 *	slave lcore id to run on
 * @return
 *    - 0 : function  execute successfully
 *    - -1 :  function  execute  failed
 */
int flib_remote_launch(lcore_function_t *f,
					void *arg, unsigned slave_id);

/**
 * Query the running stat for specific slave, wont' work in with master id
 *
 * @param slave_id:
 *	lcore id which should not be master id
 * @return
 *    - ST_FREEZE : lcore is not in enabled core mask
 *	 - ST_IDLE     : lcore is idle
 *    -  ST_RUN     : lcore is running something
 */
enum slave_stat
flib_query_slave_status(unsigned slave_id);

/**
 * Register a callback function to be notified in case specific slave exit.
 *
 * @param slave_id:
 *	lcore id which should not be master id
 * @param cb:
 *	callback pointer to register
 * @return
 *    - 0            :  function  execute successfully
 *    - -EFAULT  :  argument error
 *    - -ENOENT :  slave_id not correct
 */
int flib_register_slave_exit_notify(unsigned slave_id,
	slave_exit_notify *cb);

/**
 * Assign a lcore ID to non-slave thread.  Non-slave thread refers to thread that
 * not created by function rte_eal_remote_launch or rte_eal_mp_remote_launch.
 * These threads can either bind lcore or float among different lcores.
 * This lcore ID will be unique in multi-thread or multi-process DPDK running
 * environment, then it can benefit from using the cache mechanism provided in
 * mempool library.
 * After calling successfully, use rte_lcore_id() to get the assigned lcore ID, but
 * other lcore funtions can't guarantee to work correctly.
 *
 * @return
 *   -    -1  : can't assign a lcore id with 3 possibilities.
 *                 - it's not non-slave thread.
 *                 - it had assign a lcore id previously
 *                 - the lcore id is running out.
 *   -  > 0 :  the assigned lcore id.
 */
int flib_assign_lcore_id(void);

/**
 * Free the lcore_id that assigned in flib_assign_lcore_id().
 * call it in case non-slave thread is leaving or left.
 *
 * @param lcore_id
 * The identifier of the lcore, which MUST be between 1 and
 *   RTE_MAX_LCORE-1.
 */
void flib_free_lcore_id(unsigned lcore_id);

#endif /* __FLIB_H  */
