/*
 *   BSD LICENSE
 *
 *   Copyright(c) 2017 Cavium, Inc.. All rights reserved.
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
 *     * Neither the name of Cavium, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER(S) OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _LIO_ETHDEV_H_
#define _LIO_ETHDEV_H_

#include <stdint.h>

#include "lio_struct.h"

/* timeout to check link state updates from firmware in us */
#define LIO_LSC_TIMEOUT		100000 /* 100000us (100ms) */
#define LIO_MAX_CMD_TIMEOUT     10000 /* 10000ms (10s) */

#define LIO_DEV(_eth_dev)		((_eth_dev)->data->dev_private)

enum lio_bus_speed {
	LIO_LINK_SPEED_UNKNOWN  = 0,
	LIO_LINK_SPEED_10000    = 10000
};

struct octeon_if_cfg_info {
	uint64_t iqmask;	/** mask for IQs enabled for the port */
	uint64_t oqmask;	/** mask for OQs enabled for the port */
	struct octeon_link_info linfo; /** initial link information */
	char lio_firmware_version[LIO_FW_VERSION_LENGTH];
};

union lio_if_cfg {
	uint64_t if_cfg64;
	struct {
#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
		uint64_t base_queue : 16;
		uint64_t num_iqueues : 16;
		uint64_t num_oqueues : 16;
		uint64_t gmx_port_id : 8;
		uint64_t vf_id : 8;
#else
		uint64_t vf_id : 8;
		uint64_t gmx_port_id : 8;
		uint64_t num_oqueues : 16;
		uint64_t num_iqueues : 16;
		uint64_t base_queue : 16;
#endif
	} s;
};

struct lio_if_cfg_resp {
	uint64_t rh;
	struct octeon_if_cfg_info cfg_info;
	uint64_t status;
};

struct lio_link_status_resp {
	uint64_t rh;
	struct octeon_link_info link_info;
	uint64_t status;
};
#endif	/* _LIO_ETHDEV_H_ */
