/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include <stdint.h>

#include "flow_nthw_info.h"
#include "ntnic_mod_reg.h"
#include "nthw_fpga_model.h"
#include "hw_mod_backend.h"

/*
 * Binary Flow API backend implementation into ntservice driver
 *
 * General note on this backend implementation:
 * Maybe use shadow class to combine multiple writes. However, this backend is only for dev/testing
 */

static struct backend_dev_s {
	uint8_t adapter_no;
	enum debug_mode_e dmode;
	struct info_nthw *p_info_nthw;
} be_devs[MAX_PHYS_ADAPTERS];

const struct flow_api_backend_ops *bin_flow_backend_init(nthw_fpga_t *p_fpga, void **be_dev);
static void bin_flow_backend_done(void *be_dev);

static int set_debug_mode(void *be_dev, enum debug_mode_e mode)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	be->dmode = mode;
	return 0;
}

/*
 * INFO
 */

static int get_nb_phy_ports(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_nb_phy_ports(be->p_info_nthw);
}

static int get_nb_rx_ports(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_nb_rx_ports(be->p_info_nthw);
}

static int get_ltx_avail(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_ltx_avail(be->p_info_nthw);
}

static int get_nb_cat_funcs(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_nb_cat_funcs(be->p_info_nthw);
}

static int get_nb_categories(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_nb_categories(be->p_info_nthw);
}

static int get_nb_cat_km_if_cnt(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_nb_cat_km_if_cnt(be->p_info_nthw);
}

static int get_nb_cat_km_if_m0(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_nb_cat_km_if_m0(be->p_info_nthw);
}

static int get_nb_cat_km_if_m1(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_nb_cat_km_if_m1(be->p_info_nthw);
}

static int get_nb_queues(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_nb_queues(be->p_info_nthw);
}

static int get_nb_km_flow_types(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_nb_km_flow_types(be->p_info_nthw);
}

static int get_nb_pm_ext(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_nb_pm_ext(be->p_info_nthw);
}

static int get_nb_len(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_nb_len(be->p_info_nthw);
}

static int get_kcc_size(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_kcc_size(be->p_info_nthw);
}

static int get_kcc_banks(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_kcc_banks(be->p_info_nthw);
}

static int get_nb_km_categories(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_nb_km_categories(be->p_info_nthw);
}

static int get_nb_km_cam_banks(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_nb_km_cam_banks(be->p_info_nthw);
}

static int get_nb_km_cam_record_words(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_nb_km_cam_record_words(be->p_info_nthw);
}

static int get_nb_km_cam_records(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_nb_km_cam_records(be->p_info_nthw);
}

static int get_nb_km_tcam_banks(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_nb_km_tcam_banks(be->p_info_nthw);
}

static int get_nb_km_tcam_bank_width(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_nb_km_tcam_bank_width(be->p_info_nthw);
}

static int get_nb_flm_categories(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_nb_flm_categories(be->p_info_nthw);
}

static int get_nb_flm_size_mb(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_nb_flm_size_mb(be->p_info_nthw);
}

static int get_nb_flm_entry_size(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_nb_flm_entry_size(be->p_info_nthw);
}

static int get_nb_flm_variant(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_nb_flm_variant(be->p_info_nthw);
}

static int get_nb_flm_prios(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_nb_flm_prios(be->p_info_nthw);
}

static int get_nb_flm_pst_profiles(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_nb_flm_pst_profiles(be->p_info_nthw);
}

static int get_nb_flm_scrub_profiles(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_nb_flm_scrub_profiles(be->p_info_nthw);
}

static int get_nb_flm_load_aps_max(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_nb_flm_load_aps_max(be->p_info_nthw);
}

static int get_nb_qsl_categories(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_nb_qsl_categories(be->p_info_nthw);
}

static int get_nb_qsl_qst_entries(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_nb_qsl_qst_entries(be->p_info_nthw);
}

static int get_nb_pdb_categories(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_nb_pdb_categories(be->p_info_nthw);
}

static int get_nb_roa_categories(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_nb_roa_categories(be->p_info_nthw);
}

static int get_nb_tpe_categories(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_nb_tpe_categories(be->p_info_nthw);
}

static int get_nb_tx_cpy_writers(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_nb_tx_cpy_writers(be->p_info_nthw);
}

static int get_nb_tx_cpy_mask_mem(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_nb_tx_cpy_mask_mem(be->p_info_nthw);
}

static int get_nb_tx_rpl_depth(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_nb_tx_rpl_depth(be->p_info_nthw);
}

static int get_nb_tx_rpl_ext_categories(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_nb_tx_rpl_ext_categories(be->p_info_nthw);
}

static int get_nb_tpe_ifr_categories(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_nb_tpe_ifr_categories(be->p_info_nthw);
}

static int get_nb_rpp_per_ps(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_nb_rpp_per_ps(be->p_info_nthw);
}

static int get_nb_hsh_categories(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_nb_hsh_categories(be->p_info_nthw);
}

static int get_nb_hsh_toeplitz(void *be_dev)
{
	struct backend_dev_s *be = (struct backend_dev_s *)be_dev;
	return info_nthw_get_nb_hsh_toeplitz(be->p_info_nthw);
}

/*
 * DBS
 */

static int alloc_rx_queue(void *be_dev, int queue_id)
{
	(void)be_dev;
	(void)queue_id;
	NT_LOG(ERR, FILTER, "ERROR alloc Rx queue");
	return -1;
}

static int free_rx_queue(void *be_dev, int hw_queue)
{
	(void)be_dev;
	(void)hw_queue;
	NT_LOG(ERR, FILTER, "ERROR free Rx queue");
	return 0;
}

const struct flow_api_backend_ops flow_be_iface = {
	1,

	set_debug_mode,
	get_nb_phy_ports,
	get_nb_rx_ports,
	get_ltx_avail,
	get_nb_cat_funcs,
	get_nb_categories,
	get_nb_cat_km_if_cnt,
	get_nb_cat_km_if_m0,
	get_nb_cat_km_if_m1,
	get_nb_queues,
	get_nb_km_flow_types,
	get_nb_pm_ext,
	get_nb_len,
	get_kcc_size,
	get_kcc_banks,
	get_nb_km_categories,
	get_nb_km_cam_banks,
	get_nb_km_cam_record_words,
	get_nb_km_cam_records,
	get_nb_km_tcam_banks,
	get_nb_km_tcam_bank_width,
	get_nb_flm_categories,
	get_nb_flm_size_mb,
	get_nb_flm_entry_size,
	get_nb_flm_variant,
	get_nb_flm_prios,
	get_nb_flm_pst_profiles,
	get_nb_flm_scrub_profiles,
	get_nb_flm_load_aps_max,
	get_nb_qsl_categories,
	get_nb_qsl_qst_entries,
	get_nb_pdb_categories,
	get_nb_roa_categories,
	get_nb_tpe_categories,
	get_nb_tx_cpy_writers,
	get_nb_tx_cpy_mask_mem,
	get_nb_tx_rpl_depth,
	get_nb_tx_rpl_ext_categories,
	get_nb_tpe_ifr_categories,
	get_nb_rpp_per_ps,
	get_nb_hsh_categories,
	get_nb_hsh_toeplitz,

	alloc_rx_queue,
	free_rx_queue,
};

const struct flow_api_backend_ops *bin_flow_backend_init(nthw_fpga_t *p_fpga, void **dev)
{
	uint8_t physical_adapter_no = (uint8_t)p_fpga->p_fpga_info->adapter_no;

	struct info_nthw *pinfonthw = info_nthw_new();
	info_nthw_init(pinfonthw, p_fpga, physical_adapter_no);
	be_devs[physical_adapter_no].p_info_nthw = pinfonthw;

	be_devs[physical_adapter_no].adapter_no = physical_adapter_no;
	*dev = (void *)&be_devs[physical_adapter_no];

	return &flow_be_iface;
}

static void bin_flow_backend_done(void *dev)
{
	struct backend_dev_s *be_dev = (struct backend_dev_s *)dev;
	info_nthw_delete(be_dev->p_info_nthw);
}

static const struct flow_backend_ops ops = {
	.bin_flow_backend_init = bin_flow_backend_init,
	.bin_flow_backend_done = bin_flow_backend_done,
};

void flow_backend_init(void)
{
	register_flow_backend_ops(&ops);
}
