/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef I2C_NIM_H_
#define I2C_NIM_H_

#include "ntnic_nim.h"

typedef struct sfp_nim_state {
	uint8_t br;	/* bit rate, units of 100 MBits/sec */
} sfp_nim_state_t, *sfp_nim_state_p;

/*
 * Builds an nim state for the port implied by `ctx`, returns zero
 * if successful, and non-zero otherwise. SFP and QSFP nims are supported
 */
int nthw_nim_state_build(nim_i2c_ctx_t *ctx, sfp_nim_state_t *state);

/*
 * Returns a type name such as "SFP/SFP+" for a given NIM type identifier,
 * or the string "ILLEGAL!".
 */
const char *nthw_nim_id_to_text(uint8_t nim_id);

int nthw_nim_qsfp_plus_nim_set_tx_laser_disable(nim_i2c_ctx_t *ctx, bool disable, int lane_idx);

/*
 * This function tries to classify NIM based on it's ID and some register reads
 * and collects information into ctx structure. The @extra parameter could contain
 * the initialization argument for specific type of NIMS.
 */
int nthw_construct_and_preinit_nim(nim_i2c_ctx_p ctx, void *extra);

void nthw_qsfp28_set_high_power(nim_i2c_ctx_p ctx);
bool nthw_qsfp28_set_fec_enable(nim_i2c_ctx_p ctx, bool media_side_fec, bool host_side_fec);

void nthw_nim_agx_setup(struct nim_i2c_ctx *ctx, nthw_pcal6416a_t *p_io_nim, nthw_i2cm_t *p_nt_i2cm,
	nthw_pca9849_t *p_ca9849);

#endif	/* I2C_NIM_H_ */
