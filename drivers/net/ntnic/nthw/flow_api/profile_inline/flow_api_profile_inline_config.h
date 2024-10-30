/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef _FLOW_API_PROFILE_INLINE_CONFIG_H_
#define _FLOW_API_PROFILE_INLINE_CONFIG_H_

/*
 * Statistics are generated each time the byte counter crosses a limit.
 * If BYTE_LIMIT is zero then the byte counter does not trigger statistics
 * generation.
 *
 * Format: 2^(BYTE_LIMIT + 15) bytes
 * Valid range: 0 to 31
 *
 * Example: 2^(8 + 15) = 2^23 ~~ 8MB
 */
#define NTNIC_FLOW_PERIODIC_STATS_BYTE_LIMIT 8

/*
 * Statistics are generated each time the packet counter crosses a limit.
 * If PKT_LIMIT is zero then the packet counter does not trigger statistics
 * generation.
 *
 * Format: 2^(PKT_LIMIT + 11) pkts
 * Valid range: 0 to 31
 *
 * Example: 2^(5 + 11) = 2^16 pkts ~~ 64K pkts
 */
#define NTNIC_FLOW_PERIODIC_STATS_PKT_LIMIT 5

/*
 * Statistics are generated each time flow time (measured in ns) crosses a
 * limit.
 * If BYTE_TIMEOUT is zero then the flow time does not trigger statistics
 * generation.
 *
 * Format: 2^(BYTE_TIMEOUT + 15) ns
 * Valid range: 0 to 31
 *
 * Example: 2^(23 + 15) = 2^38 ns ~~ 275 sec
 */
#define NTNIC_FLOW_PERIODIC_STATS_BYTE_TIMEOUT 23

/*
 * This define sets the percentage of the full processing capacity
 * being reserved for scan operations. The scanner is responsible
 * for detecting aged out flows and meters with statistics timeout.
 *
 * A high scanner load percentage will make this detection more precise
 * but will also give lower packet processing capacity.
 *
 * The percentage is given as a decimal number, e.g. 0.01 for 1%, which is the recommended value.
 */
#define NTNIC_SCANNER_LOAD 0.01

#endif  /* _FLOW_API_PROFILE_INLINE_CONFIG_H_ */
