/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include "ntlog.h"
#include "nt_util.h"

#include "nthw_drv.h"
#include "nthw_register.h"

#include "nthw_gpio_phy.h"

int nthw_gpio_phy_init(nthw_gpio_phy_t *p, nthw_fpga_t *p_fpga, int n_instance)
{
	nthw_module_t *p_mod = nthw_fpga_query_module(p_fpga, MOD_GPIO_PHY, n_instance);

	if (p == NULL)
		return p_mod == NULL ? -1 : 0;

	if (p_mod == NULL) {
		NT_LOG(ERR, NTHW, "%s: GPIO_PHY %d: no such instance",
			p_fpga->p_fpga_info->mp_adapter_id_str, n_instance);
		return -1;
	}

	p->mp_fpga = p_fpga;
	p->mn_instance = n_instance;
	p->mp_mod_gpio_phy = p_mod;

	/* Registers */
	p->mp_reg_config = nthw_module_get_register(p->mp_mod_gpio_phy, GPIO_PHY_CFG);
	p->mp_reg_gpio = nthw_module_get_register(p->mp_mod_gpio_phy, GPIO_PHY_GPIO);

	/* PORT-0, config fields */
	p->mpa_fields[0].cfg_fld_lp_mode =
		nthw_register_get_field(p->mp_reg_config, GPIO_PHY_CFG_PORT0_LPMODE);
	p->mpa_fields[0].cfg_int =
		nthw_register_get_field(p->mp_reg_config, GPIO_PHY_CFG_PORT0_INT_B);
	p->mpa_fields[0].cfg_reset =
		nthw_register_get_field(p->mp_reg_config, GPIO_PHY_CFG_PORT0_RESET_B);
	p->mpa_fields[0].cfg_mod_prs =
		nthw_register_get_field(p->mp_reg_config, GPIO_PHY_CFG_PORT0_MODPRS_B);

	/* PORT-0, Non-mandatory fields (query_field) */
	p->mpa_fields[0].cfg_pll_int =
		nthw_register_query_field(p->mp_reg_config, GPIO_PHY_CFG_PORT0_PLL_INTR);
	p->mpa_fields[0].cfg_port_rxlos =
		nthw_register_query_field(p->mp_reg_config, GPIO_PHY_CFG_E_PORT0_RXLOS);

	/* PORT-1, config fields */
	p->mpa_fields[1].cfg_fld_lp_mode =
		nthw_register_get_field(p->mp_reg_config, GPIO_PHY_CFG_PORT1_LPMODE);
	p->mpa_fields[1].cfg_int =
		nthw_register_get_field(p->mp_reg_config, GPIO_PHY_CFG_PORT1_INT_B);
	p->mpa_fields[1].cfg_reset =
		nthw_register_get_field(p->mp_reg_config, GPIO_PHY_CFG_PORT1_RESET_B);
	p->mpa_fields[1].cfg_mod_prs =
		nthw_register_get_field(p->mp_reg_config, GPIO_PHY_CFG_PORT1_MODPRS_B);

	/* PORT-1, Non-mandatory fields (query_field) */
	p->mpa_fields[1].cfg_pll_int =
		nthw_register_query_field(p->mp_reg_config, GPIO_PHY_CFG_PORT1_PLL_INTR);
	p->mpa_fields[1].cfg_port_rxlos =
		nthw_register_query_field(p->mp_reg_config, GPIO_PHY_CFG_E_PORT1_RXLOS);

	/* PORT-0, gpio fields */
	p->mpa_fields[0].gpio_fld_lp_mode =
		nthw_register_get_field(p->mp_reg_gpio, GPIO_PHY_GPIO_PORT0_LPMODE);
	p->mpa_fields[0].gpio_int =
		nthw_register_get_field(p->mp_reg_gpio, GPIO_PHY_GPIO_PORT0_INT_B);
	p->mpa_fields[0].gpio_reset =
		nthw_register_get_field(p->mp_reg_gpio, GPIO_PHY_GPIO_PORT0_RESET_B);
	p->mpa_fields[0].gpio_mod_prs =
		nthw_register_get_field(p->mp_reg_gpio, GPIO_PHY_GPIO_PORT0_MODPRS_B);

	/* PORT-0, Non-mandatory fields (query_field) */
	p->mpa_fields[0].gpio_pll_int =
		nthw_register_query_field(p->mp_reg_gpio, GPIO_PHY_GPIO_PORT0_PLL_INTR);
	p->mpa_fields[0].gpio_port_rxlos =
		nthw_register_query_field(p->mp_reg_gpio, GPIO_PHY_GPIO_E_PORT0_RXLOS);

	/* PORT-1, gpio fields */
	p->mpa_fields[1].gpio_fld_lp_mode =
		nthw_register_get_field(p->mp_reg_gpio, GPIO_PHY_GPIO_PORT1_LPMODE);
	p->mpa_fields[1].gpio_int =
		nthw_register_get_field(p->mp_reg_gpio, GPIO_PHY_GPIO_PORT1_INT_B);
	p->mpa_fields[1].gpio_reset =
		nthw_register_get_field(p->mp_reg_gpio, GPIO_PHY_GPIO_PORT1_RESET_B);
	p->mpa_fields[1].gpio_mod_prs =
		nthw_register_get_field(p->mp_reg_gpio, GPIO_PHY_GPIO_PORT1_MODPRS_B);

	/* PORT-1, Non-mandatory fields (query_field) */
	p->mpa_fields[1].gpio_pll_int =
		nthw_register_query_field(p->mp_reg_gpio, GPIO_PHY_GPIO_PORT1_PLL_INTR);
	p->mpa_fields[1].gpio_port_rxlos =
		nthw_register_query_field(p->mp_reg_gpio, GPIO_PHY_GPIO_E_PORT1_RXLOS);

	nthw_register_update(p->mp_reg_config);

	return 0;
}

bool nthw_gpio_phy_is_module_present(nthw_gpio_phy_t *p, uint8_t if_no)
{
	if (if_no >= ARRAY_SIZE(p->mpa_fields)) {
		RTE_ASSERT(false);
		return false;
	}

	/* NOTE: This is a negated GPIO PIN "MODPRS_B" */
	return nthw_field_get_updated(p->mpa_fields[if_no].gpio_mod_prs) == 0U ? true : false;
}

void nthw_gpio_phy_set_low_power(nthw_gpio_phy_t *p, uint8_t if_no, bool enable)
{
	if (if_no >= ARRAY_SIZE(p->mpa_fields)) {
		RTE_ASSERT(false);
		return;
	}

	if (enable)
		nthw_field_set_flush(p->mpa_fields[if_no].gpio_fld_lp_mode);

	else
		nthw_field_clr_flush(p->mpa_fields[if_no].gpio_fld_lp_mode);

	nthw_field_clr_flush(p->mpa_fields[if_no].cfg_fld_lp_mode);	/* enable output */
}

void nthw_gpio_phy_set_reset(nthw_gpio_phy_t *p, uint8_t if_no, bool enable)
{
	if (if_no >= ARRAY_SIZE(p->mpa_fields)) {
		RTE_ASSERT(false);
		return;
	}

	if (enable)
		nthw_field_clr_flush(p->mpa_fields[if_no].gpio_reset);

	else
		nthw_field_set_flush(p->mpa_fields[if_no].gpio_reset);

	nthw_field_clr_flush(p->mpa_fields[if_no].cfg_reset);	/* enable output */
}
