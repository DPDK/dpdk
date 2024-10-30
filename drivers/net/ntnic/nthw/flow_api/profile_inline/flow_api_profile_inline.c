/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include "ntlog.h"
#include "nt_util.h"

#include "hw_mod_backend.h"
#include "flm_lrn_queue.h"
#include "flow_api.h"
#include "flow_api_engine.h"
#include "flow_api_hw_db_inline.h"
#include "flow_id_table.h"
#include "stream_binary_flow_api.h"

#include "flow_api_profile_inline.h"
#include "ntnic_mod_reg.h"
#include <rte_common.h>

#define NT_FLM_OP_UNLEARN 0
#define NT_FLM_OP_LEARN 1

static void *flm_lrn_queue_arr;

static int rx_queue_idx_to_hw_id(const struct flow_eth_dev *dev, int id)
{
	for (int i = 0; i < dev->num_queues; ++i)
		if (dev->rx_queue[i].id == id)
			return dev->rx_queue[i].hw_id;

	return -1;
}

struct flm_flow_key_def_s {
	union {
		struct {
			uint64_t qw0_dyn : 7;
			uint64_t qw0_ofs : 8;
			uint64_t qw4_dyn : 7;
			uint64_t qw4_ofs : 8;
			uint64_t sw8_dyn : 7;
			uint64_t sw8_ofs : 8;
			uint64_t sw9_dyn : 7;
			uint64_t sw9_ofs : 8;
			uint64_t outer_proto : 1;
			uint64_t inner_proto : 1;
			uint64_t pad : 2;
		};
		uint64_t data;
	};
	uint32_t mask[10];
};

/*
 * Flow Matcher functionality
 */
static uint8_t get_port_from_port_id(const struct flow_nic_dev *ndev, uint32_t port_id)
{
	struct flow_eth_dev *dev = ndev->eth_base;

	while (dev) {
		if (dev->port_id == port_id)
			return dev->port;

		dev = dev->next;
	}

	return UINT8_MAX;
}

static void nic_insert_flow(struct flow_nic_dev *ndev, struct flow_handle *fh)
{
	pthread_mutex_lock(&ndev->flow_mtx);

	if (ndev->flow_base)
		ndev->flow_base->prev = fh;

	fh->next = ndev->flow_base;
	fh->prev = NULL;
	ndev->flow_base = fh;

	pthread_mutex_unlock(&ndev->flow_mtx);
}

static void nic_remove_flow(struct flow_nic_dev *ndev, struct flow_handle *fh)
{
	struct flow_handle *next = fh->next;
	struct flow_handle *prev = fh->prev;

	pthread_mutex_lock(&ndev->flow_mtx);

	if (next && prev) {
		prev->next = next;
		next->prev = prev;

	} else if (next) {
		ndev->flow_base = next;
		next->prev = NULL;

	} else if (prev) {
		prev->next = NULL;

	} else if (ndev->flow_base == fh) {
		ndev->flow_base = NULL;
	}

	pthread_mutex_unlock(&ndev->flow_mtx);
}

static void nic_insert_flow_flm(struct flow_nic_dev *ndev, struct flow_handle *fh)
{
	pthread_mutex_lock(&ndev->flow_mtx);

	if (ndev->flow_base_flm)
		ndev->flow_base_flm->prev = fh;

	fh->next = ndev->flow_base_flm;
	fh->prev = NULL;
	ndev->flow_base_flm = fh;

	pthread_mutex_unlock(&ndev->flow_mtx);
}

static void nic_remove_flow_flm(struct flow_nic_dev *ndev, struct flow_handle *fh_flm)
{
	struct flow_handle *next = fh_flm->next;
	struct flow_handle *prev = fh_flm->prev;

	pthread_mutex_lock(&ndev->flow_mtx);

	if (next && prev) {
		prev->next = next;
		next->prev = prev;

	} else if (next) {
		ndev->flow_base_flm = next;
		next->prev = NULL;

	} else if (prev) {
		prev->next = NULL;

	} else if (ndev->flow_base_flm == fh_flm) {
		ndev->flow_base_flm = NULL;
	}

	pthread_mutex_unlock(&ndev->flow_mtx);
}

static inline struct nic_flow_def *prepare_nic_flow_def(struct nic_flow_def *fd)
{
	if (fd) {
		fd->full_offload = -1;
		fd->in_port_override = -1;
		fd->mark = UINT32_MAX;
		fd->jump_to_group = UINT32_MAX;

		fd->l2_prot = -1;
		fd->l3_prot = -1;
		fd->l4_prot = -1;
		fd->vlans = 0;
		fd->tunnel_prot = -1;
		fd->tunnel_l3_prot = -1;
		fd->tunnel_l4_prot = -1;
		fd->fragmentation = -1;
		fd->ip_prot = -1;
		fd->tunnel_ip_prot = -1;

		fd->non_empty = -1;
	}

	return fd;
}

static inline struct nic_flow_def *allocate_nic_flow_def(void)
{
	return prepare_nic_flow_def(calloc(1, sizeof(struct nic_flow_def)));
}

static bool fd_has_empty_pattern(const struct nic_flow_def *fd)
{
	return fd && fd->vlans == 0 && fd->l2_prot < 0 && fd->l3_prot < 0 && fd->l4_prot < 0 &&
		fd->tunnel_prot < 0 && fd->tunnel_l3_prot < 0 && fd->tunnel_l4_prot < 0 &&
		fd->ip_prot < 0 && fd->tunnel_ip_prot < 0 && fd->non_empty < 0;
}

static inline const void *memcpy_mask_if(void *dest, const void *src, const void *mask,
	size_t count)
{
	if (mask == NULL)
		return src;

	unsigned char *dest_ptr = (unsigned char *)dest;
	const unsigned char *src_ptr = (const unsigned char *)src;
	const unsigned char *mask_ptr = (const unsigned char *)mask;

	for (size_t i = 0; i < count; ++i)
		dest_ptr[i] = src_ptr[i] & mask_ptr[i];

	return dest;
}

static int flm_flow_programming(struct flow_handle *fh, uint32_t flm_op)
{
	struct flm_v25_lrn_data_s *learn_record = NULL;

	if (fh->type != FLOW_HANDLE_TYPE_FLM)
		return -1;

	if (flm_op == NT_FLM_OP_LEARN) {
		union flm_handles flm_h;
		flm_h.p = fh;
		fh->flm_id = ntnic_id_table_get_id(fh->dev->ndev->id_table_handle, flm_h,
			fh->caller_id, 1);
	}

	uint32_t flm_id = fh->flm_id;

	if (flm_op == NT_FLM_OP_UNLEARN) {
		ntnic_id_table_free_id(fh->dev->ndev->id_table_handle, flm_id);

		if (fh->learn_ignored == 1)
			return 0;
	}

	learn_record =
		(struct flm_v25_lrn_data_s *)
			flm_lrn_queue_get_write_buffer(flm_lrn_queue_arr);

	while (learn_record == NULL) {
		nt_os_wait_usec(1);
		learn_record =
			(struct flm_v25_lrn_data_s *)
			flm_lrn_queue_get_write_buffer(flm_lrn_queue_arr);
	}

	memset(learn_record, 0x0, sizeof(struct flm_v25_lrn_data_s));

	learn_record->id = flm_id;

	learn_record->qw0[0] = fh->flm_data[9];
	learn_record->qw0[1] = fh->flm_data[8];
	learn_record->qw0[2] = fh->flm_data[7];
	learn_record->qw0[3] = fh->flm_data[6];
	learn_record->qw4[0] = fh->flm_data[5];
	learn_record->qw4[1] = fh->flm_data[4];
	learn_record->qw4[2] = fh->flm_data[3];
	learn_record->qw4[3] = fh->flm_data[2];
	learn_record->sw8 = fh->flm_data[1];
	learn_record->sw9 = fh->flm_data[0];
	learn_record->prot = fh->flm_prot;

	/* Last non-zero mtr is used for statistics */
	uint8_t mbrs = 0;

	learn_record->vol_idx = mbrs;

	learn_record->nat_ip = fh->flm_nat_ipv4;
	learn_record->nat_port = fh->flm_nat_port;
	learn_record->nat_en = fh->flm_nat_ipv4 || fh->flm_nat_port ? 1 : 0;

	learn_record->dscp = fh->flm_dscp;
	learn_record->teid = fh->flm_teid;
	learn_record->qfi = fh->flm_qfi;
	learn_record->rqi = fh->flm_rqi;
	/* Lower 10 bits used for RPL EXT PTR */
	learn_record->color = fh->flm_rpl_ext_ptr & 0x3ff;

	learn_record->ent = 0;
	learn_record->op = flm_op & 0xf;
	/* Suppress generation of statistics INF_DATA */
	learn_record->nofi = 1;
	learn_record->prio = fh->flm_prio & 0x3;
	learn_record->ft = fh->flm_ft;
	learn_record->kid = fh->flm_kid;
	learn_record->eor = 1;
	learn_record->scrub_prof = 0;

	flm_lrn_queue_release_write_buffer(flm_lrn_queue_arr);
	return 0;
}

/*
 * This function must be callable without locking any mutexes
 */
static int interpret_flow_actions(const struct flow_eth_dev *dev,
	const struct rte_flow_action action[],
	const struct rte_flow_action *action_mask,
	struct nic_flow_def *fd,
	struct rte_flow_error *error,
	uint32_t *num_dest_port,
	uint32_t *num_queues)
{
	unsigned int encap_decap_order = 0;

	*num_dest_port = 0;
	*num_queues = 0;

	if (action == NULL) {
		flow_nic_set_error(ERR_FAILED, error);
		NT_LOG(ERR, FILTER, "Flow actions missing");
		return -1;
	}

	/*
	 * Gather flow match + actions and convert into internal flow definition structure (struct
	 * nic_flow_def_s) This is the 1st step in the flow creation - validate, convert and
	 * prepare
	 */
	for (int aidx = 0; action[aidx].type != RTE_FLOW_ACTION_TYPE_END; ++aidx) {
		switch (action[aidx].type) {
		case RTE_FLOW_ACTION_TYPE_PORT_ID:
			NT_LOG(DBG, FILTER, "Dev:%p: RTE_FLOW_ACTION_TYPE_PORT_ID", dev);

			if (action[aidx].conf) {
				struct rte_flow_action_port_id port_id_tmp;
				const struct rte_flow_action_port_id *port_id =
					memcpy_mask_if(&port_id_tmp, action[aidx].conf,
					action_mask ? action_mask[aidx].conf : NULL,
					sizeof(struct rte_flow_action_port_id));

				if (*num_dest_port > 0) {
					NT_LOG(ERR, FILTER,
						"Multiple port_id actions for one flow is not supported");
					flow_nic_set_error(ERR_ACTION_MULTIPLE_PORT_ID_UNSUPPORTED,
						error);
					return -1;
				}

				uint8_t port = get_port_from_port_id(dev->ndev, port_id->id);

				if (fd->dst_num_avail == MAX_OUTPUT_DEST) {
					NT_LOG(ERR, FILTER, "Too many output destinations");
					flow_nic_set_error(ERR_OUTPUT_TOO_MANY, error);
					return -1;
				}

				if (port >= dev->ndev->be.num_phy_ports) {
					NT_LOG(ERR, FILTER, "Phy port out of range");
					flow_nic_set_error(ERR_OUTPUT_INVALID, error);
					return -1;
				}

				/* New destination port to add */
				fd->dst_id[fd->dst_num_avail].owning_port_id = port_id->id;
				fd->dst_id[fd->dst_num_avail].type = PORT_PHY;
				fd->dst_id[fd->dst_num_avail].id = (int)port;
				fd->dst_id[fd->dst_num_avail].active = 1;
				fd->dst_num_avail++;

				if (fd->full_offload < 0)
					fd->full_offload = 1;

				*num_dest_port += 1;

				NT_LOG(DBG, FILTER, "Phy port ID: %i", (int)port);
			}

			break;

		case RTE_FLOW_ACTION_TYPE_QUEUE:
			NT_LOG(DBG, FILTER, "Dev:%p: RTE_FLOW_ACTION_TYPE_QUEUE", dev);

			if (action[aidx].conf) {
				struct rte_flow_action_queue queue_tmp;
				const struct rte_flow_action_queue *queue =
					memcpy_mask_if(&queue_tmp, action[aidx].conf,
					action_mask ? action_mask[aidx].conf : NULL,
					sizeof(struct rte_flow_action_queue));

				int hw_id = rx_queue_idx_to_hw_id(dev, queue->index);

				fd->dst_id[fd->dst_num_avail].owning_port_id = dev->port;
				fd->dst_id[fd->dst_num_avail].id = hw_id;
				fd->dst_id[fd->dst_num_avail].type = PORT_VIRT;
				fd->dst_id[fd->dst_num_avail].active = 1;
				fd->dst_num_avail++;

				NT_LOG(DBG, FILTER,
					"Dev:%p: RTE_FLOW_ACTION_TYPE_QUEUE port %u, queue index: %u, hw id %u",
					dev, dev->port, queue->index, hw_id);

				fd->full_offload = 0;
				*num_queues += 1;
			}

			break;

		default:
			NT_LOG(ERR, FILTER, "Invalid or unsupported flow action received - %i",
				action[aidx].type);
			flow_nic_set_error(ERR_ACTION_UNSUPPORTED, error);
			return -1;
		}
	}

	if (!(encap_decap_order == 0 || encap_decap_order == 2)) {
		NT_LOG(ERR, FILTER, "Invalid encap/decap actions");
		return -1;
	}

	return 0;
}

static int interpret_flow_elements(const struct flow_eth_dev *dev,
	const struct rte_flow_item elem[],
	struct nic_flow_def *fd __rte_unused,
	struct rte_flow_error *error,
	uint16_t implicit_vlan_vid __rte_unused,
	uint32_t *in_port_id,
	uint32_t *packet_data,
	uint32_t *packet_mask,
	struct flm_flow_key_def_s *key_def)
{
	*in_port_id = UINT32_MAX;

	memset(packet_data, 0x0, sizeof(uint32_t) * 10);
	memset(packet_mask, 0x0, sizeof(uint32_t) * 10);
	memset(key_def, 0x0, sizeof(struct flm_flow_key_def_s));

	if (elem == NULL) {
		flow_nic_set_error(ERR_FAILED, error);
		NT_LOG(ERR, FILTER, "Flow items missing");
		return -1;
	}

	int qw_reserved_mac = 0;
	int qw_reserved_ipv6 = 0;

	int qw_free = 2 - qw_reserved_mac - qw_reserved_ipv6;

	if (qw_free < 0) {
		NT_LOG(ERR, FILTER, "Key size too big. Out of QW resources.");
		flow_nic_set_error(ERR_FAILED, error);
		return -1;
	}

	for (int eidx = 0; elem[eidx].type != RTE_FLOW_ITEM_TYPE_END; ++eidx) {
		switch (elem[eidx].type) {
		case RTE_FLOW_ITEM_TYPE_ANY:
			NT_LOG(DBG, FILTER, "Adap %i, Port %i: RTE_FLOW_ITEM_TYPE_ANY",
				dev->ndev->adapter_no, dev->port);
			break;

		default:
			NT_LOG(ERR, FILTER, "Invalid or unsupported flow request: %d",
				(int)elem[eidx].type);
			flow_nic_set_error(ERR_MATCH_INVALID_OR_UNSUPPORTED_ELEM, error);
			return -1;
		}
	}

	return 0;
}

static void copy_fd_to_fh_flm(struct flow_handle *fh, const struct nic_flow_def *fd,
	const uint32_t *packet_data, uint32_t flm_key_id, uint32_t flm_ft,
	uint16_t rpl_ext_ptr, uint32_t flm_scrub __rte_unused, uint32_t priority)
{
	switch (fd->l4_prot) {
	case PROT_L4_ICMP:
		fh->flm_prot = fd->ip_prot;
		break;

	default:
		switch (fd->tunnel_l4_prot) {
		case PROT_TUN_L4_ICMP:
			fh->flm_prot = fd->tunnel_ip_prot;
			break;

		default:
			fh->flm_prot = 0;
			break;
		}

		break;
	}

	memcpy(fh->flm_data, packet_data, sizeof(uint32_t) * 10);

	fh->flm_kid = flm_key_id;
	fh->flm_rpl_ext_ptr = rpl_ext_ptr;
	fh->flm_prio = (uint8_t)priority;
	fh->flm_ft = (uint8_t)flm_ft;

	for (unsigned int i = 0; i < fd->modify_field_count; ++i) {
		switch (fd->modify_field[i].select) {
		case CPY_SELECT_DSCP_IPV4:
		case CPY_SELECT_RQI_QFI:
			fh->flm_rqi = (fd->modify_field[i].value8[0] >> 6) & 0x1;
			fh->flm_qfi = fd->modify_field[i].value8[0] & 0x3f;
			break;

		case CPY_SELECT_IPV4:
			fh->flm_nat_ipv4 = ntohl(fd->modify_field[i].value32[0]);
			break;

		case CPY_SELECT_PORT:
			fh->flm_nat_port = ntohs(fd->modify_field[i].value16[0]);
			break;

		case CPY_SELECT_TEID:
			fh->flm_teid = ntohl(fd->modify_field[i].value32[0]);
			break;

		default:
			NT_LOG(DBG, FILTER, "Unknown modify field: %d",
				fd->modify_field[i].select);
			break;
		}
	}
}

static int convert_fh_to_fh_flm(struct flow_handle *fh, const uint32_t *packet_data,
	uint32_t flm_key_id, uint32_t flm_ft, uint16_t rpl_ext_ptr,
	uint32_t flm_scrub, uint32_t priority)
{
	struct nic_flow_def *fd;
	struct flow_handle fh_copy;

	if (fh->type != FLOW_HANDLE_TYPE_FLOW)
		return -1;

	memcpy(&fh_copy, fh, sizeof(struct flow_handle));
	memset(fh, 0x0, sizeof(struct flow_handle));
	fd = fh_copy.fd;

	fh->type = FLOW_HANDLE_TYPE_FLM;
	fh->caller_id = fh_copy.caller_id;
	fh->dev = fh_copy.dev;
	fh->next = fh_copy.next;
	fh->prev = fh_copy.prev;
	fh->user_data = fh_copy.user_data;

	fh->flm_db_idx_counter = fh_copy.db_idx_counter;

	for (int i = 0; i < RES_COUNT; ++i)
		fh->flm_db_idxs[i] = fh_copy.db_idxs[i];

	copy_fd_to_fh_flm(fh, fd, packet_data, flm_key_id, flm_ft, rpl_ext_ptr, flm_scrub,
		priority);

	free(fd);

	return 0;
}

static int setup_flow_flm_actions(struct flow_eth_dev *dev __rte_unused,
	const struct nic_flow_def *fd __rte_unused,
	const struct hw_db_inline_qsl_data *qsl_data __rte_unused,
	const struct hw_db_inline_hsh_data *hsh_data __rte_unused,
	uint32_t group __rte_unused,
	uint32_t local_idxs[] __rte_unused,
	uint32_t *local_idx_counter __rte_unused,
	uint16_t *flm_rpl_ext_ptr __rte_unused,
	uint32_t *flm_ft __rte_unused,
	uint32_t *flm_scrub __rte_unused,
	struct rte_flow_error *error __rte_unused)
{
	return 0;
}

static struct flow_handle *create_flow_filter(struct flow_eth_dev *dev, struct nic_flow_def *fd,
	const struct rte_flow_attr *attr,
	uint16_t forced_vlan_vid __rte_unused, uint16_t caller_id,
	struct rte_flow_error *error, uint32_t port_id,
	uint32_t num_dest_port __rte_unused, uint32_t num_queues __rte_unused,
	uint32_t *packet_data __rte_unused, uint32_t *packet_mask __rte_unused,
	struct flm_flow_key_def_s *key_def __rte_unused)
{
	struct flow_handle *fh = calloc(1, sizeof(struct flow_handle));

	fh->type = FLOW_HANDLE_TYPE_FLOW;
	fh->port_id = port_id;
	fh->dev = dev;
	fh->fd = fd;
	fh->caller_id = caller_id;

	struct hw_db_inline_qsl_data qsl_data;

	struct hw_db_inline_hsh_data hsh_data;

	if (attr->group > 0 && fd_has_empty_pattern(fd)) {
		/*
		 * Default flow for group 1..32
		 */

		if (setup_flow_flm_actions(dev, fd, &qsl_data, &hsh_data, attr->group, fh->db_idxs,
			&fh->db_idx_counter, NULL, NULL, NULL, error)) {
			goto error_out;
		}

		nic_insert_flow(dev->ndev, fh);

	} else if (attr->group > 0) {
		/*
		 * Flow for group 1..32
		 */

		/* Setup Actions */
		uint16_t flm_rpl_ext_ptr = 0;
		uint32_t flm_ft = 0;
		uint32_t flm_scrub = 0;

		if (setup_flow_flm_actions(dev, fd, &qsl_data, &hsh_data, attr->group, fh->db_idxs,
			&fh->db_idx_counter, &flm_rpl_ext_ptr, &flm_ft,
			&flm_scrub, error)) {
			goto error_out;
		}

		/* Program flow */
		convert_fh_to_fh_flm(fh, packet_data, 2, flm_ft, flm_rpl_ext_ptr,
			flm_scrub, attr->priority & 0x3);
		flm_flow_programming(fh, NT_FLM_OP_LEARN);

		nic_insert_flow_flm(dev->ndev, fh);

	} else {
		/*
		 * Flow for group 0
		 */
		nic_insert_flow(dev->ndev, fh);
	}

	return fh;

error_out:

	if (fh->type == FLOW_HANDLE_TYPE_FLM) {
		hw_db_inline_deref_idxs(dev->ndev, dev->ndev->hw_db_handle,
			(struct hw_db_idx *)fh->flm_db_idxs,
			fh->flm_db_idx_counter);

	} else {
		hw_db_inline_deref_idxs(dev->ndev, dev->ndev->hw_db_handle,
			(struct hw_db_idx *)fh->db_idxs, fh->db_idx_counter);
	}

	free(fh);

	return NULL;
}

/*
 * Public functions
 */

int initialize_flow_management_of_ndev_profile_inline(struct flow_nic_dev *ndev)
{
	if (!ndev->flow_mgnt_prepared) {
		/* Check static arrays are big enough */
		assert(ndev->be.tpe.nb_cpy_writers <= MAX_CPY_WRITERS_SUPPORTED);

		ndev->id_table_handle = ntnic_id_table_create();

		if (ndev->id_table_handle == NULL)
			goto err_exit0;

		if (flow_group_handle_create(&ndev->group_handle, ndev->be.flm.nb_categories))
			goto err_exit0;

		if (hw_db_inline_create(ndev, &ndev->hw_db_handle))
			goto err_exit0;

		ndev->flow_mgnt_prepared = 1;
	}

	return 0;

err_exit0:
	done_flow_management_of_ndev_profile_inline(ndev);
	return -1;
}

int done_flow_management_of_ndev_profile_inline(struct flow_nic_dev *ndev)
{
#ifdef FLOW_DEBUG
	ndev->be.iface->set_debug_mode(ndev->be.be_dev, FLOW_BACKEND_DEBUG_MODE_WRITE);
#endif

	if (ndev->flow_mgnt_prepared) {
		flow_nic_free_resource(ndev, RES_KM_FLOW_TYPE, 0);
		flow_nic_free_resource(ndev, RES_KM_CATEGORY, 0);

		flow_group_handle_destroy(&ndev->group_handle);
		ntnic_id_table_destroy(ndev->id_table_handle);

		flow_nic_free_resource(ndev, RES_CAT_CFN, 0);

		hw_mod_tpe_reset(&ndev->be);
		flow_nic_free_resource(ndev, RES_TPE_RCP, 0);
		flow_nic_free_resource(ndev, RES_TPE_EXT, 0);
		flow_nic_free_resource(ndev, RES_TPE_RPL, 0);

		hw_db_inline_destroy(ndev->hw_db_handle);

#ifdef FLOW_DEBUG
		ndev->be.iface->set_debug_mode(ndev->be.be_dev, FLOW_BACKEND_DEBUG_MODE_NONE);
#endif

		ndev->flow_mgnt_prepared = 0;
	}

	return 0;
}

struct flow_handle *flow_create_profile_inline(struct flow_eth_dev *dev __rte_unused,
	const struct rte_flow_attr *attr __rte_unused,
	uint16_t forced_vlan_vid __rte_unused,
	uint16_t caller_id __rte_unused,
	const struct rte_flow_item elem[] __rte_unused,
	const struct rte_flow_action action[] __rte_unused,
	struct rte_flow_error *error __rte_unused)
{
	struct flow_handle *fh = NULL;
	int res;

	uint32_t port_id = UINT32_MAX;
	uint32_t num_dest_port;
	uint32_t num_queues;

	uint32_t packet_data[10];
	uint32_t packet_mask[10];
	struct flm_flow_key_def_s key_def;

	struct rte_flow_attr attr_local;
	memcpy(&attr_local, attr, sizeof(struct rte_flow_attr));
	uint16_t forced_vlan_vid_local = forced_vlan_vid;
	uint16_t caller_id_local = caller_id;

	if (attr_local.group > 0)
		forced_vlan_vid_local = 0;

	flow_nic_set_error(ERR_SUCCESS, error);

	struct nic_flow_def *fd = allocate_nic_flow_def();

	if (fd == NULL)
		goto err_exit;

	res = interpret_flow_actions(dev, action, NULL, fd, error, &num_dest_port, &num_queues);

	if (res)
		goto err_exit;

	res = interpret_flow_elements(dev, elem, fd, error, forced_vlan_vid_local, &port_id,
		packet_data, packet_mask, &key_def);

	if (res)
		goto err_exit;

	pthread_mutex_lock(&dev->ndev->mtx);

	/* Translate group IDs */
	if (fd->jump_to_group != UINT32_MAX &&
		flow_group_translate_get(dev->ndev->group_handle, caller_id_local, dev->port,
		fd->jump_to_group, &fd->jump_to_group)) {
		NT_LOG(ERR, FILTER, "ERROR: Could not get group resource");
		flow_nic_set_error(ERR_MATCH_RESOURCE_EXHAUSTION, error);
		goto err_exit;
	}

	if (attr_local.group > 0 &&
		flow_group_translate_get(dev->ndev->group_handle, caller_id_local, dev->port,
		attr_local.group, &attr_local.group)) {
		NT_LOG(ERR, FILTER, "ERROR: Could not get group resource");
		flow_nic_set_error(ERR_MATCH_RESOURCE_EXHAUSTION, error);
		goto err_exit;
	}

	if (port_id == UINT32_MAX)
		port_id = dev->port_id;

	/* Create and flush filter to NIC */
	fh = create_flow_filter(dev, fd, &attr_local, forced_vlan_vid_local,
		caller_id_local, error, port_id, num_dest_port, num_queues, packet_data,
		packet_mask, &key_def);

	if (!fh)
		goto err_exit;

	NT_LOG(DBG, FILTER, "New FlOW: fh (flow handle) %p, fd (flow definition) %p", fh, fd);
	NT_LOG(DBG, FILTER, ">>>>> [Dev %p] Nic %i, Port %i: fh %p fd %p - implementation <<<<<",
		dev, dev->ndev->adapter_no, dev->port, fh, fd);

	pthread_mutex_unlock(&dev->ndev->mtx);

	return fh;

err_exit:

	if (fh)
		flow_destroy_locked_profile_inline(dev, fh, NULL);

	else
		free(fd);

	pthread_mutex_unlock(&dev->ndev->mtx);

	NT_LOG(ERR, FILTER, "ERR: %s", __func__);
	return NULL;
}

int flow_destroy_locked_profile_inline(struct flow_eth_dev *dev,
	struct flow_handle *fh,
	struct rte_flow_error *error)
{
	assert(dev);
	assert(fh);

	int err = 0;

	flow_nic_set_error(ERR_SUCCESS, error);

	/* take flow out of ndev list - may not have been put there yet */
	if (fh->type == FLOW_HANDLE_TYPE_FLM)
		nic_remove_flow_flm(dev->ndev, fh);

	else
		nic_remove_flow(dev->ndev, fh);

#ifdef FLOW_DEBUG
	dev->ndev->be.iface->set_debug_mode(dev->ndev->be.be_dev, FLOW_BACKEND_DEBUG_MODE_WRITE);
#endif

	NT_LOG(DBG, FILTER, "removing flow :%p", fh);
	if (fh->type == FLOW_HANDLE_TYPE_FLM) {
		hw_db_inline_deref_idxs(dev->ndev, dev->ndev->hw_db_handle,
			(struct hw_db_idx *)fh->flm_db_idxs,
			fh->flm_db_idx_counter);

		flm_flow_programming(fh, NT_FLM_OP_UNLEARN);

	} else {
		NT_LOG(DBG, FILTER, "removing flow :%p", fh);

		hw_db_inline_deref_idxs(dev->ndev, dev->ndev->hw_db_handle,
			(struct hw_db_idx *)fh->db_idxs, fh->db_idx_counter);
		free(fh->fd);
	}

	if (err) {
		NT_LOG(ERR, FILTER, "FAILED removing flow: %p", fh);
		flow_nic_set_error(ERR_REMOVE_FLOW_FAILED, error);
	}

	free(fh);

#ifdef FLOW_DEBUG
	dev->ndev->be.iface->set_debug_mode(dev->ndev->be.be_dev, FLOW_BACKEND_DEBUG_MODE_NONE);
#endif

	return err;
}

int flow_destroy_profile_inline(struct flow_eth_dev *dev, struct flow_handle *flow,
	struct rte_flow_error *error)
{
	int err = 0;

	flow_nic_set_error(ERR_SUCCESS, error);

	if (flow) {
		/* Delete this flow */
		pthread_mutex_lock(&dev->ndev->mtx);
		err = flow_destroy_locked_profile_inline(dev, flow, error);
		pthread_mutex_unlock(&dev->ndev->mtx);
	}

	return err;
}

static const struct profile_inline_ops ops = {
	/*
	 * Management
	 */
	.done_flow_management_of_ndev_profile_inline = done_flow_management_of_ndev_profile_inline,
	.initialize_flow_management_of_ndev_profile_inline =
		initialize_flow_management_of_ndev_profile_inline,
	/*
	 * Flow functionality
	 */
	.flow_destroy_locked_profile_inline = flow_destroy_locked_profile_inline,
	.flow_create_profile_inline = flow_create_profile_inline,
	.flow_destroy_profile_inline = flow_destroy_profile_inline,
};

void profile_inline_init(void)
{
	register_profile_inline_ops(&ops);
}