/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <inttypes.h>
#include <dirent.h>
#include <errno.h>

#include <sys/queue.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/select.h>

#include <rte_string_fns.h>
#include <rte_malloc.h>
#include <rte_memory.h>
#include <rte_mempool.h>
#include <rte_log.h>
#include <rte_spinlock.h>
#include <rte_tailq.h>

#include <libvirt/libvirt.h>

#include "channel_manager.h"
#include "channel_monitor.h"
#include "power_manager.h"


#define RTE_LOGTYPE_CHANNEL_MANAGER RTE_LOGTYPE_USER1

struct libvirt_vm_info lvm_info[MAX_CLIENTS];

/* Global pointer to libvirt connection */
static virConnectPtr global_vir_conn_ptr;

static unsigned char *global_cpumaps;
static virVcpuInfo *global_vircpuinfo;
static size_t global_maplen;

static unsigned int global_n_host_cpus;
static bool global_hypervisor_available;

/*
 * Represents a single Virtual Machine
 */
struct virtual_machine_info {
	char name[CHANNEL_MGR_MAX_NAME_LEN];
	uint16_t pcpu_map[RTE_MAX_LCORE];
	struct channel_info *channels[RTE_MAX_LCORE];
	char channel_mask[RTE_MAX_LCORE];
	uint8_t num_channels;
	enum vm_status status;
	virDomainPtr domainPtr;
	virDomainInfo info;
	rte_spinlock_t config_spinlock;
	int allow_query;
	RTE_TAILQ_ENTRY(virtual_machine_info) vms_info;
};

RTE_TAILQ_HEAD(, virtual_machine_info) vm_list_head;

static struct virtual_machine_info *
find_domain_by_name(const char *name)
{
	struct virtual_machine_info *info;
	RTE_TAILQ_FOREACH(info, &vm_list_head, vms_info) {
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

	memset(global_cpumaps, 0, RTE_MAX_LCORE*global_maplen);

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
			RTE_MAX_LCORE);

	cpuinfo = global_vircpuinfo;

	n_vcpus = virDomainGetVcpus(vm_info->domainPtr, cpuinfo,
			RTE_MAX_LCORE, global_cpumaps, global_maplen);
	if (n_vcpus < 0) {
		RTE_LOG(ERR, CHANNEL_MANAGER, "Error getting vCPU info for "
				"active VM '%s'\n", vm_info->name);
		return -1;
	}
update_pcpus:
	if (n_vcpus >= RTE_MAX_LCORE) {
		RTE_LOG(ERR, CHANNEL_MANAGER, "Number of vCPUS(%u) is out of range "
				"0...%d\n", n_vcpus, RTE_MAX_LCORE-1);
		return -1;
	}
	if (n_vcpus != vm_info->info.nrVirtCpu) {
		RTE_LOG(INFO, CHANNEL_MANAGER, "Updating the number of vCPUs for VM '%s"
				" from %d -> %d\n", vm_info->name, vm_info->info.nrVirtCpu,
				n_vcpus);
		vm_info->info.nrVirtCpu = n_vcpus;
	}
	rte_spinlock_lock(&(vm_info->config_spinlock));
	for (i = 0; i < vm_info->info.nrVirtCpu; i++) {
		for (j = 0; j < global_n_host_cpus; j++) {
			if (VIR_CPU_USABLE(global_cpumaps,
					global_maplen, i, j) <= 0)
				continue;
			vm_info->pcpu_map[i] = j;
		}
	}
	rte_spinlock_unlock(&(vm_info->config_spinlock));
	return 0;
}

int
set_pcpu(char *vm_name, unsigned int vcpu, unsigned int pcpu)
{
	int flags = VIR_DOMAIN_AFFECT_LIVE|VIR_DOMAIN_AFFECT_CONFIG;
	struct virtual_machine_info *vm_info;

	if (vcpu >= RTE_MAX_LCORE) {
		RTE_LOG(ERR, CHANNEL_MANAGER, "vCPU(%u) exceeds max allowable(%d)\n",
				vcpu, RTE_MAX_LCORE-1);
		return -1;
	}

	vm_info = find_domain_by_name(vm_name);
	if (vm_info == NULL) {
		RTE_LOG(ERR, CHANNEL_MANAGER, "VM '%s' not found\n", vm_name);
		return -1;
	}

	if (!virDomainIsActive(vm_info->domainPtr)) {
		RTE_LOG(ERR, CHANNEL_MANAGER, "Unable to set vCPU(%u) to pCPU "
				" for VM '%s', VM is not active\n",
				vcpu, vm_info->name);
		return -1;
	}

	if (vcpu >= vm_info->info.nrVirtCpu) {
		RTE_LOG(ERR, CHANNEL_MANAGER, "vCPU(%u) exceeds the assigned number of "
				"vCPUs(%u)\n", vcpu, vm_info->info.nrVirtCpu);
		return -1;
	}
	memset(global_cpumaps, 0, RTE_MAX_LCORE * global_maplen);

	VIR_USE_CPU(global_cpumaps, pcpu);

	if (pcpu >= global_n_host_cpus) {
		RTE_LOG(ERR, CHANNEL_MANAGER, "CPU(%u) exceeds the available "
				"number of CPUs(%u)\n",
				pcpu, global_n_host_cpus);
		return -1;
	}

	if (virDomainPinVcpuFlags(vm_info->domainPtr, vcpu, global_cpumaps,
			global_maplen, flags) < 0) {
		RTE_LOG(ERR, CHANNEL_MANAGER, "Unable to set vCPU(%u) to pCPU "
				" for VM '%s'\n", vcpu,
				vm_info->name);
		return -1;
	}
	rte_spinlock_lock(&(vm_info->config_spinlock));
	vm_info->pcpu_map[vcpu] = pcpu;
	rte_spinlock_unlock(&(vm_info->config_spinlock));
	return 0;
}

uint16_t
get_pcpu(struct channel_info *chan_info, unsigned int vcpu)
{
	struct virtual_machine_info *vm_info =
			(struct virtual_machine_info *)chan_info->priv_info;

	if (global_hypervisor_available && (vm_info != NULL)) {
		uint16_t pcpu;
		rte_spinlock_lock(&(vm_info->config_spinlock));
		pcpu = vm_info->pcpu_map[vcpu];
		rte_spinlock_unlock(&(vm_info->config_spinlock));
		return pcpu;
	} else
		return 0;
}

static inline int
channel_exists(struct virtual_machine_info *vm_info, unsigned channel_num)
{
	rte_spinlock_lock(&(vm_info->config_spinlock));
	if (vm_info->channel_mask[channel_num] == 1) {
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
	if (info->fd < 0) {
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
open_host_channel(struct channel_info *info)
{
	int flags;

	info->fd = open(info->channel_path, O_RDWR | O_RSYNC);
	if (info->fd < 0) {
		RTE_LOG(ERR, CHANNEL_MANAGER, "Error(%s) opening fifo for '%s'\n",
				strerror(errno),
				info->channel_path);
		return -1;
	}

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
		RTE_LOG(WARNING, CHANNEL_MANAGER,
				"Error(%s) setting non-blocking "
				"socket for '%s'\n",
				strerror(errno), info->channel_path);
		return -1;
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
	chan_info->type = CHANNEL_TYPE_BINARY;
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
	vm_info->channel_mask[channel_num] = 1;
	vm_info->channels[channel_num] = chan_info;
	chan_info->status = CHANNEL_MGR_CHANNEL_CONNECTED;
	rte_spinlock_unlock(&(vm_info->config_spinlock));
	return 0;
}

static int
fifo_path(char *dst, unsigned int len, unsigned int id)
{
	int cnt;

	cnt = snprintf(dst, len, "%s%s%d", CHANNEL_MGR_SOCKET_PATH,
			CHANNEL_MGR_FIFO_PATTERN_NAME, id);

	if ((cnt < 0) || (cnt > (int)len - 1)) {
		RTE_LOG(ERR, CHANNEL_MANAGER, "Could not create proper "
			"string for fifo path\n");

		return -1;
	}

	return 0;
}

static int
setup_host_channel_info(struct channel_info **chan_info_dptr,
		unsigned int channel_num)
{
	struct channel_info *chan_info = *chan_info_dptr;

	chan_info->channel_num = channel_num;
	chan_info->priv_info = (void *)NULL;
	chan_info->status = CHANNEL_MGR_CHANNEL_DISCONNECTED;
	chan_info->type = CHANNEL_TYPE_JSON;

	if (open_host_channel(chan_info) < 0) {
		RTE_LOG(ERR, CHANNEL_MANAGER, "Could not open host channel: "
				"'%s'\n",
				chan_info->channel_path);
		return -1;
	}
	if (add_channel_to_monitor(&chan_info) < 0) {
		RTE_LOG(ERR, CHANNEL_MANAGER, "Could add channel: "
				"'%s' to epoll ctl\n",
				chan_info->channel_path);
		return -1;

	}
	chan_info->status = CHANNEL_MGR_CHANNEL_CONNECTED;
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

		strlcpy(socket_name, dir->d_name, sizeof(socket_name));
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
		if (channel_num >= RTE_MAX_LCORE) {
			RTE_LOG(WARNING, CHANNEL_MANAGER, "Channel number(%u) is "
					"greater than max allowable: %d, skipping '%s%s'\n",
					channel_num, RTE_MAX_LCORE-1,
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

		if ((size_t)snprintf(chan_info->channel_path,
				sizeof(chan_info->channel_path), "%s%s",
				CHANNEL_MGR_SOCKET_PATH, dir->d_name)
					>= sizeof(chan_info->channel_path)) {
			RTE_LOG(ERR, CHANNEL_MANAGER, "Pathname too long for channel '%s%s'\n",
					CHANNEL_MGR_SOCKET_PATH, dir->d_name);
			rte_free(chan_info);
			continue;
		}

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
		if (channel_list[i] >= RTE_MAX_LCORE) {
			RTE_LOG(INFO, CHANNEL_MANAGER, "Channel(%u) is out of range "
							"0...%d\n", channel_list[i],
							RTE_MAX_LCORE-1);
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
add_host_channels(void)
{
	struct channel_info *chan_info;
	char socket_path[PATH_MAX];
	int num_channels_enabled = 0;
	int ret;
	struct core_info *ci;
	struct channel_info *chan_infos[RTE_MAX_LCORE];
	int i;

	for (i = 0; i < RTE_MAX_LCORE; i++)
		chan_infos[i] = NULL;

	ci = get_core_info();
	if (ci == NULL) {
		RTE_LOG(ERR, CHANNEL_MANAGER, "Cannot allocate memory for core_info\n");
		return 0;
	}

	for (i = 0; i < ci->core_count; i++) {
		if (rte_lcore_index(i) == -1)
			continue;

		if (ci->cd[i].global_enabled_cpus == 0)
			continue;

		ret = fifo_path(socket_path, sizeof(socket_path), i);
		if (ret < 0)
			goto error;

		ret = mkfifo(socket_path, 0660);
		RTE_LOG(DEBUG, CHANNEL_MANAGER, "TRY CREATE fifo '%s'\n",
			socket_path);
		if ((errno != EEXIST) && (ret < 0)) {
			RTE_LOG(ERR, CHANNEL_MANAGER, "Cannot create fifo '%s' error: "
					"%s\n", socket_path, strerror(errno));
			goto error;
		}
		chan_info = rte_malloc(NULL, sizeof(*chan_info), 0);
		if (chan_info == NULL) {
			RTE_LOG(ERR, CHANNEL_MANAGER, "Error allocating memory for "
					"channel '%s'\n", socket_path);
			goto error;
		}
		chan_infos[i] = chan_info;
		strlcpy(chan_info->channel_path, socket_path,
				sizeof(chan_info->channel_path));

		if (setup_host_channel_info(&chan_info, i) < 0) {
			rte_free(chan_info);
			chan_infos[i] = NULL;
			goto error;
		}
		num_channels_enabled++;
	}

	return num_channels_enabled;
error:
	/* Clean up the channels opened before we hit an error. */
	for (i = 0; i < ci->core_count; i++) {
		if (chan_infos[i] != NULL) {
			remove_channel_from_monitor(chan_infos[i]);
			close(chan_infos[i]->fd);
			rte_free(chan_infos[i]);
		}
	}
	return 0;
}

int
remove_channel(struct channel_info **chan_info_dptr)
{
	struct virtual_machine_info *vm_info;
	struct channel_info *chan_info = *chan_info_dptr;

	close(chan_info->fd);

	vm_info = (struct virtual_machine_info *)chan_info->priv_info;

	rte_spinlock_lock(&(vm_info->config_spinlock));
	vm_info->channel_mask[chan_info->channel_num] = 0;
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
	char mask[RTE_MAX_LCORE];
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
	memcpy(mask, (char *)vm_info->channel_mask, RTE_MAX_LCORE);
	for (i = 0; i < RTE_MAX_LCORE; i++) {
		if (mask[i] != 1)
			continue;
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

void
get_all_vm(int *num_vm, int *num_vcpu)
{

	virNodeInfo node_info;
	virDomainPtr *domptr;
	int i, ii, numVcpus[MAX_VCPUS], n_vcpus;
	unsigned int jj;
	const char *vm_name;
	unsigned int domain_flags = VIR_CONNECT_LIST_DOMAINS_RUNNING |
				VIR_CONNECT_LIST_DOMAINS_PERSISTENT;
	unsigned int domain_flag = VIR_DOMAIN_VCPU_CONFIG;

	if (!global_hypervisor_available)
		return;

	memset(global_cpumaps, 0, RTE_MAX_LCORE*global_maplen);
	if (virNodeGetInfo(global_vir_conn_ptr, &node_info)) {
		RTE_LOG(ERR, CHANNEL_MANAGER, "Unable to retrieve node Info\n");
		return;
	}

	/* Returns number of pcpus */
	global_n_host_cpus = (unsigned int)node_info.cpus;

	/* Returns number of active domains */
	*num_vm = virConnectListAllDomains(global_vir_conn_ptr, &domptr,
					domain_flags);
	if (*num_vm <= 0) {
		RTE_LOG(ERR, CHANNEL_MANAGER, "No Active Domains Running\n");
		return;
	}

	for (i = 0; i < *num_vm; i++) {

		/* Get Domain Names */
		vm_name = virDomainGetName(domptr[i]);
		lvm_info[i].vm_name = vm_name;

		/* Get Number of Vcpus */
		numVcpus[i] = virDomainGetVcpusFlags(domptr[i], domain_flag);

		/* Get Number of VCpus & VcpuPinInfo */
		n_vcpus = virDomainGetVcpuPinInfo(domptr[i],
				numVcpus[i], global_cpumaps,
				global_maplen, domain_flag);

		if ((int)n_vcpus > 0) {
			*num_vcpu = n_vcpus;
			lvm_info[i].num_cpus = n_vcpus;
		}

		/* Save pcpu in use by libvirt VMs */
		for (ii = 0; ii < n_vcpus; ii++) {
			for (jj = 0; jj < global_n_host_cpus; jj++) {
				if (VIR_CPU_USABLE(global_cpumaps,
						global_maplen, ii, jj) > 0) {
					lvm_info[i].pcpus[ii] = jj;
				}
			}
		}
	}
}

int
get_info_vm(const char *vm_name, struct vm_info *info)
{
	struct virtual_machine_info *vm_info;
	unsigned i, channel_num = 0;
	char mask[RTE_MAX_LCORE];

	vm_info = find_domain_by_name(vm_name);
	if (vm_info == NULL) {
		RTE_LOG(ERR, CHANNEL_MANAGER, "VM '%s' not found\n", vm_name);
		return -1;
	}
	info->status = CHANNEL_MGR_VM_ACTIVE;
	if (!virDomainIsActive(vm_info->domainPtr))
		info->status = CHANNEL_MGR_VM_INACTIVE;

	rte_spinlock_lock(&(vm_info->config_spinlock));

	memcpy(mask, (char *)vm_info->channel_mask, RTE_MAX_LCORE);
	for (i = 0; i < RTE_MAX_LCORE; i++) {
		if (mask[i] != 1)
			continue;
		info->channels[channel_num].channel_num = i;
		memcpy(info->channels[channel_num].channel_path,
				vm_info->channels[i]->channel_path,
				UNIX_PATH_MAX);
		info->channels[channel_num].status =
				vm_info->channels[i]->status;
		info->channels[channel_num].fd =
				vm_info->channels[i]->fd;
		channel_num++;
	}

	info->allow_query = vm_info->allow_query;
	info->num_channels = channel_num;
	info->num_vcpus = vm_info->info.nrVirtCpu;
	rte_spinlock_unlock(&(vm_info->config_spinlock));

	memcpy(info->name, vm_info->name, sizeof(vm_info->name));
	rte_spinlock_lock(&(vm_info->config_spinlock));
	for (i = 0; i < info->num_vcpus; i++) {
		info->pcpu_map[i] = vm_info->pcpu_map[i];
	}
	rte_spinlock_unlock(&(vm_info->config_spinlock));
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
	if (new_domain->info.nrVirtCpu > RTE_MAX_LCORE) {
		RTE_LOG(ERR, CHANNEL_MANAGER, "Error the number of virtual CPUs(%u) is "
				"greater than allowable(%d)\n", new_domain->info.nrVirtCpu,
				RTE_MAX_LCORE);
		rte_free(new_domain);
		return -1;
	}

	for (i = 0; i < RTE_MAX_LCORE; i++)
		new_domain->pcpu_map[i] = 0;

	if (update_pcpus_mask(new_domain) < 0) {
		RTE_LOG(ERR, CHANNEL_MANAGER, "Error getting physical CPU pinning\n");
		rte_free(new_domain);
		return -1;
	}
	strncpy(new_domain->name, vm_name, sizeof(new_domain->name));
	new_domain->name[sizeof(new_domain->name) - 1] = '\0';
	memset(new_domain->channel_mask, 0, RTE_MAX_LCORE);
	new_domain->num_channels = 0;

	if (!virDomainIsActive(dom_ptr))
		new_domain->status = CHANNEL_MGR_VM_INACTIVE;
	else
		new_domain->status = CHANNEL_MGR_VM_ACTIVE;

	new_domain->allow_query = 0;
	rte_spinlock_init(&(new_domain->config_spinlock));
	TAILQ_INSERT_HEAD(&vm_list_head, new_domain, vms_info);
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
	TAILQ_REMOVE(&vm_list_head, vm_info, vms_info);
	rte_spinlock_unlock(&vm_info->config_spinlock);
	rte_free(vm_info);
	return 0;
}

int
set_query_status(char *vm_name,
		bool allow_query)
{
	struct virtual_machine_info *vm_info;

	vm_info = find_domain_by_name(vm_name);
	if (vm_info == NULL) {
		RTE_LOG(ERR, CHANNEL_MANAGER, "VM '%s' not found\n", vm_name);
		return -1;
	}
	rte_spinlock_lock(&(vm_info->config_spinlock));
	vm_info->allow_query = allow_query ? 1 : 0;
	rte_spinlock_unlock(&(vm_info->config_spinlock));
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
channel_manager_init(const char *path __rte_unused)
{
	virNodeInfo info;

	TAILQ_INIT(&vm_list_head);
	if (connect_hypervisor(path) < 0) {
		global_n_host_cpus = 64;
		global_hypervisor_available = 0;
		RTE_LOG(INFO, CHANNEL_MANAGER, "Unable to initialize channel manager\n");
	} else {
		global_hypervisor_available = 1;

		global_maplen = VIR_CPU_MAPLEN(RTE_MAX_LCORE);

		global_vircpuinfo = rte_zmalloc(NULL,
				sizeof(*global_vircpuinfo) *
				RTE_MAX_LCORE, RTE_CACHE_LINE_SIZE);
		if (global_vircpuinfo == NULL) {
			RTE_LOG(ERR, CHANNEL_MANAGER, "Error allocating memory for CPU Info\n");
			goto error;
		}
		global_cpumaps = rte_zmalloc(NULL,
				RTE_MAX_LCORE * global_maplen,
				RTE_CACHE_LINE_SIZE);
		if (global_cpumaps == NULL)
			goto error;

		if (virNodeGetInfo(global_vir_conn_ptr, &info)) {
			RTE_LOG(ERR, CHANNEL_MANAGER, "Unable to retrieve node Info\n");
			goto error;
		}
		global_n_host_cpus = (unsigned int)info.cpus;
	}



	if (global_n_host_cpus > RTE_MAX_LCORE) {
		RTE_LOG(WARNING, CHANNEL_MANAGER, "The number of host CPUs(%u) exceeds the "
				"maximum of %u. No cores over %u should be used.\n",
				global_n_host_cpus, RTE_MAX_LCORE,
				RTE_MAX_LCORE - 1);
		global_n_host_cpus = RTE_MAX_LCORE;
	}

	return 0;
error:
	if (global_hypervisor_available)
		disconnect_hypervisor();
	return -1;
}

void
channel_manager_exit(void)
{
	unsigned i;
	char mask[RTE_MAX_LCORE];
	struct virtual_machine_info *vm_info, *tmp;

	RTE_TAILQ_FOREACH_SAFE(vm_info, &vm_list_head, vms_info, tmp) {

		rte_spinlock_lock(&(vm_info->config_spinlock));

		memcpy(mask, (char *)vm_info->channel_mask, RTE_MAX_LCORE);
		for (i = 0; i < RTE_MAX_LCORE; i++) {
			if (mask[i] != 1)
				continue;
			remove_channel_from_monitor(
					vm_info->channels[i]);
			close(vm_info->channels[i]->fd);
			rte_free(vm_info->channels[i]);
		}
		rte_spinlock_unlock(&(vm_info->config_spinlock));

		TAILQ_REMOVE(&vm_list_head, vm_info, vms_info);
		rte_free(vm_info);
	}

	if (global_hypervisor_available) {
		/* Only needed if hypervisor available */
		rte_free(global_cpumaps);
		rte_free(global_vircpuinfo);
		disconnect_hypervisor();
	}
}
