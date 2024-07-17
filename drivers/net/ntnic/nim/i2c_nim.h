/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef I2C_NIM_H_
#define I2C_NIM_H_

#include "ntnic_nim.h"

/*
 * Returns a type name such as "SFP/SFP+" for a given NIM type identifier,
 * or the string "ILLEGAL!".
 */
const char *nim_id_to_text(uint8_t nim_id);

/*
 * This function tries to classify NIM based on it's ID and some register reads
 * and collects information into ctx structure. The @extra parameter could contain
 * the initialization argument for specific type of NIMS.
 */
int construct_and_preinit_nim(nim_i2c_ctx_p ctx);

#endif	/* I2C_NIM_H_ */
