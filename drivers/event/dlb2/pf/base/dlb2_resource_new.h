/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2020 Intel Corporation
 */

#ifndef __DLB2_RESOURCE_NEW_H
#define __DLB2_RESOURCE_NEW_H

#include "dlb2_user.h"
#include "dlb2_osdep_types.h"

/**
 * dlb2_resource_init() - initialize the device
 * @hw: pointer to struct dlb2_hw.
 * @ver: device version.
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
int dlb2_resource_init(struct dlb2_hw *hw, enum dlb2_hw_ver ver);

/**
 * dlb2_clr_pmcsr_disable() - power on bulk of DLB 2.0 logic
 * @hw: dlb2_hw handle for a particular device.
 * @ver: device version.
 *
 * Clearing the PMCSR must be done at initialization to make the device fully
 * operational.
 */
void dlb2_clr_pmcsr_disable(struct dlb2_hw *hw, enum dlb2_hw_ver ver);

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
 * dlb2_resource_free() - free device state memory
 * @hw: dlb2_hw handle for a particular device.
 *
 * This function frees software state pointed to by dlb2_hw. This function
 * should be called when resetting the device or unloading the driver.
 */
void dlb2_resource_free(struct dlb2_hw *hw);

#endif /* __DLB2_RESOURCE_NEW_H */
