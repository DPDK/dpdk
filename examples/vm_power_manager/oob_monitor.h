/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#ifndef OOB_MONITOR_H_
#define OOB_MONITOR_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Setup the Branch Monitor resources required to initialize epoll.
 * Must be called first before calling other functions.
 *
 * @return
 *  - 0 on success.
 *  - Negative on error.
 */
int branch_monitor_init(void);

/**
 * Run the OOB branch monitor, loops forever on on epoll_wait.
 *
 *
 * @return
 *  None
 */
void run_branch_monitor(void);

/**
 * Exit the OOB Branch Monitor.
 *
 * @return
 *  None
 */
void branch_monitor_exit(void);

/**
 * Add a core to the list of cores to monitor.
 *
 * @param core
 *  Core Number
 *
 * @return
 *  - 0 on success.
 *  - Negative on error.
 */
int add_core_to_monitor(int core);

/**
 * Remove a previously added core from core list.
 *
 * @param core
 *  Core Number
 *
 * @return
 *  - 0 on success.
 *  - Negative on error.
 */
int remove_core_from_monitor(int core);

#ifdef __cplusplus
}
#endif


#endif /* OOB_MONITOR_H_ */
