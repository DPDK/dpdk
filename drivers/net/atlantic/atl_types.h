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
#include <netinet/in.h>
#include <pthread.h>

#include <rte_common.h>

typedef uint8_t		u8;
typedef int8_t		s8;
typedef uint16_t	u16;
typedef int16_t		s16;
typedef uint32_t	u32;
typedef int32_t		s32;
typedef uint64_t	u64;

#define min(a, b)	RTE_MIN(a, b)
#define max(a, b)	RTE_MAX(a, b)

#include "hw_atl/hw_atl_b0_internal.h"
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

struct aq_rss_parameters {
	u16 base_cpu_number;
	u16 indirection_table_size;
	u16 hash_secret_key_size;
	u32 hash_secret_key[HW_ATL_B0_RSS_HASHKEY_BITS / 8];
	u8 indirection_table[HW_ATL_B0_RSS_REDIRECTION_MAX];
};

/* Macsec stuff */
struct aq_macsec_config {
	struct {
		u32 macsec_enabled;
		u32 encryption_enabled;
		u32 replay_protection_enabled;
	} common;

	struct {
		u32 idx;
		u32 mac[2]; /* 6 bytes */
	} txsc;

	struct {
		u32 idx;
		u32 an; /* association number on the local side */
		u32 pn; /* packet number on the local side */
		u32 key[4]; /* 128 bit key */
	} txsa;

	struct {
		u32 mac[2]; /* 6 bytes */
		u32 pi;
	} rxsc;

	struct {
		u32 idx;
		u32 an; /* association number on the remote side */
		u32 pn; /* packet number on the remote side */
		u32 key[4]; /* 128 bit key */
	} rxsa;
};

struct aq_hw_cfg_s {
	bool is_lro;
	bool is_rss;
	unsigned int num_rss_queues;
	int wol;

	int link_speed_msk;
	int irq_type;
	int irq_mask;
	unsigned int vecs;

	bool vlan_strip;
	uint32_t vlan_filter[HW_ATL_B0_MAX_VLAN_IDS];
	uint32_t flow_control;

	struct aq_rss_parameters aq_rss;
	struct aq_macsec_config aq_macsec;
};

struct aq_hw_s {
	u16 device_id;
	u16 vendor_id;
	bool adapter_stopped;

	u8 rbl_enabled:1;
	struct aq_hw_cfg_s *aq_nic_cfg;
	const struct aq_fw_ops *aq_fw_ops;
	void *mmio;

	struct aq_hw_link_status_s aq_link_status;
	bool is_autoneg;

	struct hw_aq_atl_utils_mbox mbox;
	struct hw_atl_stats_s last_stats;
	struct aq_stats_s curr_stats;

	u32 caps_lo;

	u64 speed;
	unsigned int chip_features;
	u32 fw_ver_actual;
	u32 mbox_addr;
	u32 rpc_addr;
	u32 rpc_tid;
	struct hw_aq_atl_utils_fw_rpc rpc;

	pthread_mutex_t mbox_mutex;
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

	int (*get_flow_control)(struct aq_hw_s *self, u32 *fc);
	int (*set_flow_control)(struct aq_hw_s *self);

	int (*led_control)(struct aq_hw_s *self, u32 mode);

	int (*get_eeprom)(struct aq_hw_s *self, int dev_addr,
			  u32 *data, u32 len, u32 offset);

	int (*set_eeprom)(struct aq_hw_s *self, int dev_addr,
			  u32 *data, u32 len, u32 offset);

	int (*send_macsec_req)(struct aq_hw_s *self,
			       struct macsec_msg_fw_request *req,
			       struct macsec_msg_fw_response *response);
};

struct atl_sw_stats {
	u64 crcerrs;
	u64 errbc;
	u64 mspdc;
	u64 mpctotal;
	u64 mpc[8];
	u64 mlfc;
	u64 mrfc;
	u64 rlec;
	u64 lxontxc;
	u64 lxonrxc;
	u64 lxofftxc;
	u64 lxoffrxc;
	u64 pxontxc[8];
	u64 pxonrxc[8];
	u64 pxofftxc[8];
	u64 pxoffrxc[8];
	u64 gprc;
	u64 bprc;
	u64 mprc;
	u64 gptc;
	u64 gorc;
	u64 gotc;
	u64 tor;
	u64 tpr;
	u64 tpt;
	u64 mptc;
	u64 bptc;
	u64 xec;
	u64 fccrc;
	u64 ldpcec;
	u64 pcrc8ec;

	u64 rx_nombuf;
	u64 q_ipackets[RTE_ETHDEV_QUEUE_STAT_CNTRS];
	u64 q_opackets[RTE_ETHDEV_QUEUE_STAT_CNTRS];
	u64 q_ibytes[RTE_ETHDEV_QUEUE_STAT_CNTRS];
	u64 q_obytes[RTE_ETHDEV_QUEUE_STAT_CNTRS];
	u64 q_errors[RTE_ETHDEV_QUEUE_STAT_CNTRS];
};

#endif
