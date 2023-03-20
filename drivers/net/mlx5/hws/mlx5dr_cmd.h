/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */

#ifndef MLX5DR_CMD_H_
#define MLX5DR_CMD_H_

struct mlx5dr_cmd_set_fte_attr {
	uint32_t action_flags;
	uint8_t destination_type;
	uint32_t destination_id;
	uint8_t flow_source;
};

struct mlx5dr_cmd_ft_create_attr {
	uint8_t type;
	uint8_t level;
	bool rtc_valid;
};

#define ACCESS_KEY_LEN	32

struct mlx5dr_cmd_ft_modify_attr {
	uint8_t type;
	uint32_t rtc_id_0;
	uint32_t rtc_id_1;
	uint32_t table_miss_id;
	uint8_t table_miss_action;
	uint64_t modify_fs;
};

struct mlx5dr_cmd_ft_query_attr {
	uint8_t type;
};

struct mlx5dr_cmd_fg_attr {
	uint32_t table_id;
	uint32_t table_type;
};

struct mlx5dr_cmd_forward_tbl {
	struct mlx5dr_devx_obj	*ft;
	struct mlx5dr_devx_obj	*fg;
	struct mlx5dr_devx_obj	*fte;
	uint32_t refcount;
};

struct mlx5dr_cmd_rtc_create_attr {
	uint32_t pd;
	uint32_t stc_base;
	uint32_t ste_base;
	uint32_t ste_offset;
	uint32_t miss_ft_id;
	bool fw_gen_wqe;
	uint8_t update_index_mode;
	uint8_t access_index_mode;
	uint8_t num_hash_definer;
	uint8_t log_depth;
	uint8_t log_size;
	uint8_t table_type;
	uint8_t match_definer_0;
	uint8_t match_definer_1;
	bool is_frst_jumbo;
	bool is_scnd_range;
};

struct mlx5dr_cmd_alias_obj_create_attr {
	uint32_t obj_id;
	uint16_t vhca_id;
	uint16_t obj_type;
	uint8_t access_key[ACCESS_KEY_LEN];
};

struct mlx5dr_cmd_stc_create_attr {
	uint8_t log_obj_range;
	uint8_t table_type;
};

struct mlx5dr_cmd_stc_modify_attr {
	uint32_t stc_offset;
	uint8_t action_offset;
	enum mlx5_ifc_stc_action_type action_type;
	union {
		uint32_t id; /* TIRN, TAG, FT ID, STE ID */
		struct {
			uint8_t decap;
			uint16_t start_anchor;
			uint16_t end_anchor;
		} remove_header;
		struct {
			uint32_t arg_id;
			uint32_t pattern_id;
		} modify_header;
		struct {
			__be64 data;
		} modify_action;
		struct {
			uint32_t arg_id;
			uint32_t header_size;
			uint8_t is_inline;
			uint8_t encap;
			uint16_t insert_anchor;
			uint16_t insert_offset;
		} insert_header;
		struct {
			uint8_t aso_type;
			uint32_t devx_obj_id;
			uint8_t return_reg_id;
		} aso;
		struct {
			uint16_t vport_num;
			uint16_t esw_owner_vhca_id;
		} vport;
		struct {
			struct mlx5dr_pool_chunk ste;
			struct mlx5dr_pool *ste_pool;
			uint32_t ste_obj_id; /* Internal */
			uint32_t match_definer_id;
			uint8_t log_hash_size;
		} ste_table;
		struct {
			uint16_t start_anchor;
			uint16_t num_of_words;
		} remove_words;

		uint32_t dest_table_id;
		uint32_t dest_tir_num;
	};
};

struct mlx5dr_cmd_ste_create_attr {
	uint8_t log_obj_range;
	uint8_t table_type;
};

struct mlx5dr_cmd_definer_create_attr {
	uint8_t *dw_selector;
	uint8_t *byte_selector;
	uint8_t *match_mask;
};

struct mlx5dr_cmd_sq_create_attr {
	uint32_t cqn;
	uint32_t pdn;
	uint32_t page_id;
	uint32_t dbr_id;
	uint32_t wq_id;
	uint32_t log_wq_sz;
	uint32_t ts_format;
};

struct mlx5dr_cmd_allow_other_vhca_access_attr {
	uint16_t obj_type;
	uint32_t obj_id;
	uint8_t access_key[ACCESS_KEY_LEN];
};

struct mlx5dr_cmd_query_ft_caps {
	uint8_t max_level;
	uint8_t reparse;
};

struct mlx5dr_cmd_query_vport_caps {
	uint16_t vport_num;
	uint16_t esw_owner_vhca_id;
	uint32_t metadata_c;
	uint32_t metadata_c_mask;
};

struct mlx5dr_cmd_generate_wqe_attr {
	uint8_t *wqe_ctrl;
	uint8_t *gta_ctrl;
	uint8_t *gta_data_0;
	uint8_t *gta_data_1;
	uint32_t pdn;
};

struct mlx5dr_cmd_query_caps {
	uint32_t wire_regc;
	uint32_t wire_regc_mask;
	uint32_t flex_protocols;
	uint8_t wqe_based_update;
	uint8_t rtc_reparse_mode;
	uint16_t ste_format;
	uint8_t rtc_index_mode;
	uint8_t ste_alloc_log_max;
	uint8_t ste_alloc_log_gran;
	uint8_t stc_alloc_log_max;
	uint8_t stc_alloc_log_gran;
	uint8_t rtc_log_depth_max;
	uint8_t format_select_gtpu_dw_0;
	uint8_t format_select_gtpu_dw_1;
	uint8_t format_select_gtpu_dw_2;
	uint8_t format_select_gtpu_ext_dw_0;
	uint32_t linear_match_definer;
	uint8_t access_index_mode;
	bool full_dw_jumbo_support;
	bool rtc_hash_split_table;
	bool rtc_linear_lookup_table;
	uint32_t supp_type_gen_wqe;
	uint8_t rtc_max_hash_def_gen_wqe;
	uint16_t supp_ste_format_gen_wqe;
	struct mlx5dr_cmd_query_ft_caps nic_ft;
	struct mlx5dr_cmd_query_ft_caps fdb_ft;
	bool eswitch_manager;
	uint32_t eswitch_manager_vport_number;
	uint8_t log_header_modify_argument_granularity;
	uint8_t log_header_modify_argument_max_alloc;
	uint8_t sq_ts_format;
	uint64_t definer_format_sup;
	uint32_t trivial_match_definer;
	uint32_t vhca_id;
	bool cross_vhca_resources;
	uint32_t shared_vhca_id;
	char fw_ver[64];
	bool ipsec_offload;
};

int mlx5dr_cmd_destroy_obj(struct mlx5dr_devx_obj *devx_obj);

struct mlx5dr_devx_obj *
mlx5dr_cmd_flow_table_create(struct ibv_context *ctx,
			     struct mlx5dr_cmd_ft_create_attr *ft_attr);

int
mlx5dr_cmd_flow_table_modify(struct mlx5dr_devx_obj *devx_obj,
			     struct mlx5dr_cmd_ft_modify_attr *ft_attr);

int
mlx5dr_cmd_flow_table_query(struct mlx5dr_devx_obj *devx_obj,
			    struct mlx5dr_cmd_ft_query_attr *ft_attr,
			    uint64_t *icm_addr_0, uint64_t *icm_addr_1);

struct mlx5dr_devx_obj *
mlx5dr_cmd_rtc_create(struct ibv_context *ctx,
		      struct mlx5dr_cmd_rtc_create_attr *rtc_attr);

struct mlx5dr_devx_obj *
mlx5dr_cmd_stc_create(struct ibv_context *ctx,
		      struct mlx5dr_cmd_stc_create_attr *stc_attr);

int
mlx5dr_cmd_stc_modify(struct mlx5dr_devx_obj *devx_obj,
		      struct mlx5dr_cmd_stc_modify_attr *stc_attr);

int
mlx5dr_cmd_generate_wqe(struct ibv_context *ctx,
			struct mlx5dr_cmd_generate_wqe_attr *attr,
			struct mlx5_cqe64 *ret_cqe);

struct mlx5dr_devx_obj *
mlx5dr_cmd_ste_create(struct ibv_context *ctx,
		      struct mlx5dr_cmd_ste_create_attr *ste_attr);

struct mlx5dr_devx_obj *
mlx5dr_cmd_definer_create(struct ibv_context *ctx,
			  struct mlx5dr_cmd_definer_create_attr *def_attr);

struct mlx5dr_devx_obj *
mlx5dr_cmd_sq_create(struct ibv_context *ctx,
		     struct mlx5dr_cmd_sq_create_attr *attr);

struct mlx5dr_devx_obj *
mlx5dr_cmd_arg_create(struct ibv_context *ctx,
		      uint16_t log_obj_range,
		      uint32_t pd);

struct mlx5dr_devx_obj *
mlx5dr_cmd_header_modify_pattern_create(struct ibv_context *ctx,
					uint32_t pattern_length,
					uint8_t *actions);

struct mlx5dr_devx_obj *
mlx5dr_cmd_set_fte(struct ibv_context *ctx,
		   uint32_t table_type,
		   uint32_t table_id,
		   uint32_t group_id,
		   struct mlx5dr_cmd_set_fte_attr *fte_attr);

struct mlx5dr_cmd_forward_tbl *
mlx5dr_cmd_forward_tbl_create(struct ibv_context *ctx,
			      struct mlx5dr_cmd_ft_create_attr *ft_attr,
			      struct mlx5dr_cmd_set_fte_attr *fte_attr);

void mlx5dr_cmd_forward_tbl_destroy(struct mlx5dr_cmd_forward_tbl *tbl);

struct mlx5dr_devx_obj *
mlx5dr_cmd_alias_obj_create(struct ibv_context *ctx,
			    struct mlx5dr_cmd_alias_obj_create_attr *alias_attr);

int mlx5dr_cmd_sq_modify_rdy(struct mlx5dr_devx_obj *devx_obj);

int mlx5dr_cmd_query_ib_port(struct ibv_context *ctx,
			     struct mlx5dr_cmd_query_vport_caps *vport_caps,
			     uint32_t port_num);
int mlx5dr_cmd_query_caps(struct ibv_context *ctx,
			  struct mlx5dr_cmd_query_caps *caps);

void mlx5dr_cmd_set_attr_connect_miss_tbl(struct mlx5dr_context *ctx,
					  uint32_t fw_ft_type,
					  enum mlx5dr_table_type type,
					  struct mlx5dr_cmd_ft_modify_attr *ft_attr);

int mlx5dr_cmd_allow_other_vhca_access(struct ibv_context *ctx,
				       struct mlx5dr_cmd_allow_other_vhca_access_attr *attr);
#endif /* MLX5DR_CMD_H_ */
