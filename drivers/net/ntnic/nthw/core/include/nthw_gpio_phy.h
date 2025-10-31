/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef NTHW_GPIO_PHY_H_
#define NTHW_GPIO_PHY_H_

#define GPIO_PHY_INTERFACES (2)

typedef struct {
	nthw_field_t *cfg_fld_lp_mode;	/* Cfg Low Power Mode */
	nthw_field_t *cfg_int;	/* Cfg Port Interrupt */
	nthw_field_t *cfg_reset;/* Cfg Reset */
	nthw_field_t *cfg_mod_prs;	/* Cfg Module Present */
	nthw_field_t *cfg_pll_int;	/* Cfg PLL Interrupt */
	nthw_field_t *cfg_port_rxlos;	/* Emulate Cfg Port RXLOS */

	nthw_field_t *gpio_fld_lp_mode;	/* Gpio Low Power Mode */
	nthw_field_t *gpio_int;	/* Gpio Port Interrupt */
	nthw_field_t *gpio_reset;	/* Gpio Reset */
	nthw_field_t *gpio_mod_prs;	/* Gpio Module Present */
	nthw_field_t *gpio_pll_int;	/* Gpio PLL Interrupt */
	nthw_field_t *gpio_port_rxlos;	/* Emulate Gpio Port RXLOS */
} gpio_phy_fields_t;

struct nthw_gpio_phy {
	nthw_fpga_t *mp_fpga;
	nthw_module_t *mp_mod_gpio_phy;
	int mn_instance;

	/* Registers */
	nthw_register_t *mp_reg_config;
	nthw_register_t *mp_reg_gpio;

	/* Fields */
	gpio_phy_fields_t mpa_fields[GPIO_PHY_INTERFACES];
};

typedef struct nthw_gpio_phy nthw_gpio_phy_t;

int nthw_gpio_phy_init(nthw_gpio_phy_t *p, nthw_fpga_t *p_fpga, int n_instance);

bool nthw_gpio_phy_is_module_present(nthw_gpio_phy_t *p, uint8_t if_no);
void nthw_gpio_phy_set_low_power(nthw_gpio_phy_t *p, uint8_t if_no, bool enable);
void nthw_gpio_phy_set_reset(nthw_gpio_phy_t *p, uint8_t if_no, bool enable);

#endif	/* NTHW_GPIO_PHY_H_ */
