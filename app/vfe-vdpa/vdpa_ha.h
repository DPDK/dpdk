/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2024, NVIDIA CORPORATION & AFFILIATES.
 */

#ifndef _VDPA_HA_H_
#define _VDPA_HA_H_

#include <virtio_ha.h>

int virtio_ha_client_start(ver_time_set set_ver);
int virtio_ha_client_dev_restore_pf(int *total_vf);
int virtio_ha_client_dev_restore_vf(int total_vf);
bool virtio_ha_client_pf_has_restore_vf(const char *pf_name);
uint32_t virtio_ha_client_pf_vf_list(const char *pf_name, struct vdpa_vf_with_devargs **vf_list);
bool virtio_ha_client_vf_in_restore(const char *vf_name);
bool virtio_ha_client_vf_restore_devargs(const char *vf_name, struct vdpa_vf_with_devargs *args);
int virtio_ha_client_init_finish(void);
void virtio_ha_dev_lock(void);
void virtio_ha_dev_unlock(void);

#endif /* _VDPA_HA_H_ */
