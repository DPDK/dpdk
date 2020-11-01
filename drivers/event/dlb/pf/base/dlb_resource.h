/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2020 Intel Corporation
 */

#ifndef __DLB_RESOURCE_H
#define __DLB_RESOURCE_H

#include "dlb_hw_types.h"
#include "dlb_osdep_types.h"

/**
 * dlb_resource_init() - initialize the device
 * @hw: pointer to struct dlb_hw.
 *
 * This function initializes the device's software state (pointed to by the hw
 * argument) and programs global scheduling QoS registers. This function should
 * be called during driver initialization.
 *
 * The dlb_hw struct must be unique per DLB device and persist until the device
 * is reset.
 *
 * Return:
 * Returns 0 upon success, -1 otherwise.
 */
int dlb_resource_init(struct dlb_hw *hw);

/**
 * dlb_resource_free() - free device state memory
 * @hw: dlb_hw handle for a particular device.
 *
 * This function frees software state pointed to by dlb_hw. This function
 * should be called when resetting the device or unloading the driver.
 */
void dlb_resource_free(struct dlb_hw *hw);

/**
 * dlb_resource_reset() - reset in-use resources to their initial state
 * @hw: dlb_hw handle for a particular device.
 *
 * This function resets in-use resources, and makes them available for use.
 */
void dlb_resource_reset(struct dlb_hw *hw);

/**
 * dlb_hw_create_sched_domain() - create a scheduling domain
 * @hw: dlb_hw handle for a particular device.
 * @args: scheduling domain creation arguments.
 * @resp: response structure.
 *
 * This function creates a scheduling domain containing the resources specified
 * in args. The individual resources (queues, ports, credit pools) can be
 * configured after creating a scheduling domain.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise. If an error occurs, resp->status is
 * assigned a detailed error code from enum dlb_error. If successful, resp->id
 * contains the domain ID.
 *
 * Errors:
 * EINVAL - A requested resource is unavailable, or the requested domain name
 *	    is already in use.
 * EFAULT - Internal error (resp->status not set).
 */
int dlb_hw_create_sched_domain(struct dlb_hw *hw,
			       struct dlb_create_sched_domain_args *args,
			       struct dlb_cmd_response *resp);

/**
 * dlb_hw_create_ldb_pool() - create a load-balanced credit pool
 * @hw: dlb_hw handle for a particular device.
 * @domain_id: domain ID.
 * @args: credit pool creation arguments.
 * @resp: response structure.
 *
 * This function creates a load-balanced credit pool containing the number of
 * requested credits.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise. If an error occurs, resp->status is
 * assigned a detailed error code from enum dlb_error. If successful, resp->id
 * contains the pool ID.
 *
 * Errors:
 * EINVAL - A requested resource is unavailable, the domain is not configured,
 *	    or the domain has already been started.
 * EFAULT - Internal error (resp->status not set).
 */
int dlb_hw_create_ldb_pool(struct dlb_hw *hw,
			   u32 domain_id,
			   struct dlb_create_ldb_pool_args *args,
			   struct dlb_cmd_response *resp);

/**
 * dlb_hw_create_dir_pool() - create a directed credit pool
 * @hw: dlb_hw handle for a particular device.
 * @domain_id: domain ID.
 * @args: credit pool creation arguments.
 * @resp: response structure.
 *
 * This function creates a directed credit pool containing the number of
 * requested credits.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise. If an error occurs, resp->status is
 * assigned a detailed error code from enum dlb_error. If successful, resp->id
 * contains the pool ID.
 *
 * Errors:
 * EINVAL - A requested resource is unavailable, the domain is not configured,
 *	    or the domain has already been started.
 * EFAULT - Internal error (resp->status not set).
 */
int dlb_hw_create_dir_pool(struct dlb_hw *hw,
			   u32 domain_id,
			   struct dlb_create_dir_pool_args *args,
			   struct dlb_cmd_response *resp);

/**
 * dlb_hw_create_ldb_queue() - create a load-balanced queue
 * @hw: dlb_hw handle for a particular device.
 * @domain_id: domain ID.
 * @args: queue creation arguments.
 * @resp: response structure.
 *
 * This function creates a load-balanced queue.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise. If an error occurs, resp->status is
 * assigned a detailed error code from enum dlb_error. If successful, resp->id
 * contains the queue ID.
 *
 * Errors:
 * EINVAL - A requested resource is unavailable, the domain is not configured,
 *	    the domain has already been started, or the requested queue name is
 *	    already in use.
 * EFAULT - Internal error (resp->status not set).
 */
int dlb_hw_create_ldb_queue(struct dlb_hw *hw,
			    u32 domain_id,
			    struct dlb_create_ldb_queue_args *args,
			    struct dlb_cmd_response *resp);

/**
 * dlb_hw_create_dir_queue() - create a directed queue
 * @hw: dlb_hw handle for a particular device.
 * @domain_id: domain ID.
 * @args: queue creation arguments.
 * @resp: response structure.
 *
 * This function creates a directed queue.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise. If an error occurs, resp->status is
 * assigned a detailed error code from enum dlb_error. If successful, resp->id
 * contains the queue ID.
 *
 * Errors:
 * EINVAL - A requested resource is unavailable, the domain is not configured,
 *	    or the domain has already been started.
 * EFAULT - Internal error (resp->status not set).
 */
int dlb_hw_create_dir_queue(struct dlb_hw *hw,
			    u32 domain_id,
			    struct dlb_create_dir_queue_args *args,
			    struct dlb_cmd_response *resp);

/**
 * dlb_hw_create_dir_port() - create a directed port
 * @hw: dlb_hw handle for a particular device.
 * @domain_id: domain ID.
 * @args: port creation arguments.
 * @pop_count_dma_base: base address of the pop count memory. This can be
 *			a PA or an IOVA.
 * @cq_dma_base: base address of the CQ memory. This can be a PA or an IOVA.
 * @resp: response structure.
 *
 * This function creates a directed port.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise. If an error occurs, resp->status is
 * assigned a detailed error code from enum dlb_error. If successful, resp->id
 * contains the port ID.
 *
 * Errors:
 * EINVAL - A requested resource is unavailable, a credit setting is invalid, a
 *	    pool ID is invalid, a pointer address is not properly aligned, the
 *	    domain is not configured, or the domain has already been started.
 * EFAULT - Internal error (resp->status not set).
 */
int dlb_hw_create_dir_port(struct dlb_hw *hw,
			   u32 domain_id,
			   struct dlb_create_dir_port_args *args,
			   u64 pop_count_dma_base,
			   u64 cq_dma_base,
			   struct dlb_cmd_response *resp);

/**
 * dlb_hw_create_ldb_port() - create a load-balanced port
 * @hw: dlb_hw handle for a particular device.
 * @domain_id: domain ID.
 * @args: port creation arguments.
 * @pop_count_dma_base: base address of the pop count memory. This can be
 *			 a PA or an IOVA.
 * @cq_dma_base: base address of the CQ memory. This can be a PA or an IOVA.
 * @resp: response structure.
 *
 * This function creates a load-balanced port.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise. If an error occurs, resp->status is
 * assigned a detailed error code from enum dlb_error. If successful, resp->id
 * contains the port ID.
 *
 * Errors:
 * EINVAL - A requested resource is unavailable, a credit setting is invalid, a
 *	    pool ID is invalid, a pointer address is not properly aligned, the
 *	    domain is not configured, or the domain has already been started.
 * EFAULT - Internal error (resp->status not set).
 */
int dlb_hw_create_ldb_port(struct dlb_hw *hw,
			   u32 domain_id,
			   struct dlb_create_ldb_port_args *args,
			   u64 pop_count_dma_base,
			   u64 cq_dma_base,
			   struct dlb_cmd_response *resp);

/**
 * dlb_hw_start_domain() - start a scheduling domain
 * @hw: dlb_hw handle for a particular device.
 * @domain_id: domain ID.
 * @args: start domain arguments.
 * @resp: response structure.
 *
 * This function starts a scheduling domain, which allows applications to send
 * traffic through it. Once a domain is started, its resources can no longer be
 * configured (besides QID remapping and port enable/disable).
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise. If an error occurs, resp->status is
 * assigned a detailed error code from enum dlb_error.
 *
 * Errors:
 * EINVAL - the domain is not configured, or the domain is already started.
 */
int dlb_hw_start_domain(struct dlb_hw *hw,
			u32 domain_id,
			struct dlb_start_domain_args *args,
			struct dlb_cmd_response *resp);

/**
 * dlb_hw_map_qid() - map a load-balanced queue to a load-balanced port
 * @hw: dlb_hw handle for a particular device.
 * @domain_id: domain ID.
 * @args: map QID arguments.
 * @resp: response structure.
 *
 * This function configures the DLB to schedule QEs from the specified queue to
 * the specified port. Each load-balanced port can be mapped to up to 8 queues;
 * each load-balanced queue can potentially map to all the load-balanced ports.
 *
 * A successful return does not necessarily mean the mapping was configured. If
 * this function is unable to immediately map the queue to the port, it will
 * add the requested operation to a per-port list of pending map/unmap
 * operations, and (if it's not already running) launch a kernel thread that
 * periodically attempts to process all pending operations. In a sense, this is
 * an asynchronous function.
 *
 * This asynchronicity creates two views of the state of hardware: the actual
 * hardware state and the requested state (as if every request completed
 * immediately). If there are any pending map/unmap operations, the requested
 * state will differ from the actual state. All validation is performed with
 * respect to the pending state; for instance, if there are 8 pending map
 * operations for port X, a request for a 9th will fail because a load-balanced
 * port can only map up to 8 queues.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise. If an error occurs, resp->status is
 * assigned a detailed error code from enum dlb_error.
 *
 * Errors:
 * EINVAL - A requested resource is unavailable, invalid port or queue ID, or
 *	    the domain is not configured.
 * EFAULT - Internal error (resp->status not set).
 */
int dlb_hw_map_qid(struct dlb_hw *hw,
		   u32 domain_id,
		   struct dlb_map_qid_args *args,
		   struct dlb_cmd_response *resp);

/**
 * dlb_hw_unmap_qid() - Unmap a load-balanced queue from a load-balanced port
 * @hw: dlb_hw handle for a particular device.
 * @domain_id: domain ID.
 * @args: unmap QID arguments.
 * @resp: response structure.
 *
 * This function configures the DLB to stop scheduling QEs from the specified
 * queue to the specified port.
 *
 * A successful return does not necessarily mean the mapping was removed. If
 * this function is unable to immediately unmap the queue from the port, it
 * will add the requested operation to a per-port list of pending map/unmap
 * operations, and (if it's not already running) launch a kernel thread that
 * periodically attempts to process all pending operations. See
 * dlb_hw_map_qid() for more details.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise. If an error occurs, resp->status is
 * assigned a detailed error code from enum dlb_error.
 *
 * Errors:
 * EINVAL - A requested resource is unavailable, invalid port or queue ID, or
 *	    the domain is not configured.
 * EFAULT - Internal error (resp->status not set).
 */
int dlb_hw_unmap_qid(struct dlb_hw *hw,
		     u32 domain_id,
		     struct dlb_unmap_qid_args *args,
		     struct dlb_cmd_response *resp);

/**
 * dlb_finish_unmap_qid_procedures() - finish any pending unmap procedures
 * @hw: dlb_hw handle for a particular device.
 *
 * This function attempts to finish any outstanding unmap procedures.
 * This function should be called by the kernel thread responsible for
 * finishing map/unmap procedures.
 *
 * Return:
 * Returns the number of procedures that weren't completed.
 */
unsigned int dlb_finish_unmap_qid_procedures(struct dlb_hw *hw);

/**
 * dlb_finish_map_qid_procedures() - finish any pending map procedures
 * @hw: dlb_hw handle for a particular device.
 *
 * This function attempts to finish any outstanding map procedures.
 * This function should be called by the kernel thread responsible for
 * finishing map/unmap procedures.
 *
 * Return:
 * Returns the number of procedures that weren't completed.
 */
unsigned int dlb_finish_map_qid_procedures(struct dlb_hw *hw);

/**
 * dlb_hw_enable_ldb_port() - enable a load-balanced port for scheduling
 * @hw: dlb_hw handle for a particular device.
 * @domain_id: domain ID.
 * @args: port enable arguments.
 * @resp: response structure.
 *
 * This function configures the DLB to schedule QEs to a load-balanced port.
 * Ports are enabled by default.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise. If an error occurs, resp->status is
 * assigned a detailed error code from enum dlb_error.
 *
 * Errors:
 * EINVAL - The port ID is invalid or the domain is not configured.
 * EFAULT - Internal error (resp->status not set).
 */
int dlb_hw_enable_ldb_port(struct dlb_hw *hw,
			   u32 domain_id,
			   struct dlb_enable_ldb_port_args *args,
			   struct dlb_cmd_response *resp);

/**
 * dlb_hw_disable_ldb_port() - disable a load-balanced port for scheduling
 * @hw: dlb_hw handle for a particular device.
 * @domain_id: domain ID.
 * @args: port disable arguments.
 * @resp: response structure.
 *
 * This function configures the DLB to stop scheduling QEs to a load-balanced
 * port. Ports are enabled by default.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise. If an error occurs, resp->status is
 * assigned a detailed error code from enum dlb_error.
 *
 * Errors:
 * EINVAL - The port ID is invalid or the domain is not configured.
 * EFAULT - Internal error (resp->status not set).
 */
int dlb_hw_disable_ldb_port(struct dlb_hw *hw,
			    u32 domain_id,
			    struct dlb_disable_ldb_port_args *args,
			    struct dlb_cmd_response *resp);

/**
 * dlb_hw_enable_dir_port() - enable a directed port for scheduling
 * @hw: dlb_hw handle for a particular device.
 * @domain_id: domain ID.
 * @args: port enable arguments.
 * @resp: response structure.
 *
 * This function configures the DLB to schedule QEs to a directed port.
 * Ports are enabled by default.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise. If an error occurs, resp->status is
 * assigned a detailed error code from enum dlb_error.
 *
 * Errors:
 * EINVAL - The port ID is invalid or the domain is not configured.
 * EFAULT - Internal error (resp->status not set).
 */
int dlb_hw_enable_dir_port(struct dlb_hw *hw,
			   u32 domain_id,
			   struct dlb_enable_dir_port_args *args,
			   struct dlb_cmd_response *resp);

/**
 * dlb_hw_disable_dir_port() - disable a directed port for scheduling
 * @hw: dlb_hw handle for a particular device.
 * @domain_id: domain ID.
 * @args: port disable arguments.
 * @resp: response structure.
 *
 * This function configures the DLB to stop scheduling QEs to a directed port.
 * Ports are enabled by default.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise. If an error occurs, resp->status is
 * assigned a detailed error code from enum dlb_error.
 *
 * Errors:
 * EINVAL - The port ID is invalid or the domain is not configured.
 * EFAULT - Internal error (resp->status not set).
 */
int dlb_hw_disable_dir_port(struct dlb_hw *hw,
			    u32 domain_id,
			    struct dlb_disable_dir_port_args *args,
			    struct dlb_cmd_response *resp);

/**
 * dlb_configure_ldb_cq_interrupt() - configure load-balanced CQ for interrupts
 * @hw: dlb_hw handle for a particular device.
 * @port_id: load-balancd port ID.
 * @vector: interrupt vector ID. Should be 0 for MSI or compressed MSI-X mode,
 *	    else a value up to 64.
 * @mode: interrupt type (DLB_CQ_ISR_MODE_MSI or DLB_CQ_ISR_MODE_MSIX)
 * @threshold: the minimum CQ depth at which the interrupt can fire. Must be
 *	greater than 0.
 *
 * This function configures the DLB registers for load-balanced CQ's interrupts.
 * This doesn't enable the CQ's interrupt; that can be done with
 * dlb_arm_cq_interrupt() or through an interrupt arm QE.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise.
 *
 * Errors:
 * EINVAL - The port ID is invalid.
 */
int dlb_configure_ldb_cq_interrupt(struct dlb_hw *hw,
				   int port_id,
				   int vector,
				   int mode,
				   u16 threshold);

/**
 * dlb_configure_dir_cq_interrupt() - configure directed CQ for interrupts
 * @hw: dlb_hw handle for a particular device.
 * @port_id: load-balancd port ID.
 * @vector: interrupt vector ID. Should be 0 for MSI or compressed MSI-X mode,
 *	    else a value up to 64.
 * @mode: interrupt type (DLB_CQ_ISR_MODE_MSI or DLB_CQ_ISR_MODE_MSIX)
 * @threshold: the minimum CQ depth at which the interrupt can fire. Must be
 *	greater than 0.
 *
 * This function configures the DLB registers for directed CQ's interrupts.
 * This doesn't enable the CQ's interrupt; that can be done with
 * dlb_arm_cq_interrupt() or through an interrupt arm QE.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise.
 *
 * Errors:
 * EINVAL - The port ID is invalid.
 */
int dlb_configure_dir_cq_interrupt(struct dlb_hw *hw,
				   int port_id,
				   int vector,
				   int mode,
				   u16 threshold);

/**
 * dlb_enable_alarm_interrupts() - enable certain hardware alarm interrupts
 * @hw: dlb_hw handle for a particular device.
 *
 * This function configures the ingress error alarm. (Other alarms are enabled
 * by default.)
 */
void dlb_enable_alarm_interrupts(struct dlb_hw *hw);

/**
 * dlb_disable_alarm_interrupts() - disable certain hardware alarm interrupts
 * @hw: dlb_hw handle for a particular device.
 *
 * This function configures the ingress error alarm. (Other alarms are disabled
 * by default.)
 */
void dlb_disable_alarm_interrupts(struct dlb_hw *hw);

/**
 * dlb_set_msix_mode() - enable certain hardware alarm interrupts
 * @hw: dlb_hw handle for a particular device.
 * @mode: MSI-X mode (DLB_MSIX_MODE_PACKED or DLB_MSIX_MODE_COMPRESSED)
 *
 * This function configures the hardware to use either packed or compressed
 * mode. This function should not be called if using MSI interrupts.
 */
void dlb_set_msix_mode(struct dlb_hw *hw, int mode);

/**
 * dlb_arm_cq_interrupt() - arm a CQ's interrupt
 * @hw: dlb_hw handle for a particular device.
 * @port_id: port ID
 * @is_ldb: true for load-balanced port, false for a directed port
 *
 * This function arms the CQ's interrupt. The CQ must be configured prior to
 * calling this function.
 *
 * The function does no parameter validation; that is the caller's
 * responsibility.
 *
 * Return: returns 0 upon success, <0 otherwise.
 *
 * EINVAL - Invalid port ID.
 */
int dlb_arm_cq_interrupt(struct dlb_hw *hw, int port_id, bool is_ldb);

/**
 * dlb_read_compressed_cq_intr_status() - read compressed CQ interrupt status
 * @hw: dlb_hw handle for a particular device.
 * @ldb_interrupts: 2-entry array of u32 bitmaps
 * @dir_interrupts: 4-entry array of u32 bitmaps
 *
 * This function can be called from a compressed CQ interrupt handler to
 * determine which CQ interrupts have fired. The caller should take appropriate
 * (such as waking threads blocked on a CQ's interrupt) then ack the interrupts
 * with dlb_ack_compressed_cq_intr().
 */
void dlb_read_compressed_cq_intr_status(struct dlb_hw *hw,
					u32 *ldb_interrupts,
					u32 *dir_interrupts);

/**
 * dlb_ack_compressed_cq_intr_status() - ack compressed CQ interrupts
 * @hw: dlb_hw handle for a particular device.
 * @ldb_interrupts: 2-entry array of u32 bitmaps
 * @dir_interrupts: 4-entry array of u32 bitmaps
 *
 * This function ACKs compressed CQ interrupts. Its arguments should be the
 * same ones passed to dlb_read_compressed_cq_intr_status().
 */
void dlb_ack_compressed_cq_intr(struct dlb_hw *hw,
				u32 *ldb_interrupts,
				u32 *dir_interrupts);

/**
 * dlb_process_alarm_interrupt() - process an alarm interrupt
 * @hw: dlb_hw handle for a particular device.
 *
 * This function reads the alarm syndrome, logs its, and acks the interrupt.
 * This function should be called from the alarm interrupt handler when
 * interrupt vector DLB_INT_ALARM fires.
 */
void dlb_process_alarm_interrupt(struct dlb_hw *hw);

/**
 * dlb_process_ingress_error_interrupt() - process ingress error interrupts
 * @hw: dlb_hw handle for a particular device.
 *
 * This function reads the alarm syndrome, logs it, notifies user-space, and
 * acks the interrupt. This function should be called from the alarm interrupt
 * handler when interrupt vector DLB_INT_INGRESS_ERROR fires.
 */
void dlb_process_ingress_error_interrupt(struct dlb_hw *hw);

/**
 * dlb_get_group_sequence_numbers() - return a group's number of SNs per queue
 * @hw: dlb_hw handle for a particular device.
 * @group_id: sequence number group ID.
 *
 * This function returns the configured number of sequence numbers per queue
 * for the specified group.
 *
 * Return:
 * Returns -EINVAL if group_id is invalid, else the group's SNs per queue.
 */
int dlb_get_group_sequence_numbers(struct dlb_hw *hw, unsigned int group_id);

/**
 * dlb_get_group_sequence_number_occupancy() - return a group's in-use slots
 * @hw: dlb_hw handle for a particular device.
 * @group_id: sequence number group ID.
 *
 * This function returns the group's number of in-use slots (i.e. load-balanced
 * queues using the specified group).
 *
 * Return:
 * Returns -EINVAL if group_id is invalid, else the group's occupancy.
 */
int dlb_get_group_sequence_number_occupancy(struct dlb_hw *hw,
					    unsigned int group_id);

/**
 * dlb_set_group_sequence_numbers() - assign a group's number of SNs per queue
 * @hw: dlb_hw handle for a particular device.
 * @group_id: sequence number group ID.
 * @val: requested amount of sequence numbers per queue.
 *
 * This function configures the group's number of sequence numbers per queue.
 * val can be a power-of-two between 32 and 1024, inclusive. This setting can
 * be configured until the first ordered load-balanced queue is configured, at
 * which point the configuration is locked.
 *
 * Return:
 * Returns 0 upon success; -EINVAL if group_id or val is invalid, -EPERM if an
 * ordered queue is configured.
 */
int dlb_set_group_sequence_numbers(struct dlb_hw *hw,
				   unsigned int group_id,
				   unsigned long val);

/**
 * dlb_reset_domain() - reset a scheduling domain
 * @hw: dlb_hw handle for a particular device.
 * @domain_id: domain ID.
 *
 * This function resets and frees a DLB scheduling domain and its associated
 * resources.
 *
 * Pre-condition: the driver must ensure software has stopped sending QEs
 * through this domain's producer ports before invoking this function, or
 * undefined behavior will result.
 *
 * Return:
 * Returns 0 upon success, -1 otherwise.
 *
 * EINVAL - Invalid domain ID, or the domain is not configured.
 * EFAULT - Internal error. (Possibly caused if software is the pre-condition
 *	    is not met.)
 * ETIMEDOUT - Hardware component didn't reset in the expected time.
 */
int dlb_reset_domain(struct dlb_hw *hw, u32 domain_id);

/**
 * dlb_ldb_port_owned_by_domain() - query whether a port is owned by a domain
 * @hw: dlb_hw handle for a particular device.
 * @domain_id: domain ID.
 * @port_id: port ID.
 *
 * This function returns whether a load-balanced port is owned by a specified
 * domain.
 *
 * Return:
 * Returns 0 if false, 1 if true, <0 otherwise.
 *
 * EINVAL - Invalid domain or port ID, or the domain is not configured.
 */
int dlb_ldb_port_owned_by_domain(struct dlb_hw *hw,
				 u32 domain_id,
				 u32 port_id);

/**
 * dlb_dir_port_owned_by_domain() - query whether a port is owned by a domain
 * @hw: dlb_hw handle for a particular device.
 * @domain_id: domain ID.
 * @port_id: port ID.
 *
 * This function returns whether a directed port is owned by a specified
 * domain.
 *
 * Return:
 * Returns 0 if false, 1 if true, <0 otherwise.
 *
 * EINVAL - Invalid domain or port ID, or the domain is not configured.
 */
int dlb_dir_port_owned_by_domain(struct dlb_hw *hw,
				 u32 domain_id,
				 u32 port_id);

/**
 * dlb_hw_get_num_resources() - query the PCI function's available resources
 * @arg: pointer to resource counts.
 *
 * This function returns the number of available resources for the PF.
 */
void dlb_hw_get_num_resources(struct dlb_hw *hw,
			      struct dlb_get_num_resources_args *arg);

/**
 * dlb_hw_get_num_used_resources() - query the PCI function's used resources
 * @arg: pointer to resource counts.
 *
 * This function returns the number of resources in use by the PF. It fills in
 * the fields that args points to, except the following:
 * - max_contiguous_atomic_inflights
 * - max_contiguous_hist_list_entries
 * - max_contiguous_ldb_credits
 * - max_contiguous_dir_credits
 */
void dlb_hw_get_num_used_resources(struct dlb_hw *hw,
				   struct dlb_get_num_resources_args *arg);

/**
 * dlb_disable_dp_vasr_feature() - disable directed pipe VAS reset hardware
 * @hw: dlb_hw handle for a particular device.
 *
 * This function disables certain hardware in the directed pipe,
 * necessary to workaround a DLB VAS reset issue.
 */
void dlb_disable_dp_vasr_feature(struct dlb_hw *hw);

/**
 * dlb_enable_excess_tokens_alarm() - enable interrupts for the excess token
 * pop alarm
 * @hw: dlb_hw handle for a particular device.
 *
 * This function enables the PF ingress error alarm interrupt to fire when an
 * excess token pop occurs.
 */
void dlb_enable_excess_tokens_alarm(struct dlb_hw *hw);

/**
 * dlb_disable_excess_tokens_alarm() - disable interrupts for the excess token
 * pop alarm
 * @hw: dlb_hw handle for a particular device.
 *
 * This function disables the PF ingress error alarm interrupt to fire when an
 * excess token pop occurs.
 */
void dlb_disable_excess_tokens_alarm(struct dlb_hw *hw);

/**
 * dlb_hw_get_ldb_queue_depth() - returns the depth of a load-balanced queue
 * @hw: dlb_hw handle for a particular device.
 * @domain_id: domain ID.
 * @args: queue depth args
 *
 * This function returns the depth of a load-balanced queue.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise. If an error occurs, resp->status is
 * assigned a detailed error code from enum dlb_error. If successful, resp->id
 * contains the depth.
 *
 * Errors:
 * EINVAL - Invalid domain ID or queue ID.
 */
int dlb_hw_get_ldb_queue_depth(struct dlb_hw *hw,
			       u32 domain_id,
			       struct dlb_get_ldb_queue_depth_args *args,
			       struct dlb_cmd_response *resp);

/**
 * dlb_hw_get_dir_queue_depth() - returns the depth of a directed queue
 * @hw: dlb_hw handle for a particular device.
 * @domain_id: domain ID.
 * @args: queue depth args
 *
 * This function returns the depth of a directed queue.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise. If an error occurs, resp->status is
 * assigned a detailed error code from enum dlb_error. If successful, resp->id
 * contains the depth.
 *
 * Errors:
 * EINVAL - Invalid domain ID or queue ID.
 */
int dlb_hw_get_dir_queue_depth(struct dlb_hw *hw,
			       u32 domain_id,
			       struct dlb_get_dir_queue_depth_args *args,
			       struct dlb_cmd_response *resp);

/**
 * dlb_hw_pending_port_unmaps() - returns the number of unmap operations in
 *	progress for a load-balanced port.
 * @hw: dlb_hw handle for a particular device.
 * @domain_id: domain ID.
 * @args: number of unmaps in progress args
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise. If an error occurs, resp->status is
 * assigned a detailed error code from enum dlb_error. If successful, resp->id
 * contains the number of unmaps in progress.
 *
 * Errors:
 * EINVAL - Invalid port ID.
 */
int dlb_hw_pending_port_unmaps(struct dlb_hw *hw,
			       u32 domain_id,
			       struct dlb_pending_port_unmaps_args *args,
			       struct dlb_cmd_response *resp);

/**
 * dlb_hw_enable_sparse_ldb_cq_mode() - enable sparse mode for load-balanced
 *	ports.
 * @hw: dlb_hw handle for a particular device.
 *
 * This function must be called prior to configuring scheduling domains.
 */
void dlb_hw_enable_sparse_ldb_cq_mode(struct dlb_hw *hw);

/**
 * dlb_hw_enable_sparse_dir_cq_mode() - enable sparse mode for directed ports
 * @hw: dlb_hw handle for a particular device.
 *
 * This function must be called prior to configuring scheduling domains.
 */
void dlb_hw_enable_sparse_dir_cq_mode(struct dlb_hw *hw);

/**
 * dlb_hw_set_qe_arbiter_weights() - program QE arbiter weights
 * @hw: dlb_hw handle for a particular device.
 * @weight: 8-entry array of arbiter weights.
 *
 * weight[N] programs priority N's weight. In cases where the 8 priorities are
 * reduced to 4 bins, the mapping is:
 * - weight[1] programs bin 0
 * - weight[3] programs bin 1
 * - weight[5] programs bin 2
 * - weight[7] programs bin 3
 */
void dlb_hw_set_qe_arbiter_weights(struct dlb_hw *hw, u8 weight[8]);

/**
 * dlb_hw_set_qid_arbiter_weights() - program QID arbiter weights
 * @hw: dlb_hw handle for a particular device.
 * @weight: 8-entry array of arbiter weights.
 *
 * weight[N] programs priority N's weight. In cases where the 8 priorities are
 * reduced to 4 bins, the mapping is:
 * - weight[1] programs bin 0
 * - weight[3] programs bin 1
 * - weight[5] programs bin 2
 * - weight[7] programs bin 3
 */
void dlb_hw_set_qid_arbiter_weights(struct dlb_hw *hw, u8 weight[8]);

/**
 * dlb_hw_enable_pp_sw_alarms() - enable out-of-credit alarm for all producer
 * ports
 * @hw: dlb_hw handle for a particular device.
 */
void dlb_hw_enable_pp_sw_alarms(struct dlb_hw *hw);

/**
 * dlb_hw_disable_pp_sw_alarms() - disable out-of-credit alarm for all producer
 * ports
 * @hw: dlb_hw handle for a particular device.
 */
void dlb_hw_disable_pp_sw_alarms(struct dlb_hw *hw);

/**
 * dlb_hw_disable_pf_to_vf_isr_pend_err() - disable alarm triggered by PF
 *	access to VF's ISR pending register
 * @hw: dlb_hw handle for a particular device.
 */
void dlb_hw_disable_pf_to_vf_isr_pend_err(struct dlb_hw *hw);

/**
 * dlb_hw_disable_vf_to_pf_isr_pend_err() - disable alarm triggered by VF
 *	access to PF's ISR pending register
 * @hw: dlb_hw handle for a particular device.
 */
void dlb_hw_disable_vf_to_pf_isr_pend_err(struct dlb_hw *hw);

#endif /* __DLB_RESOURCE_H */
