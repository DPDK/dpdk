/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include "ntlog.h"

#include "nthw_drv.h"
#include "nthw_register.h"

#include "nthw_fpga.h"

#include "nthw_fpga_instances.h"
#include "nthw_fpga_mod_str_map.h"

#include "nthw_tsm.h"
#include "nthw_spi_v3.h"

#include <arpa/inet.h>

static int nthw_fpga_get_param_info(struct fpga_info_s *p_fpga_info, nthw_fpga_t *p_fpga)
{
	mcu_info_t *p_mcu_info = &p_fpga_info->mcu_info;

	const int n_nims = nthw_fpga_get_product_param(p_fpga, NT_NIMS, -1);
	const int n_phy_ports = nthw_fpga_get_product_param(p_fpga, NT_PHY_PORTS, -1);
	const int n_phy_quads = nthw_fpga_get_product_param(p_fpga, NT_PHY_QUADS, -1);
	const int n_rx_ports = nthw_fpga_get_product_param(p_fpga, NT_RX_PORTS, -1);
	const int n_tx_ports = nthw_fpga_get_product_param(p_fpga, NT_TX_PORTS, -1);
	const int n_vf_offset = nthw_fpga_get_product_param(p_fpga, NT_HIF_VF_OFFSET, 4);

	p_fpga_info->n_nims = n_nims;
	p_fpga_info->n_phy_ports = n_phy_ports;
	p_fpga_info->n_phy_quads = n_phy_quads;
	p_fpga_info->n_rx_ports = n_rx_ports;
	p_fpga_info->n_tx_ports = n_tx_ports;
	p_fpga_info->n_vf_offset = n_vf_offset;
	p_fpga_info->profile = FPGA_INFO_PROFILE_UNKNOWN;

	/* Check for MCU */
	if (nthw_fpga_get_product_param(p_fpga, NT_MCU_PRESENT, 0) != 0) {
		p_mcu_info->mb_has_mcu = true;
		/* Check MCU Type */
		p_mcu_info->mn_mcu_type = nthw_fpga_get_product_param(p_fpga, NT_MCU_TYPE, -1);
		/* MCU DRAM size */
		p_mcu_info->mn_mcu_dram_size =
			nthw_fpga_get_product_param(p_fpga, NT_MCU_DRAM_SIZE, -1);

	} else {
		p_mcu_info->mb_has_mcu = false;
		p_mcu_info->mn_mcu_type = -1;
		p_mcu_info->mn_mcu_dram_size = -1;
	}

	/* Check for VSWITCH FPGA */
	if (nthw_fpga_get_product_param(p_fpga, NT_NFV_OVS_PRODUCT, 0) != 0) {
		p_fpga_info->profile = FPGA_INFO_PROFILE_VSWITCH;

	} else if (nthw_fpga_get_product_param(p_fpga, NT_IOA_PRESENT, 0) != 0) {
		/* Check for VSWITCH FPGA - legacy */
		p_fpga_info->profile = FPGA_INFO_PROFILE_VSWITCH;

	} else if (nthw_fpga_get_product_param(p_fpga, NT_QM_PRESENT, 0) != 0) {
		p_fpga_info->profile = FPGA_INFO_PROFILE_CAPTURE;

	} else {
		p_fpga_info->profile = FPGA_INFO_PROFILE_INLINE;
	}

	return 0;
}

int nthw_fpga_iic_scan(nthw_fpga_t *p_fpga, const int n_instance_no_begin,
	const int n_instance_no_end)
{
	int i;

	RTE_ASSERT(n_instance_no_begin <= n_instance_no_end);

	for (i = n_instance_no_begin; i <= n_instance_no_end; i++) {
		nthw_iic_t *p_nthw_iic = nthw_iic_new();

		if (p_nthw_iic) {
			const int rc = nthw_iic_init(p_nthw_iic, p_fpga, i, 8);

			if (rc == 0) {
				nthw_iic_set_retry_params(p_nthw_iic, -1, 100, 100, 3, 3);
				nthw_iic_scan(p_nthw_iic);
			}

			nthw_iic_delete(p_nthw_iic);
			p_nthw_iic = NULL;
		}
	}

	return 0;
}

int nthw_fpga_silabs_detect(nthw_fpga_t *p_fpga, const int n_instance_no, const int n_dev_addr,
	const int n_page_reg_addr)
{
	const char *const p_adapter_id_str = p_fpga->p_fpga_info->mp_adapter_id_str;
	(void)p_adapter_id_str;
	uint64_t ident = -1;
	int res = -1;

	nthw_iic_t *p_nthw_iic = nthw_iic_new();

	if (p_nthw_iic) {
		uint8_t data;
		uint8_t a_silabs_ident[8];
		nthw_iic_init(p_nthw_iic, p_fpga, n_instance_no, 8);

		data = 0;
		/* switch to page 0 */
		nthw_iic_write_data(p_nthw_iic, (uint8_t)n_dev_addr, (uint8_t)n_page_reg_addr, 1,
			&data);
		res = nthw_iic_read_data(p_nthw_iic, (uint8_t)n_dev_addr, 0x00,
				sizeof(a_silabs_ident), a_silabs_ident);

		if (res == 0) {
			int i;

			for (i = 0; i < (int)sizeof(a_silabs_ident); i++) {
				ident <<= 8;
				ident |= a_silabs_ident[i];
			}
		}

		nthw_iic_delete(p_nthw_iic);
		p_nthw_iic = NULL;

		/* Conclude SiLabs part */
		if (res == 0) {
			if (a_silabs_ident[3] == 0x53) {
				if (a_silabs_ident[2] == 0x40)
					res = 5340;

				else if (a_silabs_ident[2] == 0x41)
					res = 5341;

			} else if (a_silabs_ident[2] == 38) {
				res = 5338;

			} else {
				res = -1;
			}
		}
	}

	NT_LOG(DBG, NTHW, "%s: %016" PRIX64 ": %d", p_adapter_id_str, ident, res);
	return res;
}

int nthw_fpga_avr_probe(nthw_fpga_t *p_fpga, const int n_instance_no)
{
	struct fpga_info_s *p_fpga_info = p_fpga->p_fpga_info;
	const char *const p_adapter_id_str = p_fpga_info->mp_adapter_id_str;
	nthw_spi_v3_t *p_avr_spi;
	int res = -1;

	p_avr_spi = nthw_spi_v3_new();

	if (p_avr_spi) {
		struct avr_vpd_info_s {
			/* avr info */
			uint32_t n_avr_spi_version;
			uint8_t n_avr_fw_ver_major;
			uint8_t n_avr_fw_ver_minor;
			uint8_t n_avr_fw_ver_micro;
			uint8_t a_avr_fw_ver_str[50];
			uint8_t a_avr_fw_plat_id_str[20];

			/* vpd_eeprom_t */
			uint8_t psu_hw_version;
			uint8_t vpd_pn[GEN2_PN_SIZE];
			uint8_t vpd_pba[GEN2_PBA_SIZE];
			uint8_t vpd_sn[GEN2_SN_SIZE];
			uint8_t vpd_board_name[GEN2_BNAME_SIZE];
			uint8_t vpd_platform_section[GEN2_PLATFORM_SIZE];

			/* board_info_t aka vpd_platform_section: */
			uint32_t product_family;/* uint8_t 1: capture, 2: Inline, 3: analysis */
			uint32_t feature_mask;	/* Bit 0: OC192 capable */
			uint32_t invfeature_mask;
			uint8_t no_of_macs;
			uint8_t mac_address[6];
			uint16_t custom_id;
			uint8_t user_id[8];
			/*
			 * Reserved NT operations to monitor the reprogram count of user_id with
			 * vpduser
			 */
			uint16_t user_id_erase_write_count;

			/*
			 * AVR_OP_SYSINFO: struct version_sysinfo_request_container
			 * Which version of the sysinfo container to retrieve. Set to zero to fetch
			 * latest. Offset zero of latest always contain an uint8_t version info
			 */
			uint8_t sysinfo_container_version;

			/* AVR_OP_SYSINFO: struct AvrLibcVersion */
			/* The constant __AVR_LIBC_VERSION__ */
			uint32_t sysinfo_avr_libc_version;

			/* AVR_OP_SYSINFO: struct AvrLibcSignature */
			uint8_t sysinfo_signature_0;	/* The constant SIGNATURE_0 */
			uint8_t sysinfo_signature_1;	/* The constant SIGNATURE_1 */
			uint8_t sysinfo_signature_2;	/* The constant SIGNATURE_2 */

			/* AVR_OP_SYSINFO: struct AvrOs */
			uint8_t sysinfo_spi_version;	/* SPI command layer version */
			/*
			 * Hardware revision. Locked to eeprom address zero. Is also available via
			 * VPD read opcode (prior to v1.4b, this is required)
			 */
			uint8_t sysinfo_hw_revision;
			/*
			 * Number of ticks/second (Note: Be aware this may become zero if timer
			 * module is rewritten to a tickles system!)
			 */
			uint8_t sysinfo_ticks_per_second;
			uint32_t sysinfo_uptime;/* Uptime in seconds since last AVR reset */
			uint8_t sysinfo_osccal;	/* OSCCAL value */

			/*
			 * Meta data concluded/calculated from req/reply
			 */
			bool b_feature_mask_valid;
			bool b_crc16_valid;
			uint16_t n_crc16_stored;
			uint16_t n_crc16_calced;
			uint64_t n_mac_val;
		};

		struct avr_vpd_info_s avr_vpd_info;
		struct tx_rx_buf tx_buf;
		struct tx_rx_buf rx_buf;
		char rx_data[MAX_AVR_CONTAINER_SIZE];
		uint32_t u32;

		memset(&avr_vpd_info, 0, sizeof(avr_vpd_info));

		nthw_spi_v3_init(p_avr_spi, p_fpga, n_instance_no);

		/* AVR_OP_SPI_VERSION */
		tx_buf.size = 0;
		tx_buf.p_buf = NULL;
		rx_buf.size = sizeof(u32);
		rx_buf.p_buf = &u32;
		u32 = 0;
		res = nthw_spi_v3_transfer(p_avr_spi, AVR_OP_SPI_VERSION, &tx_buf, &rx_buf);
		avr_vpd_info.n_avr_spi_version = u32;
		NT_LOG(DBG, NTHW, "%s: AVR%d: SPI_VER: %d", p_adapter_id_str, n_instance_no,
			avr_vpd_info.n_avr_spi_version);

		/* AVR_OP_VERSION */
		tx_buf.size = 0;
		tx_buf.p_buf = NULL;
		rx_buf.size = sizeof(rx_data);
		rx_buf.p_buf = &rx_data;
		res = nthw_spi_v3_transfer(p_avr_spi, AVR_OP_VERSION, &tx_buf, &rx_buf);

		avr_vpd_info.n_avr_fw_ver_major = rx_data[0];
		avr_vpd_info.n_avr_fw_ver_minor = rx_data[1];
		avr_vpd_info.n_avr_fw_ver_micro = rx_data[2];
		NT_LOG(DBG, NTHW, "%s: AVR%d: FW_VER: %c.%c.%c", p_adapter_id_str, n_instance_no,
			avr_vpd_info.n_avr_fw_ver_major, avr_vpd_info.n_avr_fw_ver_minor,
			avr_vpd_info.n_avr_fw_ver_micro);

		memcpy(avr_vpd_info.a_avr_fw_ver_str, &rx_data[0 + 3],
			sizeof(avr_vpd_info.a_avr_fw_ver_str));
		NT_LOG(DBG, NTHW, "%s: AVR%d: FW_VER_STR: '%.*s'", p_adapter_id_str,
			n_instance_no, (int)sizeof(avr_vpd_info.a_avr_fw_ver_str),
			avr_vpd_info.a_avr_fw_ver_str);

		memcpy(avr_vpd_info.a_avr_fw_plat_id_str, &rx_data[0 + 3 + 50],
			sizeof(avr_vpd_info.a_avr_fw_plat_id_str));
		NT_LOG(DBG, NTHW, "%s: AVR%d: FW_HW_ID_STR: '%.*s'", p_adapter_id_str,
			n_instance_no, (int)sizeof(avr_vpd_info.a_avr_fw_plat_id_str),
			avr_vpd_info.a_avr_fw_plat_id_str);

		snprintf(p_fpga_info->nthw_hw_info.hw_plat_id_str,
			sizeof(p_fpga_info->nthw_hw_info.hw_plat_id_str), "%s",
			(char *)avr_vpd_info.a_avr_fw_plat_id_str);
		p_fpga_info->nthw_hw_info
		.hw_plat_id_str[sizeof(p_fpga_info->nthw_hw_info.hw_plat_id_str) - 1] = 0;

		/* AVR_OP_SYSINFO_2 */
		tx_buf.size = 0;
		tx_buf.p_buf = NULL;
		rx_buf.size = sizeof(rx_data);
		rx_buf.p_buf = &rx_data;
		res = nthw_spi_v3_transfer(p_avr_spi, AVR_OP_SYSINFO_2, &tx_buf, &rx_buf);

		/* AVR_OP_SYSINFO */
		tx_buf.size = 0;
		tx_buf.p_buf = NULL;
		rx_buf.size = sizeof(rx_data);
		rx_buf.p_buf = &rx_data;
		res = nthw_spi_v3_transfer(p_avr_spi, AVR_OP_SYSINFO, &tx_buf, &rx_buf);

		NT_LOG(ERR, NTHW, "%s: AVR%d: SYSINFO: NA: res=%d sz=%d",
				p_adapter_id_str, n_instance_no, res, rx_buf.size);

		/* AVR_OP_VPD_READ */
		tx_buf.size = 0;
		tx_buf.p_buf = NULL;
		rx_buf.size = sizeof(rx_data);
		rx_buf.p_buf = &rx_data;
		res = nthw_spi_v3_transfer(p_avr_spi, AVR_OP_VPD_READ, &tx_buf, &rx_buf);

		NT_LOG(ERR, NTHW, "%s:%u: res=%d", __func__, __LINE__, res);
		NT_LOG(ERR, NTHW, "%s: AVR%d: SYSINFO2: NA: res=%d sz=%d",
			p_adapter_id_str, n_instance_no, res, rx_buf.size);
	}
	return res;
}

/*
 * NT200A02, NT200A01-HWbuild2
 */
int nthw_fpga_si5340_clock_synth_init_fmt2(nthw_fpga_t *p_fpga, const uint8_t n_iic_addr,
	const clk_profile_data_fmt2_t *p_clk_profile,
	const int n_clk_profile_rec_cnt)
{
	int res;
	nthw_iic_t *p_nthw_iic = nthw_iic_new();
	nthw_si5340_t *p_nthw_si5340 = nthw_si5340_new();

	RTE_ASSERT(p_nthw_iic);
	RTE_ASSERT(p_nthw_si5340);
	nthw_iic_init(p_nthw_iic, p_fpga, 0, 8);/* I2C cycle time 125Mhz ~ 8ns */

	nthw_si5340_init(p_nthw_si5340, p_nthw_iic, n_iic_addr);/* si5340_u23_i2c_addr_7bit */
	res = nthw_si5340_config_fmt2(p_nthw_si5340, p_clk_profile, n_clk_profile_rec_cnt);
	nthw_si5340_delete(p_nthw_si5340);
	p_nthw_si5340 = NULL;

	return res;
}

int nthw_fpga_init(struct fpga_info_s *p_fpga_info)
{
	RTE_ASSERT(p_fpga_info);
	const char *const p_adapter_id_str = p_fpga_info->mp_adapter_id_str;

	nthw_hif_t *p_nthw_hif = NULL;
	nthw_pcie3_t *p_nthw_pcie3 = NULL;
	nthw_rac_t *p_nthw_rac = NULL;
	nthw_tsm_t *p_nthw_tsm = NULL;

	mcu_info_t *p_mcu_info = &p_fpga_info->mcu_info;
	uint64_t n_fpga_ident = 0;
	nthw_fpga_mgr_t *p_fpga_mgr = NULL;
	nthw_fpga_t *p_fpga = NULL;

	char s_fpga_prod_ver_rev_str[32] = { 0 };

	int res = 0;

	{
		const uint64_t n_fpga_ident = nthw_fpga_read_ident(p_fpga_info);
		const uint32_t n_fpga_build_time = nthw_fpga_read_buildtime(p_fpga_info);
		const int n_fpga_type_id = nthw_fpga_extract_type_id(n_fpga_ident);
		const int n_fpga_prod_id = nthw_fpga_extract_prod_id(n_fpga_ident);
		const int n_fpga_ver_id = nthw_fpga_extract_ver_id(n_fpga_ident);
		const int n_fpga_rev_id = nthw_fpga_extract_rev_id(n_fpga_ident);

		p_fpga_info->n_fpga_ident = n_fpga_ident;
		p_fpga_info->n_fpga_type_id = n_fpga_type_id;
		p_fpga_info->n_fpga_prod_id = n_fpga_prod_id;
		p_fpga_info->n_fpga_ver_id = n_fpga_ver_id;
		p_fpga_info->n_fpga_rev_id = n_fpga_rev_id;
		p_fpga_info->n_fpga_build_time = n_fpga_build_time;

		snprintf(s_fpga_prod_ver_rev_str, sizeof(s_fpga_prod_ver_rev_str),
			"%04d-%04d-%02d-%02d", n_fpga_type_id, n_fpga_prod_id, n_fpga_ver_id,
			n_fpga_rev_id);

		NT_LOG(INF, NTHW, "%s: FPGA %s (%" PRIX64 ") [%08X]", p_adapter_id_str,
			s_fpga_prod_ver_rev_str, n_fpga_ident, n_fpga_build_time);
	}

	n_fpga_ident = p_fpga_info->n_fpga_ident;

	p_fpga_mgr = nthw_fpga_mgr_new();
	if (p_fpga_mgr) {
		nthw_fpga_mgr_init(p_fpga_mgr, nthw_fpga_instances,
			(const void *)nthw_sa_fpga_mod_str_map);
		nthw_fpga_mgr_log_dump(p_fpga_mgr);
		p_fpga = nthw_fpga_mgr_query_fpga(p_fpga_mgr, n_fpga_ident, p_fpga_info);
		p_fpga_info->mp_fpga = p_fpga;

		if (p_fpga == NULL) {
			NT_LOG(ERR, NTHW, "%s: Unsupported FPGA: %s (%08X)", p_adapter_id_str,
				s_fpga_prod_ver_rev_str, p_fpga_info->n_fpga_build_time);
			nthw_fpga_mgr_delete(p_fpga_mgr);
			p_fpga_mgr = NULL;
			return -1;
		}

		nthw_fpga_mgr_delete(p_fpga_mgr);
		p_fpga_mgr = NULL;
	}

	/* Read Fpga param info */
	nthw_fpga_get_param_info(p_fpga_info, p_fpga);

	/* debug: report params */
	NT_LOG(DBG, NTHW, "%s: NT_NIMS=%d", p_adapter_id_str, p_fpga_info->n_nims);
	NT_LOG(DBG, NTHW, "%s: NT_PHY_PORTS=%d", p_adapter_id_str, p_fpga_info->n_phy_ports);
	NT_LOG(DBG, NTHW, "%s: NT_PHY_QUADS=%d", p_adapter_id_str, p_fpga_info->n_phy_quads);
	NT_LOG(DBG, NTHW, "%s: NT_RX_PORTS=%d", p_adapter_id_str, p_fpga_info->n_rx_ports);
	NT_LOG(DBG, NTHW, "%s: NT_TX_PORTS=%d", p_adapter_id_str, p_fpga_info->n_tx_ports);
	NT_LOG(DBG, NTHW, "%s: nProfile=%d", p_adapter_id_str, (int)p_fpga_info->profile);
	NT_LOG(DBG, NTHW, "%s: bHasMcu=%d", p_adapter_id_str, p_mcu_info->mb_has_mcu);
	NT_LOG(DBG, NTHW, "%s: McuType=%d", p_adapter_id_str, p_mcu_info->mn_mcu_type);
	NT_LOG(DBG, NTHW, "%s: McuDramSize=%d", p_adapter_id_str, p_mcu_info->mn_mcu_dram_size);

	p_nthw_rac = nthw_rac_new();

	if (p_nthw_rac == NULL) {
		NT_LOG(ERR, NTHW, "%s: Unsupported FPGA: RAC is not found: %s (%08X)",
			p_adapter_id_str, s_fpga_prod_ver_rev_str, p_fpga_info->n_fpga_build_time);
		return -1;
	}

	nthw_rac_init(p_nthw_rac, p_fpga, p_fpga_info);
	nthw_rac_rab_flush(p_nthw_rac);
	p_fpga_info->mp_nthw_rac = p_nthw_rac;

	struct nt200a0x_ops *nt200a0x_ops = nthw_get_nt200a0x_ops();
	struct nt400dxx_ops *nt400dxx_ops = nthw_get_nt400dxx_ops();

	switch (p_fpga_info->n_nthw_adapter_id) {
	case NT_HW_ADAPTER_ID_NT200A02:
		if (nt200a0x_ops != NULL)
			res = nt200a0x_ops->nthw_fpga_nt200a0x_init(p_fpga_info);
		break;

	case NT_HW_ADAPTER_ID_NT400D13:
		if (nt400dxx_ops != NULL)
			res = nt400dxx_ops->nthw_fpga_nt400dxx_init(p_fpga_info);
		break;

	default:
		NT_LOG(ERR, NTHW, "%s: Unsupported HW product id: %d", p_adapter_id_str,
			p_fpga_info->n_nthw_adapter_id);
		res = -1;
		break;
	}

	if (res) {
		NT_LOG(ERR, NTHW, "%s: status: 0x%08X", p_adapter_id_str, res);
		return res;
	}

	res = nthw_pcie3_init(NULL, p_fpga, 0);	/* Probe for module */

	if (res == 0) {
		p_nthw_pcie3 = nthw_pcie3_new();

		if (p_nthw_pcie3) {
			res = nthw_pcie3_init(p_nthw_pcie3, p_fpga, 0);

			if (res == 0) {
				NT_LOG(DBG, NTHW, "%s: Pcie3 module found", p_adapter_id_str);
				nthw_pcie3_trigger_sample_time(p_nthw_pcie3);

			} else {
				nthw_pcie3_delete(p_nthw_pcie3);
				p_nthw_pcie3 = NULL;
			}
		}

		p_fpga_info->mp_nthw_pcie3 = p_nthw_pcie3;
	}

	if (p_nthw_pcie3 == NULL) {
		p_nthw_hif = nthw_hif_new();

		if (p_nthw_hif) {
			res = nthw_hif_init(p_nthw_hif, p_fpga, 0);

			if (res == 0) {
				NT_LOG(DBG, NTHW, "%s: Hif module found", p_adapter_id_str);
				nthw_hif_trigger_sample_time(p_nthw_hif);

			} else {
				nthw_hif_delete(p_nthw_hif);
				p_nthw_hif = NULL;
			}
		}
	}

	p_fpga_info->mp_nthw_hif = p_nthw_hif;


	p_nthw_tsm = nthw_tsm_new();

	if (p_nthw_tsm) {
		nthw_tsm_init(p_nthw_tsm, p_fpga, 0);

		nthw_tsm_set_config_ts_format(p_nthw_tsm, 1);	/* 1 = TSM: TS format native */

		/* Timer T0 - stat toggle timer */
		nthw_tsm_set_timer_t0_enable(p_nthw_tsm, false);
		nthw_tsm_set_timer_t0_max_count(p_nthw_tsm, 50 * 1000 * 1000);	/* ns */
		nthw_tsm_set_timer_t0_enable(p_nthw_tsm, true);

		/* Timer T1 - keep alive timer */
		nthw_tsm_set_timer_t1_enable(p_nthw_tsm, false);
		nthw_tsm_set_timer_t1_max_count(p_nthw_tsm, 100 * 1000 * 1000);	/* ns */
		nthw_tsm_set_timer_t1_enable(p_nthw_tsm, true);
	}

	p_fpga_info->mp_nthw_tsm = p_nthw_tsm;

	/* TSM sample triggering: test validation... */
#if defined(DEBUG) && (1)
	{
		uint64_t n_time, n_ts;
		int i;

		for (i = 0; i < 4; i++) {
			if (p_nthw_hif)
				nthw_hif_trigger_sample_time(p_nthw_hif);

			else if (p_nthw_pcie3)
				nthw_pcie3_trigger_sample_time(p_nthw_pcie3);

			nthw_tsm_get_time(p_nthw_tsm, &n_time);
			nthw_tsm_get_ts(p_nthw_tsm, &n_ts);

			NT_LOG(DBG, NTHW, "%s: TSM time: %016" PRIX64 " %016" PRIX64 "\n",
				p_adapter_id_str, n_time, n_ts);

			nthw_os_wait_usec(1000);
		}
	}
#endif

	return res;
}

int nthw_fpga_shutdown(struct fpga_info_s *p_fpga_info)
{
	int res = -1;

		if (p_fpga_info->mp_nthw_rac)
			res = nthw_rac_rab_reset(p_fpga_info->mp_nthw_rac);

	return res;
}

static struct nt200a0x_ops *nt200a0x_ops;

void nthw_reg_nt200a0x_ops(struct nt200a0x_ops *ops)
{
	nt200a0x_ops = ops;
}

struct nt200a0x_ops *nthw_get_nt200a0x_ops(void)
{
	if (nt200a0x_ops == NULL)
		nthw_nt200a0x_ops_init();
	return nt200a0x_ops;
}

static struct nt400dxx_ops *nt400dxx_ops;

void nthw_reg_nt400dxx_ops(struct nt400dxx_ops *ops)
{
	nt400dxx_ops = ops;
}

struct nt400dxx_ops *nthw_get_nt400dxx_ops(void)
{
	if (nt400dxx_ops == NULL)
		nthw_nt400dxx_ops_init();
	return nt400dxx_ops;
}
