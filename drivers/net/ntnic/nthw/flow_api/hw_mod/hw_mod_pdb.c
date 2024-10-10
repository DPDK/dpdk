/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "hw_mod_backend.h"

#define _MOD_ "PDB"
#define _VER_ be->pdb.ver

bool hw_mod_pdb_present(struct flow_api_backend_s *be)
{
	return be->iface->get_pdb_present(be->be_dev);
}

int hw_mod_pdb_alloc(struct flow_api_backend_s *be)
{
	int nb;
	_VER_ = be->iface->get_pdb_version(be->be_dev);
	NT_LOG(DBG, FILTER, "PDB MODULE VERSION  %i.%i", VER_MAJOR(_VER_), VER_MINOR(_VER_));

	nb = be->iface->get_nb_pdb_categories(be->be_dev);

	if (nb <= 0) {
		COUNT_ERROR_LOG(pdb_categories);
		return COUNT_ERROR;
	}

	be->pdb.nb_pdb_rcp_categories = (uint32_t)nb;

	switch (_VER_) {
	case 9:
		if (!callocate_mod((struct common_func_s *)&be->pdb, 2, &be->pdb.v9.rcp,
				be->pdb.nb_pdb_rcp_categories, sizeof(struct pdb_v9_rcp_s),
				&be->pdb.v9.config, 1, sizeof(struct pdb_v9_config_s)))
			return -1;

		break;

	/* end case 9 */
	default:
		UNSUP_VER_LOG;
		return UNSUP_VER;
	}

	return 0;
}

void hw_mod_pdb_free(struct flow_api_backend_s *be)
{
	if (be->pdb.base) {
		free(be->pdb.base);
		be->pdb.base = NULL;
	}
}

int hw_mod_pdb_reset(struct flow_api_backend_s *be)
{
	int err = 0;
	/* Zero entire cache area */
	zero_module_cache((struct common_func_s *)(&be->hsh));

	NT_LOG(DBG, FILTER, "INIT PDB RCP");
	err |= hw_mod_pdb_rcp_flush(be, 0, ALL_ENTRIES);

	NT_LOG(DBG, FILTER, "INIT PDB CONFIG");
	err |= hw_mod_pdb_config_flush(be);
	return err;
}

int hw_mod_pdb_rcp_flush(struct flow_api_backend_s *be, int start_idx, int count)
{
	if (count == ALL_ENTRIES)
		count = be->pdb.nb_pdb_rcp_categories;

	if ((unsigned int)(start_idx + count) > be->pdb.nb_pdb_rcp_categories) {
		INDEX_TOO_LARGE_LOG;
		return INDEX_TOO_LARGE;
	}

	return be->iface->pdb_rcp_flush(be->be_dev, &be->pdb, start_idx, count);
}

int hw_mod_pdb_config_flush(struct flow_api_backend_s *be)
{
	return be->iface->pdb_config_flush(be->be_dev, &be->pdb);
}
