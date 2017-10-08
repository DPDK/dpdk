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

#include <string.h>

#include "octeontx_bgx.h"

int
octeontx_bgx_port_open(int port, octeontx_mbox_bgx_port_conf_t *conf)
{
	struct octeontx_mbox_hdr hdr;
	octeontx_mbox_bgx_port_conf_t bgx_conf;
	int len = sizeof(octeontx_mbox_bgx_port_conf_t);
	int res;

	memset(&bgx_conf, 0, sizeof(octeontx_mbox_bgx_port_conf_t));
	hdr.coproc = OCTEONTX_BGX_COPROC;
	hdr.msg = MBOX_BGX_PORT_OPEN;
	hdr.vfid = port;

	res = octeontx_ssovf_mbox_send(&hdr, NULL, 0, &bgx_conf, len);
	if (res < 0)
		return -EACCES;

	conf->enable = bgx_conf.enable;
	conf->promisc = bgx_conf.promisc;
	conf->bpen = bgx_conf.bpen;
	conf->node = bgx_conf.node;
	conf->base_chan = bgx_conf.base_chan;
	conf->num_chans = bgx_conf.num_chans;
	conf->mtu = bgx_conf.mtu;
	conf->bgx = bgx_conf.bgx;
	conf->lmac = bgx_conf.lmac;
	conf->mode = bgx_conf.mode;
	conf->pkind = bgx_conf.pkind;
	memcpy(conf->macaddr, bgx_conf.macaddr, 6);

	return res;
}

int
octeontx_bgx_port_close(int port)
{
	struct octeontx_mbox_hdr hdr;
	int res;

	hdr.coproc = OCTEONTX_BGX_COPROC;
	hdr.msg = MBOX_BGX_PORT_CLOSE;
	hdr.vfid = port;

	res = octeontx_ssovf_mbox_send(&hdr, NULL, 0, NULL, 0);
	if (res < 0)
		return -EACCES;

	return res;
}

int
octeontx_bgx_port_start(int port)
{
	struct octeontx_mbox_hdr hdr;
	int res;

	hdr.coproc = OCTEONTX_BGX_COPROC;
	hdr.msg = MBOX_BGX_PORT_START;
	hdr.vfid = port;

	res = octeontx_ssovf_mbox_send(&hdr, NULL, 0, NULL, 0);
	if (res < 0)
		return -EACCES;

	return res;
}

int
octeontx_bgx_port_stop(int port)
{
	struct octeontx_mbox_hdr hdr;
	int res;

	hdr.coproc = OCTEONTX_BGX_COPROC;
	hdr.msg = MBOX_BGX_PORT_STOP;
	hdr.vfid = port;

	res = octeontx_ssovf_mbox_send(&hdr, NULL, 0, NULL, 0);
	if (res < 0)
		return -EACCES;

	return res;
}
