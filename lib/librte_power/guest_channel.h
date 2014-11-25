/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
int guest_channel_host_connect(const char *path, unsigned lcore_id);

/**
 * Disconnect from an already connected Virtio-Serial Endpoint.
 *
 *
 * @param lcore_id
 *  lcore_id.
 *
 */
void guest_channel_host_disconnect(unsigned lcore_id);

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
int guest_channel_send_msg(struct channel_packet *pkt, unsigned lcore_id);


#ifdef __cplusplus
}
#endif

#endif
