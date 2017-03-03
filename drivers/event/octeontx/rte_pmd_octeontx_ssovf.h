/*
 *   BSD LICENSE
 *
 *   Copyright (C) Cavium networks Ltd. 2017.
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

#ifndef __RTE_PMD_OCTEONTX_SSOVF_H__
#define __RTE_PMD_OCTEONTX_SSOVF_H__

#include <rte_common.h>

struct octeontx_ssovf_info {
	uint16_t domain; /* Domain id */
	uint8_t total_ssovfs; /* Total sso groups available in domain */
	uint8_t total_ssowvfs;/* Total sso hws available in domain */
};

enum octeontx_ssovf_type {
	OCTEONTX_SSO_GROUP, /* SSO group vf */
	OCTEONTX_SSO_HWS,  /* SSO hardware workslot vf */
};

struct octeontx_mbox_hdr {
	uint16_t vfid;  /* VF index or pf resource index local to the domain */
	uint8_t coproc; /* Coprocessor id */
	uint8_t msg;    /* Message id */
	uint8_t res_code; /* Functional layer response code */
};

int octeontx_ssovf_info(struct octeontx_ssovf_info *info);
void *octeontx_ssovf_bar(enum octeontx_ssovf_type, uint8_t id, uint8_t bar);
int octeontx_ssovf_mbox_send(struct octeontx_mbox_hdr *hdr,
		void *txdata, uint16_t txlen, void *rxdata, uint16_t rxlen);

#endif /* __RTE_PMD_OCTEONTX_SSOVF_H__ */
