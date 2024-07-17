/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef __NTNIC_MOD_REG_H__
#define __NTNIC_MOD_REG_H__

#include <stdint.h>
#include "nthw_fpga_model.h"
#include "nthw_platform_drv.h"
#include "nthw_drv.h"
#include "nt4ga_adapter.h"
#include "ntnic_nthw_fpga_rst_nt200a0x.h"

struct link_ops_s {
	int (*link_init)(struct adapter_info_s *p_adapter_info, nthw_fpga_t *p_fpga);
};

void register_100g_link_ops(struct link_ops_s *ops);
const struct link_ops_s *get_100g_link_ops(void);
void link_100g_init(void);

struct port_ops {
	bool (*get_nim_present)(struct adapter_info_s *p, int port);

	/*
	 * port:s link mode
	 */
	void (*set_adm_state)(struct adapter_info_s *p, int port, bool adm_state);
	bool (*get_adm_state)(struct adapter_info_s *p, int port);

	/*
	 * port:s link status
	 */
	void (*set_link_status)(struct adapter_info_s *p, int port, bool status);
	bool (*get_link_status)(struct adapter_info_s *p, int port);

	/*
	 * port: link autoneg
	 */
	void (*set_link_autoneg)(struct adapter_info_s *p, int port, bool autoneg);
	bool (*get_link_autoneg)(struct adapter_info_s *p, int port);

	/*
	 * port: link speed
	 */
	void (*set_link_speed)(struct adapter_info_s *p, int port, nt_link_speed_t speed);
	nt_link_speed_t (*get_link_speed)(struct adapter_info_s *p, int port);

	/*
	 * port: link duplex
	 */
	void (*set_link_duplex)(struct adapter_info_s *p, int port, nt_link_duplex_t duplex);
	nt_link_duplex_t (*get_link_duplex)(struct adapter_info_s *p, int port);

	/*
	 * port: loopback mode
	 */
	void (*set_loopback_mode)(struct adapter_info_s *p, int port, uint32_t mode);
	uint32_t (*get_loopback_mode)(struct adapter_info_s *p, int port);

	uint32_t (*get_link_speed_capabilities)(struct adapter_info_s *p, int port);

	/*
	 * port: nim capabilities
	 */
	nim_i2c_ctx_t (*get_nim_capabilities)(struct adapter_info_s *p, int port);

	/*
	 * port: tx power
	 */
	int (*tx_power)(struct adapter_info_s *p, int port, bool disable);
};

void register_port_ops(const struct port_ops *ops);
const struct port_ops *get_port_ops(void);
void port_init(void);

struct adapter_ops {
	int (*init)(struct adapter_info_s *p_adapter_info);
	int (*deinit)(struct adapter_info_s *p_adapter_info);

	int (*show_info)(struct adapter_info_s *p_adapter_info, FILE *pfh);
};

void register_adapter_ops(const struct adapter_ops *ops);
const struct adapter_ops *get_adapter_ops(void);
void adapter_init(void);

struct clk9563_ops {
	const int *(*get_n_data_9563_si5340_nt200a02_u23_v5)(void);
	const clk_profile_data_fmt2_t *(*get_p_data_9563_si5340_nt200a02_u23_v5)(void);
};

void register_clk9563_ops(struct clk9563_ops *ops);
struct clk9563_ops *get_clk9563_ops(void);
void clk9563_ops_init(void);

struct rst_nt200a0x_ops {
	int (*nthw_fpga_rst_nt200a0x_init)(struct fpga_info_s *p_fpga_info,
		struct nthw_fpga_rst_nt200a0x *p_rst);
	int (*nthw_fpga_rst_nt200a0x_reset)(nthw_fpga_t *p_fpga,
		const struct nthw_fpga_rst_nt200a0x *p);
};

void register_rst_nt200a0x_ops(struct rst_nt200a0x_ops *ops);
struct rst_nt200a0x_ops *get_rst_nt200a0x_ops(void);
void rst_nt200a0x_ops_init(void);

struct rst9563_ops {
	int (*nthw_fpga_rst9563_init)(struct fpga_info_s *p_fpga_info,
		struct nthw_fpga_rst_nt200a0x *const p);
};

void register_rst9563_ops(struct rst9563_ops *ops);
struct rst9563_ops *get_rst9563_ops(void);
void rst9563_ops_init(void);

#endif	/* __NTNIC_MOD_REG_H__ */
