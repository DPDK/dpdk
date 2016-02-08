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

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/queue.h>

#include <rte_log.h>
#include <rte_memory.h>
#include <rte_malloc.h>
#include <rte_atomic.h>


#include "channel_monitor.h"
#include "channel_commands.h"
#include "channel_manager.h"
#include "power_manager.h"

#define RTE_LOGTYPE_CHANNEL_MONITOR RTE_LOGTYPE_USER1

#define MAX_EVENTS 256


static volatile unsigned run_loop = 1;
static int global_event_fd;
static struct epoll_event *global_events_list;

void channel_monitor_exit(void)
{
	run_loop = 0;
	rte_free(global_events_list);
}

static int
process_request(struct channel_packet *pkt, struct channel_info *chan_info)
{
	uint64_t core_mask;

	if (chan_info == NULL)
		return -1;

	if (rte_atomic32_cmpset(&(chan_info->status), CHANNEL_MGR_CHANNEL_CONNECTED,
			CHANNEL_MGR_CHANNEL_PROCESSING) == 0)
		return -1;

	if (pkt->command == CPU_POWER) {
		core_mask = get_pcpus_mask(chan_info, pkt->resource_id);
		if (core_mask == 0) {
			RTE_LOG(ERR, CHANNEL_MONITOR, "Error get physical CPU mask for "
				"channel '%s' using vCPU(%u)\n", chan_info->channel_path,
				(unsigned)pkt->unit);
			return -1;
		}
		if (__builtin_popcountll(core_mask) == 1) {

			unsigned core_num = __builtin_ffsll(core_mask) - 1;

			switch (pkt->unit) {
			case(CPU_POWER_SCALE_MIN):
					power_manager_scale_core_min(core_num);
			break;
			case(CPU_POWER_SCALE_MAX):
					power_manager_scale_core_max(core_num);
			break;
			case(CPU_POWER_SCALE_DOWN):
					power_manager_scale_core_down(core_num);
			break;
			case(CPU_POWER_SCALE_UP):
					power_manager_scale_core_up(core_num);
			break;
			default:
				break;
			}
		} else {
			switch (pkt->unit) {
			case(CPU_POWER_SCALE_MIN):
					power_manager_scale_mask_min(core_mask);
			break;
			case(CPU_POWER_SCALE_MAX):
					power_manager_scale_mask_max(core_mask);
			break;
			case(CPU_POWER_SCALE_DOWN):
					power_manager_scale_mask_down(core_mask);
			break;
			case(CPU_POWER_SCALE_UP):
					power_manager_scale_mask_up(core_mask);
			break;
			default:
				break;
			}

		}
	}
	/* Return is not checked as channel status may have been set to DISABLED
	 * from management thread
	 */
	rte_atomic32_cmpset(&(chan_info->status), CHANNEL_MGR_CHANNEL_PROCESSING,
			CHANNEL_MGR_CHANNEL_CONNECTED);
	return 0;

}

int
add_channel_to_monitor(struct channel_info **chan_info)
{
	struct channel_info *info = *chan_info;
	struct epoll_event event;

	event.events = EPOLLIN;
	event.data.ptr = info;
	if (epoll_ctl(global_event_fd, EPOLL_CTL_ADD, info->fd, &event) < 0) {
		RTE_LOG(ERR, CHANNEL_MONITOR, "Unable to add channel '%s' "
				"to epoll\n", info->channel_path);
		return -1;
	}
	return 0;
}

int
remove_channel_from_monitor(struct channel_info *chan_info)
{
	if (epoll_ctl(global_event_fd, EPOLL_CTL_DEL, chan_info->fd, NULL) < 0) {
		RTE_LOG(ERR, CHANNEL_MONITOR, "Unable to remove channel '%s' "
				"from epoll\n", chan_info->channel_path);
		return -1;
	}
	return 0;
}

int
channel_monitor_init(void)
{
	global_event_fd = epoll_create1(0);
	if (global_event_fd == 0) {
		RTE_LOG(ERR, CHANNEL_MONITOR, "Error creating epoll context with "
				"error %s\n", strerror(errno));
		return -1;
	}
	global_events_list = rte_malloc("epoll_events", sizeof(*global_events_list)
			* MAX_EVENTS, RTE_CACHE_LINE_SIZE);
	if (global_events_list == NULL) {
		RTE_LOG(ERR, CHANNEL_MONITOR, "Unable to rte_malloc for "
				"epoll events\n");
		return -1;
	}
	return 0;
}

void
run_channel_monitor(void)
{
	while (run_loop) {
		int n_events, i;

		n_events = epoll_wait(global_event_fd, global_events_list,
				MAX_EVENTS, 1);
		if (!run_loop)
			break;
		for (i = 0; i < n_events; i++) {
			struct channel_info *chan_info = (struct channel_info *)
					global_events_list[i].data.ptr;
			if ((global_events_list[i].events & EPOLLERR) ||
					(global_events_list[i].events & EPOLLHUP)) {
				RTE_LOG(DEBUG, CHANNEL_MONITOR, "Remote closed connection for "
						"channel '%s'\n", chan_info->channel_path);
				remove_channel(&chan_info);
				continue;
			}
			if (global_events_list[i].events & EPOLLIN) {

				int n_bytes, err = 0;
				struct channel_packet pkt;
				void *buffer = &pkt;
				int buffer_len = sizeof(pkt);

				while (buffer_len > 0) {
					n_bytes = read(chan_info->fd, buffer, buffer_len);
					if (n_bytes == buffer_len)
						break;
					if (n_bytes == -1) {
						err = errno;
						RTE_LOG(DEBUG, CHANNEL_MONITOR, "Received error on "
								"channel '%s' read: %s\n",
								chan_info->channel_path, strerror(err));
						remove_channel(&chan_info);
						break;
					}
					buffer = (char *)buffer + n_bytes;
					buffer_len -= n_bytes;
				}
				if (!err)
					process_request(&pkt, chan_info);
			}
		}
	}
}
