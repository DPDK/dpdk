/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */
#include <errno.h>
#include <string.h>

#include <rte_log.h>

#include "rte_power_guest_channel.h"
#include "guest_channel.h"
#include "power_common.h"
#include "power_kvm_vm.h"

#define FD_PATH "/dev/virtio-ports/virtio.serial.port.poweragent"

static struct rte_power_channel_packet pkt[RTE_MAX_LCORE];

int
power_kvm_vm_check_supported(void)
{
	return guest_channel_host_check_exists(FD_PATH);
}

int
power_kvm_vm_init(unsigned int lcore_id)
{
	if (lcore_id >= RTE_MAX_LCORE) {
		POWER_LOG(ERR, "Core(%u) is out of range 0...%d",
				lcore_id, RTE_MAX_LCORE-1);
		return -1;
	}
	pkt[lcore_id].command = RTE_POWER_CPU_POWER;
	pkt[lcore_id].resource_id = lcore_id;
	return guest_channel_host_connect(FD_PATH, lcore_id);
}

int
power_kvm_vm_exit(unsigned int lcore_id)
{
	guest_channel_host_disconnect(lcore_id);
	return 0;
}

uint32_t
power_kvm_vm_freqs(__rte_unused unsigned int lcore_id,
		__rte_unused uint32_t *freqs,
		__rte_unused uint32_t num)
{
	POWER_LOG(ERR, "rte_power_freqs is not implemented "
			"for Virtual Machine Power Management");
	return -ENOTSUP;
}

uint32_t
power_kvm_vm_get_freq(__rte_unused unsigned int lcore_id)
{
	POWER_LOG(ERR, "rte_power_get_freq is not implemented "
			"for Virtual Machine Power Management");
	return -ENOTSUP;
}

int
power_kvm_vm_set_freq(__rte_unused unsigned int lcore_id,
		__rte_unused uint32_t index)
{
	POWER_LOG(ERR, "rte_power_set_freq is not implemented "
			"for Virtual Machine Power Management");
	return -ENOTSUP;
}

static inline int
send_msg(unsigned int lcore_id, uint32_t scale_direction)
{
	int ret;

	if (lcore_id >= RTE_MAX_LCORE) {
		POWER_LOG(ERR, "Core(%u) is out of range 0...%d",
				lcore_id, RTE_MAX_LCORE-1);
		return -1;
	}
	pkt[lcore_id].unit = scale_direction;
	ret = guest_channel_send_msg(&pkt[lcore_id], lcore_id);
	if (ret == 0)
		return 1;
	POWER_LOG(DEBUG, "Error sending message: %s",
			ret > 0 ? strerror(ret) : "channel not connected");
	return -1;
}

int
power_kvm_vm_freq_up(unsigned int lcore_id)
{
	return send_msg(lcore_id, RTE_POWER_SCALE_UP);
}

int
power_kvm_vm_freq_down(unsigned int lcore_id)
{
	return send_msg(lcore_id, RTE_POWER_SCALE_DOWN);
}

int
power_kvm_vm_freq_max(unsigned int lcore_id)
{
	return send_msg(lcore_id, RTE_POWER_SCALE_MAX);
}

int
power_kvm_vm_freq_min(unsigned int lcore_id)
{
	return send_msg(lcore_id, RTE_POWER_SCALE_MIN);
}

int
power_kvm_vm_turbo_status(__rte_unused unsigned int lcore_id)
{
	POWER_LOG(ERR, "rte_power_turbo_status is not implemented for Virtual Machine Power Management");
	return -ENOTSUP;
}

int
power_kvm_vm_enable_turbo(unsigned int lcore_id)
{
	return send_msg(lcore_id, RTE_POWER_ENABLE_TURBO);
}

int
power_kvm_vm_disable_turbo(unsigned int lcore_id)
{
	return send_msg(lcore_id, RTE_POWER_DISABLE_TURBO);
}

struct rte_power_core_capabilities;
int power_kvm_vm_get_capabilities(__rte_unused unsigned int lcore_id,
		__rte_unused struct rte_power_core_capabilities *caps)
{
	POWER_LOG(ERR, "rte_power_get_capabilities is not implemented for Virtual Machine Power Management");
	return -ENOTSUP;
}
