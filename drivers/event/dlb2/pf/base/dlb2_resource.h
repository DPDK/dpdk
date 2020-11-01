/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2020 Intel Corporation
 */

#ifndef __DLB2_RESOURCE_H
#define __DLB2_RESOURCE_H

#include "dlb2_user.h"

#include "dlb2_hw_types.h"
#include "dlb2_osdep_types.h"

/**
 * dlb2_resource_init() - initialize the device
 * @hw: pointer to struct dlb2_hw.
 *
 * This function initializes the device's software state (pointed to by the hw
 * argument) and programs global scheduling QoS registers. This function should
 * be called during driver initialization.
 *
 * The dlb2_hw struct must be unique per DLB 2.0 device and persist until the
 * device is reset.
 *
 * Return:
 * Returns 0 upon success, <0 otherwise.
 */
int dlb2_resource_init(struct dlb2_hw *hw);

/**
 * dlb2_resource_free() - free device state memory
 * @hw: dlb2_hw handle for a particular device.
 *
 * This function frees software state pointed to by dlb2_hw. This function
 * should be called when resetting the device or unloading the driver.
 */
void dlb2_resource_free(struct dlb2_hw *hw);

/**
 * dlb2_resource_reset() - reset in-use resources to their initial state
 * @hw: dlb2_hw handle for a particular device.
 *
 * This function resets in-use resources, and makes them available for use.
 * All resources go back to their owning function, whether a PF or a VF.
 */
void dlb2_resource_reset(struct dlb2_hw *hw);

/**
 * dlb2_hw_create_sched_domain() - create a scheduling domain
 * @hw: dlb2_hw handle for a particular device.
 * @args: scheduling domain creation arguments.
 * @resp: response structure.
 * @vdev_request: indicates whether this request came from a vdev.
 * @vdev_id: If vdev_request is true, this contains the vdev's ID.
 *
 * This function creates a scheduling domain containing the resources specified
 * in args. The individual resources (queues, ports, credits) can be configured
 * after creating a scheduling domain.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise. If an error occurs, resp->status is
 * assigned a detailed error code from enum dlb2_error. If successful, resp->id
 * contains the domain ID.
 *
 * resp->id contains a virtual ID if vdev_request is true.
 *
 * Errors:
 * EINVAL - A requested resource is unavailable, or the requested domain name
 *	    is already in use.
 * EFAULT - Internal error (resp->status not set).
 */
int dlb2_hw_create_sched_domain(struct dlb2_hw *hw,
				struct dlb2_create_sched_domain_args *args,
				struct dlb2_cmd_response *resp,
				bool vdev_request,
				unsigned int vdev_id);

/**
 * dlb2_hw_create_ldb_queue() - create a load-balanced queue
 * @hw: dlb2_hw handle for a particular device.
 * @domain_id: domain ID.
 * @args: queue creation arguments.
 * @resp: response structure.
 * @vdev_request: indicates whether this request came from a vdev.
 * @vdev_id: If vdev_request is true, this contains the vdev's ID.
 *
 * This function creates a load-balanced queue.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise. If an error occurs, resp->status is
 * assigned a detailed error code from enum dlb2_error. If successful, resp->id
 * contains the queue ID.
 *
 * resp->id contains a virtual ID if vdev_request is true.
 *
 * Errors:
 * EINVAL - A requested resource is unavailable, the domain is not configured,
 *	    the domain has already been started, or the requested queue name is
 *	    already in use.
 * EFAULT - Internal error (resp->status not set).
 */
int dlb2_hw_create_ldb_queue(struct dlb2_hw *hw,
			     u32 domain_id,
			     struct dlb2_create_ldb_queue_args *args,
			     struct dlb2_cmd_response *resp,
			     bool vdev_request,
			     unsigned int vdev_id);

/**
 * dlb2_hw_create_dir_queue() - create a directed queue
 * @hw: dlb2_hw handle for a particular device.
 * @domain_id: domain ID.
 * @args: queue creation arguments.
 * @resp: response structure.
 * @vdev_request: indicates whether this request came from a vdev.
 * @vdev_id: If vdev_request is true, this contains the vdev's ID.
 *
 * This function creates a directed queue.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise. If an error occurs, resp->status is
 * assigned a detailed error code from enum dlb2_error. If successful, resp->id
 * contains the queue ID.
 *
 * resp->id contains a virtual ID if vdev_request is true.
 *
 * Errors:
 * EINVAL - A requested resource is unavailable, the domain is not configured,
 *	    or the domain has already been started.
 * EFAULT - Internal error (resp->status not set).
 */
int dlb2_hw_create_dir_queue(struct dlb2_hw *hw,
			     u32 domain_id,
			     struct dlb2_create_dir_queue_args *args,
			     struct dlb2_cmd_response *resp,
			     bool vdev_request,
			     unsigned int vdev_id);

/**
 * dlb2_hw_create_dir_port() - create a directed port
 * @hw: dlb2_hw handle for a particular device.
 * @domain_id: domain ID.
 * @args: port creation arguments.
 * @cq_dma_base: base address of the CQ memory. This can be a PA or an IOVA.
 * @resp: response structure.
 * @vdev_request: indicates whether this request came from a vdev.
 * @vdev_id: If vdev_request is true, this contains the vdev's ID.
 *
 * This function creates a directed port.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise. If an error occurs, resp->status is
 * assigned a detailed error code from enum dlb2_error. If successful, resp->id
 * contains the port ID.
 *
 * resp->id contains a virtual ID if vdev_request is true.
 *
 * Errors:
 * EINVAL - A requested resource is unavailable, a credit setting is invalid, a
 *	    pointer address is not properly aligned, the domain is not
 *	    configured, or the domain has already been started.
 * EFAULT - Internal error (resp->status not set).
 */
int dlb2_hw_create_dir_port(struct dlb2_hw *hw,
			    u32 domain_id,
			    struct dlb2_create_dir_port_args *args,
			    uintptr_t cq_dma_base,
			    struct dlb2_cmd_response *resp,
			    bool vdev_request,
			    unsigned int vdev_id);

/**
 * dlb2_hw_create_ldb_port() - create a load-balanced port
 * @hw: dlb2_hw handle for a particular device.
 * @domain_id: domain ID.
 * @args: port creation arguments.
 * @cq_dma_base: base address of the CQ memory. This can be a PA or an IOVA.
 * @resp: response structure.
 * @vdev_request: indicates whether this request came from a vdev.
 * @vdev_id: If vdev_request is true, this contains the vdev's ID.
 *
 * This function creates a load-balanced port.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise. If an error occurs, resp->status is
 * assigned a detailed error code from enum dlb2_error. If successful, resp->id
 * contains the port ID.
 *
 * resp->id contains a virtual ID if vdev_request is true.
 *
 * Errors:
 * EINVAL - A requested resource is unavailable, a credit setting is invalid, a
 *	    pointer address is not properly aligned, the domain is not
 *	    configured, or the domain has already been started.
 * EFAULT - Internal error (resp->status not set).
 */
int dlb2_hw_create_ldb_port(struct dlb2_hw *hw,
			    u32 domain_id,
			    struct dlb2_create_ldb_port_args *args,
			    uintptr_t cq_dma_base,
			    struct dlb2_cmd_response *resp,
			    bool vdev_request,
			    unsigned int vdev_id);

/**
 * dlb2_hw_start_domain() - start a scheduling domain
 * @hw: dlb2_hw handle for a particular device.
 * @domain_id: domain ID.
 * @args: start domain arguments.
 * @resp: response structure.
 * @vdev_request: indicates whether this request came from a vdev.
 * @vdev_id: If vdev_request is true, this contains the vdev's ID.
 *
 * This function starts a scheduling domain, which allows applications to send
 * traffic through it. Once a domain is started, its resources can no longer be
 * configured (besides QID remapping and port enable/disable).
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise. If an error occurs, resp->status is
 * assigned a detailed error code from enum dlb2_error.
 *
 * Errors:
 * EINVAL - the domain is not configured, or the domain is already started.
 */
int dlb2_hw_start_domain(struct dlb2_hw *hw,
			 u32 domain_id,
			 struct dlb2_start_domain_args *args,
			 struct dlb2_cmd_response *resp,
			 bool vdev_request,
			 unsigned int vdev_id);

/**
 * dlb2_hw_map_qid() - map a load-balanced queue to a load-balanced port
 * @hw: dlb2_hw handle for a particular device.
 * @domain_id: domain ID.
 * @args: map QID arguments.
 * @resp: response structure.
 * @vdev_request: indicates whether this request came from a vdev.
 * @vdev_id: If vdev_request is true, this contains the vdev's ID.
 *
 * This function configures the DLB to schedule QEs from the specified queue
 * to the specified port. Each load-balanced port can be mapped to up to 8
 * queues; each load-balanced queue can potentially map to all the
 * load-balanced ports.
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
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise. If an error occurs, resp->status is
 * assigned a detailed error code from enum dlb2_error.
 *
 * Errors:
 * EINVAL - A requested resource is unavailable, invalid port or queue ID, or
 *	    the domain is not configured.
 * EFAULT - Internal error (resp->status not set).
 */
int dlb2_hw_map_qid(struct dlb2_hw *hw,
		    u32 domain_id,
		    struct dlb2_map_qid_args *args,
		    struct dlb2_cmd_response *resp,
		    bool vdev_request,
		    unsigned int vdev_id);

/**
 * dlb2_hw_unmap_qid() - Unmap a load-balanced queue from a load-balanced port
 * @hw: dlb2_hw handle for a particular device.
 * @domain_id: domain ID.
 * @args: unmap QID arguments.
 * @resp: response structure.
 * @vdev_request: indicates whether this request came from a vdev.
 * @vdev_id: If vdev_request is true, this contains the vdev's ID.
 *
 * This function configures the DLB to stop scheduling QEs from the specified
 * queue to the specified port.
 *
 * A successful return does not necessarily mean the mapping was removed. If
 * this function is unable to immediately unmap the queue from the port, it
 * will add the requested operation to a per-port list of pending map/unmap
 * operations, and (if it's not already running) launch a kernel thread that
 * periodically attempts to process all pending operations. See
 * dlb2_hw_map_qid() for more details.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise. If an error occurs, resp->status is
 * assigned a detailed error code from enum dlb2_error.
 *
 * Errors:
 * EINVAL - A requested resource is unavailable, invalid port or queue ID, or
 *	    the domain is not configured.
 * EFAULT - Internal error (resp->status not set).
 */
int dlb2_hw_unmap_qid(struct dlb2_hw *hw,
		      u32 domain_id,
		      struct dlb2_unmap_qid_args *args,
		      struct dlb2_cmd_response *resp,
		      bool vdev_request,
		      unsigned int vdev_id);

/**
 * dlb2_finish_unmap_qid_procedures() - finish any pending unmap procedures
 * @hw: dlb2_hw handle for a particular device.
 *
 * This function attempts to finish any outstanding unmap procedures.
 * This function should be called by the kernel thread responsible for
 * finishing map/unmap procedures.
 *
 * Return:
 * Returns the number of procedures that weren't completed.
 */
unsigned int dlb2_finish_unmap_qid_procedures(struct dlb2_hw *hw);

/**
 * dlb2_finish_map_qid_procedures() - finish any pending map procedures
 * @hw: dlb2_hw handle for a particular device.
 *
 * This function attempts to finish any outstanding map procedures.
 * This function should be called by the kernel thread responsible for
 * finishing map/unmap procedures.
 *
 * Return:
 * Returns the number of procedures that weren't completed.
 */
unsigned int dlb2_finish_map_qid_procedures(struct dlb2_hw *hw);

/**
 * dlb2_hw_enable_ldb_port() - enable a load-balanced port for scheduling
 * @hw: dlb2_hw handle for a particular device.
 * @domain_id: domain ID.
 * @args: port enable arguments.
 * @resp: response structure.
 * @vdev_request: indicates whether this request came from a vdev.
 * @vdev_id: If vdev_request is true, this contains the vdev's ID.
 *
 * This function configures the DLB to schedule QEs to a load-balanced port.
 * Ports are enabled by default.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise. If an error occurs, resp->status is
 * assigned a detailed error code from enum dlb2_error.
 *
 * Errors:
 * EINVAL - The port ID is invalid or the domain is not configured.
 * EFAULT - Internal error (resp->status not set).
 */
int dlb2_hw_enable_ldb_port(struct dlb2_hw *hw,
			    u32 domain_id,
			    struct dlb2_enable_ldb_port_args *args,
			    struct dlb2_cmd_response *resp,
			    bool vdev_request,
			    unsigned int vdev_id);

/**
 * dlb2_hw_disable_ldb_port() - disable a load-balanced port for scheduling
 * @hw: dlb2_hw handle for a particular device.
 * @domain_id: domain ID.
 * @args: port disable arguments.
 * @resp: response structure.
 * @vdev_request: indicates whether this request came from a vdev.
 * @vdev_id: If vdev_request is true, this contains the vdev's ID.
 *
 * This function configures the DLB to stop scheduling QEs to a load-balanced
 * port. Ports are enabled by default.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise. If an error occurs, resp->status is
 * assigned a detailed error code from enum dlb2_error.
 *
 * Errors:
 * EINVAL - The port ID is invalid or the domain is not configured.
 * EFAULT - Internal error (resp->status not set).
 */
int dlb2_hw_disable_ldb_port(struct dlb2_hw *hw,
			     u32 domain_id,
			     struct dlb2_disable_ldb_port_args *args,
			     struct dlb2_cmd_response *resp,
			     bool vdev_request,
			     unsigned int vdev_id);

/**
 * dlb2_hw_enable_dir_port() - enable a directed port for scheduling
 * @hw: dlb2_hw handle for a particular device.
 * @domain_id: domain ID.
 * @args: port enable arguments.
 * @resp: response structure.
 * @vdev_request: indicates whether this request came from a vdev.
 * @vdev_id: If vdev_request is true, this contains the vdev's ID.
 *
 * This function configures the DLB to schedule QEs to a directed port.
 * Ports are enabled by default.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise. If an error occurs, resp->status is
 * assigned a detailed error code from enum dlb2_error.
 *
 * Errors:
 * EINVAL - The port ID is invalid or the domain is not configured.
 * EFAULT - Internal error (resp->status not set).
 */
int dlb2_hw_enable_dir_port(struct dlb2_hw *hw,
			    u32 domain_id,
			    struct dlb2_enable_dir_port_args *args,
			    struct dlb2_cmd_response *resp,
			    bool vdev_request,
			    unsigned int vdev_id);

/**
 * dlb2_hw_disable_dir_port() - disable a directed port for scheduling
 * @hw: dlb2_hw handle for a particular device.
 * @domain_id: domain ID.
 * @args: port disable arguments.
 * @resp: response structure.
 * @vdev_request: indicates whether this request came from a vdev.
 * @vdev_id: If vdev_request is true, this contains the vdev's ID.
 *
 * This function configures the DLB to stop scheduling QEs to a directed port.
 * Ports are enabled by default.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise. If an error occurs, resp->status is
 * assigned a detailed error code from enum dlb2_error.
 *
 * Errors:
 * EINVAL - The port ID is invalid or the domain is not configured.
 * EFAULT - Internal error (resp->status not set).
 */
int dlb2_hw_disable_dir_port(struct dlb2_hw *hw,
			     u32 domain_id,
			     struct dlb2_disable_dir_port_args *args,
			     struct dlb2_cmd_response *resp,
			     bool vdev_request,
			     unsigned int vdev_id);

/**
 * dlb2_configure_ldb_cq_interrupt() - configure load-balanced CQ for
 *					interrupts
 * @hw: dlb2_hw handle for a particular device.
 * @port_id: load-balanced port ID.
 * @vector: interrupt vector ID. Should be 0 for MSI or compressed MSI-X mode,
 *	    else a value up to 64.
 * @mode: interrupt type (DLB2_CQ_ISR_MODE_MSI or DLB2_CQ_ISR_MODE_MSIX)
 * @vf: If the port is VF-owned, the VF's ID. This is used for translating the
 *	virtual port ID to a physical port ID. Ignored if mode is not MSI.
 * @owner_vf: the VF to route the interrupt to. Ignore if mode is not MSI.
 * @threshold: the minimum CQ depth at which the interrupt can fire. Must be
 *	greater than 0.
 *
 * This function configures the DLB registers for load-balanced CQ's
 * interrupts. This doesn't enable the CQ's interrupt; that can be done with
 * dlb2_arm_cq_interrupt() or through an interrupt arm QE.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise.
 *
 * Errors:
 * EINVAL - The port ID is invalid.
 */
int dlb2_configure_ldb_cq_interrupt(struct dlb2_hw *hw,
				    int port_id,
				    int vector,
				    int mode,
				    unsigned int vf,
				    unsigned int owner_vf,
				    u16 threshold);

/**
 * dlb2_configure_dir_cq_interrupt() - configure directed CQ for interrupts
 * @hw: dlb2_hw handle for a particular device.
 * @port_id: load-balanced port ID.
 * @vector: interrupt vector ID. Should be 0 for MSI or compressed MSI-X mode,
 *	    else a value up to 64.
 * @mode: interrupt type (DLB2_CQ_ISR_MODE_MSI or DLB2_CQ_ISR_MODE_MSIX)
 * @vf: If the port is VF-owned, the VF's ID. This is used for translating the
 *	virtual port ID to a physical port ID. Ignored if mode is not MSI.
 * @owner_vf: the VF to route the interrupt to. Ignore if mode is not MSI.
 * @threshold: the minimum CQ depth at which the interrupt can fire. Must be
 *	greater than 0.
 *
 * This function configures the DLB registers for directed CQ's interrupts.
 * This doesn't enable the CQ's interrupt; that can be done with
 * dlb2_arm_cq_interrupt() or through an interrupt arm QE.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise.
 *
 * Errors:
 * EINVAL - The port ID is invalid.
 */
int dlb2_configure_dir_cq_interrupt(struct dlb2_hw *hw,
				    int port_id,
				    int vector,
				    int mode,
				    unsigned int vf,
				    unsigned int owner_vf,
				    u16 threshold);

/**
 * dlb2_enable_ingress_error_alarms() - enable ingress error alarm interrupts
 * @hw: dlb2_hw handle for a particular device.
 */
void dlb2_enable_ingress_error_alarms(struct dlb2_hw *hw);

/**
 * dlb2_disable_ingress_error_alarms() - disable ingress error alarm interrupts
 * @hw: dlb2_hw handle for a particular device.
 */
void dlb2_disable_ingress_error_alarms(struct dlb2_hw *hw);

/**
 * dlb2_set_msix_mode() - enable certain hardware alarm interrupts
 * @hw: dlb2_hw handle for a particular device.
 * @mode: MSI-X mode (DLB2_MSIX_MODE_PACKED or DLB2_MSIX_MODE_COMPRESSED)
 *
 * This function configures the hardware to use either packed or compressed
 * mode. This function should not be called if using MSI interrupts.
 */
void dlb2_set_msix_mode(struct dlb2_hw *hw, int mode);

/**
 * dlb2_ack_msix_interrupt() - Ack an MSI-X interrupt
 * @hw: dlb2_hw handle for a particular device.
 * @vector: interrupt vector.
 *
 * Note: Only needed for PF service interrupts (vector 0). CQ interrupts are
 * acked in dlb2_ack_compressed_cq_intr().
 */
void dlb2_ack_msix_interrupt(struct dlb2_hw *hw, int vector);

/**
 * dlb2_arm_cq_interrupt() - arm a CQ's interrupt
 * @hw: dlb2_hw handle for a particular device.
 * @port_id: port ID
 * @is_ldb: true for load-balanced port, false for a directed port
 * @vdev_request: indicates whether this request came from a vdev.
 * @vdev_id: If vdev_request is true, this contains the vdev's ID.
 *
 * This function arms the CQ's interrupt. The CQ must be configured prior to
 * calling this function.
 *
 * The function does no parameter validation; that is the caller's
 * responsibility.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 *
 * Return: returns 0 upon success, <0 otherwise.
 *
 * EINVAL - Invalid port ID.
 */
int dlb2_arm_cq_interrupt(struct dlb2_hw *hw,
			  int port_id,
			  bool is_ldb,
			  bool vdev_request,
			  unsigned int vdev_id);

/**
 * dlb2_read_compressed_cq_intr_status() - read compressed CQ interrupt status
 * @hw: dlb2_hw handle for a particular device.
 * @ldb_interrupts: 2-entry array of u32 bitmaps
 * @dir_interrupts: 4-entry array of u32 bitmaps
 *
 * This function can be called from a compressed CQ interrupt handler to
 * determine which CQ interrupts have fired. The caller should take appropriate
 * (such as waking threads blocked on a CQ's interrupt) then ack the interrupts
 * with dlb2_ack_compressed_cq_intr().
 */
void dlb2_read_compressed_cq_intr_status(struct dlb2_hw *hw,
					 u32 *ldb_interrupts,
					 u32 *dir_interrupts);

/**
 * dlb2_ack_compressed_cq_intr_status() - ack compressed CQ interrupts
 * @hw: dlb2_hw handle for a particular device.
 * @ldb_interrupts: 2-entry array of u32 bitmaps
 * @dir_interrupts: 4-entry array of u32 bitmaps
 *
 * This function ACKs compressed CQ interrupts. Its arguments should be the
 * same ones passed to dlb2_read_compressed_cq_intr_status().
 */
void dlb2_ack_compressed_cq_intr(struct dlb2_hw *hw,
				 u32 *ldb_interrupts,
				 u32 *dir_interrupts);

/**
 * dlb2_read_vf_intr_status() - read the VF interrupt status register
 * @hw: dlb2_hw handle for a particular device.
 *
 * This function can be called from a VF's interrupt handler to determine
 * which interrupts have fired. The first 31 bits correspond to CQ interrupt
 * vectors, and the final bit is for the PF->VF mailbox interrupt vector.
 *
 * Return:
 * Returns a bit vector indicating which interrupt vectors are active.
 */
u32 dlb2_read_vf_intr_status(struct dlb2_hw *hw);

/**
 * dlb2_ack_vf_intr_status() - ack VF interrupts
 * @hw: dlb2_hw handle for a particular device.
 * @interrupts: 32-bit bitmap
 *
 * This function ACKs a VF's interrupts. Its interrupts argument should be the
 * value returned by dlb2_read_vf_intr_status().
 */
void dlb2_ack_vf_intr_status(struct dlb2_hw *hw, u32 interrupts);

/**
 * dlb2_ack_vf_msi_intr() - ack VF MSI interrupt
 * @hw: dlb2_hw handle for a particular device.
 * @interrupts: 32-bit bitmap
 *
 * This function clears the VF's MSI interrupt pending register. Its interrupts
 * argument should be contain the MSI vectors to ACK. For example, if MSI MME
 * is in mode 0, then one bit 0 should ever be set.
 */
void dlb2_ack_vf_msi_intr(struct dlb2_hw *hw, u32 interrupts);

/**
 * dlb2_ack_pf_mbox_int() - ack PF->VF mailbox interrupt
 * @hw: dlb2_hw handle for a particular device.
 *
 * When done processing the PF mailbox request, this function unsets
 * the PF's mailbox ISR register.
 */
void dlb2_ack_pf_mbox_int(struct dlb2_hw *hw);

/**
 * dlb2_read_vdev_to_pf_int_bitvec() - return a bit vector of all requesting
 *					vdevs
 * @hw: dlb2_hw handle for a particular device.
 *
 * When the vdev->PF ISR fires, this function can be called to determine which
 * vdev(s) are requesting service. This bitvector must be passed to
 * dlb2_ack_vdev_to_pf_int() when processing is complete for all requesting
 * vdevs.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 *
 * Return:
 * Returns a bit vector indicating which VFs (0-15) have requested service.
 */
u32 dlb2_read_vdev_to_pf_int_bitvec(struct dlb2_hw *hw);

/**
 * dlb2_ack_vdev_mbox_int() - ack processed vdev->PF mailbox interrupt
 * @hw: dlb2_hw handle for a particular device.
 * @bitvec: bit vector returned by dlb2_read_vdev_to_pf_int_bitvec()
 *
 * When done processing all VF mailbox requests, this function unsets the VF's
 * mailbox ISR register.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 */
void dlb2_ack_vdev_mbox_int(struct dlb2_hw *hw, u32 bitvec);

/**
 * dlb2_read_vf_flr_int_bitvec() - return a bit vector of all VFs requesting
 *				    FLR
 * @hw: dlb2_hw handle for a particular device.
 *
 * When the VF FLR ISR fires, this function can be called to determine which
 * VF(s) are requesting FLRs. This bitvector must passed to
 * dlb2_ack_vf_flr_int() when processing is complete for all requesting VFs.
 *
 * Return:
 * Returns a bit vector indicating which VFs (0-15) have requested FLRs.
 */
u32 dlb2_read_vf_flr_int_bitvec(struct dlb2_hw *hw);

/**
 * dlb2_ack_vf_flr_int() - ack processed VF<->PF interrupt(s)
 * @hw: dlb2_hw handle for a particular device.
 * @bitvec: bit vector returned by dlb2_read_vf_flr_int_bitvec()
 *
 * When done processing all VF FLR requests, this function unsets the VF's FLR
 * ISR register.
 */
void dlb2_ack_vf_flr_int(struct dlb2_hw *hw, u32 bitvec);

/**
 * dlb2_ack_vdev_to_pf_int() - ack processed VF mbox and FLR interrupt(s)
 * @hw: dlb2_hw handle for a particular device.
 * @mbox_bitvec: bit vector returned by dlb2_read_vdev_to_pf_int_bitvec()
 * @flr_bitvec: bit vector returned by dlb2_read_vf_flr_int_bitvec()
 *
 * When done processing all VF requests, this function communicates to the
 * hardware that processing is complete.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 */
void dlb2_ack_vdev_to_pf_int(struct dlb2_hw *hw,
			     u32 mbox_bitvec,
			     u32 flr_bitvec);

/**
 * dlb2_process_wdt_interrupt() - process watchdog timer interrupts
 * @hw: dlb2_hw handle for a particular device.
 *
 * This function reads the watchdog timer interrupt cause registers to
 * determine which port(s) had a watchdog timeout, and notifies the
 * application(s) that own the port(s).
 */
void dlb2_process_wdt_interrupt(struct dlb2_hw *hw);

/**
 * dlb2_process_alarm_interrupt() - process an alarm interrupt
 * @hw: dlb2_hw handle for a particular device.
 *
 * This function reads and logs the alarm syndrome, then acks the interrupt.
 * This function should be called from the alarm interrupt handler when
 * interrupt vector DLB2_INT_ALARM fires.
 */
void dlb2_process_alarm_interrupt(struct dlb2_hw *hw);

/**
 * dlb2_process_ingress_error_interrupt() - process ingress error interrupts
 * @hw: dlb2_hw handle for a particular device.
 *
 * This function reads the alarm syndrome, logs it, notifies user-space, and
 * acks the interrupt. This function should be called from the alarm interrupt
 * handler when interrupt vector DLB2_INT_INGRESS_ERROR fires.
 *
 * Return:
 * Returns true if an ingress error interrupt occurred, false otherwise
 */
bool dlb2_process_ingress_error_interrupt(struct dlb2_hw *hw);

/**
 * dlb2_get_group_sequence_numbers() - return a group's number of SNs per queue
 * @hw: dlb2_hw handle for a particular device.
 * @group_id: sequence number group ID.
 *
 * This function returns the configured number of sequence numbers per queue
 * for the specified group.
 *
 * Return:
 * Returns -EINVAL if group_id is invalid, else the group's SNs per queue.
 */
int dlb2_get_group_sequence_numbers(struct dlb2_hw *hw,
				    unsigned int group_id);

/**
 * dlb2_get_group_sequence_number_occupancy() - return a group's in-use slots
 * @hw: dlb2_hw handle for a particular device.
 * @group_id: sequence number group ID.
 *
 * This function returns the group's number of in-use slots (i.e. load-balanced
 * queues using the specified group).
 *
 * Return:
 * Returns -EINVAL if group_id is invalid, else the group's SNs per queue.
 */
int dlb2_get_group_sequence_number_occupancy(struct dlb2_hw *hw,
					     unsigned int group_id);

/**
 * dlb2_set_group_sequence_numbers() - assign a group's number of SNs per queue
 * @hw: dlb2_hw handle for a particular device.
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
int dlb2_set_group_sequence_numbers(struct dlb2_hw *hw,
				    unsigned int group_id,
				    unsigned long val);

/**
 * dlb2_reset_domain() - reset a scheduling domain
 * @hw: dlb2_hw handle for a particular device.
 * @domain_id: domain ID.
 * @vdev_request: indicates whether this request came from a vdev.
 * @vdev_id: If vdev_request is true, this contains the vdev's ID.
 *
 * This function resets and frees a DLB 2.0 scheduling domain and its associated
 * resources.
 *
 * Pre-condition: the driver must ensure software has stopped sending QEs
 * through this domain's producer ports before invoking this function, or
 * undefined behavior will result.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 *
 * Return:
 * Returns 0 upon success, -1 otherwise.
 *
 * EINVAL - Invalid domain ID, or the domain is not configured.
 * EFAULT - Internal error. (Possibly caused if software is the pre-condition
 *	    is not met.)
 * ETIMEDOUT - Hardware component didn't reset in the expected time.
 */
int dlb2_reset_domain(struct dlb2_hw *hw,
		      u32 domain_id,
		      bool vdev_request,
		      unsigned int vdev_id);

/**
 * dlb2_ldb_port_owned_by_domain() - query whether a port is owned by a domain
 * @hw: dlb2_hw handle for a particular device.
 * @domain_id: domain ID.
 * @port_id: indicates whether this request came from a VF.
 * @vdev_request: indicates whether this request came from a vdev.
 * @vdev_id: If vdev_request is true, this contains the vdev's ID.
 *
 * This function returns whether a load-balanced port is owned by a specified
 * domain.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 *
 * Return:
 * Returns 0 if false, 1 if true, <0 otherwise.
 *
 * EINVAL - Invalid domain or port ID, or the domain is not configured.
 */
int dlb2_ldb_port_owned_by_domain(struct dlb2_hw *hw,
				  u32 domain_id,
				  u32 port_id,
				  bool vdev_request,
				  unsigned int vdev_id);

/**
 * dlb2_dir_port_owned_by_domain() - query whether a port is owned by a domain
 * @hw: dlb2_hw handle for a particular device.
 * @domain_id: domain ID.
 * @port_id: indicates whether this request came from a VF.
 * @vdev_request: indicates whether this request came from a vdev.
 * @vdev_id: If vdev_request is true, this contains the vdev's ID.
 *
 * This function returns whether a directed port is owned by a specified
 * domain.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 *
 * Return:
 * Returns 0 if false, 1 if true, <0 otherwise.
 *
 * EINVAL - Invalid domain or port ID, or the domain is not configured.
 */
int dlb2_dir_port_owned_by_domain(struct dlb2_hw *hw,
				  u32 domain_id,
				  u32 port_id,
				  bool vdev_request,
				  unsigned int vdev_id);

/**
 * dlb2_hw_get_num_resources() - query the PCI function's available resources
 * @hw: dlb2_hw handle for a particular device.
 * @arg: pointer to resource counts.
 * @vdev_request: indicates whether this request came from a vdev.
 * @vdev_id: If vdev_request is true, this contains the vdev's ID.
 *
 * This function returns the number of available resources for the PF or for a
 * VF.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 *
 * Return:
 * Returns 0 upon success, -EINVAL if vdev_request is true and vdev_id is
 * invalid.
 */
int dlb2_hw_get_num_resources(struct dlb2_hw *hw,
			      struct dlb2_get_num_resources_args *arg,
			      bool vdev_request,
			      unsigned int vdev_id);

/**
 * dlb2_hw_get_num_used_resources() - query the PCI function's used resources
 * @hw: dlb2_hw handle for a particular device.
 * @arg: pointer to resource counts.
 * @vdev_request: indicates whether this request came from a vdev.
 * @vdev_id: If vdev_request is true, this contains the vdev's ID.
 *
 * This function returns the number of resources in use by the PF or a VF. It
 * fills in the fields that args points to, except the following:
 * - max_contiguous_atomic_inflights
 * - max_contiguous_hist_list_entries
 * - max_contiguous_ldb_credits
 * - max_contiguous_dir_credits
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 *
 * Return:
 * Returns 0 upon success, -EINVAL if vdev_request is true and vdev_id is
 * invalid.
 */
int dlb2_hw_get_num_used_resources(struct dlb2_hw *hw,
				   struct dlb2_get_num_resources_args *arg,
				   bool vdev_request,
				   unsigned int vdev_id);

/**
 * dlb2_send_async_vdev_to_pf_msg() - (vdev only) send a mailbox message to
 *				       the PF
 * @hw: dlb2_hw handle for a particular device.
 *
 * This function sends a VF->PF mailbox message. It is asynchronous, so it
 * returns once the message is sent but potentially before the PF has processed
 * the message. The caller must call dlb2_vdev_to_pf_complete() to determine
 * when the PF has finished processing the request.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 */
void dlb2_send_async_vdev_to_pf_msg(struct dlb2_hw *hw);

/**
 * dlb2_vdev_to_pf_complete() - check the status of an asynchronous mailbox
 *				 request
 * @hw: dlb2_hw handle for a particular device.
 *
 * This function returns a boolean indicating whether the PF has finished
 * processing a VF->PF mailbox request. It should only be called after sending
 * an asynchronous request with dlb2_send_async_vdev_to_pf_msg().
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 */
bool dlb2_vdev_to_pf_complete(struct dlb2_hw *hw);

/**
 * dlb2_vf_flr_complete() - check the status of a VF FLR
 * @hw: dlb2_hw handle for a particular device.
 *
 * This function returns a boolean indicating whether the PF has finished
 * executing the VF FLR. It should only be called after setting the VF's FLR
 * bit.
 */
bool dlb2_vf_flr_complete(struct dlb2_hw *hw);

/**
 * dlb2_send_async_pf_to_vdev_msg() - (PF only) send a mailbox message to a
 *					vdev
 * @hw: dlb2_hw handle for a particular device.
 * @vdev_id: vdev ID.
 *
 * This function sends a PF->vdev mailbox message. It is asynchronous, so it
 * returns once the message is sent but potentially before the vdev has
 * processed the message. The caller must call dlb2_pf_to_vdev_complete() to
 * determine when the vdev has finished processing the request.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 */
void dlb2_send_async_pf_to_vdev_msg(struct dlb2_hw *hw, unsigned int vdev_id);

/**
 * dlb2_pf_to_vdev_complete() - check the status of an asynchronous mailbox
 *			       request
 * @hw: dlb2_hw handle for a particular device.
 * @vdev_id: vdev ID.
 *
 * This function returns a boolean indicating whether the vdev has finished
 * processing a PF->vdev mailbox request. It should only be called after
 * sending an asynchronous request with dlb2_send_async_pf_to_vdev_msg().
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 */
bool dlb2_pf_to_vdev_complete(struct dlb2_hw *hw, unsigned int vdev_id);

/**
 * dlb2_pf_read_vf_mbox_req() - (PF only) read a VF->PF mailbox request
 * @hw: dlb2_hw handle for a particular device.
 * @vf_id: VF ID.
 * @data: pointer to message data.
 * @len: size, in bytes, of the data array.
 *
 * This function copies one of the PF's VF->PF mailboxes into the array pointed
 * to by data.
 *
 * Return:
 * Returns 0 upon success, <0 otherwise.
 *
 * EINVAL - len >= DLB2_VF2PF_REQ_BYTES.
 */
int dlb2_pf_read_vf_mbox_req(struct dlb2_hw *hw,
			     unsigned int vf_id,
			     void *data,
			     int len);

/**
 * dlb2_pf_read_vf_mbox_resp() - (PF only) read a VF->PF mailbox response
 * @hw: dlb2_hw handle for a particular device.
 * @vf_id: VF ID.
 * @data: pointer to message data.
 * @len: size, in bytes, of the data array.
 *
 * This function copies one of the PF's VF->PF mailboxes into the array pointed
 * to by data.
 *
 * Return:
 * Returns 0 upon success, <0 otherwise.
 *
 * EINVAL - len >= DLB2_VF2PF_RESP_BYTES.
 */
int dlb2_pf_read_vf_mbox_resp(struct dlb2_hw *hw,
			      unsigned int vf_id,
			      void *data,
			      int len);

/**
 * dlb2_pf_write_vf_mbox_resp() - (PF only) write a PF->VF mailbox response
 * @hw: dlb2_hw handle for a particular device.
 * @vf_id: VF ID.
 * @data: pointer to message data.
 * @len: size, in bytes, of the data array.
 *
 * This function copies the user-provided message data into of the PF's VF->PF
 * mailboxes.
 *
 * Return:
 * Returns 0 upon success, <0 otherwise.
 *
 * EINVAL - len >= DLB2_PF2VF_RESP_BYTES.
 */
int dlb2_pf_write_vf_mbox_resp(struct dlb2_hw *hw,
			       unsigned int vf_id,
			       void *data,
			       int len);

/**
 * dlb2_pf_write_vf_mbox_req() - (PF only) write a PF->VF mailbox request
 * @hw: dlb2_hw handle for a particular device.
 * @vf_id: VF ID.
 * @data: pointer to message data.
 * @len: size, in bytes, of the data array.
 *
 * This function copies the user-provided message data into of the PF's VF->PF
 * mailboxes.
 *
 * Return:
 * Returns 0 upon success, <0 otherwise.
 *
 * EINVAL - len >= DLB2_PF2VF_REQ_BYTES.
 */
int dlb2_pf_write_vf_mbox_req(struct dlb2_hw *hw,
			      unsigned int vf_id,
			      void *data,
			      int len);

/**
 * dlb2_vf_read_pf_mbox_resp() - (VF only) read a PF->VF mailbox response
 * @hw: dlb2_hw handle for a particular device.
 * @data: pointer to message data.
 * @len: size, in bytes, of the data array.
 *
 * This function copies the VF's PF->VF mailbox into the array pointed to by
 * data.
 *
 * Return:
 * Returns 0 upon success, <0 otherwise.
 *
 * EINVAL - len >= DLB2_PF2VF_RESP_BYTES.
 */
int dlb2_vf_read_pf_mbox_resp(struct dlb2_hw *hw, void *data, int len);

/**
 * dlb2_vf_read_pf_mbox_req() - (VF only) read a PF->VF mailbox request
 * @hw: dlb2_hw handle for a particular device.
 * @data: pointer to message data.
 * @len: size, in bytes, of the data array.
 *
 * This function copies the VF's PF->VF mailbox into the array pointed to by
 * data.
 *
 * Return:
 * Returns 0 upon success, <0 otherwise.
 *
 * EINVAL - len >= DLB2_PF2VF_REQ_BYTES.
 */
int dlb2_vf_read_pf_mbox_req(struct dlb2_hw *hw, void *data, int len);

/**
 * dlb2_vf_write_pf_mbox_req() - (VF only) write a VF->PF mailbox request
 * @hw: dlb2_hw handle for a particular device.
 * @data: pointer to message data.
 * @len: size, in bytes, of the data array.
 *
 * This function copies the user-provided message data into of the VF's PF->VF
 * mailboxes.
 *
 * Return:
 * Returns 0 upon success, <0 otherwise.
 *
 * EINVAL - len >= DLB2_VF2PF_REQ_BYTES.
 */
int dlb2_vf_write_pf_mbox_req(struct dlb2_hw *hw, void *data, int len);

/**
 * dlb2_vf_write_pf_mbox_resp() - (VF only) write a VF->PF mailbox response
 * @hw: dlb2_hw handle for a particular device.
 * @data: pointer to message data.
 * @len: size, in bytes, of the data array.
 *
 * This function copies the user-provided message data into of the VF's PF->VF
 * mailboxes.
 *
 * Return:
 * Returns 0 upon success, <0 otherwise.
 *
 * EINVAL - len >= DLB2_VF2PF_RESP_BYTES.
 */
int dlb2_vf_write_pf_mbox_resp(struct dlb2_hw *hw, void *data, int len);

/**
 * dlb2_reset_vdev() - reset the hardware owned by a virtual device
 * @hw: dlb2_hw handle for a particular device.
 * @id: virtual device ID
 *
 * This function resets the hardware owned by a vdev, by resetting the vdev's
 * domains one by one.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 */
int dlb2_reset_vdev(struct dlb2_hw *hw, unsigned int id);

/**
 * dlb2_vdev_is_locked() - check whether the vdev's resources are locked
 * @hw: dlb2_hw handle for a particular device.
 * @id: virtual device ID
 *
 * This function returns whether or not the vdev's resource assignments are
 * locked. If locked, no resources can be added to or subtracted from the
 * group.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 */
bool dlb2_vdev_is_locked(struct dlb2_hw *hw, unsigned int id);

/**
 * dlb2_lock_vdev() - lock the vdev's resources
 * @hw: dlb2_hw handle for a particular device.
 * @id: virtual device ID
 *
 * This function sets a flag indicating that the vdev is using its resources.
 * When the vdev is locked, its resource assignment cannot be changed.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 */
void dlb2_lock_vdev(struct dlb2_hw *hw, unsigned int id);

/**
 * dlb2_unlock_vdev() - unlock the vdev's resources
 * @hw: dlb2_hw handle for a particular device.
 * @id: virtual device ID
 *
 * This function unlocks the vdev's resource assignment, allowing it to be
 * modified.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 */
void dlb2_unlock_vdev(struct dlb2_hw *hw, unsigned int id);

/**
 * dlb2_update_vdev_sched_domains() - update the domains assigned to a vdev
 * @hw: dlb2_hw handle for a particular device.
 * @id: virtual device ID
 * @num: number of scheduling domains to assign to this vdev
 *
 * This function assigns num scheduling domains to the specified vdev. If the
 * vdev already has domains assigned, this existing assignment is adjusted
 * accordingly.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 *
 * Return:
 * Returns 0 upon success, <0 otherwise.
 *
 * Errors:
 * EINVAL - id is invalid, or the requested number of resources are
 *	    unavailable.
 * EPERM  - The vdev's resource assignment is locked and cannot be changed.
 */
int dlb2_update_vdev_sched_domains(struct dlb2_hw *hw, u32 id, u32 num);

/**
 * dlb2_update_vdev_ldb_queues() - update the LDB queues assigned to a vdev
 * @hw: dlb2_hw handle for a particular device.
 * @id: virtual device ID
 * @num: number of LDB queues to assign to this vdev
 *
 * This function assigns num LDB queues to the specified vdev. If the vdev
 * already has LDB queues assigned, this existing assignment is adjusted
 * accordingly.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 *
 * Return:
 * Returns 0 upon success, <0 otherwise.
 *
 * Errors:
 * EINVAL - id is invalid, or the requested number of resources are
 *	    unavailable.
 * EPERM  - The vdev's resource assignment is locked and cannot be changed.
 */
int dlb2_update_vdev_ldb_queues(struct dlb2_hw *hw, u32 id, u32 num);

/**
 * dlb2_update_vdev_ldb_ports() - update the LDB ports assigned to a vdev
 * @hw: dlb2_hw handle for a particular device.
 * @id: virtual device ID
 * @num: number of LDB ports to assign to this vdev
 *
 * This function assigns num LDB ports to the specified vdev. If the vdev
 * already has LDB ports assigned, this existing assignment is adjusted
 * accordingly.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 *
 * Return:
 * Returns 0 upon success, <0 otherwise.
 *
 * Errors:
 * EINVAL - id is invalid, or the requested number of resources are
 *	    unavailable.
 * EPERM  - The vdev's resource assignment is locked and cannot be changed.
 */
int dlb2_update_vdev_ldb_ports(struct dlb2_hw *hw, u32 id, u32 num);

/**
 * dlb2_update_vdev_ldb_cos_ports() - update the LDB ports assigned to a vdev
 * @hw: dlb2_hw handle for a particular device.
 * @id: virtual device ID
 * @cos: class-of-service ID
 * @num: number of LDB ports to assign to this vdev
 *
 * This function assigns num LDB ports from class-of-service cos to the
 * specified vdev. If the vdev already has LDB ports from this class-of-service
 * assigned, this existing assignment is adjusted accordingly.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 *
 * Return:
 * Returns 0 upon success, <0 otherwise.
 *
 * Errors:
 * EINVAL - id is invalid, or the requested number of resources are
 *	    unavailable.
 * EPERM  - The vdev's resource assignment is locked and cannot be changed.
 */
int dlb2_update_vdev_ldb_cos_ports(struct dlb2_hw *hw,
				   u32 id,
				   u32 cos,
				   u32 num);

/**
 * dlb2_update_vdev_dir_ports() - update the DIR ports assigned to a vdev
 * @hw: dlb2_hw handle for a particular device.
 * @id: virtual device ID
 * @num: number of DIR ports to assign to this vdev
 *
 * This function assigns num DIR ports to the specified vdev. If the vdev
 * already has DIR ports assigned, this existing assignment is adjusted
 * accordingly.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 *
 * Return:
 * Returns 0 upon success, <0 otherwise.
 *
 * Errors:
 * EINVAL - id is invalid, or the requested number of resources are
 *	    unavailable.
 * EPERM  - The vdev's resource assignment is locked and cannot be changed.
 */
int dlb2_update_vdev_dir_ports(struct dlb2_hw *hw, u32 id, u32 num);

/**
 * dlb2_update_vdev_ldb_credits() - update the vdev's assigned LDB credits
 * @hw: dlb2_hw handle for a particular device.
 * @id: virtual device ID
 * @num: number of LDB credit credits to assign to this vdev
 *
 * This function assigns num LDB credit to the specified vdev. If the vdev
 * already has LDB credits assigned, this existing assignment is adjusted
 * accordingly. vdevs are assigned a contiguous chunk of credits, so this
 * function may fail if a sufficiently large contiguous chunk is not available.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 *
 * Return:
 * Returns 0 upon success, <0 otherwise.
 *
 * Errors:
 * EINVAL - id is invalid, or the requested number of resources are
 *	    unavailable.
 * EPERM  - The vdev's resource assignment is locked and cannot be changed.
 */
int dlb2_update_vdev_ldb_credits(struct dlb2_hw *hw, u32 id, u32 num);

/**
 * dlb2_update_vdev_dir_credits() - update the vdev's assigned DIR credits
 * @hw: dlb2_hw handle for a particular device.
 * @id: virtual device ID
 * @num: number of DIR credits to assign to this vdev
 *
 * This function assigns num DIR credit to the specified vdev. If the vdev
 * already has DIR credits assigned, this existing assignment is adjusted
 * accordingly. vdevs are assigned a contiguous chunk of credits, so this
 * function may fail if a sufficiently large contiguous chunk is not available.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 *
 * Return:
 * Returns 0 upon success, <0 otherwise.
 *
 * Errors:
 * EINVAL - id is invalid, or the requested number of resources are
 *	    unavailable.
 * EPERM  - The vdev's resource assignment is locked and cannot be changed.
 */
int dlb2_update_vdev_dir_credits(struct dlb2_hw *hw, u32 id, u32 num);

/**
 * dlb2_update_vdev_hist_list_entries() - update the vdev's assigned HL entries
 * @hw: dlb2_hw handle for a particular device.
 * @id: virtual device ID
 * @num: number of history list entries to assign to this vdev
 *
 * This function assigns num history list entries to the specified vdev. If the
 * vdev already has history list entries assigned, this existing assignment is
 * adjusted accordingly. vdevs are assigned a contiguous chunk of entries, so
 * this function may fail if a sufficiently large contiguous chunk is not
 * available.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 *
 * Return:
 * Returns 0 upon success, <0 otherwise.
 *
 * Errors:
 * EINVAL - id is invalid, or the requested number of resources are
 *	    unavailable.
 * EPERM  - The vdev's resource assignment is locked and cannot be changed.
 */
int dlb2_update_vdev_hist_list_entries(struct dlb2_hw *hw, u32 id, u32 num);

/**
 * dlb2_update_vdev_atomic_inflights() - update the vdev's atomic inflights
 * @hw: dlb2_hw handle for a particular device.
 * @id: virtual device ID
 * @num: number of atomic inflights to assign to this vdev
 *
 * This function assigns num atomic inflights to the specified vdev. If the vdev
 * already has atomic inflights assigned, this existing assignment is adjusted
 * accordingly. vdevs are assigned a contiguous chunk of entries, so this
 * function may fail if a sufficiently large contiguous chunk is not available.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 *
 * Return:
 * Returns 0 upon success, <0 otherwise.
 *
 * Errors:
 * EINVAL - id is invalid, or the requested number of resources are
 *	    unavailable.
 * EPERM  - The vdev's resource assignment is locked and cannot be changed.
 */
int dlb2_update_vdev_atomic_inflights(struct dlb2_hw *hw, u32 id, u32 num);

/**
 * dlb2_reset_vdev_resources() - reassign the vdev's resources to the PF
 * @hw: dlb2_hw handle for a particular device.
 * @id: virtual device ID
 *
 * This function takes any resources currently assigned to the vdev and
 * reassigns them to the PF.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 *
 * Return:
 * Returns 0 upon success, <0 otherwise.
 *
 * Errors:
 * EINVAL - id is invalid
 * EPERM  - The vdev's resource assignment is locked and cannot be changed.
 */
int dlb2_reset_vdev_resources(struct dlb2_hw *hw, unsigned int id);

/**
 * dlb2_notify_vf() - send an alarm to a VF
 * @hw: dlb2_hw handle for a particular device.
 * @vf_id: VF ID
 * @notification: notification
 *
 * This function sends a notification (as defined in dlb2_mbox.h) to a VF.
 *
 * Return:
 * Returns 0 upon success, <0 if the VF doesn't ACK the PF->VF interrupt.
 */
int dlb2_notify_vf(struct dlb2_hw *hw,
		   unsigned int vf_id,
		   u32 notification);

/**
 * dlb2_vdev_in_use() - query whether a virtual device is in use
 * @hw: dlb2_hw handle for a particular device.
 * @id: virtual device ID
 *
 * This function sends a mailbox request to the vdev to query whether the vdev
 * is in use.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 *
 * Return:
 * Returns 0 for false, 1 for true, and <0 if the mailbox request times out or
 * an internal error occurs.
 */
int dlb2_vdev_in_use(struct dlb2_hw *hw, unsigned int id);

/**
 * dlb2_clr_pmcsr_disable() - power on bulk of DLB 2.0 logic
 * @hw: dlb2_hw handle for a particular device.
 *
 * Clearing the PMCSR must be done at initialization to make the device fully
 * operational.
 */
void dlb2_clr_pmcsr_disable(struct dlb2_hw *hw);

/**
 * dlb2_hw_get_ldb_queue_depth() - returns the depth of a load-balanced queue
 * @hw: dlb2_hw handle for a particular device.
 * @domain_id: domain ID.
 * @args: queue depth args
 * @resp: response structure.
 * @vdev_request: indicates whether this request came from a vdev.
 * @vdev_id: If vdev_request is true, this contains the vdev's ID.
 *
 * This function returns the depth of a load-balanced queue.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise. If an error occurs, resp->status is
 * assigned a detailed error code from enum dlb2_error. If successful, resp->id
 * contains the depth.
 *
 * Errors:
 * EINVAL - Invalid domain ID or queue ID.
 */
int dlb2_hw_get_ldb_queue_depth(struct dlb2_hw *hw,
				u32 domain_id,
				struct dlb2_get_ldb_queue_depth_args *args,
				struct dlb2_cmd_response *resp,
				bool vdev_request,
				unsigned int vdev_id);

/**
 * dlb2_hw_get_dir_queue_depth() - returns the depth of a directed queue
 * @hw: dlb2_hw handle for a particular device.
 * @domain_id: domain ID.
 * @args: queue depth args
 * @resp: response structure.
 * @vdev_request: indicates whether this request came from a vdev.
 * @vdev_id: If vdev_request is true, this contains the vdev's ID.
 *
 * This function returns the depth of a directed queue.
 *
 * A vdev can be either an SR-IOV virtual function or a Scalable IOV virtual
 * device.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise. If an error occurs, resp->status is
 * assigned a detailed error code from enum dlb2_error. If successful, resp->id
 * contains the depth.
 *
 * Errors:
 * EINVAL - Invalid domain ID or queue ID.
 */
int dlb2_hw_get_dir_queue_depth(struct dlb2_hw *hw,
				u32 domain_id,
				struct dlb2_get_dir_queue_depth_args *args,
				struct dlb2_cmd_response *resp,
				bool vdev_request,
				unsigned int vdev_id);

enum dlb2_virt_mode {
	DLB2_VIRT_NONE,
	DLB2_VIRT_SRIOV,
	DLB2_VIRT_SIOV,

	/* NUM_DLB2_VIRT_MODES must be last */
	NUM_DLB2_VIRT_MODES,
};

/**
 * dlb2_hw_set_virt_mode() - set the device's virtualization mode
 * @hw: dlb2_hw handle for a particular device.
 * @mode: either none, SR-IOV, or Scalable IOV.
 *
 * This function sets the virtualization mode of the device. This controls
 * whether the device uses a software or hardware mailbox.
 *
 * This should be called by the PF driver when either SR-IOV or Scalable IOV is
 * selected as the virtualization mechanism, and by the VF/VDEV driver during
 * initialization after recognizing itself as an SR-IOV or Scalable IOV device.
 *
 * Errors:
 * EINVAL - Invalid mode.
 */
int dlb2_hw_set_virt_mode(struct dlb2_hw *hw, enum dlb2_virt_mode mode);

/**
 * dlb2_hw_get_virt_mode() - get the device's virtualization mode
 * @hw: dlb2_hw handle for a particular device.
 *
 * This function gets the virtualization mode of the device.
 */
enum dlb2_virt_mode dlb2_hw_get_virt_mode(struct dlb2_hw *hw);

/**
 * dlb2_hw_get_ldb_port_phys_id() - get a physical port ID from its virt ID
 * @hw: dlb2_hw handle for a particular device.
 * @id: virtual port ID.
 * @vdev_id: vdev ID.
 *
 * Return:
 * Returns >= 0 upon success, -1 otherwise.
 */
s32 dlb2_hw_get_ldb_port_phys_id(struct dlb2_hw *hw,
				 u32 id,
				 unsigned int vdev_id);

/**
 * dlb2_hw_get_dir_port_phys_id() - get a physical port ID from its virt ID
 * @hw: dlb2_hw handle for a particular device.
 * @id: virtual port ID.
 * @vdev_id: vdev ID.
 *
 * Return:
 * Returns >= 0 upon success, -1 otherwise.
 */
s32 dlb2_hw_get_dir_port_phys_id(struct dlb2_hw *hw,
				 u32 id,
				 unsigned int vdev_id);

/**
 * dlb2_hw_register_sw_mbox() - register a software mailbox
 * @hw: dlb2_hw handle for a particular device.
 * @vdev_id: vdev ID.
 * @vdev2pf_mbox: pointer to a 4KB memory page used for vdev->PF communication.
 * @pf2vdev_mbox: pointer to a 4KB memory page used for PF->vdev communication.
 * @pf2vdev_inject: callback function for injecting a PF->vdev interrupt.
 * @inject_arg: user argument for pf2vdev_inject callback.
 *
 * When Scalable IOV is enabled, the VDCM must register a software mailbox for
 * every virtual device during vdev creation.
 *
 * This function notifies the driver to use a software mailbox using the
 * provided pointers, instead of the device's hardware mailbox. When the driver
 * calls mailbox functions like dlb2_pf_write_vf_mbox_req(), the request will
 * go to the software mailbox instead of the hardware one. This is used in
 * Scalable IOV virtualization.
 */
void dlb2_hw_register_sw_mbox(struct dlb2_hw *hw,
			      unsigned int vdev_id,
			      u32 *vdev2pf_mbox,
			      u32 *pf2vdev_mbox,
			      void (*pf2vdev_inject)(void *),
			      void *inject_arg);

/**
 * dlb2_hw_unregister_sw_mbox() - unregister a software mailbox
 * @hw: dlb2_hw handle for a particular device.
 * @vdev_id: vdev ID.
 *
 * This function notifies the driver to stop using a previously registered
 * software mailbox.
 */
void dlb2_hw_unregister_sw_mbox(struct dlb2_hw *hw, unsigned int vdev_id);

/**
 * dlb2_hw_setup_cq_ims_entry() - setup a CQ's IMS entry
 * @hw: dlb2_hw handle for a particular device.
 * @vdev_id: vdev ID.
 * @virt_cq_id: virtual CQ ID.
 * @is_ldb: CQ is load-balanced.
 * @addr_lo: least-significant 32 bits of address.
 * @data: 32 data bits.
 *
 * This sets up the CQ's IMS entry with the provided address and data values.
 * This function should only be called if the device is configured for Scalable
 * IOV virtualization. The upper 32 address bits are fixed in hardware and thus
 * not needed.
 */
void dlb2_hw_setup_cq_ims_entry(struct dlb2_hw *hw,
				unsigned int vdev_id,
				u32 virt_cq_id,
				bool is_ldb,
				u32 addr_lo,
				u32 data);

/**
 * dlb2_hw_clear_cq_ims_entry() - clear a CQ's IMS entry
 * @hw: dlb2_hw handle for a particular device.
 * @vdev_id: vdev ID.
 * @virt_cq_id: virtual CQ ID.
 * @is_ldb: CQ is load-balanced.
 *
 * This clears the CQ's IMS entry, reverting it to its reset state.
 */
void dlb2_hw_clear_cq_ims_entry(struct dlb2_hw *hw,
				unsigned int vdev_id,
				u32 virt_cq_id,
				bool is_ldb);

/**
 * dlb2_hw_register_pasid() - register a vdev's PASID
 * @hw: dlb2_hw handle for a particular device.
 * @vdev_id: vdev ID.
 * @pasid: the vdev's PASID.
 *
 * This function stores the user-supplied PASID, and uses it when configuring
 * the vdev's CQs.
 *
 * Return:
 * Returns >= 0 upon success, -1 otherwise.
 */
int dlb2_hw_register_pasid(struct dlb2_hw *hw,
			   unsigned int vdev_id,
			   unsigned int pasid);

/**
 * dlb2_hw_pending_port_unmaps() - returns the number of unmap operations in
 *	progress.
 * @hw: dlb2_hw handle for a particular device.
 * @domain_id: domain ID.
 * @args: number of unmaps in progress args
 * @resp: response structure.
 * @vf_request: indicates whether this request came from a VF.
 * @vf_id: If vf_request is true, this contains the VF's ID.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise. If an error occurs, resp->status is
 * assigned a detailed error code from enum dlb2_error. If successful, resp->id
 * contains the number of unmaps in progress.
 *
 * Errors:
 * EINVAL - Invalid port ID.
 */
int dlb2_hw_pending_port_unmaps(struct dlb2_hw *hw,
				u32 domain_id,
				struct dlb2_pending_port_unmaps_args *args,
				struct dlb2_cmd_response *resp,
				bool vf_request,
				unsigned int vf_id);

/**
 * dlb2_hw_get_cos_bandwidth() - returns the percent of bandwidth allocated
 *	to a port class-of-service.
 * @hw: dlb2_hw handle for a particular device.
 * @cos_id: class-of-service ID.
 *
 * Return:
 * Returns -EINVAL if cos_id is invalid, else the class' bandwidth allocation.
 */
int dlb2_hw_get_cos_bandwidth(struct dlb2_hw *hw, u32 cos_id);

/**
 * dlb2_hw_set_cos_bandwidth() - set a bandwidth allocation percentage for a
 *	port class-of-service.
 * @hw: dlb2_hw handle for a particular device.
 * @cos_id: class-of-service ID.
 * @bandwidth: class-of-service bandwidth.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise.
 *
 * Errors:
 * EINVAL - Invalid cos ID, bandwidth is greater than 100, or bandwidth would
 *	    cause the total bandwidth across all classes of service to exceed
 *	    100%.
 */
int dlb2_hw_set_cos_bandwidth(struct dlb2_hw *hw, u32 cos_id, u8 bandwidth);

enum dlb2_wd_tmo {
	/* 40s watchdog timeout */
	DLB2_WD_TMO_40S,
	/* 10s watchdog timeout */
	DLB2_WD_TMO_10S,
	/* 1s watchdog timeout */
	DLB2_WD_TMO_1S,

	/* Must be last */
	NUM_DLB2_WD_TMOS,
};

/**
 * dlb2_hw_enable_wd_timer() - enable the CQ watchdog timers with a
 *	caller-specified timeout.
 * @hw: dlb2_hw handle for a particular device.
 * @tmo: watchdog timeout.
 *
 * This function should be called during device initialization and after reset.
 * The watchdog timer interrupt must also be enabled per-CQ, using either
 * dlb2_hw_enable_dir_cq_wd_int() or dlb2_hw_enable_ldb_cq_wd_int().
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise.
 *
 * Errors:
 * EINVAL - Invalid timeout.
 */
int dlb2_hw_enable_wd_timer(struct dlb2_hw *hw, enum dlb2_wd_tmo tmo);

/**
 * dlb2_hw_enable_dir_cq_wd_int() - enable the CQ watchdog interrupt on an
 *	individual CQ.
 * @hw: dlb2_hw handle for a particular device.
 * @id: port ID.
 * @vdev_req: indicates whether this request came from a vdev.
 * @vdev_id: If vdev_request is true, this contains the vdev's ID.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise.
 *
 * Errors:
 * EINVAL - Invalid directed port ID.
 */
int dlb2_hw_enable_dir_cq_wd_int(struct dlb2_hw *hw,
				 u32 id,
				 bool vdev_req,
				 unsigned int vdev_id);

/**
 * dlb2_hw_enable_ldb_cq_wd_int() - enable the CQ watchdog interrupt on an
 *	individual CQ.
 * @hw: dlb2_hw handle for a particular device.
 * @id: port ID.
 * @vdev_req: indicates whether this request came from a vdev.
 * @vdev_id: If vdev_request is true, this contains the vdev's ID.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise.
 *
 * Errors:
 * EINVAL - Invalid load-balanced port ID.
 */
int dlb2_hw_enable_ldb_cq_wd_int(struct dlb2_hw *hw,
				 u32 id,
				 bool vdev_req,
				 unsigned int vdev_id);

/**
 * dlb2_hw_enable_sparse_ldb_cq_mode() - enable sparse mode for load-balanced
 *	ports.
 * @hw: dlb2_hw handle for a particular device.
 *
 * This function must be called prior to configuring scheduling domains.
 */
void dlb2_hw_enable_sparse_ldb_cq_mode(struct dlb2_hw *hw);

/**
 * dlb2_hw_enable_sparse_dir_cq_mode() - enable sparse mode for directed ports.
 * @hw: dlb2_hw handle for a particular device.
 *
 * This function must be called prior to configuring scheduling domains.
 */
void dlb2_hw_enable_sparse_dir_cq_mode(struct dlb2_hw *hw);

/**
 * dlb2_hw_set_qe_arbiter_weights() - program QE arbiter weights
 * @hw: dlb2_hw handle for a particular device.
 * @weight: 8-entry array of arbiter weights.
 *
 * weight[N] programs priority N's weight. In cases where the 8 priorities are
 * reduced to 4 bins, the mapping is:
 * - weight[1] programs bin 0
 * - weight[3] programs bin 1
 * - weight[5] programs bin 2
 * - weight[7] programs bin 3
 */
void dlb2_hw_set_qe_arbiter_weights(struct dlb2_hw *hw, u8 weight[8]);

/**
 * dlb2_hw_set_qid_arbiter_weights() - program QID arbiter weights
 * @hw: dlb2_hw handle for a particular device.
 * @weight: 8-entry array of arbiter weights.
 *
 * weight[N] programs priority N's weight. In cases where the 8 priorities are
 * reduced to 4 bins, the mapping is:
 * - weight[1] programs bin 0
 * - weight[3] programs bin 1
 * - weight[5] programs bin 2
 * - weight[7] programs bin 3
 */
void dlb2_hw_set_qid_arbiter_weights(struct dlb2_hw *hw, u8 weight[8]);

/**
 * dlb2_hw_ldb_cq_interrupt_enabled() - Check if the interrupt is enabled
 * @hw: dlb2_hw handle for a particular device.
 * @port_id: physical load-balanced port ID.
 *
 * This function returns whether the load-balanced CQ interrupt is enabled.
 */
int dlb2_hw_ldb_cq_interrupt_enabled(struct dlb2_hw *hw, int port_id);

/**
 * dlb2_hw_ldb_cq_interrupt_set_mode() - Program the CQ interrupt mode
 * @hw: dlb2_hw handle for a particular device.
 * @port_id: physical load-balanced port ID.
 * @mode: interrupt type (DLB2_CQ_ISR_MODE_{DIS, MSI, MSIX, ADI})
 *
 * This function can be used to disable (MODE_DIS) and re-enable the
 * load-balanced CQ's interrupt. It should only be called after the interrupt
 * has been configured with dlb2_configure_ldb_cq_interrupt().
 */
void dlb2_hw_ldb_cq_interrupt_set_mode(struct dlb2_hw *hw,
				       int port_id,
				       int mode);

/**
 * dlb2_hw_dir_cq_interrupt_enabled() - Check if the interrupt is enabled
 * @hw: dlb2_hw handle for a particular device.
 * @port_id: physical load-balanced port ID.
 *
 * This function returns whether the load-balanced CQ interrupt is enabled.
 */
int dlb2_hw_dir_cq_interrupt_enabled(struct dlb2_hw *hw, int port_id);

/**
 * dlb2_hw_dir_cq_interrupt_set_mode() - Program the CQ interrupt mode
 * @hw: dlb2_hw handle for a particular device.
 * @port_id: physical directed port ID.
 * @mode: interrupt type (DLB2_CQ_ISR_MODE_{DIS, MSI, MSIX, ADI})
 *
 * This function can be used to disable (MODE_DIS) and re-enable the
 * directed CQ's interrupt. It should only be called after the interrupt
 * has been configured with dlb2_configure_dir_cq_interrupt().
 */
void dlb2_hw_dir_cq_interrupt_set_mode(struct dlb2_hw *hw,
				       int port_id,
				       int mode);

#endif /* __DLB2_RESOURCE_H */
