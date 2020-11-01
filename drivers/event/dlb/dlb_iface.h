/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2020 Intel Corporation
 */

#ifndef _DLB_IFACE_H
#define _DLB_IFACE_H

/* DLB PMD Internal interface function pointers.
 * If VDEV (bifurcated PMD), these will resolve to functions that issue ioctls
 * serviced by DLB kernel module.
 * If PCI (PF PMD), these will be implemented locally in user mode.
 */

extern void (*dlb_iface_low_level_io_init)(struct dlb_eventdev *dlb);

extern int (*dlb_iface_open)(struct dlb_hw_dev *handle, const char *name);

extern int (*dlb_iface_get_device_version)(struct dlb_hw_dev *handle,
					   uint8_t *revision);

extern int (*dlb_iface_get_num_resources)(struct dlb_hw_dev *handle,
				   struct dlb_get_num_resources_args *rsrcs);

extern int (*dlb_iface_get_cq_poll_mode)(struct dlb_hw_dev *handle,
					 enum dlb_cq_poll_modes *mode);

#endif /* _DLB_IFACE_H */
