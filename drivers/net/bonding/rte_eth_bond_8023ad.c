/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stddef.h>
#include <string.h>
#include <stdbool.h>

#include <rte_alarm.h>
#include <rte_malloc.h>
#include <rte_errno.h>
#include <rte_cycles.h>

#include "rte_eth_bond_private.h"

#ifdef RTE_LIBRTE_BOND_DEBUG_8023AD
#define MODE4_DEBUG(fmt, ...) RTE_LOG(DEBUG, PMD, "%6u [Port %u: %s] " fmt, \
			bond_dbg_get_time_diff_ms(), slave_id, \
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

	RTE_LOG(DEBUG, PMD, "LACP: {\n"\
			"  subtype= %02X\n"\
			"  ver_num=%02X\n"\
			"  actor={ tlv=%02X, len=%02X\n"\
			"    pri=%04X, system=%s, key=%04X, p_pri=%04X p_num=%04X\n"\
			"       state={ %s }\n"\
			"  }\n"\
			"  partner={ tlv=%02X, len=%02X\n"\
			"    pri=%04X, system=%s, key=%04X, p_pri=%04X p_num=%04X\n"\
			"       state={ %s }\n"\
			"  }\n"\
			"  collector={info=%02X, length=%02X, max_delay=%04X\n, " \
							"type_term=%02X, terminator_length = %02X}\n",\
			l->subtype,\
			l->version_number,\
			l->actor.tlv_type_info,\
			l->actor.info_length,\
			l->actor.port_params.system_priority,\
			a_address,\
			l->actor.port_params.key,\
			l->actor.port_params.port_priority,\
			l->actor.port_params.port_number,\
			a_state,\
			l->partner.tlv_type_info,\
			l->partner.info_length,\
			l->partner.port_params.system_priority,\
			p_address,\
			l->partner.port_params.key,\
			l->partner.port_params.port_priority,\
			l->partner.port_params.port_number,\
			p_state,\
			l->tlv_type_collector_info,\
			l->collector_info_length,\
			l->collector_max_delay,\
			l->tlv_type_terminator,\
			l->terminator_length);

}
#define BOND_PRINT_LACP(lacpdu) bond_print_lacp(lacpdu)
#else
#define BOND_PRINT_LACP(lacpdu) do { } while (0)
#define MODE4_DEBUG(fmt, ...) do { } while (0)
#endif

static const struct ether_addr lacp_mac_addr = {
	.addr_bytes = { 0x01, 0x80, 0xC2, 0x00, 0x00, 0x02 }
};

struct port mode_8023ad_ports[RTE_MAX_ETHPORTS];

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
show_warnings(uint8_t slave_id)
{
	struct port *port = &mode_8023ad_ports[slave_id];
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
		RTE_LOG(DEBUG, PMD,
			"Slave %u: failed to enqueue LACP packet into RX ring.\n"
			"Receive and transmit functions must be invoked on bonded\n"
			"interface at least 10 times per second or LACP will not\n"
			"work correctly\n", slave_id);
	}

	if (warnings & WRN_TX_QUEUE_FULL) {
		RTE_LOG(DEBUG, PMD,
			"Slave %u: failed to enqueue LACP packet into TX ring.\n"
			"Receive and transmit functions must be invoked on bonded\n"
			"interface at least 10 times per second or LACP will not\n"
			"work correctly\n", slave_id);
	}

	if (warnings & WRN_RX_MARKER_TO_FAST)
		RTE_LOG(INFO, PMD, "Slave %u: marker to early - ignoring.\n", slave_id);

	if (warnings & WRN_UNKNOWN_SLOW_TYPE) {
		RTE_LOG(INFO, PMD,
			"Slave %u: ignoring unknown slow protocol frame type", slave_id);
	}

	if (warnings & WRN_UNKNOWN_MARKER_TYPE)
		RTE_LOG(INFO, PMD, "Slave %u: ignoring unknown marker type", slave_id);

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
rx_machine(struct bond_dev_private *internals, uint8_t slave_id,
		struct lacpdu *lacp)
{
	struct port *agg, *port = &mode_8023ad_ports[slave_id];
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
		agg = &mode_8023ad_ports[port->aggregator_port_id];
		bool match = port->actor.system_priority ==
			lacp->partner.port_params.system_priority &&
			is_same_ether_addr(&agg->actor.system,
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
		return; /* No state change */
	}

	/* If CURRENT state timer is not running (stopped or expired)
	 * transit to EXPIRED state from DISABLED or CURRENT */
	if (!timer_is_running(&port->current_while_timer)) {
		ACTOR_STATE_SET(port, EXPIRED);
		PARTNER_STATE_CLR(port, SYNCHRONIZATION);
		PARTNER_STATE_SET(port, LACP_SHORT_TIMEOUT);
		timer_set(&port->current_while_timer, internals->mode4.short_timeout);
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
periodic_machine(struct bond_dev_private *internals, uint8_t slave_id)
{
	struct port *port = &mode_8023ad_ports[slave_id];
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
			if (!is_partner_fast)
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
mux_machine(struct bond_dev_private *internals, uint8_t slave_id)
{
	struct port *port = &mode_8023ad_ports[slave_id];

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
		RTE_VERIFY((port->actor_state & (STATE_COLLECTING |
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
				RTE_LOG(INFO, PMD,
					"Bond %u: slave id %u distributing started.\n",
					internals->port_id, slave_id);
			}
		} else {
			if (!PARTNER_STATE(port, COLLECTING)) {
				/* Disable DISTRIBUTING (enter COLLECTING state) if partner
				 * is not collecting */
				ACTOR_STATE_CLR(port, DISTRIBUTING);
				SM_FLAG_SET(port, NTT);
				MODE4_DEBUG("DISTRIBUTING -> COLLECTING\n");
				RTE_LOG(INFO, PMD,
					"Bond %u: slave id %u distributing stopped.\n",
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
tx_machine(struct bond_dev_private *internals, uint8_t slave_id)
{
	struct port *agg, *port = &mode_8023ad_ports[slave_id];

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
		RTE_LOG(ERR, PMD, "Failed to allocate LACP packet from pool\n");
		return;
	}

	lacp_pkt->data_len = sizeof(*hdr);
	lacp_pkt->pkt_len = sizeof(*hdr);

	hdr = rte_pktmbuf_mtod(lacp_pkt, struct lacpdu_header *);

	/* Source and destination MAC */
	ether_addr_copy(&lacp_mac_addr, &hdr->eth_hdr.d_addr);
	rte_eth_macaddr_get(slave_id, &hdr->eth_hdr.s_addr);
	hdr->eth_hdr.ether_type = rte_cpu_to_be_16(ETHER_TYPE_SLOW);

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
	agg = &mode_8023ad_ports[port->aggregator_port_id];
	ether_addr_copy(&agg->actor.system, &hdr->lacpdu.actor.port_params.system);
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

	if (rte_ring_enqueue(port->tx_ring, lacp_pkt) == -ENOBUFS) {
		/* If TX ring full, drop packet and free message. Retransmission
		 * will happen in next function call. */
		rte_pktmbuf_free(lacp_pkt);
		set_warning_flags(port, WRN_TX_QUEUE_FULL);
		return;
	}

	MODE4_DEBUG("sending LACP frame\n");
	BOND_PRINT_LACP(lacpdu);

	timer_set(&port->tx_machine_timer, internals->mode4.tx_period_timeout);
	SM_FLAG_CLR(port, NTT);
}

/**
 * Function assigns port to aggregator.
 *
 * @param bond_dev_private	Pointer to bond_dev_private structure.
 * @param port_pos			Port to assign.
 */
static void
selection_logic(struct bond_dev_private *internals, uint8_t slave_id)
{
	struct port *agg, *port;
	uint8_t slaves_count, new_agg_id, i;
	uint8_t *slaves;

	slaves = internals->active_slaves;
	slaves_count = internals->active_slave_count;
	port = &mode_8023ad_ports[slave_id];

	/* Search for aggregator suitable for this port */
	for (i = 0; i < slaves_count; ++i) {
		agg = &mode_8023ad_ports[slaves[i]];
		/* Skip ports that are not aggreagators */
		if (agg->aggregator_port_id != slaves[i])
			continue;

		/* Actors system ID is not checked since all slave device have the same
		 * ID (MAC address). */
		if ((agg->actor.key == port->actor.key &&
			agg->partner.system_priority == port->partner.system_priority &&
			is_same_ether_addr(&agg->partner.system, &port->partner.system) == 1
			&& (agg->partner.key == port->partner.key)) &&
			is_zero_ether_addr(&port->partner.system) != 1 &&
			(agg->actor.key &
				rte_cpu_to_be_16(BOND_LINK_FULL_DUPLEX_KEY)) != 0) {

			break;
		}
	}

	/* By default, port uses it self as agregator */
	if (i == slaves_count)
		new_agg_id = slave_id;
	else
		new_agg_id = slaves[i];

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
	case ETH_LINK_SPEED_AUTONEG:
		key_speed = 0x00;
		break;
	case ETH_LINK_SPEED_10:
		key_speed = BOND_LINK_SPEED_KEY_10M;
		break;
	case ETH_LINK_SPEED_100:
		key_speed = BOND_LINK_SPEED_KEY_100M;
		break;
	case ETH_LINK_SPEED_1000:
		key_speed = BOND_LINK_SPEED_KEY_1000M;
		break;
	case ETH_LINK_SPEED_10G:
		key_speed = BOND_LINK_SPEED_KEY_10G;
		break;
	case ETH_LINK_SPEED_20G:
		key_speed = BOND_LINK_SPEED_KEY_20G;
		break;
	case ETH_LINK_SPEED_40G:
		key_speed = BOND_LINK_SPEED_KEY_40G;
		break;
	default:
		/* Unknown speed*/
		key_speed = 0xFFFF;
	}

	return key_speed;
}

static void
bond_mode_8023ad_periodic_cb(void *arg)
{
	struct rte_eth_dev *bond_dev = arg;
	struct bond_dev_private *internals = bond_dev->data->dev_private;
	struct port *port;
	struct rte_eth_link link_info;
	struct ether_addr slave_addr;

	void *pkt = NULL;
	uint8_t i, slave_id;


	/* Update link status on each port */
	for (i = 0; i < internals->active_slave_count; i++) {
		uint16_t key;

		slave_id = internals->active_slaves[i];
		rte_eth_link_get(slave_id, &link_info);
		rte_eth_macaddr_get(slave_id, &slave_addr);

		if (link_info.link_status != 0) {
			key = link_speed_key(link_info.link_speed) << 1;
			if (link_info.link_duplex == ETH_LINK_FULL_DUPLEX)
				key |= BOND_LINK_FULL_DUPLEX_KEY;
		} else
			key = 0;

		port = &mode_8023ad_ports[slave_id];

		key = rte_cpu_to_be_16(key);
		if (key != port->actor.key) {
			if (!(key & rte_cpu_to_be_16(BOND_LINK_FULL_DUPLEX_KEY)))
				set_warning_flags(port, WRN_NOT_LACP_CAPABLE);

			port->actor.key = key;
			SM_FLAG_SET(port, NTT);
		}

		if (!is_same_ether_addr(&port->actor.system, &slave_addr)) {
			ether_addr_copy(&slave_addr, &port->actor.system);
			if (port->aggregator_port_id == slave_id)
				SM_FLAG_SET(port, NTT);
		}
	}

	for (i = 0; i < internals->active_slave_count; i++) {
		slave_id = internals->active_slaves[i];
		port = &mode_8023ad_ports[slave_id];

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

		/* Find LACP packet to this port. Do not check subtype, it is done in
		 * function that queued packet */
		if (rte_ring_dequeue(port->rx_ring, &pkt) == 0) {
			struct rte_mbuf *lacp_pkt = pkt;
			struct lacpdu_header *lacp;

			lacp = rte_pktmbuf_mtod(lacp_pkt, struct lacpdu_header *);
			RTE_VERIFY(lacp->lacpdu.subtype == SLOW_SUBTYPE_LACP);

			/* This is LACP frame so pass it to rx_machine */
			rx_machine(internals, slave_id, &lacp->lacpdu);
			rte_pktmbuf_free(lacp_pkt);
		} else
			rx_machine(internals, slave_id, NULL);

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

void
bond_mode_8023ad_activate_slave(struct rte_eth_dev *bond_dev, uint8_t slave_id)
{
	struct bond_dev_private *internals = bond_dev->data->dev_private;

	struct port *port = &mode_8023ad_ports[slave_id];
	struct port_params initial = {
			.system = { { 0 } },
			.system_priority = rte_cpu_to_be_16(0xFFFF),
			.key = rte_cpu_to_be_16(BOND_LINK_FULL_DUPLEX_KEY),
			.port_priority = rte_cpu_to_be_16(0x00FF),
			.port_number = 0,
	};

	char mem_name[RTE_ETH_NAME_MAX_LEN];
	uint8_t socket_id;
	unsigned element_size;

	/* Given slave mus not be in active list */
	RTE_VERIFY(find_slave_by_id(internals->active_slaves,
	internals->active_slave_count, slave_id) == internals->active_slave_count);

	memcpy(&port->actor, &initial, sizeof(struct port_params));
	/* Standard requires that port ID must be grater than 0.
	 * Add 1 do get corresponding port_number */
	port->actor.port_number = rte_cpu_to_be_16((uint16_t)slave_id + 1);

	memcpy(&port->partner, &initial, sizeof(struct port_params));

	/* default states */
	port->actor_state = STATE_AGGREGATION | STATE_LACP_ACTIVE | STATE_DEFAULTED;
	port->partner_state = STATE_LACP_ACTIVE;
	port->sm_flags = SM_FLAGS_BEGIN;

	/* use this port as agregator */
	port->aggregator_port_id = slave_id;
	rte_eth_promiscuous_enable(slave_id);

	timer_cancel(&port->warning_timer);

	if (port->mbuf_pool != NULL)
		return;

	RTE_VERIFY(port->rx_ring == NULL);
	RTE_VERIFY(port->tx_ring == NULL);
	socket_id = rte_eth_devices[slave_id].pci_dev->numa_node;

	element_size = sizeof(struct slow_protocol_frame) + sizeof(struct rte_mbuf)
				+ RTE_PKTMBUF_HEADROOM;

	 /* How big memory pool should be? If driver will not
	  * free packets quick enough there will be ENOMEM in tx_machine.
	  * For now give 511 pkts * max number of queued TX packets per slave.
	  * Hope it will be enough. */
	snprintf(mem_name, RTE_DIM(mem_name), "slave_port%u_pool", slave_id);
	port->mbuf_pool = rte_mempool_create(mem_name,
		BOND_MODE_8023AX_SLAVE_TX_PKTS * 512 - 1,
		element_size,
		RTE_MEMPOOL_CACHE_MAX_SIZE >= 32 ? 32 : RTE_MEMPOOL_CACHE_MAX_SIZE,
		sizeof(struct rte_pktmbuf_pool_private), rte_pktmbuf_pool_init,
		NULL, rte_pktmbuf_init, NULL, socket_id, MEMPOOL_F_NO_SPREAD);

	/* Any memory allocation failure in initalization is critical because
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
bond_mode_8023ad_deactivate_slave(struct rte_eth_dev *bond_dev,
		uint8_t slave_id)
{
	struct bond_dev_private *internals = bond_dev->data->dev_private;
	void *pkt = NULL;
	struct port *port;
	uint8_t i;

	/* Given slave mus be in active list */
	RTE_VERIFY(find_slave_by_id(internals->active_slaves,
	internals->active_slave_count, slave_id) < internals->active_slave_count);

	/* Exclude slave from transmit policy. If this slave is an aggregator
	 * make all aggregated slaves unselected to force sellection logic
	 * to select suitable aggregator for this port. */
	for (i = 0; i < internals->active_slave_count; i++) {
		port = &mode_8023ad_ports[internals->active_slaves[i]];
		if (port->aggregator_port_id != slave_id)
			continue;

		port->selected = UNSELECTED;

		/* Use default aggregator */
		port->aggregator_port_id = internals->active_slaves[i];
	}

	port = &mode_8023ad_ports[slave_id];
	port->selected = UNSELECTED;
	port->actor_state &= ~(STATE_SYNCHRONIZATION | STATE_DISTRIBUTING |
			STATE_COLLECTING);

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
	struct ether_addr slave_addr;
	struct port *slave, *agg_slave;
	uint8_t slave_id, i, j;

	bond_mode_8023ad_stop(bond_dev);

	for (i = 0; i < internals->active_slave_count; i++) {
		slave_id = internals->active_slaves[i];
		slave = &mode_8023ad_ports[slave_id];
		rte_eth_macaddr_get(slave_id, &slave_addr);

		if (is_same_ether_addr(&slave_addr, &slave->actor.system))
			continue;

		ether_addr_copy(&slave_addr, &slave->actor.system);
		/* Do nothing if this port is not an aggregator. In other case
		 * Set NTT flag on every port that use this aggregator. */
		if (slave->aggregator_port_id != slave_id)
			continue;

		for (j = 0; j < internals->active_slave_count; j++) {
			agg_slave = &mode_8023ad_ports[internals->active_slaves[j]];
			if (agg_slave->aggregator_port_id == slave_id)
				SM_FLAG_SET(agg_slave, NTT);
		}
	}

	if (bond_dev->data->dev_started)
		bond_mode_8023ad_start(bond_dev);
}

void
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
}

void
bond_mode_8023ad_setup(struct rte_eth_dev *dev,
		struct rte_eth_bond_8023ad_conf *conf)
{
	struct rte_eth_bond_8023ad_conf def_conf;
	struct bond_dev_private *internals = dev->data->dev_private;
	struct mode8023ad_private *mode4 = &internals->mode4;
	uint64_t ms_ticks = rte_get_tsc_hz() / 1000;

	if (conf == NULL) {
		conf = &def_conf;
		conf->fast_periodic_ms = BOND_8023AD_FAST_PERIODIC_MS;
		conf->slow_periodic_ms = BOND_8023AD_SLOW_PERIODIC_MS;
		conf->short_timeout_ms = BOND_8023AD_SHORT_TIMEOUT_MS;
		conf->long_timeout_ms = BOND_8023AD_LONG_TIMEOUT_MS;
		conf->aggregate_wait_timeout_ms = BOND_8023AD_AGGREGATE_WAIT_TIMEOUT_MS;
		conf->tx_period_ms = BOND_8023AD_TX_MACHINE_PERIOD_MS;
		conf->rx_marker_period_ms = BOND_8023AD_RX_MARKER_PERIOD_MS;
		conf->update_timeout_ms = BOND_MODE_8023AX_UPDATE_TIMEOUT_MS;
	}

	mode4->fast_periodic_timeout = conf->fast_periodic_ms * ms_ticks;
	mode4->slow_periodic_timeout = conf->slow_periodic_ms * ms_ticks;
	mode4->short_timeout = conf->short_timeout_ms * ms_ticks;
	mode4->long_timeout = conf->long_timeout_ms * ms_ticks;
	mode4->aggregate_wait_timeout = conf->aggregate_wait_timeout_ms * ms_ticks;
	mode4->tx_period_timeout = conf->tx_period_ms * ms_ticks;
	mode4->rx_marker_timeout = conf->rx_marker_period_ms * ms_ticks;
	mode4->update_timeout_us = conf->update_timeout_ms * 1000;
}

int
bond_mode_8023ad_enable(struct rte_eth_dev *bond_dev)
{
	struct bond_dev_private *internals = bond_dev->data->dev_private;
	uint8_t i;

	for (i = 0; i < internals->active_slave_count; i++)
		bond_mode_8023ad_activate_slave(bond_dev, i);

	return 0;
}

int
bond_mode_8023ad_start(struct rte_eth_dev *bond_dev)
{
	return rte_eal_alarm_set(BOND_MODE_8023AX_UPDATE_TIMEOUT_MS * 1000,
			&bond_mode_8023ad_periodic_cb, bond_dev);
}

void
bond_mode_8023ad_stop(struct rte_eth_dev *bond_dev)
{
	rte_eal_alarm_cancel(&bond_mode_8023ad_periodic_cb, bond_dev);
}

void
bond_mode_8023ad_handle_slow_pkt(struct bond_dev_private *internals,
	uint8_t slave_id, struct rte_mbuf *pkt)
{
	struct mode8023ad_private *mode4 = &internals->mode4;
	struct port *port = &mode_8023ad_ports[slave_id];
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

		/* Setup marker timer. Do it in loop in case concurent access. */
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

		if (unlikely(rte_ring_enqueue(port->tx_ring, pkt) == -ENOBUFS)) {
			/* reset timer */
			port->rx_marker_timer = 0;
			wrn = WRN_TX_QUEUE_FULL;
			goto free_out;
		}
	} else if (likely(subtype == SLOW_SUBTYPE_LACP)) {
		if (unlikely(rte_ring_enqueue(port->rx_ring, pkt) == -ENOBUFS)) {
			/* If RX fing full free lacpdu message and drop packet */
			wrn = WRN_RX_QUEUE_FULL;
			goto free_out;
		}
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
rte_eth_bond_8023ad_conf_get(uint8_t port_id,
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
rte_eth_bond_8023ad_setup(uint8_t port_id,
		struct rte_eth_bond_8023ad_conf *conf)
{
	struct rte_eth_dev *bond_dev;

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
			RTE_LOG(ERR, PMD, "given mode 4 configuration is invalid\n");
			return -EINVAL;
		}
	}

	bond_dev = &rte_eth_devices[port_id];
	bond_mode_8023ad_setup(bond_dev, conf);

	return 0;
}

int
rte_eth_bond_8023ad_slave_info(uint8_t port_id, uint8_t slave_id,
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

	port = &mode_8023ad_ports[slave_id];
	info->selected = port->selected;

	info->actor_state = port->actor_state;
	rte_memcpy(&info->actor, &port->actor, sizeof(port->actor));

	info->partner_state = port->partner_state;
	rte_memcpy(&info->partner, &port->partner, sizeof(port->partner));

	info->agg_port_id = port->aggregator_port_id;
	return 0;
}
