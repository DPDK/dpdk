/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "hw_mod_backend.h"

#define _MOD_ "HSH"
#define _VER_ be->hsh.ver

bool hw_mod_hsh_present(struct flow_api_backend_s *be)
{
	return be->iface->get_hsh_present(be->be_dev);
}

int hw_mod_hsh_alloc(struct flow_api_backend_s *be)
{
	int nb;
	_VER_ = be->iface->get_hsh_version(be->be_dev);
	NT_LOG(DBG, FILTER, "HSH MODULE VERSION  %i.%i", VER_MAJOR(_VER_), VER_MINOR(_VER_));

	/* detect number of HSH categories supported by FPGA */
	nb = be->iface->get_nb_hsh_categories(be->be_dev);

	if (nb <= 0) {
		COUNT_ERROR_LOG(hsh_categories);
		return COUNT_ERROR;
	}

	be->hsh.nb_rcp = (uint32_t)nb;

	/* detect if Toeplitz hashing function is supported by FPGA */
	nb = be->iface->get_nb_hsh_toeplitz(be->be_dev);

	if (nb < 0) {
		COUNT_ERROR_LOG(hsh_toeplitz);
		return COUNT_ERROR;
	}

	be->hsh.toeplitz = (uint32_t)nb;

	switch (_VER_) {
	case 5:
		if (!callocate_mod((struct common_func_s *)&be->hsh, 1, &be->hsh.v5.rcp,
				be->hsh.nb_rcp, sizeof(struct hsh_v5_rcp_s)))
			return -1;

		break;

	/* end case 5 */
	default:
		UNSUP_VER_LOG;
		return UNSUP_VER;
	}

	return 0;
}

void hw_mod_hsh_free(struct flow_api_backend_s *be)
{
	if (be->hsh.base) {
		free(be->hsh.base);
		be->hsh.base = NULL;
	}
}

int hw_mod_hsh_reset(struct flow_api_backend_s *be)
{
	/* Zero entire cache area */
	zero_module_cache((struct common_func_s *)(&be->hsh));

	NT_LOG(DBG, FILTER, "INIT HSH RCP");
	return hw_mod_hsh_rcp_flush(be, 0, be->hsh.nb_rcp);
}

int hw_mod_hsh_rcp_flush(struct flow_api_backend_s *be, int start_idx, int count)
{
	if (count == ALL_ENTRIES)
		count = be->hsh.nb_rcp;

	if ((start_idx + count) > (int)be->hsh.nb_rcp)  {
		INDEX_TOO_LARGE_LOG;
		return INDEX_TOO_LARGE;
	}

	return be->iface->hsh_rcp_flush(be->be_dev, &be->hsh, start_idx, count);
}
