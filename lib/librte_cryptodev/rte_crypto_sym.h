/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2016 Intel Corporation. All rights reserved.
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

#ifndef _RTE_CRYPTO_SYM_H_
#define _RTE_CRYPTO_SYM_H_

/**
 * @file rte_crypto_sym.h
 *
 * RTE Definitions for Symmetric Cryptography
 *
 * Defines symmetric cipher and authentication algorithms and modes, as well
 * as supported symmetric crypto operation combinations.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>

#include <rte_mbuf.h>
#include <rte_memory.h>
#include <rte_mempool.h>
#include <rte_common.h>


/** Symmetric Cipher Algorithms */
enum rte_crypto_cipher_algorithm {
	RTE_CRYPTO_CIPHER_NULL = 1,
	/**< NULL cipher algorithm. No mode applies to the NULL algorithm. */

	RTE_CRYPTO_CIPHER_3DES_CBC,
	/**< Triple DES algorithm in CBC mode */
	RTE_CRYPTO_CIPHER_3DES_CTR,
	/**< Triple DES algorithm in CTR mode */
	RTE_CRYPTO_CIPHER_3DES_ECB,
	/**< Triple DES algorithm in ECB mode */

	RTE_CRYPTO_CIPHER_AES_CBC,
	/**< AES algorithm in CBC mode */
	RTE_CRYPTO_CIPHER_AES_CCM,
	/**< AES algorithm in CCM mode. When this cipher algorithm is used the
	 * *RTE_CRYPTO_AUTH_AES_CCM* element of the
	 * *rte_crypto_hash_algorithm* enum MUST be used to set up the related
	 * *rte_crypto_auth_xform* structure in the session context or in
	 * the op_params of the crypto operation structure in the case of a
	 * session-less crypto operation
	 */
	RTE_CRYPTO_CIPHER_AES_CTR,
	/**< AES algorithm in Counter mode */
	RTE_CRYPTO_CIPHER_AES_ECB,
	/**< AES algorithm in ECB mode */
	RTE_CRYPTO_CIPHER_AES_F8,
	/**< AES algorithm in F8 mode */
	RTE_CRYPTO_CIPHER_AES_GCM,
	/**< AES algorithm in GCM mode. When this cipher algorithm is used the
	 * *RTE_CRYPTO_AUTH_AES_GCM* or *RTE_CRYPTO_AUTH_AES_GMAC* element
	 * of the *rte_crypto_auth_algorithm* enum MUST be used to set up
	 * the related *rte_crypto_auth_setup_data* structure in the session
	 * context or in the op_params of the crypto operation structure
	 * in the case of a session-less crypto operation.
	 */
	RTE_CRYPTO_CIPHER_AES_XTS,
	/**< AES algorithm in XTS mode */

	RTE_CRYPTO_CIPHER_ARC4,
	/**< (A)RC4 cipher algorithm */

	RTE_CRYPTO_CIPHER_KASUMI_F8,
	/**< KASUMI algorithm in F8 mode */

	RTE_CRYPTO_CIPHER_SNOW3G_UEA2,
	/**< SNOW 3G algorithm in UEA2 mode */

	RTE_CRYPTO_CIPHER_ZUC_EEA3,
	/**< ZUC algorithm in EEA3 mode */

	RTE_CRYPTO_CIPHER_DES_CBC,
	/**< DES algorithm in CBC mode */

	RTE_CRYPTO_CIPHER_AES_DOCSISBPI,
	/**< AES algorithm using modes required by
	 * DOCSIS Baseline Privacy Plus Spec.
	 * Chained mbufs are not supported in this mode, i.e. rte_mbuf.next
	 * for m_src and m_dst in the rte_crypto_sym_op must be NULL.
	 */

	RTE_CRYPTO_CIPHER_DES_DOCSISBPI,
	/**< DES algorithm using modes required by
	 * DOCSIS Baseline Privacy Plus Spec.
	 * Chained mbufs are not supported in this mode, i.e. rte_mbuf.next
	 * for m_src and m_dst in the rte_crypto_sym_op must be NULL.
	 */

	RTE_CRYPTO_CIPHER_LIST_END

};

/** Cipher algorithm name strings */
extern const char *
rte_crypto_cipher_algorithm_strings[];

/** Symmetric Cipher Direction */
enum rte_crypto_cipher_operation {
	RTE_CRYPTO_CIPHER_OP_ENCRYPT,
	/**< Encrypt cipher operation */
	RTE_CRYPTO_CIPHER_OP_DECRYPT
	/**< Decrypt cipher operation */
};

/** Cipher operation name strings */
extern const char *
rte_crypto_cipher_operation_strings[];

/**
 * Symmetric Cipher Setup Data.
 *
 * This structure contains data relating to Cipher (Encryption and Decryption)
 *  use to create a session.
 */
struct rte_crypto_cipher_xform {
	enum rte_crypto_cipher_operation op;
	/**< This parameter determines if the cipher operation is an encrypt or
	 * a decrypt operation. For the RC4 algorithm and the F8/CTR modes,
	 * only encrypt operations are valid.
	 */
	enum rte_crypto_cipher_algorithm algo;
	/**< Cipher algorithm */

	struct {
		uint8_t *data;	/**< pointer to key data */
		size_t length;	/**< key length in bytes */
	} key;
	/**< Cipher key
	 *
	 * For the RTE_CRYPTO_CIPHER_AES_F8 mode of operation, key.data will
	 * point to a concatenation of the AES encryption key followed by a
	 * keymask. As per RFC3711, the keymask should be padded with trailing
	 * bytes to match the length of the encryption key used.
	 *
	 * For AES-XTS mode of operation, two keys must be provided and
	 * key.data must point to the two keys concatenated together (Key1 ||
	 * Key2). The cipher key length will contain the total size of both
	 * keys.
	 *
	 * Cipher key length is in bytes. For AES it can be 128 bits (16 bytes),
	 * 192 bits (24 bytes) or 256 bits (32 bytes).
	 *
	 * For the CCM mode of operation, the only supported key length is 128
	 * bits (16 bytes).
	 *
	 * For the RTE_CRYPTO_CIPHER_AES_F8 mode of operation, key.length
	 * should be set to the combined length of the encryption key and the
	 * keymask. Since the keymask and the encryption key are the same size,
	 * key.length should be set to 2 x the AES encryption key length.
	 *
	 * For the AES-XTS mode of operation:
	 *  - Two keys must be provided and key.length refers to total length of
	 *    the two keys.
	 *  - Each key can be either 128 bits (16 bytes) or 256 bits (32 bytes).
	 *  - Both keys must have the same size.
	 **/
};

/** Symmetric Authentication / Hash Algorithms */
enum rte_crypto_auth_algorithm {
	RTE_CRYPTO_AUTH_NULL = 1,
	/**< NULL hash algorithm. */

	RTE_CRYPTO_AUTH_AES_CBC_MAC,
	/**< AES-CBC-MAC algorithm. Only 128-bit keys are supported. */
	RTE_CRYPTO_AUTH_AES_CCM,
	/**< AES algorithm in CCM mode. This is an authenticated cipher. When
	 * this hash algorithm is used, the *RTE_CRYPTO_CIPHER_AES_CCM*
	 * element of the *rte_crypto_cipher_algorithm* enum MUST be used to
	 * set up the related rte_crypto_cipher_setup_data structure in the
	 * session context or the corresponding parameter in the crypto
	 * operation data structures op_params parameter MUST be set for a
	 * session-less crypto operation.
	 */
	RTE_CRYPTO_AUTH_AES_CMAC,
	/**< AES CMAC algorithm. */
	RTE_CRYPTO_AUTH_AES_GCM,
	/**< AES algorithm in GCM mode. When this hash algorithm
	 * is used, the RTE_CRYPTO_CIPHER_AES_GCM element of the
	 * rte_crypto_cipher_algorithm enum MUST be used to set up the related
	 * rte_crypto_cipher_setup_data structure in the session context, or
	 * the corresponding parameter in the crypto operation data structures
	 * op_params parameter MUST be set for a session-less crypto operation.
	 */
	RTE_CRYPTO_AUTH_AES_GMAC,
	/**< AES GMAC algorithm. When this hash algorithm
	* is used, the RTE_CRYPTO_CIPHER_AES_GCM element of the
	* rte_crypto_cipher_algorithm enum MUST be used to set up the related
	* rte_crypto_cipher_setup_data structure in the session context,  or
	* the corresponding parameter in the crypto operation data structures
	* op_params parameter MUST be set for a session-less crypto operation.
	*/
	RTE_CRYPTO_AUTH_AES_XCBC_MAC,
	/**< AES XCBC algorithm. */

	RTE_CRYPTO_AUTH_KASUMI_F9,
	/**< KASUMI algorithm in F9 mode. */

	RTE_CRYPTO_AUTH_MD5,
	/**< MD5 algorithm */
	RTE_CRYPTO_AUTH_MD5_HMAC,
	/**< HMAC using MD5 algorithm */

	RTE_CRYPTO_AUTH_SHA1,
	/**< 128 bit SHA algorithm. */
	RTE_CRYPTO_AUTH_SHA1_HMAC,
	/**< HMAC using 128 bit SHA algorithm. */
	RTE_CRYPTO_AUTH_SHA224,
	/**< 224 bit SHA algorithm. */
	RTE_CRYPTO_AUTH_SHA224_HMAC,
	/**< HMAC using 224 bit SHA algorithm. */
	RTE_CRYPTO_AUTH_SHA256,
	/**< 256 bit SHA algorithm. */
	RTE_CRYPTO_AUTH_SHA256_HMAC,
	/**< HMAC using 256 bit SHA algorithm. */
	RTE_CRYPTO_AUTH_SHA384,
	/**< 384 bit SHA algorithm. */
	RTE_CRYPTO_AUTH_SHA384_HMAC,
	/**< HMAC using 384 bit SHA algorithm. */
	RTE_CRYPTO_AUTH_SHA512,
	/**< 512 bit SHA algorithm. */
	RTE_CRYPTO_AUTH_SHA512_HMAC,
	/**< HMAC using 512 bit SHA algorithm. */

	RTE_CRYPTO_AUTH_SNOW3G_UIA2,
	/**< SNOW 3G algorithm in UIA2 mode. */

	RTE_CRYPTO_AUTH_ZUC_EIA3,
	/**< ZUC algorithm in EIA3 mode */

	RTE_CRYPTO_AUTH_LIST_END
};

/** Authentication algorithm name strings */
extern const char *
rte_crypto_auth_algorithm_strings[];

/** Symmetric Authentication / Hash Operations */
enum rte_crypto_auth_operation {
	RTE_CRYPTO_AUTH_OP_VERIFY,	/**< Verify authentication digest */
	RTE_CRYPTO_AUTH_OP_GENERATE	/**< Generate authentication digest */
};

/** Authentication operation name strings */
extern const char *
rte_crypto_auth_operation_strings[];

/**
 * Authentication / Hash transform data.
 *
 * This structure contains data relating to an authentication/hash crypto
 * transforms. The fields op, algo and digest_length are common to all
 * authentication transforms and MUST be set.
 */
struct rte_crypto_auth_xform {
	enum rte_crypto_auth_operation op;
	/**< Authentication operation type */
	enum rte_crypto_auth_algorithm algo;
	/**< Authentication algorithm selection */

	struct {
		uint8_t *data;	/**< pointer to key data */
		size_t length;	/**< key length in bytes */
	} key;
	/**< Authentication key data.
	 * The authentication key length MUST be less than or equal to the
	 * block size of the algorithm. It is the callers responsibility to
	 * ensure that the key length is compliant with the standard being used
	 * (for example RFC 2104, FIPS 198a).
	 */

	uint32_t digest_length;
	/**< Length of the digest to be returned. If the verify option is set,
	 * this specifies the length of the digest to be compared for the
	 * session.
	 *
	 * It is the caller's responsibility to ensure that the
	 * digest length is compliant with the hash algorithm being used.
	 * If the value is less than the maximum length allowed by the hash,
	 * the result shall be truncated.
	 */

	uint32_t add_auth_data_length;
	/**< The length of the additional authenticated data (AAD) in bytes.
	 * The maximum permitted value is 65535 (2^16 - 1) bytes, unless
	 * otherwise specified below.
	 *
	 * This field must be specified when the hash algorithm is one of the
	 * following:
	 *
	 * - For SNOW 3G (@ref RTE_CRYPTO_AUTH_SNOW3G_UIA2), this is the
	 *   length of the IV (which should be 16).
	 *
	 * - For GCM (@ref RTE_CRYPTO_AUTH_AES_GCM).  In this case, this is
	 *   the length of the Additional Authenticated Data (called A, in NIST
	 *   SP800-38D).
	 *
	 * - For CCM (@ref RTE_CRYPTO_AUTH_AES_CCM).  In this case, this is
	 *   the length of the associated data (called A, in NIST SP800-38C).
	 *   Note that this does NOT include the length of any padding, or the
	 *   18 bytes reserved at the start of the above field to store the
	 *   block B0 and the encoded length.  The maximum permitted value in
	 *   this case is 222 bytes.
	 *
	 * @note
	 *  For AES-GMAC (@ref RTE_CRYPTO_AUTH_AES_GMAC) mode of operation
	 *  this field is not used and should be set to 0. Instead the length
	 *  of the AAD data is specified in additional authentication data
	 *  length field of the rte_crypto_sym_op_data structure
	 */
};

/** Crypto transformation types */
enum rte_crypto_sym_xform_type {
	RTE_CRYPTO_SYM_XFORM_NOT_SPECIFIED = 0,	/**< No xform specified */
	RTE_CRYPTO_SYM_XFORM_AUTH,		/**< Authentication xform */
	RTE_CRYPTO_SYM_XFORM_CIPHER		/**< Cipher xform  */
};

/**
 * Symmetric crypto transform structure.
 *
 * This is used to specify the crypto transforms required, multiple transforms
 * can be chained together to specify a chain transforms such as authentication
 * then cipher, or cipher then authentication. Each transform structure can
 * hold a single transform, the type field is used to specify which transform
 * is contained within the union
 */
struct rte_crypto_sym_xform {
	struct rte_crypto_sym_xform *next;
	/**< next xform in chain */
	enum rte_crypto_sym_xform_type type
	; /**< xform type */
	RTE_STD_C11
	union {
		struct rte_crypto_auth_xform auth;
		/**< Authentication / hash xform */
		struct rte_crypto_cipher_xform cipher;
		/**< Cipher xform */
	};
};

/**
 * Crypto operation session type. This is used to specify whether a crypto
 * operation has session structure attached for immutable parameters or if all
 * operation information is included in the operation data structure.
 */
enum rte_crypto_sym_op_sess_type {
	RTE_CRYPTO_SYM_OP_WITH_SESSION,	/**< Session based crypto operation */
	RTE_CRYPTO_SYM_OP_SESSIONLESS	/**< Session-less crypto operation */
};


struct rte_cryptodev_sym_session;

/**
 * Symmetric Cryptographic Operation.
 *
 * This structure contains data relating to performing symmetric cryptographic
 * processing on a referenced mbuf data buffer.
 *
 * When a symmetric crypto operation is enqueued with the device for processing
 * it must have a valid *rte_mbuf* structure attached, via m_src parameter,
 * which contains the source data which the crypto operation is to be performed
 * on.
 * While the mbuf is in use by a crypto operation no part of the mbuf should be
 * changed by the application as the device may read or write to any part of the
 * mbuf. In the case of hardware crypto devices some or all of the mbuf
 * may be DMAed in and out of the device, so writing over the original data,
 * though only the part specified by the rte_crypto_sym_op for transformation
 * will be changed.
 * Out-of-place (OOP) operation, where the source mbuf is different to the
 * destination mbuf, is a special case. Data will be copied from m_src to m_dst.
 * The part copied includes all the parts of the source mbuf that will be
 * operated on, based on the cipher.data.offset+cipher.data.length and
 * auth.data.offset+auth.data.length values in the rte_crypto_sym_op. The part
 * indicated by the cipher parameters will be transformed, any extra data around
 * this indicated by the auth parameters will be copied unchanged from source to
 * destination mbuf.
 * Also in OOP operation the cipher.data.offset and auth.data.offset apply to
 * both source and destination mbufs. As these offsets are relative to the
 * data_off parameter in each mbuf this can result in the data written to the
 * destination buffer being at a different alignment, relative to buffer start,
 * to the data in the source buffer.
 */
struct rte_crypto_sym_op {
	struct rte_mbuf *m_src;	/**< source mbuf */
	struct rte_mbuf *m_dst;	/**< destination mbuf */

	enum rte_crypto_sym_op_sess_type sess_type;

	RTE_STD_C11
	union {
		struct rte_cryptodev_sym_session *session;
		/**< Handle for the initialised session context */
		struct rte_crypto_sym_xform *xform;
		/**< Session-less API crypto operation parameters */
	};

	struct {
		struct {
			uint32_t offset;
			 /**< Starting point for cipher processing, specified
			  * as number of bytes from start of data in the source
			  * buffer. The result of the cipher operation will be
			  * written back into the output buffer starting at
			  * this location.
			  *
			  * @note
			  * For SNOW 3G @ RTE_CRYPTO_CIPHER_SNOW3G_UEA2,
			  * KASUMI @ RTE_CRYPTO_CIPHER_KASUMI_F8
			  * and ZUC @ RTE_CRYPTO_CIPHER_ZUC_EEA3,
			  * this field should be in bits.
			  */

			uint32_t length;
			 /**< The message length, in bytes, of the source buffer
			  * on which the cryptographic operation will be
			  * computed. This must be a multiple of the block size
			  * if a block cipher is being used. This is also the
			  * same as the result length.
			  *
			  * @note
			  * In the case of CCM @ref RTE_CRYPTO_AUTH_AES_CCM,
			  * this value should not include the length of the
			  * padding or the length of the MAC; the driver will
			  * compute the actual number of bytes over which the
			  * encryption will occur, which will include these
			  * values.
			  *
			  * @note
			  * For AES-GMAC @ref RTE_CRYPTO_AUTH_AES_GMAC, this
			  * field should be set to 0.
			  *
			  * @note
			  * For SNOW 3G @ RTE_CRYPTO_AUTH_SNOW3G_UEA2,
			  * KASUMI @ RTE_CRYPTO_CIPHER_KASUMI_F8
			  * and ZUC @ RTE_CRYPTO_CIPHER_ZUC_EEA3,
			  * this field should be in bits.
			  */
		} data; /**< Data offsets and length for ciphering */

		struct {
			uint8_t *data;
			/**< Initialisation Vector or Counter.
			 *
			 * - For block ciphers in CBC or F8 mode, or for KASUMI
			 * in F8 mode, or for SNOW 3G in UEA2 mode, this is the
			 * Initialisation Vector (IV) value.
			 *
			 * - For block ciphers in CTR mode, this is the counter.
			 *
			 * - For GCM mode, this is either the IV (if the length
			 * is 96 bits) or J0 (for other sizes), where J0 is as
			 * defined by NIST SP800-38D. Regardless of the IV
			 * length, a full 16 bytes needs to be allocated.
			 *
			 * - For CCM mode, the first byte is reserved, and the
			 * nonce should be written starting at &iv[1] (to allow
			 * space for the implementation to write in the flags
			 * in the first byte). Note that a full 16 bytes should
			 * be allocated, even though the length field will
			 * have a value less than this.
			 *
			 * - For AES-XTS, this is the 128bit tweak, i, from
			 * IEEE Std 1619-2007.
			 *
			 * For optimum performance, the data pointed to SHOULD
			 * be 8-byte aligned.
			 */
			phys_addr_t phys_addr;
			uint16_t length;
			/**< Length of valid IV data.
			 *
			 * - For block ciphers in CBC or F8 mode, or for KASUMI
			 * in F8 mode, or for SNOW 3G in UEA2 mode, this is the
			 * length of the IV (which must be the same as the
			 * block length of the cipher).
			 *
			 * - For block ciphers in CTR mode, this is the length
			 * of the counter (which must be the same as the block
			 * length of the cipher).
			 *
			 * - For GCM mode, this is either 12 (for 96-bit IVs)
			 * or 16, in which case data points to J0.
			 *
			 * - For CCM mode, this is the length of the nonce,
			 * which can be in the range 7 to 13 inclusive.
			 */
		} iv;	/**< Initialisation vector parameters */
	} cipher;

	struct {
		struct {
			uint32_t offset;
			 /**< Starting point for hash processing, specified as
			  * number of bytes from start of packet in source
			  * buffer.
			  *
			  * @note
			  * For CCM and GCM modes of operation, this field is
			  * ignored. The field @ref aad field
			  * should be set instead.
			  *
			  * @note For AES-GMAC (@ref RTE_CRYPTO_AUTH_AES_GMAC)
			  * mode of operation, this field is set to 0. aad data
			  * pointer of rte_crypto_sym_op_data structure is
			  * used instead
			  *
			  * @note
			  * For SNOW 3G @ RTE_CRYPTO_AUTH_SNOW3G_UIA2,
			  * KASUMI @ RTE_CRYPTO_AUTH_KASUMI_F9
			  * and ZUC @ RTE_CRYPTO_AUTH_ZUC_EIA3,
			  * this field should be in bits.
			  */

			uint32_t length;
			 /**< The message length, in bytes, of the source
			  * buffer that the hash will be computed on.
			  *
			  * @note
			  * For CCM and GCM modes of operation, this field is
			  * ignored. The field @ref aad field should be set
			  * instead.
			  *
			  * @note
			  * For AES-GMAC @ref RTE_CRYPTO_AUTH_AES_GMAC mode
			  * of operation, this field is set to 0.
			  * Auth.aad.length is used instead.
			  *
			  * @note
			  * For SNOW 3G @ RTE_CRYPTO_AUTH_SNOW3G_UIA2,
			  * KASUMI @ RTE_CRYPTO_AUTH_KASUMI_F9
			  * and ZUC @ RTE_CRYPTO_AUTH_ZUC_EIA3,
			  * this field should be in bits.
			  */
		} data; /**< Data offsets and length for authentication */

		struct {
			uint8_t *data;
			/**< This points to the location where the digest result
			 * should be inserted (in the case of digest generation)
			 * or where the purported digest exists (in the case of
			 * digest verification).
			 *
			 * At session creation time, the client specified the
			 * digest result length with the digest_length member
			 * of the @ref rte_crypto_auth_xform structure. For
			 * physical crypto devices the caller must allocate at
			 * least digest_length of physically contiguous memory
			 * at this location.
			 *
			 * For digest generation, the digest result will
			 * overwrite any data at this location.
			 *
			 * @note
			 * For GCM (@ref RTE_CRYPTO_AUTH_AES_GCM), for
			 * "digest result" read "authentication tag T".
			 */
			phys_addr_t phys_addr;
			/**< Physical address of digest */
			uint16_t length;
			/**< Length of digest. This must be the same value as
			 * @ref rte_crypto_auth_xform.digest_length.
			 */
		} digest; /**< Digest parameters */

		struct {
			uint8_t *data;
			/**< Pointer to Additional Authenticated Data (AAD)
			 * needed for authenticated cipher mechanisms (CCM and
			 * GCM), and to the IV for SNOW 3G authentication
			 * (@ref RTE_CRYPTO_AUTH_SNOW3G_UIA2). For other
			 * authentication mechanisms this pointer is ignored.
			 *
			 * The length of the data pointed to by this field is
			 * set up for the session in the @ref
			 * rte_crypto_auth_xform structure as part of the @ref
			 * rte_cryptodev_sym_session_create function call.
			 * This length must not exceed 65535 (2^16-1) bytes.
			 *
			 * Specifically for CCM (@ref RTE_CRYPTO_AUTH_AES_CCM),
			 * the caller should setup this field as follows:
			 *
			 * - the nonce should be written starting at an offset
			 * of one byte into the array, leaving room for the
			 * implementation to write in the flags to the first
			 *  byte.
			 *
			 * - the additional  authentication data itself should
			 * be written starting at an offset of 18 bytes into
			 * the array, leaving room for the length encoding in
			 * the first two bytes of the second block.
			 *
			 * - the array should be big enough to hold the above
			 *  fields, plus any padding to round this up to the
			 *  nearest multiple of the block size (16 bytes).
			 *  Padding will be added by the implementation.
			 *
			 * Finally, for GCM (@ref RTE_CRYPTO_AUTH_AES_GCM), the
			 * caller should setup this field as follows:
			 *
			 * - the AAD is written in starting at byte 0
			 * - the array must be big enough to hold the AAD, plus
			 * any space to round this up to the nearest multiple
			 * of the block size (16 bytes).
			 *
			 * @note
			 * For AES-GMAC (@ref RTE_CRYPTO_AUTH_AES_GMAC) mode of
			 * operation, this field is used to pass plaintext.
			 */
			phys_addr_t phys_addr;	/**< physical address */
			uint16_t length;
			/**< Length of additional authenticated data (AAD)
			 * in bytes
			 */
		} aad;
		/**< Additional authentication parameters */
	} auth;
} __rte_cache_aligned;


/**
 * Reset the fields of a symmetric operation to their default values.
 *
 * @param	op	The crypto operation to be reset.
 */
static inline void
__rte_crypto_sym_op_reset(struct rte_crypto_sym_op *op)
{
	memset(op, 0, sizeof(*op));

	op->sess_type = RTE_CRYPTO_SYM_OP_SESSIONLESS;
}


/**
 * Allocate space for symmetric crypto xforms in the private data space of the
 * crypto operation. This also defaults the crypto xform type to
 * RTE_CRYPTO_SYM_XFORM_NOT_SPECIFIED and configures the chaining of the xforms
 * in the crypto operation
 *
 * @return
 * - On success returns pointer to first crypto xform in crypto operations chain
 * - On failure returns NULL
 */
static inline struct rte_crypto_sym_xform *
__rte_crypto_sym_op_sym_xforms_alloc(struct rte_crypto_sym_op *sym_op,
		void *priv_data, uint8_t nb_xforms)
{
	struct rte_crypto_sym_xform *xform;

	sym_op->xform = xform = (struct rte_crypto_sym_xform *)priv_data;

	do {
		xform->type = RTE_CRYPTO_SYM_XFORM_NOT_SPECIFIED;
		xform = xform->next = --nb_xforms > 0 ? xform + 1 : NULL;
	} while (xform);

	return sym_op->xform;
}


/**
 * Attach a session to a symmetric crypto operation
 *
 * @param	sym_op	crypto operation
 * @param	sess	cryptodev session
 */
static inline int
__rte_crypto_sym_op_attach_sym_session(struct rte_crypto_sym_op *sym_op,
		struct rte_cryptodev_sym_session *sess)
{
	sym_op->session = sess;
	sym_op->sess_type = RTE_CRYPTO_SYM_OP_WITH_SESSION;

	return 0;
}


#ifdef __cplusplus
}
#endif

#endif /* _RTE_CRYPTO_SYM_H_ */
