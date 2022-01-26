/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (C) 2015-2016 Freescale Semiconductor,Inc.
 * Copyright 2018-2019 NXP
 */

#ifndef RTE_EXEC_ENV_WINDOWS

#include <time.h>

#include <rte_common.h>
#include <rte_hexdump.h>
#include <rte_mbuf.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>
#include <rte_pause.h>
#include <rte_bus_vdev.h>
#include <rte_byteorder.h>

#include <rte_crypto.h>
#include <rte_cryptodev.h>
#include <rte_security.h>

#include <rte_lcore.h>
#include "test.h"
#include "test_cryptodev_security_pdcp_test_func.h"

static int cplane_encap(uint32_t sn_size, uint8_t dir,
			enum enc_alg_off enc_alg_off,
			enum auth_alg_off auth_alg_off)
{
	int i = 0;

	switch (sn_size) {
	case 5:
		i = PDCP_CPLANE_OFFSET + enc_alg_off +
			auth_alg_off + ((dir == 0) ?
				UPLINK : DOWNLINK);
		break;
	case 12:
		i = PDCP_CPLANE_LONG_SN_OFFSET + enc_alg_off +
			auth_alg_off + ((dir == 0) ?
				UPLINK : DOWNLINK);
		break;
	default:
		printf("\nInvalid SN: %u for %s\n", sn_size, __func__);
	}

	return test_pdcp_proto_cplane_encap(i);
}

static int
cplane_decap(uint32_t sn_size, uint8_t dir,
	     enum enc_alg_off enc_alg_off,
	     enum auth_alg_off auth_alg_off)
{
	int i = 0;

	switch (sn_size) {
	case 5:
		i = PDCP_CPLANE_OFFSET + enc_alg_off +
			auth_alg_off + ((dir == 0) ?
				UPLINK : DOWNLINK);
		break;
	case 12:
		i = PDCP_CPLANE_LONG_SN_OFFSET + enc_alg_off +
			auth_alg_off + ((dir == 0) ?
				UPLINK : DOWNLINK);
		break;
	default:
		printf("\nInvalid SN: %u for %s\n", sn_size, __func__);
	}

	return test_pdcp_proto_cplane_decap(i);
}

static int uplane_encap_no_integrity(uint32_t sn_size, uint8_t dir,
		enum enc_alg_off enc_alg_off)
{
	int i = PDCP_UPLANE_OFFSET + ((dir == 0) ? UPLINK : DOWNLINK) +
			enc_alg_off;

	switch (sn_size) {
	case 7:
		i += SHORT_SEQ_NUM_OFFSET;
		break;
	case 15:
		i += FIFTEEN_BIT_SEQ_NUM_OFFSET;
		break;
	case 12:
		i += LONG_SEQ_NUM_OFFSET;
		break;
	case 18:
		i += EIGHTEEN_BIT_SEQ_NUM_OFFSET;
		break;
	default:
		printf("\nInvalid SN: %u\n", sn_size);
	}

	return test_pdcp_proto_uplane_encap(i);
}

static int
uplane_decap_no_integrity(uint32_t sn_size, uint8_t dir,
		enum enc_alg_off enc_alg_off)
{
	int i = PDCP_UPLANE_OFFSET + ((dir == 0) ? UPLINK : DOWNLINK) +
			enc_alg_off;

	switch (sn_size) {
	case 7:
		i += SHORT_SEQ_NUM_OFFSET;
		break;
	case 15:
		i += FIFTEEN_BIT_SEQ_NUM_OFFSET;
		break;
	case 12:
		i += LONG_SEQ_NUM_OFFSET;
		break;
	case 18:
		i += EIGHTEEN_BIT_SEQ_NUM_OFFSET;
		break;
	default:
		printf("\nInvalid SN: %u\n", sn_size);
	}

	return test_pdcp_proto_uplane_decap(i);
}

static int
uplane_encap_with_integrity(uint32_t sn_size, uint8_t dir,
		enum enc_alg_off enc_alg_off,
		enum auth_alg_off auth_alg_off)
{
	int i = 0;

	switch (sn_size) {
	case 12:
		i = PDCP_UPLANE_12BIT_OFFSET + enc_alg_off +
			auth_alg_off + ((dir == 0) ?
				UPLINK : DOWNLINK);
		break;
	case 18:
		i = PDCP_UPLANE_18BIT_OFFSET + enc_alg_off +
			auth_alg_off + ((dir == 0) ?
				UPLINK : DOWNLINK);
		break;
	default:
		printf("\nInvalid SN: %u\n", sn_size);
	}

	return test_pdcp_proto_uplane_encap_with_int(i);
}

static int
uplane_decap_with_integrity(uint32_t sn_size, uint8_t dir,
		enum enc_alg_off enc_alg_off,
		enum auth_alg_off auth_alg_off)
{
	int i = 0;

	switch (sn_size) {
	case 12:
		i = PDCP_UPLANE_12BIT_OFFSET + enc_alg_off +
			auth_alg_off + ((dir == 0) ?
				UPLINK : DOWNLINK);
		break;
	case 18:
		i = PDCP_UPLANE_18BIT_OFFSET + enc_alg_off +
			auth_alg_off + ((dir == 0) ?
				UPLINK : DOWNLINK);
		break;
	default:
		printf("\nInvalid SN: %u\n", sn_size);
	}

	return test_pdcp_proto_uplane_decap_with_int(i);
}

#define TEST_PDCP_COUNT(func) do {			\
	if (func == TEST_SUCCESS)  {			\
		printf("\t%d)", n++);			\
		printf(#func"-PASS\n");			\
		i++;					\
	} else {					\
		printf("\t%d)", n++);			\
		printf("+++++ FAILED:" #func"\n");	\
	}						\
} while (0)

int
test_PDCP_PROTO_cplane_encap_all(void)
{
	int i = 0, n = 0;

	TEST_PDCP_COUNT(cplane_encap(5, UPLINK, NULL_ENC, NULL_AUTH));
	TEST_PDCP_COUNT(cplane_encap(5, DOWNLINK, NULL_ENC, NULL_AUTH));
	TEST_PDCP_COUNT(cplane_encap(5, UPLINK, NULL_ENC, SNOW_AUTH));
	TEST_PDCP_COUNT(cplane_encap(5, DOWNLINK, NULL_ENC, SNOW_AUTH));
	TEST_PDCP_COUNT(cplane_encap(5, UPLINK, NULL_ENC, AES_AUTH));
	TEST_PDCP_COUNT(cplane_encap(5, DOWNLINK, NULL_ENC, AES_AUTH));
	TEST_PDCP_COUNT(cplane_encap(5, UPLINK, NULL_ENC, ZUC_AUTH));
	TEST_PDCP_COUNT(cplane_encap(5, DOWNLINK, NULL_ENC, ZUC_AUTH));
	TEST_PDCP_COUNT(cplane_encap(5, UPLINK, SNOW_ENC, NULL_AUTH));
	TEST_PDCP_COUNT(cplane_encap(5, DOWNLINK, SNOW_ENC, NULL_AUTH));
	TEST_PDCP_COUNT(cplane_encap(5, UPLINK, SNOW_ENC, SNOW_AUTH));
	TEST_PDCP_COUNT(cplane_encap(5, DOWNLINK, SNOW_ENC, SNOW_AUTH));
	TEST_PDCP_COUNT(cplane_encap(5, UPLINK, SNOW_ENC, AES_AUTH));
	TEST_PDCP_COUNT(cplane_encap(5, DOWNLINK, SNOW_ENC, AES_AUTH));
	TEST_PDCP_COUNT(cplane_encap(5, UPLINK, SNOW_ENC, ZUC_AUTH));
	TEST_PDCP_COUNT(cplane_encap(5, DOWNLINK, SNOW_ENC, ZUC_AUTH));
	TEST_PDCP_COUNT(cplane_encap(5, UPLINK, AES_ENC, NULL_AUTH));
	TEST_PDCP_COUNT(cplane_encap(5, DOWNLINK, AES_ENC, NULL_AUTH));
	TEST_PDCP_COUNT(cplane_encap(5, UPLINK, AES_ENC, SNOW_AUTH));
	TEST_PDCP_COUNT(cplane_encap(5, DOWNLINK, AES_ENC, SNOW_AUTH));
	TEST_PDCP_COUNT(cplane_encap(5, UPLINK, AES_ENC, AES_AUTH));
	TEST_PDCP_COUNT(cplane_encap(5, DOWNLINK, AES_ENC, AES_AUTH));
	TEST_PDCP_COUNT(cplane_encap(5, UPLINK, AES_ENC, ZUC_AUTH));
	TEST_PDCP_COUNT(cplane_encap(5, DOWNLINK, AES_ENC, ZUC_AUTH));
	TEST_PDCP_COUNT(cplane_encap(5, UPLINK, ZUC_ENC, NULL_AUTH));
	TEST_PDCP_COUNT(cplane_encap(5, DOWNLINK, ZUC_ENC, NULL_AUTH));
	TEST_PDCP_COUNT(cplane_encap(5, UPLINK, ZUC_ENC, SNOW_AUTH));
	TEST_PDCP_COUNT(cplane_encap(5, DOWNLINK, ZUC_ENC, SNOW_AUTH));
	TEST_PDCP_COUNT(cplane_encap(5, UPLINK, ZUC_ENC, AES_AUTH));
	TEST_PDCP_COUNT(cplane_encap(5, DOWNLINK, ZUC_ENC, AES_AUTH));
	TEST_PDCP_COUNT(cplane_encap(5, UPLINK, ZUC_ENC, ZUC_AUTH));
	TEST_PDCP_COUNT(cplane_encap(5, DOWNLINK, ZUC_ENC, ZUC_AUTH));

	/* For 12-bit SN */
	TEST_PDCP_COUNT(cplane_encap(12, UPLINK, NULL_ENC, NULL_AUTH));
	TEST_PDCP_COUNT(cplane_encap(12, DOWNLINK, NULL_ENC, NULL_AUTH));
	TEST_PDCP_COUNT(cplane_encap(12, UPLINK, NULL_ENC, SNOW_AUTH));
	TEST_PDCP_COUNT(cplane_encap(12, DOWNLINK, NULL_ENC, SNOW_AUTH));
	TEST_PDCP_COUNT(cplane_encap(12, UPLINK, NULL_ENC, AES_AUTH));
	TEST_PDCP_COUNT(cplane_encap(12, DOWNLINK, NULL_ENC, AES_AUTH));
	TEST_PDCP_COUNT(cplane_encap(12, UPLINK, NULL_ENC, ZUC_AUTH));
	TEST_PDCP_COUNT(cplane_encap(12, DOWNLINK, NULL_ENC, ZUC_AUTH));
	TEST_PDCP_COUNT(cplane_encap(12, UPLINK, SNOW_ENC, NULL_AUTH));
	TEST_PDCP_COUNT(cplane_encap(12, DOWNLINK, SNOW_ENC, NULL_AUTH));
	TEST_PDCP_COUNT(cplane_encap(12, UPLINK, SNOW_ENC, SNOW_AUTH));
	TEST_PDCP_COUNT(cplane_encap(12, DOWNLINK, SNOW_ENC, SNOW_AUTH));
	TEST_PDCP_COUNT(cplane_encap(12, UPLINK, SNOW_ENC, AES_AUTH));
	TEST_PDCP_COUNT(cplane_encap(12, DOWNLINK, SNOW_ENC, AES_AUTH));
	TEST_PDCP_COUNT(cplane_encap(12, UPLINK, SNOW_ENC, ZUC_AUTH));
	TEST_PDCP_COUNT(cplane_encap(12, DOWNLINK, SNOW_ENC, ZUC_AUTH));
	TEST_PDCP_COUNT(cplane_encap(12, UPLINK, AES_ENC, NULL_AUTH));
	TEST_PDCP_COUNT(cplane_encap(12, DOWNLINK, AES_ENC, NULL_AUTH));
	TEST_PDCP_COUNT(cplane_encap(12, UPLINK, AES_ENC, SNOW_AUTH));

	TEST_PDCP_COUNT(cplane_encap(12, DOWNLINK, AES_ENC, SNOW_AUTH));
	TEST_PDCP_COUNT(cplane_encap(12, UPLINK, AES_ENC, AES_AUTH));
	TEST_PDCP_COUNT(cplane_encap(12, DOWNLINK, AES_ENC, AES_AUTH));
	TEST_PDCP_COUNT(cplane_encap(12, UPLINK, AES_ENC, ZUC_AUTH));
	TEST_PDCP_COUNT(cplane_encap(12, DOWNLINK, AES_ENC, ZUC_AUTH));

	TEST_PDCP_COUNT(cplane_encap(12, UPLINK, ZUC_ENC, NULL_AUTH));
	TEST_PDCP_COUNT(cplane_encap(12, DOWNLINK, ZUC_ENC, NULL_AUTH));
	TEST_PDCP_COUNT(cplane_encap(12, UPLINK, ZUC_ENC, SNOW_AUTH));
	TEST_PDCP_COUNT(cplane_encap(12, DOWNLINK, ZUC_ENC, SNOW_AUTH));
	TEST_PDCP_COUNT(cplane_encap(12, UPLINK, ZUC_ENC, AES_AUTH));
	TEST_PDCP_COUNT(cplane_encap(12, DOWNLINK, ZUC_ENC, AES_AUTH));
	TEST_PDCP_COUNT(cplane_encap(12, UPLINK, ZUC_ENC, ZUC_AUTH));
	TEST_PDCP_COUNT(cplane_encap(12, DOWNLINK, ZUC_ENC, ZUC_AUTH));

	if (n - i)
		printf("## %s: %d passed out of %d\n", __func__, i, n);

	return n - i;
};

int
test_PDCP_PROTO_cplane_decap_all(void)
{
	int i = 0, n = 0;

	TEST_PDCP_COUNT(cplane_decap(5, UPLINK, NULL_ENC, NULL_AUTH));
	TEST_PDCP_COUNT(cplane_decap(5, DOWNLINK, NULL_ENC, NULL_AUTH));
	TEST_PDCP_COUNT(cplane_decap(5, UPLINK, NULL_ENC, SNOW_AUTH));
	TEST_PDCP_COUNT(cplane_decap(5, DOWNLINK, NULL_ENC, SNOW_AUTH));
	TEST_PDCP_COUNT(cplane_decap(5, UPLINK, NULL_ENC, AES_AUTH));
	TEST_PDCP_COUNT(cplane_decap(5, DOWNLINK, NULL_ENC, AES_AUTH));
	TEST_PDCP_COUNT(cplane_decap(5, UPLINK, NULL_ENC, ZUC_AUTH));
	TEST_PDCP_COUNT(cplane_decap(5, DOWNLINK, NULL_ENC, ZUC_AUTH));
	TEST_PDCP_COUNT(cplane_decap(5, UPLINK, SNOW_ENC, NULL_AUTH));
	TEST_PDCP_COUNT(cplane_decap(5, DOWNLINK, SNOW_ENC, NULL_AUTH));
	TEST_PDCP_COUNT(cplane_decap(5, UPLINK, SNOW_ENC, SNOW_AUTH));
	TEST_PDCP_COUNT(cplane_decap(5, DOWNLINK, SNOW_ENC, SNOW_AUTH));
	TEST_PDCP_COUNT(cplane_decap(5, UPLINK, SNOW_ENC, AES_AUTH));
	TEST_PDCP_COUNT(cplane_decap(5, DOWNLINK, SNOW_ENC, AES_AUTH));
	TEST_PDCP_COUNT(cplane_decap(5, UPLINK, SNOW_ENC, ZUC_AUTH));
	TEST_PDCP_COUNT(cplane_decap(5, DOWNLINK, SNOW_ENC, ZUC_AUTH));
	TEST_PDCP_COUNT(cplane_decap(5, UPLINK, AES_ENC, NULL_AUTH));
	TEST_PDCP_COUNT(cplane_decap(5, DOWNLINK, AES_ENC, NULL_AUTH));
	TEST_PDCP_COUNT(cplane_decap(5, UPLINK, AES_ENC, SNOW_AUTH));
	TEST_PDCP_COUNT(cplane_decap(5, DOWNLINK, AES_ENC, SNOW_AUTH));
	TEST_PDCP_COUNT(cplane_decap(5, UPLINK, AES_ENC, AES_AUTH));
	TEST_PDCP_COUNT(cplane_decap(5, DOWNLINK, AES_ENC, AES_AUTH));
	TEST_PDCP_COUNT(cplane_decap(5, UPLINK, AES_ENC, ZUC_AUTH));
	TEST_PDCP_COUNT(cplane_decap(5, DOWNLINK, AES_ENC, ZUC_AUTH));
	TEST_PDCP_COUNT(cplane_decap(5, UPLINK, ZUC_ENC, NULL_AUTH));
	TEST_PDCP_COUNT(cplane_decap(5, DOWNLINK, ZUC_ENC, NULL_AUTH));
	TEST_PDCP_COUNT(cplane_decap(5, UPLINK, ZUC_ENC, SNOW_AUTH));
	TEST_PDCP_COUNT(cplane_decap(5, DOWNLINK, ZUC_ENC, SNOW_AUTH));
	TEST_PDCP_COUNT(cplane_decap(5, UPLINK, ZUC_ENC, AES_AUTH));
	TEST_PDCP_COUNT(cplane_decap(5, DOWNLINK, ZUC_ENC, AES_AUTH));
	TEST_PDCP_COUNT(cplane_decap(5, UPLINK, ZUC_ENC, ZUC_AUTH));
	TEST_PDCP_COUNT(cplane_decap(5, DOWNLINK, ZUC_ENC, ZUC_AUTH));

	/* C-plane 12-bit */
	TEST_PDCP_COUNT(cplane_decap(12, UPLINK, NULL_ENC, NULL_AUTH));
	TEST_PDCP_COUNT(cplane_decap(12, DOWNLINK, NULL_ENC, NULL_AUTH));
	TEST_PDCP_COUNT(cplane_decap(12, UPLINK, NULL_ENC, SNOW_AUTH));
	TEST_PDCP_COUNT(cplane_decap(12, DOWNLINK, NULL_ENC, SNOW_AUTH));
	TEST_PDCP_COUNT(cplane_decap(12, UPLINK, NULL_ENC, AES_AUTH));
	TEST_PDCP_COUNT(cplane_decap(12, DOWNLINK, NULL_ENC, AES_AUTH));
	TEST_PDCP_COUNT(cplane_decap(12, UPLINK, NULL_ENC, ZUC_AUTH));
	TEST_PDCP_COUNT(cplane_decap(12, DOWNLINK, NULL_ENC, ZUC_AUTH));
	TEST_PDCP_COUNT(cplane_decap(12, UPLINK, SNOW_ENC, NULL_AUTH));
	TEST_PDCP_COUNT(cplane_decap(12, DOWNLINK, SNOW_ENC, NULL_AUTH));
	TEST_PDCP_COUNT(cplane_decap(12, UPLINK, SNOW_ENC, SNOW_AUTH));
	TEST_PDCP_COUNT(cplane_decap(12, DOWNLINK, SNOW_ENC, SNOW_AUTH));
	TEST_PDCP_COUNT(cplane_decap(12, UPLINK, SNOW_ENC, AES_AUTH));
	TEST_PDCP_COUNT(cplane_decap(12, DOWNLINK, SNOW_ENC, AES_AUTH));
	TEST_PDCP_COUNT(cplane_decap(12, UPLINK, SNOW_ENC, ZUC_AUTH));
	TEST_PDCP_COUNT(cplane_decap(12, DOWNLINK, SNOW_ENC, ZUC_AUTH));
	TEST_PDCP_COUNT(cplane_decap(12, UPLINK, AES_ENC, NULL_AUTH));
	TEST_PDCP_COUNT(cplane_decap(12, DOWNLINK, AES_ENC, NULL_AUTH));
	TEST_PDCP_COUNT(cplane_decap(12, UPLINK, AES_ENC, SNOW_AUTH));

	TEST_PDCP_COUNT(cplane_decap(12, DOWNLINK, AES_ENC, SNOW_AUTH));
	TEST_PDCP_COUNT(cplane_decap(12, UPLINK, AES_ENC, AES_AUTH));
	TEST_PDCP_COUNT(cplane_decap(12, DOWNLINK, AES_ENC, AES_AUTH));
	TEST_PDCP_COUNT(cplane_decap(12, UPLINK, AES_ENC, ZUC_AUTH));
	TEST_PDCP_COUNT(cplane_decap(12, DOWNLINK, AES_ENC, ZUC_AUTH));

	TEST_PDCP_COUNT(cplane_decap(12, UPLINK, ZUC_ENC, NULL_AUTH));
	TEST_PDCP_COUNT(cplane_decap(12, DOWNLINK, ZUC_ENC, NULL_AUTH));
	TEST_PDCP_COUNT(cplane_decap(12, UPLINK, ZUC_ENC, SNOW_AUTH));
	TEST_PDCP_COUNT(cplane_decap(12, DOWNLINK, ZUC_ENC, SNOW_AUTH));
	TEST_PDCP_COUNT(cplane_decap(12, UPLINK, ZUC_ENC, AES_AUTH));
	TEST_PDCP_COUNT(cplane_decap(12, DOWNLINK, ZUC_ENC, AES_AUTH));
	TEST_PDCP_COUNT(cplane_decap(12, UPLINK, ZUC_ENC, ZUC_AUTH));
	TEST_PDCP_COUNT(cplane_decap(12, DOWNLINK, ZUC_ENC, ZUC_AUTH));

	if (n - i)
		printf("## %s: %d passed out of %d\n", __func__, i, n);

	return n - i;
};

int
test_PDCP_PROTO_uplane_encap_all(void)
{
	int i = 0, n = 0;

	TEST_PDCP_COUNT(uplane_encap_no_integrity(12, UPLINK, NULL_ENC));
	TEST_PDCP_COUNT(uplane_encap_no_integrity(12, DOWNLINK, NULL_ENC));
	TEST_PDCP_COUNT(uplane_encap_no_integrity(7, UPLINK, NULL_ENC));
	TEST_PDCP_COUNT(uplane_encap_no_integrity(7, DOWNLINK, NULL_ENC));
	TEST_PDCP_COUNT(uplane_encap_no_integrity(15, UPLINK, NULL_ENC));
	TEST_PDCP_COUNT(uplane_encap_no_integrity(15, DOWNLINK, NULL_ENC));
	TEST_PDCP_COUNT(uplane_encap_no_integrity(18, UPLINK, NULL_ENC));
	TEST_PDCP_COUNT(uplane_encap_no_integrity(18, DOWNLINK, NULL_ENC));

	TEST_PDCP_COUNT(uplane_encap_no_integrity(12, UPLINK, SNOW_ENC));
	TEST_PDCP_COUNT(uplane_encap_no_integrity(12, DOWNLINK, SNOW_ENC));
	TEST_PDCP_COUNT(uplane_encap_no_integrity(7, UPLINK, SNOW_ENC));
	TEST_PDCP_COUNT(uplane_encap_no_integrity(7, DOWNLINK, SNOW_ENC));
	TEST_PDCP_COUNT(uplane_encap_no_integrity(15, UPLINK, SNOW_ENC));
	TEST_PDCP_COUNT(uplane_encap_no_integrity(15, DOWNLINK, SNOW_ENC));
	TEST_PDCP_COUNT(uplane_encap_no_integrity(18, UPLINK, SNOW_ENC));
	TEST_PDCP_COUNT(uplane_encap_no_integrity(18, DOWNLINK, SNOW_ENC));

	TEST_PDCP_COUNT(uplane_encap_no_integrity(12, UPLINK, AES_ENC));
	TEST_PDCP_COUNT(uplane_encap_no_integrity(12, DOWNLINK, AES_ENC));
	TEST_PDCP_COUNT(uplane_encap_no_integrity(7, UPLINK, AES_ENC));
	TEST_PDCP_COUNT(uplane_encap_no_integrity(7, DOWNLINK, AES_ENC));
	TEST_PDCP_COUNT(uplane_encap_no_integrity(15, UPLINK, AES_ENC));
	TEST_PDCP_COUNT(uplane_encap_no_integrity(15, DOWNLINK, AES_ENC));
	TEST_PDCP_COUNT(uplane_encap_no_integrity(18, UPLINK, AES_ENC));
	TEST_PDCP_COUNT(uplane_encap_no_integrity(18, DOWNLINK, AES_ENC));

	TEST_PDCP_COUNT(uplane_encap_no_integrity(12, UPLINK, ZUC_ENC));
	TEST_PDCP_COUNT(uplane_encap_no_integrity(12, DOWNLINK, ZUC_ENC));
	TEST_PDCP_COUNT(uplane_encap_no_integrity(7, UPLINK, ZUC_ENC));
	TEST_PDCP_COUNT(uplane_encap_no_integrity(7, DOWNLINK, ZUC_ENC));
	TEST_PDCP_COUNT(uplane_encap_no_integrity(15, UPLINK, ZUC_ENC));
	TEST_PDCP_COUNT(uplane_encap_no_integrity(15, DOWNLINK, ZUC_ENC));
	TEST_PDCP_COUNT(uplane_encap_no_integrity(18, UPLINK, ZUC_ENC));
	TEST_PDCP_COUNT(uplane_encap_no_integrity(18, DOWNLINK, ZUC_ENC));

	/* For 12-bit SN with integrity */
	TEST_PDCP_COUNT(uplane_encap_with_integrity(12, UPLINK, NULL_ENC, NULL_AUTH));
	TEST_PDCP_COUNT(uplane_encap_with_integrity(12, DOWNLINK, NULL_ENC, NULL_AUTH));
	TEST_PDCP_COUNT(uplane_encap_with_integrity(12, UPLINK, NULL_ENC, SNOW_AUTH));
	TEST_PDCP_COUNT(uplane_encap_with_integrity(12, DOWNLINK, NULL_ENC, SNOW_AUTH));
	TEST_PDCP_COUNT(uplane_encap_with_integrity(12, UPLINK, NULL_ENC, AES_AUTH));
	TEST_PDCP_COUNT(uplane_encap_with_integrity(12, DOWNLINK, NULL_ENC, AES_AUTH));
	TEST_PDCP_COUNT(uplane_encap_with_integrity(12, UPLINK, NULL_ENC, ZUC_AUTH));
	TEST_PDCP_COUNT(uplane_encap_with_integrity(12, DOWNLINK, NULL_ENC, ZUC_AUTH));

	TEST_PDCP_COUNT(uplane_encap_with_integrity(12, UPLINK, SNOW_ENC, NULL_AUTH));
	TEST_PDCP_COUNT(uplane_encap_with_integrity(12, DOWNLINK, SNOW_ENC, NULL_AUTH));
	TEST_PDCP_COUNT(uplane_encap_with_integrity(12, UPLINK, SNOW_ENC, SNOW_AUTH));
	TEST_PDCP_COUNT(uplane_encap_with_integrity(12, DOWNLINK, SNOW_ENC, SNOW_AUTH));
	TEST_PDCP_COUNT(uplane_encap_with_integrity(12, UPLINK, SNOW_ENC, AES_AUTH));
	TEST_PDCP_COUNT(uplane_encap_with_integrity(12, DOWNLINK, SNOW_ENC, AES_AUTH));
	TEST_PDCP_COUNT(uplane_encap_with_integrity(12, UPLINK, SNOW_ENC, ZUC_AUTH));
	TEST_PDCP_COUNT(uplane_encap_with_integrity(12, DOWNLINK, SNOW_ENC, ZUC_AUTH));

	TEST_PDCP_COUNT(uplane_encap_with_integrity(12, UPLINK, AES_ENC, NULL_AUTH));
	TEST_PDCP_COUNT(uplane_encap_with_integrity(12, DOWNLINK, AES_ENC, NULL_AUTH));
	TEST_PDCP_COUNT(uplane_encap_with_integrity(12, UPLINK, AES_ENC, SNOW_AUTH));
	TEST_PDCP_COUNT(uplane_encap_with_integrity(12, DOWNLINK, AES_ENC, SNOW_AUTH));
	TEST_PDCP_COUNT(uplane_encap_with_integrity(12, UPLINK, AES_ENC, AES_AUTH));
	TEST_PDCP_COUNT(uplane_encap_with_integrity(12, DOWNLINK, AES_ENC, AES_AUTH));
	TEST_PDCP_COUNT(uplane_encap_with_integrity(12, UPLINK, AES_ENC, ZUC_AUTH));
	TEST_PDCP_COUNT(uplane_encap_with_integrity(12, DOWNLINK, AES_ENC, ZUC_AUTH));

	TEST_PDCP_COUNT(uplane_encap_with_integrity(12, UPLINK, ZUC_ENC, NULL_AUTH));
	TEST_PDCP_COUNT(uplane_encap_with_integrity(12, DOWNLINK, ZUC_ENC, NULL_AUTH));
	TEST_PDCP_COUNT(uplane_encap_with_integrity(12, UPLINK, ZUC_ENC, SNOW_AUTH));
	TEST_PDCP_COUNT(uplane_encap_with_integrity(12, DOWNLINK, ZUC_ENC, SNOW_AUTH));
	TEST_PDCP_COUNT(uplane_encap_with_integrity(12, UPLINK, ZUC_ENC, AES_AUTH));
	TEST_PDCP_COUNT(uplane_encap_with_integrity(12, DOWNLINK, ZUC_ENC, AES_AUTH));
	TEST_PDCP_COUNT(uplane_encap_with_integrity(12, UPLINK, ZUC_ENC, ZUC_AUTH));
	TEST_PDCP_COUNT(uplane_encap_with_integrity(12, DOWNLINK, ZUC_ENC, ZUC_AUTH));

	/* For 18-bit SN with integrity */
	TEST_PDCP_COUNT(uplane_encap_with_integrity(18, UPLINK, NULL_ENC, NULL_AUTH));
	TEST_PDCP_COUNT(uplane_encap_with_integrity(18, DOWNLINK, NULL_ENC, NULL_AUTH));
	TEST_PDCP_COUNT(uplane_encap_with_integrity(18, UPLINK, NULL_ENC, SNOW_AUTH));
	TEST_PDCP_COUNT(uplane_encap_with_integrity(18, DOWNLINK, NULL_ENC, SNOW_AUTH));
	TEST_PDCP_COUNT(uplane_encap_with_integrity(18, UPLINK, NULL_ENC, AES_AUTH));
	TEST_PDCP_COUNT(uplane_encap_with_integrity(18, DOWNLINK, NULL_ENC, AES_AUTH));
	TEST_PDCP_COUNT(uplane_encap_with_integrity(18, UPLINK, NULL_ENC, ZUC_AUTH));
	TEST_PDCP_COUNT(uplane_encap_with_integrity(18, DOWNLINK, NULL_ENC, ZUC_AUTH));

	TEST_PDCP_COUNT(uplane_encap_with_integrity(18, UPLINK, SNOW_ENC, NULL_AUTH));
	TEST_PDCP_COUNT(uplane_encap_with_integrity(18, DOWNLINK, SNOW_ENC, NULL_AUTH));
	TEST_PDCP_COUNT(uplane_encap_with_integrity(18, UPLINK, SNOW_ENC, SNOW_AUTH));
	TEST_PDCP_COUNT(uplane_encap_with_integrity(18, DOWNLINK, SNOW_ENC, SNOW_AUTH));
	TEST_PDCP_COUNT(uplane_encap_with_integrity(18, UPLINK, SNOW_ENC, AES_AUTH));
	TEST_PDCP_COUNT(uplane_encap_with_integrity(18, DOWNLINK, SNOW_ENC, AES_AUTH));
	TEST_PDCP_COUNT(uplane_encap_with_integrity(18, UPLINK, SNOW_ENC, ZUC_AUTH));
	TEST_PDCP_COUNT(uplane_encap_with_integrity(18, DOWNLINK, SNOW_ENC, ZUC_AUTH));

	TEST_PDCP_COUNT(uplane_encap_with_integrity(18, UPLINK, AES_ENC, NULL_AUTH));
	TEST_PDCP_COUNT(uplane_encap_with_integrity(18, DOWNLINK, AES_ENC, NULL_AUTH));
	TEST_PDCP_COUNT(uplane_encap_with_integrity(18, UPLINK, AES_ENC, SNOW_AUTH));
	TEST_PDCP_COUNT(uplane_encap_with_integrity(18, DOWNLINK, AES_ENC, SNOW_AUTH));
	TEST_PDCP_COUNT(uplane_encap_with_integrity(18, UPLINK, AES_ENC, AES_AUTH));
	TEST_PDCP_COUNT(uplane_encap_with_integrity(18, DOWNLINK, AES_ENC, AES_AUTH));
	TEST_PDCP_COUNT(uplane_encap_with_integrity(18, UPLINK, AES_ENC, ZUC_AUTH));
	TEST_PDCP_COUNT(uplane_encap_with_integrity(18, DOWNLINK, AES_ENC, ZUC_AUTH));

	TEST_PDCP_COUNT(uplane_encap_with_integrity(18, UPLINK, ZUC_ENC, NULL_AUTH));
	TEST_PDCP_COUNT(uplane_encap_with_integrity(18, DOWNLINK, ZUC_ENC, NULL_AUTH));
	TEST_PDCP_COUNT(uplane_encap_with_integrity(18, UPLINK, ZUC_ENC, SNOW_AUTH));
	TEST_PDCP_COUNT(uplane_encap_with_integrity(18, DOWNLINK, ZUC_ENC, SNOW_AUTH));
	TEST_PDCP_COUNT(uplane_encap_with_integrity(18, UPLINK, ZUC_ENC, AES_AUTH));
	TEST_PDCP_COUNT(uplane_encap_with_integrity(18, DOWNLINK, ZUC_ENC, AES_AUTH));
	TEST_PDCP_COUNT(uplane_encap_with_integrity(18, UPLINK, ZUC_ENC, ZUC_AUTH));
	TEST_PDCP_COUNT(uplane_encap_with_integrity(18, DOWNLINK, ZUC_ENC, ZUC_AUTH));

	if (n - i)
		printf("## %s: %d passed out of %d\n", __func__, i, n);

	return n - i;
};

int
test_PDCP_PROTO_uplane_decap_all(void)
{
	int i = 0, n = 0;

	TEST_PDCP_COUNT(uplane_decap_no_integrity(12, UPLINK, NULL_ENC));
	TEST_PDCP_COUNT(uplane_decap_no_integrity(12, DOWNLINK, NULL_ENC));
	TEST_PDCP_COUNT(uplane_decap_no_integrity(7, UPLINK, NULL_ENC));
	TEST_PDCP_COUNT(uplane_decap_no_integrity(7, DOWNLINK, NULL_ENC));
	TEST_PDCP_COUNT(uplane_decap_no_integrity(15, UPLINK, NULL_ENC));
	TEST_PDCP_COUNT(uplane_decap_no_integrity(15, DOWNLINK, NULL_ENC));
	TEST_PDCP_COUNT(uplane_decap_no_integrity(18, UPLINK, NULL_ENC));
	TEST_PDCP_COUNT(uplane_decap_no_integrity(18, DOWNLINK, NULL_ENC));

	TEST_PDCP_COUNT(uplane_decap_no_integrity(12, UPLINK, SNOW_ENC));
	TEST_PDCP_COUNT(uplane_decap_no_integrity(12, DOWNLINK, SNOW_ENC));
	TEST_PDCP_COUNT(uplane_decap_no_integrity(7, UPLINK, SNOW_ENC));
	TEST_PDCP_COUNT(uplane_decap_no_integrity(7, DOWNLINK, SNOW_ENC));
	TEST_PDCP_COUNT(uplane_decap_no_integrity(15, UPLINK, SNOW_ENC));
	TEST_PDCP_COUNT(uplane_decap_no_integrity(15, DOWNLINK, SNOW_ENC));
	TEST_PDCP_COUNT(uplane_decap_no_integrity(18, UPLINK, SNOW_ENC));
	TEST_PDCP_COUNT(uplane_decap_no_integrity(18, DOWNLINK, SNOW_ENC));

	TEST_PDCP_COUNT(uplane_decap_no_integrity(12, UPLINK, AES_ENC));
	TEST_PDCP_COUNT(uplane_decap_no_integrity(12, DOWNLINK, AES_ENC));
	TEST_PDCP_COUNT(uplane_decap_no_integrity(7, UPLINK, AES_ENC));
	TEST_PDCP_COUNT(uplane_decap_no_integrity(7, DOWNLINK, AES_ENC));
	TEST_PDCP_COUNT(uplane_decap_no_integrity(15, UPLINK, AES_ENC));
	TEST_PDCP_COUNT(uplane_decap_no_integrity(15, DOWNLINK, AES_ENC));
	TEST_PDCP_COUNT(uplane_decap_no_integrity(18, UPLINK, AES_ENC));
	TEST_PDCP_COUNT(uplane_decap_no_integrity(18, DOWNLINK, AES_ENC));

	TEST_PDCP_COUNT(uplane_decap_no_integrity(12, UPLINK, ZUC_ENC));
	TEST_PDCP_COUNT(uplane_decap_no_integrity(12, DOWNLINK, ZUC_ENC));
	TEST_PDCP_COUNT(uplane_decap_no_integrity(7, UPLINK, ZUC_ENC));
	TEST_PDCP_COUNT(uplane_decap_no_integrity(7, DOWNLINK, ZUC_ENC));
	TEST_PDCP_COUNT(uplane_decap_no_integrity(15, UPLINK, ZUC_ENC));
	TEST_PDCP_COUNT(uplane_decap_no_integrity(15, DOWNLINK, ZUC_ENC));
	TEST_PDCP_COUNT(uplane_decap_no_integrity(18, UPLINK, ZUC_ENC));
	TEST_PDCP_COUNT(uplane_decap_no_integrity(18, DOWNLINK, ZUC_ENC));

	/* u-plane 12-bit with integrity */
	TEST_PDCP_COUNT(uplane_decap_with_integrity(12, UPLINK, NULL_ENC, NULL_AUTH));
	TEST_PDCP_COUNT(uplane_decap_with_integrity(12, DOWNLINK, NULL_ENC, NULL_AUTH));
	TEST_PDCP_COUNT(uplane_decap_with_integrity(12, UPLINK, NULL_ENC, SNOW_AUTH));
	TEST_PDCP_COUNT(uplane_decap_with_integrity(12, DOWNLINK, NULL_ENC, SNOW_AUTH));
	TEST_PDCP_COUNT(uplane_decap_with_integrity(12, UPLINK, NULL_ENC, AES_AUTH));
	TEST_PDCP_COUNT(uplane_decap_with_integrity(12, DOWNLINK, NULL_ENC, AES_AUTH));
	TEST_PDCP_COUNT(uplane_decap_with_integrity(12, UPLINK, NULL_ENC, ZUC_AUTH));
	TEST_PDCP_COUNT(uplane_decap_with_integrity(12, DOWNLINK, NULL_ENC, ZUC_AUTH));

	TEST_PDCP_COUNT(uplane_decap_with_integrity(12, UPLINK, SNOW_ENC, NULL_AUTH));
	TEST_PDCP_COUNT(uplane_decap_with_integrity(12, DOWNLINK, SNOW_ENC, NULL_AUTH));
	TEST_PDCP_COUNT(uplane_decap_with_integrity(12, UPLINK, SNOW_ENC, SNOW_AUTH));
	TEST_PDCP_COUNT(uplane_decap_with_integrity(12, DOWNLINK, SNOW_ENC, SNOW_AUTH));
	TEST_PDCP_COUNT(uplane_decap_with_integrity(12, UPLINK, SNOW_ENC, AES_AUTH));
	TEST_PDCP_COUNT(uplane_decap_with_integrity(12, DOWNLINK, SNOW_ENC, AES_AUTH));
	TEST_PDCP_COUNT(uplane_decap_with_integrity(12, UPLINK, SNOW_ENC, ZUC_AUTH));
	TEST_PDCP_COUNT(uplane_decap_with_integrity(12, DOWNLINK, SNOW_ENC, ZUC_AUTH));

	TEST_PDCP_COUNT(uplane_decap_with_integrity(12, UPLINK, AES_ENC, NULL_AUTH));
	TEST_PDCP_COUNT(uplane_decap_with_integrity(12, DOWNLINK, AES_ENC, NULL_AUTH));
	TEST_PDCP_COUNT(uplane_decap_with_integrity(12, UPLINK, AES_ENC, SNOW_AUTH));
	TEST_PDCP_COUNT(uplane_decap_with_integrity(12, DOWNLINK, AES_ENC, SNOW_AUTH));
	TEST_PDCP_COUNT(uplane_decap_with_integrity(12, UPLINK, AES_ENC, AES_AUTH));
	TEST_PDCP_COUNT(uplane_decap_with_integrity(12, DOWNLINK, AES_ENC, AES_AUTH));
	TEST_PDCP_COUNT(uplane_decap_with_integrity(12, UPLINK, AES_ENC, ZUC_AUTH));
	TEST_PDCP_COUNT(uplane_decap_with_integrity(12, DOWNLINK, AES_ENC, ZUC_AUTH));

	TEST_PDCP_COUNT(uplane_decap_with_integrity(12, UPLINK, ZUC_ENC, NULL_AUTH));
	TEST_PDCP_COUNT(uplane_decap_with_integrity(12, DOWNLINK, ZUC_ENC, NULL_AUTH));
	TEST_PDCP_COUNT(uplane_decap_with_integrity(12, UPLINK, ZUC_ENC, SNOW_AUTH));
	TEST_PDCP_COUNT(uplane_decap_with_integrity(12, DOWNLINK, ZUC_ENC, SNOW_AUTH));
	TEST_PDCP_COUNT(uplane_decap_with_integrity(12, UPLINK, ZUC_ENC, AES_AUTH));
	TEST_PDCP_COUNT(uplane_decap_with_integrity(12, DOWNLINK, ZUC_ENC, AES_AUTH));
	TEST_PDCP_COUNT(uplane_decap_with_integrity(12, UPLINK, ZUC_ENC, ZUC_AUTH));
	TEST_PDCP_COUNT(uplane_decap_with_integrity(12, DOWNLINK, ZUC_ENC, ZUC_AUTH));

	/* u-plane 18-bit with integrity */
	TEST_PDCP_COUNT(uplane_decap_with_integrity(18, UPLINK, NULL_ENC, NULL_AUTH));
	TEST_PDCP_COUNT(uplane_decap_with_integrity(18, DOWNLINK, NULL_ENC, NULL_AUTH));
	TEST_PDCP_COUNT(uplane_decap_with_integrity(18, UPLINK, NULL_ENC, SNOW_AUTH));
	TEST_PDCP_COUNT(uplane_decap_with_integrity(18, DOWNLINK, NULL_ENC, SNOW_AUTH));
	TEST_PDCP_COUNT(uplane_decap_with_integrity(18, UPLINK, NULL_ENC, AES_AUTH));
	TEST_PDCP_COUNT(uplane_decap_with_integrity(18, DOWNLINK, NULL_ENC, AES_AUTH));
	TEST_PDCP_COUNT(uplane_decap_with_integrity(18, UPLINK, NULL_ENC, ZUC_AUTH));
	TEST_PDCP_COUNT(uplane_decap_with_integrity(18, DOWNLINK, NULL_ENC, ZUC_AUTH));

	TEST_PDCP_COUNT(uplane_decap_with_integrity(18, UPLINK, SNOW_ENC, NULL_AUTH));
	TEST_PDCP_COUNT(uplane_decap_with_integrity(18, DOWNLINK, SNOW_ENC, NULL_AUTH));
	TEST_PDCP_COUNT(uplane_decap_with_integrity(18, UPLINK, SNOW_ENC, SNOW_AUTH));
	TEST_PDCP_COUNT(uplane_decap_with_integrity(18, DOWNLINK, SNOW_ENC, SNOW_AUTH));
	TEST_PDCP_COUNT(uplane_decap_with_integrity(18, UPLINK, SNOW_ENC, AES_AUTH));
	TEST_PDCP_COUNT(uplane_decap_with_integrity(18, DOWNLINK, SNOW_ENC, AES_AUTH));
	TEST_PDCP_COUNT(uplane_decap_with_integrity(18, UPLINK, SNOW_ENC, ZUC_AUTH));
	TEST_PDCP_COUNT(uplane_decap_with_integrity(18, DOWNLINK, SNOW_ENC, ZUC_AUTH));

	TEST_PDCP_COUNT(uplane_decap_with_integrity(18, UPLINK, AES_ENC, NULL_AUTH));
	TEST_PDCP_COUNT(uplane_decap_with_integrity(18, DOWNLINK, AES_ENC, NULL_AUTH));
	TEST_PDCP_COUNT(uplane_decap_with_integrity(18, UPLINK, AES_ENC, SNOW_AUTH));
	TEST_PDCP_COUNT(uplane_decap_with_integrity(18, DOWNLINK, AES_ENC, SNOW_AUTH));
	TEST_PDCP_COUNT(uplane_decap_with_integrity(18, UPLINK, AES_ENC, AES_AUTH));
	TEST_PDCP_COUNT(uplane_decap_with_integrity(18, DOWNLINK, AES_ENC, AES_AUTH));
	TEST_PDCP_COUNT(uplane_decap_with_integrity(18, UPLINK, AES_ENC, ZUC_AUTH));
	TEST_PDCP_COUNT(uplane_decap_with_integrity(18, DOWNLINK, AES_ENC, ZUC_AUTH));

	TEST_PDCP_COUNT(uplane_decap_with_integrity(18, UPLINK, ZUC_ENC, NULL_AUTH));
	TEST_PDCP_COUNT(uplane_decap_with_integrity(18, DOWNLINK, ZUC_ENC, NULL_AUTH));
	TEST_PDCP_COUNT(uplane_decap_with_integrity(18, UPLINK, ZUC_ENC, SNOW_AUTH));
	TEST_PDCP_COUNT(uplane_decap_with_integrity(18, DOWNLINK, ZUC_ENC, SNOW_AUTH));
	TEST_PDCP_COUNT(uplane_decap_with_integrity(18, UPLINK, ZUC_ENC, AES_AUTH));
	TEST_PDCP_COUNT(uplane_decap_with_integrity(18, DOWNLINK, ZUC_ENC, AES_AUTH));
	TEST_PDCP_COUNT(uplane_decap_with_integrity(18, UPLINK, ZUC_ENC, ZUC_AUTH));
	TEST_PDCP_COUNT(uplane_decap_with_integrity(18, DOWNLINK, ZUC_ENC, ZUC_AUTH));

	if (n - i)
		printf("## %s: %d passed out of %d\n", __func__, i, n);

	return n - i;
};

#endif /* !RTE_EXEC_ENV_WINDOWS */
