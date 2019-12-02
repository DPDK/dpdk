/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#ifndef _EAL_PRIVATE_H_
#define _EAL_PRIVATE_H_

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include <rte_dev.h>
#include <rte_lcore.h>

/**
 * Structure storing internal configuration (per-lcore)
 */
struct lcore_config {
	pthread_t thread_id;       /**< pthread identifier */
	int pipe_master2slave[2];  /**< communication pipe with master */
	int pipe_slave2master[2];  /**< communication pipe with master */

	lcore_function_t * volatile f; /**< function to call */
	void * volatile arg;       /**< argument of function */
	volatile int ret;          /**< return value of function */

	volatile enum rte_lcore_state_t state; /**< lcore state */
	unsigned int socket_id;    /**< physical socket id for this lcore */
	unsigned int core_id;      /**< core number on socket for this lcore */
	int core_index;            /**< relative index, starting from 0 */
	uint8_t core_role;         /**< role of core eg: OFF, RTE, SERVICE */

	rte_cpuset_t cpuset;       /**< cpu set which the lcore affinity to */
};

extern struct lcore_config lcore_config[RTE_MAX_LCORE];

/**
 * The global RTE configuration structure.
 */
struct rte_config {
	uint32_t master_lcore;       /**< Id of the master lcore */
	uint32_t lcore_count;        /**< Number of available logical cores. */
	uint32_t numa_node_count;    /**< Number of detected NUMA nodes. */
	uint32_t numa_nodes[RTE_MAX_NUMA_NODES]; /**< List of detected NUMA nodes. */
	uint32_t service_lcore_count;/**< Number of available service cores. */
	enum rte_lcore_role_t lcore_role[RTE_MAX_LCORE]; /**< State of cores. */

	/** Primary or secondary configuration */
	enum rte_proc_type_t process_type;

	/** PA or VA mapping mode */
	enum rte_iova_mode iova_mode;

	/**
	 * Pointer to memory configuration, which may be shared across multiple
	 * DPDK instances
	 */
	struct rte_mem_config *mem_config;
} __attribute__((__packed__));

/**
 * Get the global configuration structure.
 *
 * @return
 *   A pointer to the global configuration structure.
 */
struct rte_config *rte_eal_get_configuration(void);

/**
 * Initialize the memzone subsystem (private to eal).
 *
 * @return
 *   - 0 on success
 *   - Negative on error
 */
int rte_eal_memzone_init(void);

/**
 * Common log initialization function (private to eal).  Determines
 * where log data is written when no call to rte_openlog_stream is
 * in effect.
 *
 * @param default_log
 *   The default log stream to be used.
 * @return
 *   - 0 on success
 *   - Negative on error
 */
void eal_log_set_default(FILE *default_log);

/**
 * Fill configuration with number of physical and logical processors
 *
 * This function is private to EAL.
 *
 * Parse /proc/cpuinfo to get the number of physical and logical
 * processors on the machine.
 *
 * @return
 *   0 on success, negative on error
 */
int rte_eal_cpu_init(void);

/**
 * Create memseg lists
 *
 * This function is private to EAL.
 *
 * Preallocate virtual memory.
 *
 * @return
 *   0 on success, negative on error
 */
int rte_eal_memseg_init(void);

/**
 * Map memory
 *
 * This function is private to EAL.
 *
 * Fill configuration structure with these infos, and return 0 on success.
 *
 * @return
 *   0 on success, negative on error
 */
int rte_eal_memory_init(void);

/**
 * Configure timers
 *
 * This function is private to EAL.
 *
 * Mmap memory areas used by HPET (high precision event timer) that will
 * provide our time reference, and configure the TSC frequency also for it
 * to be used as a reference.
 *
 * @return
 *   0 on success, negative on error
 */
int rte_eal_timer_init(void);

/**
 * Init the default log stream
 *
 * This function is private to EAL.
 *
 * @return
 *   0 on success, negative on error
 */
int rte_eal_log_init(const char *id, int facility);

/**
 * Save the log regexp for later
 */
int rte_log_save_regexp(const char *type, int priority);
int rte_log_save_pattern(const char *pattern, int priority);

/**
 * Init tail queues for non-EAL library structures. This is to allow
 * the rings, mempools, etc. lists to be shared among multiple processes
 *
 * This function is private to EAL
 *
 * @return
 *    0 on success, negative on error
 */
int rte_eal_tailqs_init(void);

/**
 * Init interrupt handling.
 *
 * This function is private to EAL.
 *
 * @return
 *  0 on success, negative on error
 */
int rte_eal_intr_init(void);

/**
 * Init alarm mechanism. This is to allow a callback be called after
 * specific time.
 *
 * This function is private to EAL.
 *
 * @return
 *  0 on success, negative on error
 */
int rte_eal_alarm_init(void);

/**
 * Function is to check if the kernel module(like, vfio, vfio_iommu_type1,
 * etc.) loaded.
 *
 * @param module_name
 *	The module's name which need to be checked
 *
 * @return
 *	-1 means some error happens(NULL pointer or open failure)
 *	0  means the module not loaded
 *	1  means the module loaded
 */
int rte_eal_check_module(const char *module_name);

/**
 * Get virtual area of specified size from the OS.
 *
 * This function is private to the EAL.
 *
 * @param requested_addr
 *   Address where to request address space.
 * @param size
 *   Size of requested area.
 * @param page_sz
 *   Page size on which to align requested virtual area.
 * @param flags
 *   EAL_VIRTUAL_AREA_* flags.
 * @param mmap_flags
 *   Extra flags passed directly to mmap().
 *
 * @return
 *   Virtual area address if successful.
 *   NULL if unsuccessful.
 */

#define EAL_VIRTUAL_AREA_ADDR_IS_HINT (1 << 0)
/**< don't fail if cannot get exact requested address. */
#define EAL_VIRTUAL_AREA_ALLOW_SHRINK (1 << 1)
/**< try getting smaller sized (decrement by page size) virtual areas if cannot
 * get area of requested size.
 */
#define EAL_VIRTUAL_AREA_UNMAP (1 << 2)
/**< immediately unmap reserved virtual area. */
void *
eal_get_virtual_area(void *requested_addr, size_t *size,
		size_t page_sz, int flags, int mmap_flags);

/**
 * Get cpu core_id.
 *
 * This function is private to the EAL.
 */
unsigned eal_cpu_core_id(unsigned lcore_id);

/**
 * Check if cpu is present.
 *
 * This function is private to the EAL.
 */
int eal_cpu_detected(unsigned lcore_id);

/**
 * Set TSC frequency from precise value or estimation
 *
 * This function is private to the EAL.
 */
void set_tsc_freq(void);

/**
 * Get precise TSC frequency from system
 *
 * This function is private to the EAL.
 */
uint64_t get_tsc_freq(void);

/**
 * Get TSC frequency if the architecture supports.
 *
 * This function is private to the EAL.
 *
 * @return
 *   The number of TSC cycles in one second.
 *   Returns zero if the architecture support is not available.
 */
uint64_t get_tsc_freq_arch(void);

/**
 * Prepare physical memory mapping
 * i.e. hugepages on Linux and
 *      contigmem on BSD.
 *
 * This function is private to the EAL.
 */
int rte_eal_hugepage_init(void);

/**
 * Creates memory mapping in secondary process
 * i.e. hugepages on Linux and
 *      contigmem on BSD.
 *
 * This function is private to the EAL.
 */
int rte_eal_hugepage_attach(void);

/**
 * Find a bus capable of identifying a device.
 *
 * @param str
 *   A device identifier (PCI address, virtual PMD name, ...).
 *
 * @return
 *   A valid bus handle if found.
 *   NULL if no bus is able to parse this device.
 */
struct rte_bus *rte_bus_find_by_device_name(const char *str);

/**
 * Create the unix channel for primary/secondary communication.
 *
 * @return
 *   0 on success;
 *   (<0) on failure.
 */
int rte_mp_channel_init(void);

/**
 * Primary/secondary communication cleanup.
 */
void rte_mp_channel_cleanup(void);

/**
 * @internal
 * Parse a device string and store its information in an
 * rte_devargs structure.
 *
 * A device description is split by layers of abstraction of the device:
 * bus, class and driver. Each layer will offer a set of properties that
 * can be applied either to configure or recognize a device.
 *
 * This function will parse those properties and prepare the rte_devargs
 * to be given to each layers for processing.
 *
 * Note: if the "data" field of the devargs points to devstr,
 * then no dynamic allocation is performed and the rte_devargs
 * can be safely discarded.
 *
 * Otherwise ``data`` will hold a workable copy of devstr, that will be
 * used by layers descriptors within rte_devargs. In this case,
 * any rte_devargs should be cleaned-up before being freed.
 *
 * @param da
 *   rte_devargs structure to fill.
 *
 * @param devstr
 *   Device string.
 *
 * @return
 *   0 on success.
 *   Negative errno values on error (rte_errno is set).
 */
int
rte_devargs_layers_parse(struct rte_devargs *devargs,
			 const char *devstr);

/*
 * probe a device at local process.
 *
 * @param devargs
 *   Device arguments including bus, class and driver properties.
 * @param new_dev
 *   new device be probed as output.
 * @return
 *   0 on success, negative on error.
 */
int local_dev_probe(const char *devargs, struct rte_device **new_dev);

/**
 * Hotplug remove a given device from a specific bus at local process.
 *
 * @param dev
 *   Data structure of the device to remove.
 * @return
 *   0 on success, negative on error.
 */
int local_dev_remove(struct rte_device *dev);

/**
 * Iterate over all buses to find the corresponding bus to handle the sigbus
 * error.
 * @param failure_addr
 *	Pointer of the fault address of the sigbus error.
 *
 * @return
 *	 0 success to handle the sigbus.
 *	-1 failed to handle the sigbus
 *	 1 no bus can handler the sigbus
 */
int rte_bus_sigbus_handler(const void *failure_addr);

/**
 * @internal
 * Register the sigbus handler.
 *
 * @return
 *   - On success, zero.
 *   - On failure, a negative value.
 */
int
dev_sigbus_handler_register(void);

/**
 * @internal
 * Unregister the sigbus handler.
 *
 * @return
 *   - On success, zero.
 *   - On failure, a negative value.
 */
int
dev_sigbus_handler_unregister(void);

/**
 * Check if the option is registered.
 *
 * @param option
 *  The option to be parsed.
 *
 * @return
 *  0 on success
 * @return
 *  -1 on fail
 */
int
rte_option_parse(const char *opt);

/**
 * Iterate through the registered options and execute the associated
 * callback if enabled.
 */
void
rte_option_init(void);

/**
 * Iterate through the registered options and show the associated
 * usage string.
 */
void
rte_option_usage(void);

/**
 * Get OS-specific EAL mapping base address.
 */
uint64_t
eal_get_baseaddr(void);

#endif /* _EAL_PRIVATE_H_ */
