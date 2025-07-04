/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef __NTHW_DRV_H__
#define __NTHW_DRV_H__

#include "nthw_core.h"
#include "ntnic_dbs.h"

#include "nthw_si5332_si5156.h"
#include "nthw_pcal6416a.h"
#include "nthw_pca9532.h"
#include "nthw_phy_tile.h"
#include "nthw_rpf.h"
#include "nthw_prm_nt400dxx.h"
#include "nthw_pcm_nt400dxx.h"
#include "nthw_igam.h"

/*
 * Structs for controlling Agilex based NT400DXX adapter
 */
typedef struct nthw_agx_s {
	nthw_i2cm_t *p_i2cm;
	nthw_pca9849_t *p_pca9849;
	nthw_pcal6416a_t *p_io_ts;	/* PCAL6416A I/O expander for controlling TS */
	nthw_pcal6416a_t *p_io_nim;	/* PCAL6416A I/O expander for controlling TS */
	nthw_pca9532_t *p_pca9532_led;
	nthw_si5332_t *p_si5332;
	nthw_si5156_t *p_si5156;
	nthw_prm_nt400dxx_t *p_prm;
	nthw_pcm_nt400dxx_t *p_pcm;
	nthw_igam_t *p_igam;
	nthw_phy_tile_t *p_phy_tile;
	nthw_rpf_t *p_rpf;
	bool tcxo_present;
	bool tcxo_capable;
} nthw_agx_t;

typedef enum nt_meta_port_type_e {
	PORT_TYPE_PHYSICAL,
	PORT_TYPE_VIRTUAL,
	PORT_TYPE_OVERRIDE,
} nt_meta_port_type_t;

#include "nthw_helper.h"

enum fpga_info_profile {
	FPGA_INFO_PROFILE_UNKNOWN = 0,
	FPGA_INFO_PROFILE_VSWITCH = 1,
	FPGA_INFO_PROFILE_INLINE = 2,
	FPGA_INFO_PROFILE_CAPTURE = 3,
};

typedef struct mcu_info_s {
	bool mb_has_mcu;
	int mn_mcu_type;
	int mn_mcu_dram_size;
} mcu_info_t;

typedef struct nthw_hw_info_s {
	/* From FW */
	int hw_id;
	int hw_id_emulated;
	char hw_plat_id_str[32];

	struct vpd_info_s {
		int mn_mac_addr_count;
		uint64_t mn_mac_addr_value;
		uint8_t ma_mac_addr_octets[6];
	} vpd_info;
} nthw_hw_info_t;

typedef struct fpga_info_s {
	uint64_t n_fpga_ident;

	int n_fpga_type_id;
	int n_fpga_prod_id;
	int n_fpga_ver_id;
	int n_fpga_rev_id;

	int n_fpga_build_time;

	int n_fpga_debug_mode;

	int n_nims;
	int n_phy_ports;
	int n_phy_quads;
	int n_rx_ports;
	int n_tx_ports;
	int n_vf_offset;

	enum fpga_info_profile profile;

	struct nthw_fpga_s *mp_fpga;

	struct nthw_rac *mp_nthw_rac;
	struct nthw_hif *mp_nthw_hif;
	struct nthw_pcie3 *mp_nthw_pcie3;
	struct nthw_tsm *mp_nthw_tsm;

	nthw_dbs_t *mp_nthw_dbs;

	uint8_t *bar0_addr;	/* Needed for register read/write */
	size_t bar0_size;

	int adapter_no;	/* Needed for nthw_rac DMA array indexing */
	uint32_t pciident;	/* Needed for nthw_rac DMA memzone_reserve */
	int numa_node;	/* Needed for nthw_rac DMA memzone_reserve */

	char *mp_adapter_id_str;/* Pointer to string literal used in nthw log messages */

	struct mcu_info_s mcu_info;

	struct nthw_hw_info_s nthw_hw_info;

	nthw_adapter_id_t n_nthw_adapter_id;

	nthw_agx_t mp_nthw_agx;	/* For the Agilex based NT400DXX */
} fpga_info_t;


#endif	/* __NTHW_DRV_H__ */
