/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Cavium Networks
 */

#ifndef _RTE_CRYPTO_ASYM_H_
#define _RTE_CRYPTO_ASYM_H_

/**
 * @file rte_crypto_asym.h
 *
 * RTE Definitions for Asymmetric Cryptography
 *
 * Defines asymmetric algorithms and modes, as well as supported
 * asymmetric crypto operations.
 */

#include <string.h>
#include <stdint.h>

#include <rte_memory.h>
#include <rte_mempool.h>
#include <rte_common.h>

#include "rte_crypto_sym.h"

struct rte_cryptodev_asym_session;

#ifdef __cplusplus
extern "C" {
#endif

/** asym key exchange operation type name strings */
extern const char *
rte_crypto_asym_ke_strings[];

/** asym operations type name strings */
extern const char *
rte_crypto_asym_op_strings[];

#ifdef __cplusplus
}
#endif

#define RTE_CRYPTO_ASYM_FLAG_PUB_KEY_NO_PADDING		RTE_BIT32(0)
/**<
 * Flag to denote public key will be returned without leading zero bytes
 * and if the flag is not set, public key will be padded to the left with
 * zeros to the size of the underlying algorithm (default)
 */
#define RTE_CRYPTO_ASYM_FLAG_SHARED_KEY_NO_PADDING	RTE_BIT32(1)
/**<
 * Flag to denote shared secret will be returned without leading zero bytes
 * and if the flag is not set, shared secret will be padded to the left with
 * zeros to the size of the underlying algorithm (default)
 */
#define RTE_CRYPTO_ASYM_FLAG_PUB_KEY_COMPRESSED		RTE_BIT32(2)
/**<
 * Flag to denote public key will be returned in compressed form
 */

/**
 * List of elliptic curves. This enum aligns with
 * TLS "Supported Groups" registry (previously known  as
 * NamedCurve registry). FFDH groups are not, and will not
 * be included in this list.
 * Deprecation for selected curve in TLS does not deprecate
 * the selected curve in Cryptodev.
 * https://www.iana.org/assignments/tls-parameters/tls-parameters.xhtml
 */
enum rte_crypto_curve_id {
	RTE_CRYPTO_EC_GROUP_SECP192R1 = 19,
	RTE_CRYPTO_EC_GROUP_SECP224R1 = 21,
	RTE_CRYPTO_EC_GROUP_SECP256R1 = 23,
	RTE_CRYPTO_EC_GROUP_SECP384R1 = 24,
	RTE_CRYPTO_EC_GROUP_SECP521R1 = 25,
	RTE_CRYPTO_EC_GROUP_ED25519   = 29,
	RTE_CRYPTO_EC_GROUP_ED448     = 30,
	RTE_CRYPTO_EC_GROUP_SM2       = 41,
};

/**
 * List of Edwards curve instances as per RFC 8032 (Section 5).
 */
enum rte_crypto_edward_instance {
	RTE_CRYPTO_EDCURVE_25519,
	RTE_CRYPTO_EDCURVE_25519CTX,
	RTE_CRYPTO_EDCURVE_25519PH,
	RTE_CRYPTO_EDCURVE_448,
	RTE_CRYPTO_EDCURVE_448PH
};

/**
 * Asymmetric crypto transformation types.
 * Each xform type maps to one asymmetric algorithm
 * performing specific operation
 */
enum rte_crypto_asym_xform_type {
	RTE_CRYPTO_ASYM_XFORM_UNSPECIFIED = 0,
	/**< Invalid xform. */
	RTE_CRYPTO_ASYM_XFORM_NONE,
	/**< Xform type None.
	 * May be supported by PMD to support
	 * passthrough op for debugging purpose.
	 * if xform_type none , op_type is disregarded.
	 */
	RTE_CRYPTO_ASYM_XFORM_RSA,
	/**< RSA. Performs Encrypt, Decrypt, Sign and Verify.
	 * Refer to rte_crypto_asym_op_type
	 */
	RTE_CRYPTO_ASYM_XFORM_DH,
	/**< Diffie-Hellman.
	 * Performs Key Generate and Shared Secret Compute.
	 * Refer to rte_crypto_asym_op_type
	 */
	RTE_CRYPTO_ASYM_XFORM_DSA,
	/**< Digital Signature Algorithm
	 * Performs Signature Generation and Verification.
	 * Refer to rte_crypto_asym_op_type
	 */
	RTE_CRYPTO_ASYM_XFORM_MODINV,
	/**< Modular Multiplicative Inverse
	 * Perform Modular Multiplicative Inverse b^(-1) mod n
	 */
	RTE_CRYPTO_ASYM_XFORM_MODEX,
	/**< Modular Exponentiation
	 * Perform Modular Exponentiation b^e mod n
	 */
	RTE_CRYPTO_ASYM_XFORM_ECDSA,
	/**< Elliptic Curve Digital Signature Algorithm
	 * Perform Signature Generation and Verification.
	 */
	RTE_CRYPTO_ASYM_XFORM_ECDH,
	/**< Elliptic Curve Diffie Hellman */
	RTE_CRYPTO_ASYM_XFORM_ECPM,
	/**< Elliptic Curve Point Multiplication */
	RTE_CRYPTO_ASYM_XFORM_ECFPM,
	/**< Elliptic Curve Fixed Point Multiplication */
	RTE_CRYPTO_ASYM_XFORM_SM2,
	/**< ShangMi 2
	 * Performs Encrypt, Decrypt, Sign and Verify.
	 * Refer to rte_crypto_asym_op_type.
	 */
	RTE_CRYPTO_ASYM_XFORM_EDDSA,
	/**< Edwards Curve Digital Signature Algorithm
	 * Perform Signature Generation and Verification.
	 */
};

/**
 * Asymmetric crypto operation type variants
 */
enum rte_crypto_asym_op_type {
	RTE_CRYPTO_ASYM_OP_ENCRYPT,
	/**< Asymmetric Encrypt operation */
	RTE_CRYPTO_ASYM_OP_DECRYPT,
	/**< Asymmetric Decrypt operation */
	RTE_CRYPTO_ASYM_OP_SIGN,
	/**< Signature Generation operation */
	RTE_CRYPTO_ASYM_OP_VERIFY,
	/**< Signature Verification operation */
	RTE_CRYPTO_ASYM_OP_LIST_END
};

/**
 * Asymmetric crypto key exchange operation type
 */
enum rte_crypto_asym_ke_type {
	RTE_CRYPTO_ASYM_KE_PRIV_KEY_GENERATE,
	/**< Private Key generation operation */
	RTE_CRYPTO_ASYM_KE_PUB_KEY_GENERATE,
	/**< Public Key generation operation */
	RTE_CRYPTO_ASYM_KE_SHARED_SECRET_COMPUTE,
	/**< Shared Secret compute operation */
	RTE_CRYPTO_ASYM_KE_PUB_KEY_VERIFY
	/**< Public Key Verification - can be used for
	 * elliptic curve point validation.
	 */
};

/**
 * Padding types for RSA signature.
 */
enum rte_crypto_rsa_padding_type {
	RTE_CRYPTO_RSA_PADDING_NONE = 0,
	/**< RSA no padding scheme */
	RTE_CRYPTO_RSA_PADDING_PKCS1_5,
	/**< RSA PKCS#1 PKCS1-v1_5 padding scheme. For signatures block type 01,
	 * for encryption block type 02 are used.
	 */
	RTE_CRYPTO_RSA_PADDING_OAEP,
	/**< RSA PKCS#1 OAEP padding scheme */
	RTE_CRYPTO_RSA_PADDING_PSS,
	/**< RSA PKCS#1 PSS padding scheme */
};

/**
 * RSA private key type enumeration
 *
 * enumerates private key format required to perform RSA crypto
 * transform.
 */
enum rte_crypto_rsa_priv_key_type {
	RTE_RSA_KEY_TYPE_EXP,
	/**< RSA private key is an exponent */
	RTE_RSA_KEY_TYPE_QT,
	/**< RSA private key is in quintuple format
	 * See rte_crypto_rsa_priv_key_qt
	 */
};

/**
 * Buffer to hold crypto params required for asym operations.
 *
 * These buffers can be used for both input to PMD and output from PMD. When
 * used for output from PMD, application has to ensure the buffer is large
 * enough to hold the target data.
 *
 * If an operation requires the PMD to generate a random number,
 * and the device supports CSRNG, 'data' should be set to NULL.
 * The crypto parameter in question will not be used by the PMD,
 * as it is internally generated.
 */
typedef struct rte_crypto_param_t {
	uint8_t *data;
	/**< pointer to buffer holding data */
	rte_iova_t iova;
	/**< IO address of data buffer */
	size_t length;
	/**< length of data in bytes */
} rte_crypto_param;

/** Unsigned big-integer in big-endian format */
typedef rte_crypto_param rte_crypto_uint;

/**
 * Structure for elliptic curve point
 */
struct rte_crypto_ec_point {
	rte_crypto_param x;
	/**< X coordinate */
	rte_crypto_param y;
	/**< Y coordinate */
};

/**
 * Structure describing RSA private key in quintuple format.
 * See PKCS V1.5 RSA Cryptography Standard.
 */
struct rte_crypto_rsa_priv_key_qt {
	rte_crypto_uint p;
	/**< the first factor */
	rte_crypto_uint q;
	/**< the second factor */
	rte_crypto_uint dP;
	/**< the first factor's CRT exponent */
	rte_crypto_uint dQ;
	/**< the second's factor's CRT exponent */
	rte_crypto_uint qInv;
	/**< the CRT coefficient */
};

/**
 * RSA padding type
 */
struct rte_crypto_rsa_padding {
	enum rte_crypto_rsa_padding_type type;
	/**< RSA padding scheme to be used for transform */
	enum rte_crypto_auth_algorithm hash;
	/**<
	 * RSA padding hash algorithm
	 * Valid hash algorithms are:
	 * MD5, SHA1, SHA224, SHA256, SHA384, SHA512
	 *
	 * When a specific padding type is selected, the following rules apply:
	 * - RTE_CRYPTO_RSA_PADDING_NONE:
	 * This field is ignored by the PMD
	 *
	 * - RTE_CRYPTO_RSA_PADDING_PKCS1_5:
	 * When signing an operation this field is used to determine value
	 * of the DigestInfo structure, therefore specifying which algorithm
	 * was used to create the message digest.
	 * When doing encryption/decryption this field is ignored for this
	 * padding type.
	 *
	 * - RTE_CRYPTO_RSA_PADDING_OAEP
	 * This field shall be set with the hash algorithm used
	 * in the padding scheme
	 *
	 * - RTE_CRYPTO_RSA_PADDING_PSS
	 * This field shall be set with the hash algorithm used
	 * in the padding scheme (and to create the input message digest)
	 */
	enum rte_crypto_auth_algorithm mgf1hash;
	/**<
	 * Hash algorithm to be used for mask generation if the
	 * padding scheme is either OAEP or PSS. If the padding
	 * scheme is unspecified a data hash algorithm is used
	 * for mask generation. Valid hash algorithms are:
	 * MD5, SHA1, SHA224, SHA256, SHA384, SHA512
	 */
	uint16_t pss_saltlen;
	/**<
	 * RSA PSS padding salt length
	 *
	 * Used only when RTE_CRYPTO_RSA_PADDING_PSS padding is selected,
	 * otherwise ignored.
	 */
	rte_crypto_param oaep_label;
	/**<
	 * RSA OAEP padding optional label
	 *
	 * Used only when RTE_CRYPTO_RSA_PADDING_OAEP padding is selected,
	 * otherwise ignored. If label.data == NULL, a default
	 * label (empty string) is used.
	 */
};

/**
 * Asymmetric RSA transform data
 *
 * Structure describing RSA xform params
 */
struct rte_crypto_rsa_xform {
	rte_crypto_uint n;
	/**< the RSA modulus */
	rte_crypto_uint e;
	/**< the RSA public exponent */

	enum rte_crypto_rsa_priv_key_type key_type;

	struct {
		rte_crypto_uint d;
		/**< the RSA private exponent */
		struct rte_crypto_rsa_priv_key_qt qt;
		/**< qt - Private key in quintuple format */
	};

	struct rte_crypto_rsa_padding padding;
	/**< RSA padding information */
};

/**
 * Asymmetric Modular exponentiation transform data
 *
 * Structure describing modular exponentiation xform param
 */
struct rte_crypto_modex_xform {
	rte_crypto_uint modulus;
	/**< Modulus data for modexp transform operation */
	rte_crypto_uint exponent;
	/**< Exponent of the modexp transform operation */
};

/**
 * Asymmetric modular multiplicative inverse transform operation
 *
 * Structure describing modular multiplicative inverse transform
 */
struct rte_crypto_modinv_xform {
	rte_crypto_uint modulus;
	/**< Modulus data for modular multiplicative inverse operation */
};

/**
 * Asymmetric DH transform data
 *
 * Structure describing deffie-hellman xform params
 */
struct rte_crypto_dh_xform {
	rte_crypto_uint p;
	/**< Prime modulus data */
	rte_crypto_uint g;
	/**< DH Generator */
};

/**
 * Asymmetric Digital Signature transform operation
 *
 * Structure describing DSA xform params
 */
struct rte_crypto_dsa_xform {
	rte_crypto_uint p;
	/**< Prime modulus */
	rte_crypto_uint q;
	/**< Order of the subgroup */
	rte_crypto_uint g;
	/**< Generator of the subgroup */
	rte_crypto_uint x;
	/**< x: Private key of the signer */
};

/**
 * Asymmetric elliptic curve transform data
 *
 * Structure describing all EC based xform params
 */
struct rte_crypto_ec_xform {
	enum rte_crypto_curve_id curve_id;
	/**< Pre-defined ec groups */

	rte_crypto_uint pkey;
	/**< Private key */

	struct rte_crypto_ec_point q;
	/**< Public key */
};

/**
 * Operations params for modular operations:
 * exponentiation and multiplicative inverse
 */
struct rte_crypto_mod_op_param {
	rte_crypto_uint base;
	/**< Base of modular exponentiation/multiplicative inverse. */
	rte_crypto_uint result;
	/**< Result of modular exponentiation/multiplicative inverse. */
};

/**
 * RSA operation params
 */
struct rte_crypto_rsa_op_param {
	enum rte_crypto_asym_op_type op_type;
	/**< Type of RSA operation for transform */

	rte_crypto_param message;
	/**<
	 * Pointer to input data
	 * - to be encrypted for RSA public encrypt.
	 * - to be signed for RSA sign generation.
	 * - to be authenticated for RSA sign verification.
	 *
	 * Pointer to output data
	 * - for RSA private decrypt.
	 * In this case the underlying array should have been
	 * allocated with enough memory to hold plaintext output
	 * (i.e. must be at least RSA key size). The message.length
	 * field could be either 0 or minimal length expected from PMD.
	 * This could be validated and overwritten by the PMD
	 * with the decrypted length.
	 */

	rte_crypto_param cipher;
	/**<
	 * Pointer to input data
	 * - to be decrypted for RSA private decrypt.
	 *
	 * Pointer to output data
	 * - for RSA public encrypt.
	 * In this case the underlying array should have been allocated
	 * with enough memory to hold ciphertext output (i.e. must be
	 * at least RSA key size). The cipher.length field could be
	 * either 0 or minimal length expected from PMD.
	 * This could be validated and overwritten by the PMD
	 * with the encrypted length.
	 *
	 * When RTE_CRYPTO_RSA_PADDING_NONE and RTE_CRYPTO_ASYM_OP_VERIFY
	 * selected, this is an output of decrypted signature.
	 */

	rte_crypto_param sign;
	/**<
	 * Pointer to input data
	 * - to be verified for RSA public decrypt.
	 *
	 * Pointer to output data
	 * - for RSA private encrypt.
	 * In this case the underlying array should have been allocated
	 * with enough memory to hold signature output (i.e. must be
	 * at least RSA key size). The sign.length field could be
	 * either 0 or minimal length expected from PMD.
	 * This could be validated and overwritten by the PMD
	 * with the signature length.
	 */
};

/**
 * Diffie-Hellman Operations params.
 * @note:
 */
struct rte_crypto_dh_op_param {
	enum rte_crypto_asym_ke_type ke_type;
	/**< Key exchange operation type */
	rte_crypto_uint priv_key;
	/**<
	 * Output - generated private key when ke_type is
	 * RTE_CRYPTO_ASYM_KE_PRIV_KEY_GENERATE.
	 *
	 * Input - private key when ke_type is one of:
	 * RTE_CRYPTO_ASYM_KE_PUB_KEY_GENERATE,
	 * RTE_CRYPTO_ASYM_KE_SHARED_SECRET_COMPUTE.
	 *
	 * In case priv_key.length is 0 and ke_type is set with
	 * RTE_CRYPTO_ASYM_KE_PUB_KEY_GENERATE, CSRNG capable
	 * device will generate a private key and use it for public
	 * key generation.
	 */
	rte_crypto_uint pub_key;
	/**<
	 * Output - generated public key when ke_type is
	 * RTE_CRYPTO_ASYM_KE_PUB_KEY_GENERATE.
	 *
	 * Input - peer's public key when ke_type is
	 * RTE_CRYPTO_ASYM_KE_SHARED_SECRET_COMPUTE.
	 */
	rte_crypto_uint shared_secret;
	/**<
	 * Output - calculated shared secret when ke_type is
	 * RTE_CRYPTO_ASYM_KE_SHARED_SECRET_COMPUTE.
	 */
};

/**
 * Elliptic Curve Diffie-Hellman Operations params.
 */
struct rte_crypto_ecdh_op_param {
	enum rte_crypto_asym_ke_type ke_type;
	/**< Key exchange operation type */
	rte_crypto_uint priv_key;
	/**<
	 * Output - generated private key when ke_type is
	 * RTE_CRYPTO_ASYM_KE_PRIVATE_KEY_GENERATE.
	 *
	 * Input - private key when ke_type is one of:
	 * RTE_CRYPTO_ASYM_KE_PUBLIC_KEY_GENERATE,
	 * RTE_CRYPTO_ASYM_KE_SHARED_SECRET_COMPUTE.
	 *
	 * In case priv_key.length is 0 and ke_type is set with
	 * RTE_CRYPTO_ASYM_KE_PUBLIC_KEY_GENERATE, CSRNG capable
	 * device will generate private key and use it for public
	 * key generation.
	 */
	struct rte_crypto_ec_point pub_key;
	/**<
	 * Output - generated public key when ke_type is
	 * RTE_CRYPTO_ASYM_KE_PUBLIC_KEY_GENERATE.
	 *
	 * Input - peer's public key, when ke_type is one of:
	 * RTE_CRYPTO_ASYM_KE_SHARED_SECRET_COMPUTE,
	 * RTE_CRYPTO_ASYM_KE_EC_PUBLIC_KEY_VERIFY.
	 */
	struct rte_crypto_ec_point shared_secret;
	/**<
	 * Output - calculated shared secret when ke_type is
	 * RTE_CRYPTO_ASYM_KE_SHARED_SECRET_COMPUTE.
	 */
};

/**
 * DSA Operations params
 */
struct rte_crypto_dsa_op_param {
	enum rte_crypto_asym_op_type op_type;
	/**< Signature Generation or Verification */
	rte_crypto_param message;
	/**< input message to be signed or verified */
	rte_crypto_uint k;
	/**< Per-message secret number, which is an integer
	 * in the interval (1, q-1).
	 * If the random number is generated by the PMD,
	 * the 'rte_crypto_param.data' parameter should be set to NULL.
	 */
	rte_crypto_uint r;
	/**< dsa sign component 'r' value
	 *
	 * output if op_type = sign generate,
	 * input if op_type = sign verify
	 */
	rte_crypto_uint s;
	/**< dsa sign component 's' value
	 *
	 * output if op_type = sign generate,
	 * input if op_type = sign verify
	 */
	rte_crypto_uint y;
	/**< y : Public key of the signer.
	 * y = g^x mod p
	 */
};

/**
 * ECDSA operation params
 */
struct rte_crypto_ecdsa_op_param {
	enum rte_crypto_asym_op_type op_type;
	/**< Signature generation or verification */

	rte_crypto_param message;
	/**< Input message digest to be signed or verified */

	rte_crypto_uint k;
	/**< The ECDSA per-message secret number, which is an integer
	 * in the interval (1, n-1).
	 * If the random number is generated by the PMD,
	 * the 'rte_crypto_param.data' parameter should be set to NULL.
	 */

	rte_crypto_uint r;
	/**< r component of elliptic curve signature
	 *     output : for signature generation
	 *     input  : for signature verification
	 */
	rte_crypto_uint s;
	/**< s component of elliptic curve signature
	 *     output : for signature generation
	 *     input  : for signature verification
	 */
};

/**
 * EdDSA operation params
 */
struct rte_crypto_eddsa_op_param {
	enum rte_crypto_asym_op_type op_type;
	/**< Signature generation or verification */

	rte_crypto_param message;
	/**< Input message digest to be signed or verified */

	rte_crypto_param context;
	/**< Context value for the sign op.
	 *   Must not be empty for Ed25519ctx instance.
	 */

	enum rte_crypto_edward_instance instance;
	/**< Type of Edwards curve. */

	rte_crypto_uint sign;
	/**< Edward curve signature
	 *     output : for signature generation
	 *     input  : for signature verification
	 */
};

/**
 * Structure for EC point multiplication operation param
 */
struct rte_crypto_ecpm_op_param {
	struct rte_crypto_ec_point p;
	/**< x and y coordinates of input point */

	struct rte_crypto_ec_point r;
	/**< x and y coordinates of resultant point */

	rte_crypto_param scalar;
	/**< Scalar to multiply the input point */
};

/**
 * SM2 operation capabilities
 */
enum rte_crypto_sm2_op_capa {
	RTE_CRYPTO_SM2_RNG,
	/**< Random number generator supported in SM2 ops. */
	RTE_CRYPTO_SM2_PH,
	/**< Prehash message before crypto op. */
	RTE_CRYPTO_SM2_PARTIAL,
	/**< Calculate elliptic curve points only. */
};

/**
 * SM2 operation params.
 */
struct rte_crypto_sm2_op_param {
	enum rte_crypto_asym_op_type op_type;
	/**< Signature generation or verification. */

	enum rte_crypto_auth_algorithm hash;
	/**< Hash algorithm used in EC op. */

	rte_crypto_param message;
	/**<
	 * Pointer to input data
	 * - to be encrypted for SM2 public encrypt.
	 * - to be signed for SM2 sign generation.
	 * - to be authenticated for SM2 sign verification.
	 *
	 * Pointer to output data
	 * - for SM2 private decrypt.
	 * In this case the underlying array should have been
	 * allocated with enough memory to hold plaintext output
	 * (at least encrypted text length). The message.length field
	 * will be overwritten by the PMD with the decrypted length.
	 */

	union {
		rte_crypto_param cipher;
		/**<
		 * Pointer to input data
		 * - to be decrypted for SM2 private decrypt.
		 *
		 * Pointer to output data
		 * - for SM2 public encrypt.
		 * In this case the underlying array should have been allocated
		 * with enough memory to hold ciphertext output (at least X bytes
		 * for prime field curve of N bytes and for message M bytes,
		 * where X = (C1 || C2 || C3) and computed based on SM2 RFC as
		 * C1 (1 + N + N), C2 = M, C3 = N. The cipher.length field will
		 * be overwritten by the PMD with the encrypted length.
		 */
		struct {
			struct rte_crypto_ec_point c1;
			/**<
			 * This field is used only when PMD does not support the full
			 * process of the SM2 encryption/decryption, but the elliptic
			 * curve part only.
			 *
			 * In the case of encryption, it is an output - point C1 = (x1,y1).
			 * In the case of decryption, if is an input - point C1 = (x1,y1).
			 *
			 * Must be used along with the RTE_CRYPTO_SM2_PARTIAL flag.
			 */
			struct rte_crypto_ec_point kp;
			/**<
			 * This field is used only when PMD does not support the full
			 * process of the SM2 encryption/decryption, but the elliptic
			 * curve part only.
			 *
			 * It is an output in the encryption case, it is a point
			 * [k]P = (x2,y2).
			 *
			 * Must be used along with the RTE_CRYPTO_SM2_PARTIAL flag.
			 */
		};
	};

	rte_crypto_uint id;
	/**< The SM2 id used by signer and verifier. */

	rte_crypto_uint k;
	/**< The SM2 per-message secret number, which is an integer
	 * in the interval (1, n-1).
	 * If the random number is generated by the PMD,
	 * the 'rte_crypto_param.data' parameter should be set to NULL.
	 */

	rte_crypto_uint r;
	/**< r component of elliptic curve signature
	 *     output : for signature generation (of at least N bytes
	 *              where prime field length is N bytes)
	 *     input  : for signature verification
	 */
	rte_crypto_uint s;
	/**< s component of elliptic curve signature
	 *     output : for signature generation (of at least N bytes
	 *              where prime field length is N bytes)
	 *     input  : for signature verification
	 */
};

/**
 * Asymmetric crypto transform data
 *
 * Structure describing asym xforms.
 */
struct rte_crypto_asym_xform {
	struct rte_crypto_asym_xform *next;
	/**< Pointer to next xform to set up xform chain.*/
	enum rte_crypto_asym_xform_type xform_type;
	/**< Asymmetric crypto transform */

	union {
		struct rte_crypto_rsa_xform rsa;
		/**< RSA xform parameters */

		struct rte_crypto_modex_xform modex;
		/**< Modular Exponentiation xform parameters */

		struct rte_crypto_modinv_xform modinv;
		/**< Modular Multiplicative Inverse xform parameters */

		struct rte_crypto_dh_xform dh;
		/**< DH xform parameters */

		struct rte_crypto_dsa_xform dsa;
		/**< DSA xform parameters */

		struct rte_crypto_ec_xform ec;
		/**< EC xform parameters, used by elliptic curve based
		 * operations.
		 */
	};
};

/**
 * Asymmetric Cryptographic Operation.
 *
 * Structure describing asymmetric crypto operation params.
 */
struct rte_crypto_asym_op {
	union {
		struct rte_cryptodev_asym_session *session;
		/**< Handle for the initialised session context */
		struct rte_crypto_asym_xform *xform;
		/**< Session-less API crypto operation parameters */
	};

	union {
		struct rte_crypto_rsa_op_param rsa;
		struct rte_crypto_mod_op_param modex;
		struct rte_crypto_mod_op_param modinv;
		struct rte_crypto_dh_op_param dh;
		struct rte_crypto_ecdh_op_param ecdh;
		struct rte_crypto_dsa_op_param dsa;
		struct rte_crypto_ecdsa_op_param ecdsa;
		struct rte_crypto_ecpm_op_param ecpm;
		struct rte_crypto_sm2_op_param sm2;
		struct rte_crypto_eddsa_op_param eddsa;
	};
	uint16_t flags;
	/**<
	 * Asymmetric crypto operation flags.
	 * Please refer to the RTE_CRYPTO_ASYM_FLAG_*.
	 */
};

#endif /* _RTE_CRYPTO_ASYM_H_ */
