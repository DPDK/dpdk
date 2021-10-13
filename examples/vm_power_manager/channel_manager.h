/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef CHANNEL_MANAGER_H_
#define CHANNEL_MANAGER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <linux/limits.h>
#include <linux/un.h>
#include <stdbool.h>

/* Maximum name length including '\0' terminator */
#define CHANNEL_MGR_MAX_NAME_LEN    64

/* Hypervisor Path for libvirt(qemu/KVM) */
#define CHANNEL_MGR_DEFAULT_HV_PATH "qemu:///system"

/* File socket directory */
#define CHANNEL_MGR_SOCKET_PATH     "/tmp/powermonitor/"

/* FIFO file name template */
#define CHANNEL_MGR_FIFO_PATTERN_NAME   "fifo"

#define MAX_CLIENTS 64
#define MAX_VCPUS 20


struct libvirt_vm_info {
	const char *vm_name;
	unsigned int pcpus[MAX_VCPUS];
	uint8_t num_cpus;
};

extern struct libvirt_vm_info lvm_info[MAX_CLIENTS];
/* Communication Channel Status */
enum channel_status { CHANNEL_MGR_CHANNEL_DISCONNECTED = 0,
	CHANNEL_MGR_CHANNEL_CONNECTED,
	CHANNEL_MGR_CHANNEL_DISABLED,
	CHANNEL_MGR_CHANNEL_PROCESSING};

/* Communication Channel Type */
enum channel_type {
	CHANNEL_TYPE_BINARY = 0,
	CHANNEL_TYPE_INI,
	CHANNEL_TYPE_JSON
};

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
	enum channel_type type;      /**< Binary, ini, json, etc. */
	void *priv_info;             /**< Pointer to private info, do not modify */
};

/* Represents a single VM instance used to return internal information about
 * a VM */
struct vm_info {
	char name[CHANNEL_MGR_MAX_NAME_LEN];          /**< VM name */
	enum vm_status status;                        /**< libvirt status */
	uint16_t pcpu_map[RTE_MAX_LCORE];             /**< pCPU map to vCPU */
	unsigned num_vcpus;                           /**< number of vCPUS */
	struct channel_info channels[RTE_MAX_LCORE];  /**< channel_info array */
	unsigned num_channels;                        /**< Number of channels */
	int allow_query;                              /**< is query allowed */
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
 * Get the Physical CPU for VM lcore channel(vcpu).
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
uint16_t get_pcpu(struct channel_info *chan_info, unsigned int vcpu);

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
int set_pcpu(char *vm_name, unsigned int vcpu, unsigned int pcpu);

/**
 * Allow or disallow queries for specified VM.
 * It is thread-safe.
 *
 * @param name
 *  Virtual Machine name to lookup.
 *
 * @param allow_query
 *  Query status to be set.
 *
 * @return
 *  - 0 on success.
 *  - Negative on error.
 */
int set_query_status(char *vm_name, bool allow_query);

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
 * Set up fifos by which host applications can send command an policies
 * through a fifo to the vm_power_manager
 *
 * @return
 *  - 0 for success
 */
int add_host_channels(void);

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

/**
 * Populates a table with all domains running and their physical cpu.
 * All information is gathered through libvirt api.
 *
 * @param num_vm
 *  modified to store number of active VMs
 *
 * @param num_vcpu
    modified to store number of vcpus active
 *
 * @return
 *   void
 */
void get_all_vm(int *num_vm, int *num_vcpu);
#ifdef __cplusplus
}
#endif

#endif /* CHANNEL_MANAGER_H_ */
