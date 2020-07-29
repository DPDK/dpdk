/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2019 Mellanox Technologies, Ltd
 */

#ifndef RTE_PMD_MLX5_DEVX_CMDS_H_
#define RTE_PMD_MLX5_DEVX_CMDS_H_

#include "mlx5_glue.h"
#include "mlx5_prm.h"


/* This is limitation of libibverbs: in length variable type is u16. */
#define MLX5_DEVX_MAX_KLM_ENTRIES ((UINT16_MAX - \
		MLX5_ST_SZ_DW(create_mkey_in) * 4) / (MLX5_ST_SZ_DW(klm) * 4))

struct mlx5_devx_mkey_attr {
	uint64_t addr;
	uint64_t size;
	uint32_t umem_id;
	uint32_t pd;
	uint32_t log_entity_size;
	uint32_t pg_access:1;
	uint32_t relaxed_ordering:1;
	struct mlx5_klm *klm_array;
	int klm_num;
};

/* HCA qos attributes. */
struct mlx5_hca_qos_attr {
	uint32_t sup:1;	/* Whether QOS is supported. */
	uint32_t srtcm_sup:1; /* Whether srTCM mode is supported. */
	uint32_t packet_pacing:1; /* Packet pacing is supported. */
	uint32_t wqe_rate_pp:1; /* Packet pacing WQE rate mode. */
	uint32_t flow_meter_reg_share:1;
	/* Whether reg_c share is supported. */
	uint8_t log_max_flow_meter;
	/* Power of the maximum supported meters. */
	uint8_t flow_meter_reg_c_ids;
	/* Bitmap of the reg_Cs available for flow meter to use. */

};

struct mlx5_hca_vdpa_attr {
	uint8_t virtio_queue_type;
	uint32_t valid:1;
	uint32_t desc_tunnel_offload_type:1;
	uint32_t eth_frame_offload_type:1;
	uint32_t virtio_version_1_0:1;
	uint32_t tso_ipv4:1;
	uint32_t tso_ipv6:1;
	uint32_t tx_csum:1;
	uint32_t rx_csum:1;
	uint32_t event_mode:3;
	uint32_t log_doorbell_stride:5;
	uint32_t log_doorbell_bar_size:5;
	uint32_t queue_counters_valid:1;
	uint32_t max_num_virtio_queues;
	struct {
		uint32_t a;
		uint32_t b;
	} umems[3];
	uint64_t doorbell_bar_offset;
};

/* HCA supports this number of time periods for LRO. */
#define MLX5_LRO_NUM_SUPP_PERIODS 4

/* HCA attributes. */
struct mlx5_hca_attr {
	uint32_t eswitch_manager:1;
	uint32_t flow_counters_dump:1;
	uint32_t log_max_rqt_size:5;
	uint32_t parse_graph_flex_node:1;
	uint8_t flow_counter_bulk_alloc_bitmap;
	uint32_t eth_net_offloads:1;
	uint32_t eth_virt:1;
	uint32_t wqe_vlan_insert:1;
	uint32_t wqe_inline_mode:2;
	uint32_t vport_inline_mode:3;
	uint32_t tunnel_stateless_geneve_rx:1;
	uint32_t geneve_max_opt_len:1; /* 0x0: 14DW, 0x1: 63DW */
	uint32_t tunnel_stateless_gtp:1;
	uint32_t lro_cap:1;
	uint32_t tunnel_lro_gre:1;
	uint32_t tunnel_lro_vxlan:1;
	uint32_t lro_max_msg_sz_mode:2;
	uint32_t lro_timer_supported_periods[MLX5_LRO_NUM_SUPP_PERIODS];
	uint32_t flex_parser_protocols;
	uint32_t hairpin:1;
	uint32_t log_max_hairpin_queues:5;
	uint32_t log_max_hairpin_wq_data_sz:5;
	uint32_t log_max_hairpin_num_packets:5;
	uint32_t vhca_id:16;
	uint32_t relaxed_ordering_write:1;
	uint32_t relaxed_ordering_read:1;
	uint32_t access_register_user:1;
	uint32_t wqe_index_ignore:1;
	uint32_t cross_channel:1;
	uint32_t non_wire_sq:1; /* SQ with non-wire ops is supported. */
	uint32_t log_max_static_sq_wq:5; /* Static WQE size SQ. */
	uint32_t dev_freq_khz; /* Timestamp counter frequency, kHz. */
	uint32_t scatter_fcs_w_decap_disable:1;
	uint32_t regex:1;
	uint32_t regexp_num_of_engines;
	struct mlx5_hca_qos_attr qos;
	struct mlx5_hca_vdpa_attr vdpa;
};

struct mlx5_devx_wq_attr {
	uint32_t wq_type:4;
	uint32_t wq_signature:1;
	uint32_t end_padding_mode:2;
	uint32_t cd_slave:1;
	uint32_t hds_skip_first_sge:1;
	uint32_t log2_hds_buf_size:3;
	uint32_t page_offset:5;
	uint32_t lwm:16;
	uint32_t pd:24;
	uint32_t uar_page:24;
	uint64_t dbr_addr;
	uint32_t hw_counter;
	uint32_t sw_counter;
	uint32_t log_wq_stride:4;
	uint32_t log_wq_pg_sz:5;
	uint32_t log_wq_sz:5;
	uint32_t dbr_umem_valid:1;
	uint32_t wq_umem_valid:1;
	uint32_t log_hairpin_num_packets:5;
	uint32_t log_hairpin_data_sz:5;
	uint32_t single_wqe_log_num_of_strides:4;
	uint32_t two_byte_shift_en:1;
	uint32_t single_stride_log_num_of_bytes:3;
	uint32_t dbr_umem_id;
	uint32_t wq_umem_id;
	uint64_t wq_umem_offset;
};

/* Create RQ attributes structure, used by create RQ operation. */
struct mlx5_devx_create_rq_attr {
	uint32_t rlky:1;
	uint32_t delay_drop_en:1;
	uint32_t scatter_fcs:1;
	uint32_t vsd:1;
	uint32_t mem_rq_type:4;
	uint32_t state:4;
	uint32_t flush_in_error_en:1;
	uint32_t hairpin:1;
	uint32_t user_index:24;
	uint32_t cqn:24;
	uint32_t counter_set_id:8;
	uint32_t rmpn:24;
	struct mlx5_devx_wq_attr wq_attr;
};

/* Modify RQ attributes structure, used by modify RQ operation. */
struct mlx5_devx_modify_rq_attr {
	uint32_t rqn:24;
	uint32_t rq_state:4; /* Current RQ state. */
	uint32_t state:4; /* Required RQ state. */
	uint32_t scatter_fcs:1;
	uint32_t vsd:1;
	uint32_t counter_set_id:8;
	uint32_t hairpin_peer_sq:24;
	uint32_t hairpin_peer_vhca:16;
	uint64_t modify_bitmask;
	uint32_t lwm:16; /* Contained WQ lwm. */
};

struct mlx5_rx_hash_field_select {
	uint32_t l3_prot_type:1;
	uint32_t l4_prot_type:1;
	uint32_t selected_fields:30;
};

/* TIR attributes structure, used by TIR operations. */
struct mlx5_devx_tir_attr {
	uint32_t disp_type:4;
	uint32_t lro_timeout_period_usecs:16;
	uint32_t lro_enable_mask:4;
	uint32_t lro_max_msg_sz:8;
	uint32_t inline_rqn:24;
	uint32_t rx_hash_symmetric:1;
	uint32_t tunneled_offload_en:1;
	uint32_t indirect_table:24;
	uint32_t rx_hash_fn:4;
	uint32_t self_lb_block:2;
	uint32_t transport_domain:24;
	uint8_t rx_hash_toeplitz_key[MLX5_RSS_HASH_KEY_LEN];
	struct mlx5_rx_hash_field_select rx_hash_field_selector_outer;
	struct mlx5_rx_hash_field_select rx_hash_field_selector_inner;
};

/* RQT attributes structure, used by RQT operations. */
struct mlx5_devx_rqt_attr {
	uint8_t rq_type;
	uint32_t rqt_max_size:16;
	uint32_t rqt_actual_size:16;
	uint32_t rq_list[];
};

/* TIS attributes structure. */
struct mlx5_devx_tis_attr {
	uint32_t strict_lag_tx_port_affinity:1;
	uint32_t tls_en:1;
	uint32_t lag_tx_port_affinity:4;
	uint32_t prio:4;
	uint32_t transport_domain:24;
};

/* SQ attributes structure, used by SQ create operation. */
struct mlx5_devx_create_sq_attr {
	uint32_t rlky:1;
	uint32_t cd_master:1;
	uint32_t fre:1;
	uint32_t flush_in_error_en:1;
	uint32_t allow_multi_pkt_send_wqe:1;
	uint32_t min_wqe_inline_mode:3;
	uint32_t state:4;
	uint32_t reg_umr:1;
	uint32_t allow_swp:1;
	uint32_t hairpin:1;
	uint32_t non_wire:1;
	uint32_t static_sq_wq:1;
	uint32_t user_index:24;
	uint32_t cqn:24;
	uint32_t packet_pacing_rate_limit_index:16;
	uint32_t tis_lst_sz:16;
	uint32_t tis_num:24;
	struct mlx5_devx_wq_attr wq_attr;
};

/* SQ attributes structure, used by SQ modify operation. */
struct mlx5_devx_modify_sq_attr {
	uint32_t sq_state:4;
	uint32_t state:4;
	uint32_t hairpin_peer_rq:24;
	uint32_t hairpin_peer_vhca:16;
};


/* CQ attributes structure, used by CQ operations. */
struct mlx5_devx_cq_attr {
	uint32_t q_umem_valid:1;
	uint32_t db_umem_valid:1;
	uint32_t use_first_only:1;
	uint32_t overrun_ignore:1;
	uint32_t cqe_comp_en:1;
	uint32_t mini_cqe_res_format:2;
	uint32_t cqe_size:3;
	uint32_t log_cq_size:5;
	uint32_t log_page_size:5;
	uint32_t uar_page_id;
	uint32_t q_umem_id;
	uint64_t q_umem_offset;
	uint32_t db_umem_id;
	uint64_t db_umem_offset;
	uint32_t eqn;
	uint64_t db_addr;
};

/* Virtq attributes structure, used by VIRTQ operations. */
struct mlx5_devx_virtq_attr {
	uint16_t hw_available_index;
	uint16_t hw_used_index;
	uint16_t q_size;
	uint32_t pd:24;
	uint32_t virtio_version_1_0:1;
	uint32_t tso_ipv4:1;
	uint32_t tso_ipv6:1;
	uint32_t tx_csum:1;
	uint32_t rx_csum:1;
	uint32_t event_mode:3;
	uint32_t state:4;
	uint32_t dirty_bitmap_dump_enable:1;
	uint32_t dirty_bitmap_mkey;
	uint32_t dirty_bitmap_size;
	uint32_t mkey;
	uint32_t qp_id;
	uint32_t queue_index;
	uint32_t tis_id;
	uint32_t counters_obj_id;
	uint64_t dirty_bitmap_addr;
	uint64_t type;
	uint64_t desc_addr;
	uint64_t used_addr;
	uint64_t available_addr;
	struct {
		uint32_t id;
		uint32_t size;
		uint64_t offset;
	} umems[3];
};


struct mlx5_devx_qp_attr {
	uint32_t pd:24;
	uint32_t uar_index:24;
	uint32_t cqn:24;
	uint32_t log_page_size:5;
	uint32_t rq_size:17; /* Must be power of 2. */
	uint32_t log_rq_stride:3;
	uint32_t sq_size:17; /* Must be power of 2. */
	uint32_t dbr_umem_valid:1;
	uint32_t dbr_umem_id;
	uint64_t dbr_address;
	uint32_t wq_umem_id;
	uint64_t wq_umem_offset;
};

struct mlx5_devx_virtio_q_couners_attr {
	uint64_t received_desc;
	uint64_t completed_desc;
	uint32_t error_cqes;
	uint32_t bad_desc_errors;
	uint32_t exceed_max_chain;
	uint32_t invalid_buffer;
};

/*
 * graph flow match sample attributes structure,
 * used by flex parser operations.
 */
struct mlx5_devx_match_sample_attr {
	uint32_t flow_match_sample_en:1;
	uint32_t flow_match_sample_field_offset:16;
	uint32_t flow_match_sample_offset_mode:4;
	uint32_t flow_match_sample_field_offset_mask;
	uint32_t flow_match_sample_field_offset_shift:4;
	uint32_t flow_match_sample_field_base_offset:8;
	uint32_t flow_match_sample_tunnel_mode:3;
	uint32_t flow_match_sample_field_id;
};

/* graph node arc attributes structure, used by flex parser operations. */
struct mlx5_devx_graph_arc_attr {
	uint32_t compare_condition_value:16;
	uint32_t start_inner_tunnel:1;
	uint32_t arc_parse_graph_node:8;
	uint32_t parse_graph_node_handle;
};

/* Maximal number of samples per graph node. */
#define MLX5_GRAPH_NODE_SAMPLE_NUM 8

/* Maximal number of input/output arcs per graph node. */
#define MLX5_GRAPH_NODE_ARC_NUM 8

/* parse graph node attributes structure, used by flex parser operations. */
struct mlx5_devx_graph_node_attr {
	uint32_t modify_field_select;
	uint32_t header_length_mode:4;
	uint32_t header_length_base_value:16;
	uint32_t header_length_field_shift:4;
	uint32_t header_length_field_offset:16;
	uint32_t header_length_field_mask;
	struct mlx5_devx_match_sample_attr sample[MLX5_GRAPH_NODE_SAMPLE_NUM];
	uint32_t next_header_field_offset:16;
	uint32_t next_header_field_size:5;
	struct mlx5_devx_graph_arc_attr in[MLX5_GRAPH_NODE_ARC_NUM];
	struct mlx5_devx_graph_arc_attr out[MLX5_GRAPH_NODE_ARC_NUM];
};

/* mlx5_devx_cmds.c */

__rte_internal
struct mlx5_devx_obj *mlx5_devx_cmd_flow_counter_alloc(void *ctx,
						       uint32_t bulk_sz);
__rte_internal
int mlx5_devx_cmd_destroy(struct mlx5_devx_obj *obj);
__rte_internal
int mlx5_devx_cmd_flow_counter_query(struct mlx5_devx_obj *dcs,
				     int clear, uint32_t n_counters,
				     uint64_t *pkts, uint64_t *bytes,
				     uint32_t mkey, void *addr,
				     void *cmd_comp,
				     uint64_t async_id);
__rte_internal
int mlx5_devx_cmd_query_hca_attr(void *ctx,
				 struct mlx5_hca_attr *attr);
__rte_internal
struct mlx5_devx_obj *mlx5_devx_cmd_mkey_create(void *ctx,
					      struct mlx5_devx_mkey_attr *attr);
__rte_internal
int mlx5_devx_get_out_command_status(void *out);
__rte_internal
int mlx5_devx_cmd_qp_query_tis_td(void *qp, uint32_t tis_num,
				  uint32_t *tis_td);
__rte_internal
struct mlx5_devx_obj *mlx5_devx_cmd_create_rq(void *ctx,
				       struct mlx5_devx_create_rq_attr *rq_attr,
				       int socket);
__rte_internal
int mlx5_devx_cmd_modify_rq(struct mlx5_devx_obj *rq,
			    struct mlx5_devx_modify_rq_attr *rq_attr);
__rte_internal
struct mlx5_devx_obj *mlx5_devx_cmd_create_tir(void *ctx,
					   struct mlx5_devx_tir_attr *tir_attr);
__rte_internal
struct mlx5_devx_obj *mlx5_devx_cmd_create_rqt(void *ctx,
					   struct mlx5_devx_rqt_attr *rqt_attr);
__rte_internal
struct mlx5_devx_obj *mlx5_devx_cmd_create_sq(void *ctx,
				      struct mlx5_devx_create_sq_attr *sq_attr);
__rte_internal
int mlx5_devx_cmd_modify_sq(struct mlx5_devx_obj *sq,
			    struct mlx5_devx_modify_sq_attr *sq_attr);
__rte_internal
struct mlx5_devx_obj *mlx5_devx_cmd_create_tis(void *ctx,
					   struct mlx5_devx_tis_attr *tis_attr);
__rte_internal
struct mlx5_devx_obj *mlx5_devx_cmd_create_td(void *ctx);
__rte_internal
int mlx5_devx_cmd_flow_dump(void *fdb_domain, void *rx_domain, void *tx_domain,
			    FILE *file);
__rte_internal
struct mlx5_devx_obj *mlx5_devx_cmd_create_cq(void *ctx,
					      struct mlx5_devx_cq_attr *attr);
__rte_internal
struct mlx5_devx_obj *mlx5_devx_cmd_create_virtq(void *ctx,
					     struct mlx5_devx_virtq_attr *attr);
__rte_internal
int mlx5_devx_cmd_modify_virtq(struct mlx5_devx_obj *virtq_obj,
			       struct mlx5_devx_virtq_attr *attr);
__rte_internal
int mlx5_devx_cmd_query_virtq(struct mlx5_devx_obj *virtq_obj,
			      struct mlx5_devx_virtq_attr *attr);
__rte_internal
struct mlx5_devx_obj *mlx5_devx_cmd_create_qp(void *ctx,
					      struct mlx5_devx_qp_attr *attr);
__rte_internal
int mlx5_devx_cmd_modify_qp_state(struct mlx5_devx_obj *qp,
				  uint32_t qp_st_mod_op, uint32_t remote_qp_id);
__rte_internal
int mlx5_devx_cmd_modify_rqt(struct mlx5_devx_obj *rqt,
			     struct mlx5_devx_rqt_attr *rqt_attr);
__rte_internal
int mlx5_devx_cmd_query_parse_samples(struct mlx5_devx_obj *flex_obj,
				      uint32_t ids[], uint32_t num);

__rte_internal
struct mlx5_devx_obj *mlx5_devx_cmd_create_flex_parser(void *ctx,
					struct mlx5_devx_graph_node_attr *data);

__rte_internal
int mlx5_devx_cmd_register_read(void *ctx, uint16_t reg_id,
				uint32_t arg, uint32_t *data, uint32_t dw_cnt);
/**
 * Create virtio queue counters object DevX API.
 *
 * @param[in] ctx
 *   Device context.

 * @return
 *   The DevX object created, NULL otherwise and rte_errno is set.
 */
__rte_internal
struct mlx5_devx_obj *mlx5_devx_cmd_create_virtio_q_counters(void *ctx);

/**
 * Query virtio queue counters object using DevX API.
 *
 * @param[in] couners_obj
 *   Pointer to virtq object structure.
 * @param [in/out] attr
 *   Pointer to virtio queue counters attributes structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
__rte_internal
int mlx5_devx_cmd_query_virtio_q_counters(struct mlx5_devx_obj *couners_obj,
				  struct mlx5_devx_virtio_q_couners_attr *attr);

#endif /* RTE_PMD_MLX5_DEVX_CMDS_H_ */
