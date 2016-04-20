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
