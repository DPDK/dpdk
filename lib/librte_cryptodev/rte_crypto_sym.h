/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2020 Intel Corporation
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

/**
 * Crypto IO Vector (in analogy with struct iovec)
 * Supposed be used to pass input/output data buffers for crypto data-path
 * functions.
 */
struct rte_crypto_vec {
	/** virtual address of the data buffer */
	void *base;
	/** IOVA of the data buffer */
	rte_iova_t iova;
	/** length of the data buffer */
	uint32_t len;
};

/**
 * Crypto scatter-gather list descriptor. Consists of a pointer to an array
 * of Crypto IO vectors with its size.
 */
struct rte_crypto_sgl {
	/** start of an array of vectors */
	struct rte_crypto_vec *vec;
	/** size of an array of vectors */
	uint32_t num;
};

/**
 * Crypto virtual and IOVA address descriptor, used to describe cryptographic
 * data buffer without the length information. The length information is
 * normally predefined during session creation.
 */
struct rte_crypto_va_iova_ptr {
	void *va;
	rte_iova_t iova;
};

/**
 * Raw data operation descriptor.
 * Supposed to be used with synchronous CPU crypto API call or asynchronous
 * RAW data path API call.
 */
struct rte_crypto_sym_vec {
	/** number of operations to perform */
	uint32_t num;
	/** array of SGL vectors */
	struct rte_crypto_sgl *sgl;
	/** array of pointers to cipher IV */
	struct rte_crypto_va_iova_ptr *iv;
	/** array of pointers to digest */
	struct rte_crypto_va_iova_ptr *digest;

	__extension__
	union {
		/** array of pointers to auth IV, used for chain operation */
		struct rte_crypto_va_iova_ptr *auth_iv;
		/** array of pointers to AAD, used for AEAD operation */
		struct rte_crypto_va_iova_ptr *aad;
	};

	/**
	 * array of statuses for each operation:
	 * - 0 on success
	 * - errno on error
	 */
	int32_t *status;
};

/**
 * used for cpu_crypto_process_bulk() to specify head/tail offsets
 * for auth/cipher processing.
 */
union rte_crypto_sym_ofs {
	uint64_t raw;
	struct {
		struct {
			uint16_t head;
			uint16_t tail;
		} auth, cipher;
	} ofs;
};

/** Symmetric Cipher Algorithms
 *
 * Note, to avoid ABI breakage across releases
 * - LIST_END should not be added to this enum
 * - the order of enums should not be changed
 * - new algorithms should only be added to the end
 */
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
	RTE_CRYPTO_CIPHER_AES_CTR,
	/**< AES algorithm in Counter mode */
	RTE_CRYPTO_CIPHER_AES_ECB,
	/**< AES algorithm in ECB mode */
	RTE_CRYPTO_CIPHER_AES_F8,
	/**< AES algorithm in F8 mode */
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

	RTE_CRYPTO_CIPHER_DES_DOCSISBPI
	/**< DES algorithm using modes required by
	 * DOCSIS Baseline Privacy Plus Spec.
	 * Chained mbufs are not supported in this mode, i.e. rte_mbuf.next
	 * for m_src and m_dst in the rte_crypto_sym_op must be NULL.
	 */
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
		const uint8_t *data;	/**< pointer to key data */
		uint16_t length;	/**< key length in bytes */
	} key;
	/**< Cipher key
	 *
	 * For the RTE_CRYPTO_CIPHER_AES_F8 mode of operation, key.data will
	 * point to a concatenation of the AES encryption key followed by a
	 * keymask. As per RFC3711, the keymask should be padded with trailing
	 * bytes to match the length of the encryption key used.
	 *
	 * Cipher key length is in bytes. For AES it can be 128 bits (16 bytes),
	 * 192 bits (24 bytes) or 256 bits (32 bytes).
	 *
	 * For the RTE_CRYPTO_CIPHER_AES_F8 mode of operation, key.length
	 * should be set to the combined length of the encryption key and the
	 * keymask. Since the keymask and the encryption key are the same size,
	 * key.length should be set to 2 x the AES encryption key length.
	 *
	 * For the AES-XTS mode of operation:
	 *  - Two keys must be provided and key.length refers to total length of
	 *    the two keys.
	 *  - key.data must point to the two keys concatenated together
	 *    (key1 || key2).
	 *  - Each key can be either 128 bits (16 bytes) or 256 bits (32 bytes).
	 *  - Both keys must have the same size.
	 **/
	struct {
		uint16_t offset;
		/**< Starting point for Initialisation Vector or Counter,
		 * specified as number of bytes from start of crypto
		 * operation (rte_crypto_op).
		 *
		 * - For block ciphers in CBC or F8 mode, or for KASUMI
		 * in F8 mode, or for SNOW 3G in UEA2 mode, this is the
		 * Initialisation Vector (IV) value.
		 *
		 * - For block ciphers in CTR mode, this is the counter.
		 *
		 * - For CCM mode, the first byte is reserved, and the
		 * nonce should be written starting at &iv[1] (to allow
		 * space for the implementation to write in the flags
		 * in the first byte). Note that a full 16 bytes should
		 * be allocated, even though the length field will
		 * have a value less than this. Note that the PMDs may
		 * modify the memory reserved (the first byte and the
		 * final padding)
		 *
		 * - For AES-XTS, this is the 128bit tweak, i, from
		 * IEEE Std 1619-2007.
		 *
		 * For optimum performance, the data pointed to SHOULD
		 * be 8-byte aligned.
		 */
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
		 * - For CCM mode, this is the length of the nonce,
		 * which can be in the range 7 to 13 inclusive.
		 */
	} iv;	/**< Initialisation vector parameters */
};

/** Symmetric Authentication / Hash Algorithms
 *
 * Note, to avoid ABI breakage across releases
 * - LIST_END should not be added to this enum
 * - the order of enums should not be changed
 * - new algorithms should only be added to the end
 */
enum rte_crypto_auth_algorithm {
	RTE_CRYPTO_AUTH_NULL = 1,
	/**< NULL hash algorithm. */

	RTE_CRYPTO_AUTH_AES_CBC_MAC,
	/**< AES-CBC-MAC algorithm. Only 128-bit keys are supported. */
	RTE_CRYPTO_AUTH_AES_CMAC,
	/**< AES CMAC algorithm. */
	RTE_CRYPTO_AUTH_AES_GMAC,
	/**< AES GMAC algorithm. */
	RTE_CRYPTO_AUTH_AES_XCBC_MAC,
	/**< AES XCBC algorithm. */

	RTE_CRYPTO_AUTH_KASUMI_F9,
	/**< KASUMI algorithm in F9 mode. */

	RTE_CRYPTO_AUTH_MD5,
	/**< MD5 algorithm */
	RTE_CRYPTO_AUTH_MD5_HMAC,
	/**< HMAC using MD5 algorithm */

	RTE_CRYPTO_AUTH_SHA1,
	/**< 160 bit SHA algorithm. */
	RTE_CRYPTO_AUTH_SHA1_HMAC,
	/**< HMAC using 160 bit SHA algorithm.
	 * HMAC-SHA-1-96 can be generated by setting
	 * digest_length to 12 bytes in auth/aead xforms.
	 */
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

	RTE_CRYPTO_AUTH_SHA3_224,
	/**< 224 bit SHA3 algorithm. */
	RTE_CRYPTO_AUTH_SHA3_224_HMAC,
	/**< HMAC using 224 bit SHA3 algorithm. */
	RTE_CRYPTO_AUTH_SHA3_256,
	/**< 256 bit SHA3 algorithm. */
	RTE_CRYPTO_AUTH_SHA3_256_HMAC,
	/**< HMAC using 256 bit SHA3 algorithm. */
	RTE_CRYPTO_AUTH_SHA3_384,
	/**< 384 bit SHA3 algorithm. */
	RTE_CRYPTO_AUTH_SHA3_384_HMAC,
	/**< HMAC using 384 bit SHA3 algorithm. */
	RTE_CRYPTO_AUTH_SHA3_512,
	/**< 512 bit SHA3 algorithm. */
	RTE_CRYPTO_AUTH_SHA3_512_HMAC
	/**< HMAC using 512 bit SHA3 algorithm. */
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
		const uint8_t *data;	/**< pointer to key data */
		uint16_t length;	/**< key length in bytes */
	} key;
	/**< Authentication key data.
	 * The authentication key length MUST be less than or equal to the
	 * block size of the algorithm. It is the callers responsibility to
	 * ensure that the key length is compliant with the standard being used
	 * (for example RFC 2104, FIPS 198a).
	 */

	struct {
		uint16_t offset;
		/**< Starting point for Initialisation Vector or Counter,
		 * specified as number of bytes from start of crypto
		 * operation (rte_crypto_op).
		 *
		 * - For SNOW 3G in UIA2 mode, for ZUC in EIA3 mode
		 *   this is the authentication Initialisation Vector
		 *   (IV) value. For AES-GMAC IV description please refer
		 *   to the field `length` in iv struct.
		 *
		 * - For KASUMI in F9 mode and other authentication
		 *   algorithms, this field is not used.
		 *
		 * For optimum performance, the data pointed to SHOULD
		 * be 8-byte aligned.
		 */
		uint16_t length;
		/**< Length of valid IV data.
		 *
		 * - For SNOW3G in UIA2 mode, for ZUC in EIA3 mode and
		 *   for AES-GMAC, this is the length of the IV.
		 *
		 * - For KASUMI in F9 mode and other authentication
		 *   algorithms, this field is not used.
		 *
		 * - For GMAC mode, this is either:
		 * 1) Number greater or equal to one, which means that IV
		 *    is used and J0 will be computed internally, a minimum
		 *    of 16 bytes must be allocated.
		 * 2) Zero, in which case data points to J0. In this case
		 *    16 bytes of J0 should be passed where J0 is defined
		 *    by NIST SP800-38D.
		 *
		 */
	} iv;	/**< Initialisation vector parameters */

	uint16_t digest_length;
	/**< Length of the digest to be returned. If the verify option is set,
	 * this specifies the length of the digest to be compared for the
	 * session.
	 *
	 * It is the caller's responsibility to ensure that the
	 * digest length is compliant with the hash algorithm being used.
	 * If the value is less than the maximum length allowed by the hash,
	 * the result shall be truncated.
	 */
};


/** Symmetric AEAD Algorithms
 *
 * Note, to avoid ABI breakage across releases
 * - LIST_END should not be added to this enum
 * - the order of enums should not be changed
 * - new algorithms should only be added to the end
 */
enum rte_crypto_aead_algorithm {
	RTE_CRYPTO_AEAD_AES_CCM = 1,
	/**< AES algorithm in CCM mode. */
	RTE_CRYPTO_AEAD_AES_GCM,
	/**< AES algorithm in GCM mode. */
	RTE_CRYPTO_AEAD_CHACHA20_POLY1305
	/**< Chacha20 cipher with poly1305 authenticator */
};

/** AEAD algorithm name strings */
extern const char *
rte_crypto_aead_algorithm_strings[];

/** Symmetric AEAD Operations */
enum rte_crypto_aead_operation {
	RTE_CRYPTO_AEAD_OP_ENCRYPT,
	/**< Encrypt and generate digest */
	RTE_CRYPTO_AEAD_OP_DECRYPT
	/**< Verify digest and decrypt */
};

/** Authentication operation name strings */
extern const char *
rte_crypto_aead_operation_strings[];

struct rte_crypto_aead_xform {
	enum rte_crypto_aead_operation op;
	/**< AEAD operation type */
	enum rte_crypto_aead_algorithm algo;
	/**< AEAD algorithm selection */

	struct {
		const uint8_t *data;	/**< pointer to key data */
		uint16_t length;	/**< key length in bytes */
	} key;

	struct {
		uint16_t offset;
		/**< Starting point for Initialisation Vector or Counter,
		 * specified as number of bytes from start of crypto
		 * operation (rte_crypto_op).
		 *
		 * - For CCM mode, the first byte is reserved, and the
		 * nonce should be written starting at &iv[1] (to allow
		 * space for the implementation to write in the flags
		 * in the first byte). Note that a full 16 bytes should
		 * be allocated, even though the length field will
		 * have a value less than this.
		 *
		 * - For Chacha20-Poly1305 it is 96-bit nonce.
		 * PMD sets initial counter for Poly1305 key generation
		 * part to 0 and for Chacha20 encryption to 1 as per
		 * rfc8439 2.8. AEAD construction.
		 *
		 * For optimum performance, the data pointed to SHOULD
		 * be 8-byte aligned.
		 */
		uint16_t length;
		/**< Length of valid IV data.
		 *
		 * - For GCM mode, this is either:
		 * 1) Number greater or equal to one, which means that IV
		 *    is used and J0 will be computed internally, a minimum
		 *    of 16 bytes must be allocated.
		 * 2) Zero, in which case data points to J0. In this case
		 *    16 bytes of J0 should be passed where J0 is defined
		 *    by NIST SP800-38D.
		 *
		 * - For CCM mode, this is the length of the nonce,
		 * which can be in the range 7 to 13 inclusive.
		 *
		 * - For Chacha20-Poly1305 this field is always 12.
		 */
	} iv;	/**< Initialisation vector parameters */

	uint16_t digest_length;

	uint16_t aad_length;
	/**< The length of the additional authenticated data (AAD) in bytes.
	 * For CCM mode, this is the length of the actual AAD, even though
	 * it is required to reserve 18 bytes before the AAD and padding
	 * at the end of it, so a multiple of 16 bytes is allocated.
	 */
};

/** Crypto transformation types */
enum rte_crypto_sym_xform_type {
	RTE_CRYPTO_SYM_XFORM_NOT_SPECIFIED = 0,	/**< No xform specified */
	RTE_CRYPTO_SYM_XFORM_AUTH,		/**< Authentication xform */
	RTE_CRYPTO_SYM_XFORM_CIPHER,		/**< Cipher xform  */
	RTE_CRYPTO_SYM_XFORM_AEAD		/**< AEAD xform  */
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
		struct rte_crypto_aead_xform aead;
		/**< AEAD xform */
	};
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

	RTE_STD_C11
	union {
		struct rte_cryptodev_sym_session *session;
		/**< Handle for the initialised session context */
		struct rte_crypto_sym_xform *xform;
		/**< Session-less API crypto operation parameters */
		struct rte_security_session *sec_session;
		/**< Handle for the initialised security session context */
	};

	RTE_STD_C11
	union {
		struct {
			struct {
				uint32_t offset;
				 /**< Starting point for AEAD processing, specified as
				  * number of bytes from start of packet in source
				  * buffer.
				  */
				uint32_t length;
				 /**< The message length, in bytes, of the source buffer
				  * on which the cryptographic operation will be
				  * computed. This must be a multiple of the block size
				  */
			} data; /**< Data offsets and length for AEAD */
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
				 * For GCM (@ref RTE_CRYPTO_AEAD_AES_GCM), for
				 * "digest result" read "authentication tag T".
				 */
				rte_iova_t phys_addr;
				/**< Physical address of digest */
			} digest; /**< Digest parameters */
			struct {
				uint8_t *data;
				/**< Pointer to Additional Authenticated Data (AAD)
				 * needed for authenticated cipher mechanisms (CCM and
				 * GCM)
				 *
				 * Specifically for CCM (@ref RTE_CRYPTO_AEAD_AES_CCM),
				 * the caller should setup this field as follows:
				 *
				 * - the additional authentication data itself should
				 * be written starting at an offset of 18 bytes into
				 * the array, leaving room for the first block (16 bytes)
				 * and the length encoding in the first two bytes of the
				 * second block.
				 *
				 * - the array should be big enough to hold the above
				 * fields, plus any padding to round this up to the
				 * nearest multiple of the block size (16 bytes).
				 * Padding will be added by the implementation.
				 *
				 * - Note that PMDs may modify the memory reserved
				 * (first 18 bytes and the final padding).
				 *
				 * Finally, for GCM (@ref RTE_CRYPTO_AEAD_AES_GCM), the
				 * caller should setup this field as follows:
				 *
				 * - the AAD is written in starting at byte 0
				 * - the array must be big enough to hold the AAD, plus
				 * any space to round this up to the nearest multiple
				 * of the block size (16 bytes).
				 *
				 */
				rte_iova_t phys_addr;	/**< physical address */
			} aad;
			/**< Additional authentication parameters */
		} aead;

		struct {
			struct {
				struct {
					uint32_t offset;
					 /**< Starting point for cipher processing,
					  * specified as number of bytes from start
					  * of data in the source buffer.
					  * The result of the cipher operation will be
					  * written back into the output buffer
					  * starting at this location.
					  *
					  * @note
					  * For SNOW 3G @ RTE_CRYPTO_CIPHER_SNOW3G_UEA2,
					  * KASUMI @ RTE_CRYPTO_CIPHER_KASUMI_F8
					  * and ZUC @ RTE_CRYPTO_CIPHER_ZUC_EEA3,
					  * this field should be in bits. For
					  * digest-encrypted cases this must be
					  * an 8-bit multiple.
					  */
					uint32_t length;
					 /**< The message length, in bytes, of the
					  * source buffer on which the cryptographic
					  * operation will be computed.
					  * This must be a multiple of the block size
					  * if a block cipher is being used. This is
					  * also the same as the result length.
					  *
					  * @note
					  * For SNOW 3G @ RTE_CRYPTO_AUTH_SNOW3G_UEA2,
					  * KASUMI @ RTE_CRYPTO_CIPHER_KASUMI_F8
					  * and ZUC @ RTE_CRYPTO_CIPHER_ZUC_EEA3,
					  * this field should be in bits. For
					  * digest-encrypted cases this must be
					  * an 8-bit multiple.
					  */
				} data; /**< Data offsets and length for ciphering */
			} cipher;

			struct {
				struct {
					uint32_t offset;
					 /**< Starting point for hash processing,
					  * specified as number of bytes from start of
					  * packet in source buffer.
					  *
					  * @note
					  * For SNOW 3G @ RTE_CRYPTO_AUTH_SNOW3G_UIA2,
					  * KASUMI @ RTE_CRYPTO_AUTH_KASUMI_F9
					  * and ZUC @ RTE_CRYPTO_AUTH_ZUC_EIA3,
					  * this field should be in bits. For
					  * digest-encrypted cases this must be
					  * an 8-bit multiple.
					  *
					  * @note
					  * For KASUMI @ RTE_CRYPTO_AUTH_KASUMI_F9,
					  * this offset should be such that
					  * data to authenticate starts at COUNT.
					  *
					  * @note
					  * For DOCSIS security protocol, this
					  * offset is the DOCSIS header length
					  * and, therefore, also the CRC offset
					  * i.e. the number of bytes into the
					  * packet at which CRC calculation
					  * should begin.
					  */
					uint32_t length;
					 /**< The message length, in bytes, of the source
					  * buffer that the hash will be computed on.
					  *
					  * @note
					  * For SNOW 3G @ RTE_CRYPTO_AUTH_SNOW3G_UIA2,
					  * KASUMI @ RTE_CRYPTO_AUTH_KASUMI_F9
					  * and ZUC @ RTE_CRYPTO_AUTH_ZUC_EIA3,
					  * this field should be in bits. For
					  * digest-encrypted cases this must be
					  * an 8-bit multiple.
					  *
					  * @note
					  * For KASUMI @ RTE_CRYPTO_AUTH_KASUMI_F9,
					  * the length should include the COUNT,
					  * FRESH, message, direction bit and padding
					  * (to be multiple of 8 bits).
					  *
					  * @note
					  * For DOCSIS security protocol, this
					  * is the CRC length i.e. the number of
					  * bytes in the packet over which the
					  * CRC should be calculated
					  */
				} data;
				/**< Data offsets and length for authentication */

				struct {
					uint8_t *data;
					/**< This points to the location where
					 * the digest result should be inserted
					 * (in the case of digest generation)
					 * or where the purported digest exists
					 * (in the case of digest verification).
					 *
					 * At session creation time, the client
					 * specified the digest result length with
					 * the digest_length member of the
					 * @ref rte_crypto_auth_xform structure.
					 * For physical crypto devices the caller
					 * must allocate at least digest_length of
					 * physically contiguous memory at this
					 * location.
					 *
					 * For digest generation, the digest result
					 * will overwrite any data at this location.
					 *
					 * @note
					 * Digest-encrypted case.
					 * Digest can be generated, appended to
					 * the end of raw data and encrypted
					 * together using chained digest
					 * generation
					 * (@ref RTE_CRYPTO_AUTH_OP_GENERATE)
					 * and encryption
					 * (@ref RTE_CRYPTO_CIPHER_OP_ENCRYPT)
					 * xforms. Similarly, authentication
					 * of the raw data against appended,
					 * decrypted digest, can be performed
					 * using decryption
					 * (@ref RTE_CRYPTO_CIPHER_OP_DECRYPT)
					 * and digest verification
					 * (@ref RTE_CRYPTO_AUTH_OP_VERIFY)
					 * chained xforms.
					 * To perform those operations, a few
					 * additional conditions must be met:
					 * - caller must allocate at least
					 * digest_length of memory at the end of
					 * source and (in case of out-of-place
					 * operations) destination buffer; those
					 * buffers can be linear or split using
					 * scatter-gather lists,
					 * - digest data pointer must point to
					 * the end of source or (in case of
					 * out-of-place operations) destination
					 * data, which is pointer to the
					 * data buffer + auth.data.offset +
					 * auth.data.length,
					 * - cipher.data.offset +
					 * cipher.data.length must be greater
					 * than auth.data.offset +
					 * auth.data.length and is typically
					 * equal to auth.data.offset +
					 * auth.data.length + digest_length.
					 * - for wireless algorithms, i.e.
					 * SNOW 3G, KASUMI and ZUC, as the
					 * cipher.data.length,
					 * cipher.data.offset,
					 * auth.data.length and
					 * auth.data.offset are in bits, they
					 * must be 8-bit multiples.
					 *
					 * Note, that for security reasons, it
					 * is PMDs' responsibility to not
					 * leave an unencrypted digest in any
					 * buffer after performing auth-cipher
					 * operations.
					 *
					 */
					rte_iova_t phys_addr;
					/**< Physical address of digest */
				} digest; /**< Digest parameters */
			} auth;
		};
	};
};


/**
 * Reset the fields of a symmetric operation to their default values.
 *
 * @param	op	The crypto operation to be reset.
 */
static inline void
__rte_crypto_sym_op_reset(struct rte_crypto_sym_op *op)
{
	memset(op, 0, sizeof(*op));
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

	return 0;
}

/**
 * Converts portion of mbuf data into a vector representation.
 * Each segment will be represented as a separate entry in *vec* array.
 * Expects that provided *ofs* + *len* not to exceed mbuf's *pkt_len*.
 * @param mb
 *   Pointer to the *rte_mbuf* object.
 * @param ofs
 *   Offset within mbuf data to start with.
 * @param len
 *   Length of data to represent.
 * @param vec
 *   Pointer to an output array of IO vectors.
 * @param num
 *   Size of an output array.
 * @return
 *   - number of successfully filled entries in *vec* array.
 *   - negative number of elements in *vec* array required.
 */
__rte_experimental
static inline int
rte_crypto_mbuf_to_vec(const struct rte_mbuf *mb, uint32_t ofs, uint32_t len,
	struct rte_crypto_vec vec[], uint32_t num)
{
	uint32_t i;
	struct rte_mbuf *nseg;
	uint32_t left;
	uint32_t seglen;

	/* assuming that requested data starts in the first segment */
	RTE_ASSERT(mb->data_len > ofs);

	if (mb->nb_segs > num)
		return -mb->nb_segs;

	vec[0].base = rte_pktmbuf_mtod_offset(mb, void *, ofs);
	vec[0].iova = rte_pktmbuf_iova_offset(mb, ofs);

	/* whole data lies in the first segment */
	seglen = mb->data_len - ofs;
	if (len <= seglen) {
		vec[0].len = len;
		return 1;
	}

	/* data spread across segments */
	vec[0].len = seglen;
	left = len - seglen;
	for (i = 1, nseg = mb->next; nseg != NULL; nseg = nseg->next, i++) {

		vec[i].base = rte_pktmbuf_mtod(nseg, void *);
		vec[i].iova = rte_pktmbuf_iova(nseg);

		seglen = nseg->data_len;
		if (left <= seglen) {
			/* whole requested data is completed */
			vec[i].len = left;
			left = 0;
			i++;
			break;
		}

		/* use whole segment */
		vec[i].len = seglen;
		left -= seglen;
	}

	RTE_ASSERT(left == 0);
	return i;
}


#ifdef __cplusplus
}
#endif

#endif /* _RTE_CRYPTO_SYM_H_ */
