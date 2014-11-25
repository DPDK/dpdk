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

#ifndef CHANNEL_MONITOR_H_
#define CHANNEL_MONITOR_H_

#include "channel_manager.h"

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
