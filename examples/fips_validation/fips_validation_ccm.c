/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include <stdio.h>
#include <string.h>

#include <rte_string_fns.h>
#include <rte_cryptodev.h>
#include <rte_malloc.h>

#include "fips_validation.h"

#define DVPT_STR	"CCM-DVPT"
#define VADT_STR	"CCM-VADT"
#define VPT_STR		"CCM-VPT"
#define VNT_STR		"CCM-VNT"
#define VTT_STR		"CCM-VTT"

#define PARAM_PREFIX	"["
#define ALEN_PREFIX	"Alen = "
#define PLEN_PREFIX	"Plen = "
#define IVLEN_PREFIX	"Nlen = "
#define DIGESTL_PREFIX	"Tlen = "

#define COUNT_STR	"Count = "
#define KEY_STR		"Key = "
#define IV_STR		"Nonce = "
#define PT_STR		"Payload = "
#define CT_STR		"CT = "
#define AAD_STR		"Adata = "
#define POS_NEG_STR	"Result = "

#define POS_KEYWORD	"Pass"
#define NEG_KEYWORD	"Fail"

static int
parser_dvpt_interim(const char *key, char *src, struct fips_val *val)
{
	char *tmp, c, value[10];
	char num_pattern[] = "0123456789";
	int i = 0;

	memset(value, 0, 10);

	tmp = strstr(src, key);
	if (!tmp)
		return -1;

	tmp += strlen(key);

	c = tmp[0];

	while (strchr(num_pattern, c) && i < 10) {
		value[i++] = c;
		c = tmp[i];
	}

	return parser_read_uint32_val("", value, val);
}

static int
parse_dvpt_ct_hex_str(const char *key, char *src, struct fips_val *val)
{
	int ret;

	val->len = vec.pt.len;

	ret = parse_uint8_known_len_hex_str(key, src, val);
	if (ret < 0)
		return ret;

	src += strlen(key) + val->len * 2;

	ret = parse_uint8_known_len_hex_str("", src, &vec.aead.digest);
	if (ret < 0) {
		rte_free(val->val);
		memset(val, 0, sizeof(*val));
		return ret;
	}

	return 0;
}

static int
parse_uint8_ccm_aad_str(const char *key, char *src, struct fips_val *val)
{
	uint32_t len = val->len, j;

	src += strlen(key);

	/* CCM aad requires 18 bytes padding before the real content */
	val->val = rte_zmalloc(NULL, len + 18, 0);
	if (!val->val)
		return -1;

	for (j = 0; j < len; j++) {
		char byte[3] = {src[j * 2], src[j * 2 + 1], '\0'};

		if (parser_read_uint8_hex(&val->val[j + 18], byte) < 0) {
			rte_free(val->val);
			memset(val, 0, sizeof(*val));
			return -EINVAL;
		}
	}

	return 0;
}

struct fips_test_callback ccm_vnt_vec[] = {
		{IV_STR, parse_uint8_known_len_hex_str, &vec.iv},
		{AAD_STR, parse_uint8_ccm_aad_str, &vec.aead.aad},
		{PT_STR, parse_uint8_known_len_hex_str, &vec.pt},
		{NULL, NULL, NULL} /**< end pointer */
};

struct fips_test_callback ccm_vnt_interim_vec[] = {
		{ALEN_PREFIX, parser_read_uint32_val, &vec.aead.aad},
		{PLEN_PREFIX, parser_read_uint32_val, &vec.pt},
		{DIGESTL_PREFIX, parser_read_uint32_val, &vec.aead.digest},
		{IVLEN_PREFIX, parser_read_uint32_val, &vec.iv},
		{KEY_STR, parse_uint8_hex_str, &vec.aead.key},
		{NULL, NULL, NULL} /**< end pointer */
};

struct fips_test_callback ccm_vtt_vec[] = {
		{AAD_STR, parse_uint8_ccm_aad_str, &vec.aead.aad},
		{PT_STR, parse_uint8_known_len_hex_str, &vec.pt},
		{NULL, NULL, NULL} /**< end pointer */
};

struct fips_test_callback ccm_vtt_interim_vec[] = {
		{ALEN_PREFIX, parser_read_uint32_val, &vec.aead.aad},
		{PLEN_PREFIX, parser_read_uint32_val, &vec.pt},
		{IVLEN_PREFIX, parser_read_uint32_val, &vec.iv},
		{DIGESTL_PREFIX, parser_read_uint32_val, &vec.aead.digest},
		{KEY_STR, parse_uint8_hex_str, &vec.aead.key},
		{IV_STR, parse_uint8_known_len_hex_str, &vec.iv},
		{NULL, NULL, NULL} /**< end pointer */
};

struct fips_test_callback ccm_vadt_vec[] = {
		{AAD_STR, parse_uint8_ccm_aad_str, &vec.aead.aad},
		{PT_STR, parse_uint8_known_len_hex_str, &vec.pt},
		{NULL, NULL, NULL} /**< end pointer */
};

struct fips_test_callback ccm_vadt_interim_vec[] = {
		{PLEN_PREFIX, parser_read_uint32_val, &vec.pt},
		{IVLEN_PREFIX, parser_read_uint32_val, &vec.iv},
		{ALEN_PREFIX, parser_read_uint32_val, &vec.aead.aad},
		{DIGESTL_PREFIX, parser_read_uint32_val, &vec.aead.digest},
		{KEY_STR, parse_uint8_hex_str, &vec.aead.key},
		{IV_STR, parse_uint8_known_len_hex_str, &vec.iv},
		{NULL, NULL, NULL} /**< end pointer */
};

struct fips_test_callback ccm_vpt_vec[] = {
		{AAD_STR, parse_uint8_ccm_aad_str, &vec.aead.aad},
		{PT_STR, parse_uint8_known_len_hex_str, &vec.pt},
		{NULL, NULL, NULL} /**< end pointer */
};

struct fips_test_callback ccm_vpt_interim_vec[] = {
		{ALEN_PREFIX, parser_read_uint32_val, &vec.aead.aad},
		{IVLEN_PREFIX, parser_read_uint32_val, &vec.iv},
		{DIGESTL_PREFIX, parser_read_uint32_val, &vec.aead.digest},
		{PLEN_PREFIX, parser_read_uint32_val, &vec.pt},
		{KEY_STR, parse_uint8_hex_str, &vec.aead.key},
		{IV_STR, parse_uint8_known_len_hex_str, &vec.iv},
		{NULL, NULL, NULL} /**< end pointer */
};

struct fips_test_callback ccm_dvpt_vec[] = {
		{IV_STR, parse_uint8_known_len_hex_str, &vec.iv},
		{AAD_STR, parse_uint8_ccm_aad_str, &vec.aead.aad},
		{CT_STR, parse_dvpt_ct_hex_str, &vec.ct},
		{NULL, NULL, NULL} /**< end pointer */
};

struct fips_test_callback ccm_dvpt_interim_vec[] = {
		{ALEN_PREFIX, parser_dvpt_interim, &vec.aead.aad},
		{PLEN_PREFIX, parser_dvpt_interim, &vec.pt},
		{IVLEN_PREFIX, parser_dvpt_interim, &vec.iv},
		{DIGESTL_PREFIX, parser_dvpt_interim, &vec.aead.digest},
		{KEY_STR, parse_uint8_hex_str, &vec.aead.key},
		{NULL, NULL, NULL} /**< end pointer */
};

struct ccm_test_types {
	const char *str;
	uint32_t type;
	const struct fips_test_callback *cb;
	const struct fips_test_callback *cb_interim;
	enum fips_test_op op;
} ctt[] = {
		{DVPT_STR, CCM_DVPT, ccm_dvpt_vec, ccm_dvpt_interim_vec,
			FIPS_TEST_DEC_AUTH_VERIF},
		{VPT_STR, CCM_VPT, ccm_vpt_vec, ccm_vpt_interim_vec,
			FIPS_TEST_ENC_AUTH_GEN},
		{VADT_STR, CCM_VADT, ccm_vadt_vec, ccm_vadt_interim_vec,
			FIPS_TEST_ENC_AUTH_GEN},
		{VNT_STR, CCM_VNT, ccm_vnt_vec, ccm_vnt_interim_vec,
			FIPS_TEST_ENC_AUTH_GEN},
		{VTT_STR, CCM_VTT, ccm_vtt_vec, ccm_vtt_interim_vec,
			FIPS_TEST_ENC_AUTH_GEN},
};

static int
parse_test_ccm_writeback(struct fips_val *val)
{
	struct fips_val tmp_val;

	switch (info.interim_info.ccm_data.test_type) {
	case CCM_DVPT:
		fprintf(info.fp_wr, "%s", POS_NEG_STR);
		if (vec.status == RTE_CRYPTO_OP_STATUS_SUCCESS) {
			fprintf(info.fp_wr, "%s\n", POS_KEYWORD);
			fprintf(info.fp_wr, "%s", PT_STR);

			tmp_val.val = val->val;
			tmp_val.len = vec.pt.len;

			if (tmp_val.len == 0)
				fprintf(info.fp_wr, "00\n");
			else
				parse_write_hex_str(&tmp_val);
		} else
			fprintf(info.fp_wr, "%s\n", NEG_KEYWORD);

		break;

	case CCM_VADT:
	case CCM_VNT:
	case CCM_VPT:
	case CCM_VTT:
		fprintf(info.fp_wr, "%s", CT_STR);

		parse_write_hex_str(val);

		break;

	}

	return 0;
}

int
parse_test_ccm_init(void)
{

	uint32_t i;

	for (i = 0; i < info.nb_vec_lines; i++) {
		char *line = info.vec[i];
		uint32_t j;

		for (j = 0; j < RTE_DIM(ctt); j++)
			if (strstr(line, ctt[j].str)) {
				info.interim_info.ccm_data.test_type =
						ctt[j].type;
				info.callbacks = ctt[j].cb;
				info.interim_callbacks = ctt[j].cb_interim;
				info.op = ctt[j].op;
				break;
		}
	}

	info.parse_writeback = parse_test_ccm_writeback;

	return 0;
}
