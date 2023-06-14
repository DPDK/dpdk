/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Intel Corporation
 */
#ifndef __INCLUDE_RTE_SWX_IPSEC_H__
#define __INCLUDE_RTE_SWX_IPSEC_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 * RTE SWX Internet Protocol Security (IPsec)
 *
 * The IPsec block is a companion block for the SWX pipeline used to provide IPsec support to the
 * pipeline. The block is external to the pipeline, hence it needs to be explicitly instantiated by
 * the user and connected to a pipeline instance through the pipeline I/O ports.
 *
 * Main features:
 * - IPsec inbound (encrypted input packets -> clear text output packets) and outbound (clear text
 *   input packets -> encrypted output packets) processing support for tunnel and transport modes.
 *
 * Security Association (SA):
 * - Each IPsec block instance has its own set of SAs used to process the input packets. Each SA is
 *   identified by its unique SA ID. The IPsec inbound and outbound SAs share the same ID space.
 * - Each input packet is first mapped to one of the existing SAs by using the SA ID and then
 *   processed according to the identified SA. The SA ID is read from input packet. The SA ID field
 *   is typically written by the pipeline before sending the packet to the IPsec block.
 *
 * Packet format:
 * - IPsec block input packet (i.e. pipeline output packet):
 *	- IPsec block meta-data header: @see struct rte_swx_ipsec_input_packet_metadata.
 *	- IPv4 header.
 *	- IPv4 payload: on the inbound path, it includes the encrypted ESP packet.
 * - IPsec block output packet (i.e. pipeline input packet):
 *	- IPv4 header.
 *	- IPv4 payload: on the outbound path, it includes the encrypted ESP packet.
 *
 * SA update procedure:
 * - To add a new SA, @see function rte_swx_ipsec_sa_add().
 * - To delete an existing SA, @see function rte_swx_ipsec_sa_delete().
 * - To update an existing SA, the control plane has to follow the following steps:
 *   1. Add a new SA with potentially a different set of configuration parameters. This step can
 *      fail, for example when the SA table is full.
 *   2. Wait until no more packets are using the old SA.
 *   3. Delete the old SA.
 */

#include <stdint.h>
#include <stdio.h>
#include <netinet/in.h>

#include <rte_compat.h>
#include <rte_crypto_sym.h>

/**
 * IPsec Setup API
 */

/** IPsec instance opaque data structure. */
struct rte_swx_ipsec;

/** Name size. */
#ifndef RTE_SWX_IPSEC_NAME_SIZE
#define RTE_SWX_IPSEC_NAME_SIZE 64
#endif

/** Maximum burst size. */
#ifndef RTE_SWX_IPSEC_BURST_SIZE_MAX
#define RTE_SWX_IPSEC_BURST_SIZE_MAX 256
#endif

/** IPsec burst sizes. */
struct rte_swx_ipsec_burst_size {
	/** Input ring read burst size. */
	uint32_t ring_rd;

	/** Output ring write burst size. */
	uint32_t ring_wr;

	/** Crypto device request queue write burst size. */
	uint32_t crypto_wr;

	/** Crypto device response queue read burst size. */
	uint32_t crypto_rd;
};

/**
 * IPsec instance configuration parameters
 */
struct rte_swx_ipsec_params {
	/** Input packet queue. */
	const char *ring_in_name;

	/** Output packet queue.  */
	const char *ring_out_name;

	/** Crypto device name. */
	const char *crypto_dev_name;

	/** Crypto device queue pair ID. */
	uint32_t crypto_dev_queue_pair_id;

	/** Burst size. */
	struct rte_swx_ipsec_burst_size bsz;

	/** Maximum number of SAs. */
	uint32_t n_sa_max;
};

/**
 * IPsec input packet meta-data
 */
struct rte_swx_ipsec_input_packet_metadata {
	/* SA ID. */
	uint32_t sa_id;
};

/**
 * IPsec instance find
 *
 * @param[in] name
 *   IPsec instance name.
 * @return
 *   Valid IPsec instance handle if found or NULL otherwise.
 */
__rte_experimental
struct rte_swx_ipsec *
rte_swx_ipsec_find(const char *name);

/**
 * IPsec instance create
 *
 * @param[out] ipsec
 *   IPsec instance handle. Must point to valid memory. Contains valid pipeline handle once this
 *   function returns successfully.
 * @param[in] name
 *   IPsec instance unique name.
 * @param[in] params
 *   IPsec instance configuration parameters.
 * @param[in] numa_node
 *   Non-Uniform Memory Access (NUMA) node.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument;
 *   -ENOMEM: Not enough space/cannot allocate memory;
 *   -EEXIST: Pipeline with this name already exists.
 */
__rte_experimental
int
rte_swx_ipsec_create(struct rte_swx_ipsec **ipsec,
		     const char *name,
		     struct rte_swx_ipsec_params *params,
		     int numa_node);

/**
 * IPsec instance free
 *
 * @param[in] ipsec
 *   IPsec instance handle.
 */
__rte_experimental
void
rte_swx_ipsec_free(struct rte_swx_ipsec *ipsec);

/**
 * IPsec Data Plane API
 */

/**
 * IPsec instance run
 *
 * @param[in] ipsec
 *   IPsec instance handle.
 */
__rte_experimental
void
rte_swx_ipsec_run(struct rte_swx_ipsec *ipsec);

/*
 * IPsec Control Plane API
 */

/** Maximum key size in bytes. */
#define RTE_SWX_IPSEC_KEY_SIZE_MAX 64

/** IPsec SA crypto cipher parameters. */
struct rte_swx_ipsec_sa_cipher_params {
	/** Cipher algorithm. */
	enum rte_crypto_cipher_algorithm alg;

	/** Cipher key. */
	uint8_t key[RTE_SWX_IPSEC_KEY_SIZE_MAX];

	/** Cipher key size in bytes. */
	uint32_t key_size;
};

/** IPsec SA crypto authentication parameters. */
struct rte_swx_ipsec_sa_authentication_params {
	/** Authentication algorithm. */
	enum rte_crypto_auth_algorithm alg;

	/** Authentication key. */
	uint8_t key[RTE_SWX_IPSEC_KEY_SIZE_MAX];

	/** Authentication key size in bytes. */
	uint32_t key_size;
};

/** IPsec SA crypto Authenticated Encryption with Associated Data (AEAD) parameters. */
struct rte_swx_ipsec_sa_aead_params {
	/** AEAD algorithm. */
	enum rte_crypto_aead_algorithm alg;

	/** AEAD key. */
	uint8_t key[RTE_SWX_IPSEC_KEY_SIZE_MAX];

	/** AEAD key size in bytes. */
	uint32_t key_size;
};

/** IPsec protocol encapsulation parameters. */
struct rte_swx_ipsec_sa_encap_params {
	/** Encapsulating Security Payload (ESP) header. */
	struct {
		/** Security Parameters Index (SPI) field. */
		uint32_t spi;
	} esp;

	/** Tunnel mode when non-zero, transport mode when zero. */
	int tunnel_mode;

	/** Tunnel type: Non-zero for IPv4, zero for IPv6. Valid for tunnel mode only. */
	int tunnel_ipv4;

	/** Tunnel parameters. Valid for tunnel mode only. */
	union {
		/** IPv4 header. */
		struct {
			/** Source address. */
			struct in_addr src_addr;

			/** Destination address. */
			struct in_addr dst_addr;
		} ipv4;

		/** IPv6 header. */
		struct {
			/** Source address. */
			struct in6_addr src_addr;

			/** Destination address. */
			struct in6_addr dst_addr;
		} ipv6;
	} tunnel;
};

/** IPsec Security Association (SA) parameters. */
struct rte_swx_ipsec_sa_params {
	/** Crypto operation: encrypt when non-zero, decrypt when zero. */
	int encrypt;

	/** Crypto operation parameters. */
	struct {
		RTE_STD_C11
		union {
			struct {
				/** Crypto cipher operation parameters. */
				struct rte_swx_ipsec_sa_cipher_params cipher;

				/** Crypto authentication operation parameters. */
				struct rte_swx_ipsec_sa_authentication_params auth;
			} cipher_auth;

			/** Crypto AEAD operation parameters. */
			struct rte_swx_ipsec_sa_aead_params aead;
		};

		/** Non-zero for AEAD, zero for cipher & authentication. */
		int is_aead;
	} crypto;

	/** Packet encasulation parameters. */
	struct rte_swx_ipsec_sa_encap_params encap;
};

/**
 * IPsec SA add
 *
 * @param[in] ipsec
 *   IPsec instance handle.
 * @param[in] sa_params
 *   SA parameters.
 * @param[out] sa_id
 *   On success, the SA ID.
 * @return
 *   0 on success or error code otherwise.
 */
__rte_experimental
int
rte_swx_ipsec_sa_add(struct rte_swx_ipsec *ipsec,
		     struct rte_swx_ipsec_sa_params *sa_params,
		     uint32_t *sa_id);

/**
 * IPsec SA delete
 *
 * It is the responibility of the Control Plane to make sure the SA to be deleted is no longer used
 * by the Data Plane.
 *
 * @param[in] ipsec
 *   IPsec instance handle.
 * @param[in] sa_id
 *   The SA ID.
 */
__rte_experimental
void
rte_swx_ipsec_sa_delete(struct rte_swx_ipsec *ipsec,
			uint32_t sa_id);

/**
 * IPsec SA read from string
 *
 * IPsec SA syntax:
 *
 * \<sa>
 *    : encrypt \<crypto_params> \<encap_params>
 *    | decrypt \<crypto_params> \<encap_params>
 *    ;
 *
 * \<crypto_params>
 *    : \<cipher> \<auth>
 *    | \<aead>
 *    ;
 *
 * \<cipher>
 *    : cipher \<ciher_alg> key \<cipher_key>
 *    | cipher \<cipher_alg>
 *    ;
 *
 * \<auth>
 *    : auth \<authentication_alg> key \<authentication_key>
 *    | auth \<authentication_alg>
 *    ;
 *
 * \<aead>
 *    : aead \<aead_alg> key \<aead_key>
 *    ;
 *
 * \<encap_params>
 *    : esp spi \<spi> tunnel ipv4 srcaddr \<ipv4_src_addr> dstaddr \<ipv4_dst_addr>
 *    | esp spi \<spi> tunnel ipv6 srcaddr \<ipv6_src_addr> dstaddr \<ipv6_dst_addr>
 *    | esp spi \<spi> transport
 *    ;
 *
 * @param[in] ipsec
 *   IPsec instance handle.
 * @param[in] string
 *   String containing the SA.
 * @param[in,out] is_blank_or_comment
 *   On error, when its input value is not NULL, this argument is set to a non-zero value when
 *   *string* contains a blank or comment line and to zero otherwise.
 * @param[in,out] errmsg
 *   On error, when its input value is not NULL, this argument points to a string with details on
 *   the detected error.
 * @return
 *   Pointer to valid IPsec SA parameters data structure on success or NULL on error.
 */
__rte_experimental
struct rte_swx_ipsec_sa_params *
rte_swx_ipsec_sa_read(struct rte_swx_ipsec *ipsec,
		      const char *string,
		      int *is_blank_or_comment,
		      const char **errmsg);

#ifdef __cplusplus
}
#endif

#endif
