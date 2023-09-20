/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2022 Marvell.
 */


#include <stdio.h>
#include <inttypes.h>

#include <rte_ethdev.h>
#include <rte_malloc.h>
#include <rte_security.h>

#include "test.h"
#include "test_security_inline_proto_vectors.h"

#ifdef RTE_EXEC_ENV_WINDOWS
static int
test_inline_ipsec(void)
{
	printf("Inline ipsec not supported on Windows, skipping test\n");
	return TEST_SKIPPED;
}

static int
test_event_inline_ipsec(void)
{
	printf("Event inline ipsec not supported on Windows, skipping test\n");
	return TEST_SKIPPED;
}

static int
test_inline_ipsec_sg(void)
{
	printf("Inline ipsec SG not supported on Windows, skipping test\n");
	return TEST_SKIPPED;
}

#else

#include <rte_eventdev.h>
#include <rte_event_eth_rx_adapter.h>
#include <rte_event_eth_tx_adapter.h>

#define NB_ETHPORTS_USED		1
#define MEMPOOL_CACHE_SIZE		32
#define MAX_PKT_BURST			32
#define RX_DESC_DEFAULT	1024
#define TX_DESC_DEFAULT	1024
#define RTE_PORT_ALL		(~(uint16_t)0x0)

#define RX_PTHRESH 8 /**< Default values of RX prefetch threshold reg. */
#define RX_HTHRESH 8 /**< Default values of RX host threshold reg. */
#define RX_WTHRESH 0 /**< Default values of RX write-back threshold reg. */

#define TX_PTHRESH 32 /**< Default values of TX prefetch threshold reg. */
#define TX_HTHRESH 0  /**< Default values of TX host threshold reg. */
#define TX_WTHRESH 0  /**< Default values of TX write-back threshold reg. */

#define MAX_TRAFFIC_BURST		2048
#define NB_MBUF				10240

#define ENCAP_DECAP_BURST_SZ		33
#define APP_REASS_TIMEOUT		10

extern struct ipsec_test_data pkt_aes_128_gcm;
extern struct ipsec_test_data pkt_aes_192_gcm;
extern struct ipsec_test_data pkt_aes_256_gcm;
extern struct ipsec_test_data pkt_aes_128_gcm_frag;
extern struct ipsec_test_data pkt_aes_128_cbc_null;
extern struct ipsec_test_data pkt_null_aes_xcbc;
extern struct ipsec_test_data pkt_aes_128_cbc_hmac_sha384;
extern struct ipsec_test_data pkt_aes_128_cbc_hmac_sha512;
extern struct ipsec_test_data pkt_3des_cbc_hmac_sha256;
extern struct ipsec_test_data pkt_3des_cbc_hmac_sha384;
extern struct ipsec_test_data pkt_3des_cbc_hmac_sha512;
extern struct ipsec_test_data pkt_3des_cbc_hmac_sha256_v6;
extern struct ipsec_test_data pkt_des_cbc_hmac_sha256;
extern struct ipsec_test_data pkt_des_cbc_hmac_sha384;
extern struct ipsec_test_data pkt_des_cbc_hmac_sha512;
extern struct ipsec_test_data pkt_des_cbc_hmac_sha256_v6;
extern struct ipsec_test_data pkt_aes_128_cbc_md5;

static struct rte_mempool *mbufpool;
static struct rte_mempool *sess_pool;
/* ethernet addresses of ports */
static struct rte_ether_addr ports_eth_addr[RTE_MAX_ETHPORTS];

static struct rte_eth_conf port_conf = {
	.rxmode = {
		.mq_mode = RTE_ETH_MQ_RX_NONE,
		.offloads = RTE_ETH_RX_OFFLOAD_CHECKSUM |
			    RTE_ETH_RX_OFFLOAD_SECURITY,
	},
	.txmode = {
		.mq_mode = RTE_ETH_MQ_TX_NONE,
		.offloads = RTE_ETH_TX_OFFLOAD_SECURITY |
			    RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE,
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
static uint8_t eventdev_id;
static uint8_t rx_adapter_id;
static uint8_t tx_adapter_id;
static uint16_t plaintext_len;
static bool sg_mode;

static bool event_mode_enabled;

static uint64_t link_mbps;

static int ip_reassembly_dynfield_offset = -1;

static struct rte_flow *default_flow[RTE_MAX_ETHPORTS];

/* Create Inline IPsec session */
static int
create_inline_ipsec_session(struct ipsec_test_data *sa, uint16_t portid,
		void **sess, void **ctx,
		uint32_t *ol_flags, const struct ipsec_test_flags *flags,
		struct rte_security_session_conf *sess_conf)
{
	uint16_t src_v6[8] = {0x2607, 0xf8b0, 0x400c, 0x0c03, 0x0000, 0x0000,
				0x0000, 0x001a};
	uint16_t dst_v6[8] = {0x2001, 0x0470, 0xe5bf, 0xdead, 0x4957, 0x2174,
				0xe82c, 0x4887};
	uint32_t src_v4 = rte_cpu_to_be_32(RTE_IPV4(192, 168, 1, 2));
	uint32_t dst_v4 = rte_cpu_to_be_32(RTE_IPV4(192, 168, 1, 1));
	struct rte_security_capability_idx sec_cap_idx;
	const struct rte_security_capability *sec_cap;
	enum rte_security_ipsec_sa_direction dir;
	void *sec_ctx;
	uint32_t verify;

	sess_conf->action_type = RTE_SECURITY_ACTION_TYPE_INLINE_PROTOCOL;
	sess_conf->protocol = RTE_SECURITY_PROTOCOL_IPSEC;
	sess_conf->ipsec = sa->ipsec_xform;

	dir = sa->ipsec_xform.direction;
	verify = flags->tunnel_hdr_verify;

	if ((dir == RTE_SECURITY_IPSEC_SA_DIR_INGRESS) && verify) {
		if (verify == RTE_SECURITY_IPSEC_TUNNEL_VERIFY_SRC_DST_ADDR)
			src_v4 += 1;
		else if (verify == RTE_SECURITY_IPSEC_TUNNEL_VERIFY_DST_ADDR)
			dst_v4 += 1;
	}

	if (sa->ipsec_xform.mode == RTE_SECURITY_IPSEC_SA_MODE_TUNNEL) {
		if (sa->ipsec_xform.tunnel.type ==
				RTE_SECURITY_IPSEC_TUNNEL_IPV4) {
			memcpy(&sess_conf->ipsec.tunnel.ipv4.src_ip, &src_v4,
					sizeof(src_v4));
			memcpy(&sess_conf->ipsec.tunnel.ipv4.dst_ip, &dst_v4,
					sizeof(dst_v4));

			if (flags->df == TEST_IPSEC_SET_DF_0_INNER_1)
				sess_conf->ipsec.tunnel.ipv4.df = 0;

			if (flags->df == TEST_IPSEC_SET_DF_1_INNER_0)
				sess_conf->ipsec.tunnel.ipv4.df = 1;

			if (flags->dscp == TEST_IPSEC_SET_DSCP_0_INNER_1)
				sess_conf->ipsec.tunnel.ipv4.dscp = 0;

			if (flags->dscp == TEST_IPSEC_SET_DSCP_1_INNER_0)
				sess_conf->ipsec.tunnel.ipv4.dscp =
						TEST_IPSEC_DSCP_VAL;
		} else {
			if (flags->dscp == TEST_IPSEC_SET_DSCP_0_INNER_1)
				sess_conf->ipsec.tunnel.ipv6.dscp = 0;

			if (flags->dscp == TEST_IPSEC_SET_DSCP_1_INNER_0)
				sess_conf->ipsec.tunnel.ipv6.dscp =
						TEST_IPSEC_DSCP_VAL;

			if (flags->flabel == TEST_IPSEC_SET_FLABEL_0_INNER_1)
				sess_conf->ipsec.tunnel.ipv6.flabel = 0;

			if (flags->flabel == TEST_IPSEC_SET_FLABEL_1_INNER_0)
				sess_conf->ipsec.tunnel.ipv6.flabel =
						TEST_IPSEC_FLABEL_VAL;

			memcpy(&sess_conf->ipsec.tunnel.ipv6.src_addr, &src_v6,
					sizeof(src_v6));
			memcpy(&sess_conf->ipsec.tunnel.ipv6.dst_addr, &dst_v6,
					sizeof(dst_v6));
		}
	}

	/* Save SA as userdata for the security session. When
	 * the packet is received, this userdata will be
	 * retrieved using the metadata from the packet.
	 *
	 * The PMD is expected to set similar metadata for other
	 * operations, like rte_eth_event, which are tied to
	 * security session. In such cases, the userdata could
	 * be obtained to uniquely identify the security
	 * parameters denoted.
	 */

	sess_conf->userdata = (void *) sa;

	sec_ctx = rte_eth_dev_get_sec_ctx(portid);
	if (sec_ctx == NULL) {
		printf("Ethernet device doesn't support security features.\n");
		return TEST_SKIPPED;
	}

	sec_cap_idx.action = RTE_SECURITY_ACTION_TYPE_INLINE_PROTOCOL;
	sec_cap_idx.protocol = RTE_SECURITY_PROTOCOL_IPSEC;
	sec_cap_idx.ipsec.proto = sess_conf->ipsec.proto;
	sec_cap_idx.ipsec.mode = sess_conf->ipsec.mode;
	sec_cap_idx.ipsec.direction = sess_conf->ipsec.direction;
	sec_cap = rte_security_capability_get(sec_ctx, &sec_cap_idx);
	if (sec_cap == NULL) {
		printf("No capabilities registered\n");
		return TEST_SKIPPED;
	}

	if (sa->aead || sa->aes_gmac)
		memcpy(&sess_conf->ipsec.salt, sa->salt.data,
			RTE_MIN(sizeof(sess_conf->ipsec.salt), sa->salt.len));

	/* Copy cipher session parameters */
	if (sa->aead) {
		rte_memcpy(sess_conf->crypto_xform, &sa->xform.aead,
				sizeof(struct rte_crypto_sym_xform));
		sess_conf->crypto_xform->aead.key.data = sa->key.data;
		/* Verify crypto capabilities */
		if (test_ipsec_crypto_caps_aead_verify(sec_cap,
					sess_conf->crypto_xform) != 0) {
			RTE_LOG(INFO, USER1,
				"Crypto capabilities not supported\n");
			return TEST_SKIPPED;
		}
	} else {
		if (dir == RTE_SECURITY_IPSEC_SA_DIR_EGRESS) {
			rte_memcpy(&sess_conf->crypto_xform->cipher,
					&sa->xform.chain.cipher.cipher,
					sizeof(struct rte_crypto_cipher_xform));

			rte_memcpy(&sess_conf->crypto_xform->next->auth,
					&sa->xform.chain.auth.auth,
					sizeof(struct rte_crypto_auth_xform));
			sess_conf->crypto_xform->cipher.key.data =
							sa->key.data;
			sess_conf->crypto_xform->next->auth.key.data =
							sa->auth_key.data;
			/* Verify crypto capabilities */
			if (test_ipsec_crypto_caps_cipher_verify(sec_cap,
					sess_conf->crypto_xform) != 0) {
				RTE_LOG(INFO, USER1,
					"Cipher crypto capabilities not supported\n");
				return TEST_SKIPPED;
			}

			if (test_ipsec_crypto_caps_auth_verify(sec_cap,
					sess_conf->crypto_xform->next) != 0) {
				RTE_LOG(INFO, USER1,
					"Auth crypto capabilities not supported\n");
				return TEST_SKIPPED;
			}
		} else {
			rte_memcpy(&sess_conf->crypto_xform->next->cipher,
					&sa->xform.chain.cipher.cipher,
					sizeof(struct rte_crypto_cipher_xform));
			rte_memcpy(&sess_conf->crypto_xform->auth,
					&sa->xform.chain.auth.auth,
					sizeof(struct rte_crypto_auth_xform));
			sess_conf->crypto_xform->auth.key.data =
							sa->auth_key.data;
			sess_conf->crypto_xform->next->cipher.key.data =
							sa->key.data;

			/* Verify crypto capabilities */
			if (test_ipsec_crypto_caps_cipher_verify(sec_cap,
					sess_conf->crypto_xform->next) != 0) {
				RTE_LOG(INFO, USER1,
					"Cipher crypto capabilities not supported\n");
				return TEST_SKIPPED;
			}

			if (test_ipsec_crypto_caps_auth_verify(sec_cap,
					sess_conf->crypto_xform) != 0) {
				RTE_LOG(INFO, USER1,
					"Auth crypto capabilities not supported\n");
				return TEST_SKIPPED;
			}
		}
	}

	if (test_ipsec_sec_caps_verify(&sess_conf->ipsec, sec_cap, false) != 0)
		return TEST_SKIPPED;

	if ((sa->ipsec_xform.direction ==
			RTE_SECURITY_IPSEC_SA_DIR_EGRESS) &&
			(sa->ipsec_xform.options.iv_gen_disable == 1)) {
		/* Set env variable when IV generation is disabled */
		char arr[128];
		int len = 0, j = 0;
		int iv_len = (sa->aead || sa->aes_gmac) ? 8 : 16;

		for (; j < iv_len; j++)
			len += snprintf(arr+len, sizeof(arr) - len,
					"0x%x, ", sa->iv.data[j]);
		setenv("ETH_SEC_IV_OVR", arr, 1);
	}

	*sess = rte_security_session_create(sec_ctx, sess_conf, sess_pool);
	if (*sess == NULL) {
		printf("SEC Session init failed.\n");
		return TEST_FAILED;
	}

	*ol_flags = sec_cap->ol_flags;
	*ctx = sec_ctx;

	return 0;
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

		if (all_ports_up == 0) {
			fflush(stdout);
			rte_delay_ms(CHECK_INTERVAL);
		}

		/* set the print_flag if all ports up or timeout */
		if (all_ports_up == 1 || count == (MAX_CHECK_TIME - 1))
			print_flag = 1;
	}
}

static void
print_ethaddr(const char *name, const struct rte_ether_addr *eth_addr)
{
	char buf[RTE_ETHER_ADDR_FMT_SIZE];
	rte_ether_format_addr(buf, RTE_ETHER_ADDR_FMT_SIZE, eth_addr);
	printf("%s%s", name, buf);
}

static void
copy_buf_to_pkt_segs(const uint8_t *buf, unsigned int len,
		     struct rte_mbuf *pkt, unsigned int offset)
{
	unsigned int copied = 0;
	unsigned int copy_len;
	struct rte_mbuf *seg;
	void *seg_buf;

	seg = pkt;
	while (offset >= rte_pktmbuf_tailroom(seg)) {
		offset -= rte_pktmbuf_tailroom(seg);
		seg = seg->next;
	}
	copy_len = seg->buf_len - seg->data_off - offset;
	seg_buf = rte_pktmbuf_mtod_offset(seg, char *, offset);
	while (len > copy_len) {
		rte_memcpy(seg_buf, buf + copied, (size_t) copy_len);
		len -= copy_len;
		copied += copy_len;
		seg->data_len += copy_len;

		seg = seg->next;
		copy_len = seg->buf_len - seg->data_off;
		seg_buf = rte_pktmbuf_mtod(seg, void *);
	}
	rte_memcpy(seg_buf, buf + copied, (size_t) len);
	seg->data_len = len;

	pkt->pkt_len += copied + len;
}

static bool
is_outer_ipv4(struct ipsec_test_data *td)
{
	bool outer_ipv4;

	if (td->ipsec_xform.direction == RTE_SECURITY_IPSEC_SA_DIR_INGRESS ||
	    td->ipsec_xform.mode == RTE_SECURITY_IPSEC_SA_MODE_TRANSPORT)
		outer_ipv4 = (((td->input_text.data[0] & 0xF0) >> 4) == IPVERSION);
	else
		outer_ipv4 = (td->ipsec_xform.tunnel.type == RTE_SECURITY_IPSEC_TUNNEL_IPV4);
	return outer_ipv4;
}

static inline struct rte_mbuf *
init_packet(struct rte_mempool *mp, const uint8_t *data, unsigned int len, bool outer_ipv4)
{
	struct rte_mbuf *pkt, *tail;
	uint16_t space;

	pkt = rte_pktmbuf_alloc(mp);
	if (pkt == NULL)
		return NULL;

	if (outer_ipv4) {
		rte_memcpy(rte_pktmbuf_append(pkt, RTE_ETHER_HDR_LEN),
				&dummy_ipv4_eth_hdr, RTE_ETHER_HDR_LEN);
		pkt->l3_len = sizeof(struct rte_ipv4_hdr);
	} else {
		rte_memcpy(rte_pktmbuf_append(pkt, RTE_ETHER_HDR_LEN),
				&dummy_ipv6_eth_hdr, RTE_ETHER_HDR_LEN);
		pkt->l3_len = sizeof(struct rte_ipv6_hdr);
	}
	pkt->l2_len = RTE_ETHER_HDR_LEN;

	space = rte_pktmbuf_tailroom(pkt);
	tail = pkt;
	/* Error if SG mode is not enabled */
	if (!sg_mode && space < len) {
		rte_pktmbuf_free(pkt);
		return NULL;
	}
	/* Extra room for expansion */
	while (space < len) {
		tail->next = rte_pktmbuf_alloc(mp);
		if (!tail->next)
			goto error;
		tail = tail->next;
		space += rte_pktmbuf_tailroom(tail);
		pkt->nb_segs++;
	}

	if (pkt->buf_len > len + RTE_ETHER_HDR_LEN)
		rte_memcpy(rte_pktmbuf_append(pkt, len), data, len);
	else
		copy_buf_to_pkt_segs(data, len, pkt, RTE_ETHER_HDR_LEN);
	return pkt;
error:
	rte_pktmbuf_free(pkt);
	return NULL;
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
				MEMPOOL_CACHE_SIZE, RTE_CACHE_LINE_SIZE,
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

static int
create_default_flow(uint16_t portid)
{
	struct rte_flow_action action[2];
	struct rte_flow_item pattern[2];
	struct rte_flow_attr attr = {0};
	struct rte_flow_error err;
	struct rte_flow *flow;
	int ret;

	/* Add the default rte_flow to enable SECURITY for all ESP packets */

	pattern[0].type = RTE_FLOW_ITEM_TYPE_ESP;
	pattern[0].spec = NULL;
	pattern[0].mask = NULL;
	pattern[0].last = NULL;
	pattern[1].type = RTE_FLOW_ITEM_TYPE_END;

	action[0].type = RTE_FLOW_ACTION_TYPE_SECURITY;
	action[0].conf = NULL;
	action[1].type = RTE_FLOW_ACTION_TYPE_END;
	action[1].conf = NULL;

	attr.ingress = 1;

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

	default_flow[portid] = flow;

	return 0;
}

static void
destroy_default_flow(uint16_t portid)
{
	struct rte_flow_error err;
	int ret;

	if (!default_flow[portid])
		return;
	ret = rte_flow_destroy(portid, default_flow[portid], &err);
	if (ret) {
		printf("\nDefault flow rule destroy failed\n");
		return;
	}
	default_flow[portid] = NULL;
}

struct rte_mbuf **tx_pkts_burst;
struct rte_mbuf **rx_pkts_burst;

static int
compare_pkt_data(struct rte_mbuf *m, uint8_t *ref, unsigned int tot_len)
{
	unsigned int len;
	unsigned int nb_segs = m->nb_segs;
	unsigned int matched = 0;
	struct rte_mbuf *save = m;

	while (m) {
		len = tot_len;
		if (len > m->data_len)
			len = m->data_len;
		if (len != 0) {
			if (memcmp(rte_pktmbuf_mtod(m, char *),
					ref + matched, len)) {
				printf("\n====Reassembly case failed: Data Mismatch");
				rte_hexdump(stdout, "Reassembled",
					rte_pktmbuf_mtod(m, char *),
					len);
				rte_hexdump(stdout, "reference",
					ref + matched,
					len);
				return TEST_FAILED;
			}
		}
		tot_len -= len;
		matched += len;
		m = m->next;
	}

	if (tot_len) {
		printf("\n====Reassembly case failed: Data Missing %u",
		       tot_len);
		printf("\n====nb_segs %u, tot_len %u", nb_segs, tot_len);
		rte_pktmbuf_dump(stderr, save, -1);
		return TEST_FAILED;
	}
	return TEST_SUCCESS;
}

static inline bool
is_ip_reassembly_incomplete(struct rte_mbuf *mbuf)
{
	static uint64_t ip_reassembly_dynflag;
	int ip_reassembly_dynflag_offset;

	if (ip_reassembly_dynflag == 0) {
		ip_reassembly_dynflag_offset = rte_mbuf_dynflag_lookup(
			RTE_MBUF_DYNFLAG_IP_REASSEMBLY_INCOMPLETE_NAME, NULL);
		if (ip_reassembly_dynflag_offset < 0)
			return false;
		ip_reassembly_dynflag = RTE_BIT64(ip_reassembly_dynflag_offset);
	}

	return (mbuf->ol_flags & ip_reassembly_dynflag) != 0;
}

static void
free_mbuf(struct rte_mbuf *mbuf)
{
	rte_eth_ip_reassembly_dynfield_t dynfield;

	if (!mbuf)
		return;

	if (!is_ip_reassembly_incomplete(mbuf)) {
		rte_pktmbuf_free(mbuf);
	} else {
		if (ip_reassembly_dynfield_offset < 0)
			return;

		while (mbuf) {
			dynfield = *RTE_MBUF_DYNFIELD(mbuf,
					ip_reassembly_dynfield_offset,
					rte_eth_ip_reassembly_dynfield_t *);
			rte_pktmbuf_free(mbuf);
			if (dynfield.nb_frags == 0)
				break;
			mbuf = dynfield.next_frag;
		}
	}
}


static int
get_and_verify_incomplete_frags(struct rte_mbuf *mbuf,
				struct reassembly_vector *vector)
{
	rte_eth_ip_reassembly_dynfield_t *dynfield[MAX_PKT_BURST];
	int j = 0, ret;
	/**
	 * IP reassembly offload is incomplete, and fragments are listed in
	 * dynfield which can be reassembled in SW.
	 */
	printf("\nHW IP Reassembly is not complete; attempt SW IP Reassembly,"
		"\nMatching with original frags.");

	if (ip_reassembly_dynfield_offset < 0)
		return -1;

	printf("\ncomparing frag: %d", j);
	/* Skip Ethernet header comparison */
	rte_pktmbuf_adj(mbuf, RTE_ETHER_HDR_LEN);
	ret = compare_pkt_data(mbuf, vector->frags[j]->data,
				vector->frags[j]->len);
	if (ret)
		return ret;
	j++;
	dynfield[j] = RTE_MBUF_DYNFIELD(mbuf, ip_reassembly_dynfield_offset,
					rte_eth_ip_reassembly_dynfield_t *);
	printf("\ncomparing frag: %d", j);
	/* Skip Ethernet header comparison */
	rte_pktmbuf_adj(dynfield[j]->next_frag, RTE_ETHER_HDR_LEN);
	ret = compare_pkt_data(dynfield[j]->next_frag, vector->frags[j]->data,
			vector->frags[j]->len);
	if (ret)
		return ret;

	while ((dynfield[j]->nb_frags > 1) &&
			is_ip_reassembly_incomplete(dynfield[j]->next_frag)) {
		j++;
		dynfield[j] = RTE_MBUF_DYNFIELD(dynfield[j-1]->next_frag,
					ip_reassembly_dynfield_offset,
					rte_eth_ip_reassembly_dynfield_t *);
		printf("\ncomparing frag: %d", j);
		/* Skip Ethernet header comparison */
		rte_pktmbuf_adj(dynfield[j]->next_frag, RTE_ETHER_HDR_LEN);
		ret = compare_pkt_data(dynfield[j]->next_frag,
				vector->frags[j]->data, vector->frags[j]->len);
		if (ret)
			return ret;
	}
	return ret;
}

static int
event_tx_burst(struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	struct rte_event ev;
	int i, nb_sent = 0;

	/* Convert packets to events */
	memset(&ev, 0, sizeof(ev));
	ev.sched_type = RTE_SCHED_TYPE_PARALLEL;
	for (i = 0; i < nb_pkts; i++) {
		ev.mbuf = tx_pkts[i];
		ev.mbuf->port = port_id;
		nb_sent += rte_event_eth_tx_adapter_enqueue(
				eventdev_id, port_id, &ev, 1, 0);
	}

	return nb_sent;
}

static int
event_rx_burst(struct rte_mbuf **rx_pkts, uint16_t nb_pkts_to_rx)
{
	int nb_ev, nb_rx = 0, j = 0;
	const int ms_per_pkt = 5;
	struct rte_event ev;

	do {
		nb_ev = rte_event_dequeue_burst(eventdev_id, port_id,
				&ev, 1, 0);

		if (nb_ev == 0) {
			rte_delay_ms(1);
			continue;
		}

		/* Get packet from event */
		if (ev.event_type != RTE_EVENT_TYPE_ETHDEV) {
			printf("Unsupported event type: %i\n",
				ev.event_type);
			continue;
		}
		rx_pkts[nb_rx++] = ev.mbuf;
	} while (j++ < (nb_pkts_to_rx * ms_per_pkt) && nb_rx < nb_pkts_to_rx);

	return nb_rx;
}

static int
verify_inbound_oop(struct ipsec_test_data *td,
		   bool silent, struct rte_mbuf *mbuf)
{
	int ret = TEST_SUCCESS, rc;
	struct rte_mbuf *orig;
	uint32_t len;
	void *data;

	orig = *rte_security_oop_dynfield(mbuf);
	if (!orig) {
		if (!silent)
			printf("\nUnable to get orig buffer OOP session");
		return TEST_FAILED;
	}

	/* Skip Ethernet header comparison */
	rte_pktmbuf_adj(orig, RTE_ETHER_HDR_LEN);

	len = td->input_text.len;
	if (orig->pkt_len != len) {
		if (!silent)
			printf("\nOriginal packet length mismatch, expected %u, got %u ",
			       len, orig->pkt_len);
		ret = TEST_FAILED;
	}

	data = rte_pktmbuf_mtod(orig, void *);
	rc = memcmp(data, td->input_text.data, len);
	if (rc) {
		ret = TEST_FAILED;
		if (silent)
			goto exit;

		printf("TestCase %s line %d: %s\n", __func__, __LINE__,
		       "output text not as expected\n");

		rte_hexdump(stdout, "expected", td->input_text.data, len);
		rte_hexdump(stdout, "actual", data, len);
	}
exit:
	rte_pktmbuf_free(orig);
	return ret;
}

static int
test_ipsec_with_reassembly(struct reassembly_vector *vector,
		const struct ipsec_test_flags *flags)
{
	void *out_ses[ENCAP_DECAP_BURST_SZ] = {0};
	void *in_ses[ENCAP_DECAP_BURST_SZ] = {0};
	struct rte_eth_ip_reassembly_params reass_capa = {0};
	struct rte_security_session_conf sess_conf_out = {0};
	struct rte_security_session_conf sess_conf_in = {0};
	unsigned int nb_tx, burst_sz, nb_sent = 0;
	struct rte_crypto_sym_xform cipher_out = {0};
	struct rte_crypto_sym_xform auth_out = {0};
	struct rte_crypto_sym_xform aead_out = {0};
	struct rte_crypto_sym_xform cipher_in = {0};
	struct rte_crypto_sym_xform auth_in = {0};
	struct rte_crypto_sym_xform aead_in = {0};
	struct ipsec_test_data sa_data;
	void *ctx;
	unsigned int i, nb_rx = 0, j;
	uint32_t ol_flags;
	bool outer_ipv4;
	int ret = 0;

	burst_sz = vector->burst ? ENCAP_DECAP_BURST_SZ : 1;
	nb_tx = vector->nb_frags * burst_sz;

	rte_eth_ip_reassembly_capability_get(port_id, &reass_capa);
	if (reass_capa.max_frags < vector->nb_frags)
		return TEST_SKIPPED;

	memset(tx_pkts_burst, 0, sizeof(tx_pkts_burst[0]) * nb_tx);
	memset(rx_pkts_burst, 0, sizeof(rx_pkts_burst[0]) * nb_tx);

	memcpy(&sa_data, vector->sa_data, sizeof(struct ipsec_test_data));
	sa_data.ipsec_xform.direction =	RTE_SECURITY_IPSEC_SA_DIR_EGRESS;
	outer_ipv4 = is_outer_ipv4(&sa_data);

	for (i = 0; i < nb_tx; i += vector->nb_frags) {
		for (j = 0; j < vector->nb_frags; j++) {
			tx_pkts_burst[i+j] = init_packet(mbufpool,
						vector->frags[j]->data,
						vector->frags[j]->len, outer_ipv4);
			if (tx_pkts_burst[i+j] == NULL) {
				ret = -1;
				printf("\n packed init failed\n");
				goto out;
			}
		}
	}

	for (i = 0; i < burst_sz; i++) {
		memcpy(&sa_data, vector->sa_data,
				sizeof(struct ipsec_test_data));
		/* Update SPI for every new SA */
		sa_data.ipsec_xform.spi += i;
		sa_data.ipsec_xform.direction =
					RTE_SECURITY_IPSEC_SA_DIR_EGRESS;
		if (sa_data.aead) {
			sess_conf_out.crypto_xform = &aead_out;
		} else {
			sess_conf_out.crypto_xform = &cipher_out;
			sess_conf_out.crypto_xform->next = &auth_out;
		}

		/* Create Inline IPsec outbound session. */
		ret = create_inline_ipsec_session(&sa_data, port_id,
				&out_ses[i], &ctx, &ol_flags, flags,
				&sess_conf_out);
		if (ret) {
			printf("\nInline outbound session create failed\n");
			goto out;
		}
	}

	j = 0;
	for (i = 0; i < nb_tx; i++) {
		if (ol_flags & RTE_SECURITY_TX_OLOAD_NEED_MDATA)
			rte_security_set_pkt_metadata(ctx,
				out_ses[j], tx_pkts_burst[i], NULL);
		tx_pkts_burst[i]->ol_flags |= RTE_MBUF_F_TX_SEC_OFFLOAD;

		/* Move to next SA after nb_frags */
		if ((i + 1) % vector->nb_frags == 0)
			j++;
	}

	for (i = 0; i < burst_sz; i++) {
		memcpy(&sa_data, vector->sa_data,
				sizeof(struct ipsec_test_data));
		/* Update SPI for every new SA */
		sa_data.ipsec_xform.spi += i;
		sa_data.ipsec_xform.direction =
					RTE_SECURITY_IPSEC_SA_DIR_INGRESS;

		if (sa_data.aead) {
			sess_conf_in.crypto_xform = &aead_in;
		} else {
			sess_conf_in.crypto_xform = &auth_in;
			sess_conf_in.crypto_xform->next = &cipher_in;
		}
		/* Create Inline IPsec inbound session. */
		ret = create_inline_ipsec_session(&sa_data, port_id, &in_ses[i],
				&ctx, &ol_flags, flags, &sess_conf_in);
		if (ret) {
			printf("\nInline inbound session create failed\n");
			goto out;
		}
	}

	/* Retrieve reassembly dynfield offset if available */
	if (ip_reassembly_dynfield_offset < 0 && vector->nb_frags > 1)
		ip_reassembly_dynfield_offset = rte_mbuf_dynfield_lookup(
				RTE_MBUF_DYNFIELD_IP_REASSEMBLY_NAME, NULL);


	ret = create_default_flow(port_id);
	if (ret)
		goto out;

	if (event_mode_enabled)
		nb_sent = event_tx_burst(tx_pkts_burst, nb_tx);
	else
		nb_sent = rte_eth_tx_burst(port_id, 0, tx_pkts_burst, nb_tx);
	if (nb_sent != nb_tx) {
		ret = -1;
		printf("\nFailed to tx %u pkts", nb_tx);
		goto out;
	}

	rte_delay_ms(1);

	/* Retry few times before giving up */
	nb_rx = 0;
	j = 0;
	if (event_mode_enabled)
		nb_rx = event_rx_burst(rx_pkts_burst, nb_tx);
	else
		do {
			nb_rx += rte_eth_rx_burst(port_id, 0, &rx_pkts_burst[nb_rx],
						  nb_tx - nb_rx);
			j++;
			if (nb_rx >= nb_tx)
				break;
			rte_delay_ms(1);
		} while (j < 5 || !nb_rx);

	/* Check for minimum number of Rx packets expected */
	if ((vector->nb_frags == 1 && nb_rx != nb_tx) ||
	    (vector->nb_frags > 1 && nb_rx < burst_sz)) {
		printf("\nreceived less Rx pkts(%u) pkts\n", nb_rx);
		ret = TEST_FAILED;
		goto out;
	}

	for (i = 0; i < nb_rx; i++) {
		if (vector->nb_frags > 1 &&
		    is_ip_reassembly_incomplete(rx_pkts_burst[i])) {
			ret = get_and_verify_incomplete_frags(rx_pkts_burst[i],
							      vector);
			if (ret != TEST_SUCCESS)
				break;
			continue;
		}

		if (rx_pkts_burst[i]->ol_flags &
		    RTE_MBUF_F_RX_SEC_OFFLOAD_FAILED ||
		    !(rx_pkts_burst[i]->ol_flags & RTE_MBUF_F_RX_SEC_OFFLOAD)) {
			printf("\nsecurity offload failed\n");
			ret = TEST_FAILED;
			break;
		}

		if (vector->full_pkt->len + RTE_ETHER_HDR_LEN !=
				rx_pkts_burst[i]->pkt_len) {
			printf("\nreassembled/decrypted packet length mismatch\n");
			ret = TEST_FAILED;
			break;
		}
		rte_pktmbuf_adj(rx_pkts_burst[i], RTE_ETHER_HDR_LEN);
		ret = compare_pkt_data(rx_pkts_burst[i],
				       vector->full_pkt->data,
				       vector->full_pkt->len);
		if (ret != TEST_SUCCESS)
			break;
	}

out:
	destroy_default_flow(port_id);

	/* Clear session data. */
	for (i = 0; i < burst_sz; i++) {
		if (out_ses[i])
			rte_security_session_destroy(ctx, out_ses[i]);
		if (in_ses[i])
			rte_security_session_destroy(ctx, in_ses[i]);
	}

	for (i = nb_sent; i < nb_tx; i++)
		free_mbuf(tx_pkts_burst[i]);
	for (i = 0; i < nb_rx; i++)
		free_mbuf(rx_pkts_burst[i]);
	return ret;
}

static int
test_ipsec_inline_sa_exp_event_callback(uint16_t port_id,
		enum rte_eth_event_type type, void *param, void *ret_param)
{
	struct sa_expiry_vector *vector = (struct sa_expiry_vector *)param;
	struct rte_eth_event_ipsec_desc *event_desc = NULL;

	RTE_SET_USED(port_id);

	if (type != RTE_ETH_EVENT_IPSEC)
		return -1;

	event_desc = ret_param;
	if (event_desc == NULL) {
		printf("Event descriptor not set\n");
		return -1;
	}
	vector->notify_event = true;
	if (event_desc->metadata != (uint64_t)vector->sa_data) {
		printf("Mismatch in event specific metadata\n");
		return -1;
	}
	switch (event_desc->subtype) {
	case RTE_ETH_EVENT_IPSEC_SA_PKT_EXPIRY:
		vector->event = RTE_ETH_EVENT_IPSEC_SA_PKT_EXPIRY;
		break;
	case RTE_ETH_EVENT_IPSEC_SA_BYTE_EXPIRY:
		vector->event = RTE_ETH_EVENT_IPSEC_SA_BYTE_EXPIRY;
		break;
	case RTE_ETH_EVENT_IPSEC_SA_PKT_HARD_EXPIRY:
		vector->event = RTE_ETH_EVENT_IPSEC_SA_PKT_HARD_EXPIRY;
		break;
	case RTE_ETH_EVENT_IPSEC_SA_BYTE_HARD_EXPIRY:
		vector->event = RTE_ETH_EVENT_IPSEC_SA_BYTE_HARD_EXPIRY;
		break;
	default:
		printf("Invalid IPsec event reported\n");
		return -1;
	}

	return 0;
}

static enum rte_eth_event_ipsec_subtype
test_ipsec_inline_setup_expiry_vector(struct sa_expiry_vector *vector,
		const struct ipsec_test_flags *flags,
		struct ipsec_test_data *tdata)
{
	enum rte_eth_event_ipsec_subtype event = RTE_ETH_EVENT_IPSEC_UNKNOWN;

	vector->event = RTE_ETH_EVENT_IPSEC_UNKNOWN;
	vector->notify_event = false;
	vector->sa_data = (void *)tdata;
	if (flags->sa_expiry_pkts_soft)
		event = RTE_ETH_EVENT_IPSEC_SA_PKT_EXPIRY;
	else if (flags->sa_expiry_bytes_soft)
		event = RTE_ETH_EVENT_IPSEC_SA_BYTE_EXPIRY;
	else if (flags->sa_expiry_pkts_hard)
		event = RTE_ETH_EVENT_IPSEC_SA_PKT_HARD_EXPIRY;
	else
		event = RTE_ETH_EVENT_IPSEC_SA_BYTE_HARD_EXPIRY;
	rte_eth_dev_callback_register(port_id, RTE_ETH_EVENT_IPSEC,
		       test_ipsec_inline_sa_exp_event_callback, vector);

	return event;
}

static int
test_ipsec_inline_proto_process(struct ipsec_test_data *td,
		struct ipsec_test_data *res_d,
		int nb_pkts,
		bool silent,
		const struct ipsec_test_flags *flags)
{
	enum rte_eth_event_ipsec_subtype event = RTE_ETH_EVENT_IPSEC_UNKNOWN;
	struct rte_security_session_conf sess_conf = {0};
	struct rte_crypto_sym_xform cipher = {0};
	struct rte_crypto_sym_xform auth = {0};
	struct rte_crypto_sym_xform aead = {0};
	struct sa_expiry_vector vector = {0};
	void *ctx;
	int nb_rx = 0, nb_sent;
	uint32_t ol_flags;
	int i, j = 0, ret;
	bool outer_ipv4;
	void *ses;

	memset(rx_pkts_burst, 0, sizeof(rx_pkts_burst[0]) * nb_pkts);

	if (flags->sa_expiry_pkts_soft || flags->sa_expiry_bytes_soft ||
		flags->sa_expiry_pkts_hard || flags->sa_expiry_bytes_hard) {
		if (td->ipsec_xform.direction == RTE_SECURITY_IPSEC_SA_DIR_INGRESS)
			return TEST_SUCCESS;
		event = test_ipsec_inline_setup_expiry_vector(&vector, flags, td);
	}

	if (td->aead) {
		sess_conf.crypto_xform = &aead;
	} else {
		if (td->ipsec_xform.direction ==
				RTE_SECURITY_IPSEC_SA_DIR_EGRESS) {
			sess_conf.crypto_xform = &cipher;
			sess_conf.crypto_xform->type = RTE_CRYPTO_SYM_XFORM_CIPHER;
			sess_conf.crypto_xform->next = &auth;
			sess_conf.crypto_xform->next->type = RTE_CRYPTO_SYM_XFORM_AUTH;
		} else {
			sess_conf.crypto_xform = &auth;
			sess_conf.crypto_xform->type = RTE_CRYPTO_SYM_XFORM_AUTH;
			sess_conf.crypto_xform->next = &cipher;
			sess_conf.crypto_xform->next->type = RTE_CRYPTO_SYM_XFORM_CIPHER;
		}
	}

	/* Create Inline IPsec session. */
	ret = create_inline_ipsec_session(td, port_id, &ses, &ctx,
					  &ol_flags, flags, &sess_conf);
	if (ret)
		return ret;

	if (flags->inb_oop && rte_security_oop_dynfield_offset < 0) {
		printf("\nDynamic field not available for inline inbound OOP");
		ret = TEST_FAILED;
		goto out;
	}

	if (td->ipsec_xform.direction == RTE_SECURITY_IPSEC_SA_DIR_INGRESS) {
		ret = create_default_flow(port_id);
		if (ret)
			goto out;
	}
	outer_ipv4 = is_outer_ipv4(td);

	for (i = 0; i < nb_pkts; i++) {
		tx_pkts_burst[i] = init_packet(mbufpool, td->input_text.data,
						td->input_text.len, outer_ipv4);
		if (tx_pkts_burst[i] == NULL) {
			while (i--)
				rte_pktmbuf_free(tx_pkts_burst[i]);
			ret = TEST_FAILED;
			goto out;
		}

		if (test_ipsec_pkt_update(rte_pktmbuf_mtod_offset(tx_pkts_burst[i],
					uint8_t *, RTE_ETHER_HDR_LEN), flags)) {
			while (i--)
				rte_pktmbuf_free(tx_pkts_burst[i]);
			ret = TEST_FAILED;
			goto out;
		}

		if (td->ipsec_xform.direction == RTE_SECURITY_IPSEC_SA_DIR_EGRESS) {
			if (ol_flags & RTE_SECURITY_TX_OLOAD_NEED_MDATA)
				rte_security_set_pkt_metadata(ctx, ses,
						tx_pkts_burst[i], NULL);
			tx_pkts_burst[i]->ol_flags |= RTE_MBUF_F_TX_SEC_OFFLOAD;
		}
	}
	/* Send packet to ethdev for inline IPsec processing. */
	if (event_mode_enabled)
		nb_sent = event_tx_burst(tx_pkts_burst, nb_pkts);
	else
		nb_sent = rte_eth_tx_burst(port_id, 0, tx_pkts_burst, nb_pkts);

	if (nb_sent != nb_pkts) {
		printf("\nUnable to TX %d packets, sent: %i", nb_pkts, nb_sent);
		for ( ; nb_sent < nb_pkts; nb_sent++)
			rte_pktmbuf_free(tx_pkts_burst[nb_sent]);
		ret = TEST_FAILED;
		goto out;
	}

	rte_pause();

	/* Receive back packet on loopback interface. */
	if (event_mode_enabled)
		nb_rx = event_rx_burst(rx_pkts_burst, nb_sent);
	else
		do {
			rte_delay_ms(1);
			nb_rx += rte_eth_rx_burst(port_id, 0,
					&rx_pkts_burst[nb_rx],
					nb_sent - nb_rx);
			if (nb_rx >= nb_sent)
				break;
		} while (j++ < 5 || nb_rx == 0);

	if (!flags->sa_expiry_pkts_hard &&
			!flags->sa_expiry_bytes_hard &&
			(nb_rx != nb_sent)) {
		printf("\nUnable to RX all %d packets, received(%i)",
				nb_sent, nb_rx);
		while (--nb_rx >= 0)
			rte_pktmbuf_free(rx_pkts_burst[nb_rx]);
		ret = TEST_FAILED;
		goto out;
	}

	for (i = 0; i < nb_rx; i++) {
		rte_pktmbuf_adj(rx_pkts_burst[i], RTE_ETHER_HDR_LEN);

		ret = test_ipsec_post_process(rx_pkts_burst[i], td,
					      res_d, silent, flags);
		if (ret != TEST_SUCCESS) {
			for ( ; i < nb_rx; i++)
				rte_pktmbuf_free(rx_pkts_burst[i]);
			goto out;
		}

		ret = test_ipsec_stats_verify(ctx, ses, flags,
					td->ipsec_xform.direction);
		if (ret != TEST_SUCCESS) {
			for ( ; i < nb_rx; i++)
				rte_pktmbuf_free(rx_pkts_burst[i]);
			goto out;
		}

		if (flags->inb_oop) {
			ret = verify_inbound_oop(td, silent, rx_pkts_burst[i]);
			if (ret != TEST_SUCCESS) {
				for ( ; i < nb_rx; i++)
					rte_pktmbuf_free(rx_pkts_burst[i]);
				goto out;
			}
		}

		rte_pktmbuf_free(rx_pkts_burst[i]);
		rx_pkts_burst[i] = NULL;
	}

out:
	if (td->ipsec_xform.direction == RTE_SECURITY_IPSEC_SA_DIR_INGRESS)
		destroy_default_flow(port_id);
	if (flags->sa_expiry_pkts_soft || flags->sa_expiry_bytes_soft ||
		flags->sa_expiry_pkts_hard || flags->sa_expiry_bytes_hard) {
		if (vector.notify_event && (vector.event == event))
			ret = TEST_SUCCESS;
		else
			ret = TEST_FAILED;

		rte_eth_dev_callback_unregister(port_id, RTE_ETH_EVENT_IPSEC,
			test_ipsec_inline_sa_exp_event_callback, &vector);
	}

	/* Destroy session so that other cases can create the session again */
	rte_security_session_destroy(ctx, ses);
	ses = NULL;

	return ret;
}

static int
test_ipsec_inline_proto_all(const struct ipsec_test_flags *flags)
{
	struct ipsec_test_data td_outb;
	struct ipsec_test_data td_inb;
	unsigned int i, nb_pkts = 1, pass_cnt = 0, fail_cnt = 0;
	int ret;

	if (flags->iv_gen || flags->sa_expiry_pkts_soft ||
			flags->sa_expiry_bytes_soft ||
			flags->sa_expiry_bytes_hard ||
			flags->sa_expiry_pkts_hard)
		nb_pkts = IPSEC_TEST_PACKETS_MAX;

	for (i = 0; i < RTE_DIM(alg_list); i++) {
		test_ipsec_td_prepare(alg_list[i].param1,
				      alg_list[i].param2,
				      flags, &td_outb, 1);

		if (!td_outb.aead) {
			enum rte_crypto_cipher_algorithm cipher_alg;
			enum rte_crypto_auth_algorithm auth_alg;

			cipher_alg = td_outb.xform.chain.cipher.cipher.algo;
			auth_alg = td_outb.xform.chain.auth.auth.algo;

			if (td_outb.aes_gmac && cipher_alg != RTE_CRYPTO_CIPHER_NULL)
				continue;

			/* ICV is not applicable for NULL auth */
			if (flags->icv_corrupt &&
			    auth_alg == RTE_CRYPTO_AUTH_NULL)
				continue;

			/* IV is not applicable for NULL cipher */
			if (flags->iv_gen &&
			    cipher_alg == RTE_CRYPTO_CIPHER_NULL)
				continue;
		}

		if (flags->udp_encap)
			td_outb.ipsec_xform.options.udp_encap = 1;

		if (flags->sa_expiry_bytes_soft)
			td_outb.ipsec_xform.life.bytes_soft_limit =
				(((td_outb.output_text.len + RTE_ETHER_HDR_LEN)
				  * nb_pkts) >> 3) - 1;
		if (flags->sa_expiry_pkts_hard)
			td_outb.ipsec_xform.life.packets_hard_limit =
					IPSEC_TEST_PACKETS_MAX - 1;
		if (flags->sa_expiry_bytes_hard)
			td_outb.ipsec_xform.life.bytes_hard_limit =
				(((td_outb.output_text.len + RTE_ETHER_HDR_LEN)
				  * nb_pkts) >> 3) - 1;

		ret = test_ipsec_inline_proto_process(&td_outb, &td_inb, nb_pkts,
						false, flags);
		if (ret == TEST_SKIPPED)
			continue;

		if (ret == TEST_FAILED) {
			printf("\n TEST FAILED");
			test_ipsec_display_alg(alg_list[i].param1,
					       alg_list[i].param2);
			fail_cnt++;
			continue;
		}

		test_ipsec_td_update(&td_inb, &td_outb, 1, flags);

		ret = test_ipsec_inline_proto_process(&td_inb, NULL, nb_pkts,
						false, flags);
		if (ret == TEST_SKIPPED)
			continue;

		if (ret == TEST_FAILED) {
			printf("\n TEST FAILED");
			test_ipsec_display_alg(alg_list[i].param1,
					       alg_list[i].param2);
			fail_cnt++;
			continue;
		}

		if (flags->display_alg)
			test_ipsec_display_alg(alg_list[i].param1,
					       alg_list[i].param2);

		pass_cnt++;
	}

	printf("Tests passed: %d, failed: %d", pass_cnt, fail_cnt);
	if (fail_cnt > 0)
		return TEST_FAILED;
	if (pass_cnt > 0)
		return TEST_SUCCESS;
	else
		return TEST_SKIPPED;
}

static int
test_ipsec_inline_proto_process_with_esn(struct ipsec_test_data td[],
		struct ipsec_test_data res_d[],
		int nb_pkts,
		bool silent,
		const struct ipsec_test_flags *flags)
{
	struct rte_security_session_conf sess_conf = {0};
	struct ipsec_test_data *res_d_tmp = NULL;
	struct rte_crypto_sym_xform cipher = {0};
	struct rte_crypto_sym_xform auth = {0};
	struct rte_crypto_sym_xform aead = {0};
	struct rte_mbuf *rx_pkt = NULL;
	struct rte_mbuf *tx_pkt = NULL;
	int nb_rx, nb_sent;
	void *ses;
	void *ctx;
	uint32_t ol_flags;
	bool outer_ipv4;
	int i, ret;

	if (td[0].aead) {
		sess_conf.crypto_xform = &aead;
	} else {
		if (td[0].ipsec_xform.direction ==
				RTE_SECURITY_IPSEC_SA_DIR_EGRESS) {
			sess_conf.crypto_xform = &cipher;
			sess_conf.crypto_xform->type = RTE_CRYPTO_SYM_XFORM_CIPHER;
			sess_conf.crypto_xform->next = &auth;
			sess_conf.crypto_xform->next->type = RTE_CRYPTO_SYM_XFORM_AUTH;
		} else {
			sess_conf.crypto_xform = &auth;
			sess_conf.crypto_xform->type = RTE_CRYPTO_SYM_XFORM_AUTH;
			sess_conf.crypto_xform->next = &cipher;
			sess_conf.crypto_xform->next->type = RTE_CRYPTO_SYM_XFORM_CIPHER;
		}
	}

	/* Create Inline IPsec session. */
	ret = create_inline_ipsec_session(&td[0], port_id, &ses, &ctx,
					  &ol_flags, flags, &sess_conf);
	if (ret)
		return ret;

	if (td[0].ipsec_xform.direction == RTE_SECURITY_IPSEC_SA_DIR_INGRESS) {
		ret = create_default_flow(port_id);
		if (ret)
			goto out;
	}
	outer_ipv4 = is_outer_ipv4(td);

	for (i = 0; i < nb_pkts; i++) {
		tx_pkt = init_packet(mbufpool, td[i].input_text.data,
					td[i].input_text.len, outer_ipv4);
		if (tx_pkt == NULL) {
			ret = TEST_FAILED;
			goto out;
		}

		if (test_ipsec_pkt_update(rte_pktmbuf_mtod_offset(tx_pkt,
					uint8_t *, RTE_ETHER_HDR_LEN), flags)) {
			ret = TEST_FAILED;
			goto out;
		}

		if (td[i].ipsec_xform.direction ==
				RTE_SECURITY_IPSEC_SA_DIR_EGRESS) {
			if (flags->antireplay) {
				sess_conf.ipsec.esn.value =
						td[i].ipsec_xform.esn.value;
				ret = rte_security_session_update(ctx, ses,
						&sess_conf);
				if (ret) {
					printf("Could not update ESN in session\n");
					rte_pktmbuf_free(tx_pkt);
					ret = TEST_SKIPPED;
					goto out;
				}
			}
			if (ol_flags & RTE_SECURITY_TX_OLOAD_NEED_MDATA)
				rte_security_set_pkt_metadata(ctx, ses,
						tx_pkt, NULL);
			tx_pkt->ol_flags |= RTE_MBUF_F_TX_SEC_OFFLOAD;
		}

		/* Send packet to ethdev for inline IPsec processing. */
		if (event_mode_enabled)
			nb_sent = event_tx_burst(&tx_pkt, 1);
		else
			nb_sent = rte_eth_tx_burst(port_id, 0, &tx_pkt, 1);

		if (nb_sent != 1) {
			printf("\nUnable to TX packets");
			rte_pktmbuf_free(tx_pkt);
			ret = TEST_FAILED;
			goto out;
		}

		rte_pause();

		/* Receive back packet on loopback interface. */
		if (event_mode_enabled)
			nb_rx = event_rx_burst(&rx_pkt, nb_sent);
		else {
			do {
				rte_delay_ms(1);
				nb_rx = rte_eth_rx_burst(port_id, 0, &rx_pkt, 1);
			} while (nb_rx == 0);
		}
		rte_pktmbuf_adj(rx_pkt, RTE_ETHER_HDR_LEN);

		if (res_d != NULL)
			res_d_tmp = &res_d[i];

		ret = test_ipsec_post_process(rx_pkt, &td[i],
					      res_d_tmp, silent, flags);
		if (ret != TEST_SUCCESS) {
			rte_pktmbuf_free(rx_pkt);
			goto out;
		}

		ret = test_ipsec_stats_verify(ctx, ses, flags,
					td->ipsec_xform.direction);
		if (ret != TEST_SUCCESS) {
			rte_pktmbuf_free(rx_pkt);
			goto out;
		}

		rte_pktmbuf_free(rx_pkt);
		rx_pkt = NULL;
		tx_pkt = NULL;
	}

out:
	if (td->ipsec_xform.direction == RTE_SECURITY_IPSEC_SA_DIR_INGRESS)
		destroy_default_flow(port_id);

	/* Destroy session so that other cases can create the session again */
	rte_security_session_destroy(ctx, ses);
	ses = NULL;

	return ret;
}

static int
ut_setup_inline_ipsec_reassembly(void)
{
	struct rte_eth_ip_reassembly_params reass_capa = {0};
	int ret;

	rte_eth_ip_reassembly_capability_get(port_id, &reass_capa);
	if (reass_capa.timeout_ms > APP_REASS_TIMEOUT) {
		reass_capa.timeout_ms = APP_REASS_TIMEOUT;
		rte_eth_ip_reassembly_conf_set(port_id, &reass_capa);
	}

	/* Start event devices */
	if (event_mode_enabled) {
		ret = rte_event_eth_rx_adapter_start(rx_adapter_id);
		if (ret < 0) {
			printf("Failed to start rx adapter %d\n", ret);
			return ret;
		}

		ret = rte_event_dev_start(eventdev_id);
		if (ret < 0) {
			printf("Failed to start event device %d\n", ret);
			return ret;
		}
	}

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
ut_teardown_inline_ipsec_reassembly(void)
{
	struct rte_eth_ip_reassembly_params reass_conf = {0};
	uint16_t portid;
	int ret;

	/* Stop event devices */
	if (event_mode_enabled)
		rte_event_dev_stop(eventdev_id);

	/* port tear down */
	RTE_ETH_FOREACH_DEV(portid) {
		ret = rte_eth_dev_stop(portid);
		if (ret != 0)
			printf("rte_eth_dev_stop: err=%s, port=%u\n",
			       rte_strerror(-ret), portid);

		/* Clear reassembly configuration */
		rte_eth_ip_reassembly_conf_set(portid, &reass_conf);
	}
}
static int
ut_setup_inline_ipsec(void)
{
	int ret;

	/* Start event devices */
	if (event_mode_enabled) {
		ret = rte_event_dev_start(eventdev_id);
		if (ret < 0) {
			printf("Failed to start event device %d\n", ret);
			return ret;
		}
	}

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
ut_teardown_inline_ipsec(void)
{
	uint16_t portid;
	int ret;

	/* Stop event devices */
	if (event_mode_enabled)
		rte_event_dev_stop(eventdev_id);

	/* port tear down */
	RTE_ETH_FOREACH_DEV(portid) {
		ret = rte_eth_dev_stop(portid);
		if (ret != 0)
			printf("rte_eth_dev_stop: err=%s, port=%u\n",
			       rte_strerror(-ret), portid);
	}
}

static int
inline_ipsec_testsuite_setup(void)
{
	struct rte_eth_conf local_port_conf;
	struct rte_eth_dev_info dev_info;
	uint16_t nb_rxd;
	uint16_t nb_txd;
	uint16_t nb_ports;
	int ret;
	uint16_t nb_rx_queue = 1, nb_tx_queue = 1;

	printf("Start inline IPsec test.\n");

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

	nb_rxd = RX_DESC_DEFAULT;
	nb_txd = TX_DESC_DEFAULT;

	/* configuring port 0 for the test is enough */
	port_id = 0;
	if (rte_eth_dev_info_get(0, &dev_info)) {
		printf("Failed to get devinfo");
		return -1;
	}

	memcpy(&local_port_conf, &port_conf, sizeof(port_conf));
	/* Add Multi seg flags */
	if (sg_mode) {
		uint16_t max_data_room = RTE_MBUF_DEFAULT_DATAROOM *
			dev_info.rx_desc_lim.nb_seg_max;

		local_port_conf.rxmode.offloads |= RTE_ETH_RX_OFFLOAD_SCATTER;
		local_port_conf.txmode.offloads |= RTE_ETH_TX_OFFLOAD_MULTI_SEGS;
		local_port_conf.rxmode.mtu = RTE_MIN(dev_info.max_mtu, max_data_room - 256);
	}

	/* port configure */
	ret = rte_eth_dev_configure(port_id, nb_rx_queue,
				    nb_tx_queue, &local_port_conf);
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
	test_ipsec_alg_list_populate();

	/* Change the plaintext size for tests without Known vectors */
	if (sg_mode) {
		/* Leave space of 256B as ESP packet would be bigger and we
		 * expect packets to be received back on same interface.
		 * Without SG mode, default value is picked.
		 */
		plaintext_len = local_port_conf.rxmode.mtu - 256;
	} else {
		plaintext_len = 0;
	}

	return 0;
}

static void
inline_ipsec_testsuite_teardown(void)
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

static int
event_inline_ipsec_testsuite_setup(void)
{
	struct rte_event_eth_rx_adapter_queue_conf queue_conf = {0};
	struct rte_event_dev_info evdev_default_conf = {0};
	struct rte_event_dev_config eventdev_conf = {0};
	struct rte_event_queue_conf eventq_conf = {0};
	struct rte_event_port_conf ev_port_conf = {0};
	const uint16_t nb_txd = 1024, nb_rxd = 1024;
	uint16_t nb_rx_queue = 1, nb_tx_queue = 1;
	uint8_t ev_queue_id = 0, tx_queue_id = 0;
	int nb_eventqueue = 1, nb_eventport = 1;
	const int all_queues = -1;
	uint32_t caps = 0;
	uint16_t nb_ports;
	int ret;

	printf("Start event inline IPsec test.\n");

	nb_ports = rte_eth_dev_count_avail();
	if (nb_ports == 0) {
		printf("Test require: 1 port, available: 0\n");
		return TEST_SKIPPED;
	}

	init_mempools(NB_MBUF);

	if (tx_pkts_burst == NULL) {
		tx_pkts_burst = (struct rte_mbuf **)rte_calloc("tx_buff",
					  MAX_TRAFFIC_BURST,
					  sizeof(void *),
					  RTE_CACHE_LINE_SIZE);
		if (!tx_pkts_burst)
			return -1;

		rx_pkts_burst = (struct rte_mbuf **)rte_calloc("rx_buff",
					  MAX_TRAFFIC_BURST,
					  sizeof(void *),
					  RTE_CACHE_LINE_SIZE);
		if (!rx_pkts_burst)
			return -1;

	}

	printf("Generate %d packets\n", MAX_TRAFFIC_BURST);

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

	/* Tx queue setup */
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

	/* Setup eventdev */
	eventdev_id = 0;
	rx_adapter_id = 0;
	tx_adapter_id = 0;

	/* Get default conf of eventdev */
	ret = rte_event_dev_info_get(eventdev_id, &evdev_default_conf);
	if (ret < 0) {
		printf("Error in getting event device info[devID:%d]\n",
				eventdev_id);
		return ret;
	}

	/* Get Tx adapter capabilities */
	ret = rte_event_eth_tx_adapter_caps_get(eventdev_id, tx_adapter_id, &caps);
	if (ret < 0) {
		printf("Failed to get event device %d eth tx adapter"
				" capabilities for port %d\n",
				eventdev_id, port_id);
		return ret;
	}
	if (!(caps & RTE_EVENT_ETH_TX_ADAPTER_CAP_INTERNAL_PORT))
		tx_queue_id = nb_eventqueue++;

	eventdev_conf.nb_events_limit =
			evdev_default_conf.max_num_events;
	eventdev_conf.nb_event_queue_flows =
			evdev_default_conf.max_event_queue_flows;
	eventdev_conf.nb_event_port_dequeue_depth =
			evdev_default_conf.max_event_port_dequeue_depth;
	eventdev_conf.nb_event_port_enqueue_depth =
			evdev_default_conf.max_event_port_enqueue_depth;

	eventdev_conf.nb_event_queues = nb_eventqueue;
	eventdev_conf.nb_event_ports = nb_eventport;

	/* Configure event device */

	ret = rte_event_dev_configure(eventdev_id, &eventdev_conf);
	if (ret < 0) {
		printf("Error in configuring event device\n");
		return ret;
	}

	/* Configure event queue */
	eventq_conf.schedule_type = RTE_SCHED_TYPE_PARALLEL;
	eventq_conf.nb_atomic_flows = 1024;
	eventq_conf.nb_atomic_order_sequences = 1024;

	/* Setup the queue */
	ret = rte_event_queue_setup(eventdev_id, ev_queue_id, &eventq_conf);
	if (ret < 0) {
		printf("Failed to setup event queue %d\n", ret);
		return ret;
	}

	/* Configure event port */
	ret = rte_event_port_setup(eventdev_id, port_id, NULL);
	if (ret < 0) {
		printf("Failed to setup event port %d\n", ret);
		return ret;
	}

	/* Make event queue - event port link */
	ret = rte_event_port_link(eventdev_id, port_id, NULL, NULL, 1);
	if (ret < 0) {
		printf("Failed to link event port %d\n", ret);
		return ret;
	}

	/* Setup port conf */
	ev_port_conf.new_event_threshold = 1200;
	ev_port_conf.dequeue_depth =
			evdev_default_conf.max_event_port_dequeue_depth;
	ev_port_conf.enqueue_depth =
			evdev_default_conf.max_event_port_enqueue_depth;

	/* Create Rx adapter */
	ret = rte_event_eth_rx_adapter_create(rx_adapter_id, eventdev_id,
			&ev_port_conf);
	if (ret < 0) {
		printf("Failed to create rx adapter %d\n", ret);
		return ret;
	}

	/* Setup queue conf */
	queue_conf.ev.queue_id = ev_queue_id;
	queue_conf.ev.sched_type = RTE_SCHED_TYPE_PARALLEL;
	queue_conf.ev.event_type = RTE_EVENT_TYPE_ETHDEV;

	/* Add queue to the adapter */
	ret = rte_event_eth_rx_adapter_queue_add(rx_adapter_id, port_id,
			all_queues, &queue_conf);
	if (ret < 0) {
		printf("Failed to add eth queue to rx adapter %d\n", ret);
		return ret;
	}

	/* Start rx adapter */
	ret = rte_event_eth_rx_adapter_start(rx_adapter_id);
	if (ret < 0) {
		printf("Failed to start rx adapter %d\n", ret);
		return ret;
	}

	/* Create tx adapter */
	ret = rte_event_eth_tx_adapter_create(tx_adapter_id, eventdev_id,
			&ev_port_conf);
	if (ret < 0) {
		printf("Failed to create tx adapter %d\n", ret);
		return ret;
	}

	/* Add queue to the adapter */
	ret = rte_event_eth_tx_adapter_queue_add(tx_adapter_id, port_id,
			all_queues);
	if (ret < 0) {
		printf("Failed to add eth queue to tx adapter %d\n", ret);
		return ret;
	}
	/* Setup Tx queue & port */
	if (tx_queue_id) {
		/* Setup the queue */
		ret = rte_event_queue_setup(eventdev_id, tx_queue_id,
				&eventq_conf);
		if (ret < 0) {
			printf("Failed to setup tx event queue %d\n", ret);
			return ret;
		}
		/* Link Tx event queue to Tx port */
		ret = rte_event_port_link(eventdev_id, port_id,
				&tx_queue_id, NULL, 1);
		if (ret != 1) {
			printf("Failed to link event queue to port\n");
			return ret;
		}
	}

	/* Start tx adapter */
	ret = rte_event_eth_tx_adapter_start(tx_adapter_id);
	if (ret < 0) {
		printf("Failed to start tx adapter %d\n", ret);
		return ret;
	}

	/* Start eventdev */
	ret = rte_event_dev_start(eventdev_id);
	if (ret < 0) {
		printf("Failed to start event device %d\n", ret);
		return ret;
	}

	event_mode_enabled = true;
	test_ipsec_alg_list_populate();

	return 0;
}

static void
event_inline_ipsec_testsuite_teardown(void)
{
	uint16_t portid;
	int ret;

	event_mode_enabled = false;

	/* Stop and release rx adapter */
	ret = rte_event_eth_rx_adapter_stop(rx_adapter_id);
	if (ret < 0)
		printf("Failed to stop rx adapter %d\n", ret);
	ret = rte_event_eth_rx_adapter_queue_del(rx_adapter_id, port_id, -1);
	if (ret < 0)
		printf("Failed to remove rx adapter queues %d\n", ret);
	ret = rte_event_eth_rx_adapter_free(rx_adapter_id);
	if (ret < 0)
		printf("Failed to free rx adapter %d\n", ret);

	/* Stop and release tx adapter */
	ret = rte_event_eth_tx_adapter_stop(tx_adapter_id);
	if (ret < 0)
		printf("Failed to stop tx adapter %d\n", ret);
	ret = rte_event_eth_tx_adapter_queue_del(tx_adapter_id, port_id, -1);
	if (ret < 0)
		printf("Failed to remove tx adapter queues %d\n", ret);
	ret = rte_event_eth_tx_adapter_free(tx_adapter_id);
	if (ret < 0)
		printf("Failed to free tx adapter %d\n", ret);

	/* Stop and release event devices */
	rte_event_dev_stop(eventdev_id);
	ret = rte_event_dev_close(eventdev_id);
	if (ret < 0)
		printf("Failed to close event dev %d, %d\n", eventdev_id, ret);

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

static int
test_inline_ip_reassembly(const void *testdata)
{
	struct reassembly_vector reassembly_td = {0};
	const struct reassembly_vector *td = testdata;
	struct ip_reassembly_test_packet full_pkt;
	struct ip_reassembly_test_packet frags[MAX_FRAGS];
	uint16_t extra_data, extra_data_sum = 0;
	struct ipsec_test_flags flags = {0};
	int i = 0;

	reassembly_td.sa_data = td->sa_data;
	reassembly_td.nb_frags = td->nb_frags;
	reassembly_td.burst = td->burst;

	memcpy(&full_pkt, td->full_pkt,
			sizeof(struct ip_reassembly_test_packet));
	reassembly_td.full_pkt = &full_pkt;

	for (; i < reassembly_td.nb_frags; i++) {
		memcpy(&frags[i], td->frags[i],
			sizeof(struct ip_reassembly_test_packet));
		reassembly_td.frags[i] = &frags[i];

		/* Add extra data for multi-seg test on all fragments except last one */
		extra_data = 0;
		if (plaintext_len && reassembly_td.frags[i]->len < plaintext_len &&
		    (i != reassembly_td.nb_frags - 1))
			extra_data = ((plaintext_len - reassembly_td.frags[i]->len) & ~0x7ULL);

		test_vector_payload_populate(reassembly_td.frags[i],
				(i == 0) ? true : false, extra_data, extra_data_sum);
		extra_data_sum += extra_data;
	}
	test_vector_payload_populate(reassembly_td.full_pkt, true, extra_data_sum, 0);

	return test_ipsec_with_reassembly(&reassembly_td, &flags);
}

static int
test_ipsec_inline_proto_known_vec(const void *test_data)
{
	struct ipsec_test_data td_outb;
	struct ipsec_test_flags flags;

	memset(&flags, 0, sizeof(flags));

	memcpy(&td_outb, test_data, sizeof(td_outb));

	if (td_outb.aead ||
	    td_outb.xform.chain.cipher.cipher.algo != RTE_CRYPTO_CIPHER_NULL) {
		/* Disable IV gen to be able to test with known vectors */
		td_outb.ipsec_xform.options.iv_gen_disable = 1;
	}

	return test_ipsec_inline_proto_process(&td_outb, NULL, 1,
				false, &flags);
}

static int
test_ipsec_inline_proto_known_vec_inb(const void *test_data)
{
	const struct ipsec_test_data *td = test_data;
	struct ipsec_test_flags flags;
	struct ipsec_test_data td_inb;

	memset(&flags, 0, sizeof(flags));

	if (td->ipsec_xform.direction == RTE_SECURITY_IPSEC_SA_DIR_EGRESS)
		test_ipsec_td_in_from_out(td, &td_inb);
	else
		memcpy(&td_inb, td, sizeof(td_inb));

	return test_ipsec_inline_proto_process(&td_inb, NULL, 1, false, &flags);
}

static int
test_ipsec_inline_proto_oop_inb(const void *test_data)
{
	const struct ipsec_test_data *td = test_data;
	struct ipsec_test_flags flags;
	struct ipsec_test_data td_inb;

	memset(&flags, 0, sizeof(flags));
	flags.inb_oop = true;

	if (td->ipsec_xform.direction == RTE_SECURITY_IPSEC_SA_DIR_EGRESS)
		test_ipsec_td_in_from_out(td, &td_inb);
	else
		memcpy(&td_inb, td, sizeof(td_inb));

	td_inb.ipsec_xform.options.ingress_oop = true;

	return test_ipsec_inline_proto_process(&td_inb, NULL, 1, false, &flags);
}

static int
test_ipsec_inline_proto_display_list(void)
{
	struct ipsec_test_flags flags;

	memset(&flags, 0, sizeof(flags));

	flags.display_alg = true;
	flags.plaintext_len = plaintext_len;

	return test_ipsec_inline_proto_all(&flags);
}

static int
test_ipsec_inline_proto_udp_encap(void)
{
	struct ipsec_test_flags flags;

	memset(&flags, 0, sizeof(flags));

	flags.udp_encap = true;
	flags.plaintext_len = plaintext_len;

	return test_ipsec_inline_proto_all(&flags);
}

static int
test_ipsec_inline_proto_udp_ports_verify(void)
{
	struct ipsec_test_flags flags;

	memset(&flags, 0, sizeof(flags));

	flags.udp_encap = true;
	flags.udp_ports_verify = true;
	flags.plaintext_len = plaintext_len;

	return test_ipsec_inline_proto_all(&flags);
}

static int
test_ipsec_inline_proto_err_icv_corrupt(void)
{
	struct ipsec_test_flags flags;

	memset(&flags, 0, sizeof(flags));

	flags.icv_corrupt = true;
	flags.plaintext_len = plaintext_len;

	return test_ipsec_inline_proto_all(&flags);
}

static int
test_ipsec_inline_proto_tunnel_dst_addr_verify(void)
{
	struct ipsec_test_flags flags;

	memset(&flags, 0, sizeof(flags));

	flags.tunnel_hdr_verify = RTE_SECURITY_IPSEC_TUNNEL_VERIFY_DST_ADDR;
	flags.plaintext_len = plaintext_len;

	return test_ipsec_inline_proto_all(&flags);
}

static int
test_ipsec_inline_proto_tunnel_src_dst_addr_verify(void)
{
	struct ipsec_test_flags flags;

	memset(&flags, 0, sizeof(flags));

	flags.tunnel_hdr_verify = RTE_SECURITY_IPSEC_TUNNEL_VERIFY_SRC_DST_ADDR;
	flags.plaintext_len = plaintext_len;

	return test_ipsec_inline_proto_all(&flags);
}

static int
test_ipsec_inline_proto_inner_ip_csum(void)
{
	struct ipsec_test_flags flags;

	memset(&flags, 0, sizeof(flags));

	flags.ip_csum = true;
	flags.plaintext_len = plaintext_len;

	return test_ipsec_inline_proto_all(&flags);
}

static int
test_ipsec_inline_proto_inner_l4_csum(void)
{
	struct ipsec_test_flags flags;

	memset(&flags, 0, sizeof(flags));

	flags.l4_csum = true;
	flags.plaintext_len = plaintext_len;

	return test_ipsec_inline_proto_all(&flags);
}

static int
test_ipsec_inline_proto_tunnel_v4_in_v4(void)
{
	struct ipsec_test_flags flags;

	memset(&flags, 0, sizeof(flags));

	flags.ipv6 = false;
	flags.tunnel_ipv6 = false;
	flags.plaintext_len = plaintext_len;

	return test_ipsec_inline_proto_all(&flags);
}

static int
test_ipsec_inline_proto_tunnel_v6_in_v6(void)
{
	struct ipsec_test_flags flags;

	memset(&flags, 0, sizeof(flags));

	flags.ipv6 = true;
	flags.tunnel_ipv6 = true;
	flags.plaintext_len = plaintext_len;

	return test_ipsec_inline_proto_all(&flags);
}

static int
test_ipsec_inline_proto_tunnel_v4_in_v6(void)
{
	struct ipsec_test_flags flags;

	memset(&flags, 0, sizeof(flags));

	flags.ipv6 = false;
	flags.tunnel_ipv6 = true;
	flags.plaintext_len = plaintext_len;

	return test_ipsec_inline_proto_all(&flags);
}

static int
test_ipsec_inline_proto_tunnel_v6_in_v4(void)
{
	struct ipsec_test_flags flags;

	memset(&flags, 0, sizeof(flags));

	flags.ipv6 = true;
	flags.tunnel_ipv6 = false;
	flags.plaintext_len = plaintext_len;

	return test_ipsec_inline_proto_all(&flags);
}

static int
test_ipsec_inline_proto_transport_v4(void)
{
	struct ipsec_test_flags flags;

	memset(&flags, 0, sizeof(flags));

	flags.ipv6 = false;
	flags.transport = true;
	flags.plaintext_len = plaintext_len;

	return test_ipsec_inline_proto_all(&flags);
}

static int
test_ipsec_inline_proto_transport_l4_csum(void)
{
	struct ipsec_test_flags flags = {
		.l4_csum = true,
		.transport = true,
		.plaintext_len = plaintext_len,
	};

	return test_ipsec_inline_proto_all(&flags);
}

static int
test_ipsec_inline_proto_stats(void)
{
	struct ipsec_test_flags flags;

	memset(&flags, 0, sizeof(flags));

	flags.stats_success = true;
	flags.plaintext_len = plaintext_len;

	return test_ipsec_inline_proto_all(&flags);
}

static int
test_ipsec_inline_proto_pkt_fragment(void)
{
	struct ipsec_test_flags flags;

	memset(&flags, 0, sizeof(flags));

	flags.fragment = true;
	flags.plaintext_len = plaintext_len;

	return test_ipsec_inline_proto_all(&flags);

}

static int
test_ipsec_inline_proto_copy_df_inner_0(void)
{
	struct ipsec_test_flags flags;

	memset(&flags, 0, sizeof(flags));

	flags.df = TEST_IPSEC_COPY_DF_INNER_0;
	flags.plaintext_len = plaintext_len;

	return test_ipsec_inline_proto_all(&flags);
}

static int
test_ipsec_inline_proto_copy_df_inner_1(void)
{
	struct ipsec_test_flags flags;

	memset(&flags, 0, sizeof(flags));

	flags.df = TEST_IPSEC_COPY_DF_INNER_1;
	flags.plaintext_len = plaintext_len;

	return test_ipsec_inline_proto_all(&flags);
}

static int
test_ipsec_inline_proto_set_df_0_inner_1(void)
{
	struct ipsec_test_flags flags;

	memset(&flags, 0, sizeof(flags));

	flags.df = TEST_IPSEC_SET_DF_0_INNER_1;
	flags.plaintext_len = plaintext_len;

	return test_ipsec_inline_proto_all(&flags);
}

static int
test_ipsec_inline_proto_set_df_1_inner_0(void)
{
	struct ipsec_test_flags flags;

	memset(&flags, 0, sizeof(flags));

	flags.df = TEST_IPSEC_SET_DF_1_INNER_0;
	flags.plaintext_len = plaintext_len;

	return test_ipsec_inline_proto_all(&flags);
}

static int
test_ipsec_inline_proto_ipv4_copy_dscp_inner_0(void)
{
	struct ipsec_test_flags flags;

	memset(&flags, 0, sizeof(flags));

	flags.dscp = TEST_IPSEC_COPY_DSCP_INNER_0;
	flags.plaintext_len = plaintext_len;

	return test_ipsec_inline_proto_all(&flags);
}

static int
test_ipsec_inline_proto_ipv4_copy_dscp_inner_1(void)
{
	struct ipsec_test_flags flags;

	memset(&flags, 0, sizeof(flags));

	flags.dscp = TEST_IPSEC_COPY_DSCP_INNER_1;
	flags.plaintext_len = plaintext_len;

	return test_ipsec_inline_proto_all(&flags);
}

static int
test_ipsec_inline_proto_ipv4_set_dscp_0_inner_1(void)
{
	struct ipsec_test_flags flags;

	memset(&flags, 0, sizeof(flags));

	flags.dscp = TEST_IPSEC_SET_DSCP_0_INNER_1;
	flags.plaintext_len = plaintext_len;

	return test_ipsec_inline_proto_all(&flags);
}

static int
test_ipsec_inline_proto_ipv4_set_dscp_1_inner_0(void)
{
	struct ipsec_test_flags flags;

	memset(&flags, 0, sizeof(flags));

	flags.dscp = TEST_IPSEC_SET_DSCP_1_INNER_0;
	flags.plaintext_len = plaintext_len;

	return test_ipsec_inline_proto_all(&flags);
}

static int
test_ipsec_inline_proto_ipv6_copy_dscp_inner_0(void)
{
	struct ipsec_test_flags flags;

	memset(&flags, 0, sizeof(flags));

	flags.ipv6 = true;
	flags.tunnel_ipv6 = true;
	flags.dscp = TEST_IPSEC_COPY_DSCP_INNER_0;
	flags.plaintext_len = plaintext_len;

	return test_ipsec_inline_proto_all(&flags);
}

static int
test_ipsec_inline_proto_ipv6_copy_dscp_inner_1(void)
{
	struct ipsec_test_flags flags;

	memset(&flags, 0, sizeof(flags));

	flags.ipv6 = true;
	flags.tunnel_ipv6 = true;
	flags.dscp = TEST_IPSEC_COPY_DSCP_INNER_1;
	flags.plaintext_len = plaintext_len;

	return test_ipsec_inline_proto_all(&flags);
}

static int
test_ipsec_inline_proto_ipv6_set_dscp_0_inner_1(void)
{
	struct ipsec_test_flags flags;

	memset(&flags, 0, sizeof(flags));

	flags.ipv6 = true;
	flags.tunnel_ipv6 = true;
	flags.dscp = TEST_IPSEC_SET_DSCP_0_INNER_1;
	flags.plaintext_len = plaintext_len;

	return test_ipsec_inline_proto_all(&flags);
}

static int
test_ipsec_inline_proto_ipv6_set_dscp_1_inner_0(void)
{
	struct ipsec_test_flags flags;

	memset(&flags, 0, sizeof(flags));

	flags.ipv6 = true;
	flags.tunnel_ipv6 = true;
	flags.dscp = TEST_IPSEC_SET_DSCP_1_INNER_0;
	flags.plaintext_len = plaintext_len;

	return test_ipsec_inline_proto_all(&flags);
}

static int
test_ipsec_inline_proto_ipv6_copy_flabel_inner_0(void)
{
	struct ipsec_test_flags flags;

	memset(&flags, 0, sizeof(flags));

	flags.ipv6 = true;
	flags.tunnel_ipv6 = true;
	flags.flabel = TEST_IPSEC_COPY_FLABEL_INNER_0;

	return test_ipsec_inline_proto_all(&flags);
}

static int
test_ipsec_inline_proto_ipv6_copy_flabel_inner_1(void)
{
	struct ipsec_test_flags flags;

	memset(&flags, 0, sizeof(flags));

	flags.ipv6 = true;
	flags.tunnel_ipv6 = true;
	flags.flabel = TEST_IPSEC_COPY_FLABEL_INNER_1;

	return test_ipsec_inline_proto_all(&flags);
}

static int
test_ipsec_inline_proto_ipv6_set_flabel_0_inner_1(void)
{
	struct ipsec_test_flags flags;

	memset(&flags, 0, sizeof(flags));

	flags.ipv6 = true;
	flags.tunnel_ipv6 = true;
	flags.flabel = TEST_IPSEC_SET_FLABEL_0_INNER_1;

	return test_ipsec_inline_proto_all(&flags);
}

static int
test_ipsec_inline_proto_ipv6_set_flabel_1_inner_0(void)
{
	struct ipsec_test_flags flags;

	memset(&flags, 0, sizeof(flags));

	flags.ipv6 = true;
	flags.tunnel_ipv6 = true;
	flags.flabel = TEST_IPSEC_SET_FLABEL_1_INNER_0;

	return test_ipsec_inline_proto_all(&flags);
}

static int
test_ipsec_inline_proto_ipv4_ttl_decrement(void)
{
	struct ipsec_test_flags flags = {
		.dec_ttl_or_hop_limit = true,
		.plaintext_len = plaintext_len,
	};

	return test_ipsec_inline_proto_all(&flags);
}

static int
test_ipsec_inline_proto_ipv6_hop_limit_decrement(void)
{
	struct ipsec_test_flags flags = {
		.ipv6 = true,
		.dec_ttl_or_hop_limit = true,
		.plaintext_len = plaintext_len,
	};

	return test_ipsec_inline_proto_all(&flags);
}

static int
test_ipsec_inline_proto_iv_gen(void)
{
	struct ipsec_test_flags flags;

	memset(&flags, 0, sizeof(flags));

	flags.iv_gen = true;
	flags.plaintext_len = plaintext_len;

	return test_ipsec_inline_proto_all(&flags);
}

static int
test_ipsec_inline_proto_sa_pkt_soft_expiry(void)
{
	struct ipsec_test_flags flags = {
		.sa_expiry_pkts_soft = true,
		.plaintext_len = plaintext_len,
	};
	return test_ipsec_inline_proto_all(&flags);
}
static int
test_ipsec_inline_proto_sa_byte_soft_expiry(void)
{
	struct ipsec_test_flags flags = {
		.sa_expiry_bytes_soft = true,
		.plaintext_len = plaintext_len,
	};
	return test_ipsec_inline_proto_all(&flags);
}

static int
test_ipsec_inline_proto_sa_pkt_hard_expiry(void)
{
	struct ipsec_test_flags flags = {
		.sa_expiry_pkts_hard = true
	};

	return test_ipsec_inline_proto_all(&flags);
}

static int
test_ipsec_inline_proto_sa_byte_hard_expiry(void)
{
	struct ipsec_test_flags flags = {
		.sa_expiry_bytes_hard = true
	};

	return test_ipsec_inline_proto_all(&flags);
}

static int
test_ipsec_inline_proto_known_vec_fragmented(const void *test_data)
{
	struct ipsec_test_data td_outb;
	struct ipsec_test_flags flags;

	memset(&flags, 0, sizeof(flags));
	flags.fragment = true;
	flags.plaintext_len = plaintext_len;

	memcpy(&td_outb, test_data, sizeof(td_outb));

	/* Disable IV gen to be able to test with known vectors */
	td_outb.ipsec_xform.options.iv_gen_disable = 1;

	return test_ipsec_inline_proto_process(&td_outb, NULL, 1, false,
						&flags);
}

static int
test_ipsec_inline_pkt_replay(const void *test_data, const uint64_t esn[],
		      bool replayed_pkt[], uint32_t nb_pkts, bool esn_en,
		      uint64_t winsz)
{
	struct ipsec_test_data td_outb[IPSEC_TEST_PACKETS_MAX];
	struct ipsec_test_data td_inb[IPSEC_TEST_PACKETS_MAX];
	struct ipsec_test_flags flags;
	uint32_t i, ret = 0;

	memset(&flags, 0, sizeof(flags));
	flags.antireplay = true;
	flags.plaintext_len = plaintext_len;

	for (i = 0; i < nb_pkts; i++) {
		memcpy(&td_outb[i], test_data, sizeof(td_outb[0]));
		td_outb[i].ipsec_xform.options.iv_gen_disable = 1;
		td_outb[i].ipsec_xform.replay_win_sz = winsz;
		td_outb[i].ipsec_xform.options.esn = esn_en;
	}

	for (i = 0; i < nb_pkts; i++)
		td_outb[i].ipsec_xform.esn.value = esn[i];

	ret = test_ipsec_inline_proto_process_with_esn(td_outb, td_inb,
				nb_pkts, true, &flags);
	if (ret != TEST_SUCCESS)
		return ret;

	test_ipsec_td_update(td_inb, td_outb, nb_pkts, &flags);

	for (i = 0; i < nb_pkts; i++) {
		td_inb[i].ipsec_xform.options.esn = esn_en;
		/* Set antireplay flag for packets to be dropped */
		td_inb[i].ar_packet = replayed_pkt[i];
	}

	ret = test_ipsec_inline_proto_process_with_esn(td_inb, NULL, nb_pkts,
				true, &flags);

	return ret;
}

static int
test_ipsec_inline_proto_pkt_antireplay(const void *test_data, uint64_t winsz)
{

	uint32_t nb_pkts = 5;
	bool replayed_pkt[5];
	uint64_t esn[5];

	/* 1. Advance the TOP of the window to WS * 2 */
	esn[0] = winsz * 2;
	/* 2. Test sequence number within the new window(WS + 1) */
	esn[1] = winsz + 1;
	/* 3. Test sequence number less than the window BOTTOM */
	esn[2] = winsz;
	/* 4. Test sequence number in the middle of the window */
	esn[3] = winsz + (winsz / 2);
	/* 5. Test replay of the packet in the middle of the window */
	esn[4] = winsz + (winsz / 2);

	replayed_pkt[0] = false;
	replayed_pkt[1] = false;
	replayed_pkt[2] = true;
	replayed_pkt[3] = false;
	replayed_pkt[4] = true;

	return test_ipsec_inline_pkt_replay(test_data, esn, replayed_pkt,
			nb_pkts, false, winsz);
}

static int
test_ipsec_inline_proto_pkt_antireplay1024(const void *test_data)
{
	return test_ipsec_inline_proto_pkt_antireplay(test_data, 1024);
}

static int
test_ipsec_inline_proto_pkt_antireplay2048(const void *test_data)
{
	return test_ipsec_inline_proto_pkt_antireplay(test_data, 2048);
}

static int
test_ipsec_inline_proto_pkt_antireplay4096(const void *test_data)
{
	return test_ipsec_inline_proto_pkt_antireplay(test_data, 4096);
}

static int
test_ipsec_inline_proto_pkt_esn_antireplay(const void *test_data, uint64_t winsz)
{

	uint32_t nb_pkts = 7;
	bool replayed_pkt[7];
	uint64_t esn[7];

	/* Set the initial sequence number */
	esn[0] = (uint64_t)(0xFFFFFFFF - winsz);
	/* 1. Advance the TOP of the window to (1<<32 + WS/2) */
	esn[1] = (uint64_t)((1ULL << 32) + (winsz / 2));
	/* 2. Test sequence number within new window (1<<32 + WS/2 + 1) */
	esn[2] = (uint64_t)((1ULL << 32) - (winsz / 2) + 1);
	/* 3. Test with sequence number within window (1<<32 - 1) */
	esn[3] = (uint64_t)((1ULL << 32) - 1);
	/* 4. Test with sequence number within window (1<<32 - 1) */
	esn[4] = (uint64_t)(1ULL << 32);
	/* 5. Test with duplicate sequence number within
	 * new window (1<<32 - 1)
	 */
	esn[5] = (uint64_t)((1ULL << 32) - 1);
	/* 6. Test with duplicate sequence number within new window (1<<32) */
	esn[6] = (uint64_t)(1ULL << 32);

	replayed_pkt[0] = false;
	replayed_pkt[1] = false;
	replayed_pkt[2] = false;
	replayed_pkt[3] = false;
	replayed_pkt[4] = false;
	replayed_pkt[5] = true;
	replayed_pkt[6] = true;

	return test_ipsec_inline_pkt_replay(test_data, esn, replayed_pkt, nb_pkts,
				     true, winsz);
}

static int
test_ipsec_inline_proto_pkt_esn_antireplay1024(const void *test_data)
{
	return test_ipsec_inline_proto_pkt_esn_antireplay(test_data, 1024);
}

static int
test_ipsec_inline_proto_pkt_esn_antireplay2048(const void *test_data)
{
	return test_ipsec_inline_proto_pkt_esn_antireplay(test_data, 2048);
}

static int
test_ipsec_inline_proto_pkt_esn_antireplay4096(const void *test_data)
{
	return test_ipsec_inline_proto_pkt_esn_antireplay(test_data, 4096);
}

static struct unit_test_suite inline_ipsec_testsuite  = {
	.suite_name = "Inline IPsec Ethernet Device Unit Test Suite",
	.unit_test_cases = {
		TEST_CASE_NAMED_WITH_DATA(
			"Outbound known vector (ESP tunnel mode IPv4 AES-GCM 128)",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_known_vec, &pkt_aes_128_gcm),
		TEST_CASE_NAMED_WITH_DATA(
			"Outbound known vector (ESP tunnel mode IPv4 AES-GCM 192)",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_known_vec, &pkt_aes_192_gcm),
		TEST_CASE_NAMED_WITH_DATA(
			"Outbound known vector (ESP tunnel mode IPv4 AES-GCM 256)",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_known_vec, &pkt_aes_256_gcm),
		TEST_CASE_NAMED_WITH_DATA(
			"Outbound known vector (ESP tunnel mode IPv4 AES-CBC MD5 [12B ICV])",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_known_vec,
			&pkt_aes_128_cbc_md5),
		TEST_CASE_NAMED_WITH_DATA(
			"Outbound known vector (ESP tunnel mode IPv4 AES-CBC 128 HMAC-SHA256 [16B ICV])",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_known_vec,
			&pkt_aes_128_cbc_hmac_sha256),
		TEST_CASE_NAMED_WITH_DATA(
			"Outbound known vector (ESP tunnel mode IPv4 AES-CBC 128 HMAC-SHA384 [24B ICV])",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_known_vec,
			&pkt_aes_128_cbc_hmac_sha384),
		TEST_CASE_NAMED_WITH_DATA(
			"Outbound known vector (ESP tunnel mode IPv4 AES-CBC 128 HMAC-SHA512 [32B ICV])",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_known_vec,
			&pkt_aes_128_cbc_hmac_sha512),
		TEST_CASE_NAMED_WITH_DATA(
			"Outbound known vector (ESP tunnel mode IPv4 3DES-CBC HMAC-SHA256 [16B ICV])",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_known_vec,
			&pkt_3des_cbc_hmac_sha256),
		TEST_CASE_NAMED_WITH_DATA(
			"Outbound known vector (ESP tunnel mode IPv4 3DES-CBC HMAC-SHA384 [24B ICV])",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_known_vec,
			&pkt_3des_cbc_hmac_sha384),
		TEST_CASE_NAMED_WITH_DATA(
			"Outbound known vector (ESP tunnel mode IPv4 3DES-CBC HMAC-SHA512 [32B ICV])",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_known_vec,
			&pkt_3des_cbc_hmac_sha512),
		TEST_CASE_NAMED_WITH_DATA(
			"Outbound known vector (ESP tunnel mode IPv6 AES-GCM 128)",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_known_vec, &pkt_aes_256_gcm_v6),
		TEST_CASE_NAMED_WITH_DATA(
			"Outbound known vector (ESP tunnel mode IPv6 AES-CBC 128 HMAC-SHA256 [16B ICV])",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_known_vec,
			&pkt_aes_128_cbc_hmac_sha256_v6),
		TEST_CASE_NAMED_WITH_DATA(
			"Outbound known vector (ESP tunnel mode IPv6 3DES-CBC HMAC-SHA256 [16B ICV])",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_known_vec,
			&pkt_3des_cbc_hmac_sha256_v6),
		TEST_CASE_NAMED_WITH_DATA(
			"Outbound known vector (ESP tunnel mode IPv4 NULL AES-XCBC-MAC [12B ICV])",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_known_vec,
			&pkt_null_aes_xcbc),
		TEST_CASE_NAMED_WITH_DATA(
			"Outbound known vector (ESP tunnel mode IPv4 DES-CBC HMAC-SHA256 [16B ICV])",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_known_vec,
			&pkt_des_cbc_hmac_sha256),
		TEST_CASE_NAMED_WITH_DATA(
			"Outbound known vector (ESP tunnel mode IPv4 DES-CBC HMAC-SHA384 [24B ICV])",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_known_vec,
			&pkt_des_cbc_hmac_sha384),
		TEST_CASE_NAMED_WITH_DATA(
			"Outbound known vector (ESP tunnel mode IPv4 DES-CBC HMAC-SHA512 [32B ICV])",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_known_vec,
			&pkt_des_cbc_hmac_sha512),
		TEST_CASE_NAMED_WITH_DATA(
			"Outbound known vector (ESP tunnel mode IPv6 DES-CBC HMAC-SHA256 [16B ICV])",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_known_vec,
			&pkt_des_cbc_hmac_sha256_v6),

		TEST_CASE_NAMED_WITH_DATA(
			"Outbound fragmented packet",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_known_vec_fragmented,
			&pkt_aes_128_gcm_frag),

		TEST_CASE_NAMED_WITH_DATA(
			"Inbound known vector (ESP tunnel mode IPv4 AES-GCM 128)",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_known_vec_inb, &pkt_aes_128_gcm),
		TEST_CASE_NAMED_WITH_DATA(
			"Inbound known vector (ESP tunnel mode IPv4 AES-GCM 192)",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_known_vec_inb, &pkt_aes_192_gcm),
		TEST_CASE_NAMED_WITH_DATA(
			"Inbound known vector (ESP tunnel mode IPv4 AES-GCM 256)",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_known_vec_inb, &pkt_aes_256_gcm),
		TEST_CASE_NAMED_WITH_DATA(
			"Inbound known vector (ESP tunnel mode IPv4 AES-CBC 128)",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_known_vec_inb, &pkt_aes_128_cbc_null),
		TEST_CASE_NAMED_WITH_DATA(
			"Inbound known vector (ESP tunnel mode IPv4 AES-CBC MD5 [12B ICV])",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_known_vec_inb,
			&pkt_aes_128_cbc_md5),
		TEST_CASE_NAMED_WITH_DATA(
			"Inbound known vector (ESP tunnel mode IPv4 AES-CBC 128 HMAC-SHA256 [16B ICV])",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_known_vec_inb,
			&pkt_aes_128_cbc_hmac_sha256),
		TEST_CASE_NAMED_WITH_DATA(
			"Inbound known vector (ESP tunnel mode IPv4 AES-CBC 128 HMAC-SHA384 [24B ICV])",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_known_vec_inb,
			&pkt_aes_128_cbc_hmac_sha384),
		TEST_CASE_NAMED_WITH_DATA(
			"Inbound known vector (ESP tunnel mode IPv4 AES-CBC 128 HMAC-SHA512 [32B ICV])",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_known_vec_inb,
			&pkt_aes_128_cbc_hmac_sha512),
		TEST_CASE_NAMED_WITH_DATA(
			"Inbound known vector (ESP tunnel mode IPv4 3DES-CBC HMAC-SHA256 [16B ICV])",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_known_vec_inb,
			&pkt_3des_cbc_hmac_sha256),
		TEST_CASE_NAMED_WITH_DATA(
			"Inbound known vector (ESP tunnel mode IPv4 3DES-CBC HMAC-SHA384 [24B ICV])",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_known_vec_inb,
			&pkt_3des_cbc_hmac_sha384),
		TEST_CASE_NAMED_WITH_DATA(
			"Inbound known vector (ESP tunnel mode IPv4 3DES-CBC HMAC-SHA512 [32B ICV])",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_known_vec_inb,
			&pkt_3des_cbc_hmac_sha512),
		TEST_CASE_NAMED_WITH_DATA(
			"Inbound known vector (ESP tunnel mode IPv6 AES-GCM 128)",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_known_vec_inb, &pkt_aes_256_gcm_v6),
		TEST_CASE_NAMED_WITH_DATA(
			"Inbound known vector (ESP tunnel mode IPv6 AES-CBC 128 HMAC-SHA256 [16B ICV])",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_known_vec_inb,
			&pkt_aes_128_cbc_hmac_sha256_v6),
		TEST_CASE_NAMED_WITH_DATA(
			"Inbound known vector (ESP tunnel mode IPv6 3DES-CBC HMAC-SHA256 [16B ICV])",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_known_vec_inb,
			&pkt_3des_cbc_hmac_sha256_v6),
		TEST_CASE_NAMED_WITH_DATA(
			"Inbound known vector (ESP tunnel mode IPv4 NULL AES-XCBC-MAC [12B ICV])",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_known_vec_inb,
			&pkt_null_aes_xcbc),
		TEST_CASE_NAMED_WITH_DATA(
			"Inbound known vector (ESP tunnel mode IPv4 DES-CBC HMAC-SHA256 [16B ICV])",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_known_vec_inb,
			&pkt_des_cbc_hmac_sha256),
		TEST_CASE_NAMED_WITH_DATA(
			"Inbound known vector (ESP tunnel mode IPv4 DES-CBC HMAC-SHA384 [24B ICV])",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_known_vec_inb,
			&pkt_des_cbc_hmac_sha384),
		TEST_CASE_NAMED_WITH_DATA(
			"Inbound known vector (ESP tunnel mode IPv4 DES-CBC HMAC-SHA512 [32B ICV])",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_known_vec_inb,
			&pkt_des_cbc_hmac_sha512),
		TEST_CASE_NAMED_WITH_DATA(
			"Inbound known vector (ESP tunnel mode IPv6 DES-CBC HMAC-SHA256 [16B ICV])",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_known_vec_inb,
			&pkt_des_cbc_hmac_sha256_v6),


		TEST_CASE_NAMED_ST(
			"Combined test alg list",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_display_list),

		TEST_CASE_NAMED_ST(
			"UDP encapsulation",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_udp_encap),
		TEST_CASE_NAMED_ST(
			"UDP encapsulation ports verification test",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_udp_ports_verify),
		TEST_CASE_NAMED_ST(
			"Negative test: ICV corruption",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_err_icv_corrupt),
		TEST_CASE_NAMED_ST(
			"Tunnel dst addr verification",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_tunnel_dst_addr_verify),
		TEST_CASE_NAMED_ST(
			"Tunnel src and dst addr verification",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_tunnel_src_dst_addr_verify),
		TEST_CASE_NAMED_ST(
			"Inner IP checksum",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_inner_ip_csum),
		TEST_CASE_NAMED_ST(
			"Inner L4 checksum",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_inner_l4_csum),
		TEST_CASE_NAMED_ST(
			"Tunnel IPv4 in IPv4",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_tunnel_v4_in_v4),
		TEST_CASE_NAMED_ST(
			"Tunnel IPv6 in IPv6",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_tunnel_v6_in_v6),
		TEST_CASE_NAMED_ST(
			"Tunnel IPv4 in IPv6",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_tunnel_v4_in_v6),
		TEST_CASE_NAMED_ST(
			"Tunnel IPv6 in IPv4",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_tunnel_v6_in_v4),
		TEST_CASE_NAMED_ST(
			"Transport IPv4",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_transport_v4),
		TEST_CASE_NAMED_ST(
			"Transport l4 checksum",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_transport_l4_csum),
		TEST_CASE_NAMED_ST(
			"Statistics: success",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_stats),
		TEST_CASE_NAMED_ST(
			"Fragmented packet",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_pkt_fragment),
		TEST_CASE_NAMED_ST(
			"Tunnel header copy DF (inner 0)",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_copy_df_inner_0),
		TEST_CASE_NAMED_ST(
			"Tunnel header copy DF (inner 1)",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_copy_df_inner_1),
		TEST_CASE_NAMED_ST(
			"Tunnel header set DF 0 (inner 1)",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_set_df_0_inner_1),
		TEST_CASE_NAMED_ST(
			"Tunnel header set DF 1 (inner 0)",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_set_df_1_inner_0),
		TEST_CASE_NAMED_ST(
			"Tunnel header IPv4 copy DSCP (inner 0)",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_ipv4_copy_dscp_inner_0),
		TEST_CASE_NAMED_ST(
			"Tunnel header IPv4 copy DSCP (inner 1)",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_ipv4_copy_dscp_inner_1),
		TEST_CASE_NAMED_ST(
			"Tunnel header IPv4 set DSCP 0 (inner 1)",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_ipv4_set_dscp_0_inner_1),
		TEST_CASE_NAMED_ST(
			"Tunnel header IPv4 set DSCP 1 (inner 0)",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_ipv4_set_dscp_1_inner_0),
		TEST_CASE_NAMED_ST(
			"Tunnel header IPv6 copy DSCP (inner 0)",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_ipv6_copy_dscp_inner_0),
		TEST_CASE_NAMED_ST(
			"Tunnel header IPv6 copy DSCP (inner 1)",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_ipv6_copy_dscp_inner_1),
		TEST_CASE_NAMED_ST(
			"Tunnel header IPv6 set DSCP 0 (inner 1)",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_ipv6_set_dscp_0_inner_1),
		TEST_CASE_NAMED_ST(
			"Tunnel header IPv6 set DSCP 1 (inner 0)",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_ipv6_set_dscp_1_inner_0),
		TEST_CASE_NAMED_ST(
			"Tunnel header IPv6 copy FLABEL (inner 0)",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_ipv6_copy_flabel_inner_0),
		TEST_CASE_NAMED_ST(
			"Tunnel header IPv6 copy FLABEL (inner 1)",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_ipv6_copy_flabel_inner_1),
		TEST_CASE_NAMED_ST(
			"Tunnel header IPv6 set FLABEL 0 (inner 1)",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_ipv6_set_flabel_0_inner_1),
		TEST_CASE_NAMED_ST(
			"Tunnel header IPv6 set FLABEL 1 (inner 0)",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_ipv6_set_flabel_1_inner_0),
		TEST_CASE_NAMED_ST(
			"Tunnel header IPv4 decrement inner TTL",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_ipv4_ttl_decrement),
		TEST_CASE_NAMED_ST(
			"Tunnel header IPv6 decrement inner hop limit",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_ipv6_hop_limit_decrement),
		TEST_CASE_NAMED_ST(
			"IV generation",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_iv_gen),
		TEST_CASE_NAMED_ST(
			"SA soft expiry with packet limit",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_sa_pkt_soft_expiry),
		TEST_CASE_NAMED_ST(
			"SA soft expiry with byte limit",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_sa_byte_soft_expiry),
		TEST_CASE_NAMED_ST(
			"SA hard expiry with packet limit",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_sa_pkt_hard_expiry),
		TEST_CASE_NAMED_ST(
			"SA hard expiry with byte limit",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_sa_byte_hard_expiry),

		TEST_CASE_NAMED_WITH_DATA(
			"Antireplay with window size 1024",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_pkt_antireplay1024,
			&pkt_aes_128_gcm),
		TEST_CASE_NAMED_WITH_DATA(
			"Antireplay with window size 2048",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_pkt_antireplay2048,
			&pkt_aes_128_gcm),
		TEST_CASE_NAMED_WITH_DATA(
			"Antireplay with window size 4096",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_pkt_antireplay4096,
			&pkt_aes_128_gcm),
		TEST_CASE_NAMED_WITH_DATA(
			"ESN and Antireplay with window size 1024",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_pkt_esn_antireplay1024,
			&pkt_aes_128_gcm),
		TEST_CASE_NAMED_WITH_DATA(
			"ESN and Antireplay with window size 2048",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_pkt_esn_antireplay2048,
			&pkt_aes_128_gcm),
		TEST_CASE_NAMED_WITH_DATA(
			"ESN and Antireplay with window size 4096",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_pkt_esn_antireplay4096,
			&pkt_aes_128_gcm),

		TEST_CASE_NAMED_WITH_DATA(
			"IPv4 Reassembly with 2 fragments",
			ut_setup_inline_ipsec_reassembly, ut_teardown_inline_ipsec_reassembly,
			test_inline_ip_reassembly, &ipv4_2frag_vector),
		TEST_CASE_NAMED_WITH_DATA(
			"IPv6 Reassembly with 2 fragments",
			ut_setup_inline_ipsec_reassembly, ut_teardown_inline_ipsec_reassembly,
			test_inline_ip_reassembly, &ipv6_2frag_vector),
		TEST_CASE_NAMED_WITH_DATA(
			"IPv4 Reassembly with 4 fragments",
			ut_setup_inline_ipsec_reassembly, ut_teardown_inline_ipsec_reassembly,
			test_inline_ip_reassembly, &ipv4_4frag_vector),
		TEST_CASE_NAMED_WITH_DATA(
			"IPv6 Reassembly with 4 fragments",
			ut_setup_inline_ipsec_reassembly, ut_teardown_inline_ipsec_reassembly,
			test_inline_ip_reassembly, &ipv6_4frag_vector),
		TEST_CASE_NAMED_WITH_DATA(
			"IPv4 Reassembly with 5 fragments",
			ut_setup_inline_ipsec_reassembly, ut_teardown_inline_ipsec_reassembly,
			test_inline_ip_reassembly, &ipv4_5frag_vector),
		TEST_CASE_NAMED_WITH_DATA(
			"IPv6 Reassembly with 5 fragments",
			ut_setup_inline_ipsec_reassembly, ut_teardown_inline_ipsec_reassembly,
			test_inline_ip_reassembly, &ipv6_5frag_vector),
		TEST_CASE_NAMED_WITH_DATA(
			"IPv4 Reassembly with incomplete fragments",
			ut_setup_inline_ipsec_reassembly, ut_teardown_inline_ipsec_reassembly,
			test_inline_ip_reassembly, &ipv4_incomplete_vector),
		TEST_CASE_NAMED_WITH_DATA(
			"IPv4 Reassembly with overlapping fragments",
			ut_setup_inline_ipsec_reassembly, ut_teardown_inline_ipsec_reassembly,
			test_inline_ip_reassembly, &ipv4_overlap_vector),
		TEST_CASE_NAMED_WITH_DATA(
			"IPv4 Reassembly with out of order fragments",
			ut_setup_inline_ipsec_reassembly, ut_teardown_inline_ipsec_reassembly,
			test_inline_ip_reassembly, &ipv4_out_of_order_vector),
		TEST_CASE_NAMED_WITH_DATA(
			"IPv4 Reassembly with burst of 4 fragments",
			ut_setup_inline_ipsec_reassembly, ut_teardown_inline_ipsec_reassembly,
			test_inline_ip_reassembly, &ipv4_4frag_burst_vector),
		TEST_CASE_NAMED_WITH_DATA(
			"Inbound Out-Of-Place processing",
			ut_setup_inline_ipsec, ut_teardown_inline_ipsec,
			test_ipsec_inline_proto_oop_inb,
			&pkt_aes_128_gcm),

		TEST_CASES_END() /**< NULL terminate unit test array */
	},
};


static int
test_inline_ipsec(void)
{
	inline_ipsec_testsuite.setup = inline_ipsec_testsuite_setup;
	inline_ipsec_testsuite.teardown = inline_ipsec_testsuite_teardown;
	return unit_test_suite_runner(&inline_ipsec_testsuite);
}


static int
test_inline_ipsec_sg(void)
{
	int rc;

	inline_ipsec_testsuite.setup = inline_ipsec_testsuite_setup;
	inline_ipsec_testsuite.teardown = inline_ipsec_testsuite_teardown;

	sg_mode = true;
	/* Run the tests */
	rc = unit_test_suite_runner(&inline_ipsec_testsuite);
	sg_mode = false;

	port_conf.rxmode.offloads &= ~RTE_ETH_RX_OFFLOAD_SCATTER;
	port_conf.txmode.offloads &= ~RTE_ETH_TX_OFFLOAD_MULTI_SEGS;
	return rc;
}

static int
test_event_inline_ipsec(void)
{
	inline_ipsec_testsuite.setup = event_inline_ipsec_testsuite_setup;
	inline_ipsec_testsuite.teardown = event_inline_ipsec_testsuite_teardown;
	return unit_test_suite_runner(&inline_ipsec_testsuite);
}

#endif /* !RTE_EXEC_ENV_WINDOWS */

REGISTER_TEST_COMMAND(inline_ipsec_autotest, test_inline_ipsec);
REGISTER_TEST_COMMAND(inline_ipsec_sg_autotest, test_inline_ipsec_sg);
REGISTER_TEST_COMMAND(event_inline_ipsec_autotest, test_event_inline_ipsec);
