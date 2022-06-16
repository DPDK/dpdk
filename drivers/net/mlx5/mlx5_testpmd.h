/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2021 6WIND S.A.
 * Copyright 2021 Mellanox Technologies, Ltd
 */

#ifndef RTE_PMD_MLX5_TEST_H_
#define RTE_PMD_MLX5_TEST_H_

#include <cmdline_parse.h>
#include <cmdline_parse_num.h>
#include <cmdline_parse_string.h>

/**
 * RTE_ETH_EVENT_RX_AVAIL_THRESH handler sample code.
 * It's called in testpmd, the work flow here is delay a while until
 * RX queueu is empty, then disable host shaper.
 *
 * @param[in] port_id
 *   Port identifier.
 * @param[in] rxq_id
 *   Rx queue identifier.
 */
void
mlx5_test_avail_thresh_event_handler(uint16_t port_id, uint16_t rxq_id);

#endif
