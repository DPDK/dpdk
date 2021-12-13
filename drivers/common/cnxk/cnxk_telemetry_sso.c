
/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include <unistd.h>

#include "cnxk_telemetry.h"
#include "roc_api.h"
#include "roc_priv.h"

static int
cnxk_tel_sso(struct plt_tel_data *d)
{
	struct roc_sso *roc_sso;
	struct sso *sso;

	roc_sso = idev_sso_get();
	if (roc_sso == NULL)
		return SSO_ERR_DEVICE_NOT_BOUNDED;

	sso = roc_sso_to_sso_priv(roc_sso);
	plt_tel_data_add_dict_ptr(d, "roc_sso", roc_sso);
	plt_tel_data_add_dict_ptr(d, "sso", sso);
	plt_tel_data_add_dict_int(d, "max_hws", roc_sso->max_hws);
	plt_tel_data_add_dict_int(d, "max_hwgrp", roc_sso->max_hwgrp);
	plt_tel_data_add_dict_int(d, "nb_hws", roc_sso->nb_hws);
	plt_tel_data_add_dict_int(d, "nb_hwgrp", roc_sso->nb_hwgrp);
	plt_tel_data_add_dict_int(d, "pf_func", sso->dev.pf_func);
	plt_tel_data_add_dict_int(d, "pid", getpid());

	return 0;
}

static int
cnxk_sso_tel_handle_info(const char *cmd __plt_unused,
			 const char *params __plt_unused,
			 struct plt_tel_data *d)
{
	plt_tel_data_start_dict(d);
	cnxk_tel_sso(d);
	return 0;
}

PLT_INIT(cnxk_telemetry_sso_init)
{
	plt_telemetry_register_cmd(
		"/cnxk/sso/info", cnxk_sso_tel_handle_info,
		"Returns sso information. Takes no parameters");
}
