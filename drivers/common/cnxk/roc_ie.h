/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef __ROC_IE_H__
#define __ROC_IE_H__

enum {
	ROC_IE_SA_DIR_INBOUND = 0,
	ROC_IE_SA_DIR_OUTBOUND = 1,
};

enum {
	ROC_IE_SA_IP_VERSION_4 = 0,
	ROC_IE_SA_IP_VERSION_6 = 1,
};

enum {
	ROC_IE_SA_MODE_TRANSPORT = 0,
	ROC_IE_SA_MODE_TUNNEL = 1,
};

enum {
	ROC_IE_SA_PROTOCOL_AH = 0,
	ROC_IE_SA_PROTOCOL_ESP = 1,
};

enum {
	ROC_IE_SA_AES_KEY_LEN_128 = 1,
	ROC_IE_SA_AES_KEY_LEN_192 = 2,
	ROC_IE_SA_AES_KEY_LEN_256 = 3,
};

#endif /* __ROC_IE_H__ */
