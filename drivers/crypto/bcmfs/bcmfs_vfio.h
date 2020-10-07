/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Broadcom
 * All rights reserved.
 */

#ifndef _BCMFS_VFIO_H_
#define _BCMFS_VFIO_H_

/* Attach the bcmfs device to vfio */
int
bcmfs_attach_vfio(struct bcmfs_device *dev);

/* Release the bcmfs device from vfio */
void
bcmfs_release_vfio(struct bcmfs_device *dev);

#endif /* _BCMFS_VFIO_H_ */
