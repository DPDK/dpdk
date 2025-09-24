/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include "hw_mod_backend.h"
#include "rte_debug.h"

#include <stdlib.h>
#include <string.h>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

static const struct {
	const char *name;
	int (*allocate)(struct flow_api_backend_s *be);
	void (*free)(struct flow_api_backend_s *be);
	int (*reset)(struct flow_api_backend_s *be);
	bool (*present)(struct flow_api_backend_s *be);
} module[] = {
	{ "CAT", nthw_mod_cat_alloc, nthw_mod_cat_free, nthw_mod_cat_reset, nthw_mod_cat_present },
	{ "KM", nthw_mod_km_alloc, nthw_mod_km_free, nthw_mod_km_reset, nthw_mod_km_present },
	{ "FLM", nthw_mod_flm_alloc, nthw_mod_flm_free, nthw_mod_flm_reset, nthw_mod_flm_present },
	{ "HSH", nthw_mod_hsh_alloc, nthw_mod_hsh_free, nthw_mod_hsh_reset, nthw_mod_hsh_present },
	{ "QSL", nthw_mod_qsl_alloc, nthw_mod_qsl_free, nthw_mod_qsl_reset, nthw_mod_qsl_present },
	{
		"SLC LR", nthw_mod_slc_lr_alloc, nthw_mod_slc_lr_free, nthw_mod_slc_lr_reset,
		nthw_mod_slc_lr_present
	},
	{ "PDB", nthw_mod_pdb_alloc, nthw_mod_pdb_free, nthw_mod_pdb_reset, nthw_mod_pdb_present },
	{ "TPE", nthw_mod_tpe_alloc, nthw_mod_tpe_free, nthw_mod_tpe_reset, nthw_mod_tpe_present },
};
#define MOD_COUNT (ARRAY_SIZE(module))

void *nthw_callocate_mod(struct common_func_s *mod, int sets, ...)
{
#define MAX_SETS 38
	void *base = NULL;
	void **plist[MAX_SETS];
	int len[MAX_SETS];
	int offs[MAX_SETS];
	unsigned int total_bytes = 0;
	int cnt, elem_size;

	RTE_ASSERT(sets <= MAX_SETS);
	RTE_ASSERT(sets > 0);

	va_list args;
	va_start(args, sets);

	for (int i = 0; i < sets; i++) {
		plist[i] = va_arg(args, void *);
		cnt = va_arg(args, int);
		elem_size = va_arg(args, int);
		offs[i] = EXTRA_INDEXES * elem_size;
		len[i] = offs[i] + cnt * elem_size;
		total_bytes += len[i];
	}

	if (total_bytes > 0) {
		base = calloc(1, total_bytes);

		if (base) {
			char *p_b = (char *)base;

			for (int i = 0; i < sets; i++) {
				*plist[i] = (void *)((char *)p_b + offs[i]);
				p_b += len[i];
			}

		} else {
			NT_LOG(ERR, FILTER, "ERROR: module memory allocation failed");
		}

	} else {
		NT_LOG(ERR, FILTER, "ERROR: module request to allocate 0 bytes of memory");
	}

	va_end(args);

	mod->base = base;
	mod->allocated_size = total_bytes;

	return base;
}

void nthw_zero_module_cache(struct common_func_s *mod)
{
	if (mod)
		memset(mod->base, 0, mod->allocated_size);
}

int nthw_flow_api_backend_init(struct flow_api_backend_s *dev,
	const struct flow_api_backend_ops *iface,
	void *be_dev)
{
	RTE_ASSERT(dev);
	dev->iface = iface;
	dev->be_dev = be_dev;
	dev->num_phy_ports = iface->get_nb_phy_port(be_dev);
	dev->num_rx_ports = iface->get_nb_rx_port(be_dev);
	dev->max_categories = iface->get_nb_categories(be_dev);
	dev->max_queues = iface->get_nb_queues(be_dev);

	NT_LOG(DBG,
		FILTER,
		"*************** FLOW REGISTER MODULES AND INITIALIZE - SET ALL TO DEFAULT *****************");

	/*
	 * Create Cache and SW, version independent, NIC module representation
	 */
	for (unsigned int mod = 0; mod < MOD_COUNT; mod++) {
		if (!module[mod].present(dev))
			continue;

		if (module[mod].allocate(dev) == 0 && module[mod].reset(dev) == 0) {
			/* OK */
			continue;
		}

		NT_LOG(ERR,
			FILTER,
			"ERROR: Initialization of NIC module failed : [ %s ]",
			module[mod].name);
		nthw_flow_api_backend_done(dev);
		NT_LOG(ERR,
			FILTER,
			"*************** Failed to create Binary Flow API *******************");
		NT_LOG(ERR,
			FILTER,
			"******** ERROR ERROR: Binary Flow API will not be available ********");
		NT_LOG(ERR,
			FILTER,
			"********************************************************************");
		return -1;
	}

	return 0;
}

int nthw_flow_api_backend_done(struct flow_api_backend_s *dev)
{
	for (unsigned int mod = 0; mod < MOD_COUNT; mod++)
		module[mod].free(dev);

	return 0;
}
