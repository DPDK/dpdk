/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */
#ifndef _GUEST_CHANNEL_H
#define _GUEST_CHANNEL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <channel_commands.h>

/**
 * Connect to the Virtio-Serial VM end-point located in path. It is
 * thread safe for unique lcore_ids. This function must be only called once from
 * each lcore.
 *
 * @param path
 *  The path to the serial device on the filesystem
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
 *
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
int guest_channel_send_msg(struct channel_packet *pkt, unsigned int lcore_id);

/**
 * Send a message contained in pkt over the Virtio-Serial to the host endpoint.
 *
 * @param pkt
 *  Pointer to a populated struct channel_packet
 *
 * @param lcore_id
 *  lcore_id.
 *
 * @return
 *  - 0 on success.
 *  - Negative on error.
 */
int rte_power_guest_channel_send_msg(struct channel_packet *pkt,
			unsigned int lcore_id);

#ifdef __cplusplus
}
#endif

#endif
