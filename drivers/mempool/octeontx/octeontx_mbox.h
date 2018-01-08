/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Cavium, Inc
 */

#ifndef __OCTEONTX_MBOX_H__
#define __OCTEONTX_MBOX_H__

#include <rte_common.h>

#define SSOW_BAR4_LEN			(64 * 1024)
#define SSO_VHGRP_PF_MBOX(x)		(0x200ULL | ((x) << 3))

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

#endif /* __OCTEONTX_MBOX_H__ */
