/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef CHANNEL_MONITOR_H_
#define CHANNEL_MONITOR_H_

#include "channel_manager.h"
#include "channel_commands.h"

struct core_share {
	unsigned int pcpu;
	/*
	 * 1 CORE SHARE
	 * 0 NOT SHARED
	 */
	int status;
};

struct policy {
	struct channel_packet pkt;
	uint32_t pfid[MAX_VFS];
	uint32_t port[MAX_VFS];
	unsigned int enabled;
	struct core_share core_share[MAX_VCPU_PER_VM];
};

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Setup the Channel Monitor resources required to initialize epoll.
 * Must be called first before calling other functions.
 *
 * @return
 *  - 0 on success.
 *  - Negative on error.
 */
int channel_monitor_init(void);

/**
 * Run the channel monitor, loops forever on on epoll_wait.
 *
 *
 * @return
 *  None
 */
void run_channel_monitor(void);

/**
 * Exit the Channel Monitor, exiting the epoll_wait loop and events processing.
 *
 * @return
 *  - 0 on success.
 *  - Negative on error.
 */
void channel_monitor_exit(void);

/**
 * Add an open channel to monitor via epoll. A pointer to struct channel_info
 * will be registered with epoll for event processing.
 * It is thread-safe.
 *
 * @param chan_info
 *  Pointer to struct channel_info pointer.
 *
 * @return
 *  - 0 on success.
 *  - Negative on error.
 */
int add_channel_to_monitor(struct channel_info **chan_info);

/**
 * Remove a previously added channel from epoll control.
 *
 * @param chan_info
 *  Pointer to struct channel_info.
 *
 * @return
 *  - 0 on success.
 *  - Negative on error.
 */
int remove_channel_from_monitor(struct channel_info *chan_info);

#ifdef __cplusplus
}
#endif


#endif /* CHANNEL_MONITOR_H_ */
