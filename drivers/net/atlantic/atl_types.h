/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Aquantia Corporation
 */
#ifndef ATL_TYPES_H
#define ATL_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <inttypes.h>
#include <string.h>
#include <stdbool.h>


typedef uint8_t		u8;
typedef int8_t		s8;
typedef uint16_t	u16;
typedef int16_t		s16;
typedef uint32_t	u32;
typedef int32_t		s32;
typedef uint64_t	u64;

#define min(a, b)	RTE_MIN(a, b)
#define max(a, b)	RTE_MAX(a, b)

#include "hw_atl/hw_atl_utils.h"

struct aq_hw_link_status_s {
	unsigned int mbps;
};

struct aq_stats_s {
	u64 uprc;
	u64 mprc;
	u64 bprc;
	u64 erpt;
	u64 uptc;
	u64 mptc;
	u64 bptc;
	u64 erpr;
	u64 mbtc;
	u64 bbtc;
	u64 mbrc;
	u64 bbrc;
	u64 ubrc;
	u64 ubtc;
	u64 dpc;
	u64 dma_pkt_rc;
	u64 dma_pkt_tc;
	u64 dma_oct_rc;
	u64 dma_oct_tc;
};

struct aq_hw_cfg_s {
	bool is_lro;
	int wol;

	int link_speed_msk;
	int irq_type;
	int irq_mask;
	unsigned int vecs;

	uint32_t flow_control;
};

struct aq_hw_s {
	u8 rbl_enabled:1;
	struct aq_hw_cfg_s *aq_nic_cfg;
	const struct aq_fw_ops *aq_fw_ops;
	void *mmio;

	struct aq_hw_link_status_s aq_link_status;

	struct hw_aq_atl_utils_mbox mbox;
	struct hw_atl_stats_s last_stats;
	struct aq_stats_s curr_stats;

	unsigned int chip_features;
	u32 fw_ver_actual;
	u32 mbox_addr;
	u32 rpc_addr;
	u32 rpc_tid;
	struct hw_aq_atl_utils_fw_rpc rpc;
};

struct aq_fw_ops {
	int (*init)(struct aq_hw_s *self);

	int (*deinit)(struct aq_hw_s *self);

	int (*reset)(struct aq_hw_s *self);

	int (*get_mac_permanent)(struct aq_hw_s *self, u8 *mac);

	int (*set_link_speed)(struct aq_hw_s *self, u32 speed);

	int (*set_state)(struct aq_hw_s *self,
			enum hal_atl_utils_fw_state_e state);

	int (*update_link_status)(struct aq_hw_s *self);

	int (*update_stats)(struct aq_hw_s *self);

	int (*set_power)(struct aq_hw_s *self, unsigned int power_state,
			u8 *mac);

	int (*get_temp)(struct aq_hw_s *self, int *temp);

	int (*get_cable_len)(struct aq_hw_s *self, int *cable_len);

	int (*set_eee_rate)(struct aq_hw_s *self, u32 speed);

	int (*get_eee_rate)(struct aq_hw_s *self, u32 *rate,
			u32 *supported_rates);

	int (*set_flow_control)(struct aq_hw_s *self);

	int (*led_control)(struct aq_hw_s *self, u32 mode);

	int (*get_eeprom)(struct aq_hw_s *self, u32 *data, u32 len);

	int (*set_eeprom)(struct aq_hw_s *self, u32 *data, u32 len);
};

#endif
