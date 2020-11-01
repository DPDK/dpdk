/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2020 Intel Corporation
 */

#ifndef _DLB2_IFACE_H_
#define _DLB2_IFACE_H_

/* DLB2 PMD Internal interface function pointers.
 * If VDEV (bifurcated PMD),  these will resolve to functions that issue ioctls
 * serviced by DLB kernel module.
 * If PCI (PF PMD),  these will be implemented locally in user mode.
 */

extern void (*dlb2_iface_low_level_io_init)(void);

extern int (*dlb2_iface_open)(struct dlb2_hw_dev *handle, const char *name);

extern int (*dlb2_iface_get_device_version)(struct dlb2_hw_dev *handle,
					    uint8_t *revision);

extern void (*dlb2_iface_hardware_init)(struct dlb2_hw_dev *handle);

extern int (*dlb2_iface_get_cq_poll_mode)(struct dlb2_hw_dev *handle,
					  enum dlb2_cq_poll_modes *mode);

extern int (*dlb2_iface_get_num_resources)(struct dlb2_hw_dev *handle,
				struct dlb2_get_num_resources_args *rsrcs);

#endif /* _DLB2_IFACE_H_ */
