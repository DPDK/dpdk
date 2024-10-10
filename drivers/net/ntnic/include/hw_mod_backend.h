/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef _HW_MOD_BACKEND_H_
#define _HW_MOD_BACKEND_H_

#define MAX_PHYS_ADAPTERS 8

enum debug_mode_e {
	FLOW_BACKEND_DEBUG_MODE_NONE = 0x0000,
	FLOW_BACKEND_DEBUG_MODE_WRITE = 0x0001
};

struct flow_api_backend_ops {
	int version;
	int (*set_debug_mode)(void *dev, enum debug_mode_e mode);
	int (*get_nb_phy_port)(void *dev);
	int (*get_nb_rx_port)(void *dev);
	int (*get_ltx_avail)(void *dev);
	int (*get_nb_cat_funcs)(void *dev);
	int (*get_nb_categories)(void *dev);
	int (*get_nb_cat_km_if_cnt)(void *dev);
	int (*get_nb_cat_km_if_m0)(void *dev);
	int (*get_nb_cat_km_if_m1)(void *dev);

	int (*get_nb_queues)(void *dev);
	int (*get_nb_km_flow_types)(void *dev);
	int (*get_nb_pm_ext)(void *dev);
	int (*get_nb_len)(void *dev);
	int (*get_kcc_size)(void *dev);
	int (*get_kcc_banks)(void *dev);
	int (*get_nb_km_categories)(void *dev);
	int (*get_nb_km_cam_banks)(void *dev);
	int (*get_nb_km_cam_record_words)(void *dev);
	int (*get_nb_km_cam_records)(void *dev);
	int (*get_nb_km_tcam_banks)(void *dev);
	int (*get_nb_km_tcam_bank_width)(void *dev);
	int (*get_nb_flm_categories)(void *dev);
	int (*get_nb_flm_size_mb)(void *dev);
	int (*get_nb_flm_entry_size)(void *dev);
	int (*get_nb_flm_variant)(void *dev);
	int (*get_nb_flm_prios)(void *dev);
	int (*get_nb_flm_pst_profiles)(void *dev);
	int (*get_nb_flm_scrub_profiles)(void *dev);
	int (*get_nb_flm_load_aps_max)(void *dev);
	int (*get_nb_qsl_categories)(void *dev);
	int (*get_nb_qsl_qst_entries)(void *dev);
	int (*get_nb_pdb_categories)(void *dev);
	int (*get_nb_roa_categories)(void *dev);
	int (*get_nb_tpe_categories)(void *dev);
	int (*get_nb_tx_cpy_writers)(void *dev);
	int (*get_nb_tx_cpy_mask_mem)(void *dev);
	int (*get_nb_tx_rpl_depth)(void *dev);
	int (*get_nb_tx_rpl_ext_categories)(void *dev);
	int (*get_nb_tpe_ifr_categories)(void *dev);
	int (*get_nb_rpp_per_ps)(void *dev);
	int (*get_nb_hsh_categories)(void *dev);
	int (*get_nb_hsh_toeplitz)(void *dev);

	int (*alloc_rx_queue)(void *dev, int queue_id);
	int (*free_rx_queue)(void *dev, int hw_queue);
};

struct flow_api_backend_s {
	void *be_dev;
};

#endif  /* _HW_MOD_BACKEND_H_ */
