/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#ifdef RTE_EXEC_ENV_FREEBSD
	#define _WITH_GETLINE
#endif
#include <stdio.h>
#include <stdbool.h>
#include <rte_malloc.h>

#include "test_bbdev_vector.h"

#define VALUE_DELIMITER ","
#define ENTRY_DELIMITER "="

const char *op_data_prefixes[] = {
	"input",
	"soft_output",
	"hard_output",
	"harq_input",
	"harq_output",
};

/* trim leading and trailing spaces */
static void
trim_space(char *str)
{
	char *start, *end;

	for (start = str; *start; start++) {
		if (!isspace((unsigned char) start[0]))
			break;
	}

	for (end = start + strlen(start); end > start + 1; end--) {
		if (!isspace((unsigned char) end[-1]))
			break;
	}

	*end = 0;

	/* Shift from "start" to the beginning of the string */
	if (start > str)
		memmove(str, start, (end - start) + 1);
}

static bool
starts_with(const char *str, const char *pre)
{
	return strncmp(pre, str, strlen(pre)) == 0;
}

/* tokenization test values separated by a comma */
static int
parse_values(char *tokens, uint32_t **data, uint32_t *data_length)
{
	uint32_t n_tokens = 0;
	uint32_t data_size = 32;

	uint32_t *values, *values_resized;
	char *tok, *error = NULL;

	tok = strtok(tokens, VALUE_DELIMITER);
	if (tok == NULL)
		return -1;

	values = (uint32_t *)
			rte_zmalloc(NULL, sizeof(uint32_t) * data_size, 0);
	if (values == NULL)
		return -1;

	while (tok != NULL) {
		values_resized = NULL;

		if (n_tokens >= data_size) {
			data_size *= 2;

			values_resized = (uint32_t *) rte_realloc(values,
				sizeof(uint32_t) * data_size, 0);
			if (values_resized == NULL) {
				rte_free(values);
				return -1;
			}
			values = values_resized;
		}

		values[n_tokens] = (uint32_t) strtoul(tok, &error, 0);

		if ((error == NULL) || (*error != '\0')) {
			printf("Failed with convert '%s'\n", tok);
			rte_free(values);
			return -1;
		}

		*data_length = *data_length + (strlen(tok) - strlen("0x"))/2;

		tok = strtok(NULL, VALUE_DELIMITER);
		if (tok == NULL)
			break;

		n_tokens++;
	}

	values_resized = (uint32_t *) rte_realloc(values,
		sizeof(uint32_t) * (n_tokens + 1), 0);

	if (values_resized == NULL) {
		rte_free(values);
		return -1;
	}

	*data = values_resized;

	return 0;
}

/* convert turbo decoder flag from string to unsigned long int*/
static int
op_decoder_flag_strtoul(char *token, uint32_t *op_flag_value)
{
	if (!strcmp(token, "RTE_BBDEV_TURBO_SUBBLOCK_DEINTERLEAVE"))
		*op_flag_value = RTE_BBDEV_TURBO_SUBBLOCK_DEINTERLEAVE;
	else if (!strcmp(token, "RTE_BBDEV_TURBO_CRC_TYPE_24B"))
		*op_flag_value = RTE_BBDEV_TURBO_CRC_TYPE_24B;
	else if (!strcmp(token, "RTE_BBDEV_TURBO_EQUALIZER"))
		*op_flag_value = RTE_BBDEV_TURBO_EQUALIZER;
	else if (!strcmp(token, "RTE_BBDEV_TURBO_SOFT_OUT_SATURATE"))
		*op_flag_value = RTE_BBDEV_TURBO_SOFT_OUT_SATURATE;
	else if (!strcmp(token, "RTE_BBDEV_TURBO_HALF_ITERATION_EVEN"))
		*op_flag_value = RTE_BBDEV_TURBO_HALF_ITERATION_EVEN;
	else if (!strcmp(token, "RTE_BBDEV_TURBO_CONTINUE_CRC_MATCH"))
		*op_flag_value = RTE_BBDEV_TURBO_CONTINUE_CRC_MATCH;
	else if (!strcmp(token, "RTE_BBDEV_TURBO_SOFT_OUTPUT"))
		*op_flag_value = RTE_BBDEV_TURBO_SOFT_OUTPUT;
	else if (!strcmp(token, "RTE_BBDEV_TURBO_EARLY_TERMINATION"))
		*op_flag_value = RTE_BBDEV_TURBO_EARLY_TERMINATION;
	else if (!strcmp(token, "RTE_BBDEV_TURBO_POS_LLR_1_BIT_IN"))
		*op_flag_value = RTE_BBDEV_TURBO_POS_LLR_1_BIT_IN;
	else if (!strcmp(token, "RTE_BBDEV_TURBO_NEG_LLR_1_BIT_IN"))
		*op_flag_value = RTE_BBDEV_TURBO_NEG_LLR_1_BIT_IN;
	else if (!strcmp(token, "RTE_BBDEV_TURBO_POS_LLR_1_BIT_SOFT_OUT"))
		*op_flag_value = RTE_BBDEV_TURBO_POS_LLR_1_BIT_SOFT_OUT;
	else if (!strcmp(token, "RTE_BBDEV_TURBO_NEG_LLR_1_BIT_SOFT_OUT"))
		*op_flag_value = RTE_BBDEV_TURBO_NEG_LLR_1_BIT_SOFT_OUT;
	else if (!strcmp(token, "RTE_BBDEV_TURBO_MAP_DEC"))
		*op_flag_value = RTE_BBDEV_TURBO_MAP_DEC;
	else if (!strcmp(token, "RTE_BBDEV_TURBO_DEC_SCATTER_GATHER"))
		*op_flag_value = RTE_BBDEV_TURBO_DEC_SCATTER_GATHER;
	else if (!strcmp(token, "RTE_BBDEV_TURBO_DEC_TB_CRC_24B_KEEP"))
		*op_flag_value = RTE_BBDEV_TURBO_DEC_TB_CRC_24B_KEEP;
	else if (!strcmp(token, "RTE_BBDEV_TURBO_DEC_CRC_24B_DROP"))
		*op_flag_value = RTE_BBDEV_TURBO_DEC_CRC_24B_DROP;
	else {
		printf("The given value is not a turbo decoder flag\n");
		return -1;
	}

	return 0;
}

/* convert LDPC flag from string to unsigned long int*/
static int
op_ldpc_decoder_flag_strtoul(char *token, uint32_t *op_flag_value)
{
	if (!strcmp(token, "RTE_BBDEV_LDPC_CRC_TYPE_24A_CHECK"))
		*op_flag_value = RTE_BBDEV_LDPC_CRC_TYPE_24A_CHECK;
	else if (!strcmp(token, "RTE_BBDEV_LDPC_CRC_TYPE_24B_CHECK"))
		*op_flag_value = RTE_BBDEV_LDPC_CRC_TYPE_24B_CHECK;
	else if (!strcmp(token, "RTE_BBDEV_LDPC_CRC_TYPE_24B_DROP"))
		*op_flag_value = RTE_BBDEV_LDPC_CRC_TYPE_24B_DROP;
	else if (!strcmp(token, "RTE_BBDEV_LDPC_CRC_TYPE_16_CHECK"))
		*op_flag_value = RTE_BBDEV_LDPC_CRC_TYPE_16_CHECK;
	else if (!strcmp(token, "RTE_BBDEV_LDPC_DEINTERLEAVER_BYPASS"))
		*op_flag_value = RTE_BBDEV_LDPC_DEINTERLEAVER_BYPASS;
	else if (!strcmp(token, "RTE_BBDEV_LDPC_HQ_COMBINE_IN_ENABLE"))
		*op_flag_value = RTE_BBDEV_LDPC_HQ_COMBINE_IN_ENABLE;
	else if (!strcmp(token, "RTE_BBDEV_LDPC_HQ_COMBINE_OUT_ENABLE"))
		*op_flag_value = RTE_BBDEV_LDPC_HQ_COMBINE_OUT_ENABLE;
	else if (!strcmp(token, "RTE_BBDEV_LDPC_DECODE_BYPASS"))
		*op_flag_value = RTE_BBDEV_LDPC_DECODE_BYPASS;
	else if (!strcmp(token, "RTE_BBDEV_LDPC_SOFT_OUT_ENABLE"))
		*op_flag_value = RTE_BBDEV_LDPC_SOFT_OUT_ENABLE;
	else if (!strcmp(token, "RTE_BBDEV_LDPC_SOFT_OUT_RM_BYPASS"))
		*op_flag_value = RTE_BBDEV_LDPC_SOFT_OUT_RM_BYPASS;
	else if (!strcmp(token, "RTE_BBDEV_LDPC_SOFT_OUT_DEINTERLEAVER_BYPASS"))
		*op_flag_value = RTE_BBDEV_LDPC_SOFT_OUT_DEINTERLEAVER_BYPASS;
	else if (!strcmp(token, "RTE_BBDEV_LDPC_ITERATION_STOP_ENABLE"))
		*op_flag_value = RTE_BBDEV_LDPC_ITERATION_STOP_ENABLE;
	else if (!strcmp(token, "RTE_BBDEV_LDPC_DEC_INTERRUPTS"))
		*op_flag_value = RTE_BBDEV_LDPC_DEC_INTERRUPTS;
	else if (!strcmp(token, "RTE_BBDEV_LDPC_DEC_SCATTER_GATHER"))
		*op_flag_value = RTE_BBDEV_LDPC_DEC_SCATTER_GATHER;
	else if (!strcmp(token, "RTE_BBDEV_LDPC_HARQ_6BIT_COMPRESSION"))
		*op_flag_value = RTE_BBDEV_LDPC_HARQ_6BIT_COMPRESSION;
	else if (!strcmp(token, "RTE_BBDEV_LDPC_LLR_COMPRESSION"))
		*op_flag_value = RTE_BBDEV_LDPC_LLR_COMPRESSION;
	else if (!strcmp(token,
			"RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_IN_ENABLE"))
		*op_flag_value = RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_IN_ENABLE;
	else if (!strcmp(token,
			"RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_OUT_ENABLE"))
		*op_flag_value = RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_OUT_ENABLE;
	else if (!strcmp(token,
			"RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_LOOPBACK"))
		*op_flag_value = RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_LOOPBACK;
	else {
		printf("The given value is not a LDPC decoder flag\n");
		return -1;
	}

	return 0;
}

/* convert turbo encoder flag from string to unsigned long int*/
static int
op_encoder_flag_strtoul(char *token, uint32_t *op_flag_value)
{
	if (!strcmp(token, "RTE_BBDEV_TURBO_RV_INDEX_BYPASS"))
		*op_flag_value = RTE_BBDEV_TURBO_RV_INDEX_BYPASS;
	else if (!strcmp(token, "RTE_BBDEV_TURBO_RATE_MATCH"))
		*op_flag_value = RTE_BBDEV_TURBO_RATE_MATCH;
	else if (!strcmp(token, "RTE_BBDEV_TURBO_CRC_24B_ATTACH"))
		*op_flag_value = RTE_BBDEV_TURBO_CRC_24B_ATTACH;
	else if (!strcmp(token, "RTE_BBDEV_TURBO_CRC_24A_ATTACH"))
		*op_flag_value = RTE_BBDEV_TURBO_CRC_24A_ATTACH;
	else if (!strcmp(token, "RTE_BBDEV_TURBO_ENC_SCATTER_GATHER"))
		*op_flag_value = RTE_BBDEV_TURBO_ENC_SCATTER_GATHER;
	else {
		printf("The given value is not a turbo encoder flag\n");
		return -1;
	}

	return 0;
}

/* convert LDPC encoder flag from string to unsigned long int*/
static int
op_ldpc_encoder_flag_strtoul(char *token, uint32_t *op_flag_value)
{
	if (!strcmp(token, "RTE_BBDEV_LDPC_INTERLEAVER_BYPASS"))
		*op_flag_value = RTE_BBDEV_LDPC_INTERLEAVER_BYPASS;
	else if (!strcmp(token, "RTE_BBDEV_LDPC_RATE_MATCH"))
		*op_flag_value = RTE_BBDEV_LDPC_RATE_MATCH;
	else if (!strcmp(token, "RTE_BBDEV_LDPC_CRC_24A_ATTACH"))
		*op_flag_value = RTE_BBDEV_LDPC_CRC_24A_ATTACH;
	else if (!strcmp(token, "RTE_BBDEV_LDPC_CRC_24B_ATTACH"))
		*op_flag_value = RTE_BBDEV_LDPC_CRC_24B_ATTACH;
	else if (!strcmp(token, "RTE_BBDEV_LDPC_CRC_16_ATTACH"))
		*op_flag_value = RTE_BBDEV_LDPC_CRC_16_ATTACH;
	else if (!strcmp(token, "RTE_BBDEV_LDPC_ENC_INTERRUPTS"))
		*op_flag_value = RTE_BBDEV_LDPC_ENC_INTERRUPTS;
	else if (!strcmp(token, "RTE_BBDEV_LDPC_ENC_SCATTER_GATHER"))
		*op_flag_value = RTE_BBDEV_LDPC_ENC_SCATTER_GATHER;
	else {
		printf("The given value is not a turbo encoder flag\n");
		return -1;
	}

	return 0;
}

/* tokenization turbo decoder/encoder flags values separated by a comma */
static int
parse_turbo_flags(char *tokens, uint32_t *op_flags,
		enum rte_bbdev_op_type op_type)
{
	char *tok = NULL;
	uint32_t op_flag_value = 0;

	tok = strtok(tokens, VALUE_DELIMITER);
	if (tok == NULL)
		return -1;

	while (tok != NULL) {
		trim_space(tok);
		if (op_type == RTE_BBDEV_OP_TURBO_DEC) {
			if (op_decoder_flag_strtoul(tok, &op_flag_value) == -1)
				return -1;
		} else if (op_type == RTE_BBDEV_OP_TURBO_ENC) {
			if (op_encoder_flag_strtoul(tok, &op_flag_value) == -1)
				return -1;
		} else if (op_type == RTE_BBDEV_OP_LDPC_ENC) {
			if (op_ldpc_encoder_flag_strtoul(tok, &op_flag_value)
					== -1)
				return -1;
		} else if (op_type == RTE_BBDEV_OP_LDPC_DEC) {
			if (op_ldpc_decoder_flag_strtoul(tok, &op_flag_value)
					== -1)
				return -1;
		} else {
			return -1;
		}

		*op_flags = *op_flags | op_flag_value;

		tok = strtok(NULL, VALUE_DELIMITER);
		if (tok == NULL)
			break;
	}

	return 0;
}

/* convert turbo encoder/decoder op_type from string to enum*/
static int
op_turbo_type_strtol(char *token, enum rte_bbdev_op_type *op_type)
{
	trim_space(token);
	if (!strcmp(token, "RTE_BBDEV_OP_TURBO_DEC"))
		*op_type = RTE_BBDEV_OP_TURBO_DEC;
	else if (!strcmp(token, "RTE_BBDEV_OP_TURBO_ENC"))
		*op_type = RTE_BBDEV_OP_TURBO_ENC;
	else if (!strcmp(token, "RTE_BBDEV_OP_LDPC_ENC"))
		*op_type = RTE_BBDEV_OP_LDPC_ENC;
	else if (!strcmp(token, "RTE_BBDEV_OP_LDPC_DEC"))
		*op_type = RTE_BBDEV_OP_LDPC_DEC;
	else if (!strcmp(token, "RTE_BBDEV_OP_NONE"))
		*op_type = RTE_BBDEV_OP_NONE;
	else {
		printf("Not valid turbo op_type: '%s'\n", token);
		return -1;
	}

	return 0;
}

/* tokenization expected status values separated by a comma */
static int
parse_expected_status(char *tokens, int *status, enum rte_bbdev_op_type op_type)
{
	char *tok = NULL;
	bool status_ok = false;

	tok = strtok(tokens, VALUE_DELIMITER);
	if (tok == NULL)
		return -1;

	while (tok != NULL) {
		trim_space(tok);
		if (!strcmp(tok, "OK"))
			status_ok = true;
		else if (!strcmp(tok, "DMA"))
			*status = *status | (1 << RTE_BBDEV_DRV_ERROR);
		else if (!strcmp(tok, "FCW"))
			*status = *status | (1 << RTE_BBDEV_DATA_ERROR);
		else if (!strcmp(tok, "SYNCRC")) {
			*status = *status | (1 << RTE_BBDEV_SYNDROME_ERROR);
			*status = *status | (1 << RTE_BBDEV_CRC_ERROR);
		} else if (!strcmp(tok, "SYN"))
			*status = *status | (1 << RTE_BBDEV_SYNDROME_ERROR);
		else if (!strcmp(tok, "CRC")) {
			if ((op_type == RTE_BBDEV_OP_TURBO_DEC) ||
					(op_type == RTE_BBDEV_OP_LDPC_DEC))
				*status = *status | (1 << RTE_BBDEV_CRC_ERROR);
			else {
				printf(
						"CRC is only a valid value for decoder\n");
				return -1;
			}
		} else {
			printf("Not valid status: '%s'\n", tok);
			return -1;
		}

		tok = strtok(NULL, VALUE_DELIMITER);
		if (tok == NULL)
			break;
	}

	if (status_ok && *status != 0) {
		printf(
				"Not valid status values. Cannot be OK and ERROR at the same time.\n");
		return -1;
	}

	return 0;
}

/* parse ops data entry (there can be more than 1 input entry, each will be
 * contained in a separate op_data_buf struct)
 */
static int
parse_data_entry(const char *key_token, char *token,
		struct test_bbdev_vector *vector, enum op_data_type type,
		const char *prefix)
{
	int ret;
	uint32_t data_length = 0;
	uint32_t *data = NULL;
	unsigned int id;
	struct op_data_buf *op_data;
	unsigned int *nb_ops;

	if (type >= DATA_NUM_TYPES) {
		printf("Unknown op type: %d!\n", type);
		return -1;
	}

	op_data = vector->entries[type].segments;
	nb_ops = &vector->entries[type].nb_segments;

	if (*nb_ops >= RTE_BBDEV_TURBO_MAX_CODE_BLOCKS) {
		printf("Too many segments (code blocks defined): %u, max %d!\n",
				*nb_ops, RTE_BBDEV_TURBO_MAX_CODE_BLOCKS);
		return -1;
	}

	if (sscanf(key_token + strlen(prefix), "%u", &id) != 1) {
		printf("Missing ID of %s\n", prefix);
		return -1;
	}
	if (id != *nb_ops) {
		printf(
			"Please order data entries sequentially, i.e. %s0, %s1, ...\n",
				prefix, prefix);
		return -1;
	}

	/* Clear new op data struct */
	memset(op_data + *nb_ops, 0, sizeof(struct op_data_buf));

	ret = parse_values(token, &data, &data_length);
	if (!ret) {
		op_data[*nb_ops].addr = data;
		op_data[*nb_ops].length = data_length;
		++(*nb_ops);
	}

	return ret;
}

/* parses turbo decoder parameters and assigns to global variable */
static int
parse_decoder_params(const char *key_token, char *token,
		struct test_bbdev_vector *vector)
{
	int ret = 0, status = 0;
	uint32_t op_flags = 0;
	char *err = NULL;

	struct rte_bbdev_op_turbo_dec *turbo_dec = &vector->turbo_dec;

	/* compare keys */
	if (starts_with(key_token, op_data_prefixes[DATA_INPUT]))
		ret = parse_data_entry(key_token, token, vector,
				DATA_INPUT, op_data_prefixes[DATA_INPUT]);

	else if (starts_with(key_token, op_data_prefixes[DATA_SOFT_OUTPUT]))
		ret = parse_data_entry(key_token, token, vector,
				DATA_SOFT_OUTPUT,
				op_data_prefixes[DATA_SOFT_OUTPUT]);

	else if (starts_with(key_token, op_data_prefixes[DATA_HARD_OUTPUT]))
		ret = parse_data_entry(key_token, token, vector,
				DATA_HARD_OUTPUT,
				op_data_prefixes[DATA_HARD_OUTPUT]);
	else if (!strcmp(key_token, "e")) {
		vector->mask |= TEST_BBDEV_VF_E;
		turbo_dec->cb_params.e = (uint32_t) strtoul(token, &err, 0);
	} else if (!strcmp(key_token, "ea")) {
		vector->mask |= TEST_BBDEV_VF_EA;
		turbo_dec->tb_params.ea = (uint32_t) strtoul(token, &err, 0);
		ret = ((err == NULL) || (*err != '\0')) ? -1 : 0;
	} else if (!strcmp(key_token, "eb")) {
		vector->mask |= TEST_BBDEV_VF_EB;
		turbo_dec->tb_params.eb = (uint32_t) strtoul(token, &err, 0);
		ret = ((err == NULL) || (*err != '\0')) ? -1 : 0;
	} else if (!strcmp(key_token, "k")) {
		vector->mask |= TEST_BBDEV_VF_K;
		turbo_dec->cb_params.k = (uint16_t) strtoul(token, &err, 0);
		ret = ((err == NULL) || (*err != '\0')) ? -1 : 0;
	} else if (!strcmp(key_token, "k_pos")) {
		vector->mask |= TEST_BBDEV_VF_K_POS;
		turbo_dec->tb_params.k_pos = (uint16_t) strtoul(token, &err, 0);
		ret = ((err == NULL) || (*err != '\0')) ? -1 : 0;
	} else if (!strcmp(key_token, "k_neg")) {
		vector->mask |= TEST_BBDEV_VF_K_NEG;
		turbo_dec->tb_params.k_neg = (uint16_t) strtoul(token, &err, 0);
		ret = ((err == NULL) || (*err != '\0')) ? -1 : 0;
	} else if (!strcmp(key_token, "c")) {
		vector->mask |= TEST_BBDEV_VF_C;
		turbo_dec->tb_params.c = (uint16_t) strtoul(token, &err, 0);
		ret = ((err == NULL) || (*err != '\0')) ? -1 : 0;
	} else if (!strcmp(key_token, "c_neg")) {
		vector->mask |= TEST_BBDEV_VF_C_NEG;
		turbo_dec->tb_params.c_neg = (uint16_t) strtoul(token, &err, 0);
		ret = ((err == NULL) || (*err != '\0')) ? -1 : 0;
	} else if (!strcmp(key_token, "cab")) {
		vector->mask |= TEST_BBDEV_VF_CAB;
		turbo_dec->tb_params.cab = (uint8_t) strtoul(token, &err, 0);
		ret = ((err == NULL) || (*err != '\0')) ? -1 : 0;
	} else if (!strcmp(key_token, "rv_index")) {
		vector->mask |= TEST_BBDEV_VF_RV_INDEX;
		turbo_dec->rv_index = (uint8_t) strtoul(token, &err, 0);
		ret = ((err == NULL) || (*err != '\0')) ? -1 : 0;
	} else if (!strcmp(key_token, "iter_max")) {
		vector->mask |= TEST_BBDEV_VF_ITER_MAX;
		turbo_dec->iter_max = (uint8_t) strtoul(token, &err, 0);
		ret = ((err == NULL) || (*err != '\0')) ? -1 : 0;
	} else if (!strcmp(key_token, "iter_min")) {
		vector->mask |= TEST_BBDEV_VF_ITER_MIN;
		turbo_dec->iter_min = (uint8_t) strtoul(token, &err, 0);
		ret = ((err == NULL) || (*err != '\0')) ? -1 : 0;
	} else if (!strcmp(key_token, "expected_iter_count")) {
		vector->mask |= TEST_BBDEV_VF_EXPECTED_ITER_COUNT;
		turbo_dec->iter_count = (uint8_t) strtoul(token, &err, 0);
		ret = ((err == NULL) || (*err != '\0')) ? -1 : 0;
	} else if (!strcmp(key_token, "ext_scale")) {
		vector->mask |= TEST_BBDEV_VF_EXT_SCALE;
		turbo_dec->ext_scale = (uint8_t) strtoul(token, &err, 0);
		ret = ((err == NULL) || (*err != '\0')) ? -1 : 0;
	} else if (!strcmp(key_token, "num_maps")) {
		vector->mask |= TEST_BBDEV_VF_NUM_MAPS;
		turbo_dec->num_maps = (uint8_t) strtoul(token, &err, 0);
		ret = ((err == NULL) || (*err != '\0')) ? -1 : 0;
	} else if (!strcmp(key_token, "r")) {
		vector->mask |= TEST_BBDEV_VF_R;
		turbo_dec->tb_params.r = (uint8_t)strtoul(token, &err, 0);
		ret = ((err == NULL) || (*err != '\0')) ? -1 : 0;
	} else if (!strcmp(key_token, "code_block_mode")) {
		vector->mask |= TEST_BBDEV_VF_CODE_BLOCK_MODE;
		turbo_dec->code_block_mode = (uint8_t) strtoul(token, &err, 0);
		ret = ((err == NULL) || (*err != '\0')) ? -1 : 0;
	} else if (!strcmp(key_token, "op_flags")) {
		vector->mask |= TEST_BBDEV_VF_OP_FLAGS;
		ret = parse_turbo_flags(token, &op_flags,
			vector->op_type);
		if (!ret)
			turbo_dec->op_flags = op_flags;
	} else if (!strcmp(key_token, "expected_status")) {
		vector->mask |= TEST_BBDEV_VF_EXPECTED_STATUS;
		ret = parse_expected_status(token, &status, vector->op_type);
		if (!ret)
			vector->expected_status = status;
	} else {
		printf("Not valid dec key: '%s'\n", key_token);
		return -1;
	}

	if (ret != 0) {
		printf("Failed with convert '%s\t%s'\n", key_token, token);
		return -1;
	}

	return 0;
}

/* parses turbo encoder parameters and assigns to global variable */
static int
parse_encoder_params(const char *key_token, char *token,
		struct test_bbdev_vector *vector)
{
	int ret = 0, status = 0;
	uint32_t op_flags = 0;
	char *err = NULL;


	struct rte_bbdev_op_turbo_enc *turbo_enc = &vector->turbo_enc;

	if (starts_with(key_token, op_data_prefixes[DATA_INPUT]))
		ret = parse_data_entry(key_token, token, vector,
				DATA_INPUT, op_data_prefixes[DATA_INPUT]);
	else if (starts_with(key_token, "output"))
		ret = parse_data_entry(key_token, token, vector,
				DATA_HARD_OUTPUT, "output");
	else if (!strcmp(key_token, "e")) {
		vector->mask |= TEST_BBDEV_VF_E;
		turbo_enc->cb_params.e = (uint32_t) strtoul(token, &err, 0);
		ret = ((err == NULL) || (*err != '\0')) ? -1 : 0;
	} else if (!strcmp(key_token, "ea")) {
		vector->mask |= TEST_BBDEV_VF_EA;
		turbo_enc->tb_params.ea = (uint32_t) strtoul(token, &err, 0);
		ret = ((err == NULL) || (*err != '\0')) ? -1 : 0;
	} else if (!strcmp(key_token, "eb")) {
		vector->mask |= TEST_BBDEV_VF_EB;
		turbo_enc->tb_params.eb = (uint32_t) strtoul(token, &err, 0);
		ret = ((err == NULL) || (*err != '\0')) ? -1 : 0;
	} else if (!strcmp(key_token, "k")) {
		vector->mask |= TEST_BBDEV_VF_K;
		turbo_enc->cb_params.k = (uint16_t) strtoul(token, &err, 0);
		ret = ((err == NULL) || (*err != '\0')) ? -1 : 0;
	} else if (!strcmp(key_token, "k_neg")) {
		vector->mask |= TEST_BBDEV_VF_K_NEG;
		turbo_enc->tb_params.k_neg = (uint16_t) strtoul(token, &err, 0);
		ret = ((err == NULL) || (*err != '\0')) ? -1 : 0;
	} else if (!strcmp(key_token, "k_pos")) {
		vector->mask |= TEST_BBDEV_VF_K_POS;
		turbo_enc->tb_params.k_pos = (uint16_t) strtoul(token, &err, 0);
		ret = ((err == NULL) || (*err != '\0')) ? -1 : 0;
	} else if (!strcmp(key_token, "c_neg")) {
		vector->mask |= TEST_BBDEV_VF_C_NEG;
		turbo_enc->tb_params.c_neg = (uint8_t) strtoul(token, &err, 0);
		ret = ((err == NULL) || (*err != '\0')) ? -1 : 0;
	} else if (!strcmp(key_token, "c")) {
		vector->mask |= TEST_BBDEV_VF_C;
		turbo_enc->tb_params.c = (uint8_t) strtoul(token, &err, 0);
		ret = ((err == NULL) || (*err != '\0')) ? -1 : 0;
	} else if (!strcmp(key_token, "cab")) {
		vector->mask |= TEST_BBDEV_VF_CAB;
		turbo_enc->tb_params.cab = (uint8_t) strtoul(token, &err, 0);
		ret = ((err == NULL) || (*err != '\0')) ? -1 : 0;
	} else if (!strcmp(key_token, "rv_index")) {
		vector->mask |= TEST_BBDEV_VF_RV_INDEX;
		turbo_enc->rv_index = (uint8_t) strtoul(token, &err, 0);
		ret = ((err == NULL) || (*err != '\0')) ? -1 : 0;
	} else if (!strcmp(key_token, "ncb")) {
		vector->mask |= TEST_BBDEV_VF_NCB;
		turbo_enc->cb_params.ncb = (uint16_t) strtoul(token, &err, 0);
		ret = ((err == NULL) || (*err != '\0')) ? -1 : 0;
	} else if (!strcmp(key_token, "ncb_neg")) {
		vector->mask |= TEST_BBDEV_VF_NCB_NEG;
		turbo_enc->tb_params.ncb_neg =
				(uint16_t) strtoul(token, &err, 0);
		ret = ((err == NULL) || (*err != '\0')) ? -1 : 0;
	} else if (!strcmp(key_token, "ncb_pos")) {
		vector->mask |= TEST_BBDEV_VF_NCB_POS;
		turbo_enc->tb_params.ncb_pos =
				(uint16_t) strtoul(token, &err, 0);
		ret = ((err == NULL) || (*err != '\0')) ? -1 : 0;
	} else if (!strcmp(key_token, "r")) {
		vector->mask |= TEST_BBDEV_VF_R;
		turbo_enc->tb_params.r = (uint8_t) strtoul(token, &err, 0);
		ret = ((err == NULL) || (*err != '\0')) ? -1 : 0;
	} else if (!strcmp(key_token, "code_block_mode")) {
		vector->mask |= TEST_BBDEV_VF_CODE_BLOCK_MODE;
		turbo_enc->code_block_mode = (uint8_t) strtoul(token, &err, 0);
		ret = ((err == NULL) || (*err != '\0')) ? -1 : 0;
	} else if (!strcmp(key_token, "op_flags")) {
		vector->mask |= TEST_BBDEV_VF_OP_FLAGS;
		ret = parse_turbo_flags(token, &op_flags,
				vector->op_type);
		if (!ret)
			turbo_enc->op_flags = op_flags;
	} else if (!strcmp(key_token, "expected_status")) {
		vector->mask |= TEST_BBDEV_VF_EXPECTED_STATUS;
		ret = parse_expected_status(token, &status, vector->op_type);
		if (!ret)
			vector->expected_status = status;
	} else {
		printf("Not valid enc key: '%s'\n", key_token);
		return -1;
	}

	if (ret != 0) {
		printf("Failed with convert '%s\t%s'\n", key_token, token);
		return -1;
	}

	return 0;
}


/* parses LDPC encoder parameters and assigns to global variable */
static int
parse_ldpc_encoder_params(const char *key_token, char *token,
		struct test_bbdev_vector *vector)
{
	int ret = 0, status = 0;
	uint32_t op_flags = 0;
	char *err = NULL;

	struct rte_bbdev_op_ldpc_enc *ldpc_enc = &vector->ldpc_enc;

	if (starts_with(key_token, op_data_prefixes[DATA_INPUT]))
		ret = parse_data_entry(key_token, token, vector,
				DATA_INPUT,
				op_data_prefixes[DATA_INPUT]);
	else if (starts_with(key_token, "output"))
		ret = parse_data_entry(key_token, token, vector,
				DATA_HARD_OUTPUT,
				"output");
	else if (!strcmp(key_token, "e")) {
		vector->mask |= TEST_BBDEV_VF_E;
		ldpc_enc->cb_params.e = (uint32_t) strtoul(token, &err, 0);
		ret = ((err == NULL) || (*err != '\0')) ? -1 : 0;
	} else if (!strcmp(key_token, "ea")) {
		vector->mask |= TEST_BBDEV_VF_EA;
		ldpc_enc->tb_params.ea = (uint32_t) strtoul(token, &err, 0);
		ret = ((err == NULL) || (*err != '\0')) ? -1 : 0;
	} else if (!strcmp(key_token, "eb")) {
		vector->mask |= TEST_BBDEV_VF_EB;
		ldpc_enc->tb_params.eb = (uint32_t) strtoul(token, &err, 0);
		ret = ((err == NULL) || (*err != '\0')) ? -1 : 0;
	} else if (!strcmp(key_token, "c")) {
		vector->mask |= TEST_BBDEV_VF_C;
		ldpc_enc->tb_params.c = (uint8_t) strtoul(token, &err, 0);
		ret = ((err == NULL) || (*err != '\0')) ? -1 : 0;
	} else if (!strcmp(key_token, "cab")) {
		vector->mask |= TEST_BBDEV_VF_CAB;
		ldpc_enc->tb_params.cab = (uint8_t) strtoul(token, &err, 0);
		ret = ((err == NULL) || (*err != '\0')) ? -1 : 0;
	} else if (!strcmp(key_token, "rv_index")) {
		vector->mask |= TEST_BBDEV_VF_RV_INDEX;
		ldpc_enc->rv_index = (uint8_t) strtoul(token, &err, 0);
		ret = ((err == NULL) || (*err != '\0')) ? -1 : 0;
	} else if (!strcmp(key_token, "n_cb")) {
		vector->mask |= TEST_BBDEV_VF_NCB;
		ldpc_enc->n_cb = (uint16_t) strtoul(token, &err, 0);
		ret = ((err == NULL) || (*err != '\0')) ? -1 : 0;
	} else if (!strcmp(key_token, "r")) {
		vector->mask |= TEST_BBDEV_VF_R;
		ldpc_enc->tb_params.r = (uint8_t) strtoul(token, &err, 0);
		ret = ((err == NULL) || (*err != '\0')) ? -1 : 0;
	} else if (!strcmp(key_token, "q_m")) {
		vector->mask |= TEST_BBDEV_VF_QM;
		ldpc_enc->q_m = (uint8_t) strtoul(token, &err, 0);
		ret = ((err == NULL) || (*err != '\0')) ? -1 : 0;
	} else if (!strcmp(key_token, "basegraph")) {
		vector->mask |= TEST_BBDEV_VF_BG;
		ldpc_enc->basegraph = (uint8_t) strtoul(token, &err, 0);
		ret = ((err == NULL) || (*err != '\0')) ? -1 : 0;
	} else if (!strcmp(key_token, "z_c")) {
		vector->mask |= TEST_BBDEV_VF_ZC;
		ldpc_enc->z_c = (uint16_t) strtoul(token, &err, 0);
		ret = ((err == NULL) || (*err != '\0')) ? -1 : 0;
	} else if (!strcmp(key_token, "n_filler")) {
		vector->mask |= TEST_BBDEV_VF_F;
		ldpc_enc->n_filler = (uint16_t) strtoul(token, &err, 0);
		ret = ((err == NULL) || (*err != '\0')) ? -1 : 0;
	} else if (!strcmp(key_token, "code_block_mode")) {
		vector->mask |= TEST_BBDEV_VF_CODE_BLOCK_MODE;
		ldpc_enc->code_block_mode = (uint8_t) strtoul(token, &err, 0);
		ret = ((err == NULL) || (*err != '\0')) ? -1 : 0;
	} else if (!strcmp(key_token, "op_flags")) {
		vector->mask |= TEST_BBDEV_VF_OP_FLAGS;
		ret = parse_turbo_flags(token, &op_flags, vector->op_type);
		if (!ret)
			ldpc_enc->op_flags = op_flags;
	} else if (!strcmp(key_token, "expected_status")) {
		vector->mask |= TEST_BBDEV_VF_EXPECTED_STATUS;
		ret = parse_expected_status(token, &status, vector->op_type);
		if (!ret)
			vector->expected_status = status;
	} else {
		printf("Not valid ldpc enc key: '%s'\n", key_token);
		return -1;
	}

	if (ret != 0) {
		printf("Failed with convert '%s\t%s'\n", key_token, token);
		return -1;
	}

	return 0;
}

/* parses LDPC decoder parameters and assigns to global variable */
static int
parse_ldpc_decoder_params(const char *key_token, char *token,
		struct test_bbdev_vector *vector)
{
	int ret = 0, status = 0;
	uint32_t op_flags = 0;
	char *err = NULL;

	struct rte_bbdev_op_ldpc_dec *ldpc_dec = &vector->ldpc_dec;

	if (starts_with(key_token, op_data_prefixes[DATA_INPUT]))
		ret = parse_data_entry(key_token, token, vector,
				DATA_INPUT,
				op_data_prefixes[DATA_INPUT]);
	else if (starts_with(key_token, "output"))
		ret = parse_data_entry(key_token, token, vector,
				DATA_HARD_OUTPUT,
				"output");
	else if (starts_with(key_token, op_data_prefixes[DATA_HARQ_INPUT]))
		ret = parse_data_entry(key_token, token, vector,
				DATA_HARQ_INPUT,
				op_data_prefixes[DATA_HARQ_INPUT]);
	else if (starts_with(key_token, op_data_prefixes[DATA_HARQ_OUTPUT]))
		ret = parse_data_entry(key_token, token, vector,
				DATA_HARQ_OUTPUT,
				op_data_prefixes[DATA_HARQ_OUTPUT]);
	else if (!strcmp(key_token, "e")) {
		vector->mask |= TEST_BBDEV_VF_E;
		ldpc_dec->cb_params.e = (uint32_t) strtoul(token, &err, 0);
		ret = ((err == NULL) || (*err != '\0')) ? -1 : 0;
	} else if (!strcmp(key_token, "ea")) {
		vector->mask |= TEST_BBDEV_VF_EA;
		ldpc_dec->tb_params.ea = (uint32_t) strtoul(token, &err, 0);
		ret = ((err == NULL) || (*err != '\0')) ? -1 : 0;
	} else if (!strcmp(key_token, "eb")) {
		vector->mask |= TEST_BBDEV_VF_EB;
		ldpc_dec->tb_params.eb = (uint32_t) strtoul(token, &err, 0);
		ret = ((err == NULL) || (*err != '\0')) ? -1 : 0;
	} else if (!strcmp(key_token, "c")) {
		vector->mask |= TEST_BBDEV_VF_C;
		ldpc_dec->tb_params.c = (uint8_t) strtoul(token, &err, 0);
		ret = ((err == NULL) || (*err != '\0')) ? -1 : 0;
	} else if (!strcmp(key_token, "cab")) {
		vector->mask |= TEST_BBDEV_VF_CAB;
		ldpc_dec->tb_params.cab = (uint8_t) strtoul(token, &err, 0);
		ret = ((err == NULL) || (*err != '\0')) ? -1 : 0;
	} else if (!strcmp(key_token, "rv_index")) {
		vector->mask |= TEST_BBDEV_VF_RV_INDEX;
		ldpc_dec->rv_index = (uint8_t) strtoul(token, &err, 0);
		ret = ((err == NULL) || (*err != '\0')) ? -1 : 0;
	} else if (!strcmp(key_token, "n_cb")) {
		vector->mask |= TEST_BBDEV_VF_NCB;
		ldpc_dec->n_cb = (uint16_t) strtoul(token, &err, 0);
		ret = ((err == NULL) || (*err != '\0')) ? -1 : 0;
	} else if (!strcmp(key_token, "r")) {
		vector->mask |= TEST_BBDEV_VF_R;
		ldpc_dec->tb_params.r = (uint8_t) strtoul(token, &err, 0);
		ret = ((err == NULL) || (*err != '\0')) ? -1 : 0;
	} else if (!strcmp(key_token, "q_m")) {
		vector->mask |= TEST_BBDEV_VF_QM;
		ldpc_dec->q_m = (uint8_t) strtoul(token, &err, 0);
		ret = ((err == NULL) || (*err != '\0')) ? -1 : 0;
	} else if (!strcmp(key_token, "basegraph")) {
		vector->mask |= TEST_BBDEV_VF_BG;
		ldpc_dec->basegraph = (uint8_t) strtoul(token, &err, 0);
		ret = ((err == NULL) || (*err != '\0')) ? -1 : 0;
	} else if (!strcmp(key_token, "z_c")) {
		vector->mask |= TEST_BBDEV_VF_ZC;
		ldpc_dec->z_c = (uint16_t) strtoul(token, &err, 0);
		ret = ((err == NULL) || (*err != '\0')) ? -1 : 0;
	} else if (!strcmp(key_token, "n_filler")) {
		vector->mask |= TEST_BBDEV_VF_F;
		ldpc_dec->n_filler = (uint16_t) strtoul(token, &err, 0);
		ret = ((err == NULL) || (*err != '\0')) ? -1 : 0;
	} else if (!strcmp(key_token, "expected_iter_count")) {
		vector->mask |= TEST_BBDEV_VF_EXPECTED_ITER_COUNT;
		ldpc_dec->iter_count = (uint8_t) strtoul(token, &err, 0);
		ret = ((err == NULL) || (*err != '\0')) ? -1 : 0;
	} else if (!strcmp(key_token, "iter_max")) {
		vector->mask |= TEST_BBDEV_VF_ITER_MAX;
		ldpc_dec->iter_max = (uint8_t) strtoul(token, &err, 0);
		ret = ((err == NULL) || (*err != '\0')) ? -1 : 0;
	} else if (!strcmp(key_token, "code_block_mode")) {
		vector->mask |= TEST_BBDEV_VF_CODE_BLOCK_MODE;
		ldpc_dec->code_block_mode = (uint8_t) strtoul(token, &err, 0);
		ret = ((err == NULL) || (*err != '\0')) ? -1 : 0;
	} else if (!strcmp(key_token, "op_flags")) {
		vector->mask |= TEST_BBDEV_VF_OP_FLAGS;
		ret = parse_turbo_flags(token, &op_flags, vector->op_type);
		if (!ret)
			ldpc_dec->op_flags = op_flags;
	} else if (!strcmp(key_token, "expected_status")) {
		vector->mask |= TEST_BBDEV_VF_EXPECTED_STATUS;
		ret = parse_expected_status(token, &status, vector->op_type);
		if (!ret)
			vector->expected_status = status;
	} else {
		printf("Not valid ldpc dec key: '%s'\n", key_token);
		return -1;
	}

	if (ret != 0) {
		printf("Failed with convert '%s\t%s'\n", key_token, token);
		return -1;
	}

	return 0;
}

/* checks the type of key and assigns data */
static int
parse_entry(char *entry, struct test_bbdev_vector *vector)
{
	int ret = 0;
	char *token, *key_token;
	enum rte_bbdev_op_type op_type = RTE_BBDEV_OP_NONE;

	if (entry == NULL) {
		printf("Expected entry value\n");
		return -1;
	}

	/* get key */
	token = strtok(entry, ENTRY_DELIMITER);
	key_token = token;
	/* get values for key */
	token = strtok(NULL, ENTRY_DELIMITER);

	if (key_token == NULL || token == NULL) {
		printf("Expected 'key = values' but was '%.40s'..\n", entry);
		return -1;
	}
	trim_space(key_token);

	/* first key_token has to specify type of operation */
	if (vector->op_type == RTE_BBDEV_OP_NONE) {
		if (!strcmp(key_token, "op_type")) {
			ret = op_turbo_type_strtol(token, &op_type);
			if (!ret)
				vector->op_type = op_type;
			return (!ret) ? 0 : -1;
		}
		printf("First key_token (%s) does not specify op_type\n",
				key_token);
		return -1;
	}

	/* compare keys */
	if (vector->op_type == RTE_BBDEV_OP_TURBO_DEC) {
		if (parse_decoder_params(key_token, token, vector) == -1)
			return -1;
	} else if (vector->op_type == RTE_BBDEV_OP_TURBO_ENC) {
		if (parse_encoder_params(key_token, token, vector) == -1)
			return -1;
	} else if (vector->op_type == RTE_BBDEV_OP_LDPC_ENC) {
		if (parse_ldpc_encoder_params(key_token, token, vector) == -1)
			return -1;
	} else if (vector->op_type == RTE_BBDEV_OP_LDPC_DEC) {
		if (parse_ldpc_decoder_params(key_token, token, vector) == -1)
			return -1;
	}

	return 0;
}

static int
check_decoder_segments(struct test_bbdev_vector *vector)
{
	unsigned char i;
	struct rte_bbdev_op_turbo_dec *turbo_dec = &vector->turbo_dec;

	if (vector->entries[DATA_INPUT].nb_segments == 0)
		return -1;

	for (i = 0; i < vector->entries[DATA_INPUT].nb_segments; i++)
		if (vector->entries[DATA_INPUT].segments[i].addr == NULL)
			return -1;

	if (vector->entries[DATA_HARD_OUTPUT].nb_segments == 0)
		return -1;

	for (i = 0; i < vector->entries[DATA_HARD_OUTPUT].nb_segments;
			i++)
		if (vector->entries[DATA_HARD_OUTPUT].segments[i].addr == NULL)
			return -1;

	if ((turbo_dec->op_flags & RTE_BBDEV_TURBO_SOFT_OUTPUT) &&
			(vector->entries[DATA_SOFT_OUTPUT].nb_segments == 0))
		return -1;

	for (i = 0; i < vector->entries[DATA_SOFT_OUTPUT].nb_segments;
			i++)
		if (vector->entries[DATA_SOFT_OUTPUT].segments[i].addr == NULL)
			return -1;

	return 0;
}

static int
check_ldpc_decoder_segments(struct test_bbdev_vector *vector)
{
	unsigned char i;
	struct rte_bbdev_op_ldpc_dec *ldpc_dec = &vector->ldpc_dec;

	for (i = 0; i < vector->entries[DATA_INPUT].nb_segments; i++)
		if (vector->entries[DATA_INPUT].segments[i].addr == NULL)
			return -1;

	for (i = 0; i < vector->entries[DATA_HARD_OUTPUT].nb_segments; i++)
		if (vector->entries[DATA_HARD_OUTPUT].segments[i].addr == NULL)
			return -1;

	if ((ldpc_dec->op_flags & RTE_BBDEV_LDPC_SOFT_OUT_ENABLE) &&
			(vector->entries[DATA_SOFT_OUTPUT].nb_segments == 0))
		return -1;

	for (i = 0; i < vector->entries[DATA_SOFT_OUTPUT].nb_segments; i++)
		if (vector->entries[DATA_SOFT_OUTPUT].segments[i].addr == NULL)
			return -1;

	if ((ldpc_dec->op_flags & RTE_BBDEV_LDPC_HQ_COMBINE_OUT_ENABLE) &&
			(vector->entries[DATA_HARQ_OUTPUT].nb_segments == 0))
		return -1;

	for (i = 0; i < vector->entries[DATA_HARQ_OUTPUT].nb_segments; i++)
		if (vector->entries[DATA_HARQ_OUTPUT].segments[i].addr == NULL)
			return -1;

	return 0;
}

static int
check_decoder_llr_spec(struct test_bbdev_vector *vector)
{
	struct rte_bbdev_op_turbo_dec *turbo_dec = &vector->turbo_dec;

	/* Check input LLR sign formalism specification */
	if ((turbo_dec->op_flags & RTE_BBDEV_TURBO_POS_LLR_1_BIT_IN) &&
			(turbo_dec->op_flags &
			RTE_BBDEV_TURBO_NEG_LLR_1_BIT_IN)) {
		printf(
			"Both positive and negative LLR input flags were set!\n");
		return -1;
	}
	if (!(turbo_dec->op_flags & RTE_BBDEV_TURBO_POS_LLR_1_BIT_IN) &&
			!(turbo_dec->op_flags &
			RTE_BBDEV_TURBO_NEG_LLR_1_BIT_IN)) {
		printf(
			"INFO: input LLR sign formalism was not specified and will be set to negative LLR for '1' bit\n");
		turbo_dec->op_flags |= RTE_BBDEV_TURBO_NEG_LLR_1_BIT_IN;
	}

	if (!(turbo_dec->op_flags & RTE_BBDEV_TURBO_SOFT_OUTPUT))
		return 0;

	/* Check output LLR sign formalism specification */
	if ((turbo_dec->op_flags & RTE_BBDEV_TURBO_POS_LLR_1_BIT_SOFT_OUT) &&
			(turbo_dec->op_flags &
			RTE_BBDEV_TURBO_NEG_LLR_1_BIT_SOFT_OUT)) {
		printf(
			"Both positive and negative LLR output flags were set!\n");
		return -1;
	}
	if (!(turbo_dec->op_flags & RTE_BBDEV_TURBO_POS_LLR_1_BIT_SOFT_OUT) &&
			!(turbo_dec->op_flags &
			RTE_BBDEV_TURBO_NEG_LLR_1_BIT_SOFT_OUT)) {
		printf(
			"INFO: soft output LLR sign formalism was not specified and will be set to negative LLR for '1' bit\n");
		turbo_dec->op_flags |=
				RTE_BBDEV_TURBO_NEG_LLR_1_BIT_SOFT_OUT;
	}

	return 0;
}

static int
check_decoder_op_flags(struct test_bbdev_vector *vector)
{
	struct rte_bbdev_op_turbo_dec *turbo_dec = &vector->turbo_dec;

	if ((turbo_dec->op_flags & RTE_BBDEV_TURBO_DEC_TB_CRC_24B_KEEP) &&
		!(turbo_dec->op_flags & RTE_BBDEV_TURBO_CRC_TYPE_24B)) {
		printf(
			"WARNING: RTE_BBDEV_TURBO_DEC_TB_CRC_24B_KEEP flag is missing RTE_BBDEV_TURBO_CRC_TYPE_24B\n");
		return -1;
	}

	return 0;
}

/* checks decoder parameters */
static int
check_decoder(struct test_bbdev_vector *vector)
{
	struct rte_bbdev_op_turbo_dec *turbo_dec = &vector->turbo_dec;
	const int mask = vector->mask;

	if (check_decoder_segments(vector) < 0)
		return -1;

	if (check_decoder_llr_spec(vector) < 0)
		return -1;

	if (check_decoder_op_flags(vector) < 0)
		return -1;

	/* Check which params were set */
	if (!(mask & TEST_BBDEV_VF_CODE_BLOCK_MODE)) {
		printf(
			"WARNING: code_block_mode was not specified in vector file and will be set to 1 (0 - TB Mode, 1 - CB mode)\n");
		turbo_dec->code_block_mode = RTE_BBDEV_CODE_BLOCK;
	}
	if (turbo_dec->code_block_mode == RTE_BBDEV_TRANSPORT_BLOCK) {
		if (!(mask & TEST_BBDEV_VF_EA))
			printf(
				"WARNING: ea was not specified in vector file and will be set to 0\n");
		if (!(mask & TEST_BBDEV_VF_EB))
			printf(
				"WARNING: eb was not specified in vector file and will be set to 0\n");
		if (!(mask & TEST_BBDEV_VF_K_NEG))
			printf(
				"WARNING: k_neg was not specified in vector file and will be set to 0\n");
		if (!(mask & TEST_BBDEV_VF_K_POS))
			printf(
				"WARNING: k_pos was not specified in vector file and will be set to 0\n");
		if (!(mask & TEST_BBDEV_VF_C_NEG))
			printf(
				"WARNING: c_neg was not specified in vector file and will be set to 0\n");
		if (!(mask & TEST_BBDEV_VF_C)) {
			printf(
				"WARNING: c was not specified in vector file and will be set to 1\n");
			turbo_dec->tb_params.c = 1;
		}
		if (!(mask & TEST_BBDEV_VF_CAB))
			printf(
				"WARNING: cab was not specified in vector file and will be set to 0\n");
		if (!(mask & TEST_BBDEV_VF_R))
			printf(
				"WARNING: r was not specified in vector file and will be set to 0\n");
	} else {
		if (!(mask & TEST_BBDEV_VF_E))
			printf(
				"WARNING: e was not specified in vector file and will be set to 0\n");
		if (!(mask & TEST_BBDEV_VF_K))
			printf(
				"WARNING: k was not specified in vector file and will be set to 0\n");
	}
	if (!(mask & TEST_BBDEV_VF_RV_INDEX))
		printf(
			"INFO: rv_index was not specified in vector file and will be set to 0\n");
	if (!(mask & TEST_BBDEV_VF_ITER_MIN))
		printf(
			"WARNING: iter_min was not specified in vector file and will be set to 0\n");
	if (!(mask & TEST_BBDEV_VF_ITER_MAX))
		printf(
			"WARNING: iter_max was not specified in vector file and will be set to 0\n");
	if (!(mask & TEST_BBDEV_VF_EXPECTED_ITER_COUNT))
		printf(
			"WARNING: expected_iter_count was not specified in vector file and iter_count will not be validated\n");
	if (!(mask & TEST_BBDEV_VF_EXT_SCALE))
		printf(
			"WARNING: ext_scale was not specified in vector file and will be set to 0\n");
	if (!(mask & TEST_BBDEV_VF_OP_FLAGS)) {
		printf(
			"WARNING: op_flags was not specified in vector file and capabilities will not be validated\n");
		turbo_dec->num_maps = 0;
	} else if (!(turbo_dec->op_flags & RTE_BBDEV_TURBO_MAP_DEC) &&
			mask & TEST_BBDEV_VF_NUM_MAPS) {
		printf(
			"INFO: RTE_BBDEV_TURBO_MAP_DEC was not set in vector file and num_maps will be set to 0\n");
		turbo_dec->num_maps = 0;
	}
	if (!(mask & TEST_BBDEV_VF_EXPECTED_STATUS))
		printf(
			"WARNING: expected_status was not specified in vector file and will be set to 0\n");
	return 0;
}

/* checks LDPC decoder parameters */
static int
check_ldpc_decoder(struct test_bbdev_vector *vector)
{
	struct rte_bbdev_op_ldpc_dec *ldpc_dec = &vector->ldpc_dec;
	const int mask = vector->mask;

	if (check_ldpc_decoder_segments(vector) < 0)
		return -1;

	/*
	 * if (check_ldpc_decoder_llr_spec(vector) < 0)
	 *	return -1;
	 *
	 * if (check_ldpc_decoder_op_flags(vector) < 0)
	 *	return -1;
	 */

	/* Check which params were set */
	if (!(mask & TEST_BBDEV_VF_CODE_BLOCK_MODE)) {
		printf(
			"WARNING: code_block_mode was not specified in vector file and will be set to 1 (0 - TB Mode, 1 - CB mode)\n");
		ldpc_dec->code_block_mode = RTE_BBDEV_CODE_BLOCK;
	}
	if (ldpc_dec->code_block_mode == RTE_BBDEV_TRANSPORT_BLOCK) {
		if (!(mask & TEST_BBDEV_VF_EA))
			printf(
				"WARNING: ea was not specified in vector file and will be set to 0\n");
		if (!(mask & TEST_BBDEV_VF_EB))
			printf(
				"WARNING: eb was not specified in vector file and will be set to 0\n");
		if (!(mask & TEST_BBDEV_VF_C)) {
			printf(
				"WARNING: c was not specified in vector file and will be set to 1\n");
			ldpc_dec->tb_params.c = 1;
		}
		if (!(mask & TEST_BBDEV_VF_CAB))
			printf(
				"WARNING: cab was not specified in vector file and will be set to 0\n");
		if (!(mask & TEST_BBDEV_VF_R))
			printf(
				"WARNING: r was not specified in vector file and will be set to 0\n");
	} else {
		if (!(mask & TEST_BBDEV_VF_E))
			printf(
				"WARNING: e was not specified in vector file and will be set to 0\n");
	}
	if (!(mask & TEST_BBDEV_VF_RV_INDEX))
		printf(
			"INFO: rv_index was not specified in vector file and will be set to 0\n");
	if (!(mask & TEST_BBDEV_VF_ITER_MAX))
		printf(
			"WARNING: iter_max was not specified in vector file and will be set to 0\n");
	if (!(mask & TEST_BBDEV_VF_EXPECTED_ITER_COUNT))
		printf(
			"WARNING: expected_iter_count was not specified in vector file and iter_count will not be validated\n");
	if (!(mask & TEST_BBDEV_VF_OP_FLAGS)) {
		printf(
			"WARNING: op_flags was not specified in vector file and capabilities will not be validated\n");
	}
	if (!(mask & TEST_BBDEV_VF_EXPECTED_STATUS))
		printf(
			"WARNING: expected_status was not specified in vector file and will be set to 0\n");
	return 0;
}

/* checks encoder parameters */
static int
check_encoder(struct test_bbdev_vector *vector)
{
	unsigned char i;
	const int mask = vector->mask;

	if (vector->entries[DATA_INPUT].nb_segments == 0)
		return -1;

	for (i = 0; i < vector->entries[DATA_INPUT].nb_segments; i++)
		if (vector->entries[DATA_INPUT].segments[i].addr == NULL)
			return -1;

	if (vector->entries[DATA_HARD_OUTPUT].nb_segments == 0)
		return -1;

	for (i = 0; i < vector->entries[DATA_HARD_OUTPUT].nb_segments; i++)
		if (vector->entries[DATA_HARD_OUTPUT].segments[i].addr == NULL)
			return -1;

	if (!(mask & TEST_BBDEV_VF_CODE_BLOCK_MODE)) {
		printf(
			"WARNING: code_block_mode was not specified in vector file and will be set to 1\n");
		vector->turbo_enc.code_block_mode = RTE_BBDEV_CODE_BLOCK;
	}
	if (vector->turbo_enc.code_block_mode == RTE_BBDEV_TRANSPORT_BLOCK) {
		if (!(mask & TEST_BBDEV_VF_EA) && (vector->turbo_enc.op_flags &
				RTE_BBDEV_TURBO_RATE_MATCH))
			printf(
				"WARNING: ea was not specified in vector file and will be set to 0\n");
		if (!(mask & TEST_BBDEV_VF_EB) && (vector->turbo_enc.op_flags &
				RTE_BBDEV_TURBO_RATE_MATCH))
			printf(
				"WARNING: eb was not specified in vector file and will be set to 0\n");
		if (!(mask & TEST_BBDEV_VF_K_NEG))
			printf(
				"WARNING: k_neg was not specified in vector file and will be set to 0\n");
		if (!(mask & TEST_BBDEV_VF_K_POS))
			printf(
				"WARNING: k_pos was not specified in vector file and will be set to 0\n");
		if (!(mask & TEST_BBDEV_VF_C_NEG))
			printf(
				"WARNING: c_neg was not specified in vector file and will be set to 0\n");
		if (!(mask & TEST_BBDEV_VF_C)) {
			printf(
				"WARNING: c was not specified in vector file and will be set to 1\n");
			vector->turbo_enc.tb_params.c = 1;
		}
		if (!(mask & TEST_BBDEV_VF_CAB) && (vector->turbo_enc.op_flags &
				RTE_BBDEV_TURBO_RATE_MATCH))
			printf(
				"WARNING: cab was not specified in vector file and will be set to 0\n");
		if (!(mask & TEST_BBDEV_VF_NCB_NEG))
			printf(
				"WARNING: ncb_neg was not specified in vector file and will be set to 0\n");
		if (!(mask & TEST_BBDEV_VF_NCB_POS))
			printf(
				"WARNING: ncb_pos was not specified in vector file and will be set to 0\n");
		if (!(mask & TEST_BBDEV_VF_R))
			printf(
				"WARNING: r was not specified in vector file and will be set to 0\n");
	} else {
		if (!(mask & TEST_BBDEV_VF_E) && (vector->turbo_enc.op_flags &
				RTE_BBDEV_TURBO_RATE_MATCH))
			printf(
				"WARNING: e was not specified in vector file and will be set to 0\n");
		if (!(mask & TEST_BBDEV_VF_K))
			printf(
				"WARNING: k was not specified in vector file and will be set to 0\n");
		if (!(mask & TEST_BBDEV_VF_NCB))
			printf(
				"WARNING: ncb was not specified in vector file and will be set to 0\n");
	}
	if (!(mask & TEST_BBDEV_VF_RV_INDEX))
		printf(
			"INFO: rv_index was not specified in vector file and will be set to 0\n");
	if (!(mask & TEST_BBDEV_VF_OP_FLAGS))
		printf(
			"INFO: op_flags was not specified in vector file and capabilities will not be validated\n");
	if (!(mask & TEST_BBDEV_VF_EXPECTED_STATUS))
		printf(
			"WARNING: expected_status was not specified in vector file and will be set to 0\n");

	return 0;
}


/* checks encoder parameters */
static int
check_ldpc_encoder(struct test_bbdev_vector *vector)
{
	unsigned char i;
	const int mask = vector->mask;

	if (vector->entries[DATA_INPUT].nb_segments == 0)
		return -1;

	for (i = 0; i < vector->entries[DATA_INPUT].nb_segments; i++)
		if (vector->entries[DATA_INPUT].segments[i].addr == NULL)
			return -1;

	if (vector->entries[DATA_HARD_OUTPUT].nb_segments == 0)
		return -1;

	for (i = 0; i < vector->entries[DATA_HARD_OUTPUT].nb_segments; i++)
		if (vector->entries[DATA_HARD_OUTPUT].segments[i].addr == NULL)
			return -1;

	if (!(mask & TEST_BBDEV_VF_CODE_BLOCK_MODE)) {
		printf(
			"WARNING: code_block_mode was not specified in vector file and will be set to 1\n");
		vector->turbo_enc.code_block_mode = RTE_BBDEV_CODE_BLOCK;
	}
	if (vector->turbo_enc.code_block_mode == RTE_BBDEV_TRANSPORT_BLOCK) {
	} else {
		if (!(mask & TEST_BBDEV_VF_E) && (vector->turbo_enc.op_flags &
				RTE_BBDEV_TURBO_RATE_MATCH))
			printf(
				"WARNING: e was not specified in vector file and will be set to 0\n");
		if (!(mask & TEST_BBDEV_VF_NCB))
			printf(
				"WARNING: ncb was not specified in vector file and will be set to 0\n");
	}
	if (!(mask & TEST_BBDEV_VF_BG))
		printf(
			"WARNING: BG was not specified in vector file and will be set to 0\n");
	if (!(mask & TEST_BBDEV_VF_ZC))
		printf(
			"WARNING: Zc was not specified in vector file and will be set to 0\n");
	if (!(mask & TEST_BBDEV_VF_RV_INDEX))
		printf(
			"INFO: rv_index was not specified in vector file and will be set to 0\n");
	if (!(mask & TEST_BBDEV_VF_OP_FLAGS))
		printf(
			"INFO: op_flags was not specified in vector file and capabilities will not be validated\n");
	if (!(mask & TEST_BBDEV_VF_EXPECTED_STATUS))
		printf(
			"WARNING: expected_status was not specified in vector file and will be set to 0\n");

	return 0;
}

static int
bbdev_check_vector(struct test_bbdev_vector *vector)
{
	if (vector->op_type == RTE_BBDEV_OP_TURBO_DEC) {
		if (check_decoder(vector) == -1)
			return -1;
	} else if (vector->op_type == RTE_BBDEV_OP_TURBO_ENC) {
		if (check_encoder(vector) == -1)
			return -1;
	} else if (vector->op_type == RTE_BBDEV_OP_LDPC_ENC) {
		if (check_ldpc_encoder(vector) == -1)
			return -1;
	} else if (vector->op_type == RTE_BBDEV_OP_LDPC_DEC) {
		if (check_ldpc_decoder(vector) == -1)
			return -1;
	} else if (vector->op_type != RTE_BBDEV_OP_NONE) {
		printf("Vector was not filled\n");
		return -1;
	}

	return 0;
}

int
test_bbdev_vector_read(const char *filename,
		struct test_bbdev_vector *vector)
{
	int ret = 0;
	size_t len = 0;

	FILE *fp = NULL;
	char *line = NULL;
	char *entry = NULL;

	fp = fopen(filename, "r");
	if (fp == NULL) {
		printf("File %s does not exist\n", filename);
		return -1;
	}

	while (getline(&line, &len, fp) != -1) {

		/* ignore comments and new lines */
		if (line[0] == '#' || line[0] == '/' || line[0] == '\n'
			|| line[0] == '\r')
			continue;

		trim_space(line);

		/* buffer for multiline */
		entry = realloc(entry, strlen(line) + 1);
		if (entry == NULL) {
			printf("Fail to realloc %zu bytes\n", strlen(line) + 1);
			ret = -ENOMEM;
			goto exit;
		}

		strcpy(entry, line);

		/* check if entry ends with , or = */
		if (entry[strlen(entry) - 1] == ','
			|| entry[strlen(entry) - 1] == '=') {
			while (getline(&line, &len, fp) != -1) {
				trim_space(line);

				/* extend entry about length of new line */
				char *entry_extended = realloc(entry,
						strlen(line) +
						strlen(entry) + 1);

				if (entry_extended == NULL) {
					printf("Fail to allocate %zu bytes\n",
							strlen(line) +
							strlen(entry) + 1);
					ret = -ENOMEM;
					goto exit;
				}

				entry = entry_extended;
				/* entry has been allocated accordingly */
				strcpy(&entry[strlen(entry)], line);

				if (entry[strlen(entry) - 1] != ',')
					break;
			}
		}
		ret = parse_entry(entry, vector);
		if (ret != 0) {
			printf("An error occurred while parsing!\n");
			goto exit;
		}
	}
	ret = bbdev_check_vector(vector);
	if (ret != 0)
		printf("An error occurred while checking!\n");

exit:
	fclose(fp);
	free(line);
	free(entry);

	return ret;
}
