/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Marvell.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <setjmp.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdbool.h>

#include <rte_common.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_memory.h>
#include <rte_memcpy.h>
#include <rte_eal.h>
#include <rte_launch.h>
#include <rte_cycles.h>
#include <rte_prefetch.h>
#include <rte_lcore.h>
#include <rte_per_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_interrupts.h>
#include <rte_random.h>
#include <rte_debug.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_security.h>
#include <rte_string_fns.h>

static volatile bool force_quit;

/* MAC updating enabled by default */
static int mac_updating;

/* Ports set in promiscuous mode on by default. */
static int promiscuous_on = 1;

#define RTE_LOGTYPE_L2FWD RTE_LOGTYPE_USER1

#define MAX_PKT_BURST 32
#define BURST_TX_DRAIN_US 100 /* TX drain every ~100us */
#define MEMPOOL_CACHE_SIZE 256
#define SESSION_POOL_CACHE_SIZE 0

/* Configurable number of RX/TX ring descriptors */
#define RX_DESC_DEFAULT 1024
#define TX_DESC_DEFAULT 1024
static uint16_t nb_rxd = RX_DESC_DEFAULT;
static uint16_t nb_txd = TX_DESC_DEFAULT;

/* ethernet addresses of ports */
static struct rte_ether_addr l2fwd_ports_eth_addr[RTE_MAX_ETHPORTS];

/* Ethernet header configuration for MACsec flow on each port. */
static struct rte_ether_hdr port_ether_hdr_config[RTE_MAX_ETHPORTS];

/* mask of enabled ports */
static uint32_t l2fwd_enabled_port_mask;

/* list of enabled ports */
static uint32_t l2fwd_dst_ports[RTE_MAX_ETHPORTS];

struct port_pair_params {
#define NUM_PORTS	2
	uint16_t port[NUM_PORTS];
} __rte_cache_aligned;

static struct port_pair_params port_pair_params_array[RTE_MAX_ETHPORTS / 2];
static struct port_pair_params *port_pair_params;
static uint16_t nb_port_pair_params;

static unsigned int l2fwd_rx_queue_per_lcore = 1;

#define MAX_RX_QUEUE_PER_LCORE 16
#define MAX_TX_QUEUE_PER_PORT 16
/* List of queues to be polled for a given lcore. 8< */
struct lcore_queue_conf {
	unsigned int n_rx_port;
	unsigned int rx_port_list[MAX_RX_QUEUE_PER_LCORE];
} __rte_cache_aligned;
struct lcore_queue_conf lcore_queue_conf[RTE_MAX_LCORE];
/* >8 End of list of queues to be polled for a given lcore. */

static struct rte_eth_dev_tx_buffer *tx_buffer[RTE_MAX_ETHPORTS];

static struct rte_eth_conf port_conf = {
	.txmode = {
		.mq_mode = RTE_ETH_MQ_TX_NONE,
	},
};

/* Mempools for mbuf and security session */
struct rte_mempool *l2fwd_pktmbuf_pool;

/* Per-port statistics struct */
struct l2fwd_port_statistics {
	uint64_t tx;
	uint64_t rx;
	uint64_t dropped;
} __rte_cache_aligned;
struct l2fwd_port_statistics port_statistics[RTE_MAX_ETHPORTS];

#define MAX_TIMER_PERIOD 86400 /* 1 day max */
/* A tsc-based timer responsible for triggering statistics printout */
static uint64_t timer_period = 10; /* default period is 10 seconds */

#define MCS_MAX_KEY_LEN		32
#define MCS_SALT_LEN		12

struct l2fwd_key {
	uint8_t data[MCS_MAX_KEY_LEN];
	uint32_t len;
	rte_iova_t phys_addr;
};

/** l2fwd macsec application command line options */
struct l2fwd_macsec_options {
	unsigned int portmask;
	unsigned int tx_portmask;
	unsigned int rx_portmask;
	unsigned int nb_ports_per_lcore;
	unsigned int refresh_period;
	unsigned int single_lcore:1;
};

/** l2fwd macsec lcore params */
struct l2fwd_macsec_port_params {
	uint8_t dev_id;
	uint8_t qp_id;
	void *sec_ctx;
	struct rte_mempool *sess_pool;

	void *sess;
	uint16_t sa_id[4];
	uint16_t sc_id;
	struct rte_flow *tx_flow;
	struct rte_flow *rx_flow;

	enum rte_security_macsec_direction dir;
	enum rte_security_macsec_alg alg;
	struct l2fwd_key sa_key;
	uint8_t salt[MCS_SALT_LEN];

	uint8_t eth_hdr[RTE_ETHER_HDR_LEN];
	uint32_t ssci;
	uint64_t sci;
	uint64_t pn_threshold;
	uint32_t xpn;
	uint32_t next_pn;
	uint32_t mtu;
	uint8_t sectag_insert_mode;
	bool encrypt;
	bool protect_frames;
	bool replay_protect;
	int val_frames;
	uint32_t replay_win_sz;
	bool send_sci;
	bool end_station;
	bool scb;
	uint8_t an;
};
struct l2fwd_macsec_port_params mcs_port_params[RTE_MAX_ETHPORTS];

static void
mcs_stats_dump(uint16_t portid)
{
	struct rte_security_stats sess_stats = {0};
	struct rte_security_macsec_secy_stats *secy_stat;
	struct rte_security_macsec_sc_stats sc_stat = {0};

	if (mcs_port_params[portid].dir == RTE_SECURITY_MACSEC_DIR_RX) {
		printf("\n********* RX SECY STATS ************\n");
		rte_security_session_stats_get(mcs_port_params[portid].sec_ctx,
				mcs_port_params[portid].sess, &sess_stats);
		secy_stat = &sess_stats.macsec;

		if (secy_stat->ctl_pkt_bcast_cnt)
			printf("RX: ctl_pkt_bcast_cnt: 0x%" PRIx64 "\n",
					secy_stat->ctl_pkt_bcast_cnt);
		if (secy_stat->ctl_pkt_mcast_cnt)
			printf("RX: ctl_pkt_mcast_cnt: 0x%" PRIx64 "\n",
					secy_stat->ctl_pkt_mcast_cnt);
		if (secy_stat->ctl_pkt_ucast_cnt)
			printf("RX: ctl_pkt_ucast_cnt: 0x%" PRIx64 "\n",
					secy_stat->ctl_pkt_ucast_cnt);
		if (secy_stat->ctl_octet_cnt)
			printf("RX: ctl_octet_cnt: 0x%" PRIx64 "\n", secy_stat->ctl_octet_cnt);
		if (secy_stat->unctl_pkt_bcast_cnt)
			printf("RX: unctl_pkt_bcast_cnt: 0x%" PRIx64 "\n",
					secy_stat->unctl_pkt_bcast_cnt);
		if (secy_stat->unctl_pkt_mcast_cnt)
			printf("RX: unctl_pkt_mcast_cnt: 0x%" PRIx64 "\n",
					secy_stat->unctl_pkt_mcast_cnt);
		if (secy_stat->unctl_pkt_ucast_cnt)
			printf("RX: unctl_pkt_ucast_cnt: 0x%" PRIx64 "\n",
					secy_stat->unctl_pkt_ucast_cnt);
		if (secy_stat->unctl_octet_cnt)
			printf("RX: unctl_octet_cnt: 0x%" PRIx64 "\n", secy_stat->unctl_octet_cnt);
		/* Valid only for RX */
		if (secy_stat->octet_decrypted_cnt)
			printf("RX: octet_decrypted_cnt: 0x%" PRIx64 "\n",
					secy_stat->octet_decrypted_cnt);
		if (secy_stat->octet_validated_cnt)
			printf("RX: octet_validated_cnt: 0x%" PRIx64 "\n",
					secy_stat->octet_validated_cnt);
		if (secy_stat->pkt_port_disabled_cnt)
			printf("RX: pkt_port_disabled_cnt: 0x%" PRIx64 "\n",
					secy_stat->pkt_port_disabled_cnt);
		if (secy_stat->pkt_badtag_cnt)
			printf("RX: pkt_badtag_cnt: 0x%" PRIx64 "\n", secy_stat->pkt_badtag_cnt);
		if (secy_stat->pkt_nosa_cnt)
			printf("RX: pkt_nosa_cnt: 0x%" PRIx64 "\n", secy_stat->pkt_nosa_cnt);
		if (secy_stat->pkt_nosaerror_cnt)
			printf("RX: pkt_nosaerror_cnt: 0x%" PRIx64 "\n",
					secy_stat->pkt_nosaerror_cnt);
		if (secy_stat->pkt_tagged_ctl_cnt)
			printf("RX: pkt_tagged_ctl_cnt: 0x%" PRIx64 "\n",
					secy_stat->pkt_tagged_ctl_cnt);
		if (secy_stat->pkt_untaged_cnt)
			printf("RX: pkt_untaged_cnt: 0x%" PRIx64 "\n", secy_stat->pkt_untaged_cnt);
		if (secy_stat->pkt_ctl_cnt)
			printf("RX: pkt_ctl_cnt: 0x%" PRIx64 "\n", secy_stat->pkt_ctl_cnt);
		if (secy_stat->pkt_notag_cnt)
			printf("RX: pkt_notag_cnt: 0x%" PRIx64 "\n", secy_stat->pkt_notag_cnt);
		printf("\n");
		printf("\n********** RX SC[%u] STATS **************\n",
				mcs_port_params[portid].sc_id);

		rte_security_macsec_sc_stats_get(mcs_port_params[portid].sec_ctx,
				mcs_port_params[portid].sc_id, RTE_SECURITY_MACSEC_DIR_RX,
						 &sc_stat);
		/* RX */
		if (sc_stat.hit_cnt)
			printf("RX hit_cnt: 0x%" PRIx64 "\n", sc_stat.hit_cnt);
		if (sc_stat.pkt_invalid_cnt)
			printf("RX pkt_invalid_cnt: 0x%" PRIx64 "\n", sc_stat.pkt_invalid_cnt);
		if (sc_stat.pkt_late_cnt)
			printf("RX pkt_late_cnt: 0x%" PRIx64 "\n", sc_stat.pkt_late_cnt);
		if (sc_stat.pkt_notvalid_cnt)
			printf("RX pkt_notvalid_cnt: 0x%" PRIx64 "\n", sc_stat.pkt_notvalid_cnt);
		if (sc_stat.pkt_unchecked_cnt)
			printf("RX pkt_unchecked_cnt: 0x%" PRIx64 "\n", sc_stat.pkt_unchecked_cnt);
		if (sc_stat.pkt_delay_cnt)
			printf("RX pkt_delay_cnt: 0x%" PRIx64 "\n", sc_stat.pkt_delay_cnt);
		if (sc_stat.pkt_ok_cnt)
			printf("RX pkt_ok_cnt: 0x%" PRIx64 "\n", sc_stat.pkt_ok_cnt);
		if (sc_stat.octet_decrypt_cnt)
			printf("RX octet_decrypt_cnt: 0x%" PRIx64 "\n", sc_stat.octet_decrypt_cnt);
		if (sc_stat.octet_validate_cnt)
			printf("RX octet_validate_cnt: 0x%" PRIx64 "\n",
					sc_stat.octet_validate_cnt);
	}

	if (mcs_port_params[portid].dir == RTE_SECURITY_MACSEC_DIR_TX) {
		memset(&sess_stats, 0, sizeof(struct rte_security_stats));
		rte_security_session_stats_get(mcs_port_params[portid].sec_ctx,
				mcs_port_params[portid].sess, &sess_stats);
		secy_stat = &sess_stats.macsec;

		printf("\n********* TX SECY STATS ************\n");
		if (secy_stat->ctl_pkt_bcast_cnt)
			printf("TX: ctl_pkt_bcast_cnt: 0x%" PRIx64 "\n",
					secy_stat->ctl_pkt_bcast_cnt);
		if (secy_stat->ctl_pkt_mcast_cnt)
			printf("TX: ctl_pkt_mcast_cnt: 0x%" PRIx64 "\n",
					secy_stat->ctl_pkt_mcast_cnt);
		if (secy_stat->ctl_pkt_ucast_cnt)
			printf("TX: ctl_pkt_ucast_cnt: 0x%" PRIx64 "\n",
					secy_stat->ctl_pkt_ucast_cnt);
		if (secy_stat->ctl_octet_cnt)
			printf("TX: ctl_octet_cnt: 0x%" PRIx64 "\n", secy_stat->ctl_octet_cnt);
		if (secy_stat->unctl_pkt_bcast_cnt)
			printf("TX: unctl_pkt_bcast_cnt: 0x%" PRIx64 "\n",
					secy_stat->unctl_pkt_bcast_cnt);
		if (secy_stat->unctl_pkt_mcast_cnt)
			printf("TX: unctl_pkt_mcast_cnt: 0x%" PRIx64 "\n",
					secy_stat->unctl_pkt_mcast_cnt);
		if (secy_stat->unctl_pkt_ucast_cnt)
			printf("TX: unctl_pkt_ucast_cnt: 0x%" PRIx64 "\n",
					secy_stat->unctl_pkt_ucast_cnt);
		if (secy_stat->unctl_octet_cnt)
			printf("TX: unctl_octet_cnt: 0x%" PRIx64 "\n",
					secy_stat->unctl_octet_cnt);
		/* Valid only for TX */
		if (secy_stat->octet_encrypted_cnt)
			printf("TX: octet_encrypted_cnt: 0x%" PRIx64 "\n",
					secy_stat->octet_encrypted_cnt);
		if (secy_stat->octet_protected_cnt)
			printf("TX: octet_protected_cnt: 0x%" PRIx64 "\n",
					secy_stat->octet_protected_cnt);
		if (secy_stat->pkt_noactivesa_cnt)
			printf("TX: pkt_noactivesa_cnt: 0x%" PRIx64 "\n",
					secy_stat->pkt_noactivesa_cnt);
		if (secy_stat->pkt_toolong_cnt)
			printf("TX: pkt_toolong_cnt: 0x%" PRIx64 "\n", secy_stat->pkt_toolong_cnt);
		if (secy_stat->pkt_untagged_cnt)
			printf("TX: pkt_untagged_cnt: 0x%" PRIx64 "\n",
					secy_stat->pkt_untagged_cnt);


		memset(&sc_stat, 0, sizeof(struct rte_security_macsec_sc_stats));
		rte_security_macsec_sc_stats_get(mcs_port_params[portid].sec_ctx,
				mcs_port_params[portid].sc_id, RTE_SECURITY_MACSEC_DIR_TX,
				&sc_stat);
		printf("\n********** TX SC[%u] STATS **************\n",
				mcs_port_params[portid].sc_id);
		if (sc_stat.pkt_encrypt_cnt)
			printf("TX pkt_encrypt_cnt: 0x%" PRIx64 "\n", sc_stat.pkt_encrypt_cnt);
		if (sc_stat.pkt_protected_cnt)
			printf("TX pkt_protected_cnt: 0x%" PRIx64 "\n", sc_stat.pkt_protected_cnt);
		if (sc_stat.octet_encrypt_cnt)
			printf("TX octet_encrypt_cnt: 0x%" PRIx64 "\n", sc_stat.octet_encrypt_cnt);
	}
}

/* Print out statistics on packets dropped */
static void
print_stats(void)
{
	uint64_t total_packets_dropped, total_packets_tx, total_packets_rx;
	unsigned int portid;

	total_packets_dropped = 0;
	total_packets_tx = 0;
	total_packets_rx = 0;

	const char clr[] = { 27, '[', '2', 'J', '\0' };
	const char topLeft[] = { 27, '[', '1', ';', '1', 'H', '\0' };

		/* Clear screen and move to top left */
	printf("%s%s", clr, topLeft);

	printf("\nPort statistics ====================================");

	for (portid = 0; portid < RTE_MAX_ETHPORTS; portid++) {
		/* skip disabled ports */
		if ((l2fwd_enabled_port_mask & (1 << portid)) == 0)
			continue;
		printf("\nStatistics for port %u ------------------------------"
			   "\nPackets sent: %24"PRIu64
			   "\nPackets received: %20"PRIu64
			   "\nPackets dropped: %21"PRIu64,
			   portid,
			   port_statistics[portid].tx,
			   port_statistics[portid].rx,
			   port_statistics[portid].dropped);

		total_packets_dropped += port_statistics[portid].dropped;
		total_packets_tx += port_statistics[portid].tx;
		total_packets_rx += port_statistics[portid].rx;

		mcs_stats_dump(portid);
	}
	printf("\nAggregate statistics ==============================="
		   "\nTotal packets sent: %18"PRIu64
		   "\nTotal packets received: %14"PRIu64
		   "\nTotal packets dropped: %15"PRIu64,
		   total_packets_tx,
		   total_packets_rx,
		   total_packets_dropped);
	printf("\n====================================================\n");

	fflush(stdout);
}

static void
l2fwd_mac_updating(struct rte_mbuf *m, unsigned int dest_portid)
{
	struct rte_ether_hdr *eth;
	void *tmp;

	eth = rte_pktmbuf_mtod(m, struct rte_ether_hdr *);

	/* 02:00:00:00:00:xx */
	tmp = &eth->dst_addr.addr_bytes[0];
	*((uint64_t *)tmp) = 0x000000000002 + ((uint64_t)dest_portid << 40);

	/* src addr */
	rte_ether_addr_copy(&l2fwd_ports_eth_addr[dest_portid], &eth->src_addr);
}

static void
l2fwd_simple_forward(struct rte_mbuf *m, unsigned int portid)
{
	unsigned int dst_port;
	int sent;
	struct rte_eth_dev_tx_buffer *buffer;

	dst_port = l2fwd_dst_ports[portid];

	if (mac_updating)
		l2fwd_mac_updating(m, dst_port);

	buffer = tx_buffer[dst_port];
	sent = rte_eth_tx_buffer(dst_port, 0, buffer, m);
	if (sent)
		port_statistics[dst_port].tx += sent;
}

static void
fill_macsec_sa_conf(uint16_t portid, struct rte_security_macsec_sa *sa)
{
	sa->dir = mcs_port_params[portid].dir;

	sa->key.data = mcs_port_params[portid].sa_key.data;
	sa->key.length = mcs_port_params[portid].sa_key.len;

	memcpy((uint8_t *)sa->salt, (const uint8_t *)mcs_port_params[portid].salt,
		RTE_SECURITY_MACSEC_SALT_LEN);

	/* AN is set as per the value in secure packet in test vector */
	sa->an = mcs_port_params[portid].an & RTE_MACSEC_AN_MASK;

	sa->ssci = mcs_port_params[portid].ssci;
	sa->xpn = mcs_port_params[portid].xpn;
	/* Starting packet number which is expected to come next.
	 * It is take from the test vector so that we can match the out packet.
	 */
	sa->next_pn = mcs_port_params[portid].next_pn;
}

static void
fill_macsec_sc_conf(uint16_t portid, struct rte_security_macsec_sc *sc_conf)
{
	uint8_t i;

	sc_conf->dir = mcs_port_params[portid].dir;
	sc_conf->pn_threshold = mcs_port_params[portid].pn_threshold;
	if (sc_conf->dir == RTE_SECURITY_MACSEC_DIR_TX) {
		sc_conf->sc_tx.sa_id = mcs_port_params[portid].sa_id[0];
		if (mcs_port_params[portid].sa_id[1] != 0xFFFF) {
			sc_conf->sc_tx.sa_id_rekey = mcs_port_params[portid].sa_id[1];
			sc_conf->sc_tx.re_key_en = 1;
		}
		sc_conf->sc_tx.active = 1;
		sc_conf->sc_tx.sci = mcs_port_params[portid].sci;
		if (mcs_port_params[portid].xpn > 0)
			sc_conf->sc_tx.is_xpn = 1;
	} else {
		for (i = 0; i < RTE_SECURITY_MACSEC_NUM_AN; i++) {
			if (mcs_port_params[portid].sa_id[i] != 0xFFFF) {
				sc_conf->sc_rx.sa_id[i] = mcs_port_params[portid].sa_id[i];
				sc_conf->sc_rx.sa_in_use[i] = 1;
			}
		}
		sc_conf->sc_rx.active = 1;
		if (mcs_port_params[portid].xpn > 0)
			sc_conf->sc_rx.is_xpn = 1;
	}
}

/* Create Inline MACsec session */
static int
fill_session_conf(uint16_t portid, struct rte_security_session_conf *sess_conf)
{
	sess_conf->action_type = RTE_SECURITY_ACTION_TYPE_INLINE_PROTOCOL;
	sess_conf->protocol = RTE_SECURITY_PROTOCOL_MACSEC;
	sess_conf->macsec.dir = mcs_port_params[portid].dir;
	sess_conf->macsec.alg = mcs_port_params[portid].alg;
	sess_conf->macsec.cipher_off = 0;
	sess_conf->macsec.sci = mcs_port_params[portid].sci;
	sess_conf->macsec.sc_id = mcs_port_params[portid].sc_id;
	if (mcs_port_params[portid].dir == RTE_SECURITY_MACSEC_DIR_TX) {
		sess_conf->macsec.tx_secy.mtu = mcs_port_params[portid].mtu;
		sess_conf->macsec.tx_secy.sectag_off =
			(mcs_port_params[portid].sectag_insert_mode == 1) ?
					2 * RTE_ETHER_ADDR_LEN : RTE_VLAN_HLEN;
		sess_conf->macsec.tx_secy.sectag_insert_mode =
			mcs_port_params[portid].sectag_insert_mode;
		sess_conf->macsec.tx_secy.ctrl_port_enable = 1;
		sess_conf->macsec.tx_secy.sectag_version = 0;
		sess_conf->macsec.tx_secy.end_station = mcs_port_params[portid].end_station;
		sess_conf->macsec.tx_secy.send_sci = mcs_port_params[portid].send_sci;
		sess_conf->macsec.tx_secy.scb = mcs_port_params[portid].scb;
		sess_conf->macsec.tx_secy.encrypt = mcs_port_params[portid].encrypt;
		sess_conf->macsec.tx_secy.protect_frames = mcs_port_params[portid].protect_frames;
		sess_conf->macsec.tx_secy.icv_include_da_sa = 1;
	} else {
		sess_conf->macsec.rx_secy.replay_win_sz = mcs_port_params[portid].replay_win_sz;
		sess_conf->macsec.rx_secy.replay_protect = mcs_port_params[portid].replay_protect;
		sess_conf->macsec.rx_secy.icv_include_da_sa = 1;
		sess_conf->macsec.rx_secy.ctrl_port_enable = 1;
		sess_conf->macsec.rx_secy.preserve_sectag = 0;
		sess_conf->macsec.rx_secy.preserve_icv = 0;
		sess_conf->macsec.rx_secy.validate_frames = mcs_port_params[portid].val_frames;
	}

	return 0;
}

static int
create_default_flow(uint16_t portid)
{
	struct rte_flow_action action[2];
	struct rte_flow_item pattern[2];
	struct rte_flow_attr attr = {0};
	struct rte_flow_error err;
	struct rte_flow *flow;
	struct rte_flow_item_eth eth;
	static const struct rte_flow_item_eth eth_mask = {
		.hdr.dst_addr.addr_bytes = "\x00\x00\x00\x00\x00\x00",
		.hdr.src_addr.addr_bytes = "\x00\x00\x00\x00\x00\x00",
		.hdr.ether_type = RTE_BE16(0xFFFF),
	};
	int ret;

	eth.has_vlan = 0;
	memcpy(&eth.hdr, mcs_port_params[portid].eth_hdr, RTE_ETHER_HDR_LEN);

	printf("Creating flow on port %u with DST MAC address: " RTE_ETHER_ADDR_PRT_FMT
			", SRC MAC address: "RTE_ETHER_ADDR_PRT_FMT"\n\n",
			portid,
			RTE_ETHER_ADDR_BYTES(&eth.hdr.dst_addr),
			RTE_ETHER_ADDR_BYTES(&eth.hdr.src_addr));

	pattern[0].type = RTE_FLOW_ITEM_TYPE_ETH;
	pattern[0].spec = &eth;
	pattern[0].mask = &eth_mask;
	pattern[0].last = NULL;
	pattern[1].type = RTE_FLOW_ITEM_TYPE_END;

	action[0].type = RTE_FLOW_ACTION_TYPE_SECURITY;
	action[0].conf = mcs_port_params[portid].sess;
	action[1].type = RTE_FLOW_ACTION_TYPE_END;
	action[1].conf = NULL;

	attr.ingress = (mcs_port_params[portid].dir == RTE_SECURITY_MACSEC_DIR_RX) ? 1 : 0;
	attr.egress = (mcs_port_params[portid].dir == RTE_SECURITY_MACSEC_DIR_TX) ? 1 : 0;

	ret = rte_flow_validate(portid, &attr, pattern, action, &err);
	if (ret) {
		printf("\nValidate flow failed, ret = %d\n", ret);
		return -1;
	}
	flow = rte_flow_create(portid, &attr, pattern, action, &err);
	if (flow == NULL) {
		printf("\nDefault flow rule create failed\n");
		return -1;
	}

	if (mcs_port_params[portid].dir == RTE_SECURITY_MACSEC_DIR_TX)
		mcs_port_params[portid].tx_flow = flow;
	else
		mcs_port_params[portid].rx_flow = flow;

	return 0;
}

static void
destroy_default_flow(uint16_t portid)
{
	struct rte_flow_error err;
	int ret;

	if (mcs_port_params[portid].tx_flow) {
		ret = rte_flow_destroy(portid, mcs_port_params[portid].tx_flow, &err);
		if (ret) {
			printf("\nDefault Tx flow rule destroy failed\n");
			return;
		}
		mcs_port_params[portid].tx_flow = NULL;
	}
	if (mcs_port_params[portid].rx_flow) {
		ret = rte_flow_destroy(portid, mcs_port_params[portid].rx_flow, &err);
		if (ret) {
			printf("\nDefault Rx flow rule destroy failed\n");
			return;
		}
		mcs_port_params[portid].rx_flow = NULL;
	}
}

static void
clean_macsec_resources(uint16_t portid)
{
	uint8_t an;

	destroy_default_flow(portid);
	rte_security_session_destroy(mcs_port_params[portid].sec_ctx,
				mcs_port_params[portid].sess);
	rte_security_macsec_sc_destroy(mcs_port_params[portid].sec_ctx,
				mcs_port_params[portid].sc_id,
				mcs_port_params[portid].dir);
	for (an = 0; an < RTE_SECURITY_MACSEC_NUM_AN; an++) {
		if (mcs_port_params[portid].sa_id[an] != 0xFFFF)
			rte_security_macsec_sa_destroy(mcs_port_params[portid].sec_ctx,
				mcs_port_params[portid].sa_id[an],
				mcs_port_params[portid].dir);
	}
}

static int
initialize_macsec_session(uint8_t portid)
{
	struct rte_security_session_conf sess_conf = {0};
	struct rte_security_macsec_sa sa_conf = {0};
	struct rte_security_macsec_sc sc_conf = {0};
	int id, ret;

	/* Create MACsec SA. */
	fill_macsec_sa_conf(portid, &sa_conf);
	id = rte_security_macsec_sa_create(mcs_port_params[portid].sec_ctx, &sa_conf);
	if (id < 0) {
		printf("MACsec SA create failed : %d.\n", id);
		return -1;
	}
	mcs_port_params[portid].sa_id[0] = (uint16_t)id;
	mcs_port_params[portid].sa_id[1] = 0xFFFF;
	mcs_port_params[portid].sa_id[2] = 0xFFFF;
	mcs_port_params[portid].sa_id[3] = 0xFFFF;

	printf("\nsa_id %d created.\n", mcs_port_params[portid].sa_id[0]);

	/* Create MACsec SC. */
	fill_macsec_sc_conf(portid, &sc_conf);
	id = rte_security_macsec_sc_create(mcs_port_params[portid].sec_ctx, &sc_conf);
	if (id < 0) {
		printf("MACsec SC create failed : %d.\n", id);
		goto out;
	}
	mcs_port_params[portid].sc_id = (uint16_t)id;
	printf("\nsc_id %d created.\n", mcs_port_params[portid].sc_id);

	/* Create Inline MACsec session. */
	ret = fill_session_conf(portid, &sess_conf);
	if (ret) {
		printf("MACsec Session conf failed.\n");
		goto out;
	}
	mcs_port_params[portid].sess =
		rte_security_session_create(mcs_port_params[portid].sec_ctx,
				&sess_conf, mcs_port_params[portid].sess_pool);
	if (mcs_port_params[portid].sess == NULL) {
		printf("MACSEC Session init failed errno: %d.\n", rte_errno);
		goto out;
	}

	/* Create MACsec flow. */
	ret = create_default_flow(portid);
	if (ret)
		goto out;

	return 0;
out:
	clean_macsec_resources(portid);
	return -1;
}

/* main processing loop */
static void
l2fwd_main_loop(void)
{
	struct rte_mbuf *pkts_burst[MAX_PKT_BURST];
	struct rte_mbuf *m;
	int sent;
	unsigned int lcore_id;
	uint64_t prev_tsc, diff_tsc, cur_tsc, timer_tsc;
	unsigned int i, j, portid, nb_rx;
	struct lcore_queue_conf *qconf;
	const uint64_t drain_tsc = (rte_get_tsc_hz() + US_PER_S - 1) / US_PER_S *
			BURST_TX_DRAIN_US;
	struct rte_eth_dev_tx_buffer *buffer;

	prev_tsc = 0;
	timer_tsc = 0;

	lcore_id = rte_lcore_id();
	qconf = &lcore_queue_conf[lcore_id];

	if (qconf->n_rx_port == 0) {
		RTE_LOG(INFO, L2FWD, "lcore %u has nothing to do\n", lcore_id);
		return;
	}

	RTE_LOG(INFO, L2FWD, "entering main loop on lcore %u\n", lcore_id);

	for (i = 0; i < qconf->n_rx_port; i++) {

		portid = qconf->rx_port_list[i];

		RTE_LOG(INFO, L2FWD, " -- lcoreid=%u portid=%u\n", lcore_id,
			portid);
	}

	while (!force_quit) {

		/* Drains TX queue in its main loop. 8< */
		cur_tsc = rte_rdtsc();

		/*
		 * TX burst queue drain
		 */
		diff_tsc = cur_tsc - prev_tsc;
		if (unlikely(diff_tsc > drain_tsc)) {

			for (i = 0; i < qconf->n_rx_port; i++) {

				portid = l2fwd_dst_ports[qconf->rx_port_list[i]];
				buffer = tx_buffer[portid];

				sent = rte_eth_tx_buffer_flush(portid, 0, buffer);
				if (sent)
					port_statistics[portid].tx += sent;

			}

			/* if timer is enabled */
			if (timer_period > 0) {

				/* advance the timer */
				timer_tsc += diff_tsc;

				/* if timer has reached its timeout */
				if (unlikely(timer_tsc >= timer_period)) {

					/* do this only on main core */
					if (lcore_id == rte_get_main_lcore()) {
						print_stats();
						/* reset the timer */
						timer_tsc = 0;
					}
				}
			}

			prev_tsc = cur_tsc;
		}
		/* >8 End of draining TX queue. */

		/* Read packet from RX queues. 8< */
		for (i = 0; i < qconf->n_rx_port; i++) {

			portid = qconf->rx_port_list[i];
			nb_rx = rte_eth_rx_burst(portid, 0,
						 pkts_burst, MAX_PKT_BURST);

			if (unlikely(nb_rx == 0))
				continue;

			port_statistics[portid].rx += nb_rx;

			for (j = 0; j < nb_rx; j++) {
				m = pkts_burst[j];
				rte_prefetch0(rte_pktmbuf_mtod(m, void *));
				l2fwd_simple_forward(m, portid);
			}
		}
		/* >8 End of read packet from RX queues. */
	}
	if (force_quit) {
		for (i = 0; i < qconf->n_rx_port; i++) {
			portid = qconf->rx_port_list[i];
			clean_macsec_resources(portid);
		}
	}
}
static int
l2fwd_launch_one_lcore(__rte_unused void *arg)
{
	l2fwd_main_loop();
	return 0;
}

/* display usage */
static void
l2fwd_usage(const char *prgname)
{
	printf("%s [EAL options] -- -p PORTMASK [-q NQ]\n"
	       "  -p PORTMASK: hexadecimal bitmask of ports to configure\n"
	       "  -q NQ: number of queue (=ports) per lcore (default is 1)\n"
	       "  -T PERIOD: statistics will be refreshed each PERIOD seconds (0 to disable, 10 default, 86400 maximum)\n"
	       "  --mcs-tx-portmask: Hexadecimal bitmask for MACsec Tx(Encap) ports\n"
	       "  --mcs-rx-portmask: Hexadecimal bitmask for MACsec Rx(Decap) ports\n"
	       "  --mcs-port-config '(<port>,<src-mac>,<dst-mac>)'\n"
	       "  --portmap: Configure forwarding port pair mapping\n"
	       "	      Default: alternate port pairs\n\n",
	       prgname);
}

static int
l2fwd_parse_portmask(const char *portmask)
{
	char *end = NULL;
	unsigned long pm;

	/* parse hexadecimal string */
	pm = strtoul(portmask, &end, 16);
	if ((portmask[0] == '\0') || (end == NULL) || (*end != '\0'))
		return 0;

	return pm;
}

static int
l2fwd_parse_port_pair_config(const char *q_arg)
{
	enum fieldnames {
		FLD_PORT1 = 0,
		FLD_PORT2,
		_NUM_FLD
	};
	unsigned long int_fld[_NUM_FLD];
	const char *p, *p0 = q_arg;
	char *str_fld[_NUM_FLD];
	unsigned int size;
	char s[256];
	char *end;
	int i;

	nb_port_pair_params = 0;

	while ((p = strchr(p0, '(')) != NULL) {
		++p;
		p0 = strchr(p, ')');
		if (p0 == NULL)
			return -1;

		size = p0 - p;
		if (size >= sizeof(s))
			return -1;

		memcpy(s, p, size);
		s[size] = '\0';
		if (rte_strsplit(s, sizeof(s), str_fld,
				 _NUM_FLD, ',') != _NUM_FLD)
			return -1;
		for (i = 0; i < _NUM_FLD; i++) {
			errno = 0;
			int_fld[i] = strtoul(str_fld[i], &end, 0);
			if (errno != 0 || end == str_fld[i] ||
			    int_fld[i] >= RTE_MAX_ETHPORTS)
				return -1;
		}
		if (nb_port_pair_params >= RTE_MAX_ETHPORTS/2) {
			printf("exceeded max number of port pair params: %hu\n",
				nb_port_pair_params);
			return -1;
		}
		port_pair_params_array[nb_port_pair_params].port[0] =
				(uint16_t)int_fld[FLD_PORT1];
		port_pair_params_array[nb_port_pair_params].port[1] =
				(uint16_t)int_fld[FLD_PORT2];
		++nb_port_pair_params;
	}
	port_pair_params = port_pair_params_array;
	return 0;
}

static int
l2fwd_parse_macsec_port_config(const char *q_arg)
{
	enum fieldnames {
		FLD_PORT = 0,
		FLD_SRC,
		FLD_DST,
		_NUM_FLD
	};
	unsigned int portid;
	struct rte_ether_addr src, dst;
	const char *p, *p0 = q_arg;
	char *str_fld[_NUM_FLD];
	unsigned int size;
	char s[256];
	char *end;

	nb_port_pair_params = 0;

	while ((p = strchr(p0, '(')) != NULL) {
		++p;
		p0 = strchr(p, ')');
		if (p0 == NULL)
			return -1;

		size = p0 - p;
		if (size >= sizeof(s))
			return -1;

		memcpy(s, p, size);
		s[size] = '\0';
		if (rte_strsplit(s, sizeof(s), str_fld,
				 _NUM_FLD, ',') != _NUM_FLD)
			return -1;
		errno = 0;
		portid = strtoul(str_fld[FLD_PORT], &end, 0);
		if (errno != 0 || end == str_fld[FLD_PORT] || portid >= RTE_MAX_ETHPORTS)
			return -1;
		if (port_ether_hdr_config[portid].ether_type == 0x0800) {
			printf("MACsec src-dst MAC addr already parsed for port: %d\n",
					portid);
			return -1;
		}
		if (rte_ether_unformat_addr(str_fld[FLD_SRC], &src) ||
				rte_ether_unformat_addr(str_fld[FLD_DST], &dst))
			return -1;

		memcpy(&port_ether_hdr_config[portid].src_addr, &src, sizeof(src));
		memcpy(&port_ether_hdr_config[portid].dst_addr, &dst, sizeof(dst));
		port_ether_hdr_config[portid].ether_type = 0x0800;
	}

	return 0;
}


static unsigned int
l2fwd_parse_nqueue(const char *q_arg)
{
	char *end = NULL;
	unsigned long n;

	/* parse hexadecimal string */
	n = strtoul(q_arg, &end, 10);
	if ((q_arg[0] == '\0') || (end == NULL) || (*end != '\0'))
		return 0;
	if (n == 0)
		return 0;
	if (n >= MAX_RX_QUEUE_PER_LCORE)
		return 0;

	return n;
}

static int
l2fwd_parse_timer_period(const char *q_arg)
{
	char *end = NULL;
	int n;

	/* parse number string */
	n = strtol(q_arg, &end, 10);
	if ((q_arg[0] == '\0') || (end == NULL) || (*end != '\0'))
		return -1;
	if (n >= MAX_TIMER_PERIOD)
		return -1;

	return n;
}

static const char short_options[] =
	"p:"  /* portmask */
	"P"   /* promiscuous */
	"q:"  /* number of queues */
	"T:"  /* timer period */
	;

#define CMD_LINE_OPT_NO_MAC_UPDATING	"no-mac-updating"
#define CMD_LINE_OPT_PORTMAP_CONFIG	"portmap"
#define CMD_LINE_OPT_MACSEC_TX_PORTMASK	"mcs-tx-portmask"
#define CMD_LINE_OPT_MACSEC_RX_PORTMASK	"mcs-rx-portmask"
#define CMD_LINE_OPT_MACSEC_PORT_CONFIG	"mcs-port-config"

enum {
	/* Long options mapped to a short option.
	 * First long only option value must be >= 256,
	 * so that we won't conflict with short options.
	 */
	CMD_LINE_OPT_NO_MAC_UPDATING_NUM = 256,
	CMD_LINE_OPT_PORTMAP_NUM,
	CMD_LINE_OPT_MACSEC_TX_PORTMASK_NUM,
	CMD_LINE_OPT_MACSEC_RX_PORTMASK_NUM,
	CMD_LINE_OPT_MACSEC_PORT_CFG_NUM,
};

static const struct option lgopts[] = {
	{ CMD_LINE_OPT_NO_MAC_UPDATING, no_argument, 0,
		CMD_LINE_OPT_NO_MAC_UPDATING_NUM},
	{ CMD_LINE_OPT_PORTMAP_CONFIG, 1, 0, CMD_LINE_OPT_PORTMAP_NUM},
	{ CMD_LINE_OPT_MACSEC_TX_PORTMASK, required_argument, 0,
		CMD_LINE_OPT_MACSEC_TX_PORTMASK_NUM},
	{ CMD_LINE_OPT_MACSEC_RX_PORTMASK, required_argument, 0,
		CMD_LINE_OPT_MACSEC_RX_PORTMASK_NUM},
	{ CMD_LINE_OPT_MACSEC_PORT_CONFIG, 1, 0, CMD_LINE_OPT_MACSEC_PORT_CFG_NUM},
	{NULL, 0, 0, 0}
};

/** Generate default options for application. */
static void
l2fwd_macsec_default_options(struct l2fwd_macsec_options *options)
{
	uint16_t portid;
	uint8_t salt[MCS_SALT_LEN] = {0};
	uint8_t key[16] = {
			0x07, 0x1B, 0x11, 0x3B, 0x0C, 0xA7, 0x43, 0xFE,
			0xCC, 0xCF, 0x3D, 0x05, 0x1F, 0x73, 0x73, 0x82
		};

	options->portmask = 0xffffffff;
	options->nb_ports_per_lcore = 1;
	options->single_lcore = 0;

	RTE_ETH_FOREACH_DEV(portid) {
		if ((l2fwd_enabled_port_mask & (1 << portid)) == 0)
			continue;
		if ((options->tx_portmask & (1 << portid)) != 0)
			mcs_port_params[portid].dir = RTE_SECURITY_MACSEC_DIR_TX;

		if ((options->rx_portmask & (1 << portid)) != 0)
			mcs_port_params[portid].dir = RTE_SECURITY_MACSEC_DIR_RX;

		mcs_port_params[portid].alg = RTE_SECURITY_MACSEC_ALG_GCM_128;
		memcpy(mcs_port_params[portid].sa_key.data, key, 16);
		mcs_port_params[portid].sa_key.len = 16;
		memcpy(mcs_port_params[portid].salt, salt, MCS_SALT_LEN);

		memcpy(mcs_port_params[portid].eth_hdr, &port_ether_hdr_config[portid],
				RTE_ETHER_HDR_LEN);

		mcs_port_params[portid].ssci = 0;
		mcs_port_params[portid].pn_threshold = 0xffffffffffffffff;
		mcs_port_params[portid].xpn = 0;
		mcs_port_params[portid].next_pn = 1;
		mcs_port_params[portid].mtu = 1500;
		mcs_port_params[portid].sectag_insert_mode = 1;
		mcs_port_params[portid].encrypt = true;
		mcs_port_params[portid].protect_frames = true;
		mcs_port_params[portid].replay_protect = false;
		mcs_port_params[portid].val_frames = RTE_SECURITY_MACSEC_VALIDATE_STRICT;
		mcs_port_params[portid].send_sci = true;
		mcs_port_params[portid].end_station = false;
		mcs_port_params[portid].scb = false;
	}
}

/* Parse the argument given in the command line of the application */
static int
l2fwd_parse_args(struct l2fwd_macsec_options *options,
		int argc, char **argv)
{
	int opt, ret, timer_secs;
	char **argvopt;
	int option_index;
	char *prgname = argv[0];

	argvopt = argv;
	port_pair_params = NULL;

	while ((opt = getopt_long(argc, argvopt, short_options,
				  lgopts, &option_index)) != EOF) {

		switch (opt) {
		/* portmask */
		case 'p':
			l2fwd_enabled_port_mask = l2fwd_parse_portmask(optarg);
			if (l2fwd_enabled_port_mask == 0) {
				printf("invalid portmask\n");
				l2fwd_usage(prgname);
				return -1;
			}
			break;
		case 'P':
			promiscuous_on = 1;
			break;

		/* nqueue */
		case 'q':
			l2fwd_rx_queue_per_lcore = l2fwd_parse_nqueue(optarg);
			if (l2fwd_rx_queue_per_lcore == 0) {
				printf("invalid queue number\n");
				l2fwd_usage(prgname);
				return -1;
			}
			break;

		/* timer period */
		case 'T':
			timer_secs = l2fwd_parse_timer_period(optarg);
			if (timer_secs < 0) {
				printf("invalid timer period\n");
				l2fwd_usage(prgname);
				return -1;
			}
			timer_period = timer_secs;
			break;

		/* long options */
		case CMD_LINE_OPT_PORTMAP_NUM:
			ret = l2fwd_parse_port_pair_config(optarg);
			if (ret) {
				fprintf(stderr, "Invalid config\n");
				l2fwd_usage(prgname);
				return -1;
			}
			break;

		case CMD_LINE_OPT_NO_MAC_UPDATING_NUM:
			mac_updating = 0;
			break;

		case CMD_LINE_OPT_MACSEC_TX_PORTMASK_NUM:
			options->tx_portmask = l2fwd_parse_portmask(optarg);
			if (options->tx_portmask == 0) {
				l2fwd_usage(prgname);
				return -1;
			}
			break;

		case CMD_LINE_OPT_MACSEC_RX_PORTMASK_NUM:
			options->rx_portmask = l2fwd_parse_portmask(optarg);
			if (options->rx_portmask == 0) {
				l2fwd_usage(prgname);
				return -1;
			}
			break;

		case CMD_LINE_OPT_MACSEC_PORT_CFG_NUM:
			ret = l2fwd_parse_macsec_port_config(optarg);
			if (ret) {
				fprintf(stderr, "Invalid MACsec port config\n");
				l2fwd_usage(prgname);
				return -1;
			}
			break;

		default:
			l2fwd_usage(prgname);
			return -1;
		}
	}
	l2fwd_macsec_default_options(options);

	if (optind >= 0)
		argv[optind-1] = prgname;

	ret = optind-1;
	optind = 1; /* reset getopt lib */
	return ret;
}

/*
 * Check port pair config with enabled port mask,
 * and for valid port pair combinations.
 */
static int
check_port_pair_config(void)
{
	uint32_t port_pair_config_mask = 0;
	uint32_t port_pair_mask = 0;
	uint16_t index, i, portid;

	for (index = 0; index < nb_port_pair_params; index++) {
		port_pair_mask = 0;

		for (i = 0; i < NUM_PORTS; i++)  {
			portid = port_pair_params[index].port[i];
			if ((l2fwd_enabled_port_mask & (1 << portid)) == 0) {
				printf("port %u is not enabled in port mask\n",
				       portid);
				return -1;
			}
			if (!rte_eth_dev_is_valid_port(portid)) {
				printf("port %u is not present on the board\n",
				       portid);
				return -1;
			}

			port_pair_mask |= 1 << portid;
		}

		if (port_pair_config_mask & port_pair_mask) {
			printf("port %u is used in other port pairs\n", portid);
			return -1;
		}
		port_pair_config_mask |= port_pair_mask;
	}

	l2fwd_enabled_port_mask &= port_pair_config_mask;

	return 0;
}

/* Check the link status of all ports in up to 9s, and print them finally */
static void
check_all_ports_link_status(uint32_t port_mask)
{
#define CHECK_INTERVAL 100 /* 100ms */
#define MAX_CHECK_TIME 90 /* 9s (90 * 100ms) in total */
	uint16_t portid;
	uint8_t count, all_ports_up, print_flag = 0;
	struct rte_eth_link link;
	int ret;
	char link_status_text[RTE_ETH_LINK_MAX_STR_LEN];

	printf("\nChecking link status");
	fflush(stdout);
	for (count = 0; count <= MAX_CHECK_TIME; count++) {
		if (force_quit)
			return;
		all_ports_up = 1;
		RTE_ETH_FOREACH_DEV(portid) {
			if (force_quit)
				return;
			if ((port_mask & (1 << portid)) == 0)
				continue;
			memset(&link, 0, sizeof(link));
			ret = rte_eth_link_get_nowait(portid, &link);
			if (ret < 0) {
				all_ports_up = 0;
				if (print_flag == 1)
					printf("Port %u link get failed: %s\n",
						portid, rte_strerror(-ret));
				continue;
			}
			/* print link status if flag set */
			if (print_flag == 1) {
				rte_eth_link_to_str(link_status_text,
					sizeof(link_status_text), &link);
				printf("Port %d %s\n", portid,
				       link_status_text);
				continue;
			}
			/* clear all_ports_up flag if any link down */
			if (link.link_status == RTE_ETH_LINK_DOWN) {
				all_ports_up = 0;
				break;
			}
		}
		/* after finally printing all link status, get out */
		if (print_flag == 1)
			break;

		if (all_ports_up == 0) {
			printf(".");
			fflush(stdout);
			rte_delay_ms(CHECK_INTERVAL);
		}

		/* set the print_flag if all ports up or timeout */
		if (all_ports_up == 1 || count == (MAX_CHECK_TIME - 1)) {
			print_flag = 1;
			printf("done\n");
		}
	}
}

static void
signal_handler(int signum)
{
	if (signum == SIGINT || signum == SIGTERM) {
		printf("\n\nSignal %d received, preparing to exit...\n",
				signum);
		force_quit = true;
	}
}

int
main(int argc, char **argv)
{
	struct l2fwd_macsec_options options = {0};
	struct lcore_queue_conf *qconf;
	int ret;
	uint16_t nb_ports;
	uint16_t nb_ports_available = 0;
	uint16_t portid, last_port;
	unsigned int lcore_id, rx_lcore_id;
	unsigned int nb_ports_in_mask = 0;
	unsigned int nb_lcores = 0;
	unsigned int nb_mbufs;
	uint16_t nb_sess = 512;
	uint32_t sess_sz;
	char s[64];

	/* Init EAL. 8< */
	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid EAL arguments\n");
	argc -= ret;
	argv += ret;

	force_quit = false;
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	/* parse application arguments (after the EAL ones) */
	ret = l2fwd_parse_args(&options, argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid L2FWD arguments\n");
	/* >8 End of init EAL. */

	printf("MAC updating %s\n", mac_updating ? "enabled" : "disabled");

	/* convert to number of cycles */
	timer_period *= rte_get_timer_hz();

	nb_ports = rte_eth_dev_count_avail();
	if (nb_ports == 0)
		rte_exit(EXIT_FAILURE, "No Ethernet ports - bye\n");

	if (port_pair_params != NULL) {
		if (check_port_pair_config() < 0)
			rte_exit(EXIT_FAILURE, "Invalid port pair config\n");
	}

	/* check port mask to possible port mask */
	if (l2fwd_enabled_port_mask & ~((1 << nb_ports) - 1))
		rte_exit(EXIT_FAILURE, "Invalid portmask; possible (0x%x)\n",
			(1 << nb_ports) - 1);

	/* Initialization of the driver. 8< */

	/* reset l2fwd_dst_ports */
	for (portid = 0; portid < RTE_MAX_ETHPORTS; portid++)
		l2fwd_dst_ports[portid] = 0;
	last_port = 0;

	/* populate destination port details */
	if (port_pair_params != NULL) {
		uint16_t idx, p;

		for (idx = 0; idx < (nb_port_pair_params << 1); idx++) {
			p = idx & 1;
			portid = port_pair_params[idx >> 1].port[p];
			l2fwd_dst_ports[portid] =
				port_pair_params[idx >> 1].port[p ^ 1];
		}
	} else {
		RTE_ETH_FOREACH_DEV(portid) {
			/* skip ports that are not enabled */
			if ((l2fwd_enabled_port_mask & (1 << portid)) == 0)
				continue;

			if (nb_ports_in_mask % 2) {
				l2fwd_dst_ports[portid] = last_port;
				l2fwd_dst_ports[last_port] = portid;
			} else {
				last_port = portid;
			}

			nb_ports_in_mask++;
		}
		if (nb_ports_in_mask % 2) {
			printf("Notice: odd number of ports in portmask.\n");
			l2fwd_dst_ports[last_port] = last_port;
		}
	}
	/* >8 End of initialization of the driver. */

	rx_lcore_id = 0;
	qconf = NULL;

	/* Initialize the port/queue configuration of each logical core */
	RTE_ETH_FOREACH_DEV(portid) {
		/* skip ports that are not enabled */
		if ((l2fwd_enabled_port_mask & (1 << portid)) == 0)
			continue;

		/* get the lcore_id for this port */
		while (rte_lcore_is_enabled(rx_lcore_id) == 0 ||
		       lcore_queue_conf[rx_lcore_id].n_rx_port ==
		       l2fwd_rx_queue_per_lcore) {
			rx_lcore_id++;
			if (rx_lcore_id >= RTE_MAX_LCORE)
				rte_exit(EXIT_FAILURE, "Not enough cores\n");
		}

		if (qconf != &lcore_queue_conf[rx_lcore_id]) {
			/* Assigned a new logical core in the loop above. */
			qconf = &lcore_queue_conf[rx_lcore_id];
			nb_lcores++;
		}

		qconf->rx_port_list[qconf->n_rx_port] = portid;
		qconf->n_rx_port++;
		printf("Lcore %u: RX port %u TX port %u\n", rx_lcore_id,
		       portid, l2fwd_dst_ports[portid]);
	}

	nb_mbufs = RTE_MAX(nb_ports * (nb_rxd + nb_txd + MAX_PKT_BURST +
		nb_lcores * MEMPOOL_CACHE_SIZE), 8192U);

	/* Create the mbuf pool. 8< */
	l2fwd_pktmbuf_pool = rte_pktmbuf_pool_create("mbuf_pool", nb_mbufs,
		MEMPOOL_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE,
		rte_socket_id());
	if (l2fwd_pktmbuf_pool == NULL)
		rte_exit(EXIT_FAILURE, "Cannot init mbuf pool\n");
	/* >8 End of create the mbuf pool. */

	/* Initialise each port */
	RTE_ETH_FOREACH_DEV(portid) {
		struct rte_eth_rxconf rxq_conf;
		struct rte_eth_txconf txq_conf;
		struct rte_eth_conf local_port_conf = port_conf;
		struct rte_eth_dev_info dev_info;

		/* skip ports that are not enabled */
		if ((l2fwd_enabled_port_mask & (1 << portid)) == 0) {
			printf("Skipping disabled port %u\n", portid);
			continue;
		}
		nb_ports_available++;

		/* init port */
		printf("Initializing port %u... ", portid);
		fflush(stdout);

		ret = rte_eth_dev_info_get(portid, &dev_info);
		if (ret != 0)
			rte_exit(EXIT_FAILURE,
				"Error during getting device (port %u) info: %s\n",
				portid, strerror(-ret));

		if (dev_info.tx_offload_capa & RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE)
			local_port_conf.txmode.offloads |=
				RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE;
		/* Configure the number of queues for a port. */
		ret = rte_eth_dev_configure(portid, 1, 1, &local_port_conf);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "Cannot configure device: err=%d, port=%u\n",
				  ret, portid);
		/* >8 End of configuration of the number of queues for a port. */

		ret = rte_eth_dev_adjust_nb_rx_tx_desc(portid, &nb_rxd,
						       &nb_txd);
		if (ret < 0)
			rte_exit(EXIT_FAILURE,
				 "Cannot adjust number of descriptors: err=%d, port=%u\n",
				 ret, portid);

		ret = rte_eth_macaddr_get(portid,
					  &l2fwd_ports_eth_addr[portid]);
		if (ret < 0)
			rte_exit(EXIT_FAILURE,
				 "Cannot get MAC address: err=%d, port=%u\n",
				 ret, portid);

		/* init one RX queue */
		fflush(stdout);
		rxq_conf = dev_info.default_rxconf;
		rxq_conf.offloads = local_port_conf.rxmode.offloads;
		/* RX queue setup. 8< */
		ret = rte_eth_rx_queue_setup(portid, 0, nb_rxd,
					     rte_eth_dev_socket_id(portid),
					     &rxq_conf,
					     l2fwd_pktmbuf_pool);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "rte_eth_rx_queue_setup:err=%d, port=%u\n",
				  ret, portid);
		/* >8 End of RX queue setup. */

		/* Init one TX queue on each port. 8< */
		fflush(stdout);
		txq_conf = dev_info.default_txconf;
		txq_conf.offloads = local_port_conf.txmode.offloads;
		ret = rte_eth_tx_queue_setup(portid, 0, nb_txd,
				rte_eth_dev_socket_id(portid),
				&txq_conf);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "rte_eth_tx_queue_setup:err=%d, port=%u\n",
				ret, portid);
		/* >8 End of init one TX queue on each port. */

		/* Initialize TX buffers */
		tx_buffer[portid] = rte_zmalloc_socket("tx_buffer",
				RTE_ETH_TX_BUFFER_SIZE(MAX_PKT_BURST), 0,
				rte_eth_dev_socket_id(portid));
		if (tx_buffer[portid] == NULL)
			rte_exit(EXIT_FAILURE, "Cannot allocate buffer for tx on port %u\n",
					portid);

		rte_eth_tx_buffer_init(tx_buffer[portid], MAX_PKT_BURST);

		ret = rte_eth_tx_buffer_set_err_callback(tx_buffer[portid],
				rte_eth_tx_buffer_count_callback,
				&port_statistics[portid].dropped);
		if (ret < 0)
			rte_exit(EXIT_FAILURE,
			"Cannot set error callback for tx buffer on port %u\n",
				 portid);

		ret = rte_eth_dev_set_ptypes(portid, RTE_PTYPE_UNKNOWN, NULL,
					     0);
		if (ret < 0)
			printf("Port %u, Failed to disable Ptype parsing\n",
					portid);
		/* Start device */
		ret = rte_eth_dev_start(portid);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "rte_eth_dev_start:err=%d, port=%u\n",
				  ret, portid);

		printf("done:\n");
		if (promiscuous_on) {
			ret = rte_eth_promiscuous_enable(portid);
			if (ret != 0)
				rte_exit(EXIT_FAILURE,
					"rte_eth_promiscuous_enable:err=%s, port=%u\n",
					rte_strerror(-ret), portid);
		}

		printf("Port %u, MAC address: " RTE_ETHER_ADDR_PRT_FMT "\n\n",
			portid,
			RTE_ETHER_ADDR_BYTES(&l2fwd_ports_eth_addr[portid]));

		/* initialize port stats */
		memset(&port_statistics, 0, sizeof(port_statistics));

		mcs_port_params[portid].sec_ctx = rte_eth_dev_get_sec_ctx(portid);
		if (mcs_port_params[portid].sec_ctx == NULL)
			rte_exit(EXIT_FAILURE, "Device does not support Security ctx\n");

		sess_sz = rte_security_session_get_size(mcs_port_params[portid].sec_ctx);
		if (mcs_port_params[portid].sess_pool == NULL) {
			snprintf(s, sizeof(s), "sess_pool_p%d", portid);
			mcs_port_params[portid].sess_pool = rte_mempool_create(s,
							nb_sess, sess_sz,
							SESSION_POOL_CACHE_SIZE, 0,
							NULL, NULL, NULL, NULL,
							SOCKET_ID_ANY, 0);
			if (mcs_port_params[portid].sess_pool == NULL)
				rte_exit(EXIT_FAILURE, "Cannot init sess pool\n");

			printf("Allocated sess pool\n");
		}

		if (((options.tx_portmask & (1 << portid)) != 0) ||
				((options.rx_portmask & (1 << portid)) != 0)) {
			ret = initialize_macsec_session(portid);
			if (ret < 0)
				rte_exit(EXIT_FAILURE,
					"Failed to initialize MACsec session for port: %d\n",
					portid);
		}
	}

	if (!nb_ports_available) {
		rte_exit(EXIT_FAILURE,
			"All available ports are disabled. Please set portmask.\n");
	}

	check_all_ports_link_status(l2fwd_enabled_port_mask);

	ret = 0;
	/* launch per-lcore init on every lcore */
	rte_eal_mp_remote_launch(l2fwd_launch_one_lcore, NULL, CALL_MAIN);
	RTE_LCORE_FOREACH_WORKER(lcore_id) {
		if (rte_eal_wait_lcore(lcore_id) < 0) {
			ret = -1;
			break;
		}
	}

	RTE_ETH_FOREACH_DEV(portid) {
		if ((l2fwd_enabled_port_mask & (1 << portid)) == 0)
			continue;
		printf("Closing port %d...", portid);
		ret = rte_eth_dev_stop(portid);
		if (ret != 0)
			printf("rte_eth_dev_stop: err=%d, port=%d\n",
			       ret, portid);
		rte_eth_dev_close(portid);
		printf(" Done\n");
	}

	/* clean up the EAL */
	rte_eal_cleanup();
	printf("Bye...\n");

	return ret;
}
