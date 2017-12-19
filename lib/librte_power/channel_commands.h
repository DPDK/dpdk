/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
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
