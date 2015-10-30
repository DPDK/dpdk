/*-
 *   BSD LICENSE
 *
 *   Copyright 2015 6WIND S.A.
 *   Copyright 2015 Mellanox.
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
 *     * Neither the name of 6WIND S.A. nor the names of its
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

#include <stddef.h>
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include <netinet/in.h>
#include <linux/if.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>

/* Verbs header. */
/* ISO C doesn't support unnamed structs/unions, disabling -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-pedantic"
#endif
#include <infiniband/verbs.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-pedantic"
#endif

/* DPDK headers don't like -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-pedantic"
#endif
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_common.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-pedantic"
#endif

#include "mlx5.h"
#include "mlx5_utils.h"

/**
 * Get MAC address by querying netdevice.
 *
 * @param[in] priv
 *   struct priv for the requested device.
 * @param[out] mac
 *   MAC address output buffer.
 *
 * @return
 *   0 on success, -1 on failure and errno is set.
 */
int
priv_get_mac(struct priv *priv, uint8_t (*mac)[ETHER_ADDR_LEN])
{
	struct ifreq request;

	if (priv_ifreq(priv, SIOCGIFHWADDR, &request))
		return -1;
	memcpy(mac, request.ifr_hwaddr.sa_data, ETHER_ADDR_LEN);
	return 0;
}

/**
 * Unregister a MAC address.
 *
 * @param priv
 *   Pointer to private structure.
 * @param mac_index
 *   MAC address index.
 */
static void
priv_mac_addr_del(struct priv *priv, unsigned int mac_index)
{
	assert(mac_index < RTE_DIM(priv->mac));
	if (!BITFIELD_ISSET(priv->mac_configured, mac_index))
		return;
	BITFIELD_RESET(priv->mac_configured, mac_index);
}

/**
 * Register a MAC address.
 *
 * @param priv
 *   Pointer to private structure.
 * @param mac_index
 *   MAC address index to use.
 * @param mac
 *   MAC address to register.
 *
 * @return
 *   0 on success, errno value on failure.
 */
int
priv_mac_addr_add(struct priv *priv, unsigned int mac_index,
		  const uint8_t (*mac)[ETHER_ADDR_LEN])
{
	unsigned int i;

	assert(mac_index < RTE_DIM(priv->mac));
	/* First, make sure this address isn't already configured. */
	for (i = 0; (i != RTE_DIM(priv->mac)); ++i) {
		/* Skip this index, it's going to be reconfigured. */
		if (i == mac_index)
			continue;
		if (!BITFIELD_ISSET(priv->mac_configured, i))
			continue;
		if (memcmp(priv->mac[i].addr_bytes, *mac, sizeof(*mac)))
			continue;
		/* Address already configured elsewhere, return with error. */
		return EADDRINUSE;
	}
	if (BITFIELD_ISSET(priv->mac_configured, mac_index))
		priv_mac_addr_del(priv, mac_index);
	priv->mac[mac_index] = (struct ether_addr){
		{
			(*mac)[0], (*mac)[1], (*mac)[2],
			(*mac)[3], (*mac)[4], (*mac)[5]
		}
	};
	BITFIELD_SET(priv->mac_configured, mac_index);
	return 0;
}
