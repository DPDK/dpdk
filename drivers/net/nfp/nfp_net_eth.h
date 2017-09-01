/*
 * Copyright (c) 2017 Netronome Systems, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *  this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *  notice, this list of conditions and the following disclaimer in the
 *  documentation and/or other materials provided with the distribution
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *  contributors may be used to endorse or promote products derived from this
 *  software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * vim:shiftwidth=8:noexpandtab
 *
 * @file dpdk/pmd/nfp_net_eth.h
 *
 * Netronome NFP_NET PDM driver
 */

union eth_table_entry {
	struct {
		uint64_t port;
		uint64_t state;
		uint8_t mac_addr[6];
		uint8_t resv[2];
		uint64_t control;
	};
	uint64_t raw[4];
};

#ifndef BIT_ULL
#define BIT_ULL(a) (1ULL << (a))
#endif

#define NSP_ETH_NBI_PORT_COUNT          24
#define NSP_ETH_MAX_COUNT               (2 * NSP_ETH_NBI_PORT_COUNT)
#define NSP_ETH_TABLE_SIZE   (NSP_ETH_MAX_COUNT * sizeof(union eth_table_entry))

#define NSP_ETH_PORT_LANES              0xf
#define NSP_ETH_PORT_INDEX              0xff00
#define NSP_ETH_PORT_LABEL              0x3f000000000000
#define NSP_ETH_PORT_PHYLABEL           0xfc0000000000000

#define NSP_ETH_PORT_LANES_MASK         rte_cpu_to_le_64(NSP_ETH_PORT_LANES)

#define NSP_ETH_STATE_CONFIGURED        BIT_ULL(0)
#define NSP_ETH_STATE_ENABLED           BIT_ULL(1)
#define NSP_ETH_STATE_TX_ENABLED        BIT_ULL(2)
#define NSP_ETH_STATE_RX_ENABLED        BIT_ULL(3)
#define NSP_ETH_STATE_RATE              0xf00
#define NSP_ETH_STATE_INTERFACE         0xff000
#define NSP_ETH_STATE_MEDIA             0x300000
#define NSP_ETH_STATE_OVRD_CHNG         BIT_ULL(22)
#define NSP_ETH_STATE_ANEG              0x3800000

#define NSP_ETH_CTRL_CONFIGURED         BIT_ULL(0)
#define NSP_ETH_CTRL_ENABLED            BIT_ULL(1)
#define NSP_ETH_CTRL_TX_ENABLED         BIT_ULL(2)
#define NSP_ETH_CTRL_RX_ENABLED         BIT_ULL(3)
#define NSP_ETH_CTRL_SET_RATE           BIT_ULL(4)
#define NSP_ETH_CTRL_SET_LANES          BIT_ULL(5)
#define NSP_ETH_CTRL_SET_ANEG           BIT_ULL(6)
