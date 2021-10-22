
/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "cnxk_telemetry.h"
#include "roc_api.h"
#include "roc_priv.h"

static int
cnxk_tel_bphy(struct plt_tel_data *d)
{
	struct idev_cfg *idev;

	idev = idev_get_cfg();
	if (!idev || !idev->bphy)
		return -ENODEV;

	plt_tel_data_add_dict_int(d, "sso_pf_func", roc_bphy_sso_pf_func_get());
	plt_tel_data_add_dict_int(d, "npa_pf_func", roc_bphy_npa_pf_func_get());

	return 0;
}

static int
cnxk_bphy_tel_handle_info(const char *cmd __plt_unused,
			  const char *params __plt_unused,
			  struct plt_tel_data *d)
{
	plt_tel_data_start_dict(d);

	return cnxk_tel_bphy(d);
}

static int
cnxk_bphy_telemetry_register(void)
{
	if (!(roc_model->flag & ROC_MODEL_CNF9K) &&
	    !(roc_model->flag & ROC_MODEL_CNF10K))
		return 0;

	plt_telemetry_register_cmd(
		"/cnxk/bphy/info", cnxk_bphy_tel_handle_info,
		"Returns bphy information. Takes no parameters");

	return 0;
}

PLT_INIT(cnxk_telemetry_bphy_init)
{
	roc_plt_init_cb_register(cnxk_bphy_telemetry_register);
}
