/*-
 * This file is provided under a dual BSD/GPLv2 license. When using or
 * redistributing this file, you may do so under either license.
 *
 *   BSD LICENSE
 *
 * Copyright 2008-2016 Freescale Semiconductor Inc.
 * Copyright 2016 NXP.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * * Neither the name of the above-listed copyright holders nor the
 * names of any contributors may be used to endorse or promote products
 * derived from this software without specific prior written permission.
 *
 *   GPL LICENSE SUMMARY
 *
 * ALTERNATIVELY, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") as published by the Free Software
 * Foundation, either version 2 of that License or (at your option) any
 * later version.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __RTA_OPERATION_CMD_H__
#define __RTA_OPERATION_CMD_H__

extern enum rta_sec_era rta_sec_era;

static inline int
__rta_alg_aai_aes(uint16_t aai)
{
	uint16_t aes_mode = aai & OP_ALG_AESA_MODE_MASK;

	if (aai & OP_ALG_AAI_C2K) {
		if (rta_sec_era < RTA_SEC_ERA_5)
			return -1;
		if ((aes_mode != OP_ALG_AAI_CCM) &&
		    (aes_mode != OP_ALG_AAI_GCM))
			return -EINVAL;
	}

	switch (aes_mode) {
	case OP_ALG_AAI_CBC_CMAC:
	case OP_ALG_AAI_CTR_CMAC_LTE:
	case OP_ALG_AAI_CTR_CMAC:
		if (rta_sec_era < RTA_SEC_ERA_2)
			return -EINVAL;
		/* no break */
	case OP_ALG_AAI_CTR:
	case OP_ALG_AAI_CBC:
	case OP_ALG_AAI_ECB:
	case OP_ALG_AAI_OFB:
	case OP_ALG_AAI_CFB:
	case OP_ALG_AAI_XTS:
	case OP_ALG_AAI_CMAC:
	case OP_ALG_AAI_XCBC_MAC:
	case OP_ALG_AAI_CCM:
	case OP_ALG_AAI_GCM:
	case OP_ALG_AAI_CBC_XCBCMAC:
	case OP_ALG_AAI_CTR_XCBCMAC:
		return 0;
	}

	return -EINVAL;
}

static inline int
__rta_alg_aai_des(uint16_t aai)
{
	uint16_t aai_code = (uint16_t)(aai & ~OP_ALG_AAI_CHECKODD);

	switch (aai_code) {
	case OP_ALG_AAI_CBC:
	case OP_ALG_AAI_ECB:
	case OP_ALG_AAI_CFB:
	case OP_ALG_AAI_OFB:
		return 0;
	}

	return -EINVAL;
}

static inline int
__rta_alg_aai_md5(uint16_t aai)
{
	switch (aai) {
	case OP_ALG_AAI_HMAC:
		if (rta_sec_era < RTA_SEC_ERA_2)
			return -EINVAL;
		/* no break */
	case OP_ALG_AAI_SMAC:
	case OP_ALG_AAI_HASH:
	case OP_ALG_AAI_HMAC_PRECOMP:
		return 0;
	}

	return -EINVAL;
}

static inline int
__rta_alg_aai_sha(uint16_t aai)
{
	switch (aai) {
	case OP_ALG_AAI_HMAC:
		if (rta_sec_era < RTA_SEC_ERA_2)
			return -EINVAL;
		/* no break */
	case OP_ALG_AAI_HASH:
	case OP_ALG_AAI_HMAC_PRECOMP:
		return 0;
	}

	return -EINVAL;
}

static inline int
__rta_alg_aai_rng(uint16_t aai)
{
	uint16_t rng_mode = aai & OP_ALG_RNG_MODE_MASK;
	uint16_t rng_sh = aai & OP_ALG_AAI_RNG4_SH_MASK;

	switch (rng_mode) {
	case OP_ALG_AAI_RNG:
	case OP_ALG_AAI_RNG_NZB:
	case OP_ALG_AAI_RNG_OBP:
		break;
	default:
		return -EINVAL;
	}

	/* State Handle bits are valid only for SEC Era >= 5 */
	if ((rta_sec_era < RTA_SEC_ERA_5) && rng_sh)
		return -EINVAL;

	/* PS, AI, SK bits are also valid only for SEC Era >= 5 */
	if ((rta_sec_era < RTA_SEC_ERA_5) && (aai &
	     (OP_ALG_AAI_RNG4_PS | OP_ALG_AAI_RNG4_AI | OP_ALG_AAI_RNG4_SK)))
		return -EINVAL;

	switch (rng_sh) {
	case OP_ALG_AAI_RNG4_SH_0:
	case OP_ALG_AAI_RNG4_SH_1:
		return 0;
	}

	return -EINVAL;
}

static inline int
__rta_alg_aai_crc(uint16_t aai)
{
	uint16_t aai_code = aai & OP_ALG_CRC_POLY_MASK;

	switch (aai_code) {
	case OP_ALG_AAI_802:
	case OP_ALG_AAI_3385:
	case OP_ALG_AAI_CUST_POLY:
		return 0;
	}

	return -EINVAL;
}

static inline int
__rta_alg_aai_kasumi(uint16_t aai)
{
	switch (aai) {
	case OP_ALG_AAI_GSM:
	case OP_ALG_AAI_EDGE:
	case OP_ALG_AAI_F8:
	case OP_ALG_AAI_F9:
		return 0;
	}

	return -EINVAL;
}

static inline int
__rta_alg_aai_snow_f9(uint16_t aai)
{
	if (aai == OP_ALG_AAI_F9)
		return 0;

	return -EINVAL;
}

static inline int
__rta_alg_aai_snow_f8(uint16_t aai)
{
	if (aai == OP_ALG_AAI_F8)
		return 0;

	return -EINVAL;
}

static inline int
__rta_alg_aai_zuce(uint16_t aai)
{
	if (aai == OP_ALG_AAI_F8)
		return 0;

	return -EINVAL;
}

static inline int
__rta_alg_aai_zuca(uint16_t aai)
{
	if (aai == OP_ALG_AAI_F9)
		return 0;

	return -EINVAL;
}

struct alg_aai_map {
	uint32_t chipher_algo;
	int (*aai_func)(uint16_t);
	uint32_t class;
};

static const struct alg_aai_map alg_table[] = {
/*1*/	{ OP_ALG_ALGSEL_AES,      __rta_alg_aai_aes,    OP_TYPE_CLASS1_ALG },
	{ OP_ALG_ALGSEL_DES,      __rta_alg_aai_des,    OP_TYPE_CLASS1_ALG },
	{ OP_ALG_ALGSEL_3DES,     __rta_alg_aai_des,    OP_TYPE_CLASS1_ALG },
	{ OP_ALG_ALGSEL_MD5,      __rta_alg_aai_md5,    OP_TYPE_CLASS2_ALG },
	{ OP_ALG_ALGSEL_SHA1,     __rta_alg_aai_md5,    OP_TYPE_CLASS2_ALG },
	{ OP_ALG_ALGSEL_SHA224,   __rta_alg_aai_sha,    OP_TYPE_CLASS2_ALG },
	{ OP_ALG_ALGSEL_SHA256,   __rta_alg_aai_sha,    OP_TYPE_CLASS2_ALG },
	{ OP_ALG_ALGSEL_SHA384,   __rta_alg_aai_sha,    OP_TYPE_CLASS2_ALG },
	{ OP_ALG_ALGSEL_SHA512,   __rta_alg_aai_sha,    OP_TYPE_CLASS2_ALG },
	{ OP_ALG_ALGSEL_RNG,      __rta_alg_aai_rng,    OP_TYPE_CLASS1_ALG },
/*11*/	{ OP_ALG_ALGSEL_CRC,      __rta_alg_aai_crc,    OP_TYPE_CLASS2_ALG },
	{ OP_ALG_ALGSEL_ARC4,     NULL,                 OP_TYPE_CLASS1_ALG },
	{ OP_ALG_ALGSEL_SNOW_F8,  __rta_alg_aai_snow_f8, OP_TYPE_CLASS1_ALG },
/*14*/	{ OP_ALG_ALGSEL_KASUMI,   __rta_alg_aai_kasumi, OP_TYPE_CLASS1_ALG },
	{ OP_ALG_ALGSEL_SNOW_F9,  __rta_alg_aai_snow_f9, OP_TYPE_CLASS2_ALG },
	{ OP_ALG_ALGSEL_ZUCE,     __rta_alg_aai_zuce,   OP_TYPE_CLASS1_ALG },
/*17*/	{ OP_ALG_ALGSEL_ZUCA,     __rta_alg_aai_zuca,   OP_TYPE_CLASS2_ALG }
};

/*
 * Allowed OPERATION algorithms for each SEC Era.
 * Values represent the number of entries from alg_table[] that are supported.
 */
static const unsigned int alg_table_sz[] = {14, 15, 15, 15, 17, 17, 11, 17};

static inline int
rta_operation(struct program *program, uint32_t cipher_algo,
	      uint16_t aai, uint8_t algo_state,
	      int icv_checking, int enc)
{
	uint32_t opcode = CMD_OPERATION;
	unsigned int i, found = 0;
	unsigned int start_pc = program->current_pc;
	int ret;

	for (i = 0; i < alg_table_sz[rta_sec_era]; i++) {
		if (alg_table[i].chipher_algo == cipher_algo) {
			opcode |= cipher_algo | alg_table[i].class;
			/* nothing else to verify */
			if (alg_table[i].aai_func == NULL) {
				found = 1;
				break;
			}

			aai &= OP_ALG_AAI_MASK;

			ret = (*alg_table[i].aai_func)(aai);
			if (ret < 0) {
				pr_err("OPERATION: Bad AAI Type. SEC Program Line: %d\n",
				       program->current_pc);
				goto err;
			}
			opcode |= aai;
			found = 1;
			break;
		}
	}
	if (!found) {
		pr_err("OPERATION: Invalid Command. SEC Program Line: %d\n",
		       program->current_pc);
		ret = -EINVAL;
		goto err;
	}

	switch (algo_state) {
	case OP_ALG_AS_UPDATE:
	case OP_ALG_AS_INIT:
	case OP_ALG_AS_FINALIZE:
	case OP_ALG_AS_INITFINAL:
		opcode |= algo_state;
		break;
	default:
		pr_err("Invalid Operation Command\n");
		ret = -EINVAL;
		goto err;
	}

	switch (icv_checking) {
	case ICV_CHECK_DISABLE:
		/*
		 * opcode |= OP_ALG_ICV_OFF;
		 * OP_ALG_ICV_OFF is 0
		 */
		break;
	case ICV_CHECK_ENABLE:
		opcode |= OP_ALG_ICV_ON;
		break;
	default:
		pr_err("Invalid Operation Command\n");
		ret = -EINVAL;
		goto err;
	}

	switch (enc) {
	case DIR_DEC:
		/*
		 * opcode |= OP_ALG_DECRYPT;
		 * OP_ALG_DECRYPT is 0
		 */
		break;
	case DIR_ENC:
		opcode |= OP_ALG_ENCRYPT;
		break;
	default:
		pr_err("Invalid Operation Command\n");
		ret = -EINVAL;
		goto err;
	}

	__rta_out32(program, opcode);
	program->current_instruction++;
	return (int)start_pc;

 err:
	program->first_error_pc = start_pc;
	return ret;
}

/*
 * OPERATION PKHA routines
 */
static inline int
__rta_pkha_clearmem(uint32_t pkha_op)
{
	switch (pkha_op) {
	case (OP_ALG_PKMODE_CLEARMEM_ALL):
	case (OP_ALG_PKMODE_CLEARMEM_ABE):
	case (OP_ALG_PKMODE_CLEARMEM_ABN):
	case (OP_ALG_PKMODE_CLEARMEM_AB):
	case (OP_ALG_PKMODE_CLEARMEM_AEN):
	case (OP_ALG_PKMODE_CLEARMEM_AE):
	case (OP_ALG_PKMODE_CLEARMEM_AN):
	case (OP_ALG_PKMODE_CLEARMEM_A):
	case (OP_ALG_PKMODE_CLEARMEM_BEN):
	case (OP_ALG_PKMODE_CLEARMEM_BE):
	case (OP_ALG_PKMODE_CLEARMEM_BN):
	case (OP_ALG_PKMODE_CLEARMEM_B):
	case (OP_ALG_PKMODE_CLEARMEM_EN):
	case (OP_ALG_PKMODE_CLEARMEM_N):
	case (OP_ALG_PKMODE_CLEARMEM_E):
		return 0;
	}

	return -EINVAL;
}

static inline int
__rta_pkha_mod_arithmetic(uint32_t pkha_op)
{
	pkha_op &= (uint32_t)~OP_ALG_PKMODE_OUT_A;

	switch (pkha_op) {
	case (OP_ALG_PKMODE_MOD_ADD):
	case (OP_ALG_PKMODE_MOD_SUB_AB):
	case (OP_ALG_PKMODE_MOD_SUB_BA):
	case (OP_ALG_PKMODE_MOD_MULT):
	case (OP_ALG_PKMODE_MOD_MULT_IM):
	case (OP_ALG_PKMODE_MOD_MULT_IM_OM):
	case (OP_ALG_PKMODE_MOD_EXPO):
	case (OP_ALG_PKMODE_MOD_EXPO_TEQ):
	case (OP_ALG_PKMODE_MOD_EXPO_IM):
	case (OP_ALG_PKMODE_MOD_EXPO_IM_TEQ):
	case (OP_ALG_PKMODE_MOD_REDUCT):
	case (OP_ALG_PKMODE_MOD_INV):
	case (OP_ALG_PKMODE_MOD_MONT_CNST):
	case (OP_ALG_PKMODE_MOD_CRT_CNST):
	case (OP_ALG_PKMODE_MOD_GCD):
	case (OP_ALG_PKMODE_MOD_PRIMALITY):
	case (OP_ALG_PKMODE_MOD_SML_EXP):
	case (OP_ALG_PKMODE_F2M_ADD):
	case (OP_ALG_PKMODE_F2M_MUL):
	case (OP_ALG_PKMODE_F2M_MUL_IM):
	case (OP_ALG_PKMODE_F2M_MUL_IM_OM):
	case (OP_ALG_PKMODE_F2M_EXP):
	case (OP_ALG_PKMODE_F2M_EXP_TEQ):
	case (OP_ALG_PKMODE_F2M_AMODN):
	case (OP_ALG_PKMODE_F2M_INV):
	case (OP_ALG_PKMODE_F2M_R2):
	case (OP_ALG_PKMODE_F2M_GCD):
	case (OP_ALG_PKMODE_F2M_SML_EXP):
	case (OP_ALG_PKMODE_ECC_F2M_ADD):
	case (OP_ALG_PKMODE_ECC_F2M_ADD_IM_OM_PROJ):
	case (OP_ALG_PKMODE_ECC_F2M_DBL):
	case (OP_ALG_PKMODE_ECC_F2M_DBL_IM_OM_PROJ):
	case (OP_ALG_PKMODE_ECC_F2M_MUL):
	case (OP_ALG_PKMODE_ECC_F2M_MUL_TEQ):
	case (OP_ALG_PKMODE_ECC_F2M_MUL_R2):
	case (OP_ALG_PKMODE_ECC_F2M_MUL_R2_TEQ):
	case (OP_ALG_PKMODE_ECC_F2M_MUL_R2_PROJ):
	case (OP_ALG_PKMODE_ECC_F2M_MUL_R2_PROJ_TEQ):
	case (OP_ALG_PKMODE_ECC_MOD_ADD):
	case (OP_ALG_PKMODE_ECC_MOD_ADD_IM_OM_PROJ):
	case (OP_ALG_PKMODE_ECC_MOD_DBL):
	case (OP_ALG_PKMODE_ECC_MOD_DBL_IM_OM_PROJ):
	case (OP_ALG_PKMODE_ECC_MOD_MUL):
	case (OP_ALG_PKMODE_ECC_MOD_MUL_TEQ):
	case (OP_ALG_PKMODE_ECC_MOD_MUL_R2):
	case (OP_ALG_PKMODE_ECC_MOD_MUL_R2_TEQ):
	case (OP_ALG_PKMODE_ECC_MOD_MUL_R2_PROJ):
	case (OP_ALG_PKMODE_ECC_MOD_MUL_R2_PROJ_TEQ):
		return 0;
	}

	return -EINVAL;
}

static inline int
__rta_pkha_copymem(uint32_t pkha_op)
{
	switch (pkha_op) {
	case (OP_ALG_PKMODE_COPY_NSZ_A0_B0):
	case (OP_ALG_PKMODE_COPY_NSZ_A0_B1):
	case (OP_ALG_PKMODE_COPY_NSZ_A0_B2):
	case (OP_ALG_PKMODE_COPY_NSZ_A0_B3):
	case (OP_ALG_PKMODE_COPY_NSZ_A1_B0):
	case (OP_ALG_PKMODE_COPY_NSZ_A1_B1):
	case (OP_ALG_PKMODE_COPY_NSZ_A1_B2):
	case (OP_ALG_PKMODE_COPY_NSZ_A1_B3):
	case (OP_ALG_PKMODE_COPY_NSZ_A2_B0):
	case (OP_ALG_PKMODE_COPY_NSZ_A2_B1):
	case (OP_ALG_PKMODE_COPY_NSZ_A2_B2):
	case (OP_ALG_PKMODE_COPY_NSZ_A2_B3):
	case (OP_ALG_PKMODE_COPY_NSZ_A3_B0):
	case (OP_ALG_PKMODE_COPY_NSZ_A3_B1):
	case (OP_ALG_PKMODE_COPY_NSZ_A3_B2):
	case (OP_ALG_PKMODE_COPY_NSZ_A3_B3):
	case (OP_ALG_PKMODE_COPY_NSZ_B0_A0):
	case (OP_ALG_PKMODE_COPY_NSZ_B0_A1):
	case (OP_ALG_PKMODE_COPY_NSZ_B0_A2):
	case (OP_ALG_PKMODE_COPY_NSZ_B0_A3):
	case (OP_ALG_PKMODE_COPY_NSZ_B1_A0):
	case (OP_ALG_PKMODE_COPY_NSZ_B1_A1):
	case (OP_ALG_PKMODE_COPY_NSZ_B1_A2):
	case (OP_ALG_PKMODE_COPY_NSZ_B1_A3):
	case (OP_ALG_PKMODE_COPY_NSZ_B2_A0):
	case (OP_ALG_PKMODE_COPY_NSZ_B2_A1):
	case (OP_ALG_PKMODE_COPY_NSZ_B2_A2):
	case (OP_ALG_PKMODE_COPY_NSZ_B2_A3):
	case (OP_ALG_PKMODE_COPY_NSZ_B3_A0):
	case (OP_ALG_PKMODE_COPY_NSZ_B3_A1):
	case (OP_ALG_PKMODE_COPY_NSZ_B3_A2):
	case (OP_ALG_PKMODE_COPY_NSZ_B3_A3):
	case (OP_ALG_PKMODE_COPY_NSZ_A_E):
	case (OP_ALG_PKMODE_COPY_NSZ_A_N):
	case (OP_ALG_PKMODE_COPY_NSZ_B_E):
	case (OP_ALG_PKMODE_COPY_NSZ_B_N):
	case (OP_ALG_PKMODE_COPY_NSZ_N_A):
	case (OP_ALG_PKMODE_COPY_NSZ_N_B):
	case (OP_ALG_PKMODE_COPY_NSZ_N_E):
	case (OP_ALG_PKMODE_COPY_SSZ_A0_B0):
	case (OP_ALG_PKMODE_COPY_SSZ_A0_B1):
	case (OP_ALG_PKMODE_COPY_SSZ_A0_B2):
	case (OP_ALG_PKMODE_COPY_SSZ_A0_B3):
	case (OP_ALG_PKMODE_COPY_SSZ_A1_B0):
	case (OP_ALG_PKMODE_COPY_SSZ_A1_B1):
	case (OP_ALG_PKMODE_COPY_SSZ_A1_B2):
	case (OP_ALG_PKMODE_COPY_SSZ_A1_B3):
	case (OP_ALG_PKMODE_COPY_SSZ_A2_B0):
	case (OP_ALG_PKMODE_COPY_SSZ_A2_B1):
	case (OP_ALG_PKMODE_COPY_SSZ_A2_B2):
	case (OP_ALG_PKMODE_COPY_SSZ_A2_B3):
	case (OP_ALG_PKMODE_COPY_SSZ_A3_B0):
	case (OP_ALG_PKMODE_COPY_SSZ_A3_B1):
	case (OP_ALG_PKMODE_COPY_SSZ_A3_B2):
	case (OP_ALG_PKMODE_COPY_SSZ_A3_B3):
	case (OP_ALG_PKMODE_COPY_SSZ_B0_A0):
	case (OP_ALG_PKMODE_COPY_SSZ_B0_A1):
	case (OP_ALG_PKMODE_COPY_SSZ_B0_A2):
	case (OP_ALG_PKMODE_COPY_SSZ_B0_A3):
	case (OP_ALG_PKMODE_COPY_SSZ_B1_A0):
	case (OP_ALG_PKMODE_COPY_SSZ_B1_A1):
	case (OP_ALG_PKMODE_COPY_SSZ_B1_A2):
	case (OP_ALG_PKMODE_COPY_SSZ_B1_A3):
	case (OP_ALG_PKMODE_COPY_SSZ_B2_A0):
	case (OP_ALG_PKMODE_COPY_SSZ_B2_A1):
	case (OP_ALG_PKMODE_COPY_SSZ_B2_A2):
	case (OP_ALG_PKMODE_COPY_SSZ_B2_A3):
	case (OP_ALG_PKMODE_COPY_SSZ_B3_A0):
	case (OP_ALG_PKMODE_COPY_SSZ_B3_A1):
	case (OP_ALG_PKMODE_COPY_SSZ_B3_A2):
	case (OP_ALG_PKMODE_COPY_SSZ_B3_A3):
	case (OP_ALG_PKMODE_COPY_SSZ_A_E):
	case (OP_ALG_PKMODE_COPY_SSZ_A_N):
	case (OP_ALG_PKMODE_COPY_SSZ_B_E):
	case (OP_ALG_PKMODE_COPY_SSZ_B_N):
	case (OP_ALG_PKMODE_COPY_SSZ_N_A):
	case (OP_ALG_PKMODE_COPY_SSZ_N_B):
	case (OP_ALG_PKMODE_COPY_SSZ_N_E):
		return 0;
	}

	return -EINVAL;
}

static inline int
rta_pkha_operation(struct program *program, uint32_t op_pkha)
{
	uint32_t opcode = CMD_OPERATION | OP_TYPE_PK | OP_ALG_PK;
	uint32_t pkha_func;
	unsigned int start_pc = program->current_pc;
	int ret = -EINVAL;

	pkha_func = op_pkha & OP_ALG_PK_FUN_MASK;

	switch (pkha_func) {
	case (OP_ALG_PKMODE_CLEARMEM):
		ret = __rta_pkha_clearmem(op_pkha);
		if (ret < 0) {
			pr_err("OPERATION PKHA: Type not supported. SEC Program Line: %d\n",
			       program->current_pc);
			goto err;
		}
		break;
	case (OP_ALG_PKMODE_MOD_ADD):
	case (OP_ALG_PKMODE_MOD_SUB_AB):
	case (OP_ALG_PKMODE_MOD_SUB_BA):
	case (OP_ALG_PKMODE_MOD_MULT):
	case (OP_ALG_PKMODE_MOD_EXPO):
	case (OP_ALG_PKMODE_MOD_REDUCT):
	case (OP_ALG_PKMODE_MOD_INV):
	case (OP_ALG_PKMODE_MOD_MONT_CNST):
	case (OP_ALG_PKMODE_MOD_CRT_CNST):
	case (OP_ALG_PKMODE_MOD_GCD):
	case (OP_ALG_PKMODE_MOD_PRIMALITY):
	case (OP_ALG_PKMODE_MOD_SML_EXP):
	case (OP_ALG_PKMODE_ECC_MOD_ADD):
	case (OP_ALG_PKMODE_ECC_MOD_DBL):
	case (OP_ALG_PKMODE_ECC_MOD_MUL):
		ret = __rta_pkha_mod_arithmetic(op_pkha);
		if (ret < 0) {
			pr_err("OPERATION PKHA: Type not supported. SEC Program Line: %d\n",
			       program->current_pc);
			goto err;
		}
		break;
	case (OP_ALG_PKMODE_COPY_NSZ):
	case (OP_ALG_PKMODE_COPY_SSZ):
		ret = __rta_pkha_copymem(op_pkha);
		if (ret < 0) {
			pr_err("OPERATION PKHA: Type not supported. SEC Program Line: %d\n",
			       program->current_pc);
			goto err;
		}
		break;
	default:
		pr_err("Invalid Operation Command\n");
		goto err;
	}

	opcode |= op_pkha;

	__rta_out32(program, opcode);
	program->current_instruction++;
	return (int)start_pc;

 err:
	program->first_error_pc = start_pc;
	program->current_instruction++;
	return ret;
}

#endif /* __RTA_OPERATION_CMD_H__ */
