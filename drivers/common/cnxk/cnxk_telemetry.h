/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 Marvell.
 */

#ifndef __CNXK_TELEMETRY_H_
#define __CNXK_TELEMETRY_H_

#define CNXK_TEL_STR(s)		  #s
#define CNXK_TEL_STR_PREFIX(s, p) CNXK_TEL_STR(p##s)
#define CNXK_TEL_DICT_INT(d, p, s, ...)                                        \
	plt_tel_data_add_dict_int(d, CNXK_TEL_STR_PREFIX(s, __VA_ARGS__),      \
				  (p)->s)
#define CNXK_TEL_DICT_PTR(d, p, s, ...)                                        \
	plt_tel_data_add_dict_ptr(d, CNXK_TEL_STR_PREFIX(s, __VA_ARGS__),      \
				  (void *)(p)->s)
#define CNXK_TEL_DICT_BF_PTR(d, p, s, ...)                                     \
	plt_tel_data_add_dict_ptr(d, CNXK_TEL_STR_PREFIX(s, __VA_ARGS__),      \
				  (void *)(uint64_t)(p)->s)
#define CNXK_TEL_DICT_U64(d, p, s, ...)                                        \
	plt_tel_data_add_dict_u64(d, CNXK_TEL_STR_PREFIX(s, __VA_ARGS__),      \
				  (p)->s)
#define CNXK_TEL_DICT_STR(d, p, s, ...)                                        \
	plt_tel_data_add_dict_string(d, CNXK_TEL_STR_PREFIX(s, __VA_ARGS__),   \
				     (p)->s)

#endif /* __CNXK_TELEMETRY_H_ */
