/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Marvell.
 */

#include <stdio.h>
#include <inttypes.h>

#include <rte_ethdev.h>
#include <rte_malloc.h>
#include <rte_security.h>

#include "test.h"
#include "test_security_inline_macsec_vectors.h"

#ifdef RTE_EXEC_ENV_WINDOWS
static int
test_inline_macsec(void)
{
	printf("Inline MACsec not supported on Windows, skipping test\n");
	return TEST_SKIPPED;
}

#else

#define NB_ETHPORTS_USED		1
#define MEMPOOL_CACHE_SIZE		32
#define RTE_TEST_RX_DESC_DEFAULT	1024
#define RTE_TEST_TX_DESC_DEFAULT	1024
#define RTE_PORT_ALL		(~(uint16_t)0x0)

#define RX_PTHRESH 8 /**< Default values of RX prefetch threshold reg. */
#define RX_HTHRESH 8 /**< Default values of RX host threshold reg. */
#define RX_WTHRESH 0 /**< Default values of RX write-back threshold reg. */

#define TX_PTHRESH 32 /**< Default values of TX prefetch threshold reg. */
#define TX_HTHRESH 0  /**< Default values of TX host threshold reg. */
#define TX_WTHRESH 0  /**< Default values of TX write-back threshold reg. */

#define MAX_TRAFFIC_BURST		2048
#define NB_MBUF				10240

#define MCS_INVALID_SA			0xFFFF
#define MCS_DEFAULT_PN_THRESHOLD	0xFFFFF

static struct rte_mempool *mbufpool;
static struct rte_mempool *sess_pool;
/* ethernet addresses of ports */
static struct rte_ether_addr ports_eth_addr[RTE_MAX_ETHPORTS];

struct mcs_test_opts {
	int val_frames;
	int nb_td;
	uint16_t mtu;
	uint8_t sa_in_use;
	bool encrypt;
	bool protect_frames;
	uint8_t sectag_insert_mode;
	uint8_t nb_vlan;
	uint32_t replay_win_sz;
	uint8_t replay_protect;
	uint8_t rekey_en;
	const struct mcs_test_vector *rekey_td;
	const struct mcs_test_vector *ar_td[3];
	bool dump_all_stats;
	uint8_t check_untagged_rx;
	uint8_t check_bad_tag_cnt;
	uint8_t check_sa_not_in_use;
	uint8_t check_decap_stats;
	uint8_t check_verify_only_stats;
	uint8_t check_pkts_invalid_stats;
	uint8_t check_pkts_unchecked_stats;
	uint8_t check_out_pkts_untagged;
	uint8_t check_out_pkts_toolong;
	uint8_t check_encap_stats;
	uint8_t check_auth_only_stats;
	uint8_t check_sectag_interrupts;
};

static struct rte_eth_conf port_conf = {
	.rxmode = {
		.mq_mode = RTE_ETH_MQ_RX_NONE,
		.offloads = RTE_ETH_RX_OFFLOAD_CHECKSUM |
			    RTE_ETH_RX_OFFLOAD_MACSEC_STRIP,
	},
	.txmode = {
		.mq_mode = RTE_ETH_MQ_TX_NONE,
		.offloads = RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE |
			    RTE_ETH_TX_OFFLOAD_MACSEC_INSERT,
	},
	.lpbk_mode = 1,  /* enable loopback */
};

static struct rte_eth_rxconf rx_conf = {
	.rx_thresh = {
		.pthresh = RX_PTHRESH,
		.hthresh = RX_HTHRESH,
		.wthresh = RX_WTHRESH,
	},
	.rx_free_thresh = 32,
};

static struct rte_eth_txconf tx_conf = {
	.tx_thresh = {
		.pthresh = TX_PTHRESH,
		.hthresh = TX_HTHRESH,
		.wthresh = TX_WTHRESH,
	},
	.tx_free_thresh = 32, /* Use PMD default values */
	.tx_rs_thresh = 32, /* Use PMD default values */
};

static uint16_t port_id;

static uint64_t link_mbps;

static struct rte_flow *default_tx_flow[RTE_MAX_ETHPORTS];
static struct rte_flow *default_rx_flow[RTE_MAX_ETHPORTS];

static struct rte_mbuf **tx_pkts_burst;
static struct rte_mbuf **rx_pkts_burst;

static inline struct rte_mbuf *
init_packet(struct rte_mempool *mp, const uint8_t *data, unsigned int len)
{
	struct rte_mbuf *pkt;

	pkt = rte_pktmbuf_alloc(mp);
	if (pkt == NULL)
		return NULL;

	rte_memcpy(rte_pktmbuf_append(pkt, len), data, len);

	return pkt;
}

static int
init_mempools(unsigned int nb_mbuf)
{
	void *sec_ctx;
	uint16_t nb_sess = 512;
	uint32_t sess_sz;
	char s[64];

	if (mbufpool == NULL) {
		snprintf(s, sizeof(s), "mbuf_pool");
		mbufpool = rte_pktmbuf_pool_create(s, nb_mbuf,
				MEMPOOL_CACHE_SIZE, 0,
				RTE_MBUF_DEFAULT_BUF_SIZE, SOCKET_ID_ANY);
		if (mbufpool == NULL) {
			printf("Cannot init mbuf pool\n");
			return TEST_FAILED;
		}
		printf("Allocated mbuf pool\n");
	}

	sec_ctx = rte_eth_dev_get_sec_ctx(port_id);
	if (sec_ctx == NULL) {
		printf("Device does not support Security ctx\n");
		return TEST_SKIPPED;
	}
	sess_sz = rte_security_session_get_size(sec_ctx);
	if (sess_pool == NULL) {
		snprintf(s, sizeof(s), "sess_pool");
		sess_pool = rte_mempool_create(s, nb_sess, sess_sz,
				MEMPOOL_CACHE_SIZE, 0,
				NULL, NULL, NULL, NULL,
				SOCKET_ID_ANY, 0);
		if (sess_pool == NULL) {
			printf("Cannot init sess pool\n");
			return TEST_FAILED;
		}
		printf("Allocated sess pool\n");
	}

	return 0;
}

static void
fill_macsec_sa_conf(const struct mcs_test_vector *td, struct rte_security_macsec_sa *sa,
			enum rte_security_macsec_direction dir, uint8_t an, uint8_t tci_off)
{
	sa->dir = dir;

	sa->key.data = td->sa_key.data;
	sa->key.length = td->sa_key.len;

	memcpy((uint8_t *)sa->salt, (const uint8_t *)td->salt, RTE_SECURITY_MACSEC_SALT_LEN);

	/* AN is set as per the value in secure packet in test vector */
	sa->an = an & RTE_MACSEC_AN_MASK;

	sa->ssci = td->ssci;
	sa->xpn = td->xpn;
	/* Starting packet number which is expected to come next.
	 * It is take from the test vector so that we can match the out packet.
	 */
	sa->next_pn = *(const uint32_t *)(&td->secure_pkt.data[tci_off + 2]);
}

static void
fill_macsec_sc_conf(const struct mcs_test_vector *td,
		    struct rte_security_macsec_sc *sc_conf,
		    const struct mcs_test_opts *opts,
		    enum rte_security_macsec_direction dir,
		    uint16_t sa_id[], uint8_t tci_off)
{
	uint8_t i;

	sc_conf->dir = dir;
	sc_conf->pn_threshold = ((uint64_t)td->xpn << 32) |
		rte_be_to_cpu_32(*(const uint32_t *)(&td->secure_pkt.data[tci_off + 2]));
	if (dir == RTE_SECURITY_MACSEC_DIR_TX) {
		sc_conf->sc_tx.sa_id = sa_id[0];
		if (sa_id[1] != MCS_INVALID_SA) {
			sc_conf->sc_tx.sa_id_rekey = sa_id[1];
			sc_conf->sc_tx.re_key_en = 1;
		}
		sc_conf->sc_tx.active = 1;
		/* is SCI valid */
		if (td->secure_pkt.data[tci_off] & RTE_MACSEC_TCI_SC) {
			memcpy(&sc_conf->sc_tx.sci, &td->secure_pkt.data[tci_off + 6],
					sizeof(sc_conf->sc_tx.sci));
			sc_conf->sc_tx.sci = rte_be_to_cpu_64(sc_conf->sc_tx.sci);
		} else if (td->secure_pkt.data[tci_off] & RTE_MACSEC_TCI_ES) {
			/* sci = source_mac + port_id when ES.bit = 1 & SC.bit = 0 */
			const uint8_t *smac = td->plain_pkt.data + RTE_ETHER_ADDR_LEN;
			uint8_t *ptr = (uint8_t *)&sc_conf->sc_tx.sci;

			ptr[0] = 0x01;
			ptr[1] = 0;
			for (i = 0; i < RTE_ETHER_ADDR_LEN; i++)
				ptr[2 + i] = smac[RTE_ETHER_ADDR_LEN - 1 - i];
		} else {
			/* use some default SCI */
			sc_conf->sc_tx.sci = 0xf1341e023a2b1c5d;
		}
		if (td->xpn > 0)
			sc_conf->sc_tx.is_xpn = 1;
	} else {
		for (i = 0; i < RTE_SECURITY_MACSEC_NUM_AN; i++) {
			sc_conf->sc_rx.sa_id[i] = sa_id[i];
			sc_conf->sc_rx.sa_in_use[i] = opts->sa_in_use;
		}
		sc_conf->sc_rx.active = 1;
		if (td->xpn > 0)
			sc_conf->sc_rx.is_xpn = 1;
	}
}


/* Create Inline MACsec session */
static int
fill_session_conf(const struct mcs_test_vector *td, uint16_t portid __rte_unused,
		const struct mcs_test_opts *opts,
		struct rte_security_session_conf *sess_conf,
		enum rte_security_macsec_direction dir,
		uint16_t sc_id,
		uint8_t tci_off)
{
	sess_conf->action_type = RTE_SECURITY_ACTION_TYPE_INLINE_PROTOCOL;
	sess_conf->protocol = RTE_SECURITY_PROTOCOL_MACSEC;
	sess_conf->macsec.dir = dir;
	sess_conf->macsec.alg = td->alg;
	sess_conf->macsec.cipher_off = 0;
	if (td->secure_pkt.data[tci_off] & RTE_MACSEC_TCI_SC) {
		sess_conf->macsec.sci = rte_be_to_cpu_64(*(const uint64_t *)
					(&td->secure_pkt.data[tci_off + 6]));
	} else if (td->secure_pkt.data[tci_off] & RTE_MACSEC_TCI_ES) {
		/* sci = source_mac + port_id when ES.bit = 1 & SC.bit = 0 */
		const uint8_t *smac = td->plain_pkt.data + RTE_ETHER_ADDR_LEN;
		uint8_t *ptr = (uint8_t *)&sess_conf->macsec.sci;
		uint8_t j;

		ptr[0] = 0x01;
		ptr[1] = 0;
		for (j = 0; j < RTE_ETHER_ADDR_LEN; j++)
			ptr[2 + j] = smac[RTE_ETHER_ADDR_LEN - 1 - j];
	}
	sess_conf->macsec.sc_id = sc_id;
	if (dir == RTE_SECURITY_MACSEC_DIR_TX) {
		sess_conf->macsec.tx_secy.mtu = opts->mtu;
		sess_conf->macsec.tx_secy.sectag_off = (opts->sectag_insert_mode == 1) ?
							2 * RTE_ETHER_ADDR_LEN :
							RTE_VLAN_HLEN;
		sess_conf->macsec.tx_secy.sectag_insert_mode = opts->sectag_insert_mode;
		sess_conf->macsec.tx_secy.ctrl_port_enable = 1;
		sess_conf->macsec.tx_secy.sectag_version = 0;
		sess_conf->macsec.tx_secy.end_station =
					(td->secure_pkt.data[tci_off] & RTE_MACSEC_TCI_ES) >> 6;
		sess_conf->macsec.tx_secy.send_sci =
					(td->secure_pkt.data[tci_off] & RTE_MACSEC_TCI_SC) >> 5;
		sess_conf->macsec.tx_secy.scb =
					(td->secure_pkt.data[tci_off] & RTE_MACSEC_TCI_SCB) >> 4;
		sess_conf->macsec.tx_secy.encrypt = opts->encrypt;
		sess_conf->macsec.tx_secy.protect_frames = opts->protect_frames;
		sess_conf->macsec.tx_secy.icv_include_da_sa = 1;
	} else {
		sess_conf->macsec.rx_secy.replay_win_sz = opts->replay_win_sz;
		sess_conf->macsec.rx_secy.replay_protect = opts->replay_protect;
		sess_conf->macsec.rx_secy.icv_include_da_sa = 1;
		sess_conf->macsec.rx_secy.ctrl_port_enable = 1;
		sess_conf->macsec.rx_secy.preserve_sectag = 0;
		sess_conf->macsec.rx_secy.preserve_icv = 0;
		sess_conf->macsec.rx_secy.validate_frames = opts->val_frames;
	}

	return 0;
}

static int
create_default_flow(const struct mcs_test_vector *td, uint16_t portid,
		    enum rte_security_macsec_direction dir, void *sess)
{
	struct rte_flow_action action[2];
	struct rte_flow_item pattern[2];
	struct rte_flow_attr attr = {0};
	struct rte_flow_error err;
	struct rte_flow *flow;
	struct rte_flow_item_eth eth = { .hdr.ether_type = 0, };
	static const struct rte_flow_item_eth eth_mask = {
		.hdr.dst_addr.addr_bytes = "\x00\x00\x00\x00\x00\x00",
		.hdr.src_addr.addr_bytes = "\x00\x00\x00\x00\x00\x00",
		.hdr.ether_type = RTE_BE16(0x0000),
	};

	int ret;

	eth.has_vlan = 0;
	if (dir == RTE_SECURITY_MACSEC_DIR_TX)
		memcpy(&eth.hdr, td->plain_pkt.data, RTE_ETHER_HDR_LEN);
	else
		memcpy(&eth.hdr, td->secure_pkt.data, RTE_ETHER_HDR_LEN);

	pattern[0].type = RTE_FLOW_ITEM_TYPE_ETH;
	pattern[0].spec = &eth;
	pattern[0].mask = &eth_mask;
	pattern[0].last = NULL;
	pattern[1].type = RTE_FLOW_ITEM_TYPE_END;

	action[0].type = RTE_FLOW_ACTION_TYPE_SECURITY;
	action[0].conf = sess;
	action[1].type = RTE_FLOW_ACTION_TYPE_END;
	action[1].conf = NULL;

	attr.ingress = (dir == RTE_SECURITY_MACSEC_DIR_RX) ? 1 : 0;
	attr.egress = (dir == RTE_SECURITY_MACSEC_DIR_TX) ? 1 : 0;

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

	if (dir == RTE_SECURITY_MACSEC_DIR_TX)
		default_tx_flow[portid] = flow;
	else
		default_rx_flow[portid] = flow;

	return 0;
}

static void
destroy_default_flow(uint16_t portid)
{
	struct rte_flow_error err;
	int ret;

	if (default_tx_flow[portid]) {
		ret = rte_flow_destroy(portid, default_tx_flow[portid], &err);
		if (ret) {
			printf("\nDefault Tx flow rule destroy failed\n");
			return;
		}
		default_tx_flow[portid] = NULL;
	}
	if (default_rx_flow[portid]) {
		ret = rte_flow_destroy(portid, default_rx_flow[portid], &err);
		if (ret) {
			printf("\nDefault Rx flow rule destroy failed\n");
			return;
		}
		default_rx_flow[portid] = NULL;
	}
}

static void
print_ethaddr(const char *name, const struct rte_ether_addr *eth_addr)
{
	char buf[RTE_ETHER_ADDR_FMT_SIZE];
	rte_ether_format_addr(buf, RTE_ETHER_ADDR_FMT_SIZE, eth_addr);
	printf("%s%s", name, buf);
}

/* Check the link status of all ports in up to 3s, and print them finally */
static void
check_all_ports_link_status(uint16_t port_num, uint32_t port_mask)
{
#define CHECK_INTERVAL 100 /* 100ms */
#define MAX_CHECK_TIME 30 /* 3s (30 * 100ms) in total */
	uint16_t portid;
	uint8_t count, all_ports_up, print_flag = 0;
	struct rte_eth_link link;
	int ret;
	char link_status[RTE_ETH_LINK_MAX_STR_LEN];

	printf("Checking link statuses...\n");
	fflush(stdout);
	for (count = 0; count <= MAX_CHECK_TIME; count++) {
		all_ports_up = 1;
		for (portid = 0; portid < port_num; portid++) {
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
				if (link.link_status && link_mbps == 0)
					link_mbps = link.link_speed;

				rte_eth_link_to_str(link_status,
					sizeof(link_status), &link);
				printf("Port %d %s\n", portid, link_status);
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

		if (all_ports_up == 0)
			fflush(stdout);

		/* set the print_flag if all ports up or timeout */
		if (all_ports_up == 1 || count == (MAX_CHECK_TIME - 1))
			print_flag = 1;
	}
}

static int
test_macsec_post_process(struct rte_mbuf *m, const struct mcs_test_vector *td,
			enum mcs_op op, uint8_t check_out_pkts_untagged)
{
	const uint8_t *dptr;
	uint16_t pkt_len;

	if (op == MCS_DECAP || op == MCS_ENCAP_DECAP ||
			op == MCS_VERIFY_ONLY || op == MCS_AUTH_VERIFY ||
			check_out_pkts_untagged == 1) {
		dptr = td->plain_pkt.data;
		pkt_len = td->plain_pkt.len;
	} else {
		dptr = td->secure_pkt.data;
		pkt_len = td->secure_pkt.len;
	}

	if (memcmp(rte_pktmbuf_mtod(m, uint8_t *), dptr, pkt_len)) {
		printf("\nData comparison failed for td.");
		rte_pktmbuf_dump(stdout, m, m->pkt_len);
		rte_hexdump(stdout, "expected_data", dptr, pkt_len);
		return TEST_FAILED;
	}

	return TEST_SUCCESS;
}

static void
mcs_stats_dump(void *ctx, enum mcs_op op,
	       void *rx_sess, void *tx_sess,
	       uint8_t rx_sc_id, uint8_t tx_sc_id,
	       uint16_t rx_sa_id[], uint16_t tx_sa_id[])
{
	struct rte_security_stats sess_stats = {0};
	struct rte_security_macsec_secy_stats *secy_stat;
	struct rte_security_macsec_sc_stats sc_stat = {0};
	struct rte_security_macsec_sa_stats sa_stat = {0};
	int i;

	if (op == MCS_DECAP || op == MCS_ENCAP_DECAP ||
			op == MCS_VERIFY_ONLY || op == MCS_AUTH_VERIFY) {
		printf("\n********* RX SECY STATS ************\n");
		rte_security_session_stats_get(ctx, rx_sess, &sess_stats);
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
		printf("\n********** RX SC[%u] STATS **************\n", rx_sc_id);

		rte_security_macsec_sc_stats_get(ctx, rx_sc_id, RTE_SECURITY_MACSEC_DIR_RX,
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
		printf("\n");
		for (i = 0; i < RTE_SECURITY_MACSEC_NUM_AN; i++) {
			printf("\n********** RX SA[%u] STATS ****************\n", rx_sa_id[i]);
			memset(&sa_stat, 0, sizeof(struct rte_security_macsec_sa_stats));
			rte_security_macsec_sa_stats_get(ctx, rx_sa_id[i],
					RTE_SECURITY_MACSEC_DIR_RX, &sa_stat);

			/* RX */
			if (sa_stat.pkt_invalid_cnt)
				printf("RX pkt_invalid_cnt: 0x%" PRIx64 "\n",
						sa_stat.pkt_invalid_cnt);
			if (sa_stat.pkt_nosaerror_cnt)
				printf("RX pkt_nosaerror_cnt: 0x%" PRIx64 "\n",
						sa_stat.pkt_nosaerror_cnt);
			if (sa_stat.pkt_notvalid_cnt)
				printf("RX pkt_notvalid_cnt: 0x%" PRIx64 "\n",
						sa_stat.pkt_notvalid_cnt);
			if (sa_stat.pkt_ok_cnt)
				printf("RX pkt_ok_cnt: 0x%" PRIx64 "\n", sa_stat.pkt_ok_cnt);
			if (sa_stat.pkt_nosa_cnt)
				printf("RX pkt_nosa_cnt: 0x%" PRIx64 "\n", sa_stat.pkt_nosa_cnt);
			printf("\n");
		}
	}

	if (op == MCS_ENCAP || op == MCS_ENCAP_DECAP ||
			op == MCS_AUTH_ONLY || op == MCS_AUTH_VERIFY) {
		memset(&sess_stats, 0, sizeof(struct rte_security_stats));
		rte_security_session_stats_get(ctx, tx_sess, &sess_stats);
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
		rte_security_macsec_sc_stats_get(ctx, tx_sc_id, RTE_SECURITY_MACSEC_DIR_TX,
						 &sc_stat);
		printf("\n********** TX SC[%u] STATS **************\n", tx_sc_id);
		if (sc_stat.pkt_encrypt_cnt)
			printf("TX pkt_encrypt_cnt: 0x%" PRIx64 "\n", sc_stat.pkt_encrypt_cnt);
		if (sc_stat.pkt_protected_cnt)
			printf("TX pkt_protected_cnt: 0x%" PRIx64 "\n", sc_stat.pkt_protected_cnt);
		if (sc_stat.octet_encrypt_cnt)
			printf("TX octet_encrypt_cnt: 0x%" PRIx64 "\n", sc_stat.octet_encrypt_cnt);

		memset(&sa_stat, 0, sizeof(struct rte_security_macsec_sa_stats));
		rte_security_macsec_sa_stats_get(ctx, tx_sa_id[0],
				RTE_SECURITY_MACSEC_DIR_TX, &sa_stat);
		printf("\n********** TX SA[%u] STATS ****************\n", tx_sa_id[0]);
		if (sa_stat.pkt_encrypt_cnt)
			printf("TX pkt_encrypt_cnt: 0x%" PRIx64 "\n", sa_stat.pkt_encrypt_cnt);
		if (sa_stat.pkt_protected_cnt)
			printf("TX pkt_protected_cnt: 0x%" PRIx64 "\n", sa_stat.pkt_protected_cnt);
	}
}

static int
mcs_stats_check(void *ctx, enum mcs_op op,
		const struct mcs_test_opts *opts,
		const struct mcs_test_vector *td,
		void *rx_sess, void *tx_sess,
		uint8_t rx_sc_id, uint8_t tx_sc_id,
		uint16_t rx_sa_id[], uint16_t tx_sa_id[])
{
	struct rte_security_stats sess_stats = {0};
	struct rte_security_macsec_secy_stats *secy_stat;
	struct rte_security_macsec_sc_stats sc_stat = {0};
	struct rte_security_macsec_sa_stats sa_stat = {0};
	int i;

	if (op == MCS_DECAP || op == MCS_ENCAP_DECAP ||
			op == MCS_VERIFY_ONLY || op == MCS_AUTH_VERIFY) {
		rte_security_session_stats_get(ctx, rx_sess, &sess_stats);
		secy_stat = &sess_stats.macsec;

		if ((opts->check_untagged_rx && secy_stat->pkt_notag_cnt != 1) ||
				(opts->check_untagged_rx && secy_stat->pkt_untaged_cnt != 1))
			return TEST_FAILED;

		if (opts->check_bad_tag_cnt && secy_stat->pkt_badtag_cnt != 1)
			return TEST_FAILED;

		if (opts->check_sa_not_in_use && secy_stat->pkt_nosaerror_cnt != 1)
			return TEST_FAILED;

		if (opts->check_decap_stats && secy_stat->octet_decrypted_cnt !=
				(uint16_t)(td->plain_pkt.len - 2 * RTE_ETHER_ADDR_LEN))
			return TEST_FAILED;

		if (opts->check_verify_only_stats && secy_stat->octet_validated_cnt !=
				(uint16_t)(td->plain_pkt.len - 2 * RTE_ETHER_ADDR_LEN))
			return TEST_FAILED;

		rte_security_macsec_sc_stats_get(ctx, rx_sc_id,
				RTE_SECURITY_MACSEC_DIR_RX, &sc_stat);

		if ((opts->check_decap_stats || opts->check_verify_only_stats) &&
				sc_stat.pkt_ok_cnt != 1)
			return TEST_FAILED;

		if (opts->check_pkts_invalid_stats && sc_stat.pkt_notvalid_cnt != 1)
			return TEST_FAILED;

		if (opts->check_pkts_unchecked_stats && sc_stat.pkt_unchecked_cnt != 1)
			return TEST_FAILED;

		if (opts->replay_protect) {
			if (opts->replay_win_sz == 0 &&
					sc_stat.pkt_late_cnt != 2)
				return TEST_FAILED;
			else if (opts->replay_win_sz == 32 &&
					sc_stat.pkt_late_cnt != 1)
				return TEST_FAILED;
		}

		for (i = 0; i < RTE_SECURITY_MACSEC_NUM_AN; i++) {
			memset(&sa_stat, 0, sizeof(struct rte_security_macsec_sa_stats));
			rte_security_macsec_sa_stats_get(ctx, rx_sa_id[i],
					RTE_SECURITY_MACSEC_DIR_RX, &sa_stat);

		}
	}

	if (op == MCS_ENCAP || op == MCS_ENCAP_DECAP ||
			op == MCS_AUTH_ONLY || op == MCS_AUTH_VERIFY) {
		memset(&sess_stats, 0, sizeof(struct rte_security_stats));
		rte_security_session_stats_get(ctx, tx_sess, &sess_stats);
		secy_stat = &sess_stats.macsec;

		if (opts->check_out_pkts_untagged && secy_stat->pkt_untagged_cnt != 1)
			return TEST_FAILED;

		if (opts->check_out_pkts_toolong && secy_stat->pkt_toolong_cnt != 1)
			return TEST_FAILED;

		if (opts->check_encap_stats && secy_stat->octet_encrypted_cnt !=
				(uint16_t)(td->plain_pkt.len - 2 * RTE_ETHER_ADDR_LEN))
			return TEST_FAILED;

		if (opts->check_auth_only_stats && secy_stat->octet_protected_cnt !=
				(uint16_t)(td->plain_pkt.len - 2 * RTE_ETHER_ADDR_LEN))
			return TEST_FAILED;


		memset(&sc_stat, 0, sizeof(struct rte_security_macsec_sc_stats));
		rte_security_macsec_sc_stats_get(ctx, tx_sc_id, RTE_SECURITY_MACSEC_DIR_TX,
						 &sc_stat);

		if (opts->check_encap_stats && sc_stat.pkt_encrypt_cnt != 1)
			return TEST_FAILED;

		if (opts->check_auth_only_stats && sc_stat.pkt_protected_cnt != 1)
			return TEST_FAILED;

		memset(&sa_stat, 0, sizeof(struct rte_security_macsec_sa_stats));
		rte_security_macsec_sa_stats_get(ctx, tx_sa_id[0],
				RTE_SECURITY_MACSEC_DIR_TX, &sa_stat);
	}

	return 0;
}

static int
test_macsec_event_callback(uint16_t port_id, enum rte_eth_event_type type,
			   void *param, void *ret_param)
{
	struct mcs_err_vector *vector = (struct mcs_err_vector *)param;
	struct rte_eth_event_macsec_desc *event_desc = NULL;

	RTE_SET_USED(port_id);

	if (type != RTE_ETH_EVENT_MACSEC)
		return -1;

	event_desc = ret_param;
	if (event_desc == NULL) {
		printf("Event descriptor not set\n");
		return -1;
	}
	vector->notify_event = true;

	switch (event_desc->type) {
	case RTE_ETH_EVENT_MACSEC_SECTAG_VAL_ERR:
		vector->event = RTE_ETH_EVENT_MACSEC_SECTAG_VAL_ERR;
		switch (event_desc->subtype) {
		case RTE_ETH_SUBEVENT_MACSEC_RX_SECTAG_V_EQ1:
			vector->event_subtype = RTE_ETH_SUBEVENT_MACSEC_RX_SECTAG_V_EQ1;
			break;
		case RTE_ETH_SUBEVENT_MACSEC_RX_SECTAG_E_EQ0_C_EQ1:
			vector->event_subtype = RTE_ETH_SUBEVENT_MACSEC_RX_SECTAG_E_EQ0_C_EQ1;
			break;
		case RTE_ETH_SUBEVENT_MACSEC_RX_SECTAG_SL_GTE48:
			vector->event_subtype = RTE_ETH_SUBEVENT_MACSEC_RX_SECTAG_SL_GTE48;
			break;
		case RTE_ETH_SUBEVENT_MACSEC_RX_SECTAG_ES_EQ1_SC_EQ1:
			vector->event_subtype = RTE_ETH_SUBEVENT_MACSEC_RX_SECTAG_ES_EQ1_SC_EQ1;
			break;
		case RTE_ETH_SUBEVENT_MACSEC_RX_SECTAG_SC_EQ1_SCB_EQ1:
			vector->event_subtype = RTE_ETH_SUBEVENT_MACSEC_RX_SECTAG_SC_EQ1_SCB_EQ1;
			break;
		default:
			printf("\nUnknown Macsec event subtype: %d", event_desc->subtype);
		}
		break;
	case RTE_ETH_EVENT_MACSEC_RX_SA_PN_HARD_EXP:
		vector->event = RTE_ETH_EVENT_MACSEC_RX_SA_PN_HARD_EXP;
		break;
	case RTE_ETH_EVENT_MACSEC_RX_SA_PN_SOFT_EXP:
		vector->event = RTE_ETH_EVENT_MACSEC_RX_SA_PN_SOFT_EXP;
		break;
	case RTE_ETH_EVENT_MACSEC_TX_SA_PN_HARD_EXP:
		vector->event = RTE_ETH_EVENT_MACSEC_TX_SA_PN_HARD_EXP;
		break;
	case RTE_ETH_EVENT_MACSEC_TX_SA_PN_SOFT_EXP:
		vector->event = RTE_ETH_EVENT_MACSEC_TX_SA_PN_SOFT_EXP;
		break;
	case RTE_ETH_EVENT_MACSEC_SA_NOT_VALID:
		vector->event = RTE_ETH_EVENT_MACSEC_SA_NOT_VALID;
		break;
	default:
		printf("Invalid MACsec event reported\n");
		return -1;
	}

	return 0;
}

static int
test_macsec_sec_caps_verify(const struct mcs_test_opts *opts,
		const struct rte_security_capability *sec_cap, bool silent)
{
	if (opts->mtu > sec_cap->macsec.mtu) {
		if (!silent)
			RTE_LOG(INFO, USER1, "MTU size is not supported\n");
		return -ENOTSUP;
	}

	if (opts->replay_protect == 1 && sec_cap->macsec.anti_replay == 0) {
		if (!silent)
			RTE_LOG(INFO, USER1, "Anti replay is not supported\n");
		return -ENOTSUP;
	}

	if (opts->replay_win_sz > sec_cap->macsec.replay_win_sz) {
		if (!silent)
			RTE_LOG(INFO, USER1, "Replay window size is not "
					"supported\n");
		return -ENOTSUP;
	}

	if (opts->rekey_en == 1 && sec_cap->macsec.re_key == 0) {
		if (!silent)
			RTE_LOG(INFO, USER1, "Rekey is not supported\n");
		return -ENOTSUP;
	}

	if (opts->sectag_insert_mode == 0 &&
			sec_cap->macsec.relative_sectag_insert == 0) {
		if (!silent)
			RTE_LOG(INFO, USER1, "Relative offset sectag insert "
					"not supported\n");
		return -ENOTSUP;
	}

	if (opts->sectag_insert_mode == 1 &&
			sec_cap->macsec.fixed_sectag_insert == 0) {
		if (!silent)
			RTE_LOG(INFO, USER1, "Fixed offset sectag insert "
					"not supported\n");
		return -ENOTSUP;
	}

	return 0;
}

static int
test_macsec(const struct mcs_test_vector *td[], enum mcs_op op, const struct mcs_test_opts *opts)
{
	uint16_t rx_sa_id[MCS_MAX_FLOWS][RTE_SECURITY_MACSEC_NUM_AN] = {{0}};
	struct rte_security_capability_idx sec_cap_idx;
	const struct rte_security_capability *sec_cap;
	uint16_t tx_sa_id[MCS_MAX_FLOWS][2] = {{0}};
	uint16_t rx_sc_id[MCS_MAX_FLOWS] = {0};
	uint16_t tx_sc_id[MCS_MAX_FLOWS] = {0};
	void *rx_sess[MCS_MAX_FLOWS] = {0};
	void *tx_sess[MCS_MAX_FLOWS] = {0};
	struct rte_security_session_conf sess_conf = {0};
	struct rte_security_macsec_sa sa_conf = {0};
	struct rte_security_macsec_sc sc_conf = {0};
	struct mcs_err_vector err_vector = {0};
	void *ctx;
	int nb_rx = 0, nb_sent;
	int i, j = 0, ret, id, an = 0;
	uint8_t tci_off;
	int k;

	memset(rx_pkts_burst, 0, sizeof(rx_pkts_burst[0]) * opts->nb_td);

	ctx = rte_eth_dev_get_sec_ctx(port_id);
	if (ctx == NULL) {
		printf("Ethernet device doesn't support security features.\n");
		return TEST_SKIPPED;
	}

	sec_cap_idx.action = RTE_SECURITY_ACTION_TYPE_INLINE_PROTOCOL;
	sec_cap_idx.protocol = RTE_SECURITY_PROTOCOL_MACSEC;
	sec_cap_idx.macsec.alg = td[0]->alg;
	sec_cap = rte_security_capability_get(ctx, &sec_cap_idx);
	if (sec_cap == NULL) {
		printf("No capabilities registered\n");
		return TEST_SKIPPED;
	}

	if (test_macsec_sec_caps_verify(opts, sec_cap, false) != 0)
		return TEST_SKIPPED;

	if (opts->rekey_en) {
		/* Verify the rekey td */
		sec_cap_idx.macsec.alg = opts->rekey_td->alg;
		sec_cap = rte_security_capability_get(ctx, &sec_cap_idx);
		if (sec_cap == NULL) {
			printf("No capabilities registered\n");
			return TEST_SKIPPED;
		}
		if (test_macsec_sec_caps_verify(opts, sec_cap, false) != 0)
			return TEST_SKIPPED;
	}

	tci_off = (opts->sectag_insert_mode == 1) ? RTE_ETHER_HDR_LEN :
			RTE_ETHER_HDR_LEN + (opts->nb_vlan * RTE_VLAN_HLEN);

	for (i = 0, j = 0; i < opts->nb_td; i++) {
		if (op == MCS_DECAP || op == MCS_VERIFY_ONLY)
			tx_pkts_burst[j] = init_packet(mbufpool, td[i]->secure_pkt.data,
							td[i]->secure_pkt.len);
		else {
			tx_pkts_burst[j] = init_packet(mbufpool, td[i]->plain_pkt.data,
							td[i]->plain_pkt.len);

			tx_pkts_burst[j]->ol_flags |= RTE_MBUF_F_TX_MACSEC;
		}
		if (tx_pkts_burst[j] == NULL) {
			while (j--)
				rte_pktmbuf_free(tx_pkts_burst[j]);
			ret = TEST_FAILED;
			goto out;
		}
		j++;

		if (opts->replay_protect) {
			for (k = 0; k < 3; k++, j++) {
				tx_pkts_burst[j] = init_packet(mbufpool,
					opts->ar_td[k]->secure_pkt.data,
					opts->ar_td[k]->secure_pkt.len);
				if (tx_pkts_burst[j] == NULL) {
					while (j--)
						rte_pktmbuf_free(tx_pkts_burst[j]);
					ret = TEST_FAILED;
					goto out;
				}
			}
		}

		if (opts->rekey_en) {

			err_vector.td = td[i];
			err_vector.rekey_td = opts->rekey_td;
			err_vector.event = RTE_ETH_EVENT_MACSEC_UNKNOWN;
			err_vector.event_subtype = RTE_ETH_SUBEVENT_MACSEC_UNKNOWN;
			rte_eth_dev_callback_register(port_id, RTE_ETH_EVENT_MACSEC,
					test_macsec_event_callback, &err_vector);
			if (op == MCS_DECAP || op == MCS_VERIFY_ONLY)
				tx_pkts_burst[j] = init_packet(mbufpool,
						opts->rekey_td->secure_pkt.data,
						opts->rekey_td->secure_pkt.len);
			else {
				tx_pkts_burst[j] = init_packet(mbufpool,
						opts->rekey_td->plain_pkt.data,
						opts->rekey_td->plain_pkt.len);

				tx_pkts_burst[j]->ol_flags |= RTE_MBUF_F_TX_MACSEC;
			}
			if (tx_pkts_burst[j] == NULL) {
				while (j--)
					rte_pktmbuf_free(tx_pkts_burst[j]);
				ret = TEST_FAILED;
				goto out;
			}
			j++;
		}

		if (op == MCS_DECAP || op == MCS_ENCAP_DECAP ||
				op == MCS_VERIFY_ONLY || op == MCS_AUTH_VERIFY) {
			for (an = 0; an < RTE_SECURITY_MACSEC_NUM_AN; an++) {
				if (opts->rekey_en && an ==
						(opts->rekey_td->secure_pkt.data[tci_off] &
						RTE_MACSEC_AN_MASK))
					fill_macsec_sa_conf(opts->rekey_td, &sa_conf,
						RTE_SECURITY_MACSEC_DIR_RX, an, tci_off);
				else
				/* For simplicity, using same SA conf for all AN */
					fill_macsec_sa_conf(td[i], &sa_conf,
						RTE_SECURITY_MACSEC_DIR_RX, an, tci_off);
				id = rte_security_macsec_sa_create(ctx, &sa_conf);
				if (id < 0) {
					printf("MACsec SA create failed : %d.\n", id);
					return TEST_FAILED;
				}
				rx_sa_id[i][an] = (uint16_t)id;
			}
			fill_macsec_sc_conf(td[i], &sc_conf, opts,
					RTE_SECURITY_MACSEC_DIR_RX, rx_sa_id[i], tci_off);
			id = rte_security_macsec_sc_create(ctx, &sc_conf);
			if (id < 0) {
				printf("MACsec SC create failed : %d.\n", id);
				goto out;
			}
			rx_sc_id[i] = (uint16_t)id;

			/* Create Inline IPsec session. */
			ret = fill_session_conf(td[i], port_id, opts, &sess_conf,
					RTE_SECURITY_MACSEC_DIR_RX, rx_sc_id[i], tci_off);
			if (ret)
				return TEST_FAILED;

			rx_sess[i] = rte_security_session_create(ctx, &sess_conf,
					sess_pool);
			if (rx_sess[i] == NULL) {
				printf("SEC Session init failed.\n");
				return TEST_FAILED;
			}
			ret = create_default_flow(td[i], port_id,
					RTE_SECURITY_MACSEC_DIR_RX, rx_sess[i]);
			if (ret)
				goto out;
		}
		if (op == MCS_ENCAP || op == MCS_ENCAP_DECAP ||
				op == MCS_AUTH_ONLY || op == MCS_AUTH_VERIFY) {
			int id;

			fill_macsec_sa_conf(td[i], &sa_conf,
					RTE_SECURITY_MACSEC_DIR_TX,
					td[i]->secure_pkt.data[tci_off] & RTE_MACSEC_AN_MASK,
					tci_off);
			id = rte_security_macsec_sa_create(ctx, &sa_conf);
			if (id < 0) {
				printf("MACsec SA create failed : %d.\n", id);
				return TEST_FAILED;
			}
			tx_sa_id[i][0] = (uint16_t)id;
			tx_sa_id[i][1] = MCS_INVALID_SA;
			if (opts->rekey_en) {
				memset(&sa_conf, 0, sizeof(struct rte_security_macsec_sa));
				fill_macsec_sa_conf(opts->rekey_td, &sa_conf,
					RTE_SECURITY_MACSEC_DIR_TX,
					opts->rekey_td->secure_pkt.data[tci_off] &
						RTE_MACSEC_AN_MASK,
					tci_off);
				id = rte_security_macsec_sa_create(ctx, &sa_conf);
				if (id < 0) {
					printf("MACsec rekey SA create failed : %d.\n", id);
					goto out;
				}
				tx_sa_id[i][1] = (uint16_t)id;
			}
			fill_macsec_sc_conf(td[i], &sc_conf, opts,
					RTE_SECURITY_MACSEC_DIR_TX, tx_sa_id[i], tci_off);
			id = rte_security_macsec_sc_create(ctx, &sc_conf);
			if (id < 0) {
				printf("MACsec SC create failed : %d.\n", id);
				goto out;
			}
			tx_sc_id[i] = (uint16_t)id;

			/* Create Inline IPsec session. */
			ret = fill_session_conf(td[i], port_id, opts, &sess_conf,
					RTE_SECURITY_MACSEC_DIR_TX, tx_sc_id[i], tci_off);
			if (ret)
				return TEST_FAILED;

			tx_sess[i] = rte_security_session_create(ctx, &sess_conf,
					sess_pool);
			if (tx_sess[i] == NULL) {
				printf("SEC Session init failed.\n");
				return TEST_FAILED;
			}
			ret = create_default_flow(td[i], port_id,
					RTE_SECURITY_MACSEC_DIR_TX, tx_sess[i]);
			if (ret)
				goto out;
		}
	}

	/* Send packet to ethdev for inline MACsec processing. */
	nb_sent = rte_eth_tx_burst(port_id, 0, tx_pkts_burst, j);

	if (nb_sent != j) {
		printf("\nUnable to TX %d packets, sent: %i", j, nb_sent);
		for ( ; nb_sent < j; nb_sent++)
			rte_pktmbuf_free(tx_pkts_burst[nb_sent]);
		ret = TEST_FAILED;
		goto out;
	}

	rte_pause();

	/* Receive back packet on loopback interface. */
	do {
		nb_rx += rte_eth_rx_burst(port_id, 0,
				&rx_pkts_burst[nb_rx],
				nb_sent - nb_rx);
		if (nb_rx >= nb_sent)
			break;
		rte_delay_ms(1);
	} while (j++ < 5 && nb_rx == 0);

	if (nb_rx != nb_sent) {
		printf("\nUnable to RX all %d packets, received(%i)",
				nb_sent, nb_rx);
		while (--nb_rx >= 0)
			rte_pktmbuf_free(rx_pkts_burst[nb_rx]);
		ret = TEST_FAILED;
		if (opts->check_sectag_interrupts == 1)
			ret = TEST_SUCCESS;
		goto out;
	}

	if (opts->rekey_en) {
		switch (err_vector.event) {
		case RTE_ETH_EVENT_MACSEC_TX_SA_PN_SOFT_EXP:
			printf("Received RTE_ETH_EVENT_MACSEC_TX_SA_PN_SOFT_EXP event\n");
			/* The first sa is active now, so the 0th sa can be
			 * reconfigured. Using the same key as zeroeth sa, but
			 * other key can also be configured.
			 */
			rte_security_macsec_sa_destroy(ctx, tx_sa_id[0][0],
					RTE_SECURITY_MACSEC_DIR_TX);
			fill_macsec_sa_conf(td[0], &sa_conf,
					RTE_SECURITY_MACSEC_DIR_TX,
					td[0]->secure_pkt.data[tci_off] &
					RTE_MACSEC_AN_MASK, tci_off);
			id = rte_security_macsec_sa_create(ctx, &sa_conf);
			if (id < 0) {
				printf("MACsec SA create failed : %d.\n", id);
				return TEST_FAILED;
			}
			tx_sa_id[0][0] = (uint16_t)id;
			break;
		case RTE_ETH_EVENT_MACSEC_RX_SA_PN_SOFT_EXP:
			printf("Received RTE_ETH_EVENT_MACSEC_RX_SA_PN_SOFT_EXP event\n");
			break;
		default:
			printf("Received unsupported event\n");
		}
	}

	if (opts->replay_protect) {
		for (i = 0; i < nb_rx; i++) {
			rte_pktmbuf_free(rx_pkts_burst[i]);
			rx_pkts_burst[i] = NULL;
		}
		ret = TEST_SUCCESS;
		goto out;
	}

	for (i = 0; i < nb_rx; i++) {
		if (opts->rekey_en && i == 1) {
			/* The second received packet is matched with
			 * rekey td
			 */
			ret = test_macsec_post_process(rx_pkts_burst[i],
					opts->rekey_td, op,
					opts->check_out_pkts_untagged);
		} else {
			ret = test_macsec_post_process(rx_pkts_burst[i], td[i],
					op, opts->check_out_pkts_untagged);
		}
		if (ret != TEST_SUCCESS) {
			for ( ; i < nb_rx; i++)
				rte_pktmbuf_free(rx_pkts_burst[i]);
			goto out;
		}

		rte_pktmbuf_free(rx_pkts_burst[i]);
		rx_pkts_burst[i] = NULL;
	}
out:
	if (opts->check_out_pkts_toolong == 1 ||
			opts->check_sa_not_in_use == 1 ||
			opts->check_bad_tag_cnt == 1)
		ret = TEST_SUCCESS;

	for (i = 0; i < opts->nb_td; i++) {
		if (opts->dump_all_stats) {
			mcs_stats_dump(ctx, op,
					rx_sess[i], tx_sess[i],
					rx_sc_id[i], tx_sc_id[i],
					rx_sa_id[i], tx_sa_id[i]);
		} else {
			if (ret == TEST_SUCCESS)
				ret = mcs_stats_check(ctx, op, opts, td[i],
					rx_sess[i], tx_sess[i],
					rx_sc_id[i], tx_sc_id[i],
					rx_sa_id[i], tx_sa_id[i]);
		}
	}

	destroy_default_flow(port_id);

	if (opts->rekey_en)
		rte_eth_dev_callback_unregister(port_id, RTE_ETH_EVENT_MACSEC,
					test_macsec_event_callback, &err_vector);

	/* Destroy session so that other cases can create the session again */
	for (i = 0; i < opts->nb_td; i++) {
		if (op == MCS_ENCAP || op == MCS_ENCAP_DECAP ||
				op == MCS_AUTH_ONLY || op == MCS_AUTH_VERIFY) {
			rte_security_session_destroy(ctx, tx_sess[i]);
			tx_sess[i] = NULL;
			rte_security_macsec_sc_destroy(ctx, tx_sc_id[i],
						RTE_SECURITY_MACSEC_DIR_TX);
			rte_security_macsec_sa_destroy(ctx, tx_sa_id[i][0],
						RTE_SECURITY_MACSEC_DIR_TX);
			if (opts->rekey_en) {
				rte_security_macsec_sa_destroy(ctx, tx_sa_id[i][1],
						RTE_SECURITY_MACSEC_DIR_TX);
			}
		}
		if (op == MCS_DECAP || op == MCS_ENCAP_DECAP ||
				op == MCS_VERIFY_ONLY || op == MCS_AUTH_VERIFY) {
			rte_security_session_destroy(ctx, rx_sess[i]);
			rx_sess[i] = NULL;
			rte_security_macsec_sc_destroy(ctx, rx_sc_id[i],
						RTE_SECURITY_MACSEC_DIR_RX);
			for (j = 0; j < RTE_SECURITY_MACSEC_NUM_AN; j++) {
				rte_security_macsec_sa_destroy(ctx, rx_sa_id[i][j],
						RTE_SECURITY_MACSEC_DIR_RX);
			}
		}
	}

	return ret;
}

static int
test_inline_macsec_encap_all(void)
{
	const struct mcs_test_vector *cur_td;
	struct mcs_test_opts opts = {0};
	int err, all_err = 0;
	int skipped = 0;
	int i, size;

	opts.val_frames = RTE_SECURITY_MACSEC_VALIDATE_STRICT;
	opts.encrypt = true;
	opts.protect_frames = true;
	opts.sa_in_use = 1;
	opts.nb_td = 1;
	opts.sectag_insert_mode = 1;
	opts.mtu = RTE_ETHER_MTU;

	size = (sizeof(list_mcs_cipher_vectors) / sizeof((list_mcs_cipher_vectors)[0]));
	for (i = 0; i < size; i++) {
		cur_td = &list_mcs_cipher_vectors[i];
		err = test_macsec(&cur_td, MCS_ENCAP, &opts);
		if (err == TEST_SKIPPED) {
			printf("Cipher Auth Encryption case %d skipped\n", cur_td->test_idx);
			skipped += 1;
			err = 0;
		} else if (err) {
			printf("\nCipher Auth Encryption case %d failed", cur_td->test_idx);
			err = -1;
		} else {
			printf("\nCipher Auth Encryption case %d Passed", cur_td->test_idx);
			err = 0;
		}
		all_err += err;
	}
	printf("\n%s: Success: %d, Failure: %d, Skipped: %d\n", __func__,
			size + all_err - skipped, -all_err, skipped);

	return skipped > 0 ? TEST_SKIPPED : all_err;
}

static int
test_inline_macsec_decap_all(void)
{
	const struct mcs_test_vector *cur_td;
	struct mcs_test_opts opts = {0};
	int err, all_err = 0;
	int skipped = 0;
	int i, size;

	opts.val_frames = RTE_SECURITY_MACSEC_VALIDATE_STRICT;
	opts.sa_in_use = 1;
	opts.nb_td = 1;
	opts.sectag_insert_mode = 1;
	opts.mtu = RTE_ETHER_MTU;

	size = (sizeof(list_mcs_cipher_vectors) / sizeof((list_mcs_cipher_vectors)[0]));
	for (i = 0; i < size; i++) {
		cur_td = &list_mcs_cipher_vectors[i];
		err = test_macsec(&cur_td, MCS_DECAP, &opts);
		if (err == TEST_SKIPPED) {
			printf("Cipher Auth Decryption case %d skipped\n", cur_td->test_idx);
			skipped += 1;
			err = 0;
		} else if (err) {
			printf("\nCipher Auth Decryption case %d failed", cur_td->test_idx);
			err = -1;
		} else {
			printf("\nCipher Auth Decryption case %d Passed", cur_td->test_idx);
			err = 0;
		}
		all_err += err;
	}
	printf("\n%s: Success: %d, Failure: %d, Skipped: %d\n", __func__,
			size + all_err - skipped, -all_err, skipped);

	return skipped > 0 ? TEST_SKIPPED : all_err;
}

static int
test_inline_macsec_auth_only_all(void)
{
	const struct mcs_test_vector *cur_td;
	struct mcs_test_opts opts = {0};
	int err, all_err = 0;
	int skipped = 0;
	int i, size;

	opts.val_frames = RTE_SECURITY_MACSEC_VALIDATE_STRICT;
	opts.protect_frames = true;
	opts.sa_in_use = 1;
	opts.nb_td = 1;
	opts.sectag_insert_mode = 1;
	opts.mtu = RTE_ETHER_MTU;

	size = (sizeof(list_mcs_integrity_vectors) / sizeof((list_mcs_integrity_vectors)[0]));

	for (i = 0; i < size; i++) {
		cur_td = &list_mcs_integrity_vectors[i];
		err = test_macsec(&cur_td, MCS_AUTH_ONLY, &opts);
		if (err == TEST_SKIPPED) {
			printf("Auth Generate case %d skipped\n", cur_td->test_idx);
			skipped += 1;
			err = 0;
		} else if (err) {
			printf("\nAuth Generate case %d failed", cur_td->test_idx);
			err = -1;
		} else {
			printf("\nAuth Generate case %d Passed", cur_td->test_idx);
			err = 0;
		}
		all_err += err;
	}
	printf("\n%s: Success: %d, Failure: %d, Skipped: %d\n", __func__,
			size + all_err - skipped, -all_err, skipped);

	return skipped > 0 ? TEST_SKIPPED : all_err;
}

static int
test_inline_macsec_verify_only_all(void)
{
	const struct mcs_test_vector *cur_td;
	struct mcs_test_opts opts = {0};
	int err, all_err = 0;
	int skipped = 0;
	int i, size;

	opts.val_frames = RTE_SECURITY_MACSEC_VALIDATE_STRICT;
	opts.sa_in_use = 1;
	opts.nb_td = 1;
	opts.sectag_insert_mode = 1;
	opts.mtu = RTE_ETHER_MTU;

	size = (sizeof(list_mcs_integrity_vectors) / sizeof((list_mcs_integrity_vectors)[0]));

	for (i = 0; i < size; i++) {
		cur_td = &list_mcs_integrity_vectors[i];
		err = test_macsec(&cur_td, MCS_VERIFY_ONLY, &opts);
		if (err == TEST_SKIPPED) {
			printf("Auth Verify case %d skipped\n", cur_td->test_idx);
			skipped += 1;
			err = 0;
		} else if (err) {
			printf("\nAuth Verify case %d failed", cur_td->test_idx);
			err = -1;
		} else {
			printf("\nAuth Verify case %d Passed", cur_td->test_idx);
			err = 0;
		}
		all_err += err;
	}
	printf("\n%s: Success: %d, Failure: %d, Skipped: %d\n", __func__,
			size + all_err - skipped, -all_err, skipped);

	return skipped > 0 ? TEST_SKIPPED : all_err;
}

static int
test_inline_macsec_encap_decap_all(void)
{
	const struct mcs_test_vector *cur_td;
	struct mcs_test_opts opts = {0};
	int err, all_err = 0;
	int skipped = 0;
	int i, size;

	opts.val_frames = RTE_SECURITY_MACSEC_VALIDATE_STRICT;
	opts.encrypt = true;
	opts.protect_frames = true;
	opts.sa_in_use = 1;
	opts.nb_td = 1;
	opts.sectag_insert_mode = 1;
	opts.mtu = RTE_ETHER_MTU;

	size = (sizeof(list_mcs_cipher_vectors) / sizeof((list_mcs_cipher_vectors)[0]));

	for (i = 0; i < size; i++) {
		cur_td = &list_mcs_cipher_vectors[i];
		err = test_macsec(&cur_td, MCS_ENCAP_DECAP, &opts);
		if (err == TEST_SKIPPED) {
			printf("Cipher Auth Encap-decap case %d skipped\n", cur_td->test_idx);
			skipped += 1;
			err = 0;
		} else if (err) {
			printf("\nCipher Auth Encap-decap case %d failed", cur_td->test_idx);
			err = -1;
		} else {
			printf("\nCipher Auth Encap-decap case %d Passed", cur_td->test_idx);
			err = 0;
		}
		all_err += err;
	}
	printf("\n%s: Success: %d, Failure: %d, Skipped: %d\n", __func__,
			size + all_err - skipped, -all_err, skipped);

	return skipped > 0 ? TEST_SKIPPED : all_err;
}


static int
test_inline_macsec_auth_verify_all(void)
{
	const struct mcs_test_vector *cur_td;
	struct mcs_test_opts opts = {0};
	int err, all_err = 0;
	int skipped = 0;
	int i, size;

	opts.val_frames = RTE_SECURITY_MACSEC_VALIDATE_STRICT;
	opts.protect_frames = true;
	opts.sa_in_use = 1;
	opts.nb_td = 1;
	opts.sectag_insert_mode = 1;
	opts.mtu = RTE_ETHER_MTU;

	size = (sizeof(list_mcs_integrity_vectors) / sizeof((list_mcs_integrity_vectors)[0]));

	for (i = 0; i < size; i++) {
		cur_td = &list_mcs_integrity_vectors[i];
		err = test_macsec(&cur_td, MCS_AUTH_VERIFY, &opts);
		if (err == TEST_SKIPPED) {
			printf("Auth Generate + Verify case %d skipped\n", cur_td->test_idx);
			skipped += 1;
			err = 0;
		} else if (err) {
			printf("\nAuth Generate + Verify case %d failed", cur_td->test_idx);
			err = -1;
		} else {
			printf("\nAuth Generate + Verify case %d Passed", cur_td->test_idx);
			err = 0;
		}
		all_err += err;
	}
	printf("\n%s: Success: %d, Failure: %d, Skipped: %d\n", __func__,
			size + all_err - skipped, -all_err, skipped);

	return skipped > 0 ? TEST_SKIPPED : all_err;
}

static int
test_inline_macsec_multi_flow(void)
{
	const struct mcs_test_vector *tv[MCS_MAX_FLOWS];
	struct mcs_test_vector iter[MCS_MAX_FLOWS];
	struct mcs_test_opts opts = {0};
	int i, err;

	opts.val_frames = RTE_SECURITY_MACSEC_VALIDATE_STRICT;
	opts.encrypt = true;
	opts.protect_frames = true;
	opts.sa_in_use = 1;
	opts.nb_td = MCS_MAX_FLOWS;
	opts.sectag_insert_mode = 1;
	opts.mtu = RTE_ETHER_MTU;

	for (i = 0; i < MCS_MAX_FLOWS; i++) {
		memcpy(&iter[i].sa_key.data, sa_key, MCS_MULTI_FLOW_TD_KEY_SZ);
		memcpy(&iter[i].plain_pkt.data, eth_addrs[i], 2 * RTE_ETHER_ADDR_LEN);
		memcpy(&iter[i].plain_pkt.data[2 * RTE_ETHER_ADDR_LEN], plain_user_data,
		       MCS_MULTI_FLOW_TD_PLAIN_DATA_SZ);
		memcpy(&iter[i].secure_pkt.data, eth_addrs[i], 2 * RTE_ETHER_ADDR_LEN);
		memcpy(&iter[i].secure_pkt.data[2 * RTE_ETHER_ADDR_LEN], secure_user_data,
		       MCS_MULTI_FLOW_TD_SECURE_DATA_SZ);
		iter[i].sa_key.len = MCS_MULTI_FLOW_TD_KEY_SZ;
		iter[i].plain_pkt.len = MCS_MULTI_FLOW_TD_PLAIN_DATA_SZ +
					(2 * RTE_ETHER_ADDR_LEN);
		iter[i].secure_pkt.len = MCS_MULTI_FLOW_TD_SECURE_DATA_SZ +
					(2 * RTE_ETHER_ADDR_LEN);
		iter[i].alg = RTE_SECURITY_MACSEC_ALG_GCM_128;
		iter[i].ssci = 0x0;
		iter[i].xpn = 0x0;
		tv[i] = (const struct mcs_test_vector *)&iter[i];
	}
	err = test_macsec(tv, MCS_ENCAP_DECAP, &opts);
	if (err == TEST_SKIPPED) {
		printf("Cipher Auth Encryption multi flow skipped\n");
	} else if (err) {
		printf("\nCipher Auth Encryption multi flow failed");
		err = -1;
	} else {
		printf("\nCipher Auth Encryption multi flow Passed");
		err = 0;
	}
	return err;
}

static int
test_inline_macsec_with_vlan(void)
{
	const struct mcs_test_vector *cur_td;
	struct mcs_test_opts opts = {0};
	int err, all_err = 0;
	int skipped = 0;
	int i, size;

	opts.val_frames = RTE_SECURITY_MACSEC_VALIDATE_STRICT;
	opts.protect_frames = true;
	opts.sa_in_use = 1;
	opts.nb_td = 1;
	opts.mtu = RTE_ETHER_MTU;

	size = (sizeof(list_mcs_vlan_vectors) / sizeof((list_mcs_vlan_vectors)[0]));

	for (i = 0; i < size; i++) {
		cur_td = &list_mcs_vlan_vectors[i];
		if (i == 0) {
			opts.sectag_insert_mode = 1;
		} else if (i == 1) {
			opts.sectag_insert_mode = 0; /* offset from special E-type */
			opts.nb_vlan = 1;
		} else if (i == 2) {
			opts.sectag_insert_mode = 0; /* offset from special E-type */
			opts.nb_vlan = 2;
		}
		err = test_macsec(&cur_td, MCS_ENCAP, &opts);
		if (err == TEST_SKIPPED) {
			printf("VLAN Encap case %d skipped", cur_td->test_idx);
			skipped += 1;
			err = 0;
		} else if (err) {
			printf("\n VLAN Encap case %d failed", cur_td->test_idx);
			err = -1;
		} else {
			printf("\n VLAN Encap case %d passed", cur_td->test_idx);
			err = 0;
		}
		all_err += err;
	}
	for (i = 0; i < size; i++) {
		cur_td = &list_mcs_vlan_vectors[i];
		if (i == 0) {
			opts.sectag_insert_mode = 1;
		} else if (i == 1) {
			opts.sectag_insert_mode = 0; /* offset from special E-type */
			opts.nb_vlan = 1;
		} else if (i == 2) {
			opts.sectag_insert_mode = 0; /* offset from special E-type */
			opts.nb_vlan = 2;
		}
		err = test_macsec(&cur_td, MCS_DECAP, &opts);
		if (err == TEST_SKIPPED) {
			printf("VLAN Decap case %d skipped", cur_td->test_idx);
			skipped += 1;
			err = 0;
		} else if (err) {
			printf("\n VLAN Decap case %d failed", cur_td->test_idx);
			err = -1;
		} else {
			printf("\n VLAN Decap case %d passed", cur_td->test_idx);
			err = 0;
		}
		all_err += err;
	}

	printf("\n%s: Success: %d, Failure: %d, Skipped: %d\n", __func__,
			2 * size + all_err - skipped, -all_err, skipped);
	return skipped > 0 ? TEST_SKIPPED : all_err;
}

static int
test_inline_macsec_pkt_drop(void)
{
	const struct mcs_test_vector *cur_td;
	struct mcs_test_opts opts = {0};
	int err, all_err = 0;
	int skipped = 0;
	int i, size;

	opts.val_frames = RTE_SECURITY_MACSEC_VALIDATE_STRICT;
	opts.encrypt = true;
	opts.protect_frames = true;
	opts.sa_in_use = 1;
	opts.nb_td = 1;
	opts.sectag_insert_mode = 1;
	opts.mtu = RTE_ETHER_MTU;

	size = (sizeof(list_mcs_err_cipher_vectors) / sizeof((list_mcs_err_cipher_vectors)[0]));

	for (i = 0; i < size; i++) {
		cur_td = &list_mcs_err_cipher_vectors[i];
		err = test_macsec(&cur_td, MCS_DECAP, &opts);
		if (err == TEST_SKIPPED) {
			printf("Packet drop case %d skipped", cur_td->test_idx);
			skipped += 1;
			err = 0;
		} else if (err) {
			printf("\nPacket drop case %d passed", cur_td->test_idx);
			err = 0;
		} else {
			printf("\nPacket drop case %d failed", cur_td->test_idx);
			err = -1;
		}
		all_err += err;
	}
	printf("\n%s: Success: %d, Failure: %d, Skipped: %d\n", __func__,
			size + all_err - skipped, -all_err, skipped);

	return skipped > 0 ? TEST_SKIPPED : all_err;
}

static int
test_inline_macsec_untagged_rx(void)
{
	const struct mcs_test_vector *cur_td;
	struct mcs_test_opts opts = {0};
	int err, all_err = 0;
	int skipped = 0;
	int i, size;

	opts.val_frames = RTE_SECURITY_MACSEC_VALIDATE_STRICT;
	opts.sa_in_use = 1;
	opts.nb_td = 1;
	opts.sectag_insert_mode = 1;
	opts.mtu = RTE_ETHER_MTU;
	opts.check_untagged_rx = 1;

	size = (sizeof(list_mcs_untagged_cipher_vectors) /
		sizeof((list_mcs_untagged_cipher_vectors)[0]));

	for (i = 0; i < size; i++) {
		cur_td = &list_mcs_untagged_cipher_vectors[i];
		err = test_macsec(&cur_td, MCS_DECAP, &opts);
		if (err == TEST_SKIPPED) {
			skipped += 1;
			err = 0;
		} else if (err)
			err = 0;
		else
			err = -1;

		all_err += err;
	}

	opts.val_frames = RTE_SECURITY_MACSEC_VALIDATE_NO_DISCARD;
	for (i = 0; i < size; i++) {
		cur_td = &list_mcs_untagged_cipher_vectors[i];
		err = test_macsec(&cur_td, MCS_DECAP, &opts);
		if (err == TEST_SKIPPED) {
			skipped += 1;
			err = 0;
		} else if (err)
			err = 0;
		else
			err = -1;

		all_err += err;
	}
	printf("\n%s: Success: %d, Failure: %d, Skipped: %d\n", __func__,
			2 * size + all_err - skipped, -all_err, skipped);

	return skipped > 0 ? TEST_SKIPPED : all_err;
}

static int
test_inline_macsec_bad_tag_rx(void)
{
	const struct mcs_test_vector *cur_td;
	struct mcs_test_opts opts = {0};
	int err, all_err = 0;
	int skipped = 0;
	int i, size;

	opts.val_frames = RTE_SECURITY_MACSEC_VALIDATE_STRICT;
	opts.protect_frames = true;
	opts.sa_in_use = 1;
	opts.nb_td = 1;
	opts.sectag_insert_mode = 1;
	opts.mtu = RTE_ETHER_MTU;
	opts.check_bad_tag_cnt = 1;

	size = (sizeof(list_mcs_bad_tag_vectors) / sizeof((list_mcs_bad_tag_vectors)[0]));

	for (i = 0; i < size; i++) {
		cur_td = &list_mcs_bad_tag_vectors[i];
		err = test_macsec(&cur_td, MCS_DECAP, &opts);
		if (err == TEST_SKIPPED) {
			skipped += 1;
			err = 0;
		} else if (err)
			err = -1;
		else
			err = 0;

		all_err += err;
	}

	printf("\n%s: Success: %d, Failure: %d, Skipped: %d\n", __func__,
			size + all_err - skipped, -all_err, skipped);

	return skipped > 0 ? TEST_SKIPPED : all_err;
}

static int
test_inline_macsec_sa_not_in_use(void)
{
	const struct mcs_test_vector *cur_td;
	struct mcs_test_opts opts = {0};
	int err, all_err = 0;
	int skipped = 0;
	int i, size;

	opts.val_frames = RTE_SECURITY_MACSEC_VALIDATE_STRICT;
	opts.protect_frames = true;
	opts.sa_in_use = 0;
	opts.nb_td = 1;
	opts.sectag_insert_mode = 1;
	opts.mtu = RTE_ETHER_MTU;
	opts.check_sa_not_in_use = 1;

	size = (sizeof(list_mcs_cipher_vectors) / sizeof((list_mcs_cipher_vectors)[0]));

	for (i = 0; i < size; i++) {
		cur_td = &list_mcs_cipher_vectors[i];
		err = test_macsec(&cur_td, MCS_DECAP, &opts);
		if (err == TEST_SKIPPED) {
			skipped += 1;
			err = 0;
		} else if (err)
			err = -1;
		else
			err = 0;

		all_err += err;
	}

	printf("\n%s: Success: %d, Failure: %d, Skipped: %d\n", __func__,
			size + all_err - skipped, -all_err, skipped);

	return skipped > 0 ? TEST_SKIPPED : all_err;
}

static int
test_inline_macsec_decap_stats(void)
{
	const struct mcs_test_vector *cur_td;
	struct mcs_test_opts opts = {0};
	int err, all_err = 0;
	int skipped = 0;
	int i, size;

	opts.val_frames = RTE_SECURITY_MACSEC_VALIDATE_STRICT;
	opts.protect_frames = true;
	opts.sa_in_use = 1;
	opts.nb_td = 1;
	opts.sectag_insert_mode = 1;
	opts.mtu = RTE_ETHER_MTU;
	opts.check_decap_stats = 1;

	size = (sizeof(list_mcs_cipher_vectors) / sizeof((list_mcs_cipher_vectors)[0]));

	for (i = 0; i < size; i++) {
		cur_td = &list_mcs_cipher_vectors[i];
		err = test_macsec(&cur_td, MCS_DECAP, &opts);
		if (err == TEST_SKIPPED) {
			printf("Decap stats case %d skipped\n", cur_td->test_idx);
			skipped += 1;
			err = 0;
		} else if (err) {
			printf("\nDecap stats case %d failed", cur_td->test_idx);
			err = -1;
		} else {
			printf("\nDecap stats case %d passed", cur_td->test_idx);
			err = 0;
		}
		all_err += err;
	}
	printf("\n%s: Success: %d, Failure: %d, Skipped: %d\n", __func__,
			size + all_err - skipped, -all_err, skipped);

	return skipped > 0 ? TEST_SKIPPED : all_err;
}

static int
test_inline_macsec_verify_only_stats(void)
{
	const struct mcs_test_vector *cur_td;
	struct mcs_test_opts opts = {0};
	int err, all_err = 0;
	int skipped = 0;
	int i, size;

	opts.val_frames = RTE_SECURITY_MACSEC_VALIDATE_STRICT;
	opts.protect_frames = true;
	opts.sa_in_use = 1;
	opts.nb_td = 1;
	opts.sectag_insert_mode = 1;
	opts.mtu = RTE_ETHER_MTU;
	opts.check_verify_only_stats = 1;

	size = (sizeof(list_mcs_integrity_vectors) / sizeof((list_mcs_integrity_vectors)[0]));

	for (i = 0; i < size; i++) {
		cur_td = &list_mcs_integrity_vectors[i];
		err = test_macsec(&cur_td, MCS_VERIFY_ONLY, &opts);
		if (err == TEST_SKIPPED) {
			printf("Verify only stats case %d skipped\n", cur_td->test_idx);
			skipped += 1;
			err = 0;
		} else if (err) {
			printf("\nVerify only stats case %d failed", cur_td->test_idx);
			err = -1;
		} else {
			printf("\nVerify only stats case %d Passed", cur_td->test_idx);
			err = 0;
		}
		all_err += err;
	}
	printf("\n%s: Success: %d, Failure: %d, Skipped: %d\n", __func__,
			size + all_err - skipped, -all_err, skipped);

	return skipped > 0 ? TEST_SKIPPED : all_err;
}

static int
test_inline_macsec_pkts_invalid_stats(void)
{
	const struct mcs_test_vector *cur_td;
	struct mcs_test_opts opts = {0};
	int err, all_err = 0;
	int skipped = 0;
	int i, size;

	opts.val_frames = RTE_SECURITY_MACSEC_VALIDATE_STRICT;
	opts.protect_frames = true;
	opts.sa_in_use = 1;
	opts.nb_td = 1;
	opts.sectag_insert_mode = 1;
	opts.mtu = RTE_ETHER_MTU;

	size = (sizeof(list_mcs_err_cipher_vectors) / sizeof((list_mcs_err_cipher_vectors)[0]));

	for (i = 0; i < size; i++) {
		cur_td = &list_mcs_err_cipher_vectors[i];
		err = test_macsec(&cur_td, MCS_DECAP, &opts);
		if (err == TEST_SKIPPED) {
			skipped += 1;
			err = 0;
		} else if (err)
			err = 0;
		else
			err = -1;

		all_err += err;
	}
	printf("\n%s: Success: %d, Failure: %d, Skipped: %d\n", __func__,
			size + all_err - skipped, -all_err, skipped);
	return skipped > 0 ? TEST_SKIPPED : all_err;
}

static int
test_inline_macsec_pkts_unchecked_stats(void)
{
	const struct mcs_test_vector *cur_td;
	struct mcs_test_opts opts = {0};
	int err, all_err = 0;
	int skipped = 0;
	int i, size;

	opts.val_frames = RTE_SECURITY_MACSEC_VALIDATE_DISABLE;
	opts.protect_frames = true;
	opts.sa_in_use = 1;
	opts.nb_td = 1;
	opts.sectag_insert_mode = 1;
	opts.mtu = RTE_ETHER_MTU;
	opts.check_pkts_unchecked_stats = 1;

	size = (sizeof(list_mcs_integrity_vectors) / sizeof((list_mcs_integrity_vectors)[0]));

	for (i = 0; i < size; i++) {
		cur_td = &list_mcs_integrity_vectors[i];
		err = test_macsec(&cur_td, MCS_VERIFY_ONLY, &opts);
		if (err == TEST_SKIPPED) {
			skipped += 1;
			err = 0;
		} else if (err)
			err = -1;
		else
			err = 0;

		all_err += err;
	}

	printf("\n%s: Success: %d, Failure: %d, Skipped: %d\n", __func__,
			size + all_err - skipped, -all_err, skipped);
	return skipped > 0 ? TEST_SKIPPED : all_err;
}

static int
test_inline_macsec_out_pkts_untagged(void)
{
	const struct mcs_test_vector *cur_td;
	struct mcs_test_opts opts = {0};
	int err, all_err = 0;
	int skipped = 0;
	int i, size;

	opts.val_frames = RTE_SECURITY_MACSEC_VALIDATE_STRICT;
	opts.encrypt = false;
	opts.protect_frames = false;
	opts.sa_in_use = 1;
	opts.nb_td = 1;
	opts.sectag_insert_mode = 1;
	opts.mtu = RTE_ETHER_MTU;
	opts.check_out_pkts_untagged = 1;

	size = (sizeof(list_mcs_cipher_vectors) / sizeof((list_mcs_cipher_vectors)[0]));
	for (i = 0; i < size; i++) {
		cur_td = &list_mcs_cipher_vectors[i];
		err = test_macsec(&cur_td, MCS_ENCAP, &opts);
		if (err == TEST_SKIPPED) {
			skipped += 1;
			err = 0;
		} else if (err)
			err = -1;
		else
			err = 0;

		all_err += err;
	}

	printf("\n%s: Success: %d, Failure: %d, Skipped: %d\n", __func__,
			size + all_err - skipped, -all_err, skipped);
	return skipped > 0 ? TEST_SKIPPED : all_err;
}

static int
test_inline_macsec_out_pkts_toolong(void)
{
	const struct mcs_test_vector *cur_td;
	struct mcs_test_opts opts = {0};
	int err, all_err = 0;
	int skipped = 0;
	int i, size;

	opts.val_frames = RTE_SECURITY_MACSEC_VALIDATE_NO_DISCARD;
	opts.encrypt = true;
	opts.protect_frames = true;
	opts.sa_in_use = 1;
	opts.nb_td = 1;
	opts.sectag_insert_mode = 1;
	opts.mtu = 50;
	opts.check_out_pkts_toolong = 1;

	size = (sizeof(list_mcs_cipher_vectors) / sizeof((list_mcs_cipher_vectors)[0]));
	for (i = 0; i < size; i++) {
		cur_td = &list_mcs_cipher_vectors[i];
		err = test_macsec(&cur_td, MCS_ENCAP, &opts);
		if (err == TEST_SKIPPED) {
			skipped += 1;
			err = 0;
		} else if (err)
			err = -1;
		else
			err = 0;

		all_err += err;
	}

	printf("\n%s: Success: %d, Failure: %d, Skipped: %d\n", __func__,
			size + all_err - skipped, -all_err, skipped);
	return skipped > 0 ? TEST_SKIPPED : all_err;
}

static int
test_inline_macsec_encap_stats(void)
{
	const struct mcs_test_vector *cur_td;
	struct mcs_test_opts opts = {0};
	int err, all_err = 0;
	int skipped = 0;
	int i, size;

	opts.val_frames = RTE_SECURITY_MACSEC_VALIDATE_STRICT;
	opts.encrypt = true;
	opts.protect_frames = true;
	opts.sa_in_use = 1;
	opts.nb_td = 1;
	opts.sectag_insert_mode = 1;
	opts.mtu = RTE_ETHER_MTU;
	opts.check_encap_stats = 1;

	size = (sizeof(list_mcs_cipher_vectors) / sizeof((list_mcs_cipher_vectors)[0]));
	for (i = 0; i < size; i++) {
		cur_td = &list_mcs_cipher_vectors[i];
		err = test_macsec(&cur_td, MCS_ENCAP, &opts);
		if (err == TEST_SKIPPED) {
			skipped += 1;
			err = 0;
		} else if (err)
			err = -1;
		else
			err = 0;

		all_err += err;
	}

	printf("\n%s: Success: %d, Failure: %d, Skipped: %d\n", __func__,
			size + all_err - skipped, -all_err, skipped);
	return skipped > 0 ? TEST_SKIPPED : all_err;
}

static int
test_inline_macsec_auth_only_stats(void)
{
	const struct mcs_test_vector *cur_td;
	struct mcs_test_opts opts = {0};
	int err, all_err = 0;
	int skipped = 0;
	int i, size;

	opts.val_frames = RTE_SECURITY_MACSEC_VALIDATE_STRICT;
	opts.protect_frames = true;
	opts.sa_in_use = 1;
	opts.nb_td = 1;
	opts.sectag_insert_mode = 1;
	opts.mtu = RTE_ETHER_MTU;
	opts.check_auth_only_stats = 1;

	size = (sizeof(list_mcs_integrity_vectors) / sizeof((list_mcs_integrity_vectors)[0]));

	for (i = 0; i < size; i++) {
		cur_td = &list_mcs_integrity_vectors[i];
		err = test_macsec(&cur_td, MCS_AUTH_ONLY, &opts);
		if (err == TEST_SKIPPED) {
			skipped += 1;
			err = 0;
		} else if (err)
			err = -1;
		else
			err = 0;

		all_err += err;
	}

	printf("\n%s: Success: %d, Failure: %d, Skipped: %d\n", __func__,
			size + all_err - skipped, -all_err, skipped);
	return skipped > 0 ? TEST_SKIPPED : all_err;
}

static int
test_inline_macsec_interrupts_all(void)
{
	struct mcs_err_vector err_vector = {0};
	const struct mcs_test_vector *cur_td;
	struct mcs_test_opts opts = {0};
	int skipped = 0;
	int i, size;
	int err, all_err = 0;
	enum rte_eth_event_macsec_subtype subtype[] =  {
		RTE_ETH_SUBEVENT_MACSEC_RX_SECTAG_V_EQ1,
		RTE_ETH_SUBEVENT_MACSEC_RX_SECTAG_E_EQ0_C_EQ1,
		RTE_ETH_SUBEVENT_MACSEC_RX_SECTAG_SL_GTE48,
		RTE_ETH_SUBEVENT_MACSEC_RX_SECTAG_ES_EQ1_SC_EQ1,
		RTE_ETH_SUBEVENT_MACSEC_RX_SECTAG_SC_EQ1_SCB_EQ1,
	};

	opts.val_frames = RTE_SECURITY_MACSEC_VALIDATE_STRICT;
	opts.protect_frames = true;
	opts.sa_in_use = 1;
	opts.nb_td = 1;
	opts.sectag_insert_mode = 1;
	opts.mtu = RTE_ETHER_MTU;
	opts.check_sectag_interrupts = 1;

	err_vector.event = RTE_ETH_EVENT_MACSEC_UNKNOWN;
	err_vector.event_subtype = RTE_ETH_SUBEVENT_MACSEC_UNKNOWN;
	rte_eth_dev_callback_register(port_id, RTE_ETH_EVENT_MACSEC,
			test_macsec_event_callback, &err_vector);

	size = (sizeof(list_mcs_intr_test_vectors) / sizeof((list_mcs_intr_test_vectors)[0]));

	for (i = 0; i < size; i++) {
		cur_td = &list_mcs_intr_test_vectors[i];
		err = test_macsec(&cur_td, MCS_DECAP, &opts);
		if (err == TEST_SKIPPED) {
			printf("Sectag val err interrupt test case %d skipped",
			       cur_td->test_idx);
			skipped += 1;
			err = 0;
		} else if ((err_vector.event == RTE_ETH_EVENT_MACSEC_SECTAG_VAL_ERR) &&
			   (err_vector.event_subtype == subtype[i])) {
			printf("\nSectag val err interrupt test case %d passed",
			       cur_td->test_idx);
			err = 0;
		} else {
			printf("\nSectag val err interrupt test case %d failed",
			       cur_td->test_idx);
			err = -1;
		}
		all_err += err;
	}
	rte_eth_dev_callback_unregister(port_id, RTE_ETH_EVENT_MACSEC,
			test_macsec_event_callback, &err_vector);

	printf("\n%s: Success: %d, Failure: %d, Skipped: %d\n", __func__,
			size + all_err - skipped, -all_err, skipped);
	return skipped > 0 ? TEST_SKIPPED : all_err;
}

static int
test_inline_macsec_rekey_tx(void)
{
	const struct mcs_test_vector *cur_td;
	struct mcs_test_opts opts = {0};
	int err, all_err = 0;
	int skipped = 0;
	int i, size;

	opts.val_frames = RTE_SECURITY_MACSEC_VALIDATE_STRICT;
	opts.protect_frames = true;
	opts.encrypt = true;
	opts.sa_in_use = 1;
	opts.nb_td = 1;
	opts.sectag_insert_mode = 1;
	opts.mtu = RTE_ETHER_MTU;
	opts.rekey_en = 1;

	size = (sizeof(list_mcs_rekey_vectors) / sizeof((list_mcs_rekey_vectors)[0]));

	for (i = 0; i < size; i++) {
		cur_td = &list_mcs_rekey_vectors[i];
		opts.rekey_td = &list_mcs_rekey_vectors[++i];
		err = test_macsec(&cur_td, MCS_ENCAP, &opts);
		if (err == TEST_SKIPPED) {
			printf("Tx hw rekey test case %d skipped\n", i);
			skipped += 1;
			err = 0;
		} else if (err) {
			printf("Tx hw rekey test case %d failed\n", i);
			err = -1;
		} else {
			printf("Tx hw rekey test case %d passed\n", i);
			err = 0;
		}
		all_err += err;
	}

	printf("\n%s: Success: %d, Failure: %d, Skipped: %d\n", __func__,
			size / 2 + all_err - skipped, -all_err, skipped);
	return skipped > 0 ? TEST_SKIPPED : all_err;
}

static int
test_inline_macsec_rekey_rx(void)
{
	const struct mcs_test_vector *cur_td;
	struct mcs_test_opts opts = {0};
	int err, all_err = 0;
	int skipped = 0;
	int i, size;

	opts.val_frames = RTE_SECURITY_MACSEC_VALIDATE_STRICT;
	opts.protect_frames = true;
	opts.sa_in_use = 1;
	opts.nb_td = 1;
	opts.sectag_insert_mode = 1;
	opts.mtu = RTE_ETHER_MTU;
	opts.rekey_en = 1;

	size = (sizeof(list_mcs_rekey_vectors) / sizeof((list_mcs_rekey_vectors)[0]));
	for (i = 0; i < size; i++) {
		cur_td = &list_mcs_rekey_vectors[i];
		opts.rekey_td = &list_mcs_rekey_vectors[++i];
		err = test_macsec(&cur_td, MCS_DECAP, &opts);
		if (err == TEST_SKIPPED) {
			printf("Rx rekey test case %d skipped\n", i);
			skipped += 1;
			err = 0;
		} else if (err) {
			printf("Rx rekey test case %d failed\n", i);
			err = -1;
		} else {
			printf("Rx rekey test case %d passed\n", i);
			err = 0;
		}
		all_err += err;
	}

	printf("\n%s: Success: %d, Failure: %d, Skipped: %d\n", __func__,
			size / 2 + all_err - skipped, -all_err, skipped);
	return skipped > 0 ? TEST_SKIPPED : all_err;
}

static int
test_inline_macsec_anti_replay(void)
{
	const struct mcs_test_vector *cur_td;
	struct mcs_test_opts opts = {0};
	uint16_t replay_win_sz[2] = {32, 0};
	int err, all_err = 0;
	int skipped = 0;
	int i, size;
	int j;

	opts.val_frames = RTE_SECURITY_MACSEC_VALIDATE_STRICT;
	opts.sa_in_use = 1;
	opts.nb_td = 1;
	opts.sectag_insert_mode = 1;
	opts.replay_protect = 1;

	size = (sizeof(list_mcs_anti_replay_vectors) / sizeof((list_mcs_anti_replay_vectors)[0]));

	for (j = 0; j < 2; j++) {
		opts.replay_win_sz = replay_win_sz[j];

		for (i = 0; i < size; i++) {
			cur_td = &list_mcs_anti_replay_vectors[i];
			opts.ar_td[0] = &list_mcs_anti_replay_vectors[++i];
			opts.ar_td[1] = &list_mcs_anti_replay_vectors[++i];
			opts.ar_td[2] = &list_mcs_anti_replay_vectors[++i];
			err = test_macsec(&cur_td, MCS_DECAP, &opts);
			if (err == TEST_SKIPPED) {
				printf("Replay window: %u, Anti replay test "
				       "case %d skipped\n", opts.replay_win_sz,
				       i);
				skipped += 1;
				err = 0;
			} else if (err) {
				printf("Replay window: %u, Anti replay test "
				       "case %d failed\n", opts.replay_win_sz,
				       i);
				err = -1;
			} else {
				printf("Replay window: %u, Anti replay test "
				       "case %d passed\n", opts.replay_win_sz,
				       i);
				err = 0;
			}
			all_err += err;
		}
	}

	printf("\n%s: Success: %d, Failure: %d, Skipped: %d\n", __func__,
			size / 2 + all_err - skipped, -all_err, skipped);
	return skipped > 0 ? TEST_SKIPPED : all_err;
}

static int
ut_setup_inline_macsec(void)
{
	int ret;

	/* Start device */
	ret = rte_eth_dev_start(port_id);
	if (ret < 0) {
		printf("rte_eth_dev_start: err=%d, port=%d\n",
			ret, port_id);
		return ret;
	}
	/* always enable promiscuous */
	ret = rte_eth_promiscuous_enable(port_id);
	if (ret != 0) {
		printf("rte_eth_promiscuous_enable: err=%s, port=%d\n",
			rte_strerror(-ret), port_id);
		return ret;
	}

	check_all_ports_link_status(1, RTE_PORT_ALL);

	return 0;
}

static void
ut_teardown_inline_macsec(void)
{
	uint16_t portid;
	int ret;

	/* port tear down */
	RTE_ETH_FOREACH_DEV(portid) {
		ret = rte_eth_dev_stop(portid);
		if (ret != 0)
			printf("rte_eth_dev_stop: err=%s, port=%u\n",
			       rte_strerror(-ret), portid);

	}
}

static int
inline_macsec_testsuite_setup(void)
{
	uint16_t nb_rxd;
	uint16_t nb_txd;
	uint16_t nb_ports;
	int ret;
	uint16_t nb_rx_queue = 1, nb_tx_queue = 1;

	printf("Start inline MACsec test.\n");

	nb_ports = rte_eth_dev_count_avail();
	if (nb_ports < NB_ETHPORTS_USED) {
		printf("At least %u port(s) used for test\n",
		       NB_ETHPORTS_USED);
		return TEST_SKIPPED;
	}

	ret = init_mempools(NB_MBUF);
	if (ret)
		return ret;

	if (tx_pkts_burst == NULL) {
		tx_pkts_burst = (struct rte_mbuf **)rte_calloc("tx_buff",
					  MAX_TRAFFIC_BURST,
					  sizeof(void *),
					  RTE_CACHE_LINE_SIZE);
		if (!tx_pkts_burst)
			return TEST_FAILED;

		rx_pkts_burst = (struct rte_mbuf **)rte_calloc("rx_buff",
					  MAX_TRAFFIC_BURST,
					  sizeof(void *),
					  RTE_CACHE_LINE_SIZE);
		if (!rx_pkts_burst)
			return TEST_FAILED;
	}

	printf("Generate %d packets\n", MAX_TRAFFIC_BURST);

	nb_rxd = RTE_TEST_RX_DESC_DEFAULT;
	nb_txd = RTE_TEST_TX_DESC_DEFAULT;

	/* configuring port 0 for the test is enough */
	port_id = 0;
	/* port configure */
	ret = rte_eth_dev_configure(port_id, nb_rx_queue,
				    nb_tx_queue, &port_conf);
	if (ret < 0) {
		printf("Cannot configure device: err=%d, port=%d\n",
			 ret, port_id);
		return ret;
	}
	ret = rte_eth_macaddr_get(port_id, &ports_eth_addr[port_id]);
	if (ret < 0) {
		printf("Cannot get mac address: err=%d, port=%d\n",
			 ret, port_id);
		return ret;
	}
	printf("Port %u ", port_id);
	print_ethaddr("Address:", &ports_eth_addr[port_id]);
	printf("\n");

	/* tx queue setup */
	ret = rte_eth_tx_queue_setup(port_id, 0, nb_txd,
				     SOCKET_ID_ANY, &tx_conf);
	if (ret < 0) {
		printf("rte_eth_tx_queue_setup: err=%d, port=%d\n",
				ret, port_id);
		return ret;
	}
	/* rx queue steup */
	ret = rte_eth_rx_queue_setup(port_id, 0, nb_rxd, SOCKET_ID_ANY,
				     &rx_conf, mbufpool);
	if (ret < 0) {
		printf("rte_eth_rx_queue_setup: err=%d, port=%d\n",
				ret, port_id);
		return ret;
	}

	return 0;
}

static void
inline_macsec_testsuite_teardown(void)
{
	uint16_t portid;
	int ret;

	/* port tear down */
	RTE_ETH_FOREACH_DEV(portid) {
		ret = rte_eth_dev_reset(portid);
		if (ret != 0)
			printf("rte_eth_dev_reset: err=%s, port=%u\n",
			       rte_strerror(-ret), port_id);
	}
	rte_free(tx_pkts_burst);
	rte_free(rx_pkts_burst);
}


static struct unit_test_suite inline_macsec_testsuite  = {
	.suite_name = "Inline MACsec Ethernet Device Unit Test Suite",
	.unit_test_cases = {
		TEST_CASE_NAMED_ST(
			"MACsec Encap + decap Multi flow",
			ut_setup_inline_macsec, ut_teardown_inline_macsec,
			test_inline_macsec_multi_flow),
		TEST_CASE_NAMED_ST(
			"MACsec encap(Cipher+Auth) known vector",
			ut_setup_inline_macsec, ut_teardown_inline_macsec,
			test_inline_macsec_encap_all),
		TEST_CASE_NAMED_ST(
			"MACsec decap(De-cipher+verify) known vector",
			ut_setup_inline_macsec, ut_teardown_inline_macsec,
			test_inline_macsec_decap_all),
		TEST_CASE_NAMED_ST(
			"MACsec auth only known vector",
			ut_setup_inline_macsec, ut_teardown_inline_macsec,
			test_inline_macsec_auth_only_all),
		TEST_CASE_NAMED_ST(
			"MACsec verify only known vector",
			ut_setup_inline_macsec, ut_teardown_inline_macsec,
			test_inline_macsec_verify_only_all),
		TEST_CASE_NAMED_ST(
			"MACsec encap + decap known vector",
			ut_setup_inline_macsec, ut_teardown_inline_macsec,
			test_inline_macsec_encap_decap_all),
		TEST_CASE_NAMED_ST(
			"MACsec auth + verify known vector",
			ut_setup_inline_macsec, ut_teardown_inline_macsec,
			test_inline_macsec_auth_verify_all),
		TEST_CASE_NAMED_ST(
			"MACsec Encap and decap with VLAN",
			ut_setup_inline_macsec, ut_teardown_inline_macsec,
			test_inline_macsec_with_vlan),
		TEST_CASE_NAMED_ST(
			"MACsec packet drop",
			ut_setup_inline_macsec, ut_teardown_inline_macsec,
			test_inline_macsec_pkt_drop),
		TEST_CASE_NAMED_ST(
			"MACsec untagged Rx",
			ut_setup_inline_macsec, ut_teardown_inline_macsec,
			test_inline_macsec_untagged_rx),
		TEST_CASE_NAMED_ST(
			"MACsec bad tag Rx",
			ut_setup_inline_macsec, ut_teardown_inline_macsec,
			test_inline_macsec_bad_tag_rx),
		TEST_CASE_NAMED_ST(
			"MACsec SA not in use",
			ut_setup_inline_macsec, ut_teardown_inline_macsec,
			test_inline_macsec_sa_not_in_use),
		TEST_CASE_NAMED_ST(
			"MACsec decap stats",
			ut_setup_inline_macsec, ut_teardown_inline_macsec,
			test_inline_macsec_decap_stats),
		TEST_CASE_NAMED_ST(
			"MACsec verify only stats",
			ut_setup_inline_macsec, ut_teardown_inline_macsec,
			test_inline_macsec_verify_only_stats),
		TEST_CASE_NAMED_ST(
			"MACsec pkts invalid stats",
			ut_setup_inline_macsec, ut_teardown_inline_macsec,
			test_inline_macsec_pkts_invalid_stats),
		TEST_CASE_NAMED_ST(
			"MACsec pkts unchecked stats",
			ut_setup_inline_macsec, ut_teardown_inline_macsec,
			test_inline_macsec_pkts_unchecked_stats),
		TEST_CASE_NAMED_ST(
			"MACsec out pkts untagged",
			ut_setup_inline_macsec, ut_teardown_inline_macsec,
			test_inline_macsec_out_pkts_untagged),
		TEST_CASE_NAMED_ST(
			"MACsec out pkts too long",
			ut_setup_inline_macsec, ut_teardown_inline_macsec,
			test_inline_macsec_out_pkts_toolong),
		TEST_CASE_NAMED_ST(
			"MACsec Encap stats",
			ut_setup_inline_macsec, ut_teardown_inline_macsec,
			test_inline_macsec_encap_stats),
		TEST_CASE_NAMED_ST(
			"MACsec auth only stats",
			ut_setup_inline_macsec, ut_teardown_inline_macsec,
			test_inline_macsec_auth_only_stats),
		TEST_CASE_NAMED_ST(
			"MACsec interrupts all",
			ut_setup_inline_macsec, ut_teardown_inline_macsec,
			test_inline_macsec_interrupts_all),
		TEST_CASE_NAMED_ST(
			"MACsec re-key Tx",
			ut_setup_inline_macsec, ut_teardown_inline_macsec,
			test_inline_macsec_rekey_tx),
		TEST_CASE_NAMED_ST(
			"MACsec re-key Rx",
			ut_setup_inline_macsec, ut_teardown_inline_macsec,
			test_inline_macsec_rekey_rx),
		TEST_CASE_NAMED_ST(
			"MACsec anti-replay",
			ut_setup_inline_macsec, ut_teardown_inline_macsec,
			test_inline_macsec_anti_replay),

		TEST_CASES_END() /**< NULL terminate unit test array */
	},
};

static int
test_inline_macsec(void)
{
	inline_macsec_testsuite.setup = inline_macsec_testsuite_setup;
	inline_macsec_testsuite.teardown = inline_macsec_testsuite_teardown;
	return unit_test_suite_runner(&inline_macsec_testsuite);
}

#endif /* !RTE_EXEC_ENV_WINDOWS */

REGISTER_TEST_COMMAND(inline_macsec_autotest, test_inline_macsec);
