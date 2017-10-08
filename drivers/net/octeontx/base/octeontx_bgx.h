/*
 *   BSD LICENSE
 *
 *   Copyright (C) Cavium Inc. 2017. All rights reserved.
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
 *     * Neither the name of Cavium networks nor the names of its
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

#ifndef __OCTEONTX_BGX_H__
#define __OCTEONTX_BGX_H__

#include <stddef.h>
#include <stdint.h>

#include <rte_pmd_octeontx_ssovf.h>

#define OCTEONTX_BGX_COPROC	        6

/* BGX messages */
#define MBOX_BGX_PORT_OPEN              0
#define MBOX_BGX_PORT_CLOSE             1
#define MBOX_BGX_PORT_START             2
#define MBOX_BGX_PORT_STOP              3

/* BGX port configuration parameters: */
typedef struct octeontx_mbox_bgx_port_conf {
	uint8_t enable;
	uint8_t promisc;
	uint8_t bpen;
	uint8_t macaddr[6]; /* MAC address.*/
	uint8_t fcs_strip;
	uint8_t bcast_mode;
	uint8_t mcast_mode;
	uint8_t node; /* CPU node */
	uint16_t base_chan;
	uint16_t num_chans;
	uint16_t mtu;
	uint8_t bgx;
	uint8_t lmac;
	uint8_t mode;
	uint8_t pkind;
} octeontx_mbox_bgx_port_conf_t;

int octeontx_bgx_port_open(int port, octeontx_mbox_bgx_port_conf_t *conf);
int octeontx_bgx_port_close(int port);
int octeontx_bgx_port_start(int port);
int octeontx_bgx_port_stop(int port);

#endif	/* __OCTEONTX_BGX_H__ */

