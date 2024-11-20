/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef NTNIC_EVENT_H_
#define NTNIC_EVENT_H_

#include <rte_ethdev.h>

typedef struct ntnic_flm_load_s {
	uint64_t lookup;
	uint64_t lookup_maximum;
	uint64_t access;
	uint64_t access_maximum;
} ntnic_flm_load_t;

typedef struct ntnic_port_load_s {
	uint64_t rx_pps;
	uint64_t rx_pps_maximum;
	uint64_t tx_pps;
	uint64_t tx_pps_maximum;
	uint64_t rx_bps;
	uint64_t rx_bps_maximum;
	uint64_t tx_bps;
	uint64_t tx_bps_maximum;
} ntnic_port_load_t;

struct ntnic_flm_statistic_s {
	uint64_t bytes;
	uint64_t packets;
	uint64_t timestamp;
	uint64_t id;
	uint8_t cause;
};

enum rte_ntnic_event_type {
	RTE_NTNIC_FLM_LOAD_EVENT = RTE_ETH_EVENT_MAX,
	RTE_NTNIC_PORT_LOAD_EVENT,
	RTE_NTNIC_FLM_STATS_EVENT,
};

#endif	/* NTNIC_EVENT_H_ */
