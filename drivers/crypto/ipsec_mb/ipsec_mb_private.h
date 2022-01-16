/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 Intel Corporation
 */

#ifndef _IPSEC_MB_PRIVATE_H_
#define _IPSEC_MB_PRIVATE_H_

#include <intel-ipsec-mb.h>
#include <cryptodev_pmd.h>
#include <rte_bus_vdev.h>

#if defined(RTE_LIB_SECURITY)
#define IPSEC_MB_DOCSIS_SEC_ENABLED 1
#include <rte_security.h>
#include <rte_security_driver.h>
#endif

/* Maximum length for digest */
#define DIGEST_LENGTH_MAX 64

/* Maximum length for memzone name */
#define IPSEC_MB_MAX_MZ_NAME 32

enum ipsec_mb_vector_mode {
	IPSEC_MB_NOT_SUPPORTED = 0,
	IPSEC_MB_SSE,
	IPSEC_MB_AVX,
	IPSEC_MB_AVX2,
	IPSEC_MB_AVX512
};

extern enum ipsec_mb_vector_mode vector_mode;

/** IMB_MGR instances, one per thread */
extern RTE_DEFINE_PER_LCORE(IMB_MGR *, mb_mgr);

#define CRYPTODEV_NAME_AESNI_MB_PMD crypto_aesni_mb
/**< IPSEC Multi buffer aesni_mb PMD device name */

#define CRYPTODEV_NAME_AESNI_GCM_PMD crypto_aesni_gcm
/**< IPSEC Multi buffer PMD aesni_gcm device name */

#define CRYPTODEV_NAME_KASUMI_PMD crypto_kasumi
/**< IPSEC Multi buffer PMD kasumi device name */

#define CRYPTODEV_NAME_SNOW3G_PMD crypto_snow3g
/**< IPSEC Multi buffer PMD snow3g device name */

#define CRYPTODEV_NAME_ZUC_PMD crypto_zuc
/**< IPSEC Multi buffer PMD zuc device name */

#define CRYPTODEV_NAME_CHACHA20_POLY1305_PMD crypto_chacha20_poly1305
/**< IPSEC Multi buffer PMD chacha20_poly1305 device name */

/** PMD LOGTYPE DRIVER, common to all PMDs */
extern int ipsec_mb_logtype_driver;
#define IPSEC_MB_LOG(level, fmt, ...)                                         \
	rte_log(RTE_LOG_##level, ipsec_mb_logtype_driver,                     \
		"%s() line %u: " fmt "\n", __func__, __LINE__, ##__VA_ARGS__)

/** All supported device types */
enum ipsec_mb_pmd_types {
	IPSEC_MB_PMD_TYPE_AESNI_MB = 0,
	IPSEC_MB_PMD_TYPE_AESNI_GCM,
	IPSEC_MB_PMD_TYPE_KASUMI,
	IPSEC_MB_PMD_TYPE_SNOW3G,
	IPSEC_MB_PMD_TYPE_ZUC,
	IPSEC_MB_PMD_TYPE_CHACHA20_POLY1305,
	IPSEC_MB_N_PMD_TYPES
};

/** Crypto operations */
enum ipsec_mb_operation {
	IPSEC_MB_OP_ENCRYPT_THEN_HASH_GEN = 0,
	IPSEC_MB_OP_HASH_VERIFY_THEN_DECRYPT,
	IPSEC_MB_OP_HASH_GEN_THEN_ENCRYPT,
	IPSEC_MB_OP_DECRYPT_THEN_HASH_VERIFY,
	IPSEC_MB_OP_ENCRYPT_ONLY,
	IPSEC_MB_OP_DECRYPT_ONLY,
	IPSEC_MB_OP_HASH_GEN_ONLY,
	IPSEC_MB_OP_HASH_VERIFY_ONLY,
	IPSEC_MB_OP_AEAD_AUTHENTICATED_DECRYPT,
	IPSEC_MB_OP_AEAD_AUTHENTICATED_ENCRYPT,
	IPSEC_MB_OP_NOT_SUPPORTED
};

extern uint8_t pmd_driver_id_aesni_mb;
extern uint8_t pmd_driver_id_aesni_gcm;
extern uint8_t pmd_driver_id_kasumi;
extern uint8_t pmd_driver_id_snow3g;
extern uint8_t pmd_driver_id_zuc;
extern uint8_t pmd_driver_id_chacha20_poly1305;

/** Helper function. Gets driver ID based on PMD type */
static __rte_always_inline uint8_t
ipsec_mb_get_driver_id(enum ipsec_mb_pmd_types pmd_type)
{
	switch (pmd_type) {
	case IPSEC_MB_PMD_TYPE_AESNI_MB:
		return pmd_driver_id_aesni_mb;
	case IPSEC_MB_PMD_TYPE_AESNI_GCM:
		return pmd_driver_id_aesni_gcm;
	case IPSEC_MB_PMD_TYPE_KASUMI:
		return pmd_driver_id_kasumi;
	case IPSEC_MB_PMD_TYPE_SNOW3G:
		return pmd_driver_id_snow3g;
	case IPSEC_MB_PMD_TYPE_ZUC:
		return pmd_driver_id_zuc;
	case IPSEC_MB_PMD_TYPE_CHACHA20_POLY1305:
		return pmd_driver_id_chacha20_poly1305;
	default:
		break;
	}
	return UINT8_MAX;
}

/** Common private data structure for each PMD */
struct ipsec_mb_dev_private {
	enum ipsec_mb_pmd_types pmd_type;
	/**< PMD  type */
	uint32_t max_nb_queue_pairs;
	/**< Max number of queue pairs supported by device */
	__extension__ uint8_t priv[0];
};

/** IPSEC Multi buffer queue pair common queue pair data for all PMDs */
struct ipsec_mb_qp {
	uint16_t id;
	/**< Queue Pair Identifier */
	char name[RTE_CRYPTODEV_NAME_MAX_LEN];
	struct rte_ring *ingress_queue;
	/**< Ring for placing operations ready for processing */
	struct rte_mempool *sess_mp;
	/**< Session Mempool */
	struct rte_mempool *sess_mp_priv;
	/**< Session Private Data Mempool */
	struct rte_cryptodev_stats stats;
	/**< Queue pair statistics */
	enum ipsec_mb_pmd_types pmd_type;
	/**< pmd type */
	uint8_t digest_idx;
	/**< Index of the next
	 * slot to be used in temp_digests,
	 * to store the digest for a given operation
	 */
	IMB_MGR *mb_mgr;
	/* Multi buffer manager */
	const struct rte_memzone *mb_mgr_mz;
	/* Shared memzone for storing mb_mgr */
	__extension__ uint8_t additional_data[0];
	/**< Storing PMD specific additional data */
};

static __rte_always_inline void *
ipsec_mb_get_qp_private_data(struct ipsec_mb_qp *qp)
{
	return (void *)qp->additional_data;
}

/** Helper function. Allocates job manager */
static __rte_always_inline IMB_MGR *
alloc_init_mb_mgr(void)
{
	IMB_MGR *mb_mgr = alloc_mb_mgr(0);

	if (unlikely(mb_mgr == NULL)) {
		IPSEC_MB_LOG(ERR, "Failed to allocate IMB_MGR data\n");
		return NULL;
	}

	init_mb_mgr_auto(mb_mgr, NULL);

	return mb_mgr;
}

/** Helper function. Gets per thread job manager */
static __rte_always_inline IMB_MGR *
get_per_thread_mb_mgr(void)
{
	if (unlikely(RTE_PER_LCORE(mb_mgr) == NULL))
		RTE_PER_LCORE(mb_mgr) = alloc_init_mb_mgr();

	return RTE_PER_LCORE(mb_mgr);
}

/** Helper function. Gets mode and chained xforms from the xform */
static __rte_always_inline int
ipsec_mb_parse_xform(const struct rte_crypto_sym_xform *xform,
			enum ipsec_mb_operation *mode,
			const struct rte_crypto_sym_xform **auth_xform,
			const struct rte_crypto_sym_xform **cipher_xform,
			const struct rte_crypto_sym_xform **aead_xform)
{
	if (xform == NULL) {
		*mode = IPSEC_MB_OP_NOT_SUPPORTED;
		return -ENOTSUP;
	}

	const struct rte_crypto_sym_xform *next = xform->next;

	if (xform->type == RTE_CRYPTO_SYM_XFORM_CIPHER) {
		if (next == NULL) {
			if (xform->cipher.op == RTE_CRYPTO_CIPHER_OP_ENCRYPT) {
				*mode = IPSEC_MB_OP_ENCRYPT_ONLY;
				*cipher_xform = xform;
				*auth_xform = NULL;
				return 0;
			}
			*mode = IPSEC_MB_OP_DECRYPT_ONLY;
			*cipher_xform = xform;
			*auth_xform = NULL;
			return 0;
		}

		if (next->type != RTE_CRYPTO_SYM_XFORM_AUTH) {
			*mode = IPSEC_MB_OP_NOT_SUPPORTED;
			return -ENOTSUP;
		}

		if (xform->cipher.op == RTE_CRYPTO_CIPHER_OP_ENCRYPT) {
			if (next->auth.op != RTE_CRYPTO_AUTH_OP_GENERATE) {
				*mode = IPSEC_MB_OP_NOT_SUPPORTED;
				return -ENOTSUP;
			}

			*mode = IPSEC_MB_OP_ENCRYPT_THEN_HASH_GEN;
			*cipher_xform = xform;
			*auth_xform = xform->next;
			return 0;
		}
		if (next->auth.op != RTE_CRYPTO_AUTH_OP_VERIFY) {
			*mode = IPSEC_MB_OP_NOT_SUPPORTED;
			return -ENOTSUP;
		}

		*mode = IPSEC_MB_OP_DECRYPT_THEN_HASH_VERIFY;
		*cipher_xform = xform;
		*auth_xform = xform->next;
		return 0;
	}

	if (xform->type == RTE_CRYPTO_SYM_XFORM_AUTH) {
		if (next == NULL) {
			if (xform->auth.op == RTE_CRYPTO_AUTH_OP_GENERATE) {
				*mode = IPSEC_MB_OP_HASH_GEN_ONLY;
				*auth_xform = xform;
				*cipher_xform = NULL;
				return 0;
			}
			*mode = IPSEC_MB_OP_HASH_VERIFY_ONLY;
			*auth_xform = xform;
			*cipher_xform = NULL;
			return 0;
		}

		if (next->type != RTE_CRYPTO_SYM_XFORM_CIPHER) {
			*mode = IPSEC_MB_OP_NOT_SUPPORTED;
			return -ENOTSUP;
		}

		if (xform->auth.op == RTE_CRYPTO_AUTH_OP_GENERATE) {
			if (next->cipher.op != RTE_CRYPTO_CIPHER_OP_ENCRYPT) {
				*mode = IPSEC_MB_OP_NOT_SUPPORTED;
				return -ENOTSUP;
			}

			*mode = IPSEC_MB_OP_HASH_GEN_THEN_ENCRYPT;
			*auth_xform = xform;
			*cipher_xform = xform->next;
			return 0;
		}
		if (next->cipher.op != RTE_CRYPTO_CIPHER_OP_DECRYPT) {
			*mode = IPSEC_MB_OP_NOT_SUPPORTED;
			return -ENOTSUP;
		}

		*mode = IPSEC_MB_OP_HASH_VERIFY_THEN_DECRYPT;
		*auth_xform = xform;
		*cipher_xform = xform->next;
		return 0;
	}

	if (xform->type == RTE_CRYPTO_SYM_XFORM_AEAD) {
		if (xform->aead.op == RTE_CRYPTO_AEAD_OP_ENCRYPT) {
			/*
			 * CCM requires to hash first and cipher later
			 * when encrypting
			 */
			if (xform->aead.algo == RTE_CRYPTO_AEAD_AES_CCM) {
				*mode = IPSEC_MB_OP_AEAD_AUTHENTICATED_DECRYPT;
				*aead_xform = xform;
				return 0;
				} else {
					*mode =
				IPSEC_MB_OP_AEAD_AUTHENTICATED_ENCRYPT;
					*aead_xform = xform;
					return 0;
				}
		} else {
			if (xform->aead.algo == RTE_CRYPTO_AEAD_AES_CCM) {
				*mode = IPSEC_MB_OP_AEAD_AUTHENTICATED_ENCRYPT;
				*aead_xform = xform;
				return 0;
			}
			*mode = IPSEC_MB_OP_AEAD_AUTHENTICATED_DECRYPT;
			*aead_xform = xform;
			return 0;
		}
	}

	*mode = IPSEC_MB_OP_NOT_SUPPORTED;
	return -ENOTSUP;
}

/** Device creation function */
int
ipsec_mb_create(struct rte_vdev_device *vdev,
	enum ipsec_mb_pmd_types pmd_type);

/** Device remove function */
int
ipsec_mb_remove(struct rte_vdev_device *vdev);

/** Configure queue pair PMD type specific data */
typedef int (*ipsec_mb_queue_pair_configure_t)(struct ipsec_mb_qp *qp);

/** Configure session PMD type specific data */
typedef int (*ipsec_mb_session_configure_t)(IMB_MGR *mbr_mgr,
		void *session_private,
		const struct rte_crypto_sym_xform *xform);

/** Configure internals PMD type specific data */
typedef int (*ipsec_mb_dev_configure_t)(struct rte_cryptodev *dev);

/** Per PMD type operation and data */
struct ipsec_mb_internals {
	uint8_t is_configured;
	dequeue_pkt_burst_t dequeue_burst;
	ipsec_mb_dev_configure_t dev_config;
	ipsec_mb_queue_pair_configure_t queue_pair_configure;
	ipsec_mb_session_configure_t session_configure;
	const struct rte_cryptodev_capabilities *caps;
	struct rte_cryptodev_ops *ops;
	struct rte_security_ops *security_ops;
	uint64_t feature_flags;
	uint32_t session_priv_size;
	uint32_t qp_priv_size;
	uint32_t internals_priv_size;
};

/** Global PMD type specific data */
extern struct ipsec_mb_internals ipsec_mb_pmds[IPSEC_MB_N_PMD_TYPES];

int
ipsec_mb_config(struct rte_cryptodev *dev,
	struct rte_cryptodev_config *config);

int
ipsec_mb_start(struct rte_cryptodev *dev);

void
ipsec_mb_stop(struct rte_cryptodev *dev);

int
ipsec_mb_close(struct rte_cryptodev *dev);

void
ipsec_mb_stats_get(struct rte_cryptodev *dev,
		struct rte_cryptodev_stats *stats);

void
ipsec_mb_stats_reset(struct rte_cryptodev *dev);

void
ipsec_mb_info_get(struct rte_cryptodev *dev,
		struct rte_cryptodev_info *dev_info);

int
ipsec_mb_qp_release(struct rte_cryptodev *dev, uint16_t qp_id);

int
ipsec_mb_qp_set_unique_name(struct rte_cryptodev *dev, struct ipsec_mb_qp *qp);

int
ipsec_mb_qp_setup(struct rte_cryptodev *dev, uint16_t qp_id,
				 const struct rte_cryptodev_qp_conf *qp_conf,
				 int socket_id);

/** Returns the size of the aesni multi-buffer session structure */
unsigned
ipsec_mb_sym_session_get_size(struct rte_cryptodev *dev);

/** Configure an aesni multi-buffer session from a crypto xform chain */
int ipsec_mb_sym_session_configure(
	struct rte_cryptodev *dev,
	struct rte_crypto_sym_xform *xform,
	struct rte_cryptodev_sym_session *sess,
	struct rte_mempool *mempool);

/** Clear the memory of session so it does not leave key material behind */
void
ipsec_mb_sym_session_clear(struct rte_cryptodev *dev,
				struct rte_cryptodev_sym_session *sess);

/** Get session from op. If sessionless create a session */
static __rte_always_inline void *
ipsec_mb_get_session_private(struct ipsec_mb_qp *qp, struct rte_crypto_op *op)
{
	void *sess = NULL;
	uint32_t driver_id = ipsec_mb_get_driver_id(qp->pmd_type);
	struct rte_crypto_sym_op *sym_op = op->sym;
	uint8_t sess_type = op->sess_type;
	void *_sess;
	void *_sess_private_data = NULL;
	struct ipsec_mb_internals *pmd_data = &ipsec_mb_pmds[qp->pmd_type];

	switch (sess_type) {
	case RTE_CRYPTO_OP_WITH_SESSION:
		if (likely(sym_op->session != NULL))
			sess = get_sym_session_private_data(sym_op->session,
							    driver_id);
	break;
	case RTE_CRYPTO_OP_SESSIONLESS:
		if (!qp->sess_mp ||
		    rte_mempool_get(qp->sess_mp, (void **)&_sess))
			return NULL;

		if (!qp->sess_mp_priv ||
		    rte_mempool_get(qp->sess_mp_priv,
					(void **)&_sess_private_data))
			return NULL;

		sess = _sess_private_data;
		if (unlikely(pmd_data->session_configure(qp->mb_mgr,
				sess, sym_op->xform) != 0)) {
			rte_mempool_put(qp->sess_mp, _sess);
			rte_mempool_put(qp->sess_mp_priv, _sess_private_data);
			sess = NULL;
		}

		sym_op->session = (struct rte_cryptodev_sym_session *)_sess;
		set_sym_session_private_data(sym_op->session, driver_id,
					     _sess_private_data);
	break;
	default:
		IPSEC_MB_LOG(ERR, "Unrecognized session type %u", sess_type);
	}

	if (unlikely(sess == NULL))
		op->status = RTE_CRYPTO_OP_STATUS_INVALID_SESSION;

	return sess;
}

#endif /* _IPSEC_MB_PRIVATE_H_ */
