/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023-2024 Broadcom
 * All rights reserved.
 */

#ifndef _CFA_BLD_P70_HOST_MPC_WRAPPER_H_
#define _CFA_BLD_P70_HOST_MPC_WRAPPER_H_

#include "cfa_bld_mpcops.h"
/**
 * MPC Cache operation command build apis
 */
int cfa_bld_p70_mpc_build_cache_read(uint8_t *cmd, uint32_t *cmd_buff_len,
				     struct cfa_mpc_data_obj *fields);

int cfa_bld_p70_mpc_build_cache_write(uint8_t *cmd, uint32_t *cmd_buff_len,
				      const uint8_t *data,
				      struct cfa_mpc_data_obj *fields);

int cfa_bld_p70_mpc_build_cache_evict(uint8_t *cmd, uint32_t *cmd_buff_len,
				      struct cfa_mpc_data_obj *fields);

int cfa_bld_p70_mpc_build_cache_rdclr(uint8_t *cmd, uint32_t *cmd_buff_len,
				      struct cfa_mpc_data_obj *fields);

/**
 * MPC EM operation command build apis
 */
int cfa_bld_p70_mpc_build_em_search(uint8_t *cmd, uint32_t *cmd_buff_len,
				    uint8_t *em_entry,
				    struct cfa_mpc_data_obj *fields);

int cfa_bld_p70_mpc_build_em_insert(uint8_t *cmd, uint32_t *cmd_buff_len,
				    const uint8_t *em_entry,
				    struct cfa_mpc_data_obj *fields);

int cfa_bld_p70_mpc_build_em_delete(uint8_t *cmd, uint32_t *cmd_buff_len,
				    struct cfa_mpc_data_obj *fields);

int cfa_bld_p70_mpc_build_em_chain(uint8_t *cmd, uint32_t *cmd_buff_len,
				   struct cfa_mpc_data_obj *fields);

/**
 * MPC Cache operation completion parse apis
 */
int cfa_bld_p70_mpc_parse_cache_read(uint8_t *resp, uint32_t resp_buff_len,
				     uint8_t *rd_data, uint32_t rd_data_len,
				     struct cfa_mpc_data_obj *fields);

int cfa_bld_p70_mpc_parse_cache_write(uint8_t *resp, uint32_t resp_buff_len,
				      struct cfa_mpc_data_obj *fields);

int cfa_bld_p70_mpc_parse_cache_evict(uint8_t *resp, uint32_t resp_buff_len,
				      struct cfa_mpc_data_obj *fields);

int cfa_bld_p70_mpc_parse_cache_rdclr(uint8_t *resp, uint32_t resp_buff_len,
				      uint8_t *rd_data, uint32_t rd_data_len,
				      struct cfa_mpc_data_obj *fields);

/**
 * MPC EM operation completion parse apis
 */
int cfa_bld_p70_mpc_parse_em_search(uint8_t *resp, uint32_t resp_buff_len,
				    struct cfa_mpc_data_obj *fields);

int cfa_bld_p70_mpc_parse_em_insert(uint8_t *resp, uint32_t resp_buff_len,
				    struct cfa_mpc_data_obj *fields);

int cfa_bld_p70_mpc_parse_em_delete(uint8_t *resp, uint32_t resp_buff_len,
				    struct cfa_mpc_data_obj *fields);

int cfa_bld_p70_mpc_parse_em_chain(uint8_t *resp, uint32_t resp_buff_len,
				   struct cfa_mpc_data_obj *fields);

#endif /* _CFA_BLD_P70_HOST_MPC_WRAPPER_H_ */
