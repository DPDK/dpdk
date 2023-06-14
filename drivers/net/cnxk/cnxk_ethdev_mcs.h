/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Marvell.
 */

#ifndef CNXK_ETHDEV_MCS_H
#define CNXK_ETHDEV_MCS_H

#include <cnxk_ethdev.h>

#define CNXK_MACSEC_HASH_KEY 16

struct cnxk_mcs_dev {
	uint64_t default_sci;
	void *mdev;
	uint8_t port_id;
	uint8_t idx;
};

enum cnxk_mcs_rsrc_type {
	CNXK_MCS_RSRC_TYPE_FLOWID,
	CNXK_MCS_RSRC_TYPE_SECY,
	CNXK_MCS_RSRC_TYPE_SC,
	CNXK_MCS_RSRC_TYPE_SA,
	CNXK_MCS_RSRC_TYPE_PORT,
};

struct cnxk_mcs_flow_opts {
	uint32_t outer_tag_id;
	/**< {VLAN_ID[11:0]}, or 20-bit MPLS label*/
	uint8_t outer_priority;
	/**< {PCP/Pbits, DE/CFI} or {1'b0, EXP} for MPLS.*/
	uint32_t second_outer_tag_id;
	/**< {VLAN_ID[11:0]}, or 20-bit MPLS label*/
	uint8_t second_outer_priority;
	/**< {PCP/Pbits, DE/CFI} or {1'b0, EXP} for MPLS. */
	uint16_t bonus_data;
	/**< 2 bytes of additional bonus data extracted from one of the custom tags*/
	uint8_t tag_match_bitmap;
	uint8_t packet_type;
	uint8_t outer_vlan_type;
	uint8_t inner_vlan_type;
	uint8_t num_tags;
	bool express;
	uint8_t lmac_id;
	uint8_t flowid_user;
};

struct cnxk_mcs_event_data {
	/* Valid for below events
	 * - ROC_MCS_EVENT_RX_SA_PN_SOFT_EXP
	 * - ROC_MCS_EVENT_TX_SA_PN_SOFT_EXP
	 */
	struct {
		uint8_t secy_idx;
		uint8_t sc_idx;
		uint8_t sa_idx;
	};
	/* Valid for below event
	 * - ROC_MCS_EVENT_FIFO_OVERFLOW
	 *
	 * Upon fatal error notification on a MCS port, driver resets below attributes of active
	 * flow entities(sc & sa) and then resets the port.
	 * - Reset NEXT_PN of active SAs to 1.
	 * - Reset TX active SA for each SC, TX_SA_ACTIVE = 0, SA_INDEX0_VLD = 1.
	 * - Clear SA_IN_USE for active ANs in RX_SA_MAP_MEM.
	 * - Clear all stats mapping to this port.
	 * - Reactivate SA_IN_USE for active ANs in RX_SA_MAP_MEM.
	 *
	 *  UMD driver notifies the following flow entity(sc & sa) details in application callback,
	 *  application is expected to exchange the Tx/Rx NEXT_PN, TX_SA_ACTIVE, active RX SC AN
	 *  details with peer device so that peer device can resets it's MACsec flow states and than
	 *  resume packet transfers.
	 */
	struct {
		uint16_t *tx_sa_array; /* Tx SAs whose PN memories were reset (NEXT_PN=1) */
		uint16_t *rx_sa_array; /* Rx SAs whose PN memories were reset (NEXT_PN=1) */
		uint16_t *tx_sc_array; /* Tx SCs whose active SAs were reset (TX_SA_ACTIVE=0) */
		uint16_t *rx_sc_array; /* Rx SCs whose state was reset */
		uint8_t *sc_an_array;  /* AN of Rx SCs(in rx_sc_array) which were reactivated */
		uint8_t num_tx_sa;     /* num entries in tx_sa_array */
		uint8_t num_rx_sa;     /* num entries in rx_sa_array */
		uint8_t num_tx_sc;     /* num entries in tx_sc_array */
		uint8_t num_rx_sc;     /* num entries in rx_sc_array */
		uint8_t lmac_id;       /* lmac_id/port which was recovered from fatal error */
	};
};

struct cnxk_mcs_event_desc {
	struct rte_eth_dev *eth_dev;
	enum roc_mcs_event_type type;
	enum roc_mcs_event_subtype subtype;
	struct cnxk_mcs_event_data metadata;
};

int cnxk_eth_macsec_sa_create(void *device, struct rte_security_macsec_sa *conf);
int cnxk_eth_macsec_sc_create(void *device, struct rte_security_macsec_sc *conf);

int cnxk_eth_macsec_sa_destroy(void *device, uint16_t sa_id,
			       enum rte_security_macsec_direction dir);
int cnxk_eth_macsec_sc_destroy(void *device, uint16_t sc_id,
			       enum rte_security_macsec_direction dir);

int cnxk_eth_macsec_sa_stats_get(void *device, uint16_t sa_id,
				 enum rte_security_macsec_direction dir,
				 struct rte_security_macsec_sa_stats *stats);
int cnxk_eth_macsec_sc_stats_get(void *device, uint16_t sa_id,
				 enum rte_security_macsec_direction dir,
				 struct rte_security_macsec_sc_stats *stats);
int cnxk_eth_macsec_session_stats_get(struct cnxk_eth_dev *dev, struct cnxk_macsec_sess *sess,
				      struct rte_security_stats *stats);

int cnxk_eth_macsec_session_create(struct cnxk_eth_dev *dev, struct rte_security_session_conf *conf,
				   struct rte_security_session *sess);
int cnxk_eth_macsec_session_destroy(struct cnxk_eth_dev *dev, struct rte_security_session *sess);

#endif /* CNXK_ETHDEV_MCS_H */
