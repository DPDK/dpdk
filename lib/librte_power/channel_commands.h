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

#ifndef CHANNEL_COMMANDS_H_
#define CHANNEL_COMMANDS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* Maximum number of channels per VM */
#define CHANNEL_CMDS_MAX_VM_CHANNELS 64

/* Valid Commands */
#define CPU_POWER               1
#define CPU_POWER_CONNECT       2
#define PKT_POLICY              3

/* CPU Power Command Scaling */
#define CPU_POWER_SCALE_UP      1
#define CPU_POWER_SCALE_DOWN    2
#define CPU_POWER_SCALE_MAX     3
#define CPU_POWER_SCALE_MIN     4
#define CPU_POWER_ENABLE_TURBO  5
#define CPU_POWER_DISABLE_TURBO 6
#define HOURS 24

#define MAX_VFS 10
#define VM_MAX_NAME_SZ 32

#define MAX_VCPU_PER_VM         8

struct t_boost_status {
	bool tbEnabled;
};

struct timer_profile {
	int busy_hours[HOURS];
	int quiet_hours[HOURS];
	int hours_to_use_traffic_profile[HOURS];
};

enum workload {HIGH, MEDIUM, LOW};
enum policy_to_use {
	TRAFFIC,
	TIME,
	WORKLOAD
};

struct traffic {
	uint32_t min_packet_thresh;
	uint32_t avg_max_packet_thresh;
	uint32_t max_max_packet_thresh;
};

struct channel_packet {
	uint64_t resource_id; /**< core_num, device */
	uint32_t unit;        /**< scale down/up/min/max */
	uint32_t command;     /**< Power, IO, etc */
	char vm_name[VM_MAX_NAME_SZ];

	uint64_t vfid[MAX_VFS];
	int nb_mac_to_monitor;
	struct traffic traffic_policy;
	uint8_t vcpu_to_control[MAX_VCPU_PER_VM];
	uint8_t num_vcpu;
	struct timer_profile timer_policy;
	enum workload workload;
	enum policy_to_use policy_to_use;
	struct t_boost_status t_boost_status;
};


#ifdef __cplusplus
}
#endif

#endif /* CHANNEL_COMMANDS_H_ */
