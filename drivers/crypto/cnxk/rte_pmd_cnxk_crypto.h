/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2024 Marvell.
 */

/**
 * @file rte_pmd_cnxk_crypto.h
 * Marvell CNXK Crypto PMD specific functions.
 *
 **/

#ifndef _PMD_CNXK_CRYPTO_H_
#define _PMD_CNXK_CRYPTO_H_

#include <stdbool.h>
#include <stdint.h>

#include <rte_compat.h>
#include <rte_crypto.h>
#include <rte_security.h>

/* Forward declarations */

/**
 * @brief Crypto CNXK PMD QPTR opaque pointer.
 *
 * This structure represents the queue pair structure that would be the input to APIs that use
 * hardware queues.
 */
struct rte_pmd_cnxk_crypto_qptr;

/**
 * @brief Crypto CNXK PMD CPTR opaque pointer.
 *
 * This structure represents the context pointer that would be used to store the hardware context.
 */
struct rte_pmd_cnxk_crypto_cptr;

/**
 *
 * @brief Crypto CNXK queue pair stats.
 *
 * This structure represents the queue pair stats retrieved from CPT HW queue.
 */
struct rte_pmd_cnxk_crypto_qp_stats {
	/** Packet counter of the packets that used CPT context cache and was encrypted */
	uint64_t ctx_enc_pkts;
	/** Byte counter of the packets that used CPT context cache and was encrypted */
	uint64_t ctx_enc_bytes;
	/** Packet counter of the packets that used CPT context cache and was decrypted */
	uint64_t ctx_dec_pkts;
	/** Byte counter of the packets that used CPT context cache and was decrypted */
	uint64_t ctx_dec_bytes;
};

/**
 * @brief Crypto CNXK PMD session structure.
 *
 * This structure represents the session structure that would be used to store the session
 * information.
 */
struct rte_pmd_cnxk_crypto_sess {
	/** Crypto type (symmetric or asymmetric). */
	enum rte_crypto_op_type op_type;
	/** Session type (Crypto or security). */
	enum rte_crypto_op_sess_type sess_type;
	/** Session pointer. */
	union {
		/** Security session pointer. */
		struct rte_security_session *sec_sess;
		/** Crypto symmetric session pointer. */
		struct rte_cryptodev_sym_session *crypto_sym_sess;
		/** Crypto asymmetric session pointer */
		struct rte_cryptodev_asym_session *crypto_asym_sess;
	};
};

/**
 * Get queue pointer of a specific queue in a cryptodev.
 *
 * @param dev_id
 *   Device identifier of cryptodev device.
 * @param qp_id
 *   Index of the queue pair.
 * @return
 *   - On success, pointer to queue pair structure that would be the input to submit APIs.
 *   - NULL on error.
 */
__rte_experimental
struct rte_pmd_cnxk_crypto_qptr *rte_pmd_cnxk_crypto_qptr_get(uint8_t dev_id, uint16_t qp_id);

/**
 * Submit CPT instruction (cpt_inst_s) to hardware (CPT).
 *
 * The ``qp`` is a pointer obtained from ``rte_pmd_cnxk_crypto_qp_get``. Application should make
 * sure it doesn't overflow the internal hardware queues. It may do so by making sure the inflight
 * packets are not more than the number of descriptors configured.
 *
 * This API may be called only after the cryptodev and queue pair is configured and is started.
 *
 * @param qptr
 *   Pointer obtained with ``rte_pmd_cnxk_crypto_qptr_get``.
 * @param inst
 *   Pointer to an array of instructions prepared by application.
 * @param nb_inst
 *   Number of instructions.
 */
__rte_experimental
void rte_pmd_cnxk_crypto_submit(struct rte_pmd_cnxk_crypto_qptr *qptr, void *inst,
				uint16_t nb_inst);

/**
 * Flush the CPTR from CPT CTX cache.
 *
 * This API must be called only after the cryptodev and queue pair is configured and is started.
 *
 * @param qptr
 *   Pointer obtained with ``rte_pmd_cnxk_crypto_qptr_get``.
 * @param cptr
 *   Pointer obtained with ``rte_pmd_cnxk_crypto_cptr_get`` or any valid CPTR address that can be
 *   used with CPT CTX cache.
 * @param invalidate
 *   If true, invalidate the CTX cache entry. If false, flush the CTX cache entry.
 * @return
 *   - 0 on success.
 *   - Negative value on error.
 *     - -EINVAL if the input parameters are invalid.
 *     - -ENOTSUP if the operation is not supported.
 *     - -EAGAIN if the operation is not successful.
 *     - -EFAULT if the operation failed.
 */
__rte_experimental
int rte_pmd_cnxk_crypto_cptr_flush(struct rte_pmd_cnxk_crypto_qptr *qptr,
				   struct rte_pmd_cnxk_crypto_cptr *cptr,
				   bool invalidate);

/**
 * Get the HW CPTR pointer from the rte_crypto/rte_security session.
 *
 * @param rte_sess
 *   Pointer to the structure holding rte_cryptodev or rte_security session.
 * @return
 *   - On success, pointer to the HW CPTR.
 *   - NULL on error.
 */
__rte_experimental
struct rte_pmd_cnxk_crypto_cptr *rte_pmd_cnxk_crypto_cptr_get(
	struct rte_pmd_cnxk_crypto_sess *rte_sess);

/**
 * Read HW context (CPTR).
 *
 * @param qptr
 *   Pointer obtained with ``rte_pmd_cnxk_crypto_qptr_get``.
 * @param cptr
 *   Pointer obtained with ``rte_pmd_cnxk_crypto_cptr_get`` or any valid CPTR address that can be
 *   used with CPT CTX cache.
 * @param[out] data
 *   Destination pointer to copy CPTR context for application.
 * @param len
 *   Length of CPTR context to copy into data parameter.
 *
 * @return
 *   - 0 On success.
 *   - Negative value on error.
 *     - -EINVAL if the input parameters are invalid.
 *     - -ENOTSUP if the operation is not supported.
 *     - -EAGAIN if the operation is not successful.
 *     - -EFAULT if the operation failed.
 */
__rte_experimental
int rte_pmd_cnxk_crypto_cptr_read(struct rte_pmd_cnxk_crypto_qptr *qptr,
				  struct rte_pmd_cnxk_crypto_cptr *cptr, void *data,
				  uint32_t len);

/**
 * Write HW context (CPTR).
 *
 * @param qptr
 *  Pointer obtained with ``rte_pmd_cnxk_crypto_qptr_get``.
 * @param cptr
 *  Pointer obtained with ``rte_pmd_cnxk_crypto_cptr_get`` or any valid CPTR address that can be
 *  used with CPT CTX cache.
 * @param data
 *  Source pointer to copy CPTR context from application.
 * @param len
 *  Length of CPTR context to copy from data parameter.
 *
 * @return
 *   - 0 On success.
 *   - Negative value on error.
 *     - -EINVAL if the input parameters are invalid.
 *     - -ENOTSUP if the operation is not supported.
 *     - -EAGAIN if the operation is not successful.
 *     - -EFAULT if the operation failed.
 */
__rte_experimental
int rte_pmd_cnxk_crypto_cptr_write(struct rte_pmd_cnxk_crypto_qptr *qptr,
				   struct rte_pmd_cnxk_crypto_cptr *cptr, void *data,
				   uint32_t len);

/**
 * Get the HW Queue Pair (LF) stats.
 *
 * @param qptr
 *  Pointer obtained with ``rte_pmd_cnxk_crypto_qptr_get``.
 * @param[out] stats
 *  Pointer to the structure where stats will be copied.
 *
 * @return
 *   - 0 On success.
 *   - Negative value on error.
 *     - -EINVAL if the input parameters are invalid.
 */
__rte_experimental
int rte_pmd_cnxk_crypto_qp_stats_get(struct rte_pmd_cnxk_crypto_qptr *qptr,
				     struct rte_pmd_cnxk_crypto_qp_stats *stats);

/**
 * Retrieves the addresses of the AE FPM (Finite Precision Math) tables.
 *
 * This API should be called only after the cryptodev device has been
 * successfully configured. The returned pointer reference memory that is
 * valid as long as the device remains configured and is not destroyed or
 * reconfigured. If the device is reconfigured or destroyed, the memory
 * referenced by the returned pointer becomes invalid and must not be used.
 *
 * @param dev_id
 *   Device identifier of cryptodev device.
 * @return
 *   - On success pointer to the AE FPM table addresses.
 *   - NULL on error.
 */
__rte_experimental
const uint64_t *rte_pmd_cnxk_ae_fpm_table_get(uint8_t dev_id);

#endif /* _PMD_CNXK_CRYPTO_H_ */
