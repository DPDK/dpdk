/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include "nt_service.h"
#include "ntlog.h"
#include "nthw_fpga.h"
#include "ntnic_mod_reg.h"

/*
 * Global variables shared by NT adapter types
 */

static int nt4ga_adapter_show_info(struct adapter_info_s *p_adapter_info, FILE *pfh)
{
	const char *const p_dev_name = p_adapter_info->p_dev_name;
	const char *const p_adapter_id_str = p_adapter_info->mp_adapter_id_str;
	fpga_info_t *p_fpga_info = &p_adapter_info->fpga_info;
	hw_info_t *p_hw_info = &p_adapter_info->hw_info;
	mcu_info_t *mcu_info = &p_adapter_info->fpga_info.mcu_info;
	char a_pci_ident_str[32];

	snprintf(a_pci_ident_str, sizeof(a_pci_ident_str), PCIIDENT_PRINT_STR,
		PCIIDENT_TO_DOMAIN(p_fpga_info->pciident),
		PCIIDENT_TO_BUSNR(p_fpga_info->pciident),
		PCIIDENT_TO_DEVNR(p_fpga_info->pciident),
		PCIIDENT_TO_FUNCNR(p_fpga_info->pciident));

	fprintf(pfh, "%s: DeviceName: %s\n", p_adapter_id_str, (p_dev_name ? p_dev_name : "NA"));
	fprintf(pfh, "%s: PCI Details:\n", p_adapter_id_str);
	fprintf(pfh, "%s: %s: %08X: %04X:%04X %04X:%04X\n", p_adapter_id_str, a_pci_ident_str,
		p_fpga_info->pciident, p_hw_info->pci_vendor_id, p_hw_info->pci_device_id,
		p_hw_info->pci_sub_vendor_id, p_hw_info->pci_sub_device_id);
	fprintf(pfh, "%s: FPGA Details:\n", p_adapter_id_str);
	fprintf(pfh, "%s: %03d-%04d-%02d-%02d [%016" PRIX64 "] (%08X)\n", p_adapter_id_str,
		p_fpga_info->n_fpga_type_id, p_fpga_info->n_fpga_prod_id,
		p_fpga_info->n_fpga_ver_id, p_fpga_info->n_fpga_rev_id, p_fpga_info->n_fpga_ident,
		p_fpga_info->n_fpga_build_time);
	fprintf(pfh, "%s: FpgaDebugMode=0x%x\n", p_adapter_id_str, p_fpga_info->n_fpga_debug_mode);
	fprintf(pfh, "%s: Nims=%d PhyPorts=%d PhyQuads=%d RxPorts=%d TxPorts=%d\n",
		p_adapter_id_str, p_fpga_info->n_nims, p_fpga_info->n_phy_ports,
		p_fpga_info->n_phy_quads, p_fpga_info->n_rx_ports, p_fpga_info->n_tx_ports);
	fprintf(pfh, "%s: Hw=0x%02X_rev%d: %s\n", p_adapter_id_str, p_hw_info->hw_platform_id,
		p_fpga_info->nthw_hw_info.hw_id, p_fpga_info->nthw_hw_info.hw_plat_id_str);
	fprintf(pfh, "%s: MCU Details:\n", p_adapter_id_str);
	fprintf(pfh, "%s: HasMcu=%d McuType=%d McuDramSize=%d\n", p_adapter_id_str,
		mcu_info->mb_has_mcu, mcu_info->mn_mcu_type, mcu_info->mn_mcu_dram_size);

	return 0;
}

static int nt4ga_adapter_init(struct adapter_info_s *p_adapter_info)
{
	const struct flow_filter_ops *flow_filter_ops = get_flow_filter_ops();

	if (flow_filter_ops == NULL)
		NT_LOG(ERR, NTNIC, "%s: flow_filter module uninitialized", __func__);

	char *const p_dev_name = malloc(24);
	char *const p_adapter_id_str = malloc(24);
	fpga_info_t *fpga_info = &p_adapter_info->fpga_info;
	hw_info_t *p_hw_info = &p_adapter_info->hw_info;

	RTE_ASSERT(fpga_info);

	/*
	 * IMPORTANT: Most variables cannot be determined before nthw fpga model is instantiated
	 * (nthw_fpga_init())
	 */
	int res = -1;
	nthw_fpga_t *p_fpga = NULL;

	p_hw_info->n_nthw_adapter_id = nthw_platform_get_nthw_adapter_id(p_hw_info->pci_device_id);

	fpga_info->n_nthw_adapter_id = p_hw_info->n_nthw_adapter_id;
	/* ref: DN-0060 section 9 */
	p_hw_info->hw_product_type = p_hw_info->pci_device_id & 0x000f;
	/* ref: DN-0060 section 9 */
	p_hw_info->hw_platform_id = (p_hw_info->pci_device_id >> 4) & 0x00ff;
	/* ref: DN-0060 section 9 */
	p_hw_info->hw_reserved1 = (p_hw_info->pci_device_id >> 12) & 0x000f;

	p_adapter_info->p_dev_name = p_dev_name;

	if (p_dev_name) {
		snprintf(p_dev_name, 24, PCIIDENT_PRINT_STR,
			PCIIDENT_TO_DOMAIN(p_adapter_info->fpga_info.pciident),
			PCIIDENT_TO_BUSNR(p_adapter_info->fpga_info.pciident),
			PCIIDENT_TO_DEVNR(p_adapter_info->fpga_info.pciident),
			PCIIDENT_TO_FUNCNR(p_adapter_info->fpga_info.pciident));
		NT_LOG(DBG, NTNIC, "%s: (0x%08X)", p_dev_name,
			p_adapter_info->fpga_info.pciident);
	}

	p_adapter_info->mp_adapter_id_str = p_adapter_id_str;

	p_adapter_info->fpga_info.mp_adapter_id_str = p_adapter_id_str;

	if (p_adapter_id_str) {
		snprintf(p_adapter_id_str, 24, "PCI:" PCIIDENT_PRINT_STR,
			PCIIDENT_TO_DOMAIN(p_adapter_info->fpga_info.pciident),
			PCIIDENT_TO_BUSNR(p_adapter_info->fpga_info.pciident),
			PCIIDENT_TO_DEVNR(p_adapter_info->fpga_info.pciident),
			PCIIDENT_TO_FUNCNR(p_adapter_info->fpga_info.pciident));
		NT_LOG(DBG, NTNIC, "%s: %s", p_adapter_id_str, p_dev_name ? p_dev_name : "NA");
	}

	{
		int i;

		for (i = 0; i < (int)ARRAY_SIZE(p_adapter_info->mp_port_id_str); i++) {
			char *p = malloc(32);

			if (p) {
				snprintf(p, 32, "%s:intf_%d",
					(p_adapter_id_str ? p_adapter_id_str : "NA"), i);
			}

			p_adapter_info->mp_port_id_str[i] = p;
		}
	}

	res = nthw_fpga_init(&p_adapter_info->fpga_info);

	if (res) {
		NT_LOG_DBGX(ERR, NTNIC, "%s: %s: FPGA=%04d res=x%08X",
			p_adapter_id_str ? p_adapter_id_str : "NA",
			p_dev_name ? p_dev_name : "NA", fpga_info->n_fpga_prod_id, res);
		return res;
	}

	RTE_ASSERT(fpga_info->n_phy_ports >= 1);
	RTE_ASSERT(fpga_info->n_nims >= 1);

	p_fpga = fpga_info->mp_fpga;
	RTE_ASSERT(p_fpga);

	/* Nt4ga Init Filter */
	nt4ga_filter_t *p_filter = &p_adapter_info->nt4ga_filter;

	if (flow_filter_ops != NULL) {
		res = flow_filter_ops->nthw_flow_filter_init(p_fpga, &p_filter->mp_flow_device,
				p_adapter_info->adapter_no);

		if (res != 0) {
			NT_LOG(ERR, NTNIC, "%s: Cannot initialize filter", p_adapter_id_str);
			return res;
		}
	}

	{
		int i;
		const struct link_ops_s *link_ops = NULL;
		RTE_ASSERT(fpga_info->n_fpga_prod_id > 0);

		for (i = 0; i < NUM_ADAPTER_PORTS_MAX; i++) {
			/* Disable all ports. Must be enabled later */
			p_adapter_info->nt4ga_link.port_action[i].port_disable = true;
		}

		switch (fpga_info->n_fpga_prod_id) {
		/* NT200A01: 2x100G (Xilinx) */
		case 9563:	/* NT200A02 (Cap) */
			link_ops = get_100g_link_ops();

			if (link_ops == NULL) {
				NT_LOG(ERR, NTNIC, "NT200A02 100G link module uninitialized");
				res = -1;
				break;
			}

			res = link_ops->link_init(p_adapter_info, p_fpga);
			break;
		case 9574: /* NT400D13 (Intel Agilex FPGA) */
			link_ops = get_agx_100g_link_ops();
			if (link_ops == NULL) {
				NT_LOG(ERR, NTNIC, "NT400D11 100G link module uninitialized");
				res = -1;
				break;
			}
			res = link_ops->link_init(p_adapter_info, p_fpga);
			break;

		default:
			NT_LOG(ERR, NTNIC, "Unsupported FPGA product: %04d",
				fpga_info->n_fpga_prod_id);
			res = -1;
			break;
		}

		if (res) {
			NT_LOG_DBGX(ERR, NTNIC, "%s: %s: FPGA=%04d res=x%08X",
				p_adapter_id_str, p_dev_name,
				fpga_info->n_fpga_prod_id, res);
			return res;
		}
	}

	const struct nt4ga_stat_ops *nt4ga_stat_ops = get_nt4ga_stat_ops();

	if (nt4ga_stat_ops != NULL) {
		/* Nt4ga Stat init/setup */
		res = nt4ga_stat_ops->nt4ga_stat_init(p_adapter_info);

		if (res != 0) {
			NT_LOG(ERR, NTNIC, "%s: Cannot initialize the statistics module",
				p_adapter_id_str);
			return res;
		}

		res = nt4ga_stat_ops->nt4ga_stat_setup(p_adapter_info);

		if (res != 0) {
			NT_LOG(ERR, NTNIC, "%s: Cannot setup the statistics module",
				p_adapter_id_str);
			return res;
		}
	}

	return 0;
}

static int nt4ga_adapter_deinit(struct adapter_info_s *p_adapter_info)
{
	const struct flow_filter_ops *flow_filter_ops = get_flow_filter_ops();

	if (flow_filter_ops == NULL)
		NT_LOG(ERR, NTNIC, "%s: flow_filter module uninitialized", __func__);

	fpga_info_t *fpga_info = &p_adapter_info->fpga_info;
	int i;
	int res = -1;

	nthw_service_del(RTE_NTNIC_SERVICE_ADAPTER_MON);

	/* Nt4ga Deinit Filter */
	nt4ga_filter_t *p_filter = &p_adapter_info->nt4ga_filter;

	if (flow_filter_ops != NULL) {
		res = flow_filter_ops->nthw_flow_filter_done(p_filter->mp_flow_device);

		if (res != 0) {
			NT_LOG(ERR, NTNIC, "Cannot deinitialize filter");
			return res;
		}
	}

	nthw_fpga_shutdown(&p_adapter_info->fpga_info);

	/* Rac rab reset flip flop */
	res = nthw_rac_rab_reset(fpga_info->mp_nthw_rac);

	/* Free adapter port ident strings */
	for (i = 0; i < fpga_info->n_phy_ports; i++) {
		if (p_adapter_info->mp_port_id_str[i]) {
			free(p_adapter_info->mp_port_id_str[i]);
			p_adapter_info->mp_port_id_str[i] = NULL;
		}
	}

	/* Free adapter ident string */
	if (p_adapter_info->mp_adapter_id_str) {
		free(p_adapter_info->mp_adapter_id_str);
		p_adapter_info->mp_adapter_id_str = NULL;
	}

	/* Free devname ident string */
	if (p_adapter_info->p_dev_name) {
		free(p_adapter_info->p_dev_name);
		p_adapter_info->p_dev_name = NULL;
	}

	return res;
}

static const struct adapter_ops ops = {
	.init = nt4ga_adapter_init,
	.deinit = nt4ga_adapter_deinit,

	.show_info = nt4ga_adapter_show_info,
};

void nthw_adapter_init(void)
{
	register_adapter_ops(&ops);
}
