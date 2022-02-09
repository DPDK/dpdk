/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2008-2016 Freescale Semiconductor Inc.
 * Copyright 2017-2020 NXP
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
#include <dpaa_ethdev.h>

#define DEV_TO_ID(p) \
	do { \
		t_device *p_dev = (t_device *)p; \
		p = UINT_TO_PTR(p_dev->id); \
	} while (0)

/* Major and minor are in sync with FMD, respin is for fmlib identification */
#define FM_LIB_VERSION_MAJOR	21
#define FM_LIB_VERSION_MINOR	1
#define FM_LIB_VERSION_RESPIN	0

#if (FMD_API_VERSION_MAJOR != FM_LIB_VERSION_MAJOR) || \
	(FMD_API_VERSION_MINOR != FM_LIB_VERSION_MINOR)
#warning FMD and FMLIB version mismatch
#endif

t_handle
fm_open(uint8_t id)
{
	t_device *p_dev;
	int fd;
	char dev_name[20];
	static bool called;
	ioc_fm_api_version_t ver;

	_fml_dbg("Calling...\n");

	p_dev = (t_device *)malloc(sizeof(t_device));
	if (p_dev == NULL)
		return NULL;

	memset(dev_name, 0, 20);
	sprintf(dev_name, "%s%s%d", "/dev/", DEV_FM_NAME, id);
	fd = open(dev_name, O_RDWR);
	if (fd < 0) {
		free(p_dev);
		return NULL;
	}

	p_dev->id = id;
	p_dev->fd = fd;
	if (!called) {
		called = true;
		fm_get_api_version((t_handle)p_dev, &ver);

		if (ver.version.major != FMD_API_VERSION_MAJOR ||
		    ver.version.minor != FMD_API_VERSION_MINOR ||
			ver.version.respin != FMD_API_VERSION_RESPIN) {
			DPAA_PMD_WARN("Compiled against FMD API ver %u.%u.%u",
				      FMD_API_VERSION_MAJOR,
				FMD_API_VERSION_MINOR, FMD_API_VERSION_RESPIN);
			DPAA_PMD_WARN("Running with FMD API ver %u.%u.%u",
				      ver.version.major, ver.version.minor,
				ver.version.respin);
		}
	}
	_fml_dbg("Finishing.\n");

	return (t_handle)p_dev;
}

void fm_close(t_handle h_fm)
{
	t_device *p_dev = (t_device *)h_fm;

	_fml_dbg("Calling...\n");

	close(p_dev->fd);
	free(p_dev);

	_fml_dbg("Finishing.\n");
}

uint32_t
fm_get_api_version(t_handle h_fm, ioc_fm_api_version_t *p_version)
{
	t_device *p_dev = (t_device *)h_fm;
	int ret;

	_fml_dbg("Calling...\n");

	ret = ioctl(p_dev->fd, FM_IOC_GET_API_VERSION, p_version);
	if (ret) {
		DPAA_PMD_ERR("cannot get API version, error %i (%s)\n",
			     errno, strerror(errno));
		RETURN_ERROR(MINOR, E_INVALID_OPERATION, NO_MSG);
	}
	_fml_dbg("Finishing.\n");

	return E_OK;
}

t_handle
fm_pcd_open(t_fm_pcd_params *p_fm_pcd_params)
{
	t_device *p_dev;
	int fd;
	char dev_name[20];

	_fml_dbg("Calling...\n");

	p_dev = (t_device *)malloc(sizeof(t_device));
	if (p_dev == NULL)
		return NULL;

	memset(dev_name, 0, 20);
	sprintf(dev_name, "%s%s%u-pcd", "/dev/", DEV_FM_NAME,
		(uint32_t)((t_device *)p_fm_pcd_params->h_fm)->id);
	fd = open(dev_name, O_RDWR);
	if (fd < 0) {
		free(p_dev);
		return NULL;
	}

	p_dev->id = ((t_device *)p_fm_pcd_params->h_fm)->id;
	p_dev->fd = fd;
	p_dev->owners = 0;

	_fml_dbg("Finishing.\n");

	return (t_handle)p_dev;
}

void
fm_pcd_close(t_handle h_fm_pcd)
{
	t_device *p_dev = (t_device *)h_fm_pcd;

	_fml_dbg("Calling...\n");

	close(p_dev->fd);

	if (p_dev->owners) {
		printf("\nTry delete a prev created pcd handler(owners:%u)!\n",
			p_dev->owners);
		return;
	}

	free(p_dev);

	_fml_dbg("Finishing.\n");
}

uint32_t
fm_pcd_enable(t_handle h_fm_pcd)
{
	t_device *p_dev = (t_device *)h_fm_pcd;

	_fml_dbg("Calling...\n");

	if (ioctl(p_dev->fd, FM_PCD_IOC_ENABLE))
		RETURN_ERROR(MINOR, E_INVALID_OPERATION, NO_MSG);

	_fml_dbg("Finishing.\n");

	return E_OK;
}

uint32_t
fm_pcd_disable(t_handle h_fm_pcd)
{
	t_device *p_dev = (t_device *)h_fm_pcd;

	_fml_dbg("Calling...\n");

	if (ioctl(p_dev->fd, FM_PCD_IOC_DISABLE))
		RETURN_ERROR(MINOR, E_INVALID_OPERATION, NO_MSG);

	_fml_dbg("Finishing.\n");

	return E_OK;
}

t_handle
fm_pcd_net_env_characteristics_set(t_handle h_fm_pcd,
		ioc_fm_pcd_net_env_params_t *params)
{
	t_device *p_pcd_dev = (t_device *)h_fm_pcd;
	t_device *p_dev = NULL;

	_fml_dbg("Calling...\n");

	params->id = NULL;

	if (ioctl(p_pcd_dev->fd, FM_PCD_IOC_NET_ENV_CHARACTERISTICS_SET,
		  params))
		return NULL;

	p_dev = (t_device *)malloc(sizeof(t_device));
	if (p_dev == NULL)
		return NULL;

	memset(p_dev, 0, sizeof(t_device));
	p_dev->h_user_priv = (t_handle)p_pcd_dev;
	p_pcd_dev->owners++;
	p_dev->id = PTR_TO_UINT(params->id);

	_fml_dbg("Finishing.\n");

	return (t_handle)p_dev;
}

uint32_t
fm_pcd_net_env_characteristics_delete(t_handle h_net_env)
{
	t_device *p_dev = (t_device *)h_net_env;
	t_device *p_pcd_dev = NULL;
	ioc_fm_obj_t id;

	_fml_dbg("Calling...\n");

	p_pcd_dev = (t_device *)p_dev->h_user_priv;
	id.obj = UINT_TO_PTR(p_dev->id);

	if (ioctl(p_pcd_dev->fd, FM_PCD_IOC_NET_ENV_CHARACTERISTICS_DELETE,
		  &id))
		RETURN_ERROR(MINOR, E_INVALID_OPERATION, NO_MSG);

	p_pcd_dev->owners--;
	free(p_dev);

	_fml_dbg("Finishing.\n");

	return E_OK;
}

t_handle
fm_pcd_kg_scheme_set(t_handle h_fm_pcd,
		     ioc_fm_pcd_kg_scheme_params_t *params)
{
	t_device *p_pcd_dev = (t_device *)h_fm_pcd;
	t_device *p_dev = NULL;
	int ret;

	_fml_dbg("Calling...\n");

	params->id = NULL;

	if (params->param.modify) {
		if (params->param.scm_id.scheme_id)
			DEV_TO_ID(params->param.scm_id.scheme_id);
		else
			return NULL;
	}

	/* correct h_net_env param from scheme */
	if (params->param.net_env_params.net_env_id)
		DEV_TO_ID(params->param.net_env_params.net_env_id);

	/* correct next engine params handlers: cc*/
	if (params->param.next_engine == e_IOC_FM_PCD_CC &&
	    params->param.kg_next_engine_params.cc.tree_id)
		DEV_TO_ID(params->param.kg_next_engine_params.cc.tree_id);

	ret = ioctl(p_pcd_dev->fd, FM_PCD_IOC_KG_SCHEME_SET, params);
	if (ret) {
		DPAA_PMD_ERR("  cannot set kg scheme, error %i (%s)\n",
			     errno, strerror(errno));
		return NULL;
	}

	p_dev = (t_device *)malloc(sizeof(t_device));
	if (p_dev == NULL)
		return NULL;

	memset(p_dev, 0, sizeof(t_device));
	p_dev->h_user_priv = (t_handle)p_pcd_dev;
	/* increase owners only if a new scheme is created */
	if (!params->param.modify)
		p_pcd_dev->owners++;
	p_dev->id = PTR_TO_UINT(params->id);

	_fml_dbg("Finishing.\n");

	return (t_handle)p_dev;
}

uint32_t
fm_pcd_kg_scheme_delete(t_handle h_scheme)
{
	t_device *p_dev = (t_device *)h_scheme;
	t_device *p_pcd_dev = NULL;
	ioc_fm_obj_t id;

	_fml_dbg("Calling...\n");

	p_pcd_dev =  (t_device *)p_dev->h_user_priv;
	id.obj = UINT_TO_PTR(p_dev->id);

	if (ioctl(p_pcd_dev->fd, FM_PCD_IOC_KG_SCHEME_DELETE, &id)) {
		DPAA_PMD_WARN("cannot delete kg scheme, error %i (%s)\n",
			      errno, strerror(errno));
		RETURN_ERROR(MINOR, E_INVALID_OPERATION, NO_MSG);
	}

	p_pcd_dev->owners--;
	free(p_dev);

	_fml_dbg("Finishing.\n");

	return E_OK;
}

typedef struct {
	e_fm_port_type	port_type;	/**< Port type */
	uint8_t		port_id;	/**< Port Id - relative to type */
} t_fm_port;

t_handle
fm_port_open(t_fm_port_params *p_fm_port_params)
{
	t_device *p_dev;
	int fd;
	char dev_name[30];
	t_fm_port *p_fm_port;

	_fml_dbg("Calling...\n");

	p_dev = (t_device *)malloc(sizeof(t_device));
	if (p_dev == NULL)
		return NULL;

	memset(p_dev, 0, sizeof(t_device));

	p_fm_port = (t_fm_port *)malloc(sizeof(t_fm_port));
	if (!p_fm_port) {
		free(p_dev);
		return NULL;
	}
	memset(p_fm_port, 0, sizeof(t_fm_port));
	memset(dev_name, 0, sizeof(dev_name));
	switch (p_fm_port_params->port_type) {
	case e_FM_PORT_TYPE_OH_OFFLINE_PARSING:
		sprintf(dev_name, "%s%s%u-port-oh%d", "/dev/", DEV_FM_NAME,
			(uint32_t)((t_device *)p_fm_port_params->h_fm)->id,
			p_fm_port_params->port_id);
		break;
	case e_FM_PORT_TYPE_RX:
		sprintf(dev_name, "%s%s%u-port-rx%d", "/dev/", DEV_FM_NAME,
			(uint32_t)((t_device *)p_fm_port_params->h_fm)->id,
			p_fm_port_params->port_id);
		break;
	case e_FM_PORT_TYPE_RX_10G:
		sprintf(dev_name, "%s%s%u-port-rx%d", "/dev/", DEV_FM_NAME,
			(uint32_t)((t_device *)p_fm_port_params->h_fm)->id,
			FM_MAX_NUM_OF_1G_RX_PORTS + p_fm_port_params->port_id);
		break;
	case e_FM_PORT_TYPE_TX:
		sprintf(dev_name, "%s%s%u-port-tx%d", "/dev/", DEV_FM_NAME,
			(uint32_t)((t_device *)p_fm_port_params->h_fm)->id,
			p_fm_port_params->port_id);
		break;
	case e_FM_PORT_TYPE_TX_10G:
		sprintf(dev_name, "%s%s%u-port-tx%d", "/dev/", DEV_FM_NAME,
			(uint32_t)((t_device *)p_fm_port_params->h_fm)->id,
			FM_MAX_NUM_OF_1G_TX_PORTS + p_fm_port_params->port_id);
		break;
	default:
		free(p_fm_port);
		free(p_dev);
		return NULL;
	}

	fd = open(dev_name, O_RDWR);
	if (fd < 0) {
		free(p_fm_port);
		free(p_dev);
		return NULL;
	}

	p_fm_port->port_type = p_fm_port_params->port_type;
	p_fm_port->port_id = p_fm_port_params->port_id;
	p_dev->id = p_fm_port_params->port_id;
	p_dev->fd = fd;
	p_dev->h_user_priv = (t_handle)p_fm_port;

	_fml_dbg("Finishing.\n");

	return (t_handle)p_dev;
}

void
fm_port_close(t_handle h_fm_port)
{
	t_device *p_dev = (t_device *)h_fm_port;

	_fml_dbg("Calling...\n");

	close(p_dev->fd);
	free(p_dev->h_user_priv);
	free(p_dev);

	_fml_dbg("Finishing.\n");
}

uint32_t
fm_port_disable(t_handle h_fm_port)
{
	t_device *p_dev = (t_device *)h_fm_port;

	_fml_dbg("Calling...\n");

	if (ioctl(p_dev->fd, FM_PORT_IOC_DISABLE))
		RETURN_ERROR(MINOR, E_INVALID_OPERATION, NO_MSG);

	_fml_dbg("Finishing.\n");

	return E_OK;
}

uint32_t
fm_port_enable(t_handle h_fm_port)
{
	t_device *p_dev = (t_device *)h_fm_port;

	_fml_dbg("Calling...\n");

	if (ioctl(p_dev->fd, FM_PORT_IOC_ENABLE))
		RETURN_ERROR(MINOR, E_INVALID_OPERATION, NO_MSG);

	_fml_dbg("Finishing.\n");

	return E_OK;
}

uint32_t
fm_port_set_pcd(t_handle h_fm_port,
		ioc_fm_port_pcd_params_t *p)
{
	t_device *p_dev = (t_device *)h_fm_port;

	_fml_dbg("Calling...\n");

	/* correct h_net_env param from t_fm_portPcdParams */
	DEV_TO_ID(p->net_env_id);

	/* correct pcd structures according to what support was set */
	if (p->pcd_support == e_IOC_FM_PCD_PRS_KG_AND_CC ||
		p->pcd_support == e_IOC_FM_PCD_PRS_KG_AND_CC_AND_PLCR ||
		p->pcd_support == e_IOC_FM_PCD_PRS_CC) {
		if (p->p_cc_params && p->p_cc_params->cc_tree_id)
			DEV_TO_ID(p->p_cc_params->cc_tree_id);
		else
			DPAA_PMD_WARN("Coarse Classification not set !");
	}

	if (p->pcd_support == e_IOC_FM_PCD_PRS_KG ||
		p->pcd_support == e_IOC_FM_PCD_PRS_KG_AND_CC ||
		p->pcd_support == e_IOC_FM_PCD_PRS_KG_AND_CC_AND_PLCR ||
		p->pcd_support == e_IOC_FM_PCD_PRS_KG_AND_PLCR){
		if (p->p_kg_params) {
			uint32_t i;
			ioc_fm_port_pcd_kg_params_t *kg_params;

			kg_params = p->p_kg_params;

			for (i = 0; i < kg_params->num_schemes; i++)
				if (kg_params->scheme_ids[i])
					DEV_TO_ID(kg_params->scheme_ids[i]);
				else
					DPAA_PMD_WARN("Scheme:%u not set!!", i);

			if (kg_params->direct_scheme)
				DEV_TO_ID(kg_params->direct_scheme_id);
		} else {
			DPAA_PMD_WARN("KeyGen not set !");
		}
	}

	if (p->pcd_support == e_IOC_FM_PCD_PLCR_ONLY ||
		p->pcd_support == e_IOC_FM_PCD_PRS_PLCR ||
		p->pcd_support == e_IOC_FM_PCD_PRS_KG_AND_CC_AND_PLCR ||
		p->pcd_support == e_IOC_FM_PCD_PRS_KG_AND_PLCR) {
		if (p->p_plcr_params) {
			if (p->p_plcr_params->plcr_profile_id)
				DEV_TO_ID(p->p_plcr_params->plcr_profile_id);
			else
				DPAA_PMD_WARN("Policer not set !");
		}
	}

	if (p->p_ip_reassembly_manip)
		DEV_TO_ID(p->p_ip_reassembly_manip);

	if (p->p_capwap_reassembly_manip)
		DEV_TO_ID(p->p_capwap_reassembly_manip);

	if (ioctl(p_dev->fd, FM_PORT_IOC_SET_PCD, p))
		RETURN_ERROR(MINOR, E_INVALID_OPERATION, NO_MSG);

	_fml_dbg("Finishing.\n");

	return E_OK;
}

uint32_t
fm_port_delete_pcd(t_handle h_fm_port)
{
	t_device *p_dev = (t_device *)h_fm_port;

	_fml_dbg("Calling...\n");

	if (ioctl(p_dev->fd, FM_PORT_IOC_DELETE_PCD))
		RETURN_ERROR(MINOR, E_INVALID_OPERATION, NO_MSG);

	_fml_dbg("Finishing.\n");

	return E_OK;
}

t_handle
create_device(t_handle h_user_priv, t_handle h_dev_id)
{
	t_device *p_user_priv_dev = (t_device *)h_user_priv;
	t_device *p_dev = NULL;

	_fml_dbg("Calling...\n");

	p_dev = (t_device *)malloc(sizeof(t_device));
	if (p_dev == NULL)
		return NULL;

	memset(p_dev, 0, sizeof(t_device));
	p_dev->h_user_priv = h_user_priv;
	p_user_priv_dev->owners++;
	p_dev->id = PTR_TO_UINT(h_dev_id);

	_fml_dbg("Finishing.\n");

	return (t_handle)p_dev;
}

t_handle
get_device_id(t_handle h_dev)
{
	t_device *p_dev = (t_device *)h_dev;

	return (t_handle)p_dev->id;
}
