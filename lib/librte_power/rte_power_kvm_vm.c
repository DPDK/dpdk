/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */
#include <errno.h>
#include <string.h>

#include <rte_log.h>

#include "guest_channel.h"
#include "channel_commands.h"
#include "rte_power_kvm_vm.h"
#include "rte_power_common.h"

#define FD_PATH "/dev/virtio-ports/virtio.serial.port.poweragent"

static struct channel_packet pkt[CHANNEL_CMDS_MAX_VM_CHANNELS];


int
rte_power_kvm_vm_init(unsigned lcore_id)
{
	if (lcore_id >= CHANNEL_CMDS_MAX_VM_CHANNELS) {
		RTE_LOG(ERR, POWER, "Core(%u) is out of range 0...%d\n",
				lcore_id, CHANNEL_CMDS_MAX_VM_CHANNELS-1);
		return -1;
	}
	pkt[lcore_id].command = CPU_POWER;
	pkt[lcore_id].resource_id = lcore_id;
	return guest_channel_host_connect(FD_PATH, lcore_id);
}

int
rte_power_kvm_vm_exit(unsigned lcore_id)
{
	guest_channel_host_disconnect(lcore_id);
	return 0;
}

uint32_t
rte_power_kvm_vm_freqs(__attribute__((unused)) unsigned lcore_id,
		__attribute__((unused)) uint32_t *freqs,
		__attribute__((unused)) uint32_t num)
{
	RTE_LOG(ERR, POWER, "rte_power_freqs is not implemented "
			"for Virtual Machine Power Management\n");
	return -ENOTSUP;
}

uint32_t
rte_power_kvm_vm_get_freq(__attribute__((unused)) unsigned lcore_id)
{
	RTE_LOG(ERR, POWER, "rte_power_get_freq is not implemented "
			"for Virtual Machine Power Management\n");
	return -ENOTSUP;
}

int
rte_power_kvm_vm_set_freq(__attribute__((unused)) unsigned lcore_id,
		__attribute__((unused)) uint32_t index)
{
	RTE_LOG(ERR, POWER, "rte_power_set_freq is not implemented "
			"for Virtual Machine Power Management\n");
	return -ENOTSUP;
}

static inline int
send_msg(unsigned lcore_id, uint32_t scale_direction)
{
	int ret;

	if (lcore_id >= CHANNEL_CMDS_MAX_VM_CHANNELS) {
		RTE_LOG(ERR, POWER, "Core(%u) is out of range 0...%d\n",
				lcore_id, CHANNEL_CMDS_MAX_VM_CHANNELS-1);
		return -1;
	}
	pkt[lcore_id].unit = scale_direction;
	ret = guest_channel_send_msg(&pkt[lcore_id], lcore_id);
	if (ret == 0)
		return 1;
	RTE_LOG(DEBUG, POWER, "Error sending message: %s\n",
			ret > 0 ? strerror(ret) : "channel not connected");
	return -1;
}

int
rte_power_kvm_vm_freq_up(unsigned lcore_id)
{
	return send_msg(lcore_id, CPU_POWER_SCALE_UP);
}

int
rte_power_kvm_vm_freq_down(unsigned lcore_id)
{
	return send_msg(lcore_id, CPU_POWER_SCALE_DOWN);
}

int
rte_power_kvm_vm_freq_max(unsigned lcore_id)
{
	return send_msg(lcore_id, CPU_POWER_SCALE_MAX);
}

int
rte_power_kvm_vm_freq_min(unsigned lcore_id)
{
	return send_msg(lcore_id, CPU_POWER_SCALE_MIN);
}

int
rte_power_kvm_vm_turbo_status(__attribute__((unused)) unsigned int lcore_id)
{
	RTE_LOG(ERR, POWER, "rte_power_turbo_status is not implemented for Virtual Machine Power Management\n");
	return -ENOTSUP;
}

int
rte_power_kvm_vm_enable_turbo(unsigned int lcore_id)
{
	return send_msg(lcore_id, CPU_POWER_ENABLE_TURBO);
}

int
rte_power_kvm_vm_disable_turbo(unsigned int lcore_id)
{
	return send_msg(lcore_id, CPU_POWER_DISABLE_TURBO);
}
