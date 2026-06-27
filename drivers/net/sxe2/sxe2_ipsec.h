/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C), 2025, Wuxi Stars Micro System Technologies Co., Ltd.
 */
#ifndef __SXE2_IPSEC_H__
#define __SXE2_IPSEC_H__

#include <rte_security.h>
#include <rte_security_driver.h>

struct sxe2_adapter;
struct sxe2_security_session;

#define        SXE2_IPSEC_AES_KEY_MIN    (32)
#define        SXE2_IPSEC_AES_KEY_MAX    (32)
#define        SXE2_IPSEC_AES_KEY_INC    (0)

#define        SXE2_IPSEC_SM4_KEY_MIN    (16)
#define        SXE2_IPSEC_SM4_KEY_MAX    (16)
#define        SXE2_IPSEC_SM4_KEY_INC    (0)

#define        SXE2_IPSEC_SHA_KEY_MIN    (32)
#define        SXE2_IPSEC_SHA_KEY_MAX    (32)
#define        SXE2_IPSEC_SHA_KEY_INC    (0)

#define        SXE2_IPSEC_SM3_KEY_MIN    (32)
#define        SXE2_IPSEC_SM3_KEY_MAX    (32)
#define        SXE2_IPSEC_SM3_KEY_INC    (0)

#define        SXE2_IPSEC_AES_IV_MIN    (16)
#define        SXE2_IPSEC_AES_IV_MAX    (16)
#define        SXE2_IPSEC_AES_IV_INC    (0)

#define        SXE2_IPSEC_SM4_IV_MIN    (16)
#define        SXE2_IPSEC_SM4_IV_MAX    (16)
#define        SXE2_IPSEC_SM4_IV_INC    (0)

#define        SXE2_IPSEC_SHA_IV_MIN    (0)
#define        SXE2_IPSEC_SHA_IV_MAX    (32)
#define        SXE2_IPSEC_SHA_IV_INC    (16)

#define        SXE2_IPSEC_SM3_IV_MIN    (0)
#define        SXE2_IPSEC_SM3_IV_MAX    (32)
#define        SXE2_IPSEC_SM3_IV_INC    (16)

#define        SXE2_IPSEC_SHA_DIGEST_MIN    (32)
#define        SXE2_IPSEC_SHA_DIGEST_MAX    (32)
#define        SXE2_IPSEC_SHA_DIGEST_INC    (0)

#define        SXE2_IPSEC_SM3_DIGEST_MIN    (32)
#define        SXE2_IPSEC_SM3_DIGEST_MAX    (32)
#define        SXE2_IPSEC_SM3_DIGEST_INC    (0)

#define        SXE2_IPSEC_AAD_MIN           (0)
#define        SXE2_IPSEC_AAD_MAX           (0)
#define        SXE2_IPSEC_AAD_INC           (0)

#define        SXE2_IPSEC_MAX_KEY_LEN		 (32)
#define        SXE2_IPSEC_MIN_KEY_LEN       (0)

#define SXE2_IPSEC_OL_FLAGS_IS_TUN    (0x1 << 0)
#define SXE2_IPSEC_OL_FLAGS_IS_ESP    (0x1 << 1)

#define SXE2_IPSEC_DEFAULT_SA_OFFSET  (0)
#define SXE2_IPSEC_DEFAULT_SA_LEN     (1024)

#define IPSEC_TX_ENCRYPT    (RTE_BIT32(0))
#define IPSEC_TX_ENGINE_SM4 (RTE_BIT32(1))

#define IPSEC_RX_VALID      (RTE_BIT32(0))
#define IPSEC_RX_IPV6       (RTE_BIT32(2))
#define IPSEC_RX_DECRYPT    (RTE_BIT32(3))
#define IPSEC_RX_ENGINE_SM4 (RTE_BIT32(4))

#define IPSEC_IPV6_LEN                 (4)
#define IPSEC_ESP_OFFSET_MIN           (16)
#define IPSEC_ESP_OFFSET_MAX           (256)

enum sxe2_ipsec_cap {
	SXE2_IPSEC_CAP_ENC_AES_CBC      = 0,
	SXE2_IPSEC_CAP_ENC_SM4_CBC      = 1,
	SXE2_IPSEC_CAP_AUTH_SHA256_HMAC = 2,
	SXE2_IPSEC_CAP_AUTH_SM3_HMAC    = 3,
	SXE2_IPSEC_CAP_MAX              = 4,
};

enum sxe2_ipsec_icv_len {
	SXE2_IPSEC_ICV_0_BYTES = 0,
	SXE2_IPSEC_ICV_12_BYTES,
	SXE2_IPSEC_ICV_16_BYTES,
	SXE2_IPSEC_ICV_INVALID,
};

enum sxe2_ipsec_bypass_dir {
	SXE2_IPSEC_BYPASS_DIR_RX = 0,
	SXE2_IPSEC_BYPASS_DIR_TX,
	SXE2_IPSEC_BYPASS_DIR_INVALID,
};

enum sxe2_ipsec_bypass_status {
	SXE2_IPSEC_BYPASS_STATUS_DISABLE = 0,
	SXE2_IPSEC_BYPASS_STATUS_ENABLE,
	SXE2_IPSEC_BYPASS_STATUS_INVALID,
};

enum sxe2_ipsec_status {
	SXE2_IPSEC_ENC_BYPASS = 0,
	SXE2_IPSEC_ENC_ENABLE,
	SXE2_IPSEC_ENC_INVALID,
};

enum sxe2_ipsec_mode {
	SXE2_IPSEC_MODE_ENC_AND_AUTH = 0,
	SXE2_IPSEC_MODE_ONLY_ENCRYPT,
	SXE2_IPSEC_MODE_INVALID,
};

struct sxe2_ipsec_ip_param {
	enum rte_security_ipsec_tunnel_type type;
	union {
		uint32_t dst_ipv4;
		uint32_t dst_ipv6[IPSEC_IPV6_LEN];
	};
};

enum sxe2_ipsec_algorithm {
	SXE2_IPSEC_ALGO_AES_CBC_AND_SHA256_128_HMAC = 0,
	SXE2_IPSEC_ALGO_SM4_CBC_AND_SM3_96_HMAC,
	SXE2_IPSEC_ALGO_INVALID,
};

struct sxe2_ipsec_pkt_metadata {
	uint16_t                  sa_idx;
	uint16_t                  esp_head_offset;
	uint8_t                   ol_flags;
	uint8_t                   mode;
	uint8_t                   algo;
};

struct sxe2_ipsec_bitmap {
	struct rte_bitmap *tx_sa_bmp;
	struct rte_bitmap *rx_sa_bmp;
	struct rte_bitmap *rx_tcam_bmp;
	struct rte_bitmap *rx_udp_bmp;
	void *tx_sa_mem;
	void *rx_sa_mem;
	void *rx_tcam_mem;
	void *rx_udp_mem;
};

struct sxe2_ipsec_security_sa {
	uint32_t spi;
	uint16_t hw_idx;
	uint16_t sw_idx;
};

struct sxe2_ipsec_esn {
	union {
		uint64_t value;
		struct {
			uint32_t hi;
			uint32_t low;
		};
	};
	uint8_t enabled;
};

struct sxe2_ipsec_udp {
	struct rte_security_ipsec_udp_param value;
	uint8_t enabled;
};

struct sxe2_ipsec_tx_sa {
	struct rte_security_ipsec_xform xform;
	uint16_t id;
	uint16_t hw_sa_id;
	enum sxe2_ipsec_mode mode;
	enum sxe2_ipsec_algorithm algo;
	uint8_t enc_key[SXE2_IPSEC_MAX_KEY_LEN];
	uint8_t auth_key[SXE2_IPSEC_MAX_KEY_LEN];
};

struct sxe2_ipsec_rx_sa {
	struct rte_security_ipsec_xform xform;
	uint32_t spi;
	uint16_t id;
	uint16_t hw_sa_id;
	uint8_t hw_ip_id;
	uint8_t hw_udp_group_id;
	uint8_t tcam_id;
	uint8_t udp_group_id;
	uint8_t sdn_group_id;
	enum sxe2_ipsec_mode mode;
	enum sxe2_ipsec_algorithm algo;
	uint8_t enc_key[SXE2_IPSEC_MAX_KEY_LEN];
	uint8_t auth_key[SXE2_IPSEC_MAX_KEY_LEN];
};

struct sxe2_ipsec_rx_tcam {
	struct sxe2_ipsec_ip_param ip_addr;
	uint16_t id;
	uint8_t hw_ip_id;
	uint8_t ref_cnt;
};

struct sxe2_ipsec_rx_udp_group {
	uint16_t udp_port;
	uint8_t sport_en;
	uint8_t dport_en;
	uint8_t id;
	uint8_t hw_group_id;
	uint8_t ref_cnt;
};

struct sxe2_ipsec_ctx {
	struct sxe2_ipsec_tx_sa             *tx_sa;
	struct sxe2_ipsec_rx_sa             *rx_sa;
	struct sxe2_ipsec_rx_tcam           *rx_tcam;
	struct sxe2_ipsec_rx_udp_group      *rx_udp_group;
	struct sxe2_ipsec_bitmap            bmp;
	int                                  md_offset;
	uint16_t                                  max_tx_sa;
	uint16_t                                  max_rx_sa;
	uint16_t                                  max_tcam;
	uint8_t                                   max_udp_group;
};

struct sxe2_ipsec_metadata_params {
	uint16_t esp_header_offset;
	uint16_t reserved;
};

bool sxe2_ipsec_supported(struct sxe2_adapter *adapter);

bool sxe2_ipsec_valid_tx_offloads(uint64_t offloads);

bool sxe2_ipsec_valid_rx_offloads(uint64_t offloads);

int sxe2_ipsec_pkt_md_offset_get(struct sxe2_adapter *adapter);

int sxe2_ipsec_session_create(void *device,
			      struct rte_security_session_conf *conf,
			      struct sxe2_security_session *sxe2_sess);

int sxe2_ipsec_session_destroy(void *device,
		struct rte_security_session *session);

int sxe2_ipsec_pkt_metadata_set(void *device, struct rte_security_session *session,
				struct rte_mbuf *m, void *params);

int32_t sxe2_ipsec_init(struct sxe2_adapter *adapter);

void sxe2_ipsec_uinit(struct sxe2_adapter *adapter);

#endif /* __SXE2_IPSEC_H__ */
