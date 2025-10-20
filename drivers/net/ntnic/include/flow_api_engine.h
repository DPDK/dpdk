/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef _FLOW_API_ENGINE_H_
#define _FLOW_API_ENGINE_H_

#include <stdint.h>
#include <stdatomic.h>

#include "hw_mod_backend.h"
#include "stream_binary_flow_api.h"

/*
 * Resource management
 */
#define BIT_CONTAINER_8_ALIGN(x) (((x) + 7) / 8)

/*
 * Resource management
 * These are free resources in FPGA
 * Other FPGA memory lists are linked to one of these
 * and will implicitly follow them
 */
enum res_type_e {
	RES_QUEUE,
	RES_CAT_CFN,
	RES_CAT_COT,
	RES_CAT_EXO,
	RES_CAT_LEN,
	RES_KM_FLOW_TYPE,
	RES_KM_CATEGORY,
	RES_HSH_RCP,
	RES_PDB_RCP,
	RES_QSL_RCP,
	RES_QSL_QST,
	RES_SLC_LR_RCP,

	RES_FLM_FLOW_TYPE,
	RES_FLM_RCP,
	RES_TPE_RCP,
	RES_TPE_EXT,
	RES_TPE_RPL,
	RES_SCRUB_RCP,
	RES_COUNT,
	RES_INVALID,
	RES_END
};

/*
 * Flow NIC offload management
 */
#define MAX_OUTPUT_DEST (128)

#define MAX_WORD_NUM 24
#define MAX_BANKS 6

#define MAX_TCAM_START_OFFSETS 4

#define MAX_FLM_MTRS_SUPPORTED 4
#define MAX_CPY_WRITERS_SUPPORTED 8

#define MAX_MATCH_FIELDS 16

/*
 *          128      128     32     32    32
 * Have  |  QW0  ||  QW4  || SW8 || SW9 | SWX   in FPGA
 *
 * Each word may start at any offset, though
 * they are combined in chronological order, with all enabled to
 * build the extracted match data, thus that is how the match key
 * must be build
 */
enum extractor_e {
	KM_USE_EXTRACTOR_UNDEF,
	KM_USE_EXTRACTOR_QWORD,
	KM_USE_EXTRACTOR_SWORD,
};

struct match_elem_s {
	enum extractor_e extr;
	int masked_for_tcam;	/* if potentially selected for TCAM */
	uint32_t e_word[4];
	uint32_t e_mask[4];

	int extr_start_offs_id;
	int8_t rel_offs;
	uint32_t word_len;
};

enum cam_tech_use_e {
	KM_CAM,
	KM_TCAM,
	KM_SYNERGY
};

struct km_flow_def_s {
	struct flow_api_backend_s *be;

	/* For keeping track of identical entries */
	struct km_flow_def_s *reference;
	struct km_flow_def_s *root;

	/* For collect flow elements and sorting */
	struct match_elem_s match[MAX_MATCH_FIELDS];
	struct match_elem_s *match_map[MAX_MATCH_FIELDS];
	int num_ftype_elem;

	/* Finally formatted CAM/TCAM entry */
	enum cam_tech_use_e target;
	uint32_t entry_word[MAX_WORD_NUM];
	uint32_t entry_mask[MAX_WORD_NUM];
	int key_word_size;

	/* TCAM calculated possible bank start offsets */
	int start_offsets[MAX_TCAM_START_OFFSETS];
	int num_start_offsets;

	/* Flow information */
	/* HW input port ID needed for compare. In port must be identical on flow types */
	uint32_t port_id;
	uint32_t info;	/* used for color (actions) */
	int info_set;
	int flow_type;	/* 0 is illegal and used as unset */
	int flushed_to_target;	/* if this km entry has been finally programmed into NIC hw */

	/* CAM specific bank management */
	int cam_paired;
	int record_indexes[MAX_BANKS];
	int bank_used;
	uint32_t *cuckoo_moves;	/* for CAM statistics only */
	struct cam_distrib_s *cam_dist;
	struct hasher_s *hsh;

	/* TCAM specific bank management */
	struct tcam_distrib_s *tcam_dist;
	int tcam_start_bank;
	int tcam_record;
};

/*
 * RSS configuration, see struct rte_flow_action_rss
 */
struct hsh_def_s {
	enum rte_eth_hash_function func;	/* RSS hash function to apply */
	/* RSS hash types, see definition of RTE_ETH_RSS_* for hash calculation options */
	uint64_t types;
	uint32_t key_len;	/* Hash key length in bytes. */
	const uint8_t *key;	/* Hash key. */
};

/*
 * AGE configuration, see struct rte_flow_action_age
 */
struct age_def_s {
	uint32_t timeout;
	void *context;
};

/*
 * Tunnel encapsulation header definition
 */
#define MAX_TUN_HDR_SIZE 128

struct tunnel_header_s {
	union {
		uint8_t hdr8[MAX_TUN_HDR_SIZE];
		uint32_t hdr32[(MAX_TUN_HDR_SIZE + 3) / 4];
	} d;

	uint8_t len;

	uint8_t nb_vlans;

	uint8_t ip_version;	/* 4: v4, 6: v6 */

	uint8_t new_outer;
	uint8_t l2_len;
	uint8_t l3_len;
	uint8_t l4_len;
};

enum flow_port_type_e {
	PORT_NONE,	/* not defined or drop */
	PORT_INTERNAL,	/* no queues attached */
	PORT_PHY,	/* MAC phy output queue */
	PORT_VIRT,	/* Memory queues to Host */
};

struct output_s {
	uint32_t owning_port_id;/* the port who owns this output destination */
	enum flow_port_type_e type;
	int id;	/* depending on port type: queue ID or physical port id or not used */
	int active;	/* activated */
};

struct nic_flow_def {
	/*
	 * Frame Decoder match info collected
	 */
	int l2_prot;
	int l3_prot;
	int l4_prot;
	int tunnel_prot;
	int tunnel_l3_prot;
	int tunnel_l4_prot;
	int vlans;
	int fragmentation;
	int ip_prot;
	int tunnel_ip_prot;
	/*
	 * Additional meta data for various functions
	 */
	int in_port_override;
	int non_empty;	/* default value is -1; value 1 means flow actions update */
	struct output_s dst_id[MAX_OUTPUT_DEST];/* define the output to use */
	/* total number of available queues defined for all outputs - i.e. number of dst_id's */
	int dst_num_avail;

	/*
	 * Mark or Action info collection
	 */
	uint32_t mark;

	uint32_t jump_to_group;

	uint32_t mtr_ids[MAX_FLM_MTRS_SUPPORTED];

	int full_offload;

	/*
	 * Action push tunnel
	 */
	struct tunnel_header_s tun_hdr;

	/*
	 * If DPDK RTE tunnel helper API used
	 * this holds the tunnel if used in flow
	 */
	struct tunnel_s *tnl;

	/*
	 * Header Stripper
	 */
	int header_strip_end_dyn;
	int header_strip_end_ofs;

	/*
	 * Modify field
	 */
	struct {
		uint32_t select;
		uint32_t dyn;
		uint32_t ofs;
		uint32_t len;
		uint32_t level;
		union {
			uint8_t value8[16];
			uint16_t value16[8];
			uint32_t value32[4];
		};
	} modify_field[MAX_CPY_WRITERS_SUPPORTED];

	uint32_t modify_field_count;
	uint8_t ttl_sub_enable;
	uint8_t ttl_sub_ipv4;
	uint8_t ttl_sub_outer;

	/*
	 * Key Matcher flow definitions
	 */
	struct km_flow_def_s km;

	/*
	 * Hash module RSS definitions
	 */
	struct hsh_def_s hsh;

	/*
	 * AGE action timeout
	 */
	struct age_def_s age;

	/*
	 * TX fragmentation IFR/RPP_LR MTU recipe
	 */
	uint8_t flm_mtu_fragmentation_recipe;
};

enum flow_handle_type {
	FLOW_HANDLE_TYPE_FLOW,
	FLOW_HANDLE_TYPE_FLM,
};

struct flow_handle {
	enum flow_handle_type type;
	uint32_t flm_id;
	uint16_t caller_id;
	RTE_ATOMIC(uint16_t) learn_ignored;

	struct flow_eth_dev *dev;
	struct flow_handle *next;
	struct flow_handle *prev;

	/* Flow specific pointer to application data stored during action creation. */
	void *context;
	void *user_data;

	union {
		struct {
			/*
			 * 1st step conversion and validation of flow
			 * verified and converted flow match + actions structure
			 */
			struct nic_flow_def *fd;
			/*
			 * 2nd step NIC HW resource allocation and configuration
			 * NIC resource management structures
			 */
			struct {
				uint32_t db_idx_counter;
				uint32_t db_idxs[RES_COUNT];
			};
			uint32_t port_id;	/* MAC port ID or override of virtual in_port */
		};

		struct {
			uint32_t flm_db_idx_counter;
			uint32_t flm_db_idxs[RES_COUNT];

			uint32_t flm_mtr_ids[MAX_FLM_MTRS_SUPPORTED];

			uint32_t flm_data[10];
			uint8_t flm_prot;
			uint8_t flm_kid;
			uint8_t flm_prio;
			uint8_t flm_ft;

			uint16_t flm_rpl_ext_ptr;
			uint32_t flm_nat_ipv4;
			uint16_t flm_nat_port;
			uint8_t flm_dscp;
			uint32_t flm_teid;
			uint8_t flm_rqi;
			uint8_t flm_qfi;
			uint8_t flm_scrub_prof;

			uint8_t flm_mtu_fragmentation_recipe;

			/* Flow specific pointer to application template table cell stored during
			 * flow create.
			 */
			struct flow_template_table_cell *template_table_cell;
			bool flm_async;
		};
	};
};

struct flow_pattern_template {
	struct nic_flow_def *fd;
};

struct flow_actions_template {
	struct nic_flow_def *fd;

	uint32_t num_dest_port;
	uint32_t num_queues;
};

struct flow_template_table_cell {
	atomic_int status;
	atomic_int counter;

	uint32_t flm_db_idx_counter;
	uint32_t flm_db_idxs[RES_COUNT];

	uint32_t flm_key_id;
	uint32_t flm_ft;

	uint16_t flm_rpl_ext_ptr;
	uint8_t  flm_scrub_prof;
};

struct flow_template_table {
	struct flow_pattern_template **pattern_templates;
	uint8_t nb_pattern_templates;

	struct flow_actions_template **actions_templates;
	uint8_t nb_actions_templates;

	struct flow_template_table_cell *pattern_action_pairs;

	struct rte_flow_attr attr;
	uint16_t forced_vlan_vid;
	uint16_t caller_id;
};

void nthw_km_attach_ndev_resource_management(struct km_flow_def_s *km, void **handle);
void nthw_km_free_ndev_resource_management(void **handle);

int nthw_km_add_match_elem(struct km_flow_def_s *km, uint32_t e_word[4], uint32_t e_mask[4],
	uint32_t word_len, enum frame_offs_e start, int8_t offset);

int nthw_km_key_create(struct km_flow_def_s *km, uint32_t port_id);
/*
 * Compares 2 KM key definitions after first collect validate and optimization.
 * km is compared against an existing km1.
 * if identical, km1 flow_type is returned
 */
int nthw_km_key_compare(struct km_flow_def_s *km, struct km_flow_def_s *km1);

int nthw_km_rcp_set(struct km_flow_def_s *km, int index);

int nthw_km_write_data_match_entry(struct km_flow_def_s *km, uint32_t color);
int nthw_km_clear_data_match_entry(struct km_flow_def_s *km);

void nthw_kcc_free_ndev_resource_mgmnt(void **handle);

/*
 * Group management
 */
int nthw_flow_group_handle_create(void **handle, uint32_t group_count);
int nthw_flow_group_handle_destroy(void **handle);

int nthw_flow_group_translate_get(void *handle, uint8_t owner_id, uint8_t port_id,
	uint32_t group_in, uint32_t *group_out);
int nthw_flow_group_translate_get_orig_group(void *handle, uint32_t translated_group,
	uint32_t *group_orig);

#endif  /* _FLOW_API_ENGINE_H_ */
