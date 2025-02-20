/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef NT4GA_LINK_H_
#define NT4GA_LINK_H_

#include "ntos_drv.h"
#include "ntnic_nim.h"
#include "nthw_gfg.h"

enum nt_link_state_e {
	NT_LINK_STATE_UNKNOWN = 0,	/* The link state has not been read yet */
	NT_LINK_STATE_DOWN = 1,	/* The link state is DOWN */
	NT_LINK_STATE_UP = 2,	/* The link state is UP */
	NT_LINK_STATE_ERROR = 3	/* The link state could not be read */
};

typedef enum nt_link_state_e nt_link_state_t, *nt_link_state_p;

enum nt_link_duplex_e {
	NT_LINK_DUPLEX_UNKNOWN = 0,
	NT_LINK_DUPLEX_HALF = 0x01,	/* Half duplex */
	NT_LINK_DUPLEX_FULL = 0x02,	/* Full duplex */
};

typedef enum nt_link_duplex_e nt_link_duplex_t;

enum nt_link_loopback_e {
	NT_LINK_LOOPBACK_OFF = 0,
	NT_LINK_LOOPBACK_HOST = 0x01,	/* Host loopback mode */
	NT_LINK_LOOPBACK_LINE = 0x02,	/* Line loopback mode */
};

enum nt_link_auto_neg_e {
	NT_LINK_AUTONEG_NA = 0,
	NT_LINK_AUTONEG_MANUAL = 0x01,
	NT_LINK_AUTONEG_OFF = NT_LINK_AUTONEG_MANUAL,	/* Auto negotiation OFF */
	NT_LINK_AUTONEG_AUTO = 0x02,
	NT_LINK_AUTONEG_ON = NT_LINK_AUTONEG_AUTO,	/* Auto negotiation ON */
};

typedef struct link_state_s {
	bool link_disabled;
	bool nim_present;
	bool lh_nim_absent;
	bool link_up;
	enum nt_link_state_e link_state;
	enum nt_link_state_e link_state_latched;
} link_state_t;

enum nt_link_speed_e {
	NT_LINK_SPEED_UNKNOWN = 0,
	NT_LINK_SPEED_10M = 0x01,	/* 10 Mbps */
	NT_LINK_SPEED_100M = 0x02,	/* 100 Mbps */
	NT_LINK_SPEED_1G = 0x04,/* 1 Gbps  (Autoneg only) */
	NT_LINK_SPEED_10G = 0x08,	/* 10 Gbps (Autoneg only) */
	NT_LINK_SPEED_40G = 0x10,	/* 40 Gbps (Autoneg only) */
	NT_LINK_SPEED_100G = 0x20,	/* 100 Gbps (Autoneg only) */
	NT_LINK_SPEED_50G = 0x40,	/* 50 Gbps (Autoneg only) */
	NT_LINK_SPEED_25G = 0x80,	/* 25 Gbps (Autoneg only) */
	NT_LINK_SPEED_END	/* always keep this entry as the last in enum */
};
typedef enum nt_link_speed_e nt_link_speed_t;

typedef struct link_info_s {
	enum nt_link_speed_e link_speed;
	enum nt_link_duplex_e link_duplex;
	enum nt_link_auto_neg_e link_auto_neg;
} link_info_t;

typedef struct port_action_s {
	bool port_disable;
	enum nt_link_speed_e port_speed;
	enum nt_link_duplex_e port_duplex;
	uint32_t port_lpbk_mode;
} port_action_t;

typedef struct adapter_100g_s {
	nim_i2c_ctx_t nim_ctx[NUM_ADAPTER_PORTS_MAX];	/* Should be the first field */
	nthw_mac_pcs_t mac_pcs100g[NUM_ADAPTER_PORTS_MAX];
	nthw_gpio_phy_t gpio_phy[NUM_ADAPTER_PORTS_MAX];
} adapter_100g_t;

typedef struct adapter_agx_100g_s {
	nim_i2c_ctx_t nim_ctx[NUM_ADAPTER_PORTS_MAX];	/* should be the first field */
	nthw_gfg_t gfg[NUM_ADAPTER_PORTS_MAX];
} adapter_agx_100g_t;

typedef union adapter_var_s {
	nim_i2c_ctx_t nim_ctx[NUM_ADAPTER_PORTS_MAX];	/* First field in all the adapters type */
	adapter_100g_t var100g;
	adapter_agx_100g_t var_a100g;
} adapter_var_u;

typedef struct nt4ga_link_s {
	link_state_t link_state[NUM_ADAPTER_PORTS_MAX];
	link_info_t link_info[NUM_ADAPTER_PORTS_MAX];
	port_action_t port_action[NUM_ADAPTER_PORTS_MAX];
	uint32_t speed_capa;
	bool variables_initialized;
	adapter_var_u u;
} nt4ga_link_t;

#endif	/* NT4GA_LINK_H_ */
