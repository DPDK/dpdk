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
int nim_state_build(nim_i2c_ctx_t *ctx, sfp_nim_state_t *state);

/*
 * Returns a type name such as "SFP/SFP+" for a given NIM type identifier,
 * or the string "ILLEGAL!".
 */
const char *nim_id_to_text(uint8_t nim_id);

int nim_qsfp_plus_nim_set_tx_laser_disable(nim_i2c_ctx_t *ctx, bool disable, int lane_idx);

/*
 * This function tries to classify NIM based on it's ID and some register reads
 * and collects information into ctx structure. The @extra parameter could contain
 * the initialization argument for specific type of NIMS.
 */
int construct_and_preinit_nim(nim_i2c_ctx_p ctx, void *extra);

#endif	/* I2C_NIM_H_ */
