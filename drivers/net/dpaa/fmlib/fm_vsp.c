/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2019-2020 NXP
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <stdbool.h>

#include <rte_common.h>
#include "fm_ext.h"
#include "fm_pcd_ext.h"
#include "fm_port_ext.h"
#include "fm_vsp_ext.h"
#include <dpaa_ethdev.h>

uint32_t
fm_port_vsp_alloc(t_handle h_fm_port,
		  t_fm_port_vspalloc_params *p_params)
{
	t_device *p_dev = (t_device *)h_fm_port;
	ioc_fm_port_vsp_alloc_params_t params;

	_fml_dbg("Calling...\n");
	memset(&params, 0, sizeof(ioc_fm_port_vsp_alloc_params_t));
	memcpy(&params.params, p_params, sizeof(t_fm_port_vspalloc_params));

	if (ioctl(p_dev->fd, FM_PORT_IOC_VSP_ALLOC, &params))
		RETURN_ERROR(MINOR, E_INVALID_OPERATION, NO_MSG);

	_fml_dbg("Called.\n");

	return E_OK;
}

t_handle
fm_vsp_config(t_fm_vsp_params *p_fm_vsp_params)
{
	t_device *p_dev = NULL;
	t_device *p_vsp_dev = NULL;
	ioc_fm_vsp_params_t param;

	p_dev = p_fm_vsp_params->h_fm;

	_fml_dbg("Performing VSP Configuration...\n");

	memset(&param, 0, sizeof(ioc_fm_vsp_params_t));
	memcpy(&param, p_fm_vsp_params, sizeof(t_fm_vsp_params));
	param.vsp_params.h_fm = UINT_TO_PTR(p_dev->id);
	param.id = NULL;

	if (ioctl(p_dev->fd, FM_IOC_VSP_CONFIG, &param)) {
		DPAA_PMD_ERR("%s ioctl error\n", __func__);
		return NULL;
	}

	p_vsp_dev = (t_device *)malloc(sizeof(t_device));
	if (!p_vsp_dev) {
		DPAA_PMD_ERR("FM VSP Params!\n");
		return NULL;
	}
	memset(p_vsp_dev, 0, sizeof(t_device));
	p_vsp_dev->h_user_priv = (t_handle)p_dev;
	p_dev->owners++;
	p_vsp_dev->id = PTR_TO_UINT(param.id);

	_fml_dbg("VSP Configuration completed\n");

	return (t_handle)p_vsp_dev;
}

uint32_t
fm_vsp_init(t_handle h_fm_vsp)
{
	t_device *p_dev = NULL;
	t_device *p_vsp_dev = (t_device *)h_fm_vsp;
	ioc_fm_obj_t id;

	_fml_dbg("Calling...\n");

	p_dev = (t_device *)p_vsp_dev->h_user_priv;
	id.obj = UINT_TO_PTR(p_vsp_dev->id);

	if (ioctl(p_dev->fd, FM_IOC_VSP_INIT, &id)) {
		DPAA_PMD_ERR("%s ioctl error\n", __func__);
		RETURN_ERROR(MINOR, E_INVALID_OPERATION, NO_MSG);
	}

	_fml_dbg("Called.\n");

	return E_OK;
}

uint32_t
fm_vsp_free(t_handle h_fm_vsp)
{
	t_device *p_dev = NULL;
	t_device *p_vsp_dev = (t_device *)h_fm_vsp;
	ioc_fm_obj_t id;

	_fml_dbg("Calling...\n");

	p_dev = (t_device *)p_vsp_dev->h_user_priv;
	id.obj = UINT_TO_PTR(p_vsp_dev->id);

	if (ioctl(p_dev->fd, FM_IOC_VSP_FREE, &id)) {
		DPAA_PMD_ERR("%s ioctl error\n", __func__);
		RETURN_ERROR(MINOR, E_INVALID_OPERATION, NO_MSG);
	}

	p_dev->owners--;
	free(p_vsp_dev);

	_fml_dbg("Called.\n");

	return E_OK;
}

uint32_t
fm_vsp_config_buffer_prefix_content(t_handle h_fm_vsp,
		t_fm_buffer_prefix_content *p_fm_buffer_prefix_content)
{
	t_device *p_dev = NULL;
	t_device *p_vsp_dev = (t_device *)h_fm_vsp;
	ioc_fm_buffer_prefix_content_params_t params;

	_fml_dbg("Calling...\n");

	p_dev = (t_device *)p_vsp_dev->h_user_priv;
	params.p_fm_vsp = UINT_TO_PTR(p_vsp_dev->id);
	memcpy(&params.fm_buffer_prefix_content,
	       p_fm_buffer_prefix_content, sizeof(*p_fm_buffer_prefix_content));

	if (ioctl(p_dev->fd, FM_IOC_VSP_CONFIG_BUFFER_PREFIX_CONTENT,
		  &params)) {
		DPAA_PMD_ERR("%s ioctl error\n", __func__);
		RETURN_ERROR(MINOR, E_INVALID_OPERATION, NO_MSG);
	}

	_fml_dbg("Called.\n");

	return E_OK;
}
