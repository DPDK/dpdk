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

#include <stdio.h>
#include <stdlib.h>
#include <sys/un.h>
#include <fcntl.h>
#include <unistd.h>
#include <inttypes.h>
#include <dirent.h>
#include <errno.h>

#include <sys/queue.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>

#include <rte_config.h>
#include <rte_malloc.h>
#include <rte_memory.h>
#include <rte_mempool.h>
#include <rte_log.h>
#include <rte_atomic.h>
#include <rte_spinlock.h>

#include <libvirt/libvirt.h>

#include "channel_manager.h"
#include "channel_commands.h"
#include "channel_monitor.h"


#define RTE_LOGTYPE_CHANNEL_MANAGER RTE_LOGTYPE_USER1

#define ITERATIVE_BITMASK_CHECK_64(mask_u64b, i) \
		for (i = 0; mask_u64b; mask_u64b &= ~(1ULL << i++)) \
		if ((mask_u64b >> i) & 1) \

/* Global pointer to libvirt connection */
static virConnectPtr global_vir_conn_ptr;

static unsigned char *global_cpumaps;
static virVcpuInfo *global_vircpuinfo;
static size_t global_maplen;

static unsigned global_n_host_cpus;

/*
 * Represents a single Virtual Machine
 */
struct virtual_machine_info {
	char name[CHANNEL_MGR_MAX_NAME_LEN];
	rte_atomic64_t pcpu_mask[CHANNEL_CMDS_MAX_CPUS];
	struct channel_info *channels[CHANNEL_CMDS_MAX_VM_CHANNELS];
	uint64_t channel_mask;
	uint8_t num_channels;
	enum vm_status status;
	virDomainPtr domainPtr;
	virDomainInfo info;
	rte_spinlock_t config_spinlock;
	LIST_ENTRY(virtual_machine_info) vms_info;
};

LIST_HEAD(, virtual_machine_info) vm_list_head;

static struct virtual_machine_info *
find_domain_by_name(const char *name)
{
	struct virtual_machine_info *info;
	LIST_FOREACH(info, &vm_list_head, vms_info) {
		if (!strncmp(info->name, name, CHANNEL_MGR_MAX_NAME_LEN-1))
			return info;
	}
	return NULL;
}

static int
update_pcpus_mask(struct virtual_machine_info *vm_info)
{
	virVcpuInfoPtr cpuinfo;
	unsigned i, j;
	int n_vcpus;
	uint64_t mask;

	memset(global_cpumaps, 0, CHANNEL_CMDS_MAX_CPUS*global_maplen);

	if (!virDomainIsActive(vm_info->domainPtr)) {
		n_vcpus = virDomainGetVcpuPinInfo(vm_info->domainPtr,
				vm_info->info.nrVirtCpu, global_cpumaps, global_maplen,
				VIR_DOMAIN_AFFECT_CONFIG);
		if (n_vcpus < 0) {
			RTE_LOG(ERR, CHANNEL_MANAGER, "Error getting vCPU info for "
					"in-active VM '%s'\n", vm_info->name);
			return -1;
		}
		goto update_pcpus;
	}

	memset(global_vircpuinfo, 0, sizeof(*global_vircpuinfo)*
			CHANNEL_CMDS_MAX_CPUS);

	cpuinfo = global_vircpuinfo;

	n_vcpus = virDomainGetVcpus(vm_info->domainPtr, cpuinfo,
			CHANNEL_CMDS_MAX_CPUS, global_cpumaps, global_maplen);
	if (n_vcpus < 0) {
		RTE_LOG(ERR, CHANNEL_MANAGER, "Error getting vCPU info for "
				"active VM '%s'\n", vm_info->name);
		return -1;
	}
update_pcpus:
	if (n_vcpus >= CHANNEL_CMDS_MAX_CPUS) {
		RTE_LOG(ERR, CHANNEL_MANAGER, "Number of vCPUS(%u) is out of range "
				"0...%d\n", n_vcpus, CHANNEL_CMDS_MAX_CPUS-1);
		return -1;
	}
	if (n_vcpus != vm_info->info.nrVirtCpu) {
		RTE_LOG(INFO, CHANNEL_MANAGER, "Updating the number of vCPUs for VM '%s"
				" from %d -> %d\n", vm_info->name, vm_info->info.nrVirtCpu,
				n_vcpus);
		vm_info->info.nrVirtCpu = n_vcpus;
	}
	for (i = 0; i < vm_info->info.nrVirtCpu; i++) {
		mask = 0;
		for (j = 0; j < global_n_host_cpus; j++) {
			if (VIR_CPU_USABLE(global_cpumaps, global_maplen, i, j) > 0) {
				mask |= 1ULL << j;
			}
		}
		rte_atomic64_set(&vm_info->pcpu_mask[i], mask);
	}
	return 0;
}

int
set_pcpus_mask(char *vm_name, unsigned vcpu, uint64_t core_mask)
{
	unsigned i = 0;
	int flags = VIR_DOMAIN_AFFECT_LIVE|VIR_DOMAIN_AFFECT_CONFIG;
	struct virtual_machine_info *vm_info;
	uint64_t mask = core_mask;

	if (vcpu >= CHANNEL_CMDS_MAX_CPUS) {
		RTE_LOG(ERR, CHANNEL_MANAGER, "vCPU(%u) exceeds max allowable(%d)\n",
				vcpu, CHANNEL_CMDS_MAX_CPUS-1);
		return -1;
	}

	vm_info = find_domain_by_name(vm_name);
	if (vm_info == NULL) {
		RTE_LOG(ERR, CHANNEL_MANAGER, "VM '%s' not found\n", vm_name);
		return -1;
	}

	if (!virDomainIsActive(vm_info->domainPtr)) {
		RTE_LOG(ERR, CHANNEL_MANAGER, "Unable to set vCPU(%u) to pCPU "
				"mask(0x%"PRIx64") for VM '%s', VM is not active\n",
				vcpu, core_mask, vm_info->name);
		return -1;
	}

	if (vcpu >= vm_info->info.nrVirtCpu) {
		RTE_LOG(ERR, CHANNEL_MANAGER, "vCPU(%u) exceeds the assigned number of "
				"vCPUs(%u)\n", vcpu, vm_info->info.nrVirtCpu);
		return -1;
	}
	memset(global_cpumaps, 0 , CHANNEL_CMDS_MAX_CPUS * global_maplen);
	ITERATIVE_BITMASK_CHECK_64(mask, i) {
		VIR_USE_CPU(global_cpumaps, i);
		if (i >= global_n_host_cpus) {
			RTE_LOG(ERR, CHANNEL_MANAGER, "CPU(%u) exceeds the available "
					"number of CPUs(%u)\n", i, global_n_host_cpus);
			return -1;
		}
	}
	if (virDomainPinVcpuFlags(vm_info->domainPtr, vcpu, global_cpumaps,
			global_maplen, flags) < 0) {
		RTE_LOG(ERR, CHANNEL_MANAGER, "Unable to set vCPU(%u) to pCPU "
				"mask(0x%"PRIx64") for VM '%s'\n", vcpu, core_mask,
				vm_info->name);
		return -1;
	}
	rte_atomic64_set(&vm_info->pcpu_mask[vcpu], core_mask);
	return 0;

}

int
set_pcpu(char *vm_name, unsigned vcpu, unsigned core_num)
{
	uint64_t mask = 1ULL << core_num;

	return set_pcpus_mask(vm_name, vcpu, mask);
}

uint64_t
get_pcpus_mask(struct channel_info *chan_info, unsigned vcpu)
{
	struct virtual_machine_info *vm_info =
			(struct virtual_machine_info *)chan_info->priv_info;
	return rte_atomic64_read(&vm_info->pcpu_mask[vcpu]);
}

static inline int
channel_exists(struct virtual_machine_info *vm_info, unsigned channel_num)
{
	rte_spinlock_lock(&(vm_info->config_spinlock));
	if (vm_info->channel_mask & (1ULL << channel_num)) {
		rte_spinlock_unlock(&(vm_info->config_spinlock));
		return 1;
	}
	rte_spinlock_unlock(&(vm_info->config_spinlock));
	return 0;
}



static int
open_non_blocking_channel(struct channel_info *info)
{
	int ret, flags;
	struct sockaddr_un sock_addr;
	fd_set soc_fd_set;
	struct timeval tv;

	info->fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (info->fd == -1) {
		RTE_LOG(ERR, CHANNEL_MANAGER, "Error(%s) creating socket for '%s'\n",
				strerror(errno),
				info->channel_path);
		return -1;
	}
	sock_addr.sun_family = AF_UNIX;
	memcpy(&sock_addr.sun_path, info->channel_path,
			strlen(info->channel_path)+1);

	/* Get current flags */
	flags = fcntl(info->fd, F_GETFL, 0);
	if (flags < 0) {
		RTE_LOG(WARNING, CHANNEL_MANAGER, "Error(%s) fcntl get flags socket for"
				"'%s'\n", strerror(errno), info->channel_path);
		return 1;
	}
	/* Set to Non Blocking */
	flags |= O_NONBLOCK;
	if (fcntl(info->fd, F_SETFL, flags) < 0) {
		RTE_LOG(WARNING, CHANNEL_MANAGER, "Error(%s) setting non-blocking "
				"socket for '%s'\n", strerror(errno), info->channel_path);
		return -1;
	}
	ret = connect(info->fd, (struct sockaddr *)&sock_addr,
			sizeof(sock_addr));
	if (ret < 0) {
		/* ECONNREFUSED error is given when VM is not active */
		if (errno == ECONNREFUSED) {
			RTE_LOG(WARNING, CHANNEL_MANAGER, "VM is not active or has not "
					"activated its endpoint to channel %s\n",
					info->channel_path);
			return -1;
		}
		/* Wait for tv_sec if in progress */
		else if (errno == EINPROGRESS) {
			tv.tv_sec = 2;
			tv.tv_usec = 0;
			FD_ZERO(&soc_fd_set);
			FD_SET(info->fd, &soc_fd_set);
			if (select(info->fd+1, NULL, &soc_fd_set, NULL, &tv) > 0) {
				RTE_LOG(WARNING, CHANNEL_MANAGER, "Timeout or error on channel "
						"'%s'\n", info->channel_path);
				return -1;
			}
		} else {
			/* Any other error */
			RTE_LOG(WARNING, CHANNEL_MANAGER, "Error(%s) connecting socket"
					" for '%s'\n", strerror(errno), info->channel_path);
			return -1;
		}
	}
	return 0;
}

static int
setup_channel_info(struct virtual_machine_info **vm_info_dptr,
		struct channel_info **chan_info_dptr, unsigned channel_num)
{
	struct channel_info *chan_info = *chan_info_dptr;
	struct virtual_machine_info *vm_info = *vm_info_dptr;

	chan_info->channel_num = channel_num;
	chan_info->priv_info = (void *)vm_info;
	chan_info->status = CHANNEL_MGR_CHANNEL_DISCONNECTED;
	if (open_non_blocking_channel(chan_info) < 0) {
		RTE_LOG(ERR, CHANNEL_MANAGER, "Could not open channel: "
				"'%s' for VM '%s'\n",
				chan_info->channel_path, vm_info->name);
		return -1;
	}
	if (add_channel_to_monitor(&chan_info) < 0) {
		RTE_LOG(ERR, CHANNEL_MANAGER, "Could add channel: "
				"'%s' to epoll ctl for VM '%s'\n",
				chan_info->channel_path, vm_info->name);
		return -1;

	}
	rte_spinlock_lock(&(vm_info->config_spinlock));
	vm_info->num_channels++;
	vm_info->channel_mask |= 1ULL << channel_num;
	vm_info->channels[channel_num] = chan_info;
	chan_info->status = CHANNEL_MGR_CHANNEL_CONNECTED;
	rte_spinlock_unlock(&(vm_info->config_spinlock));
	return 0;
}

int
add_all_channels(const char *vm_name)
{
	DIR *d;
	struct dirent *dir;
	struct virtual_machine_info *vm_info;
	struct channel_info *chan_info;
	char *token, *remaining, *tail_ptr;
	char socket_name[PATH_MAX];
	unsigned channel_num;
	int num_channels_enabled = 0;

	/* verify VM exists */
	vm_info = find_domain_by_name(vm_name);
	if (vm_info == NULL) {
		RTE_LOG(ERR, CHANNEL_MANAGER, "VM: '%s' not found"
				" during channel discovery\n", vm_name);
		return 0;
	}
	if (!virDomainIsActive(vm_info->domainPtr)) {
		RTE_LOG(ERR, CHANNEL_MANAGER, "VM: '%s' is not active\n", vm_name);
		vm_info->status = CHANNEL_MGR_VM_INACTIVE;
		return 0;
	}
	d = opendir(CHANNEL_MGR_SOCKET_PATH);
	if (d == NULL) {
		RTE_LOG(ERR, CHANNEL_MANAGER, "Error opening directory '%s': %s\n",
				CHANNEL_MGR_SOCKET_PATH, strerror(errno));
		return -1;
	}
	while ((dir = readdir(d)) != NULL) {
		if (!strncmp(dir->d_name, ".", 1) ||
				!strncmp(dir->d_name, "..", 2))
			continue;

		snprintf(socket_name, sizeof(socket_name), "%s", dir->d_name);
		remaining = socket_name;
		/* Extract vm_name from "<vm_name>.<channel_num>" */
		token = strsep(&remaining, ".");
		if (remaining == NULL)
			continue;
		if (strncmp(vm_name, token, CHANNEL_MGR_MAX_NAME_LEN))
			continue;

		/* remaining should contain only <channel_num> */
		errno = 0;
		channel_num = (unsigned)strtol(remaining, &tail_ptr, 0);
		if ((errno != 0) || (remaining[0] == '\0') ||
				tail_ptr == NULL || (*tail_ptr != '\0')) {
			RTE_LOG(WARNING, CHANNEL_MANAGER, "Malformed channel name"
					"'%s' found it should be in the form of "
					"'<guest_name>.<channel_num>(decimal)'\n",
					dir->d_name);
			continue;
		}
		if (channel_num >= CHANNEL_CMDS_MAX_VM_CHANNELS) {
			RTE_LOG(WARNING, CHANNEL_MANAGER, "Channel number(%u) is "
					"greater than max allowable: %d, skipping '%s%s'\n",
					channel_num, CHANNEL_CMDS_MAX_VM_CHANNELS-1,
					CHANNEL_MGR_SOCKET_PATH, dir->d_name);
			continue;
		}
		/* if channel has not been added previously */
		if (channel_exists(vm_info, channel_num))
			continue;

		chan_info = rte_malloc(NULL, sizeof(*chan_info),
				RTE_CACHE_LINE_SIZE);
		if (chan_info == NULL) {
			RTE_LOG(ERR, CHANNEL_MANAGER, "Error allocating memory for "
				"channel '%s%s'\n", CHANNEL_MGR_SOCKET_PATH, dir->d_name);
			continue;
		}

		snprintf(chan_info->channel_path,
				sizeof(chan_info->channel_path), "%s%s",
				CHANNEL_MGR_SOCKET_PATH, dir->d_name);

		if (setup_channel_info(&vm_info, &chan_info, channel_num) < 0) {
			rte_free(chan_info);
			continue;
		}

		num_channels_enabled++;
	}
	closedir(d);
	return num_channels_enabled;
}

int
add_channels(const char *vm_name, unsigned *channel_list,
		unsigned len_channel_list)
{
	struct virtual_machine_info *vm_info;
	struct channel_info *chan_info;
	char socket_path[PATH_MAX];
	unsigned i;
	int num_channels_enabled = 0;

	vm_info = find_domain_by_name(vm_name);
	if (vm_info == NULL) {
		RTE_LOG(ERR, CHANNEL_MANAGER, "Unable to add channels: VM '%s' "
				"not found\n", vm_name);
		return 0;
	}

	if (!virDomainIsActive(vm_info->domainPtr)) {
		RTE_LOG(ERR, CHANNEL_MANAGER, "VM: '%s' is not active\n", vm_name);
		vm_info->status = CHANNEL_MGR_VM_INACTIVE;
		return 0;
	}

	for (i = 0; i < len_channel_list; i++) {

		if (channel_list[i] >= CHANNEL_CMDS_MAX_VM_CHANNELS) {
			RTE_LOG(INFO, CHANNEL_MANAGER, "Channel(%u) is out of range "
							"0...%d\n", channel_list[i],
							CHANNEL_CMDS_MAX_VM_CHANNELS-1);
			continue;
		}
		if (channel_exists(vm_info, channel_list[i])) {
			RTE_LOG(INFO, CHANNEL_MANAGER, "Channel already exists, skipping  "
					"'%s.%u'\n", vm_name, i);
			continue;
		}

		snprintf(socket_path, sizeof(socket_path), "%s%s.%u",
				CHANNEL_MGR_SOCKET_PATH, vm_name, channel_list[i]);
		errno = 0;
		if (access(socket_path, F_OK) < 0) {
			RTE_LOG(ERR, CHANNEL_MANAGER, "Channel path '%s' error: "
					"%s\n", socket_path, strerror(errno));
			continue;
		}
		chan_info = rte_malloc(NULL, sizeof(*chan_info),
				RTE_CACHE_LINE_SIZE);
		if (chan_info == NULL) {
			RTE_LOG(ERR, CHANNEL_MANAGER, "Error allocating memory for "
					"channel '%s'\n", socket_path);
			continue;
		}
		snprintf(chan_info->channel_path,
				sizeof(chan_info->channel_path), "%s%s.%u",
				CHANNEL_MGR_SOCKET_PATH, vm_name, channel_list[i]);
		if (setup_channel_info(&vm_info, &chan_info, channel_list[i]) < 0) {
			rte_free(chan_info);
			continue;
		}
		num_channels_enabled++;

	}
	return num_channels_enabled;
}

int
remove_channel(struct channel_info **chan_info_dptr)
{
	struct virtual_machine_info *vm_info;
	struct channel_info *chan_info = *chan_info_dptr;

	close(chan_info->fd);

	vm_info = (struct virtual_machine_info *)chan_info->priv_info;

	rte_spinlock_lock(&(vm_info->config_spinlock));
	vm_info->channel_mask &= ~(1ULL << chan_info->channel_num);
	vm_info->num_channels--;
	rte_spinlock_unlock(&(vm_info->config_spinlock));

	rte_free(chan_info);
	return 0;
}

int
set_channel_status_all(const char *vm_name, enum channel_status status)
{
	struct virtual_machine_info *vm_info;
	unsigned i;
	uint64_t mask;
	int num_channels_changed = 0;

	if (!(status == CHANNEL_MGR_CHANNEL_CONNECTED ||
			status == CHANNEL_MGR_CHANNEL_DISABLED)) {
		RTE_LOG(ERR, CHANNEL_MANAGER, "Channels can only be enabled or "
				"disabled: Unable to change status for VM '%s'\n", vm_name);
	}
	vm_info = find_domain_by_name(vm_name);
	if (vm_info == NULL) {
		RTE_LOG(ERR, CHANNEL_MANAGER, "Unable to disable channels: VM '%s' "
				"not found\n", vm_name);
		return 0;
	}

	rte_spinlock_lock(&(vm_info->config_spinlock));
	mask = vm_info->channel_mask;
	ITERATIVE_BITMASK_CHECK_64(mask, i) {
		vm_info->channels[i]->status = status;
		num_channels_changed++;
	}
	rte_spinlock_unlock(&(vm_info->config_spinlock));
	return num_channels_changed;

}

int
set_channel_status(const char *vm_name, unsigned *channel_list,
		unsigned len_channel_list, enum channel_status status)
{
	struct virtual_machine_info *vm_info;
	unsigned i;
	int num_channels_changed = 0;

	if (!(status == CHANNEL_MGR_CHANNEL_CONNECTED ||
			status == CHANNEL_MGR_CHANNEL_DISABLED)) {
		RTE_LOG(ERR, CHANNEL_MANAGER, "Channels can only be enabled or "
				"disabled: Unable to change status for VM '%s'\n", vm_name);
	}
	vm_info = find_domain_by_name(vm_name);
	if (vm_info == NULL) {
		RTE_LOG(ERR, CHANNEL_MANAGER, "Unable to add channels: VM '%s' "
				"not found\n", vm_name);
		return 0;
	}
	for (i = 0; i < len_channel_list; i++) {
		if (channel_exists(vm_info, channel_list[i])) {
			rte_spinlock_lock(&(vm_info->config_spinlock));
			vm_info->channels[channel_list[i]]->status = status;
			rte_spinlock_unlock(&(vm_info->config_spinlock));
			num_channels_changed++;
		}
	}
	return num_channels_changed;
}

int
get_info_vm(const char *vm_name, struct vm_info *info)
{
	struct virtual_machine_info *vm_info;
	unsigned i, channel_num = 0;
	uint64_t mask;

	vm_info = find_domain_by_name(vm_name);
	if (vm_info == NULL) {
		RTE_LOG(ERR, CHANNEL_MANAGER, "VM '%s' not found\n", vm_name);
		return -1;
	}
	info->status = CHANNEL_MGR_VM_ACTIVE;
	if (!virDomainIsActive(vm_info->domainPtr))
		info->status = CHANNEL_MGR_VM_INACTIVE;

	rte_spinlock_lock(&(vm_info->config_spinlock));

	mask = vm_info->channel_mask;
	ITERATIVE_BITMASK_CHECK_64(mask, i) {
		info->channels[channel_num].channel_num = i;
		memcpy(info->channels[channel_num].channel_path,
				vm_info->channels[i]->channel_path, UNIX_PATH_MAX);
		info->channels[channel_num].status = vm_info->channels[i]->status;
		info->channels[channel_num].fd = vm_info->channels[i]->fd;
		channel_num++;
	}

	info->num_channels = channel_num;
	info->num_vcpus = vm_info->info.nrVirtCpu;
	rte_spinlock_unlock(&(vm_info->config_spinlock));

	memcpy(info->name, vm_info->name, sizeof(vm_info->name));
	for (i = 0; i < info->num_vcpus; i++) {
		info->pcpu_mask[i] = rte_atomic64_read(&vm_info->pcpu_mask[i]);
	}
	return 0;
}

int
add_vm(const char *vm_name)
{
	struct virtual_machine_info *new_domain;
	virDomainPtr dom_ptr;
	int i;

	if (find_domain_by_name(vm_name) != NULL) {
		RTE_LOG(ERR, CHANNEL_MANAGER, "Unable to add VM: VM '%s' "
				"already exists\n", vm_name);
		return -1;
	}

	if (global_vir_conn_ptr == NULL) {
		RTE_LOG(ERR, CHANNEL_MANAGER, "No connection to hypervisor exists\n");
		return -1;
	}
	dom_ptr = virDomainLookupByName(global_vir_conn_ptr, vm_name);
	if (dom_ptr == NULL) {
		RTE_LOG(ERR, CHANNEL_MANAGER, "Error on VM lookup with libvirt: "
				"VM '%s' not found\n", vm_name);
		return -1;
	}

	new_domain = rte_malloc("virtual_machine_info", sizeof(*new_domain),
			RTE_CACHE_LINE_SIZE);
	if (new_domain == NULL) {
		RTE_LOG(ERR, CHANNEL_MANAGER, "Unable to allocate memory for VM "
				"info\n");
		return -1;
	}
	new_domain->domainPtr = dom_ptr;
	if (virDomainGetInfo(new_domain->domainPtr, &new_domain->info) != 0) {
		RTE_LOG(ERR, CHANNEL_MANAGER, "Unable to get libvirt VM info\n");
		rte_free(new_domain);
		return -1;
	}
	if (new_domain->info.nrVirtCpu > CHANNEL_CMDS_MAX_CPUS) {
		RTE_LOG(ERR, CHANNEL_MANAGER, "Error the number of virtual CPUs(%u) is "
				"greater than allowable(%d)\n", new_domain->info.nrVirtCpu,
				CHANNEL_CMDS_MAX_CPUS);
		rte_free(new_domain);
		return -1;
	}

	for (i = 0; i < CHANNEL_CMDS_MAX_CPUS; i++) {
		rte_atomic64_init(&new_domain->pcpu_mask[i]);
	}
	if (update_pcpus_mask(new_domain) < 0) {
		RTE_LOG(ERR, CHANNEL_MANAGER, "Error getting physical CPU pinning\n");
		rte_free(new_domain);
		return -1;
	}
	strncpy(new_domain->name, vm_name, sizeof(new_domain->name));
	new_domain->channel_mask = 0;
	new_domain->num_channels = 0;

	if (!virDomainIsActive(dom_ptr))
		new_domain->status = CHANNEL_MGR_VM_INACTIVE;
	else
		new_domain->status = CHANNEL_MGR_VM_ACTIVE;

	rte_spinlock_init(&(new_domain->config_spinlock));
	LIST_INSERT_HEAD(&vm_list_head, new_domain, vms_info);
	return 0;
}

int
remove_vm(const char *vm_name)
{
	struct virtual_machine_info *vm_info = find_domain_by_name(vm_name);

	if (vm_info == NULL) {
		RTE_LOG(ERR, CHANNEL_MANAGER, "Unable to remove VM: VM '%s' "
				"not found\n", vm_name);
		return -1;
	}
	rte_spinlock_lock(&vm_info->config_spinlock);
	if (vm_info->num_channels != 0) {
		RTE_LOG(ERR, CHANNEL_MANAGER, "Unable to remove VM '%s', there are "
				"%"PRId8" channels still active\n",
				vm_name, vm_info->num_channels);
		rte_spinlock_unlock(&vm_info->config_spinlock);
		return -1;
	}
	LIST_REMOVE(vm_info, vms_info);
	rte_spinlock_unlock(&vm_info->config_spinlock);
	rte_free(vm_info);
	return 0;
}

static void
disconnect_hypervisor(void)
{
	if (global_vir_conn_ptr != NULL) {
		virConnectClose(global_vir_conn_ptr);
		global_vir_conn_ptr = NULL;
	}
}

static int
connect_hypervisor(const char *path)
{
	if (global_vir_conn_ptr != NULL) {
		RTE_LOG(ERR, CHANNEL_MANAGER, "Error connecting to %s, connection "
				"already established\n", path);
		return -1;
	}
	global_vir_conn_ptr = virConnectOpen(path);
	if (global_vir_conn_ptr == NULL) {
		RTE_LOG(ERR, CHANNEL_MANAGER, "Error failed to open connection to "
				"Hypervisor '%s'\n", path);
		return -1;
	}
	return 0;
}

int
channel_manager_init(const char *path)
{
	int n_cpus;

	LIST_INIT(&vm_list_head);
	if (connect_hypervisor(path) < 0) {
		RTE_LOG(ERR, CHANNEL_MANAGER, "Unable to initialize channel manager\n");
		return -1;
	}

	global_maplen = VIR_CPU_MAPLEN(CHANNEL_CMDS_MAX_CPUS);

	global_vircpuinfo = rte_zmalloc(NULL, sizeof(*global_vircpuinfo) *
			CHANNEL_CMDS_MAX_CPUS, RTE_CACHE_LINE_SIZE);
	if (global_vircpuinfo == NULL) {
		RTE_LOG(ERR, CHANNEL_MANAGER, "Error allocating memory for CPU Info\n");
		goto error;
	}
	global_cpumaps = rte_zmalloc(NULL, CHANNEL_CMDS_MAX_CPUS * global_maplen,
			RTE_CACHE_LINE_SIZE);
	if (global_cpumaps == NULL) {
		goto error;
	}

	n_cpus = virNodeGetCPUMap(global_vir_conn_ptr, NULL, NULL, 0);
	if (n_cpus <= 0) {
		RTE_LOG(ERR, CHANNEL_MANAGER, "Unable to get the number of Host "
				"CPUs\n");
		goto error;
	}
	global_n_host_cpus = (unsigned)n_cpus;

	if (global_n_host_cpus > CHANNEL_CMDS_MAX_CPUS) {
		RTE_LOG(ERR, CHANNEL_MANAGER, "The number of host CPUs(%u) exceeds the "
				"maximum of %u\n", global_n_host_cpus, CHANNEL_CMDS_MAX_CPUS);
		goto error;

	}

	return 0;
error:
	disconnect_hypervisor();
	return -1;
}

void
channel_manager_exit(void)
{
	unsigned i;
	uint64_t mask;
	struct virtual_machine_info *vm_info;

	LIST_FOREACH(vm_info, &vm_list_head, vms_info) {

		rte_spinlock_lock(&(vm_info->config_spinlock));

		mask = vm_info->channel_mask;
		ITERATIVE_BITMASK_CHECK_64(mask, i) {
			remove_channel_from_monitor(vm_info->channels[i]);
			close(vm_info->channels[i]->fd);
			rte_free(vm_info->channels[i]);
		}
		rte_spinlock_unlock(&(vm_info->config_spinlock));

		LIST_REMOVE(vm_info, vms_info);
		rte_free(vm_info);
	}

	if (global_cpumaps != NULL)
		rte_free(global_cpumaps);
	if (global_vircpuinfo != NULL)
		rte_free(global_vircpuinfo);
	disconnect_hypervisor();
}
