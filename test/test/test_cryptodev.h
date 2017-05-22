/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2015-2017 Intel Corporation. All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *	 * Redistributions of source code must retain the above copyright
 *	   notice, this list of conditions and the following disclaimer.
 *	 * Redistributions in binary form must reproduce the above copyright
 *	   notice, this list of conditions and the following disclaimer in
 *	   the documentation and/or other materials provided with the
 *	   distribution.
 *	 * Neither the name of Intel Corporation nor the names of its
 *	   contributors may be used to endorse or promote products derived
 *	   from this software without specific prior written permission.
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
#ifndef TEST_CRYPTODEV_H_
#define TEST_CRYPTODEV_H_

#define HEX_DUMP 0

#define FALSE                           0
#define TRUE                            1

#define MAX_NUM_OPS_INFLIGHT            (4096)
#define MIN_NUM_OPS_INFLIGHT            (128)
#define DEFAULT_NUM_OPS_INFLIGHT        (128)

#define MAX_NUM_QPS_PER_QAT_DEVICE      (2)
#define DEFAULT_NUM_QPS_PER_QAT_DEVICE  (2)
#define DEFAULT_BURST_SIZE              (64)
#define DEFAULT_NUM_XFORMS              (2)
#define NUM_MBUFS                       (8191)
#define MBUF_CACHE_SIZE                 (256)
#define MBUF_DATAPAYLOAD_SIZE		(2048 + DIGEST_BYTE_LENGTH_SHA512)
#define MBUF_SIZE			(sizeof(struct rte_mbuf) + \
		RTE_PKTMBUF_HEADROOM + MBUF_DATAPAYLOAD_SIZE)

#define BYTE_LENGTH(x)				(x/8)
/* HASH DIGEST LENGTHS */
#define DIGEST_BYTE_LENGTH_MD5			(BYTE_LENGTH(128))
#define DIGEST_BYTE_LENGTH_SHA1			(BYTE_LENGTH(160))
#define DIGEST_BYTE_LENGTH_SHA224		(BYTE_LENGTH(224))
#define DIGEST_BYTE_LENGTH_SHA256		(BYTE_LENGTH(256))
#define DIGEST_BYTE_LENGTH_SHA384		(BYTE_LENGTH(384))
#define DIGEST_BYTE_LENGTH_SHA512		(BYTE_LENGTH(512))
#define DIGEST_BYTE_LENGTH_AES_XCBC		(BYTE_LENGTH(96))
#define DIGEST_BYTE_LENGTH_SNOW3G_UIA2		(BYTE_LENGTH(32))
#define DIGEST_BYTE_LENGTH_KASUMI_F9		(BYTE_LENGTH(32))
#define AES_XCBC_MAC_KEY_SZ			(16)
#define DIGEST_BYTE_LENGTH_AES_GCM		(BYTE_LENGTH(128))

#define TRUNCATED_DIGEST_BYTE_LENGTH_SHA1		(12)
#define TRUNCATED_DIGEST_BYTE_LENGTH_SHA224		(16)
#define TRUNCATED_DIGEST_BYTE_LENGTH_SHA256		(16)
#define TRUNCATED_DIGEST_BYTE_LENGTH_SHA384		(24)
#define TRUNCATED_DIGEST_BYTE_LENGTH_SHA512		(32)

#define MAXIMUM_IV_LENGTH				(16)

#define IV_OFFSET			(sizeof(struct rte_crypto_op) + \
		sizeof(struct rte_crypto_sym_op) + DEFAULT_NUM_XFORMS * \
		sizeof(struct rte_crypto_sym_xform))

#define CRYPTODEV_NAME_NULL_PMD		crypto_null
#define CRYPTODEV_NAME_AESNI_MB_PMD	crypto_aesni_mb
#define CRYPTODEV_NAME_AESNI_GCM_PMD	crypto_aesni_gcm
#define CRYPTODEV_NAME_OPENSSL_PMD	crypto_openssl
#define CRYPTODEV_NAME_QAT_SYM_PMD	crypto_qat
#define CRYPTODEV_NAME_SNOW3G_PMD	crypto_snow3g
#define CRYPTODEV_NAME_KASUMI_PMD	crypto_kasumi
#define CRYPTODEV_NAME_ZUC_PMD		crypto_zuc
#define CRYPTODEV_NAME_ARMV8_PMD	crypto_armv8
#define CRYPTODEV_NAME_DPAA2_SEC_PMD	crypto_dpaa2_sec
#define CRYPTODEV_NAME_SCHEDULER_PMD	crypto_scheduler

/**
 * Write (spread) data from buffer to mbuf data
 *
 * @param mbuf
 *   Destination mbuf
 * @param offset
 *   Start offset in mbuf
 * @param len
 *   Number of bytes to copy
 * @param buffer
 *   Continuous source buffer
 */
static inline void
pktmbuf_write(struct rte_mbuf *mbuf, int offset, int len, const uint8_t *buffer)
{
	int n = len;
	int l;
	struct rte_mbuf *m;
	char *dst;

	for (m = mbuf; (m != NULL) && (offset > m->data_len); m = m->next)
		offset -= m->data_len;

	l = m->data_len - offset;

	/* copy data from first segment */
	dst = rte_pktmbuf_mtod_offset(m, char *, offset);
	if (len <= l) {
		rte_memcpy(dst, buffer, len);
		return;
	}

	rte_memcpy(dst, buffer, l);
	buffer += l;
	n -= l;

	for (m = m->next; (m != NULL) && (n > 0); m = m->next) {
		dst = rte_pktmbuf_mtod(m, char *);
		l = m->data_len;
		if (n < l) {
			rte_memcpy(dst, buffer, n);
			return;
		}
		rte_memcpy(dst, buffer, l);
		buffer += l;
		n -= l;
	}
}

static inline uint8_t *
pktmbuf_mtod_offset(struct rte_mbuf *mbuf, int offset) {
	struct rte_mbuf *m;

	for (m = mbuf; (m != NULL) && (offset > m->data_len); m = m->next)
		offset -= m->data_len;

	if (m == NULL) {
		printf("pktmbuf_mtod_offset: offset out of buffer\n");
		return NULL;
	}
	return rte_pktmbuf_mtod_offset(m, uint8_t *, offset);
}

static inline phys_addr_t
pktmbuf_mtophys_offset(struct rte_mbuf *mbuf, int offset) {
	struct rte_mbuf *m;

	for (m = mbuf; (m != NULL) && (offset > m->data_len); m = m->next)
		offset -= m->data_len;

	if (m == NULL) {
		printf("pktmbuf_mtophys_offset: offset out of buffer\n");
		return 0;
	}
	return rte_pktmbuf_mtophys_offset(m, offset);
}

static inline struct rte_mbuf *
create_segmented_mbuf(struct rte_mempool *mbuf_pool, int pkt_len,
		int nb_segs, uint8_t pattern) {

	struct rte_mbuf *m = NULL, *mbuf = NULL;
	uint8_t *dst;
	int data_len = 0;
	int i, size;
	int t_len;

	if (pkt_len < 1) {
		printf("Packet size must be 1 or more (is %d)\n", pkt_len);
		return NULL;
	}

	if (nb_segs < 1) {
		printf("Number of segments must be 1 or more (is %d)\n",
				nb_segs);
		return NULL;
	}

	t_len = pkt_len >= nb_segs ? pkt_len / nb_segs : 1;
	size = pkt_len;

	/* Create chained mbuf_src and fill it generated data */
	for (i = 0; size > 0; i++) {

		m = rte_pktmbuf_alloc(mbuf_pool);
		if (i == 0)
			mbuf = m;

		if (m == NULL) {
			printf("Cannot create segment for source mbuf");
			goto fail;
		}

		/* Make sure if tailroom is zeroed */
		memset(m->buf_addr, pattern, m->buf_len);

		data_len = size > t_len ? t_len : size;
		dst = (uint8_t *)rte_pktmbuf_append(m, data_len);
		if (dst == NULL) {
			printf("Cannot append %d bytes to the mbuf\n",
					data_len);
			goto fail;
		}

		if (mbuf != m)
			rte_pktmbuf_chain(mbuf, m);

		size -= data_len;

	}
	return mbuf;

fail:
	if (mbuf)
		rte_pktmbuf_free(mbuf);
	return NULL;
}

#endif /* TEST_CRYPTODEV_H_ */
