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

#ifndef CHANNEL_MANAGER_H_
#define CHANNEL_MANAGER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <linux/limits.h>
#include <sys/un.h>
#include <rte_atomic.h>

/* Maximum number of CPUs */
#define CHANNEL_CMDS_MAX_CPUS        64
#if CHANNEL_CMDS_MAX_CPUS > 64
#error Maximum number of cores is 64, overflow is guaranteed to \
    cause problems with VM Power Management
#endif

/* Maximum name length including '\0' terminator */
#define CHANNEL_MGR_MAX_NAME_LEN    64

/* Maximum number of channels to each Virtual Machine */
#define CHANNEL_MGR_MAX_CHANNELS    64

/* Hypervisor Path for libvirt(qemu/KVM) */
#define CHANNEL_MGR_DEFAULT_HV_PATH "qemu:///system"

/* File socket directory */
#define CHANNEL_MGR_SOCKET_PATH     "/tmp/powermonitor/"

#ifndef UNIX_PATH_MAX
struct sockaddr_un _sockaddr_un;
#define UNIX_PATH_MAX sizeof(_sockaddr_un.sun_path)
#endif

/* Communication Channel Status */
enum channel_status { CHANNEL_MGR_CHANNEL_DISCONNECTED = 0,
	CHANNEL_MGR_CHANNEL_CONNECTED,
	CHANNEL_MGR_CHANNEL_DISABLED,
	CHANNEL_MGR_CHANNEL_PROCESSING};

/* VM libvirt(qemu/KVM) connection status */
enum vm_status { CHANNEL_MGR_VM_INACTIVE = 0, CHANNEL_MGR_VM_ACTIVE};

/*
 *  Represents a single and exclusive VM channel that exists between a guest and
 *  the host.
 */
struct channel_info {
	char channel_path[UNIX_PATH_MAX]; /**< Path to host socket */
	volatile uint32_t status;    /**< Connection status(enum channel_status) */
	int fd;                      /**< AF_UNIX socket fd */
	unsigned channel_num;        /**< CHANNEL_MGR_SOCKET_PATH/<vm_name>.channel_num */
	void *priv_info;             /**< Pointer to private info, do not modify */
};

/* Represents a single VM instance used to return internal information about
 * a VM */
struct vm_info {
	char name[CHANNEL_MGR_MAX_NAME_LEN];          /**< VM name */
	enum vm_status status;                        /**< libvirt status */
	uint64_t pcpu_mask[CHANNEL_CMDS_MAX_CPUS];    /**< pCPU mask for each vCPU */
	unsigned num_vcpus;                           /**< number of vCPUS */
	struct channel_info channels[CHANNEL_MGR_MAX_CHANNELS]; /**< Array of channel_info */
	unsigned num_channels;                        /**< Number of channels */
};

/**
 * Initialize the Channel Manager resources and connect to the Hypervisor
 * specified in path.
 * This must be successfully called first before calling any other functions.
 * It must only be call once;
 *
 * @param path
 *  Must be a local path, e.g. qemu:///system.
 *
 * @return
 *  - 0 on success.
 *  - Negative on error.
 */
int channel_manager_init(const char *path);

/**
 * Free resources associated with the Channel Manager.
 *
 * @param path
 *  Must be a local path, e.g. qemu:///system.
 *
 * @return
 *  None
 */
void channel_manager_exit(void);

/**
 * Get the Physical CPU mask for VM lcore channel(vcpu), result is assigned to
 * core_mask.
 * It is not thread-safe.
 *
 * @param chan_info
 *  Pointer to struct channel_info
 *
 * @param vcpu
 *  The virtual CPU to query.
 *
 *
 * @return
 *  - 0 on error.
 *  - >0 on success.
 */
uint64_t get_pcpus_mask(struct channel_info *chan_info, unsigned vcpu);

/**
 * Set the Physical CPU mask for the specified vCPU.
 * It is not thread-safe.
 *
 * @param name
 *  Virtual Machine name to lookup
 *
 * @param vcpu
 *  The virtual CPU to set.
 *
 * @param core_mask
 *  The core mask of the physical CPU(s) to bind the vCPU
 *
 * @return
 *  - 0 on success.
 *  - Negative on error.
 */
int set_pcpus_mask(char *vm_name, unsigned vcpu, uint64_t core_mask);

/**
 * Set the Physical CPU for the specified vCPU.
 * It is not thread-safe.
 *
 * @param name
 *  Virtual Machine name to lookup
 *
 * @param vcpu
 *  The virtual CPU to set.
 *
 * @param core_num
 *  The core number of the physical CPU(s) to bind the vCPU
 *
 * @return
 *  - 0 on success.
 *  - Negative on error.
 */
int set_pcpu(char *vm_name, unsigned vcpu, unsigned core_num);
/**
 * Add a VM as specified by name to the Channel Manager. The name must
 * correspond to a valid libvirt domain name.
 * This is required prior to adding channels.
 * It is not thread-safe.
 *
 * @param name
 *  Virtual Machine name to lookup.
 *
 * @return
 *  - 0 on success.
 *  - Negative on error.
 */
int add_vm(const char *name);

/**
 * Remove a previously added Virtual Machine from the Channel Manager
 * It is not thread-safe.
 *
 * @param name
 *  Virtual Machine name to lookup.
 *
 * @return
 *  - 0 on success.
 *  - Negative on error.
 */
int remove_vm(const char *name);

/**
 * Add all available channels to the VM as specified by name.
 * Channels in the form of paths
 * (CHANNEL_MGR_SOCKET_PATH/<vm_name>.<channel_number>) will only be parsed.
 * It is not thread-safe.
 *
 * @param name
 *  Virtual Machine name to lookup.
 *
 * @return
 *  - N the number of channels added for the VM
 */
int add_all_channels(const char *vm_name);

/**
 * Add the channel numbers in channel_list to the domain specified by name.
 * Channels in the form of paths
 * (CHANNEL_MGR_SOCKET_PATH/<vm_name>.<channel_number>) will only be parsed.
 * It is not thread-safe.
 *
 * @param name
 *  Virtual Machine name to add channels.
 *
 * @param channel_list
 *  Pointer to list of unsigned integers, representing the channel number to add
 *  It must be allocated outside of this function.
 *
 * @param num_channels
 *  The amount of channel numbers in channel_list
 *
 * @return
 *  - N the number of channels added for the VM
 *  - 0 for error
 */
int add_channels(const char *vm_name, unsigned *channel_list,
		unsigned num_channels);

/**
 * Remove a channel definition from the channel manager. This must only be
 * called from the channel monitor thread.
 *
 * @param chan_info
 *  Pointer to a valid struct channel_info.
 *
 * @return
 *  - 0 on success.
 *  - Negative on error.
 */
int remove_channel(struct channel_info **chan_info_dptr);

/**
 * For all channels associated with a Virtual Machine name, update the
 * connection status. Valid states are CHANNEL_MGR_CHANNEL_CONNECTED or
 * CHANNEL_MGR_CHANNEL_DISABLED only.
 *
 *
 * @param name
 *  Virtual Machine name to modify all channels.
 *
 * @param status
 *  The status to set each channel
 *
 * @param num_channels
 *  The amount of channel numbers in channel_list
 *
 * @return
 *  - N the number of channels added for the VM
 *  - 0 for error
 */
int set_channel_status_all(const char *name, enum channel_status status);

/**
 * For all channels in channel_list associated with a Virtual Machine name
 * update the connection status of each.
 * Valid states are CHANNEL_MGR_CHANNEL_CONNECTED or
 * CHANNEL_MGR_CHANNEL_DISABLED only.
 * It is not thread-safe.
 *
 * @param name
 *  Virtual Machine name to add channels.
 *
 * @param channel_list
 *  Pointer to list of unsigned integers, representing the channel numbers to
 *  modify.
 *  It must be allocated outside of this function.
 *
 * @param num_channels
 *  The amount of channel numbers in channel_list
 *
 * @return
 *  - N the number of channels modified for the VM
 *  - 0 for error
 */
int set_channel_status(const char *vm_name, unsigned *channel_list,
		unsigned len_channel_list, enum channel_status status);

/**
 * Populates a pointer to struct vm_info associated with vm_name.
 *
 * @param vm_name
 *  The name of the virtual machine to lookup.
 *
 *  @param vm_info
 *   Pointer to a struct vm_info, this must be allocated prior to calling this
 *   function.
 *
 * @return
 *  - 0 on success.
 *  - Negative on error.
 */
int get_info_vm(const char *vm_name, struct vm_info *info);

#ifdef __cplusplus
}
#endif

#endif /* CHANNEL_MANAGER_H_ */
