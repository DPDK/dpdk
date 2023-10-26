/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2020 Intel Corporation
 */

#ifndef _SA_H_
#define _SA_H_


#define IPSEC_MAX_HDR_SIZE	64
#define IPSEC_MAX_IV_SIZE	16
#define IPSEC_MAX_IV_QWORD	(IPSEC_MAX_IV_SIZE / sizeof(uint64_t))
#define TUN_HDR_MSK (RTE_IPSEC_SATP_ECN_MASK | RTE_IPSEC_SATP_DSCP_MASK)

/* padding alignment for different algorithms */
enum {
	IPSEC_PAD_DEFAULT = 4,
	IPSEC_PAD_3DES_CBC = 8,
	IPSEC_PAD_AES_CBC = IPSEC_MAX_IV_SIZE,
	IPSEC_PAD_AES_CTR = IPSEC_PAD_DEFAULT,
	IPSEC_PAD_AES_GCM = IPSEC_PAD_DEFAULT,
	IPSEC_PAD_AES_CCM = IPSEC_PAD_DEFAULT,
	IPSEC_PAD_CHACHA20_POLY1305 = IPSEC_PAD_DEFAULT,
	IPSEC_PAD_NULL = IPSEC_PAD_DEFAULT,
	IPSEC_PAD_AES_GMAC = IPSEC_PAD_DEFAULT,
};

/* iv sizes for different algorithms */
enum {
	IPSEC_IV_SIZE_DEFAULT = IPSEC_MAX_IV_SIZE,
	IPSEC_AES_CTR_IV_SIZE = sizeof(uint64_t),
	/* TripleDES supports IV size of 32bits or 64bits but he library
	 * only supports 64bits.
	 */
	IPSEC_3DES_IV_SIZE = sizeof(uint64_t),
};

/* these definitions probably has to be in rte_crypto_sym.h */
union sym_op_ofslen {
	uint64_t raw;
	struct {
		uint32_t offset;
		uint32_t length;
	};
};

union sym_op_data {
#ifdef __SIZEOF_INT128__
	__uint128_t raw;
#endif
	struct {
		uint8_t *va;
		rte_iova_t pa;
	};
};

#define REPLAY_SQN_NUM		2
#define REPLAY_SQN_NEXT(n)	((n) ^ 1)

struct replay_sqn {
	rte_rwlock_t rwl;
	uint64_t sqn;
	__extension__ uint64_t window[];
};

/*IPSEC SA supported algorithms */
enum sa_algo_type	{
	ALGO_TYPE_NULL = 0,
	ALGO_TYPE_3DES_CBC,
	ALGO_TYPE_AES_CBC,
	ALGO_TYPE_AES_CTR,
	ALGO_TYPE_AES_GCM,
	ALGO_TYPE_AES_CCM,
	ALGO_TYPE_CHACHA20_POLY1305,
	ALGO_TYPE_AES_GMAC,
	ALGO_TYPE_MAX
};

struct rte_ipsec_sa {

	uint64_t type;     /* type of given SA */
	uint64_t udata;    /* user defined */
	uint32_t size;     /* size of given sa object */
	uint32_t spi;
	/* sqn calculations related */
	uint64_t sqn_mask;
	struct {
		uint32_t win_sz;
		uint16_t nb_bucket;
		uint16_t bucket_index_mask;
	} replay;
	/* template for crypto op fields */
	struct {
		union sym_op_ofslen cipher;
		union sym_op_ofslen auth;
	} ctp;
	/* cpu-crypto offsets */
	union rte_crypto_sym_ofs cofs;
	/* tx_offload template for tunnel mbuf */
	struct {
		uint64_t msk;
		uint64_t val;
	} tx_offload;
	uint32_t salt;
	uint8_t algo_type;
	uint8_t proto;    /* next proto */
	uint8_t aad_len;
	uint8_t hdr_len;
	uint8_t hdr_l3_off;
	uint8_t icv_len;
	uint8_t sqh_len;
	uint8_t iv_ofs; /* offset for algo-specific IV inside crypto op */
	uint8_t iv_len;
	uint8_t pad_align;
	uint8_t tos_mask;

	/* template for tunnel header */
	uint8_t hdr[IPSEC_MAX_HDR_SIZE];

	/*
	 * sqn and replay window
	 * In case of SA handled by multiple threads *sqn* cacheline
	 * could be shared by multiple cores.
	 * To minimise performance impact, we try to locate in a separate
	 * place from other frequently accessed data.
	 */
	union {
		RTE_ATOMIC(uint64_t) outb;
		struct {
			uint32_t rdidx; /* read index */
			uint32_t wridx; /* write index */
			struct replay_sqn *rsn[REPLAY_SQN_NUM];
		} inb;
	} sqn;
	/* Statistics */
	struct {
		uint64_t count;
		uint64_t bytes;
		struct {
			uint64_t count;
			uint64_t authentication_failed;
		} errors;
	} statistics;

} __rte_cache_aligned;

int
ipsec_sa_pkt_func_select(const struct rte_ipsec_session *ss,
	const struct rte_ipsec_sa *sa, struct rte_ipsec_sa_pkt_func *pf);

/* inbound processing */

uint16_t
esp_inb_pkt_prepare(const struct rte_ipsec_session *ss, struct rte_mbuf *mb[],
	struct rte_crypto_op *cop[], uint16_t num);

uint16_t
esp_inb_tun_pkt_process(const struct rte_ipsec_session *ss,
	struct rte_mbuf *mb[], uint16_t num);

uint16_t
inline_inb_tun_pkt_process(const struct rte_ipsec_session *ss,
	struct rte_mbuf *mb[], uint16_t num);

uint16_t
esp_inb_trs_pkt_process(const struct rte_ipsec_session *ss,
	struct rte_mbuf *mb[], uint16_t num);

uint16_t
inline_inb_trs_pkt_process(const struct rte_ipsec_session *ss,
	struct rte_mbuf *mb[], uint16_t num);

uint16_t
cpu_inb_pkt_prepare(const struct rte_ipsec_session *ss,
		struct rte_mbuf *mb[], uint16_t num);

/* outbound processing */

uint16_t
esp_outb_tun_prepare(const struct rte_ipsec_session *ss, struct rte_mbuf *mb[],
	struct rte_crypto_op *cop[], uint16_t num);

uint16_t
esp_outb_trs_prepare(const struct rte_ipsec_session *ss, struct rte_mbuf *mb[],
	struct rte_crypto_op *cop[], uint16_t num);

uint16_t
esp_outb_sqh_process(const struct rte_ipsec_session *ss, struct rte_mbuf *mb[],
	uint16_t num);

uint16_t
pkt_flag_process(const struct rte_ipsec_session *ss,
	struct rte_mbuf *mb[], uint16_t num);

uint16_t
inline_outb_tun_pkt_process(const struct rte_ipsec_session *ss,
	struct rte_mbuf *mb[], uint16_t num);

uint16_t
inline_outb_trs_pkt_process(const struct rte_ipsec_session *ss,
	struct rte_mbuf *mb[], uint16_t num);

uint16_t
inline_proto_outb_pkt_process(const struct rte_ipsec_session *ss,
	struct rte_mbuf *mb[], uint16_t num);

uint16_t
cpu_outb_tun_pkt_prepare(const struct rte_ipsec_session *ss,
		struct rte_mbuf *mb[], uint16_t num);
uint16_t
cpu_outb_trs_pkt_prepare(const struct rte_ipsec_session *ss,
		struct rte_mbuf *mb[], uint16_t num);

#endif /* _SA_H_ */
