/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef _NT_CLOCK_PROFILES_STRUCTS_H_
#define _NT_CLOCK_PROFILES_STRUCTS_H_

#include <stdint.h>

#define clk_profile_size_error_msg "Size test failed"

struct clk_profile_data_fmt1_s {
	uint16_t reg_addr;
	uint8_t reg_val;
};

struct clk_profile_data_fmt2_s {
	unsigned int reg_addr;
	unsigned char reg_val;
};

typedef struct clk_profile_data_fmt1_s clk_profile_data_fmt1_t;
typedef struct clk_profile_data_fmt2_s clk_profile_data_fmt2_t;

enum clk_profile_data_fmt_e {
	clk_profile_data_fmt_1,
	clk_profile_data_fmt_2,
};

typedef enum clk_profile_data_fmt_e clk_profile_data_fmt_t;

#endif	/* _NT_CLOCK_PROFILES_STRUCTS_H_ */
