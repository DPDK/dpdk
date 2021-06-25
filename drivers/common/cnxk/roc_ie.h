/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef __ROC_IE_H__
#define __ROC_IE_H__

/* CNXK IPSEC helper macros */
#define ROC_IE_AH_HDR_LEN      12
#define ROC_IE_AES_GCM_IV_LEN  8
#define ROC_IE_AES_GCM_MAC_LEN 16
#define ROC_IE_AES_CBC_IV_LEN  16
#define ROC_IE_SHA1_HMAC_LEN   12
#define ROC_IE_AUTH_KEY_LEN_MAX 64

#define ROC_IE_AES_GCM_ROUNDUP_BYTE_LEN 4
#define ROC_IE_AES_CBC_ROUNDUP_BYTE_LEN 16

#endif /* __ROC_IE_H__ */
