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

/*
 * This define sets the timeout resolution of aged flow scanner (scrubber).
 *
 * The timeout resolution feature is provided in order to reduce the number of
 * write-back operations for flows without attached meter. If the resolution
 * is disabled (set to 0) and flow timeout is enabled via age action, then a write-back
 * occurs every the flow is evicted from the flow cache, essentially causing the
 * lookup performance to drop to that of a flow with meter. By setting the timeout
 * resolution (>0), write-back for flows happens only when the difference between
 * the last recorded time for the flow and the current time exceeds the chosen resolution.
 *
 * The parameter value is a power of 2 in units of 2^28 nanoseconds. It means that value 8 sets
 * the timeout resolution to: 2^8 * 2^28 / 1e9 = 68,7 seconds
 *
 * NOTE: This parameter has a significant impact on flow lookup performance, especially
 * if full scanner timeout resolution (=0) is configured.
 */
#define NTNIC_SCANNER_TIMEOUT_RESOLUTION 8

#endif	/* _FLOW_API_PROFILE_INLINE_CONFIG_H_ */
