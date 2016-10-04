/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2015-2016 Intel Corporation. All rights reserved.
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

#endif /* TEST_CRYPTODEV_H_ */
