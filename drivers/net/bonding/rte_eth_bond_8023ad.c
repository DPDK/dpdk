/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2015 Intel Corporation
 */

#include <stddef.h>
#include <string.h>
#include <stdbool.h>

#include <rte_alarm.h>
#include <rte_malloc.h>
#include <rte_errno.h>
#include <rte_cycles.h>
#include <rte_compat.h>

#include "eth_bond_private.h"

static void bond_mode_8023ad_ext_periodic_cb(void *arg);
#ifdef RTE_LIBRTE_BOND_DEBUG_8023AD

#define MODE4_DEBUG(fmt, ...)				\
	rte_log(RTE_LOG_DEBUG, bond_logtype,		\
		"%6u [Port %u: %s] " fmt,		\
		bond_dbg_get_time_diff_ms(), slave_id,	\
		__func__, ##__VA_ARGS__)

static uint64_t start_time;

static unsigned
bond_dbg_get_time_diff_ms(void)
{
	uint64_t now;

	now = rte_rdtsc();
	if (start_time == 0)
		start_time = now;

	return ((now - start_time) * 1000) / rte_get_tsc_hz();
}

static void
bond_print_lacp(struct lacpdu *l)
{
	char a_address[18];
	char p_address[18];
	char a_state[256] = { 0 };
	char p_state[256] = { 0 };

	static const char * const state_labels[] = {
		"ACT", "TIMEOUT", "AGG", "SYNC", "COL", "DIST", "DEF", "EXP"
	};

	int a_len = 0;
	int p_len = 0;
	uint8_t i;
	uint8_t *addr;

	addr = l->actor.port_params.system.addr_bytes;
	snprintf(a_address, sizeof(a_address), "%02X:%02X:%02X:%02X:%02X:%02X",
		addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);

	addr = l->partner.port_params.system.addr_bytes;
	snprintf(p_address, sizeof(p_address), "%02X:%02X:%02X:%02X:%02X:%02X",
		addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);

	for (i = 0; i < 8; i++) {
		if ((l->actor.state >> i) & 1) {
			a_len += snprintf(&a_state[a_len], RTE_DIM(a_state) - a_len, "%s ",
				state_labels[i]);
		}

		if ((l->partner.state >> i) & 1) {
			p_len += snprintf(&p_state[p_len], RTE_DIM(p_state) - p_len, "%s ",
				state_labels[i]);
		}
	}

	if (a_len && a_state[a_len-1] == ' ')
		a_state[a_len-1] = '\0';

	if (p_len && p_state[p_len-1] == ' ')
		p_state[p_len-1] = '\0';

	RTE_BOND_LOG(DEBUG,
		     "LACP: {\n"
		     "  subtype= %02X\n"
		     "  ver_num=%02X\n"
		     "  actor={ tlv=%02X, len=%02X\n"
		     "    pri=%04X, system=%s, key=%04X, p_pri=%04X p_num=%04X\n"
		     "       state={ %s }\n"
		     "  }\n"
		     "  partner={ tlv=%02X, len=%02X\n"
		     "    pri=%04X, system=%s, key=%04X, p_pri=%04X p_num=%04X\n"
		     "       state={ %s }\n"
		     "  }\n"
		     "  collector={info=%02X, length=%02X, max_delay=%04X\n, "
		     "type_term=%02X, terminator_length = %02X }",
		     l->subtype,
		     l->version_number,
		     l->actor.tlv_type_info,
		     l->actor.info_length,
		     l->actor.port_params.system_priority,
		     a_address,
		     l->actor.port_params.key,
		     l->actor.port_params.port_priority,
		     l->actor.port_params.port_number,
		     a_state,
		     l->partner.tlv_type_info,
		     l->partner.info_length,
		     l->partner.port_params.system_priority,
		     p_address,
		     l->partner.port_params.key,
		     l->partner.port_params.port_priority,
		     l->partner.port_params.port_number,
		     p_state,
		     l->tlv_type_collector_info,
		     l->collector_info_length,
		     l->collector_max_delay,
		     l->tlv_type_terminator,
		     l->terminator_length);

}

#define BOND_PRINT_LACP(lacpdu) bond_print_lacp(lacpdu)
#else
#define BOND_PRINT_LACP(lacpdu) do { } while (0)
#define MODE4_DEBUG(fmt, ...) do { } while (0)
#endif

static const struct rte_ether_addr lacp_mac_addr = {
	.addr_bytes = { 0x01, 0x80, 0xC2, 0x00, 0x00, 0x02 }
};

struct port bond_mode_8023ad_ports[RTE_MAX_ETHPORTS];

static void
timer_cancel(uint64_t *timer)
{
	*timer = 0;
}

static void
timer_set(uint64_t *timer, uint64_t timeout)
{
	*timer = rte_rdtsc() + timeout;
}

/* Forces given timer to be in expired state. */
static void
timer_force_expired(uint64_t *timer)
{
	*timer = rte_rdtsc();
}

static bool
timer_is_stopped(uint64_t *timer)
{
	return *timer == 0;
}

static bool
timer_is_expired(uint64_t *timer)
{
	return *timer < rte_rdtsc();
}

/* Timer is in running state if it is not stopped nor expired */
static bool
timer_is_running(uint64_t *timer)
{
	return !timer_is_stopped(timer) && !timer_is_expired(timer);
}

static void
set_warning_flags(struct port *port, uint16_t flags)
{
	int retval;
	uint16_t old;
	uint16_t new_flag = 0;

	do {
		old = port->warnings_to_show;
		new_flag = old | flags;
		retval = rte_atomic16_cmpset(&port->warnings_to_show, old, new_flag);
	} while (unlikely(retval == 0));
}

static void
show_warnings(uint16_t slave_id)
{
	struct port *port = &bond_mode_8023ad_ports[slave_id];
	uint8_t warnings;

	do {
		warnings = port->warnings_to_show;
	} while (rte_atomic16_cmpset(&port->warnings_to_show, warnings, 0) == 0);

	if (!warnings)
		return;

	if (!timer_is_expired(&port->warning_timer))
		return;


	timer_set(&port->warning_timer, BOND_8023AD_WARNINGS_PERIOD_MS *
			rte_get_tsc_hz() / 1000);

	if (warnings & WRN_RX_QUEUE_FULL) {
		RTE_BOND_LOG(DEBUG,
			     "Slave %u: failed to enqueue LACP packet into RX ring.\n"
			     "Receive and transmit functions must be invoked on bonded"
			     "interface at least 10 times per second or LACP will notwork correctly",
			     slave_id);
	}

	if (warnings & WRN_TX_QUEUE_FULL) {
		RTE_BOND_LOG(DEBUG,
			     "Slave %u: failed to enqueue LACP packet into TX ring.\n"
			     "Receive and transmit functions must be invoked on bonded"
			     "interface at least 10 times per second or LACP will not work correctly",
			     slave_id);
	}

	if (warnings & WRN_RX_MARKER_TO_FAST)
		RTE_BOND_LOG(INFO, "Slave %u: marker to early - ignoring.",
			     slave_id);

	if (warnings & WRN_UNKNOWN_SLOW_TYPE) {
		RTE_BOND_LOG(INFO,
			"Slave %u: ignoring unknown slow protocol frame type",
			     slave_id);
	}

	if (warnings & WRN_UNKNOWN_MARKER_TYPE)
		RTE_BOND_LOG(INFO, "Slave %u: ignoring unknown marker type",
			     slave_id);

	if (warnings & WRN_NOT_LACP_CAPABLE)
		MODE4_DEBUG("Port %u is not LACP capable!\n", slave_id);
}

static void
record_default(struct port *port)
{
	/* Record default parameters for partner. Partner admin parameters
	 * are not implemented so set them to arbitrary default (last known) and
	 * mark actor that parner is in defaulted state. */
	port->partner_state = STATE_LACP_ACTIVE;
	ACTOR_STATE_SET(port, DEFAULTED);
}

/** Function handles rx state machine.
 *
 * This function implements Receive State Machine from point 5.4.12 in
 * 802.1AX documentation. It should be called periodically.
 *
 * @param lacpdu		LACPDU received.
 * @param port			Port on which LACPDU was received.
 */
static void
rx_machine(struct bond_dev_private *internals, uint16_t slave_id,
		struct lacpdu *lacp)
{
	struct port *agg, *port = &bond_mode_8023ad_ports[slave_id];
	uint64_t timeout;

	if (SM_FLAG(port, BEGIN)) {
		/* Initialize stuff */
		MODE4_DEBUG("-> INITIALIZE\n");
		SM_FLAG_CLR(port, MOVED);
		port->selected = UNSELECTED;

		record_default(port);

		ACTOR_STATE_CLR(port, EXPIRED);
		timer_cancel(&port->current_while_timer);

		/* DISABLED: On initialization partner is out of sync */
		PARTNER_STATE_CLR(port, SYNCHRONIZATION);

		/* LACP DISABLED stuff if LACP not enabled on this port */
		if (!SM_FLAG(port, LACP_ENABLED))
			PARTNER_STATE_CLR(port, AGGREGATION);
		else
			PARTNER_STATE_SET(port, AGGREGATION);
	}

	if (!SM_FLAG(port, LACP_ENABLED)) {
		/* Update parameters only if state changed */
		if (!timer_is_stopped(&port->current_while_timer)) {
			port->selected = UNSELECTED;
			record_default(port);
			PARTNER_STATE_CLR(port, AGGREGATION);
			ACTOR_STATE_CLR(port, EXPIRED);
			timer_cancel(&port->current_while_timer);
		}
		return;
	}

	if (lacp) {
		MODE4_DEBUG("LACP -> CURRENT\n");
		BOND_PRINT_LACP(lacp);
		/* Update selected flag. If partner parameters are defaulted assume they
		 * are match. If not defaulted  compare LACP actor with ports parner
		 * params. */
		if (!ACTOR_STATE(port, DEFAULTED) &&
			(ACTOR_STATE(port, AGGREGATION) != PARTNER_STATE(port, AGGREGATION)
			|| memcmp(&port->partner, &lacp->actor.port_params,
				sizeof(port->partner)) != 0)) {
			MODE4_DEBUG("selected <- UNSELECTED\n");
			port->selected = UNSELECTED;
		}

		/* Record this PDU actor params as partner params */
		memcpy(&port->partner, &lacp->actor.port_params,
			sizeof(struct port_params));
		port->partner_state = lacp->actor.state;

		/* Partner parameters are not defaulted any more */
		ACTOR_STATE_CLR(port, DEFAULTED);

		/* If LACP partner params match this port actor params */
		agg = &bond_mode_8023ad_ports[port->aggregator_port_id];
		bool match = port->actor.system_priority ==
			lacp->partner.port_params.system_priority &&
			rte_is_same_ether_addr(&agg->actor.system,
			&lacp->partner.port_params.system) &&
			port->actor.port_priority ==
			lacp->partner.port_params.port_priority &&
			port->actor.port_number ==
			lacp->partner.port_params.port_number;

		/* Update NTT if partners information are outdated (xored and masked
		 * bits are set)*/
		uint8_t state_mask = STATE_LACP_ACTIVE | STATE_LACP_SHORT_TIMEOUT |
			STATE_SYNCHRONIZATION | STATE_AGGREGATION;

		if (((port->actor_state ^ lacp->partner.state) & state_mask) ||
				match == false) {
			SM_FLAG_SET(port, NTT);
		}

		/* If LACP partner params match this port actor params */
		if (match == true && ACTOR_STATE(port, AGGREGATION) ==
				PARTNER_STATE(port,	AGGREGATION))
			PARTNER_STATE_SET(port, SYNCHRONIZATION);
		else if (!PARTNER_STATE(port, AGGREGATION) && ACTOR_STATE(port,
				AGGREGATION))
			PARTNER_STATE_SET(port, SYNCHRONIZATION);
		else
			PARTNER_STATE_CLR(port, SYNCHRONIZATION);

		if (ACTOR_STATE(port, LACP_SHORT_TIMEOUT))
			timeout = internals->mode4.short_timeout;
		else
			timeout = internals->mode4.long_timeout;

		timer_set(&port->current_while_timer, timeout);
		ACTOR_STATE_CLR(port, EXPIRED);
		SM_FLAG_CLR(port, EXPIRED);
		return; /* No state change */
	}

	/* If CURRENT state timer is not running (stopped or expired)
	 * transit to EXPIRED state from DISABLED or CURRENT */
	if (!timer_is_running(&port->current_while_timer)) {
		if (SM_FLAG(port, EXPIRED)) {
			port->selected = UNSELECTED;
			memcpy(&port->partner, &port->partner_admin,
				sizeof(struct port_params));
			record_default(port);
			ACTOR_STATE_CLR(port, EXPIRED);
			timer_cancel(&port->current_while_timer);
		} else {
			SM_FLAG_SET(port, EXPIRED);
			ACTOR_STATE_SET(port, EXPIRED);
			PARTNER_STATE_CLR(port, SYNCHRONIZATION);
			PARTNER_STATE_SET(port, LACP_SHORT_TIMEOUT);
			timer_set(&port->current_while_timer,
				internals->mode4.short_timeout);
		}
	}
}

/**
 * Function handles periodic tx state machine.
 *
 * Function implements Periodic Transmission state machine from point 5.4.13
 * in 802.1AX documentation. It should be called periodically.
 *
 * @param port			Port to handle state machine.
 */
static void
periodic_machine(struct bond_dev_private *internals, uint16_t slave_id)
{
	struct port *port = &bond_mode_8023ad_ports[slave_id];
	/* Calculate if either site is LACP enabled */
	uint64_t timeout;
	uint8_t active = ACTOR_STATE(port, LACP_ACTIVE) ||
		PARTNER_STATE(port, LACP_ACTIVE);

	uint8_t is_partner_fast, was_partner_fast;
	/* No periodic is on BEGIN, LACP DISABLE or when both sides are pasive */
	if (SM_FLAG(port, BEGIN) || !SM_FLAG(port, LACP_ENABLED) || !active) {
		timer_cancel(&port->periodic_timer);
		timer_force_expired(&port->tx_machine_timer);
		SM_FLAG_CLR(port, PARTNER_SHORT_TIMEOUT);

		MODE4_DEBUG("-> NO_PERIODIC ( %s%s%s)\n",
			SM_FLAG(port, BEGIN) ? "begind " : "",
			SM_FLAG(port, LACP_ENABLED) ? "" : "LACP disabled ",
			active ? "LACP active " : "LACP pasive ");
		return;
	}

	is_partner_fast = PARTNER_STATE(port, LACP_SHORT_TIMEOUT);
	was_partner_fast = SM_FLAG(port, PARTNER_SHORT_TIMEOUT);

	/* If periodic timer is not started, transit from NO PERIODIC to FAST/SLOW.
	 * Other case: check if timer expire or partners settings changed. */
	if (!timer_is_stopped(&port->periodic_timer)) {
		if (timer_is_expired(&port->periodic_timer)) {
			SM_FLAG_SET(port, NTT);
		} else if (is_partner_fast != was_partner_fast) {
			/* Partners timeout  was slow and now it is fast -> send LACP.
			 * In other case (was fast and now it is slow) just switch
			 * timeout to slow without forcing send of LACP (because standard
			 * say so)*/
			if (is_partner_fast)
				SM_FLAG_SET(port, NTT);
		} else
			return; /* Nothing changed */
	}

	/* Handle state transition to FAST/SLOW LACP timeout */
	if (is_partner_fast) {
		timeout = internals->mode4.fast_periodic_timeout;
		SM_FLAG_SET(port, PARTNER_SHORT_TIMEOUT);
	} else {
		timeout = internals->mode4.slow_periodic_timeout;
		SM_FLAG_CLR(port, PARTNER_SHORT_TIMEOUT);
	}

	timer_set(&port->periodic_timer, timeout);
}

/**
 * Function handles mux state machine.
 *
 * Function implements Mux Machine from point 5.4.15 in 802.1AX documentation.
 * It should be called periodically.
 *
 * @param port			Port to handle state machine.
 */
static void
mux_machine(struct bond_dev_private *internals, uint16_t slave_id)
{
	struct port *port = &bond_mode_8023ad_ports[slave_id];

	/* Save current state for later use */
	const uint8_t state_mask = STATE_SYNCHRONIZATION | STATE_DISTRIBUTING |
		STATE_COLLECTING;

	/* Enter DETACHED state on BEGIN condition or from any other state if
	 * port was unselected */
	if (SM_FLAG(port, BEGIN) ||
			port->selected == UNSELECTED || (port->selected == STANDBY &&
				(port->actor_state & state_mask) != 0)) {
		/* detach mux from aggregator */
		port->actor_state &= ~state_mask;
		/* Set ntt to true if BEGIN condition or transition from any other state
		 * which is indicated that wait_while_timer was started */
		if (SM_FLAG(port, BEGIN) ||
				!timer_is_stopped(&port->wait_while_timer)) {
			SM_FLAG_SET(port, NTT);
			MODE4_DEBUG("-> DETACHED\n");
		}
		timer_cancel(&port->wait_while_timer);
	}

	if (timer_is_stopped(&port->wait_while_timer)) {
		if (port->selected == SELECTED || port->selected == STANDBY) {
			timer_set(&port->wait_while_timer,
				internals->mode4.aggregate_wait_timeout);

			MODE4_DEBUG("DETACHED -> WAITING\n");
		}
		/* Waiting state entered */
		return;
	}

	/* Transit next state if port is ready */
	if (!timer_is_expired(&port->wait_while_timer))
		return;

	if ((ACTOR_STATE(port, DISTRIBUTING) || ACTOR_STATE(port, COLLECTING)) &&
		!PARTNER_STATE(port, SYNCHRONIZATION)) {
		/* If in COLLECTING or DISTRIBUTING state and partner becomes out of
		 * sync transit to ATACHED state.  */
		ACTOR_STATE_CLR(port, DISTRIBUTING);
		ACTOR_STATE_CLR(port, COLLECTING);
		/* Clear actor sync to activate transit ATACHED in condition bellow */
		ACTOR_STATE_CLR(port, SYNCHRONIZATION);
		MODE4_DEBUG("Out of sync -> ATTACHED\n");
	}

	if (!ACTOR_STATE(port, SYNCHRONIZATION)) {
		/* attach mux to aggregator */
		RTE_ASSERT((port->actor_state & (STATE_COLLECTING |
			STATE_DISTRIBUTING)) == 0);

		ACTOR_STATE_SET(port, SYNCHRONIZATION);
		SM_FLAG_SET(port, NTT);
		MODE4_DEBUG("ATTACHED Entered\n");
	} else if (!ACTOR_STATE(port, COLLECTING)) {
		/* Start collecting if in sync */
		if (PARTNER_STATE(port, SYNCHRONIZATION)) {
			MODE4_DEBUG("ATTACHED -> COLLECTING\n");
			ACTOR_STATE_SET(port, COLLECTING);
			SM_FLAG_SET(port, NTT);
		}
	} else if (ACTOR_STATE(port, COLLECTING)) {
		/* Check if partner is in COLLECTING state. If so this port can
		 * distribute frames to it */
		if (!ACTOR_STATE(port, DISTRIBUTING)) {
			if (PARTNER_STATE(port, COLLECTING)) {
				/* Enable  DISTRIBUTING if partner is collecting */
				ACTOR_STATE_SET(port, DISTRIBUTING);
				SM_FLAG_SET(port, NTT);
				MODE4_DEBUG("COLLECTING -> DISTRIBUTING\n");
				RTE_BOND_LOG(INFO,
					"Bond %u: slave id %u distributing started.",
					internals->port_id, slave_id);
			}
		} else {
			if (!PARTNER_STATE(port, COLLECTING)) {
				/* Disable DISTRIBUTING (enter COLLECTING state) if partner
				 * is not collecting */
				ACTOR_STATE_CLR(port, DISTRIBUTING);
				SM_FLAG_SET(port, NTT);
				MODE4_DEBUG("DISTRIBUTING -> COLLECTING\n");
				RTE_BOND_LOG(INFO,
					"Bond %u: slave id %u distributing stopped.",
					internals->port_id, slave_id);
			}
		}
	}
}

/**
 * Function handles transmit state machine.
 *
 * Function implements Transmit Machine from point 5.4.16 in 802.1AX
 * documentation.
 *
 * @param port
 */
static void
tx_machine(struct bond_dev_private *internals, uint16_t slave_id)
{
	struct port *agg, *port = &bond_mode_8023ad_ports[slave_id];

	struct rte_mbuf *lacp_pkt = NULL;
	struct lacpdu_header *hdr;
	struct lacpdu *lacpdu;

	/* If periodic timer is not running periodic machine is in NO PERIODIC and
	 * according to 802.3ax standard tx machine should not transmit any frames
	 * and set ntt to false. */
	if (timer_is_stopped(&port->periodic_timer))
		SM_FLAG_CLR(port, NTT);

	if (!SM_FLAG(port, NTT))
		return;

	if (!timer_is_expired(&port->tx_machine_timer))
		return;

	lacp_pkt = rte_pktmbuf_alloc(port->mbuf_pool);
	if (lacp_pkt == NULL) {
		RTE_BOND_LOG(ERR, "Failed to allocate LACP packet from pool");
		return;
	}

	lacp_pkt->data_len = sizeof(*hdr);
	lacp_pkt->pkt_len = sizeof(*hdr);

	hdr = rte_pktmbuf_mtod(lacp_pkt, struct lacpdu_header *);

	/* Source and destination MAC */
	rte_ether_addr_copy(&lacp_mac_addr, &hdr->eth_hdr.d_addr);
	rte_eth_macaddr_get(slave_id, &hdr->eth_hdr.s_addr);
	hdr->eth_hdr.ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_SLOW);

	lacpdu = &hdr->lacpdu;
	memset(lacpdu, 0, sizeof(*lacpdu));

	/* Initialize LACP part */
	lacpdu->subtype = SLOW_SUBTYPE_LACP;
	lacpdu->version_number = 1;

	/* ACTOR */
	lacpdu->actor.tlv_type_info = TLV_TYPE_ACTOR_INFORMATION;
	lacpdu->actor.info_length = sizeof(struct lacpdu_actor_partner_params);
	memcpy(&hdr->lacpdu.actor.port_params, &port->actor,
			sizeof(port->actor));
	agg = &bond_mode_8023ad_ports[port->aggregator_port_id];
	rte_ether_addr_copy(&agg->actor.system,
			&hdr->lacpdu.actor.port_params.system);
	lacpdu->actor.state = port->actor_state;

	/* PARTNER */
	lacpdu->partner.tlv_type_info = TLV_TYPE_PARTNER_INFORMATION;
	lacpdu->partner.info_length = sizeof(struct lacpdu_actor_partner_params);
	memcpy(&lacpdu->partner.port_params, &port->partner,
			sizeof(struct port_params));
	lacpdu->partner.state = port->partner_state;

	/* Other fields */
	lacpdu->tlv_type_collector_info = TLV_TYPE_COLLECTOR_INFORMATION;
	lacpdu->collector_info_length = 0x10;
	lacpdu->collector_max_delay = 0;

	lacpdu->tlv_type_terminator = TLV_TYPE_TERMINATOR_INFORMATION;
	lacpdu->terminator_length = 0;

	MODE4_DEBUG("Sending LACP frame\n");
	BOND_PRINT_LACP(lacpdu);

	if (internals->mode4.dedicated_queues.enabled == 0) {
		int retval = rte_ring_enqueue(port->tx_ring, lacp_pkt);
		if (retval != 0) {
			/* If TX ring full, drop packet and free message.
			   Retransmission will happen in next function call. */
			rte_pktmbuf_free(lacp_pkt);
			set_warning_flags(port, WRN_TX_QUEUE_FULL);
			return;
		}
	} else {
		uint16_t pkts_sent = rte_eth_tx_burst(slave_id,
				internals->mode4.dedicated_queues.tx_qid,
				&lacp_pkt, 1);
		if (pkts_sent != 1) {
			rte_pktmbuf_free(lacp_pkt);
			set_warning_flags(port, WRN_TX_QUEUE_FULL);
			return;
		}
	}


	timer_set(&port->tx_machine_timer, internals->mode4.tx_period_timeout);
	SM_FLAG_CLR(port, NTT);
}

static uint16_t
max_index(uint64_t *a, int n)
{
	if (n <= 0)
		return -1;

	int i, max_i = 0;
	uint64_t max = a[0];

	for (i = 1; i < n; ++i) {
		if (a[i] > max) {
			max = a[i];
			max_i = i;
		}
	}

	return max_i;
}

/**
 * Function assigns port to aggregator.
 *
 * @param bond_dev_private	Pointer to bond_dev_private structure.
 * @param port_pos			Port to assign.
 */
static void
selection_logic(struct bond_dev_private *internals, uint16_t slave_id)
{
	struct port *agg, *port;
	uint16_t slaves_count, new_agg_id, i, j = 0;
	uint16_t *slaves;
	uint64_t agg_bandwidth[RTE_MAX_ETHPORTS] = {0};
	uint64_t agg_count[RTE_MAX_ETHPORTS] = {0};
	uint16_t default_slave = 0;
	struct rte_eth_link link_info;
	uint16_t agg_new_idx = 0;
	int ret;

	slaves = internals->active_slaves;
	slaves_count = internals->active_slave_count;
	port = &bond_mode_8023ad_ports[slave_id];

	/* Search for aggregator suitable for this port */
	for (i = 0; i < slaves_count; ++i) {
		agg = &bond_mode_8023ad_ports[slaves[i]];
		/* Skip ports that are not aggreagators */
		if (agg->aggregator_port_id != slaves[i])
			continue;

		ret = rte_eth_link_get_nowait(slaves[i], &link_info);
		if (ret < 0) {
			RTE_BOND_LOG(ERR,
				"Slave (port %u) link get failed: %s\n",
				slaves[i], rte_strerror(-ret));
			continue;
		}
		agg_count[i] += 1;
		agg_bandwidth[i] += link_info.link_speed;

		/* Actors system ID is not checked since all slave device have the same
		 * ID (MAC address). */
		if ((agg->actor.key == port->actor.key &&
			agg->partner.system_priority == port->partner.system_priority &&
			rte_is_same_ether_addr(&agg->partner.system,
					&port->partner.system) == 1
			&& (agg->partner.key == port->partner.key)) &&
			rte_is_zero_ether_addr(&port->partner.system) != 1 &&
			(agg->actor.key &
				rte_cpu_to_be_16(BOND_LINK_FULL_DUPLEX_KEY)) != 0) {

			if (j == 0)
				default_slave = i;
			j++;
		}
	}

	switch (internals->mode4.agg_selection) {
	case AGG_COUNT:
		agg_new_idx = max_index(agg_count, slaves_count);
		new_agg_id = slaves[agg_new_idx];
		break;
	case AGG_BANDWIDTH:
		agg_new_idx = max_index(agg_bandwidth, slaves_count);
		new_agg_id = slaves[agg_new_idx];
		break;
	case AGG_STABLE:
		if (default_slave == slaves_count)
			new_agg_id = slaves[slave_id];
		else
			new_agg_id = slaves[default_slave];
		break;
	default:
		if (default_slave == slaves_count)
			new_agg_id = slaves[slave_id];
		else
			new_agg_id = slaves[default_slave];
		break;
	}

	if (new_agg_id != port->aggregator_port_id) {
		port->aggregator_port_id = new_agg_id;

		MODE4_DEBUG("-> SELECTED: ID=%3u\n"
			"\t%s aggregator ID=%3u\n",
			port->aggregator_port_id,
			port->aggregator_port_id == slave_id ?
				"aggregator not found, using default" : "aggregator found",
			port->aggregator_port_id);
	}

	port->selected = SELECTED;
}

/* Function maps DPDK speed to bonding speed stored in key field */
static uint16_t
link_speed_key(uint16_t speed) {
	uint16_t key_speed;

	switch (speed) {
	case ETH_SPEED_NUM_NONE:
		key_speed = 0x00;
		break;
	case ETH_SPEED_NUM_10M:
		key_speed = BOND_LINK_SPEED_KEY_10M;
		break;
	case ETH_SPEED_NUM_100M:
		key_speed = BOND_LINK_SPEED_KEY_100M;
		break;
	case ETH_SPEED_NUM_1G:
		key_speed = BOND_LINK_SPEED_KEY_1000M;
		break;
	case ETH_SPEED_NUM_10G:
		key_speed = BOND_LINK_SPEED_KEY_10G;
		break;
	case ETH_SPEED_NUM_20G:
		key_speed = BOND_LINK_SPEED_KEY_20G;
		break;
	case ETH_SPEED_NUM_40G:
		key_speed = BOND_LINK_SPEED_KEY_40G;
		break;
	default:
		/* Unknown speed*/
		key_speed = 0xFFFF;
	}

	return key_speed;
}

static void
rx_machine_update(struct bond_dev_private *internals, uint16_t slave_id,
		struct rte_mbuf *lacp_pkt) {
	struct lacpdu_header *lacp;
	struct lacpdu_actor_partner_params *partner;
	struct port *port, *agg;

	if (lacp_pkt != NULL) {
		lacp = rte_pktmbuf_mtod(lacp_pkt, struct lacpdu_header *);
		RTE_ASSERT(lacp->lacpdu.subtype == SLOW_SUBTYPE_LACP);

		partner = &lacp->lacpdu.partner;
		port = &bond_mode_8023ad_ports[slave_id];
		agg = &bond_mode_8023ad_ports[port->aggregator_port_id];

		if (rte_is_zero_ether_addr(&partner->port_params.system) ||
			rte_is_same_ether_addr(&partner->port_params.system,
				&agg->actor.system)) {
			/* This LACP frame is sending to the bonding port
			 * so pass it to rx_machine.
			 */
			rx_machine(internals, slave_id, &lacp->lacpdu);
		} else {
			char preferred_system_name[RTE_ETHER_ADDR_FMT_SIZE];
			char self_system_name[RTE_ETHER_ADDR_FMT_SIZE];

			rte_ether_format_addr(preferred_system_name,
				RTE_ETHER_ADDR_FMT_SIZE, &partner->port_params.system);
			rte_ether_format_addr(self_system_name,
				RTE_ETHER_ADDR_FMT_SIZE, &agg->actor.system);
			MODE4_DEBUG("preferred partner system %s "
				"is not equal with self system: %s\n",
				preferred_system_name, self_system_name);
		}
		rte_pktmbuf_free(lacp_pkt);
	} else
		rx_machine(internals, slave_id, NULL);
}

static void
bond_mode_8023ad_dedicated_rxq_process(struct bond_dev_private *internals,
			uint16_t slave_id)
{
#define DEDICATED_QUEUE_BURST_SIZE 32
	struct rte_mbuf *lacp_pkt[DEDICATED_QUEUE_BURST_SIZE];
	uint16_t rx_count = rte_eth_rx_burst(slave_id,
				internals->mode4.dedicated_queues.rx_qid,
				lacp_pkt, DEDICATED_QUEUE_BURST_SIZE);

	if (rx_count) {
		uint16_t i;

		for (i = 0; i < rx_count; i++)
			bond_mode_8023ad_handle_slow_pkt(internals, slave_id,
					lacp_pkt[i]);
	} else {
		rx_machine_update(internals, slave_id, NULL);
	}
}

static void
bond_mode_8023ad_periodic_cb(void *arg)
{
	struct rte_eth_dev *bond_dev = arg;
	struct bond_dev_private *internals = bond_dev->data->dev_private;
	struct port *port;
	struct rte_eth_link link_info;
	struct rte_ether_addr slave_addr;
	struct rte_mbuf *lacp_pkt = NULL;
	uint16_t slave_id;
	uint16_t i;


	/* Update link status on each port */
	for (i = 0; i < internals->active_slave_count; i++) {
		uint16_t key;
		int ret;

		slave_id = internals->active_slaves[i];
		ret = rte_eth_link_get_nowait(slave_id, &link_info);
		if (ret < 0) {
			RTE_BOND_LOG(ERR,
				"Slave (port %u) link get failed: %s\n",
				slave_id, rte_strerror(-ret));
		}

		if (ret >= 0 && link_info.link_status != 0) {
			key = link_speed_key(link_info.link_speed) << 1;
			if (link_info.link_duplex == ETH_LINK_FULL_DUPLEX)
				key |= BOND_LINK_FULL_DUPLEX_KEY;
		} else {
			key = 0;
		}

		rte_eth_macaddr_get(slave_id, &slave_addr);
		port = &bond_mode_8023ad_ports[slave_id];

		key = rte_cpu_to_be_16(key);
		if (key != port->actor.key) {
			if (!(key & rte_cpu_to_be_16(BOND_LINK_FULL_DUPLEX_KEY)))
				set_warning_flags(port, WRN_NOT_LACP_CAPABLE);

			port->actor.key = key;
			SM_FLAG_SET(port, NTT);
		}

		if (!rte_is_same_ether_addr(&port->actor.system, &slave_addr)) {
			rte_ether_addr_copy(&slave_addr, &port->actor.system);
			if (port->aggregator_port_id == slave_id)
				SM_FLAG_SET(port, NTT);
		}
	}

	for (i = 0; i < internals->active_slave_count; i++) {
		slave_id = internals->active_slaves[i];
		port = &bond_mode_8023ad_ports[slave_id];

		if ((port->actor.key &
				rte_cpu_to_be_16(BOND_LINK_FULL_DUPLEX_KEY)) == 0) {

			SM_FLAG_SET(port, BEGIN);

			/* LACP is disabled on half duples or link is down */
			if (SM_FLAG(port, LACP_ENABLED)) {
				/* If port was enabled set it to BEGIN state */
				SM_FLAG_CLR(port, LACP_ENABLED);
				ACTOR_STATE_CLR(port, DISTRIBUTING);
				ACTOR_STATE_CLR(port, COLLECTING);
			}

			/* Skip this port processing */
			continue;
		}

		SM_FLAG_SET(port, LACP_ENABLED);

		if (internals->mode4.dedicated_queues.enabled == 0) {
			/* Find LACP packet to this port. Do not check subtype,
			 * it is done in function that queued packet
			 */
			int retval = rte_ring_dequeue(port->rx_ring,
					(void **)&lacp_pkt);

			if (retval != 0)
				lacp_pkt = NULL;

			rx_machine_update(internals, slave_id, lacp_pkt);
		} else {
			bond_mode_8023ad_dedicated_rxq_process(internals,
					slave_id);
		}

		periodic_machine(internals, slave_id);
		mux_machine(internals, slave_id);
		tx_machine(internals, slave_id);
		selection_logic(internals, slave_id);

		SM_FLAG_CLR(port, BEGIN);
		show_warnings(slave_id);
	}

	rte_eal_alarm_set(internals->mode4.update_timeout_us,
			bond_mode_8023ad_periodic_cb, arg);
}

static int
bond_mode_8023ad_register_lacp_mac(uint16_t slave_id)
{
	int ret;

	ret = rte_eth_allmulticast_enable(slave_id);
	if (ret != 0) {
		RTE_BOND_LOG(ERR,
			"failed to enable allmulti mode for port %u: %s",
			slave_id, rte_strerror(-ret));
	}
	if (rte_eth_allmulticast_get(slave_id)) {
		RTE_BOND_LOG(DEBUG, "forced allmulti for port %u",
			     slave_id);
		bond_mode_8023ad_ports[slave_id].forced_rx_flags =
				BOND_8023AD_FORCED_ALLMULTI;
		return 0;
	}

	ret = rte_eth_promiscuous_enable(slave_id);
	if (ret != 0) {
		RTE_BOND_LOG(ERR,
			"failed to enable promiscuous mode for port %u: %s",
			slave_id, rte_strerror(-ret));
	}
	if (rte_eth_promiscuous_get(slave_id)) {
		RTE_BOND_LOG(DEBUG, "forced promiscuous for port %u",
			     slave_id);
		bond_mode_8023ad_ports[slave_id].forced_rx_flags =
				BOND_8023AD_FORCED_PROMISC;
		return 0;
	}

	return -1;
}

static void
bond_mode_8023ad_unregister_lacp_mac(uint16_t slave_id)
{
	int ret;

	switch (bond_mode_8023ad_ports[slave_id].forced_rx_flags) {
	case BOND_8023AD_FORCED_ALLMULTI:
		RTE_BOND_LOG(DEBUG, "unset allmulti for port %u", slave_id);
		ret = rte_eth_allmulticast_disable(slave_id);
		if (ret != 0)
			RTE_BOND_LOG(ERR,
				"failed to disable allmulti mode for port %u: %s",
				slave_id, rte_strerror(-ret));
		break;

	case BOND_8023AD_FORCED_PROMISC:
		RTE_BOND_LOG(DEBUG, "unset promisc for port %u", slave_id);
		ret = rte_eth_promiscuous_disable(slave_id);
		if (ret != 0)
			RTE_BOND_LOG(ERR,
				"failed to disable promiscuous mode for port %u: %s",
				slave_id, rte_strerror(-ret));
		break;

	default:
		break;
	}
}

void
bond_mode_8023ad_activate_slave(struct rte_eth_dev *bond_dev,
				uint16_t slave_id)
{
	struct bond_dev_private *internals = bond_dev->data->dev_private;

	struct port *port = &bond_mode_8023ad_ports[slave_id];
	struct port_params initial = {
			.system = { { 0 } },
			.system_priority = rte_cpu_to_be_16(0xFFFF),
			.key = rte_cpu_to_be_16(BOND_LINK_FULL_DUPLEX_KEY),
			.port_priority = rte_cpu_to_be_16(0x00FF),
			.port_number = 0,
	};

	char mem_name[RTE_ETH_NAME_MAX_LEN];
	int socket_id;
	unsigned element_size;
	uint32_t total_tx_desc;
	struct bond_tx_queue *bd_tx_q;
	uint16_t q_id;

	/* Given slave mus not be in active list */
	RTE_ASSERT(find_slave_by_id(internals->active_slaves,
	internals->active_slave_count, slave_id) == internals->active_slave_count);
	RTE_SET_USED(internals); /* used only for assert when enabled */

	memcpy(&port->actor, &initial, sizeof(struct port_params));
	/* Standard requires that port ID must be grater than 0.
	 * Add 1 do get corresponding port_number */
	port->actor.port_number = rte_cpu_to_be_16(slave_id + 1);

	memcpy(&port->partner, &initial, sizeof(struct port_params));
	memcpy(&port->partner_admin, &initial, sizeof(struct port_params));

	/* default states */
	port->actor_state = STATE_AGGREGATION | STATE_LACP_ACTIVE | STATE_DEFAULTED;
	port->partner_state = STATE_LACP_ACTIVE | STATE_AGGREGATION;
	port->sm_flags = SM_FLAGS_BEGIN;

	/* use this port as agregator */
	port->aggregator_port_id = slave_id;

	if (bond_mode_8023ad_register_lacp_mac(slave_id) < 0) {
		RTE_BOND_LOG(WARNING, "slave %u is most likely broken and won't receive LACP packets",
			     slave_id);
	}

	timer_cancel(&port->warning_timer);

	if (port->mbuf_pool != NULL)
		return;

	RTE_ASSERT(port->rx_ring == NULL);
	RTE_ASSERT(port->tx_ring == NULL);

	socket_id = rte_eth_dev_socket_id(slave_id);
	if (socket_id == -1)
		socket_id = rte_socket_id();

	element_size = sizeof(struct slow_protocol_frame) +
				RTE_PKTMBUF_HEADROOM;

	/* The size of the mempool should be at least:
	 * the sum of the TX descriptors + BOND_MODE_8023AX_SLAVE_TX_PKTS */
	total_tx_desc = BOND_MODE_8023AX_SLAVE_TX_PKTS;
	for (q_id = 0; q_id < bond_dev->data->nb_tx_queues; q_id++) {
		bd_tx_q = (struct bond_tx_queue*)bond_dev->data->tx_queues[q_id];
		total_tx_desc += bd_tx_q->nb_tx_desc;
	}

	snprintf(mem_name, RTE_DIM(mem_name), "slave_port%u_pool", slave_id);
	port->mbuf_pool = rte_pktmbuf_pool_create(mem_name, total_tx_desc,
		RTE_MEMPOOL_CACHE_MAX_SIZE >= 32 ?
			32 : RTE_MEMPOOL_CACHE_MAX_SIZE,
		0, element_size, socket_id);

	/* Any memory allocation failure in initialization is critical because
	 * resources can't be free, so reinitialization is impossible. */
	if (port->mbuf_pool == NULL) {
		rte_panic("Slave %u: Failed to create memory pool '%s': %s\n",
			slave_id, mem_name, rte_strerror(rte_errno));
	}

	snprintf(mem_name, RTE_DIM(mem_name), "slave_%u_rx", slave_id);
	port->rx_ring = rte_ring_create(mem_name,
			rte_align32pow2(BOND_MODE_8023AX_SLAVE_RX_PKTS), socket_id, 0);

	if (port->rx_ring == NULL) {
		rte_panic("Slave %u: Failed to create rx ring '%s': %s\n", slave_id,
			mem_name, rte_strerror(rte_errno));
	}

	/* TX ring is at least one pkt longer to make room for marker packet. */
	snprintf(mem_name, RTE_DIM(mem_name), "slave_%u_tx", slave_id);
	port->tx_ring = rte_ring_create(mem_name,
			rte_align32pow2(BOND_MODE_8023AX_SLAVE_TX_PKTS + 1), socket_id, 0);

	if (port->tx_ring == NULL) {
		rte_panic("Slave %u: Failed to create tx ring '%s': %s\n", slave_id,
			mem_name, rte_strerror(rte_errno));
	}
}

int
bond_mode_8023ad_deactivate_slave(struct rte_eth_dev *bond_dev __rte_unused,
		uint16_t slave_id)
{
	void *pkt = NULL;
	struct port *port = NULL;
	uint8_t old_partner_state;

	port = &bond_mode_8023ad_ports[slave_id];

	ACTOR_STATE_CLR(port, AGGREGATION);
	port->selected = UNSELECTED;

	old_partner_state = port->partner_state;
	record_default(port);

	bond_mode_8023ad_unregister_lacp_mac(slave_id);

	/* If partner timeout state changes then disable timer */
	if (!((old_partner_state ^ port->partner_state) &
			STATE_LACP_SHORT_TIMEOUT))
		timer_cancel(&port->current_while_timer);

	PARTNER_STATE_CLR(port, AGGREGATION);
	ACTOR_STATE_CLR(port, EXPIRED);

	/* flush rx/tx rings */
	while (rte_ring_dequeue(port->rx_ring, &pkt) == 0)
		rte_pktmbuf_free((struct rte_mbuf *)pkt);

	while (rte_ring_dequeue(port->tx_ring, &pkt) == 0)
			rte_pktmbuf_free((struct rte_mbuf *)pkt);
	return 0;
}

void
bond_mode_8023ad_mac_address_update(struct rte_eth_dev *bond_dev)
{
	struct bond_dev_private *internals = bond_dev->data->dev_private;
	struct rte_ether_addr slave_addr;
	struct port *slave, *agg_slave;
	uint16_t slave_id, i, j;

	bond_mode_8023ad_stop(bond_dev);

	for (i = 0; i < internals->active_slave_count; i++) {
		slave_id = internals->active_slaves[i];
		slave = &bond_mode_8023ad_ports[slave_id];
		rte_eth_macaddr_get(slave_id, &slave_addr);

		if (rte_is_same_ether_addr(&slave_addr, &slave->actor.system))
			continue;

		rte_ether_addr_copy(&slave_addr, &slave->actor.system);
		/* Do nothing if this port is not an aggregator. In other case
		 * Set NTT flag on every port that use this aggregator. */
		if (slave->aggregator_port_id != slave_id)
			continue;

		for (j = 0; j < internals->active_slave_count; j++) {
			agg_slave = &bond_mode_8023ad_ports[internals->active_slaves[j]];
			if (agg_slave->aggregator_port_id == slave_id)
				SM_FLAG_SET(agg_slave, NTT);
		}
	}

	if (bond_dev->data->dev_started)
		bond_mode_8023ad_start(bond_dev);
}

static void
bond_mode_8023ad_conf_get(struct rte_eth_dev *dev,
		struct rte_eth_bond_8023ad_conf *conf)
{
	struct bond_dev_private *internals = dev->data->dev_private;
	struct mode8023ad_private *mode4 = &internals->mode4;
	uint64_t ms_ticks = rte_get_tsc_hz() / 1000;

	conf->fast_periodic_ms = mode4->fast_periodic_timeout / ms_ticks;
	conf->slow_periodic_ms = mode4->slow_periodic_timeout / ms_ticks;
	conf->short_timeout_ms = mode4->short_timeout / ms_ticks;
	conf->long_timeout_ms = mode4->long_timeout / ms_ticks;
	conf->aggregate_wait_timeout_ms = mode4->aggregate_wait_timeout / ms_ticks;
	conf->tx_period_ms = mode4->tx_period_timeout / ms_ticks;
	conf->update_timeout_ms = mode4->update_timeout_us / 1000;
	conf->rx_marker_period_ms = mode4->rx_marker_timeout / ms_ticks;
	conf->slowrx_cb = mode4->slowrx_cb;
	conf->agg_selection = mode4->agg_selection;
}

static void
bond_mode_8023ad_conf_get_default(struct rte_eth_bond_8023ad_conf *conf)
{
	conf->fast_periodic_ms = BOND_8023AD_FAST_PERIODIC_MS;
	conf->slow_periodic_ms = BOND_8023AD_SLOW_PERIODIC_MS;
	conf->short_timeout_ms = BOND_8023AD_SHORT_TIMEOUT_MS;
	conf->long_timeout_ms = BOND_8023AD_LONG_TIMEOUT_MS;
	conf->aggregate_wait_timeout_ms = BOND_8023AD_AGGREGATE_WAIT_TIMEOUT_MS;
	conf->tx_period_ms = BOND_8023AD_TX_MACHINE_PERIOD_MS;
	conf->rx_marker_period_ms = BOND_8023AD_RX_MARKER_PERIOD_MS;
	conf->update_timeout_ms = BOND_MODE_8023AX_UPDATE_TIMEOUT_MS;
	conf->slowrx_cb = NULL;
	conf->agg_selection = AGG_STABLE;
}

static void
bond_mode_8023ad_conf_assign(struct mode8023ad_private *mode4,
		struct rte_eth_bond_8023ad_conf *conf)
{
	uint64_t ms_ticks = rte_get_tsc_hz() / 1000;

	mode4->fast_periodic_timeout = conf->fast_periodic_ms * ms_ticks;
	mode4->slow_periodic_timeout = conf->slow_periodic_ms * ms_ticks;
	mode4->short_timeout = conf->short_timeout_ms * ms_ticks;
	mode4->long_timeout = conf->long_timeout_ms * ms_ticks;
	mode4->aggregate_wait_timeout = conf->aggregate_wait_timeout_ms * ms_ticks;
	mode4->tx_period_timeout = conf->tx_period_ms * ms_ticks;
	mode4->rx_marker_timeout = conf->rx_marker_period_ms * ms_ticks;
	mode4->update_timeout_us = conf->update_timeout_ms * 1000;

	mode4->dedicated_queues.enabled = 0;
	mode4->dedicated_queues.rx_qid = UINT16_MAX;
	mode4->dedicated_queues.tx_qid = UINT16_MAX;
}

void
bond_mode_8023ad_setup(struct rte_eth_dev *dev,
		struct rte_eth_bond_8023ad_conf *conf)
{
	struct rte_eth_bond_8023ad_conf def_conf;
	struct bond_dev_private *internals = dev->data->dev_private;
	struct mode8023ad_private *mode4 = &internals->mode4;

	if (conf == NULL) {
		conf = &def_conf;
		bond_mode_8023ad_conf_get_default(conf);
	}

	bond_mode_8023ad_stop(dev);
	bond_mode_8023ad_conf_assign(mode4, conf);
	mode4->slowrx_cb = conf->slowrx_cb;
	mode4->agg_selection = AGG_STABLE;

	if (dev->data->dev_started)
		bond_mode_8023ad_start(dev);
}

int
bond_mode_8023ad_enable(struct rte_eth_dev *bond_dev)
{
	struct bond_dev_private *internals = bond_dev->data->dev_private;
	uint16_t i;

	for (i = 0; i < internals->active_slave_count; i++)
		bond_mode_8023ad_activate_slave(bond_dev,
				internals->active_slaves[i]);

	return 0;
}

int
bond_mode_8023ad_start(struct rte_eth_dev *bond_dev)
{
	struct bond_dev_private *internals = bond_dev->data->dev_private;
	struct mode8023ad_private *mode4 = &internals->mode4;
	static const uint64_t us = BOND_MODE_8023AX_UPDATE_TIMEOUT_MS * 1000;

	rte_eth_macaddr_get(internals->port_id, &mode4->mac_addr);
	if (mode4->slowrx_cb)
		return rte_eal_alarm_set(us, &bond_mode_8023ad_ext_periodic_cb,
					 bond_dev);

	return rte_eal_alarm_set(us, &bond_mode_8023ad_periodic_cb, bond_dev);
}

void
bond_mode_8023ad_stop(struct rte_eth_dev *bond_dev)
{
	struct bond_dev_private *internals = bond_dev->data->dev_private;
	struct mode8023ad_private *mode4 = &internals->mode4;

	if (mode4->slowrx_cb) {
		rte_eal_alarm_cancel(&bond_mode_8023ad_ext_periodic_cb,
				     bond_dev);
		return;
	}
	rte_eal_alarm_cancel(&bond_mode_8023ad_periodic_cb, bond_dev);
}

void
bond_mode_8023ad_handle_slow_pkt(struct bond_dev_private *internals,
				  uint16_t slave_id, struct rte_mbuf *pkt)
{
	struct mode8023ad_private *mode4 = &internals->mode4;
	struct port *port = &bond_mode_8023ad_ports[slave_id];
	struct marker_header *m_hdr;
	uint64_t marker_timer, old_marker_timer;
	int retval;
	uint8_t wrn, subtype;
	/* If packet is a marker, we send response now by reusing given packet
	 * and update only source MAC, destination MAC is multicast so don't
	 * update it. Other frames will be handled later by state machines */
	subtype = rte_pktmbuf_mtod(pkt,
			struct slow_protocol_frame *)->slow_protocol.subtype;

	if (subtype == SLOW_SUBTYPE_MARKER) {
		m_hdr = rte_pktmbuf_mtod(pkt, struct marker_header *);

		if (likely(m_hdr->marker.tlv_type_marker != MARKER_TLV_TYPE_INFO)) {
			wrn = WRN_UNKNOWN_MARKER_TYPE;
			goto free_out;
		}

		/* Setup marker timer. Do it in loop in case concurrent access. */
		do {
			old_marker_timer = port->rx_marker_timer;
			if (!timer_is_expired(&old_marker_timer)) {
				wrn = WRN_RX_MARKER_TO_FAST;
				goto free_out;
			}

			timer_set(&marker_timer, mode4->rx_marker_timeout);
			retval = rte_atomic64_cmpset(&port->rx_marker_timer,
				old_marker_timer, marker_timer);
		} while (unlikely(retval == 0));

		m_hdr->marker.tlv_type_marker = MARKER_TLV_TYPE_RESP;
		rte_eth_macaddr_get(slave_id, &m_hdr->eth_hdr.s_addr);

		if (internals->mode4.dedicated_queues.enabled == 0) {
			if (rte_ring_enqueue(port->tx_ring, pkt) != 0) {
				/* reset timer */
				port->rx_marker_timer = 0;
				wrn = WRN_TX_QUEUE_FULL;
				goto free_out;
			}
		} else {
			/* Send packet directly to the slow queue */
			uint16_t tx_count = rte_eth_tx_burst(slave_id,
					internals->mode4.dedicated_queues.tx_qid,
					&pkt, 1);
			if (tx_count != 1) {
				/* reset timer */
				port->rx_marker_timer = 0;
				wrn = WRN_TX_QUEUE_FULL;
				goto free_out;
			}
		}
	} else if (likely(subtype == SLOW_SUBTYPE_LACP)) {
		if (internals->mode4.dedicated_queues.enabled == 0) {
			if (rte_ring_enqueue(port->rx_ring, pkt) != 0) {
				/* If RX fing full free lacpdu message and drop packet */
				wrn = WRN_RX_QUEUE_FULL;
				goto free_out;
			}
		} else
			rx_machine_update(internals, slave_id, pkt);
	} else {
		wrn = WRN_UNKNOWN_SLOW_TYPE;
		goto free_out;
	}

	return;

free_out:
	set_warning_flags(port, wrn);
	rte_pktmbuf_free(pkt);
}

int
rte_eth_bond_8023ad_conf_get(uint16_t port_id,
		struct rte_eth_bond_8023ad_conf *conf)
{
	struct rte_eth_dev *bond_dev;

	if (valid_bonded_port_id(port_id) != 0)
		return -EINVAL;

	if (conf == NULL)
		return -EINVAL;

	bond_dev = &rte_eth_devices[port_id];
	bond_mode_8023ad_conf_get(bond_dev, conf);
	return 0;
}

int
rte_eth_bond_8023ad_agg_selection_set(uint16_t port_id,
		enum rte_bond_8023ad_agg_selection agg_selection)
{
	struct rte_eth_dev *bond_dev;
	struct bond_dev_private *internals;
	struct mode8023ad_private *mode4;

	if (valid_bonded_port_id(port_id) != 0)
		return -EINVAL;

	bond_dev = &rte_eth_devices[port_id];
	internals = bond_dev->data->dev_private;

	if (internals->mode != 4)
		return -EINVAL;

	mode4 = &internals->mode4;
	if (agg_selection == AGG_COUNT || agg_selection == AGG_BANDWIDTH
			|| agg_selection == AGG_STABLE)
		mode4->agg_selection = agg_selection;
	return 0;
}

int rte_eth_bond_8023ad_agg_selection_get(uint16_t port_id)
{
	struct rte_eth_dev *bond_dev;
	struct bond_dev_private *internals;
	struct mode8023ad_private *mode4;

	if (valid_bonded_port_id(port_id) != 0)
		return -EINVAL;

	bond_dev = &rte_eth_devices[port_id];
	internals = bond_dev->data->dev_private;

	if (internals->mode != 4)
		return -EINVAL;
	mode4 = &internals->mode4;

	return mode4->agg_selection;
}



static int
bond_8023ad_setup_validate(uint16_t port_id,
		struct rte_eth_bond_8023ad_conf *conf)
{
	if (valid_bonded_port_id(port_id) != 0)
		return -EINVAL;

	if (conf != NULL) {
		/* Basic sanity check */
		if (conf->slow_periodic_ms == 0 ||
				conf->fast_periodic_ms >= conf->slow_periodic_ms ||
				conf->long_timeout_ms == 0 ||
				conf->short_timeout_ms >= conf->long_timeout_ms ||
				conf->aggregate_wait_timeout_ms == 0 ||
				conf->tx_period_ms == 0 ||
				conf->rx_marker_period_ms == 0 ||
				conf->update_timeout_ms == 0) {
			RTE_BOND_LOG(ERR, "given mode 4 configuration is invalid");
			return -EINVAL;
		}
	}

	return 0;
}


int
rte_eth_bond_8023ad_setup(uint16_t port_id,
		struct rte_eth_bond_8023ad_conf *conf)
{
	struct rte_eth_dev *bond_dev;
	int err;

	err = bond_8023ad_setup_validate(port_id, conf);
	if (err != 0)
		return err;

	bond_dev = &rte_eth_devices[port_id];
	bond_mode_8023ad_setup(bond_dev, conf);

	return 0;
}





int
rte_eth_bond_8023ad_slave_info(uint16_t port_id, uint16_t slave_id,
		struct rte_eth_bond_8023ad_slave_info *info)
{
	struct rte_eth_dev *bond_dev;
	struct bond_dev_private *internals;
	struct port *port;

	if (info == NULL || valid_bonded_port_id(port_id) != 0 ||
			rte_eth_bond_mode_get(port_id) != BONDING_MODE_8023AD)
		return -EINVAL;

	bond_dev = &rte_eth_devices[port_id];

	internals = bond_dev->data->dev_private;
	if (find_slave_by_id(internals->active_slaves,
			internals->active_slave_count, slave_id) ==
				internals->active_slave_count)
		return -EINVAL;

	port = &bond_mode_8023ad_ports[slave_id];
	info->selected = port->selected;

	info->actor_state = port->actor_state;
	rte_memcpy(&info->actor, &port->actor, sizeof(port->actor));

	info->partner_state = port->partner_state;
	rte_memcpy(&info->partner, &port->partner, sizeof(port->partner));

	info->agg_port_id = port->aggregator_port_id;
	return 0;
}

static int
bond_8023ad_ext_validate(uint16_t port_id, uint16_t slave_id)
{
	struct rte_eth_dev *bond_dev;
	struct bond_dev_private *internals;
	struct mode8023ad_private *mode4;

	if (rte_eth_bond_mode_get(port_id) != BONDING_MODE_8023AD)
		return -EINVAL;

	bond_dev = &rte_eth_devices[port_id];

	if (!bond_dev->data->dev_started)
		return -EINVAL;

	internals = bond_dev->data->dev_private;
	if (find_slave_by_id(internals->active_slaves,
			internals->active_slave_count, slave_id) ==
				internals->active_slave_count)
		return -EINVAL;

	mode4 = &internals->mode4;
	if (mode4->slowrx_cb == NULL)
		return -EINVAL;

	return 0;
}

int
rte_eth_bond_8023ad_ext_collect(uint16_t port_id, uint16_t slave_id,
				int enabled)
{
	struct port *port;
	int res;

	res = bond_8023ad_ext_validate(port_id, slave_id);
	if (res != 0)
		return res;

	port = &bond_mode_8023ad_ports[slave_id];

	if (enabled)
		ACTOR_STATE_SET(port, COLLECTING);
	else
		ACTOR_STATE_CLR(port, COLLECTING);

	return 0;
}

int
rte_eth_bond_8023ad_ext_distrib(uint16_t port_id, uint16_t slave_id,
				int enabled)
{
	struct port *port;
	int res;

	res = bond_8023ad_ext_validate(port_id, slave_id);
	if (res != 0)
		return res;

	port = &bond_mode_8023ad_ports[slave_id];

	if (enabled)
		ACTOR_STATE_SET(port, DISTRIBUTING);
	else
		ACTOR_STATE_CLR(port, DISTRIBUTING);

	return 0;
}

int
rte_eth_bond_8023ad_ext_distrib_get(uint16_t port_id, uint16_t slave_id)
{
	struct port *port;
	int err;

	err = bond_8023ad_ext_validate(port_id, slave_id);
	if (err != 0)
		return err;

	port = &bond_mode_8023ad_ports[slave_id];
	return ACTOR_STATE(port, DISTRIBUTING);
}

int
rte_eth_bond_8023ad_ext_collect_get(uint16_t port_id, uint16_t slave_id)
{
	struct port *port;
	int err;

	err = bond_8023ad_ext_validate(port_id, slave_id);
	if (err != 0)
		return err;

	port = &bond_mode_8023ad_ports[slave_id];
	return ACTOR_STATE(port, COLLECTING);
}

int
rte_eth_bond_8023ad_ext_slowtx(uint16_t port_id, uint16_t slave_id,
		struct rte_mbuf *lacp_pkt)
{
	struct port *port;
	int res;

	res = bond_8023ad_ext_validate(port_id, slave_id);
	if (res != 0)
		return res;

	port = &bond_mode_8023ad_ports[slave_id];

	if (rte_pktmbuf_pkt_len(lacp_pkt) < sizeof(struct lacpdu_header))
		return -EINVAL;

	struct lacpdu_header *lacp;

	/* only enqueue LACPDUs */
	lacp = rte_pktmbuf_mtod(lacp_pkt, struct lacpdu_header *);
	if (lacp->lacpdu.subtype != SLOW_SUBTYPE_LACP)
		return -EINVAL;

	MODE4_DEBUG("sending LACP frame\n");

	return rte_ring_enqueue(port->tx_ring, lacp_pkt);
}

static void
bond_mode_8023ad_ext_periodic_cb(void *arg)
{
	struct rte_eth_dev *bond_dev = arg;
	struct bond_dev_private *internals = bond_dev->data->dev_private;
	struct mode8023ad_private *mode4 = &internals->mode4;
	struct port *port;
	void *pkt = NULL;
	uint16_t i, slave_id;

	for (i = 0; i < internals->active_slave_count; i++) {
		slave_id = internals->active_slaves[i];
		port = &bond_mode_8023ad_ports[slave_id];

		if (rte_ring_dequeue(port->rx_ring, &pkt) == 0) {
			struct rte_mbuf *lacp_pkt = pkt;
			struct lacpdu_header *lacp;

			lacp = rte_pktmbuf_mtod(lacp_pkt,
						struct lacpdu_header *);
			RTE_VERIFY(lacp->lacpdu.subtype == SLOW_SUBTYPE_LACP);

			/* This is LACP frame so pass it to rx callback.
			 * Callback is responsible for freeing mbuf.
			 */
			mode4->slowrx_cb(slave_id, lacp_pkt);
		}
	}

	rte_eal_alarm_set(internals->mode4.update_timeout_us,
			bond_mode_8023ad_ext_periodic_cb, arg);
}

int
rte_eth_bond_8023ad_dedicated_queues_enable(uint16_t port)
{
	int retval = 0;
	struct rte_eth_dev *dev;
	struct bond_dev_private *internals;

	if (valid_bonded_port_id(port) != 0)
		return -EINVAL;

	dev = &rte_eth_devices[port];
	internals = dev->data->dev_private;

	if (bond_8023ad_slow_pkt_hw_filter_supported(port) != 0)
		return -1;

	/* Device must be stopped to set up slow queue */
	if (dev->data->dev_started)
		return -1;

	internals->mode4.dedicated_queues.enabled = 1;

	bond_ethdev_mode_set(dev, internals->mode);
	return retval;
}

int
rte_eth_bond_8023ad_dedicated_queues_disable(uint16_t port)
{
	int retval = 0;
	struct rte_eth_dev *dev;
	struct bond_dev_private *internals;

	if (valid_bonded_port_id(port) != 0)
		return -EINVAL;

	dev = &rte_eth_devices[port];
	internals = dev->data->dev_private;

	/* Device must be stopped to set up slow queue */
	if (dev->data->dev_started)
		return -1;

	internals->mode4.dedicated_queues.enabled = 0;

	bond_ethdev_mode_set(dev, internals->mode);

	return retval;
}
