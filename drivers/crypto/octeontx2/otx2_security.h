/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2020 Marvell International Ltd.
 */

#ifndef __OTX2_SECURITY_H__
#define __OTX2_SECURITY_H__

#include "otx2_cryptodev_sec.h"
#include "otx2_ethdev_sec.h"

#define OTX2_SEC_AH_HDR_LEN			12
#define OTX2_SEC_AES_GCM_IV_LEN			8
#define OTX2_SEC_AES_GCM_MAC_LEN		16
#define OTX2_SEC_AES_CBC_IV_LEN			16
#define OTX2_SEC_SHA1_HMAC_LEN			12

#define OTX2_SEC_AES_GCM_ROUNDUP_BYTE_LEN	4
#define OTX2_SEC_AES_CBC_ROUNDUP_BYTE_LEN	16

union otx2_sec_session_ipsec {
	struct otx2_sec_session_ipsec_ip ip;
	struct otx2_sec_session_ipsec_lp lp;
};

struct otx2_sec_session {
	union otx2_sec_session_ipsec ipsec;
	void *userdata;
	/**< Userdata registered by the application */
} __rte_cache_aligned;

#endif /* __OTX2_SECURITY_H__ */
