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

enum rte_ntnic_service_tag {
	RTE_NTNIC_SERVICE_FLM_UPDATE = 0,
	RTE_NTNIC_SERVICE_STAT = 1,
	RTE_NTNIC_SERVICE_MAX
};

/**
 * Set the lcore for a specific service.
 *
 * @param tag The service tag to set the lcore for.
 * @param lcore_id The lcore ID to set for the service.
 * @return 0 on success, negative value on failure.
 */
int rte_pmd_ntnic_service_set_lcore(enum rte_ntnic_service_tag tag, uint32_t lcore_id);

/**
 * Get the ID of a specific service.
 *
 * @param tag The service tag to get the ID for.
 * @return The service ID on success, negative value on failure.
 */
int rte_pmd_ntnic_service_get_id(enum rte_ntnic_service_tag tag);

#endif	/* NTNIC_EVENT_H_ */
