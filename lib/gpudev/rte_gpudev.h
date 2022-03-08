/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2021 NVIDIA Corporation & Affiliates
 */

#ifndef RTE_GPUDEV_H
#define RTE_GPUDEV_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include <rte_mbuf.h>
#include <rte_bitops.h>
#include <rte_compat.h>

/**
 * @file
 * Generic library to interact with GPU computing device.
 *
 * The API is not thread-safe.
 * Device management must be done by a single thread.
 *
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 */

#ifdef __cplusplus
extern "C" {
#endif

/** Maximum number of devices if rte_gpu_init() is not called. */
#define RTE_GPU_DEFAULT_MAX 32

/** Empty device ID. */
#define RTE_GPU_ID_NONE -1
/** Catch-all device ID. */
#define RTE_GPU_ID_ANY INT16_MIN

/** Catch-all callback data. */
#define RTE_GPU_CALLBACK_ANY_DATA ((void *)-1)

/** Access variable as volatile. */
#define RTE_GPU_VOLATILE(x) (*(volatile typeof(x) *)&(x))

/** Max number of packets per communication list. */
#define RTE_GPU_COMM_LIST_PKTS_MAX 1024

/** Store device info. */
struct rte_gpu_info {
	/** Unique identifier name. */
	const char *name;
	/** Opaque handler of the device context. */
	uint64_t context;
	/** Device ID. */
	int16_t dev_id;
	/** ID of the parent device, RTE_GPU_ID_NONE if no parent */
	int16_t parent;
	/** Total processors available on device. */
	uint32_t processor_count;
	/** Total memory available on device. */
	size_t total_memory;
	/** GPU memory page size. */
	size_t page_size;
	/** Local NUMA memory ID. -1 if unknown. */
	int16_t numa_node;
};

/** Flags passed in notification callback. */
enum rte_gpu_event {
	/** Device is just initialized. */
	RTE_GPU_EVENT_NEW,
	/** Device is going to be released. */
	RTE_GPU_EVENT_DEL,
};

/** Prototype of event callback function. */
typedef void (rte_gpu_callback_t)(int16_t dev_id,
		enum rte_gpu_event event, void *user_data);

/** Memory where communication flag is allocated. */
enum rte_gpu_comm_flag_type {
	/** Allocate flag on CPU memory visible from device. */
	RTE_GPU_COMM_FLAG_CPU = 0,
};

/** Communication flag to coordinate CPU with the device. */
struct rte_gpu_comm_flag {
	/** Device that will use the device flag. */
	uint16_t dev_id;
	/** Pointer to flag memory area. */
	uint32_t *ptr;
	/** Type of memory used to allocate the flag. */
	enum rte_gpu_comm_flag_type mtype;
};

/** List of packets shared among CPU and device. */
struct rte_gpu_comm_pkt {
	/** Address of the packet in memory (e.g. mbuf->buf_addr). */
	uintptr_t addr;
	/** Size in byte of the packet. */
	size_t size;
};

/** Possible status for the list of packets shared among CPU and device. */
enum rte_gpu_comm_list_status {
	/** Packet list can be filled with new mbufs, no one is using it. */
	RTE_GPU_COMM_LIST_FREE = 0,
	/** Packet list has been filled with new mbufs and it's ready to be used .*/
	RTE_GPU_COMM_LIST_READY,
	/** Packet list has been processed, it's ready to be freed. */
	RTE_GPU_COMM_LIST_DONE,
	/** Some error occurred during packet list processing. */
	RTE_GPU_COMM_LIST_ERROR,
};

/**
 * Communication list holding a number of lists of packets
 * each having a status flag.
 */
struct rte_gpu_comm_list {
	/** Device that will use the communication list. */
	uint16_t dev_id;
	/** List of mbufs populated by the CPU with a set of mbufs. */
	struct rte_mbuf **mbufs;
	/** List of packets populated by the CPU with a set of mbufs info. */
	struct rte_gpu_comm_pkt *pkt_list;
	/** Number of packets in the list. */
	uint32_t num_pkts;
	/** Status of the list. CPU pointer. */
	enum rte_gpu_comm_list_status *status_h;
	/** Status of the list. GPU pointer. */
	enum rte_gpu_comm_list_status *status_d;
};

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Initialize the device array before probing devices.
 * If not called, the maximum of probed devices is RTE_GPU_DEFAULT_MAX.
 *
 * @param dev_max
 *   Maximum number of devices.
 *
 * @return
 *   0 on success, -rte_errno otherwise:
 *   - ENOMEM if out of memory
 *   - EINVAL if 0 size
 *   - EBUSY if already initialized
 */
__rte_experimental
int rte_gpu_init(size_t dev_max);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Return the number of GPU detected and associated to DPDK.
 *
 * @return
 *   The number of available computing devices.
 */
__rte_experimental
uint16_t rte_gpu_count_avail(void);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Check if the device is valid and initialized in DPDK.
 *
 * @param dev_id
 *   The input device ID.
 *
 * @return
 *   - True if dev_id is a valid and initialized computing device.
 *   - False otherwise.
 */
__rte_experimental
bool rte_gpu_is_valid(int16_t dev_id);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Create a virtual device representing a context in the parent device.
 *
 * @param name
 *   Unique string to identify the device.
 * @param parent
 *   Device ID of the parent.
 * @param child_context
 *   Opaque context handler.
 *
 * @return
 *   Device ID of the new created child, -rte_errno otherwise:
 *   - EINVAL if empty name
 *   - ENAMETOOLONG if long name
 *   - EEXIST if existing device name
 *   - ENODEV if invalid parent
 *   - EPERM if secondary process
 *   - ENOENT if too many devices
 *   - ENOMEM if out of space
 */
__rte_experimental
int16_t rte_gpu_add_child(const char *name,
		int16_t parent, uint64_t child_context);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Get the ID of the next valid GPU initialized in DPDK.
 *
 * @param dev_id
 *   The initial device ID to start the research.
 * @param parent
 *   The device ID of the parent.
 *   RTE_GPU_ID_NONE means no parent.
 *   RTE_GPU_ID_ANY means no or any parent.
 *
 * @return
 *   Next device ID corresponding to a valid and initialized computing device,
 *   RTE_GPU_ID_NONE if there is none.
 */
__rte_experimental
int16_t rte_gpu_find_next(int16_t dev_id, int16_t parent);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Macro to iterate over all valid GPU devices.
 *
 * @param dev_id
 *   The ID of the next possible valid device, usually 0 to iterate all.
 */
#define RTE_GPU_FOREACH(dev_id) \
	RTE_GPU_FOREACH_CHILD(dev_id, RTE_GPU_ID_ANY)

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Macro to iterate over all valid computing devices having no parent.
 *
 * @param dev_id
 *   The ID of the next possible valid device, usually 0 to iterate all.
 */
#define RTE_GPU_FOREACH_PARENT(dev_id) \
	RTE_GPU_FOREACH_CHILD(dev_id, RTE_GPU_ID_NONE)

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Macro to iterate over all valid children of a computing device parent.
 *
 * @param dev_id
 *   The ID of the next possible valid device, usually 0 to iterate all.
 * @param parent
 *   The device ID of the parent.
 */
#define RTE_GPU_FOREACH_CHILD(dev_id, parent) \
	for (dev_id = rte_gpu_find_next(0, parent); \
	     dev_id >= 0; \
	     dev_id = rte_gpu_find_next(dev_id + 1, parent))

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Close device or child context.
 * All resources are released.
 *
 * @param dev_id
 *   Device ID to close.
 *
 * @return
 *   0 on success, -rte_errno otherwise:
 *   - ENODEV if invalid dev_id
 *   - EPERM if driver error
 */
__rte_experimental
int rte_gpu_close(int16_t dev_id);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Register a function as event callback.
 * A function may be registered multiple times for different events.
 *
 * @param dev_id
 *   Device ID to get notified about.
 *   RTE_GPU_ID_ANY means all devices.
 * @param event
 *   Device event to be registered for.
 * @param function
 *   Callback function to be called on event.
 * @param user_data
 *   Optional parameter passed in the callback.
 *
 * @return
 *   0 on success, -rte_errno otherwise:
 *   - ENODEV if invalid dev_id
 *   - EINVAL if NULL function
 *   - ENOMEM if out of memory
 */
__rte_experimental
int rte_gpu_callback_register(int16_t dev_id, enum rte_gpu_event event,
		rte_gpu_callback_t *function, void *user_data);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Unregister for an event.
 *
 * @param dev_id
 *   Device ID to be silenced.
 *   RTE_GPU_ID_ANY means all devices.
 * @param event
 *   Registered event.
 * @param function
 *   Registered function.
 * @param user_data
 *   Optional parameter as registered.
 *   RTE_GPU_CALLBACK_ANY_DATA is a catch-all.
 *
 * @return
 *   0 on success, -rte_errno otherwise:
 *   - ENODEV if invalid dev_id
 *   - EINVAL if NULL function
 */
__rte_experimental
int rte_gpu_callback_unregister(int16_t dev_id, enum rte_gpu_event event,
		rte_gpu_callback_t *function, void *user_data);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Return device specific info.
 *
 * @param dev_id
 *   Device ID to get info.
 * @param info
 *   Memory structure to fill with the info.
 *
 * @return
 *   0 on success, -rte_errno otherwise:
 *   - ENODEV if invalid dev_id
 *   - EINVAL if NULL info
 *   - EPERM if driver error
 */
__rte_experimental
int rte_gpu_info_get(int16_t dev_id, struct rte_gpu_info *info);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Allocate a chunk of memory in the device.
 *
 * @param dev_id
 *   Device ID requiring allocated memory.
 * @param size
 *   Number of bytes to allocate.
 *   Requesting 0 will do nothing.
 * @param align
 *   If 0, the return is a pointer that is suitably aligned
 *   for any kind of variable (in the same manner as malloc()).
 *   Otherwise, the return is a pointer that is a multiple of *align*.
 *   In this case, it must obviously be a power of two.
 *
 * @return
 *   A pointer to the allocated memory, otherwise NULL and rte_errno is set:
 *   - ENODEV if invalid dev_id
 *   - EINVAL if align is not a power of two
 *   - ENOTSUP if operation not supported by the driver
 *   - E2BIG if size is higher than limit
 *   - ENOMEM if out of space
 *   - EPERM if driver error
 */
__rte_experimental
void *rte_gpu_mem_alloc(int16_t dev_id, size_t size, unsigned int align)
__rte_alloc_size(2);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Deallocate a chunk of memory allocated with rte_gpu_mem_alloc().
 *
 * @param dev_id
 *   Reference device ID.
 * @param ptr
 *   Pointer to the memory area to be deallocated.
 *   NULL is a no-op accepted value.
 *
 * @return
 *   0 on success, -rte_errno otherwise:
 *   - ENODEV if invalid dev_id
 *   - ENOTSUP if operation not supported by the driver
 *   - EPERM if driver error
 */
__rte_experimental
int rte_gpu_mem_free(int16_t dev_id, void *ptr);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Register a chunk of memory on the CPU usable by the device.
 *
 * @param dev_id
 *   Device ID requiring allocated memory.
 * @param size
 *   Number of bytes to allocate.
 *   Requesting 0 will do nothing.
 * @param ptr
 *   Pointer to the memory area to be registered.
 *   NULL is a no-op accepted value.

 * @return
 *   A pointer to the allocated memory, otherwise NULL and rte_errno is set:
 *   - ENODEV if invalid dev_id
 *   - EINVAL if reserved flags
 *   - ENOTSUP if operation not supported by the driver
 *   - E2BIG if size is higher than limit
 *   - ENOMEM if out of space
 *   - EPERM if driver error
 */
__rte_experimental
int rte_gpu_mem_register(int16_t dev_id, size_t size, void *ptr);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Deregister a chunk of memory previously registered with rte_gpu_mem_register()
 *
 * @param dev_id
 *   Reference device ID.
 * @param ptr
 *   Pointer to the memory area to be unregistered.
 *   NULL is a no-op accepted value.
 *
 * @return
 *   0 on success, -rte_errno otherwise:
 *   - ENODEV if invalid dev_id
 *   - ENOTSUP if operation not supported by the driver
 *   - EPERM if driver error
 */
__rte_experimental
int rte_gpu_mem_unregister(int16_t dev_id, void *ptr);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Map a chunk of GPU memory to make it accessible from the CPU
 * using the memory pointer returned by the function.
 * GPU memory has to be allocated via rte_gpu_mem_alloc().
 *
 * @param dev_id
 *   Device ID requiring mapped memory.
 * @param size
 *   Number of bytes to map.
 *   Requesting 0 will do nothing.
 * @param ptr
 *   Pointer to the GPU memory area to be mapped.
 *   NULL is a no-op accepted value.

 * @return
 *   A pointer to the mapped GPU memory usable by the CPU, otherwise NULL and rte_errno is set:
 *   - ENODEV if invalid dev_id
 *   - ENOTSUP if operation not supported by the driver
 *   - E2BIG if size is higher than limit
 *   - ENOMEM if out of space
 *   - EPERM if driver error
 */
__rte_experimental
void *rte_gpu_mem_cpu_map(int16_t dev_id, size_t size, void *ptr);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Unmap a chunk of GPU memory previously mapped with rte_gpu_mem_cpu_map()
 *
 * @param dev_id
 *   Reference device ID.
 * @param ptr
 *   Pointer to the GPU memory area to be unmapped.
 *   NULL is a no-op accepted value.
 *
 * @return
 *   0 on success, -rte_errno otherwise:
 *   - ENODEV if invalid dev_id
 *   - ENOTSUP if operation not supported by the driver
 *   - EPERM if driver error
 */
__rte_experimental
int rte_gpu_mem_cpu_unmap(int16_t dev_id, void *ptr);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Enforce a GPU write memory barrier.
 *
 * @param dev_id
 *   Reference device ID.
 *
 * @return
 *   0 on success, -rte_errno otherwise:
 *   - ENODEV if invalid dev_id
 *   - ENOTSUP if operation not supported by the driver
 *   - EPERM if driver error
 */
__rte_experimental
int rte_gpu_wmb(int16_t dev_id);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Create a communication flag that can be shared
 * between CPU threads and device workload to exchange some status info
 * (e.g. work is done, processing can start, etc..).
 *
 * @param dev_id
 *   Reference device ID.
 * @param devflag
 *   Pointer to the memory area of the devflag structure.
 * @param mtype
 *   Type of memory to allocate the communication flag.
 *
 * @return
 *   0 on success, -rte_errno otherwise:
 *   - ENODEV if invalid dev_id
 *   - EINVAL if invalid inputs
 *   - ENOTSUP if operation not supported by the driver
 *   - ENOMEM if out of space
 *   - EPERM if driver error
 */
__rte_experimental
int rte_gpu_comm_create_flag(uint16_t dev_id,
		struct rte_gpu_comm_flag *devflag,
		enum rte_gpu_comm_flag_type mtype);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Deallocate a communication flag.
 *
 * @param devflag
 *   Pointer to the memory area of the devflag structure.
 *
 * @return
 *   0 on success, -rte_errno otherwise:
 *   - ENODEV if invalid dev_id
 *   - EINVAL if NULL devflag
 *   - ENOTSUP if operation not supported by the driver
 *   - EPERM if driver error
 */
__rte_experimental
int rte_gpu_comm_destroy_flag(struct rte_gpu_comm_flag *devflag);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Set the value of a communication flag as the input value.
 * Flag memory area is treated as volatile.
 * The flag must have been allocated with RTE_GPU_COMM_FLAG_CPU.
 *
 * @param devflag
 *   Pointer to the memory area of the devflag structure.
 * @param val
 *   Value to set in the flag.
 *
 * @return
 *   0 on success, -rte_errno otherwise:
 *   - EINVAL if invalid input params
 */
__rte_experimental
int rte_gpu_comm_set_flag(struct rte_gpu_comm_flag *devflag,
		uint32_t val);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Get the value of the communication flag.
 * Flag memory area is treated as volatile.
 * The flag must have been allocated with RTE_GPU_COMM_FLAG_CPU.
 *
 * @param devflag
 *   Pointer to the memory area of the devflag structure.
 * @param val
 *   Flag output value.
 *
 * @return
 *   0 on success, -rte_errno otherwise:
 *   - EINVAL if invalid input params
 */
__rte_experimental
int rte_gpu_comm_get_flag_value(struct rte_gpu_comm_flag *devflag,
		uint32_t *val);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Create a communication list that can be used to share packets
 * between CPU and device.
 * Each element of the list contains:
 *  - a packet list of RTE_GPU_COMM_LIST_PKTS_MAX elements
 *  - number of packets in the list
 *  - a status flag to communicate if the packet list is FREE,
 *    READY to be processed, DONE with processing.
 *
 * The list is allocated in CPU-visible memory.
 * At creation time, every list is in FREE state.
 *
 * @param dev_id
 *   Reference device ID.
 * @param num_comm_items
 *   Number of items in the communication list.
 *
 * @return
 *   A pointer to the allocated list, otherwise NULL and rte_errno is set:
 *   - EINVAL if invalid input params
 */
__rte_experimental
struct rte_gpu_comm_list *rte_gpu_comm_create_list(uint16_t dev_id,
		uint32_t num_comm_items);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Destroy a communication list.
 *
 * @param comm_list
 *   Communication list to be destroyed.
 * @param num_comm_items
 *   Number of items in the communication list.
 *
 * @return
 *   0 on success, -rte_errno otherwise:
 *   - EINVAL if invalid input params
 */
__rte_experimental
int rte_gpu_comm_destroy_list(struct rte_gpu_comm_list *comm_list,
		uint32_t num_comm_items);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Populate the packets list of the communication item
 * with info from a list of mbufs.
 * Status flag of that packet list is set to READY.
 *
 * @param comm_list_item
 *   Communication list item to fill.
 * @param mbufs
 *   List of mbufs.
 * @param num_mbufs
 *   Number of mbufs.
 *
 * @return
 *   0 on success, -rte_errno otherwise:
 *   - EINVAL if invalid input params
 *   - ENOTSUP if mbufs are chained (multiple segments)
 */
__rte_experimental
int rte_gpu_comm_populate_list_pkts(struct rte_gpu_comm_list *comm_list_item,
		struct rte_mbuf **mbufs, uint32_t num_mbufs);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Set status flag value of a communication list item.
 *
 * @param comm_list_item
 *   Communication list item to query.
 * @param status
 *   Status value to set.
 *
 * @return
 *   0 on success, -rte_errno otherwise:
 *   - EINVAL if invalid input params
 */
__rte_experimental
int rte_gpu_comm_set_status(struct rte_gpu_comm_list *comm_list_item,
		enum rte_gpu_comm_list_status status);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Get status flag value of a communication list item.
 *
 * @param comm_list_item
 *   Communication list item to query.
 *   Input parameter.
 * @param status
 *   Communication list item status flag value.
 *   Output parameter.
 *
 * @return
 *   0 on success, -rte_errno otherwise:
 *   - EINVAL if invalid input params
 */
__rte_experimental
int rte_gpu_comm_get_status(struct rte_gpu_comm_list *comm_list_item,
		enum rte_gpu_comm_list_status *status);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Reset a communication list item to the original state.
 * The status flag set to FREE and mbufs are returned to the pool.
 *
 * @param comm_list_item
 *   Communication list item to reset.
 *
 * @return
 *   0 on success, -rte_errno otherwise:
 *   - EINVAL if invalid input params
 */
__rte_experimental
int rte_gpu_comm_cleanup_list(struct rte_gpu_comm_list *comm_list_item);

#ifdef __cplusplus
}
#endif

#endif /* RTE_GPUDEV_H */
