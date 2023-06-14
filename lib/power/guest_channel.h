/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */
#ifndef _GUEST_CHANNEL_H
#define _GUEST_CHANNEL_H

/**
 * Check if any Virtio-Serial VM end-points exist in path.
 *
 * @param path
 *  The path to the serial device on the filesystem
 *
 * @return
 *  - 1 if at least one potential end-point found.
 *  - 0 if no end-points found.
 */
int guest_channel_host_check_exists(const char *path);

/**
 * Connect to the Virtio-Serial VM end-point located in path. It is
 * thread safe for unique lcore_ids. This function must be only called once from
 * each lcore.
 *
 * @param path
 *  The path to the serial device on the filesystem
 *
 * @param lcore_id
 *  lcore_id.
 *
 * @return
 *  - 0 on success.
 *  - Negative on error.
 */
int guest_channel_host_connect(const char *path, unsigned int lcore_id);

/**
 * Disconnect from an already connected Virtio-Serial Endpoint.
 *
 *
 * @param lcore_id
 *  lcore_id.
 */
void guest_channel_host_disconnect(unsigned int lcore_id);

/**
 * Send a message contained in pkt over the Virtio-Serial to the host endpoint.
 *
 * @param pkt
 *  Pointer to a populated struct guest_agent_pkt
 *
 * @param lcore_id
 *  lcore_id.
 *
 * @return
 *  - 0 on success.
 *  - Negative on channel not connected.
 *  - errno on write to channel error.
 */
int guest_channel_send_msg(struct rte_power_channel_packet *pkt,
		unsigned int lcore_id);

/**
 * Read a message contained in pkt over the Virtio-Serial
 * from the host endpoint.
 *
 * @param pkt
 *  Pointer to rte_power_channel_packet or
 *  rte_power_channel_packet_freq_list struct.
 *
 * @param pkt_len
 *  Size of expected data packet.
 *
 * @param lcore_id
 *  lcore_id.
 *
 * @return
 *  - 0 on success.
 *  - Negative on error.
 */
int power_guest_channel_read_msg(void *pkt,
		size_t pkt_len,
		unsigned int lcore_id);

#endif
