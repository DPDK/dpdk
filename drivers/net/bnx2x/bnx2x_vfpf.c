/*
 * Copyright (c) 2013-2015 Brocade Communications Systems, Inc.
 *
 * All rights reserved.
 */

#include "bnx2x.h"

/* calculate the crc in the bulletin board */
static inline uint32_t
bnx2x_vf_crc(struct bnx2x_vf_bulletin *bull)
{
	uint32_t crc_sz = sizeof(bull->crc), length = bull->length - crc_sz;

	return ECORE_CRC32_LE(0, (uint8_t *)bull + crc_sz, length);
}

/* Checks are there mac/channel updates for VF
 * returns TRUE if something was updated
*/
int
bnx2x_check_bull(struct bnx2x_softc *sc)
{
	struct bnx2x_vf_bulletin *bull;
	uint8_t tries = 0;
	uint16_t old_version = sc->old_bulletin.version;
	uint64_t valid_bitmap;

	bull = sc->pf2vf_bulletin;
	if (old_version == bull->version) {
		return FALSE;
	} else {
		/* Check the crc until we get the correct data */
		while (tries < BNX2X_VF_BULLETIN_TRIES) {
			bull = sc->pf2vf_bulletin;
			if (bull->crc == bnx2x_vf_crc(bull))
				break;

			PMD_DRV_LOG(ERR, "bad crc on bulletin board. contained %x computed %x",
					bull->crc, bnx2x_vf_crc(bull));
			++tries;
		}
		if (tries == BNX2X_VF_BULLETIN_TRIES) {
			PMD_DRV_LOG(ERR, "pf to vf bulletin board crc was wrong %d consecutive times. Aborting",
					tries);
			return FALSE;
		}
	}

	valid_bitmap = bull->valid_bitmap;

	/* check the mac address and VLAN and allocate memory if valid */
	if (valid_bitmap & (1 << MAC_ADDR_VALID) && memcmp(bull->mac, sc->old_bulletin.mac, ETH_ALEN))
		rte_memcpy(&sc->link_params.mac_addr, bull->mac, ETH_ALEN);
	if (valid_bitmap & (1 << VLAN_VALID))
		rte_memcpy(&bull->vlan, &sc->old_bulletin.vlan, VLAN_HLEN);

	sc->old_bulletin = *bull;

	return TRUE;
}

/* add tlv to a buffer */
#define BNX2X_TLV_APPEND(_tlvs, _offset, _type, _length) \
	((struct vf_first_tlv *)((uint64_t)_tlvs + _offset))->type   = _type; \
	((struct vf_first_tlv *)((uint64_t)_tlvs + _offset))->length = _length

/* Initiliaze header of the first tlv and clear mailbox*/
static void
bnx2x_init_first_tlv(struct bnx2x_softc *sc, struct vf_first_tlv *tlv,
	uint16_t type, uint16_t len)
{
	struct bnx2x_vf_mbx_msg *mbox = sc->vf2pf_mbox;
	PMD_DRV_LOG(DEBUG, "Preparing %d tlv for sending", type);

	memset(mbox, 0, sizeof(struct bnx2x_vf_mbx_msg));

	BNX2X_TLV_APPEND(tlv, 0, type, len);

	/* Initialize header of the first tlv */
	tlv->reply_offset = sizeof(mbox->query);
}

#define BNX2X_VF_CMD_ADDR_LO PXP_VF_ADDR_CSDM_GLOBAL_START
#define BNX2X_VF_CMD_ADDR_HI BNX2X_VF_CMD_ADDR_LO + 4
#define BNX2X_VF_CMD_TRIGGER BNX2X_VF_CMD_ADDR_HI + 4
#define BNX2X_VF_CHANNEL_DELAY 100
#define BNX2X_VF_CHANNEL_TRIES 100

static int
bnx2x_do_req4pf(struct bnx2x_softc *sc, phys_addr_t phys_addr)
{
	uint8_t *status = &sc->vf2pf_mbox->resp.common_reply.status;
	uint8_t i;

	if (!*status) {
		bnx2x_check_bull(sc);
		if (sc->old_bulletin.valid_bitmap & (1 << CHANNEL_DOWN)) {
			PMD_DRV_LOG(ERR, "channel is down. Aborting message sending");
			*status = BNX2X_VF_STATUS_SUCCESS;
			return 0;
		}

		REG_WR(sc, BNX2X_VF_CMD_ADDR_LO, U64_LO(phys_addr));
		REG_WR(sc, BNX2X_VF_CMD_ADDR_HI, U64_HI(phys_addr));

		/* memory barrier to ensure that FW can read phys_addr */
		wmb();

		REG_WR8(sc, BNX2X_VF_CMD_TRIGGER, 1);

		/* Do several attempts until PF completes
		 * "." is used to show progress
		 */
		for (i = 0; i < BNX2X_VF_CHANNEL_TRIES; i++) {
			DELAY_MS(BNX2X_VF_CHANNEL_DELAY);
			if (*status)
				break;
		}

		if (i == BNX2X_VF_CHANNEL_TRIES) {
			PMD_DRV_LOG(ERR, "Response from PF timed out");
			return -EAGAIN;
		}

		if (BNX2X_VF_STATUS_SUCCESS != *status) {
			PMD_DRV_LOG(ERR, "Bad reply from PF : %u",
					*status);
			return -EINVAL;
		}
	} else {
		PMD_DRV_LOG(ERR, "status should be zero before message"
				"to pf was sent");
		return -EINVAL;
	}

	PMD_DRV_LOG(DEBUG, "Response from PF was received");
	return 0;
}

static inline uint16_t bnx2x_check_me_flags(uint32_t val)
{
	if (((val) & ME_REG_VF_VALID) && (!((val) & ME_REG_VF_ERR)))
		return ME_REG_VF_VALID;
	else
		return 0;
}

#define BNX2X_ME_ANSWER_DELAY 100
#define BNX2X_ME_ANSWER_TRIES 10

static inline int bnx2x_read_vf_id(struct bnx2x_softc *sc)
{
	uint32_t val;
	uint8_t i = 0;

	while (i <= BNX2X_ME_ANSWER_TRIES) {
		val = BNX2X_DB_READ(DOORBELL_ADDR(sc, 0));
		if (bnx2x_check_me_flags(val))
			return VF_ID(val);

		DELAY_MS(BNX2X_ME_ANSWER_DELAY);
		i++;
	}

	return -EINVAL;
}

#define BNX2X_VF_OBTAIN_MAX_TRIES 3
#define BNX2X_VF_OBTAIN_MAC_FILTERS 1
#define BNX2X_VF_OBTAIN_MC_FILTERS 10

struct bnx2x_obtain_status {
	int success;
	int err_code;
};

static
struct bnx2x_obtain_status bnx2x_loop_obtain_resources(struct bnx2x_softc *sc)
{
	int tries = 0;
	struct vf_acquire_resp_tlv *resp = &sc->vf2pf_mbox->resp.acquire_resp,
								 *sc_resp = &sc->acquire_resp;
	struct vf_resource_query    *res_query;
	struct vf_resc            *resc;
	struct bnx2x_obtain_status     status;
	int res_obtained = false;

	do {
		PMD_DRV_LOG(DEBUG, "trying to get resources");

		if ( bnx2x_do_req4pf(sc, sc->vf2pf_mbox_mapping.paddr) ) {
			/* timeout */
			status.success = 0;
			status.err_code = 0;
			return status;
		}

		memcpy(sc_resp, resp, sizeof(sc->acquire_resp));

		tries++;

		/* check PF to request acceptance */
		if (sc_resp->status == BNX2X_VF_STATUS_SUCCESS) {
			PMD_DRV_LOG(DEBUG, "resources obtained successfully");
			res_obtained = true;
		} else if (sc_resp->status == BNX2X_VF_STATUS_NO_RESOURCES &&
			tries < BNX2X_VF_OBTAIN_MAX_TRIES) {
			PMD_DRV_LOG(DEBUG,
			   "PF cannot allocate requested amount of resources");

			res_query = &sc->vf2pf_mbox->query[0].acquire.res_query;
			resc     = &sc_resp->resc;

			/* PF refused our request. Try to decrease request params */
			res_query->num_txqs         = min(res_query->num_txqs, resc->num_txqs);
			res_query->num_rxqs         = min(res_query->num_rxqs, resc->num_rxqs);
			res_query->num_sbs          = min(res_query->num_sbs, resc->num_sbs);
			res_query->num_mac_filters  = min(res_query->num_mac_filters, resc->num_mac_filters);
			res_query->num_vlan_filters = min(res_query->num_vlan_filters, resc->num_vlan_filters);
			res_query->num_mc_filters   = min(res_query->num_mc_filters, resc->num_mc_filters);

			memset(&sc->vf2pf_mbox->resp, 0, sizeof(union resp_tlvs));
		} else {
			PMD_DRV_LOG(ERR, "Resources cannot be obtained. Status of handling: %d. Aborting",
					sc_resp->status);
			status.success = 0;
			status.err_code = -EAGAIN;
			return status;
		}
	} while (!res_obtained);

	status.success = 1;
	return status;
}

int bnx2x_vf_get_resources(struct bnx2x_softc *sc, uint8_t tx_count, uint8_t rx_count)
{
	struct vf_acquire_tlv *acq = &sc->vf2pf_mbox->query[0].acquire;
	int vf_id;
	struct bnx2x_obtain_status obtain_status;

	bnx2x_vf_close(sc);
	bnx2x_init_first_tlv(sc, &acq->first_tlv, BNX2X_VF_TLV_ACQUIRE, sizeof(*acq));

	vf_id = bnx2x_read_vf_id(sc);
	if (vf_id < 0)
		return -EAGAIN;

	acq->vf_id = vf_id;

	acq->res_query.num_rxqs = rx_count;
	acq->res_query.num_txqs = tx_count;
	acq->res_query.num_sbs = sc->igu_sb_cnt;
	acq->res_query.num_mac_filters = BNX2X_VF_OBTAIN_MAC_FILTERS;
	acq->res_query.num_mc_filters = BNX2X_VF_OBTAIN_MC_FILTERS;

	acq->bulletin_addr = sc->pf2vf_bulletin_mapping.paddr;

	BNX2X_TLV_APPEND(acq, acq->first_tlv.length, BNX2X_VF_TLV_LIST_END,
			sizeof(struct channel_list_end_tlv));

	/* requesting the resources in loop */
	obtain_status = bnx2x_loop_obtain_resources(sc);
	if (!obtain_status.success)
		return obtain_status.err_code;

	struct vf_acquire_resp_tlv sc_resp = sc->acquire_resp;

	sc->devinfo.chip_id        |= (sc_resp.chip_num & 0xFFFF);
	sc->devinfo.int_block       = INT_BLOCK_IGU;
	sc->devinfo.chip_port_mode  = CHIP_2_PORT_MODE;
	sc->devinfo.mf_info.mf_ov   = 0;
	sc->devinfo.mf_info.mf_mode = 0;
	sc->devinfo.flash_size      = 0;

	sc->igu_sb_cnt  = sc_resp.resc.num_sbs;
	sc->igu_base_sb = sc_resp.resc.hw_sbs[0] & 0xFF;
	sc->igu_dsb_id  = -1;

	sc->link_params.chip_id = sc->devinfo.chip_id;
	sc->doorbell_size = sc_resp.db_size;
	sc->flags |= BNX2X_NO_WOL_FLAG | BNX2X_NO_ISCSI_OOO_FLAG | BNX2X_NO_ISCSI_FLAG | BNX2X_NO_FCOE_FLAG;

	PMD_DRV_LOG(DEBUG, "status block count = %d, base status block = %x",
		sc->igu_sb_cnt, sc->igu_base_sb);
	strncpy(sc->fw_ver, sc_resp.fw_ver, sizeof(sc->fw_ver));

	if (is_valid_ether_addr(sc_resp.resc.current_mac_addr))
		(void)rte_memcpy(sc->link_params.mac_addr,
		       sc_resp.resc.current_mac_addr,
		       ETH_ALEN);

	return 0;
}

/* Ask PF to release VF's resources */
void
bnx2x_vf_close(struct bnx2x_softc *sc)
{
	struct vf_release_tlv *query;
	int vf_id = bnx2x_read_vf_id(sc);
	int ret;

	if (vf_id >= 0) {
		query = &sc->vf2pf_mbox->query[0].release;
		bnx2x_init_first_tlv(sc, &query->first_tlv, BNX2X_VF_TLV_RELEASE,
				sizeof(*query));

		query->vf_id = vf_id;
		BNX2X_TLV_APPEND(query, query->first_tlv.length, BNX2X_VF_TLV_LIST_END,
				sizeof(struct channel_list_end_tlv));

		ret = bnx2x_do_req4pf(sc, sc->vf2pf_mbox_mapping.paddr);

		if (ret) {
			PMD_DRV_LOG(ERR, "Failed to release VF");
		}
	}
}

/* Let PF know the VF status blocks phys_addrs */
int
bnx2x_vf_init(struct bnx2x_softc *sc)
{
	struct vf_init_tlv *query;
	int i, ret;

	query = &sc->vf2pf_mbox->query[0].init;
	bnx2x_init_first_tlv(sc, &query->first_tlv, BNX2X_VF_TLV_INIT,
			sizeof(*query));

	FOR_EACH_QUEUE(sc, i) {
		query->sb_addr[i] = (unsigned long)(sc->fp[i].sb_dma.paddr);
	}

	query->stats_step = sizeof(struct per_queue_stats);
	query->stats_addr = sc->fw_stats_data_mapping +
		offsetof(struct bnx2x_fw_stats_data, queue_stats);

	BNX2X_TLV_APPEND(query, query->first_tlv.length, BNX2X_VF_TLV_LIST_END,
			sizeof(struct channel_list_end_tlv));

	ret = bnx2x_do_req4pf(sc, sc->vf2pf_mbox_mapping.paddr);

	if (ret) {
		PMD_DRV_LOG(ERR, "Failed to init VF");
		return ret;
	}

	PMD_DRV_LOG(DEBUG, "VF was initialized");
	return 0;
}

void
bnx2x_vf_unload(struct bnx2x_softc *sc)
{
	struct vf_close_tlv *query;
	struct vf_q_op_tlv *query_op;
	int i, vf_id, ret;

	vf_id = bnx2x_read_vf_id(sc);
	if (vf_id > 0) {
		FOR_EACH_QUEUE(sc, i) {
			query_op = &sc->vf2pf_mbox->query[0].q_op;
			bnx2x_init_first_tlv(sc, &query_op->first_tlv,
					BNX2X_VF_TLV_TEARDOWN_Q,
					sizeof(*query_op));

			query_op->vf_qid = i;

			BNX2X_TLV_APPEND(query_op, query_op->first_tlv.length,
					BNX2X_VF_TLV_LIST_END,
					sizeof(struct channel_list_end_tlv));

			ret = bnx2x_do_req4pf(sc, sc->vf2pf_mbox_mapping.paddr);
			if (ret)
				PMD_DRV_LOG(ERR,
						"Bad reply for vf_q %d teardown", i);
		}

		bnx2x_vf_set_mac(sc, false);

		query = &sc->vf2pf_mbox->query[0].close;
		bnx2x_init_first_tlv(sc, &query->first_tlv, BNX2X_VF_TLV_CLOSE,
				sizeof(*query));

		query->vf_id = vf_id;

		BNX2X_TLV_APPEND(query, query->first_tlv.length,
				BNX2X_VF_TLV_LIST_END,
				sizeof(struct channel_list_end_tlv));

		ret = bnx2x_do_req4pf(sc, sc->vf2pf_mbox_mapping.paddr);

		if (ret)
			PMD_DRV_LOG(ERR,
				"Bad reply from PF for close message");
	}
}

static inline uint16_t
bnx2x_vf_q_flags(uint8_t leading)
{
	uint16_t flags = leading ? BNX2X_VF_Q_FLAG_LEADING_RSS : 0;

	flags |= BNX2X_VF_Q_FLAG_CACHE_ALIGN;
	flags |= BNX2X_VF_Q_FLAG_STATS;
	flags |= BNX2X_VF_Q_FLAG_VLAN;

	return flags;
}

static void
bnx2x_vf_rx_q_prep(struct bnx2x_softc *sc, struct bnx2x_fastpath *fp,
		struct vf_rxq_params *rxq_init, uint16_t flags)
{
	struct bnx2x_rx_queue *rxq;

	rxq = sc->rx_queues[fp->index];
	if (!rxq) {
		PMD_DRV_LOG(ERR, "RX queue %d is NULL", fp->index);
		return;
	}

	rxq_init->rcq_addr = rxq->cq_ring_phys_addr;
	rxq_init->rcq_np_addr = rxq->cq_ring_phys_addr + BNX2X_PAGE_SIZE;
	rxq_init->rxq_addr = rxq->rx_ring_phys_addr;
	rxq_init->vf_sb_id = fp->index;
	rxq_init->sb_cq_index = HC_INDEX_ETH_RX_CQ_CONS;
	rxq_init->mtu = sc->mtu;
	rxq_init->buf_sz = fp->rx_buf_size;
	rxq_init->flags = flags;
	rxq_init->stat_id = -1;
	rxq_init->cache_line_log = BNX2X_RX_ALIGN_SHIFT;
}

static void
bnx2x_vf_tx_q_prep(struct bnx2x_softc *sc, struct bnx2x_fastpath *fp,
		struct vf_txq_params *txq_init, uint16_t flags)
{
	struct bnx2x_tx_queue *txq;

	txq = sc->tx_queues[fp->index];
	if (!txq) {
		PMD_DRV_LOG(ERR, "TX queue %d is NULL", fp->index);
		return;
	}

	txq_init->txq_addr = txq->tx_ring_phys_addr;
	txq_init->sb_index = HC_INDEX_ETH_TX_CQ_CONS_COS0;
	txq_init->flags = flags;
	txq_init->traffic_type = LLFC_TRAFFIC_TYPE_NW;
	txq_init->vf_sb_id = fp->index;
}

int
bnx2x_vf_setup_queue(struct bnx2x_softc *sc, struct bnx2x_fastpath *fp, int leading)
{
	struct vf_setup_q_tlv *query;
	uint16_t flags = bnx2x_vf_q_flags(leading);
	int ret;

	query = &sc->vf2pf_mbox->query[0].setup_q;
	bnx2x_init_first_tlv(sc, &query->first_tlv, BNX2X_VF_TLV_SETUP_Q,
			sizeof(*query));

	query->vf_qid = fp->index;
	query->param_valid = VF_RXQ_VALID | VF_TXQ_VALID;

	bnx2x_vf_rx_q_prep(sc, fp, &query->rxq, flags);
	bnx2x_vf_tx_q_prep(sc, fp, &query->txq, flags);

	BNX2X_TLV_APPEND(query, query->first_tlv.length, BNX2X_VF_TLV_LIST_END,
			sizeof(struct channel_list_end_tlv));

	ret = bnx2x_do_req4pf(sc, sc->vf2pf_mbox_mapping.paddr);

	if (ret) {
		PMD_DRV_LOG(ERR, "Failed to setup VF queue[%d]",
				fp->index);
		return -EINVAL;
	}

	return 0;
}

int
bnx2x_vf_set_mac(struct bnx2x_softc *sc, int set)
{
	struct vf_set_q_filters_tlv *query;
	struct vf_common_reply_tlv *reply;

	query = &sc->vf2pf_mbox->query[0].set_q_filters;
	bnx2x_init_first_tlv(sc, &query->first_tlv, BNX2X_VF_TLV_SET_Q_FILTERS,
			sizeof(*query));

	query->vf_qid = sc->fp->index;
	query->mac_filters_cnt = 1;
	query->flags = BNX2X_VF_MAC_VLAN_CHANGED;

	query->filters[0].flags = (set ? BNX2X_VF_Q_FILTER_SET_MAC : 0) |
		BNX2X_VF_Q_FILTER_DEST_MAC_VALID;

	bnx2x_check_bull(sc);

	rte_memcpy(query->filters[0].mac, sc->link_params.mac_addr, ETH_ALEN);

	BNX2X_TLV_APPEND(query, query->first_tlv.length, BNX2X_VF_TLV_LIST_END,
			sizeof(struct channel_list_end_tlv));

	bnx2x_do_req4pf(sc, sc->vf2pf_mbox_mapping.paddr);
	reply = &sc->vf2pf_mbox->resp.common_reply;

	while (BNX2X_VF_STATUS_FAILURE == reply->status &&
			bnx2x_check_bull(sc)) {
		/* A new mac was configured by PF for us */
		rte_memcpy(sc->link_params.mac_addr, sc->pf2vf_bulletin->mac,
				ETH_ALEN);
		rte_memcpy(query->filters[0].mac, sc->pf2vf_bulletin->mac,
				ETH_ALEN);

		bnx2x_do_req4pf(sc, sc->vf2pf_mbox_mapping.paddr);
	}

	if (BNX2X_VF_STATUS_SUCCESS != reply->status) {
		PMD_DRV_LOG(ERR, "Bad reply from PF for SET MAC message: %d",
				reply->status);
		return -EINVAL;
	}

	return 0;
}

int
bnx2x_vf_config_rss(struct bnx2x_softc *sc,
			  struct ecore_config_rss_params *params)
{
	struct vf_rss_tlv *query;
	int ret;

	query = &sc->vf2pf_mbox->query[0].update_rss;

	bnx2x_init_first_tlv(sc, &query->first_tlv, BNX2X_VF_TLV_UPDATE_RSS,
			sizeof(*query));

	/* add list termination tlv */
	BNX2X_TLV_APPEND(query, query->first_tlv.length, BNX2X_VF_TLV_LIST_END,
			sizeof(struct channel_list_end_tlv));

	rte_memcpy(query->rss_key, params->rss_key, sizeof(params->rss_key));
	query->rss_key_size = T_ETH_RSS_KEY;

	rte_memcpy(query->ind_table, params->ind_table, T_ETH_INDIRECTION_TABLE_SIZE);
	query->ind_table_size = T_ETH_INDIRECTION_TABLE_SIZE;

	query->rss_result_mask = params->rss_result_mask;
	query->rss_flags = params->rss_flags;

	ret = bnx2x_do_req4pf(sc, sc->vf2pf_mbox_mapping.paddr);
	if (ret) {
		PMD_DRV_LOG(ERR, "Failed to send message to PF, rc %d", ret);
		return ret;
	}

	return 0;
}

int
bnx2x_vf_set_rx_mode(struct bnx2x_softc *sc)
{
	struct vf_set_q_filters_tlv *query;
	unsigned long tx_mask;
	int ret;

	query = &sc->vf2pf_mbox->query[0].set_q_filters;
	bnx2x_init_first_tlv(sc, &query->first_tlv, BNX2X_VF_TLV_SET_Q_FILTERS,
			sizeof(*query));

	query->vf_qid = 0;
	query->flags = BNX2X_VF_RX_MASK_CHANGED;

	if (bnx2x_fill_accept_flags(sc, sc->rx_mode, &query->rx_mask, &tx_mask)) {
		return -EINVAL;
	}

	BNX2X_TLV_APPEND(query, query->first_tlv.length, BNX2X_VF_TLV_LIST_END,
			sizeof(struct channel_list_end_tlv));

	ret = bnx2x_do_req4pf(sc, sc->vf2pf_mbox_mapping.paddr);
	if (ret) {
		PMD_DRV_LOG(ERR, "Failed to send message to PF, rc %d", ret);
		return ret;
	}

	return 0;
}
