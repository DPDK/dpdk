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

#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>
#include <stdint.h>

#include <rte_memory.h>
#include <rte_mempool.h>
#include <rte_common.h>

#include "rte_crypto_sym.h"

struct rte_cryptodev_asym_session;

/** asym xform type name strings */
extern const char *
rte_crypto_asym_xform_strings[];

/** asym operations type name strings */
extern const char *
rte_crypto_asym_op_strings[];

/**
 * TLS named curves
 * https://tools.ietf.org/html/rfc8422
 */
enum rte_crypto_ec_group {
	RTE_CRYPTO_EC_GROUP_UNKNOWN  = 0,
	RTE_CRYPTO_EC_GROUP_SECP192R1 = 19,
	RTE_CRYPTO_EC_GROUP_SECP224R1 = 21,
	RTE_CRYPTO_EC_GROUP_SECP256R1 = 23,
	RTE_CRYPTO_EC_GROUP_SECP384R1 = 24,
	RTE_CRYPTO_EC_GROUP_SECP521R1 = 25,
};

/**
 * Asymmetric crypto transformation types.
 * Each xform type maps to one asymmetric algorithm
 * performing specific operation
 *
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
	RTE_CRYPTO_ASYM_XFORM_ECPM,
	/**< Elliptic Curve Point Multiplication */
	RTE_CRYPTO_ASYM_XFORM_TYPE_LIST_END
	/**< End of list */
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
	RTE_CRYPTO_ASYM_OP_PRIVATE_KEY_GENERATE,
	/**< DH Private Key generation operation */
	RTE_CRYPTO_ASYM_OP_PUBLIC_KEY_GENERATE,
	/**< DH Public Key generation operation */
	RTE_CRYPTO_ASYM_OP_SHARED_SECRET_COMPUTE,
	/**< DH Shared Secret compute operation */
	RTE_CRYPTO_ASYM_OP_LIST_END
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
	RTE_CRYPTO_RSA_PADDING_TYPE_LIST_END
};

/**
 * RSA private key type enumeration
 *
 * enumerates private key format required to perform RSA crypto
 * transform.
 *
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
 * Asymmetric RSA transform data
 *
 * Structure describing RSA xform params
 *
 */
struct rte_crypto_rsa_xform {
	rte_crypto_uint n;
	/**< the RSA modulus */
	rte_crypto_uint e;
	/**< the RSA public exponent */

	enum rte_crypto_rsa_priv_key_type key_type;

	RTE_STD_C11
	union {
		rte_crypto_uint d;
		/**< the RSA private exponent */
		struct rte_crypto_rsa_priv_key_qt qt;
		/**< qt - Private key in quintuple format */
	};
};

/**
 * Asymmetric Modular exponentiation transform data
 *
 * Structure describing modular exponentiation xform param
 *
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
 *
 */
struct rte_crypto_modinv_xform {
	rte_crypto_uint modulus;
	/**< Modulus data for modular multiplicative inverse operation */
};

/**
 * Asymmetric DH transform data
 *
 * Structure describing deffie-hellman xform params
 *
 */
struct rte_crypto_dh_xform {
	enum rte_crypto_asym_op_type type;
	/**< Setup xform for key generate or shared secret compute */
	rte_crypto_uint p;
	/**< Prime modulus data */
	rte_crypto_uint g;
	/**< DH Generator */
};

/**
 * Asymmetric Digital Signature transform operation
 *
 * Structure describing DSA xform params
 *
 */
struct rte_crypto_dsa_xform {
	rte_crypto_uint p;
	/**< Prime modulus */
	rte_crypto_uint q;
	/**< Order of the subgroup */
	rte_crypto_uint g;
	/**< Generator of the subgroup */
	rte_crypto_uint x;
	/**< x: Private key of the signer in octet-string network
	 * byte order format.
	 * Used when app has pre-defined private key.
	 * Valid only when xform chain is DSA ONLY.
	 * if xform chain is DH private key generate + DSA, then DSA sign
	 * compute will use internally generated key.
	 */
};

/**
 * Asymmetric elliptic curve transform data
 *
 * Structure describing all EC based xform params
 *
 */
struct rte_crypto_ec_xform {
	enum rte_crypto_ec_group curve_id;
	/**< Pre-defined ec groups */
};

/**
 * Operations params for modular operations:
 * exponentiation and multiplicative inverse
 *
 */
struct rte_crypto_mod_op_param {
	rte_crypto_uint base;
	/** Base of modular exponentiation/multiplicative inverse */
	rte_crypto_uint result;
	/** Result of modular exponentiation/multiplicative inverse */
};

/**
 * RSA operation params
 *
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
	 * field should be 0 and will be overwritten by the PMD
	 * with the decrypted length.
	 *
	 * All data is in Octet-string network byte order format.
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
	 * at least RSA key size). The cipher.length field should
	 * be 0 and will be overwritten by the PMD with the encrypted length.
	 *
	 * All data is in Octet-string network byte order format.
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
	 * at least RSA key size). The sign.length field should
	 * be 0 and will be overwritten by the PMD with the signature length.
	 *
	 * All data is in Octet-string network byte order format.
	 */

	enum rte_crypto_rsa_padding_type pad;
	/**< RSA padding scheme to be used for transform */

	enum rte_crypto_auth_algorithm md;
	/**< Hash algorithm to be used for data hash if padding
	 * scheme is either OAEP or PSS. Valid hash algorithms
	 * are:
	 * MD5, SHA1, SHA224, SHA256, SHA384, SHA512
	 */

	enum rte_crypto_auth_algorithm mgf1md;
	/**<
	 * Hash algorithm to be used for mask generation if
	 * padding scheme is either OAEP or PSS. If padding
	 * scheme is unspecified data hash algorithm is used
	 * for mask generation. Valid hash algorithms are:
	 * MD5, SHA1, SHA224, SHA256, SHA384, SHA512
	 */
};

/**
 * Diffie-Hellman Operations params.
 * @note:
 */
struct rte_crypto_dh_op_param {
	rte_crypto_uint pub_key;
	/**<
	 * Output generated public key when xform type is
	 * DH PUB_KEY_GENERATION.
	 * Input peer public key when xform type is DH
	 * SHARED_SECRET_COMPUTATION
	 *
	 */

	rte_crypto_uint priv_key;
	/**<
	 * Output generated private key if xform type is
	 * DH PRIVATE_KEY_GENERATION
	 * Input when xform type is DH SHARED_SECRET_COMPUTATION.
	 *
	 */

	rte_crypto_uint shared_secret;
	/**<
	 * Output with calculated shared secret
	 * when dh xform set up with op type = SHARED_SECRET_COMPUTATION.
	 *
	 */
};

/**
 * DSA Operations params
 *
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

	rte_crypto_uint pkey;
	/**< Private key of the signer for signature generation */

	struct rte_crypto_ec_point q;
	/**< Public key of the signer for verification */

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
 * Asymmetric crypto transform data
 *
 * Structure describing asym xforms.
 */
struct rte_crypto_asym_xform {
	struct rte_crypto_asym_xform *next;
	/**< Pointer to next xform to set up xform chain.*/
	enum rte_crypto_asym_xform_type xform_type;
	/**< Asymmetric crypto transform */

	RTE_STD_C11
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
 *
 */
struct rte_crypto_asym_op {
	RTE_STD_C11
	union {
		struct rte_cryptodev_asym_session *session;
		/**< Handle for the initialised session context */
		struct rte_crypto_asym_xform *xform;
		/**< Session-less API crypto operation parameters */
	};

	RTE_STD_C11
	union {
		struct rte_crypto_rsa_op_param rsa;
		struct rte_crypto_mod_op_param modex;
		struct rte_crypto_mod_op_param modinv;
		struct rte_crypto_dh_op_param dh;
		struct rte_crypto_dsa_op_param dsa;
		struct rte_crypto_ecdsa_op_param ecdsa;
		struct rte_crypto_ecpm_op_param ecpm;
	};
};

#ifdef __cplusplus
}
#endif

#endif /* _RTE_CRYPTO_ASYM_H_ */
