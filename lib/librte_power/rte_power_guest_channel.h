/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2021 Intel Corporation
 */
#ifndef RTE_POWER_GUEST_CHANNEL_H
#define RTE_POWER_GUEST_CHANNEL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

#define MAX_VFS 10
#define VM_MAX_NAME_SZ 32
#define MAX_VCPU_PER_VM         8
#define HOURS 24

/* Valid Commands */
#define CPU_POWER               1
#define CPU_POWER_CONNECT       2
#define PKT_POLICY              3
#define PKT_POLICY_REMOVE       4

#define CORE_TYPE_VIRTUAL 0
#define CORE_TYPE_PHYSICAL 1

/* CPU Power Command Scaling */
#define CPU_POWER_SCALE_UP      1
#define CPU_POWER_SCALE_DOWN    2
#define CPU_POWER_SCALE_MAX     3
#define CPU_POWER_SCALE_MIN     4
#define CPU_POWER_ENABLE_TURBO  5
#define CPU_POWER_DISABLE_TURBO 6

/* CPU Power Queries */
#define CPU_POWER_QUERY_FREQ_LIST  7
#define CPU_POWER_QUERY_FREQ       8
#define CPU_POWER_QUERY_CAPS_LIST  9
#define CPU_POWER_QUERY_CAPS       10

/* --- Outgoing messages --- */

/* Generic Power Command Response */
#define CPU_POWER_CMD_ACK       1
#define CPU_POWER_CMD_NACK      2

/* CPU Power Query Responses */
#define CPU_POWER_FREQ_LIST     3
#define CPU_POWER_CAPS_LIST     4

struct rte_power_timer_profile {
	int busy_hours[HOURS];
	int quiet_hours[HOURS];
	int hours_to_use_traffic_profile[HOURS];
};

enum rte_power_workload_level {HIGH, MEDIUM, LOW};

enum rte_power_policy {
	TRAFFIC,
	TIME,
	WORKLOAD,
	BRANCH_RATIO
};

struct rte_power_traffic_policy {
	uint32_t min_packet_thresh;
	uint32_t avg_max_packet_thresh;
	uint32_t max_max_packet_thresh;
};

struct rte_power_turbo_status {
	bool tbEnabled;
};

struct rte_power_channel_packet {
	uint64_t resource_id; /**< core_num, device */
	uint32_t unit;        /**< scale down/up/min/max */
	uint32_t command;     /**< Power, IO, etc */
	char vm_name[VM_MAX_NAME_SZ];

	uint64_t vfid[MAX_VFS];
	int nb_mac_to_monitor;
	struct rte_power_traffic_policy traffic_policy;
	uint8_t vcpu_to_control[MAX_VCPU_PER_VM];
	uint8_t num_vcpu;
	struct rte_power_timer_profile timer_policy;
	bool core_type;
	enum rte_power_workload_level workload;
	enum rte_power_policy policy_to_use;
	struct rte_power_turbo_status t_boost_status;
};

struct rte_power_channel_packet_freq_list {
	uint64_t resource_id; /**< core_num, device */
	uint32_t unit;        /**< scale down/up/min/max */
	uint32_t command;     /**< Power, IO, etc */
	char vm_name[VM_MAX_NAME_SZ];

	uint32_t freq_list[MAX_VCPU_PER_VM];
	uint8_t num_vcpu;
};

struct rte_power_channel_packet_caps_list {
	uint64_t resource_id; /**< core_num, device */
	uint32_t unit;        /**< scale down/up/min/max */
	uint32_t command;     /**< Power, IO, etc */
	char vm_name[VM_MAX_NAME_SZ];

	uint64_t turbo[MAX_VCPU_PER_VM];
	uint64_t priority[MAX_VCPU_PER_VM];
	uint8_t num_vcpu;
};

/**
 * @internal
 *
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Send a message contained in pkt over the Virtio-Serial to the host endpoint.
 *
 * @param pkt
 *  Pointer to a populated struct channel_packet.
 *
 * @param lcore_id
 *  Use channel specific to this lcore_id.
 *
 * @return
 *  - 0 on success.
 *  - Negative on error.
 */
__rte_experimental
int rte_power_guest_channel_send_msg(struct rte_power_channel_packet *pkt,
			unsigned int lcore_id);

/**
 * @internal
 *
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Receive a message contained in pkt over the Virtio-Serial
 * from the host endpoint.
 *
 * @param pkt
 *  Pointer to channel_packet or
 *  channel_packet_freq_list struct.
 *
 * @param pkt_len
 *  Size of expected data packet.
 *
 * @param lcore_id
 *  Use channel specific to this lcore_id.
 *
 * @return
 *  - 0 on success.
 *  - Negative on error.
 */
__rte_experimental
int rte_power_guest_channel_receive_msg(void *pkt,
		size_t pkt_len,
		unsigned int lcore_id);


#ifdef __cplusplus
}
#endif

#endif /* RTE_POWER_GUEST_CHANNEL_H_ */
