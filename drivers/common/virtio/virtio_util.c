/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2023, NVIDIA CORPORATION & AFFILIATES.
 */

#include <unistd.h>
#include <net/if.h>
#include <rte_malloc.h>

#include "virtio_api.h"

static const char virtio_unknown_err_str[] = "Unknown error";
static const char virtio_success_str[] = "Success";

static const char *virtio_err_str[] =
{
	"Please provide PF name", /*0*/
	"PF device not found",
	"Please provide VF name",
	"VF device not found",
	"VFIO device fd invalid",
	"Number exceed limit", /*5*/
	"Device reset timeout",
	"Devargs parse fail",
	"PF device already add",
	"PF device probe fail",
	"PF deviceid not support", /*10*/
	"PF device feature does not meet requirements",
	"Can't allocate admin queue",
	"Please remove all VF devices before remove PF",
	"VF device already add",
	"No vfid found", /*15*/
	"Can't create VFIO container",
	"Can't bind to VFIO container group",
	"Can't get IOMMU group",
	"Can't allocate device",
	"Can't register device", /*20*/
	"Can't allocate queue",
	"No vector",
	"Can't allocate interrupt",
	"Can't register interrupt",
	"Can't enable interrupt", /*25*/
	"BAR copy wrong",
	"set status quiesced fail",
	"set status freezed fail",
	"Exceed vport limit",
	"Can't access vhost socket path", /*30*/
	"Fail in calling rte_vhost_driver_register",
	"Fail in calling rte_vdpa_get_features",
	"Fail in calling rte_vhost_driver_set_features",
	"Fail in calling rte_vhost_driver_callback_register",
	"Fail in calling rte_vhost_driver_attach_vdpa_device", /*35*/
	"Fail in calling rte_vhost_driver_start",
};

const char *rte_vdpa_err_str_get(int32_t err)
{
#define STD_ERR_MAX	(160)
	const char *p_ret = virtio_unknown_err_str;

	err = (err >= 0) ? err : -err;

	if (err > 0 && err < STD_ERR_MAX)
		return strerror(err);

	if (err == VFE_VDPA_SUCCESS)
		p_ret = virtio_success_str;
	else if (err - VFE_VDPA_ERR_BASE >= 0 &&
		 err - VFE_VDPA_ERR_BASE < VFE_VDPA_ERR_MAX_NUM)
		p_ret = virtio_err_str[err - VFE_VDPA_ERR_BASE];

	return p_ret;
}
