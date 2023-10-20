/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef _RTE_INTERRUPTS_H_
#define _RTE_INTERRUPTS_H_

#include <stdbool.h>

#include <rte_bitops.h>
#include <rte_common.h>
#include <rte_compat.h>
#include <rte_epoll.h>

/**
 * @file
 *
 * The RTE interrupt interface provides functions to register/unregister
 * callbacks for a specific interrupt.
 */

#ifdef __cplusplus
extern "C" {
#endif

/** Interrupt handle */
struct rte_intr_handle;

/** Interrupt instance allocation flags
 * @see rte_intr_instance_alloc
 */

/** Interrupt instance will not be shared between primary and secondary processes. */
#define RTE_INTR_INSTANCE_F_PRIVATE     UINT32_C(0)
/** Interrupt instance will be shared between primary and secondary processes. */
#define RTE_INTR_INSTANCE_F_SHARED      RTE_BIT32(0)

#define RTE_MAX_RXTX_INTR_VEC_ID      512
#define RTE_INTR_VEC_ZERO_OFFSET      0
#define RTE_INTR_VEC_RXTX_OFFSET      1

/**
 * The interrupt source type, e.g. UIO, VFIO, ALARM etc.
 */
enum rte_intr_handle_type {
	RTE_INTR_HANDLE_UNKNOWN = 0,  /**< generic unknown handle */
	RTE_INTR_HANDLE_UIO,          /**< uio device handle */
	RTE_INTR_HANDLE_UIO_INTX,     /**< uio generic handle */
	RTE_INTR_HANDLE_VFIO_LEGACY,  /**< vfio device handle (legacy) */
	RTE_INTR_HANDLE_VFIO_MSI,     /**< vfio device handle (MSI) */
	RTE_INTR_HANDLE_VFIO_MSIX,    /**< vfio device handle (MSIX) */
	RTE_INTR_HANDLE_ALARM,        /**< alarm handle */
	RTE_INTR_HANDLE_EXT,          /**< external handler */
	RTE_INTR_HANDLE_VDEV,         /**< virtual device */
	RTE_INTR_HANDLE_DEV_EVENT,    /**< device event handle */
	RTE_INTR_HANDLE_VFIO_REQ,     /**< VFIO request handle */
	RTE_INTR_HANDLE_MAX           /**< count of elements */
};

/** Function to be registered for the specific interrupt */
typedef void (*rte_intr_callback_fn)(void *cb_arg);

/**
 * Function to call after a callback is unregistered.
 * Can be used to close fd and free cb_arg.
 */
typedef void (*rte_intr_unregister_callback_fn)(struct rte_intr_handle *intr_handle,
						void *cb_arg);

/**
 * It registers the callback for the specific interrupt. Multiple
 * callbacks can be registered at the same time.
 * @param intr_handle
 *  Pointer to the interrupt handle.
 * @param cb
 *  callback address.
 * @param cb_arg
 *  address of parameter for callback.
 *
 * @return
 *  - On success, zero.
 *  - On failure, a negative value.
 */
int rte_intr_callback_register(const struct rte_intr_handle *intr_handle,
				rte_intr_callback_fn cb, void *cb_arg);

/**
 * It unregisters the callback according to the specified interrupt handle.
 *
 * @param intr_handle
 *  pointer to the interrupt handle.
 * @param cb
 *  callback address.
 * @param cb_arg
 *  address of parameter for callback, (void *)-1 means to remove all
 *  registered which has the same callback address.
 *
 * @return
 *  - On success, return the number of callback entities removed.
 *  - On failure, a negative value.
 */
int rte_intr_callback_unregister(const struct rte_intr_handle *intr_handle,
				rte_intr_callback_fn cb, void *cb_arg);

/**
 * Unregister the callback according to the specified interrupt handle,
 * after it's no longer active. Fail if source is not active.
 *
 * @param intr_handle
 *  pointer to the interrupt handle.
 * @param cb_fn
 *  callback address.
 * @param cb_arg
 *  address of parameter for callback, (void *)-1 means to remove all
 *  registered which has the same callback address.
 * @param ucb_fn
 *  callback to call before cb is unregistered (optional).
 *  can be used to close fd and free cb_arg.
 *
 * @return
 *  - On success, return the number of callback entities marked for remove.
 *  - On failure, a negative value.
 */
int
rte_intr_callback_unregister_pending(const struct rte_intr_handle *intr_handle,
				rte_intr_callback_fn cb_fn, void *cb_arg,
				rte_intr_unregister_callback_fn ucb_fn);

/**
 * Loop until rte_intr_callback_unregister() succeeds.
 * After a call to this function,
 * the callback provided by the specified interrupt handle is unregistered.
 *
 * @param intr_handle
 *  pointer to the interrupt handle.
 * @param cb
 *  callback address.
 * @param cb_arg
 *  address of parameter for callback, (void *)-1 means to remove all
 *  registered which has the same callback address.
 *
 * @return
 *  - On success, return the number of callback entities removed.
 *  - On failure, a negative value.
 */
int
rte_intr_callback_unregister_sync(const struct rte_intr_handle *intr_handle,
				rte_intr_callback_fn cb, void *cb_arg);

/**
 * It enables the interrupt for the specified handle.
 *
 * @param intr_handle
 *  pointer to the interrupt handle.
 *
 * @return
 *  - On success, zero.
 *  - On failure, a negative value.
 */
int rte_intr_enable(const struct rte_intr_handle *intr_handle);

/**
 * It disables the interrupt for the specified handle.
 *
 * @param intr_handle
 *  pointer to the interrupt handle.
 *
 * @return
 *  - On success, zero.
 *  - On failure, a negative value.
 */
int rte_intr_disable(const struct rte_intr_handle *intr_handle);

/**
 * It acknowledges an interrupt raised for the specified handle.
 *
 * This function should be called at the end of each interrupt handler either
 * from application or driver, so that currently raised interrupt is acked and
 * further new interrupts are raised.
 *
 * @param intr_handle
 *  pointer to the interrupt handle.
 *
 * @return
 *  - On success, zero.
 *  - On failure, a negative value.
 */
int rte_intr_ack(const struct rte_intr_handle *intr_handle);

/**
 * Check if currently executing in interrupt context
 *
 * @return
 *  - non zero in case of interrupt context
 *  - zero in case of process context
 */
int rte_thread_is_intr(void);

/**
 * It allocates memory for interrupt instance. API takes flag as an argument
 * which define from where memory should be allocated i.e. using DPDK memory
 * management library APIs or normal heap allocation.
 * Default memory allocation for event fds and event list array is done which
 * can be realloced later based on size of MSIX interrupts supported by a PCI
 * device.
 *
 * This function should be called from application or driver, before calling
 * any of the interrupt APIs.
 *
 * @param flags
 *  See RTE_INTR_INSTANCE_F_* flags definitions.
 *
 * @return
 *  - On success, address of interrupt handle.
 *  - On failure, NULL.
 */
struct rte_intr_handle *
rte_intr_instance_alloc(uint32_t flags);

/**
 * Free the memory allocated for interrupt handle resources.
 *
 * @param intr_handle
 *  Interrupt handle allocated with rte_intr_instance_alloc().
 *  If intr_handle is NULL, no operation is performed.
 */
void
rte_intr_instance_free(struct rte_intr_handle *intr_handle);

/**
 * Set the fd field of interrupt handle with user provided
 * file descriptor.
 *
 * @param intr_handle
 *  pointer to the interrupt handle.
 * @param fd
 *  file descriptor value provided by user.
 *
 * @return
 *  - On success, zero.
 *  - On failure, a negative value and rte_errno is set.
 */
int
rte_intr_fd_set(struct rte_intr_handle *intr_handle, int fd);

/**
 * Returns the fd field of the given interrupt handle instance.
 *
 * @param intr_handle
 *  pointer to the interrupt handle.
 *
 * @return
 *  - On success, fd field.
 *  - On failure, a negative value.
 */
int
rte_intr_fd_get(const struct rte_intr_handle *intr_handle);

/**
 * Set the type field of interrupt handle with user provided
 * interrupt type.
 *
 * @param intr_handle
 *  pointer to the interrupt handle.
 * @param type
 *  interrupt type
 *
 * @return
 *  - On success, zero.
 *  - On failure, a negative value and rte_errno is set.
 */
int
rte_intr_type_set(struct rte_intr_handle *intr_handle,
		  enum rte_intr_handle_type type);

/**
 * Returns the type field of the given interrupt handle instance.
 *
 * @param intr_handle
 *  pointer to the interrupt handle.
 *
 * @return
 *  - On success, interrupt type
 *  - On failure, RTE_INTR_HANDLE_UNKNOWN.
 */
enum rte_intr_handle_type
rte_intr_type_get(const struct rte_intr_handle *intr_handle);

/**
 * @internal
 * The function returns the per thread epoll instance.
 *
 * @return
 *   epfd the epoll instance referred to.
 */
__rte_internal
int
rte_intr_tls_epfd(void);

/**
 * @internal
 * @param intr_handle
 *   Pointer to the interrupt handle.
 * @param epfd
 *   Epoll instance fd which the intr vector associated to.
 * @param op
 *   The operation be performed for the vector.
 *   Operation type of {ADD, DEL}.
 * @param vec
 *   RX intr vector number added to the epoll instance wait list.
 * @param data
 *   User raw data.
 * @return
 *   - On success, zero.
 *   - On failure, a negative value.
 */
__rte_internal
int
rte_intr_rx_ctl(struct rte_intr_handle *intr_handle,
		int epfd, int op, unsigned int vec, void *data);

/**
 * @internal
 * It deletes registered eventfds.
 *
 * @param intr_handle
 *   Pointer to the interrupt handle.
 */
__rte_internal
void
rte_intr_free_epoll_fd(struct rte_intr_handle *intr_handle);

/**
 * @internal
 * It enables the packet I/O interrupt event if it's necessary.
 * It creates event fd for each interrupt vector when MSIX is used,
 * otherwise it multiplexes a single event fd.
 *
 * @param intr_handle
 *   Pointer to the interrupt handle.
 * @param nb_efd
 *   Number of interrupt vector trying to enable.
 *   The value 0 is not allowed.
 * @return
 *   - On success, zero.
 *   - On failure, a negative value.
 */
__rte_internal
int
rte_intr_efd_enable(struct rte_intr_handle *intr_handle, uint32_t nb_efd);

/**
 * @internal
 * It disables the packet I/O interrupt event.
 * It deletes registered eventfds and closes the open fds.
 *
 * @param intr_handle
 *   Pointer to the interrupt handle.
 */
__rte_internal
void
rte_intr_efd_disable(struct rte_intr_handle *intr_handle);

/**
 * @internal
 * The packet I/O interrupt on datapath is enabled or not.
 *
 * @param intr_handle
 *   Pointer to the interrupt handle.
 */
__rte_internal
int
rte_intr_dp_is_en(struct rte_intr_handle *intr_handle);

/**
 * @internal
 * The interrupt handle instance allows other causes or not.
 * Other causes stand for any none packet I/O interrupts.
 *
 * @param intr_handle
 *   Pointer to the interrupt handle.
 */
__rte_internal
int
rte_intr_allow_others(struct rte_intr_handle *intr_handle);

/**
 * @internal
 * The multiple interrupt vector capability of interrupt handle instance.
 * It returns zero if no multiple interrupt vector support.
 *
 * @param intr_handle
 *   Pointer to the interrupt handle.
 */
__rte_internal
int
rte_intr_cap_multiple(struct rte_intr_handle *intr_handle);

/**
 * @internal
 * Creates a clone of src by allocating a new handle and copying src content.
 *
 * @param src
 *  Source interrupt handle to be cloned.
 *
 * @return
 *  - On success, address of interrupt handle.
 *  - On failure, NULL.
 */
__rte_internal
struct rte_intr_handle *
rte_intr_instance_dup(const struct rte_intr_handle *src);

/**
 * @internal
 * Set the device fd field of interrupt handle with user
 * provided dev fd. Device fd corresponds to VFIO device fd or UIO config fd.
 *
 * @param intr_handle
 *  pointer to the interrupt handle.
 * @param fd
 *  interrupt type
 *
 * @return
 *  - On success, zero.
 *  - On failure, a negative value and rte_errno is set.
 */
__rte_internal
int
rte_intr_dev_fd_set(struct rte_intr_handle *intr_handle, int fd);

/**
 * @internal
 * Returns the device fd field of the given interrupt handle instance.
 *
 * @param intr_handle
 *  pointer to the interrupt handle.
 *
 * @return
 *  - On success, dev fd.
 *  - On failure, a negative value and rte_errno is set.
 */
__rte_internal
int
rte_intr_dev_fd_get(const struct rte_intr_handle *intr_handle);

/**
 * @internal
 * Set the max intr field of interrupt handle with user
 * provided max intr value.
 *
 * @param intr_handle
 *  pointer to the interrupt handle.
 * @param max_intr
 *  interrupt type
 *
 * @return
 *  - On success, zero.
 *  - On failure, a negative value and rte_errno is set.
 */
__rte_internal
int
rte_intr_max_intr_set(struct rte_intr_handle *intr_handle, int max_intr);

/**
 * @internal
 * Returns the max intr field of the given interrupt handle instance.
 *
 * @param intr_handle
 *  pointer to the interrupt handle.
 *
 * @return
 *  - On success, max intr.
 *  - On failure, a negative value and rte_errno is set.
 */
__rte_internal
int
rte_intr_max_intr_get(const struct rte_intr_handle *intr_handle);

/**
 * @internal
 * Set the number of event fd field of interrupt handle
 * with user provided available event file descriptor value.
 *
 * @param intr_handle
 *  pointer to the interrupt handle.
 * @param nb_efd
 *  Available event fd
 *
 * @return
 *  - On success, zero.
 *  - On failure, a negative value and rte_errno is set.
 */
__rte_internal
int
rte_intr_nb_efd_set(struct rte_intr_handle *intr_handle, int nb_efd);

/**
 * @internal
 * Returns the number of available event fd field of the given interrupt handle
 * instance.
 *
 * @param intr_handle
 *  pointer to the interrupt handle.
 *
 * @return
 *  - On success, nb_efd
 *  - On failure, a negative value and rte_errno is set.
 */
__rte_internal
int
rte_intr_nb_efd_get(const struct rte_intr_handle *intr_handle);

/**
 * @internal
 * Returns the number of interrupt vector field of the given interrupt handle
 * instance. This field is to configured on device probe time, and based on
 * this value efds and elist arrays are dynamically allocated. By default
 * this value is set to RTE_MAX_RXTX_INTR_VEC_ID.
 * For eg. in case of PCI device, its msix size is queried and efds/elist
 * arrays are allocated accordingly.
 *
 * @param intr_handle
 *  pointer to the interrupt handle.
 *
 * @return
 *  - On success, nb_intr
 *  - On failure, a negative value and rte_errno is set.
 */
__rte_internal
int
rte_intr_nb_intr_get(const struct rte_intr_handle *intr_handle);

/**
 * @internal
 * Set the event fd counter size field of interrupt handle
 * with user provided efd counter size.
 *
 * @param intr_handle
 *  pointer to the interrupt handle.
 * @param efd_counter_size
 *  size of efd counter.
 *
 * @return
 *  - On success, zero.
 *  - On failure, a negative value and rte_errno is set.
 */
__rte_internal
int
rte_intr_efd_counter_size_set(struct rte_intr_handle *intr_handle,
			      uint8_t efd_counter_size);

/**
 * @internal
 * Returns the event fd counter size field of the given interrupt handle
 * instance.
 *
 * @param intr_handle
 *  pointer to the interrupt handle.
 *
 * @return
 *  - On success, efd_counter_size
 *  - On failure, a negative value and rte_errno is set.
 */
__rte_internal
int
rte_intr_efd_counter_size_get(const struct rte_intr_handle *intr_handle);

/**
 * @internal
 * Set the event fd array index with the given fd.
 *
 * @param intr_handle
 *  pointer to the interrupt handle.
 * @param index
 *  efds array index to be set
 * @param fd
 *  event fd
 *
 * @return
 *  - On success, zero.
 *  - On failure, a negative value and rte_errno is set.
 */
__rte_internal
int
rte_intr_efds_index_set(struct rte_intr_handle *intr_handle, int index, int fd);

/**
 * @internal
 * Returns the fd value of event fds array at a given index.
 *
 * @param intr_handle
 *  pointer to the interrupt handle.
 * @param index
 *  efds array index to be returned
 *
 * @return
 *  - On success, fd
 *  - On failure, a negative value and rte_errno is set.
 */
__rte_internal
int
rte_intr_efds_index_get(const struct rte_intr_handle *intr_handle, int index);

/**
 * @internal
 * Set the epoll event object array index with the given
 * elist instance.
 *
 * @param intr_handle
 *  pointer to the interrupt handle.
 * @param index
 *  elist array index to be set
 * @param elist
 *  epoll event instance of struct rte_epoll_event
 *
 * @return
 *  - On success, zero.
 *  - On failure, a negative value and rte_errno is set.
 */
__rte_internal
int
rte_intr_elist_index_set(struct rte_intr_handle *intr_handle, int index,
			 struct rte_epoll_event elist);

/**
 * @internal
 * Returns the address of epoll event instance from elist array at a given
 * index.
 *
 * @param intr_handle
 *  pointer to the interrupt handle.
 * @param index
 *  elist array index to be returned
 *
 * @return
 *  - On success, elist
 *  - On failure, a negative value and rte_errno is set.
 */
__rte_internal
struct rte_epoll_event *
rte_intr_elist_index_get(struct rte_intr_handle *intr_handle, int index);

/**
 * @internal
 * Allocates the memory of interrupt vector list array, with size defining the
 * number of elements required in the array.
 *
 * @param intr_handle
 *  pointer to the interrupt handle.
 * @param name
 *  Name assigned to the allocation, or NULL.
 * @param size
 *  Number of element required in the array.
 *
 * @return
 *  - On success, zero
 *  - On failure, a negative value and rte_errno is set.
 */
__rte_internal
int
rte_intr_vec_list_alloc(struct rte_intr_handle *intr_handle, const char *name,
			int size);

/**
 * @internal
 * Sets the vector value at given index of interrupt vector list field of given
 * interrupt handle.
 *
 * @param intr_handle
 *  pointer to the interrupt handle.
 * @param index
 *  intr_vec array index to be set
 * @param vec
 *  Interrupt vector value.
 *
 * @return
 *  - On success, zero
 *  - On failure, a negative value and rte_errno is set.
 */
__rte_internal
int
rte_intr_vec_list_index_set(struct rte_intr_handle *intr_handle, int index,
			    int vec);

/**
 * @internal
 * Returns the vector value at the given index of interrupt vector list array.
 *
 * @param intr_handle
 *  pointer to the interrupt handle.
 * @param index
 *  intr_vec array index to be returned
 *
 * @return
 *  - On success, interrupt vector
 *  - On failure, a negative value and rte_errno is set.
 */
__rte_internal
int
rte_intr_vec_list_index_get(const struct rte_intr_handle *intr_handle,
			    int index);

/**
 * @internal
 * Frees the memory allocated for interrupt vector list array.
 *
 * @param intr_handle
 *  pointer to the interrupt handle.
 *
 * @return
 *  - On success, zero
 *  - On failure, a negative value and rte_errno is set.
 */
__rte_internal
void
rte_intr_vec_list_free(struct rte_intr_handle *intr_handle);

/**
 * @internal
 * Reallocates the size efds and elist array based on size provided by user.
 * By default efds and elist array are allocated with default size
 * RTE_MAX_RXTX_INTR_VEC_ID on interrupt handle array creation. Later on device
 * probe, device may have capability of more interrupts than
 * RTE_MAX_RXTX_INTR_VEC_ID. Using this API, PMDs can reallocate the arrays as
 * per the max interrupts capability of device.
 *
 * @param intr_handle
 *  pointer to the interrupt handle.
 * @param size
 *  efds and elist array size.
 *
 * @return
 *  - On success, zero
 *  - On failure, a negative value and rte_errno is set.
 */
__rte_internal
int
rte_intr_event_list_update(struct rte_intr_handle *intr_handle, int size);

/**
 * @internal
 * Returns the Windows handle of the given interrupt instance.
 *
 * @param intr_handle
 *  pointer to the interrupt handle.
 *
 * @return
 *  - On success, Windows handle.
 *  - On failure, NULL.
 */
__rte_internal
void *
rte_intr_instance_windows_handle_get(struct rte_intr_handle *intr_handle);

/**
 * @internal
 * Set the Windows handle for the given interrupt instance.
 *
 * @param intr_handle
 *  pointer to the interrupt handle.
 * @param windows_handle
 *  Windows handle to be set.
 *
 * @return
 *  - On success, zero
 *  - On failure, a negative value and rte_errno is set.
 */
__rte_internal
int
rte_intr_instance_windows_handle_set(struct rte_intr_handle *intr_handle,
				     void *windows_handle);

#ifdef __cplusplus
}
#endif

#endif
