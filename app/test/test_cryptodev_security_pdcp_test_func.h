/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2019 NXP
 */

#ifndef SECURITY_PDCP_TEST_FUNC_H_
#define SECURITY_PDCP_TEST_FUNC_H_

#define PDCP_CPLANE_OFFSET		0
#define PDCP_CPLANE_LONG_SN_OFFSET	32
#define LONG_SEQ_NUM_OFFSET		0
#define SHORT_SEQ_NUM_OFFSET		2
#define FIFTEEN_BIT_SEQ_NUM_OFFSET	4
#define EIGHTEEN_BIT_SEQ_NUM_OFFSET	6
#define UPLINK				0
#define DOWNLINK			1
/* key length(in bytes) for F8 */
#define F8_KEY_LEN			16

enum enc_alg_off {
	NULL_ENC = 0,
	SNOW_ENC = 8,
	AES_ENC = 16,
	ZUC_ENC = 24
};
enum auth_alg_off {
	NULL_AUTH = 0,
	SNOW_AUTH = 2,
	AES_AUTH = 4,
	ZUC_AUTH = 6
};

int test_pdcp_proto_cplane_encap(int i);

int test_PDCP_PROTO_cplane_encap_all(void);

#endif /* SECURITY_PDCP_TEST_FUNC_H_ */
