/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Broadcom
 * All rights reserved.
 */

/*
 * This header file defines the Portability structures and APIs for
 * TruFlow.
 */

#ifndef _TFP_H_
#define _TFP_H_

#include <rte_config.h>
#include <rte_spinlock.h>
#include <rte_log.h>
#include <rte_byteorder.h>

/**
 * DPDK/Driver specific log level for the BNXT Eth driver.
 */
extern int bnxt_logtype_driver;

/** Spinlock
 */
struct tfp_spinlock_parms {
	rte_spinlock_t slock;
};

#define TFP_DRV_LOG_RAW(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, bnxt_logtype_driver, "%s(): " fmt, \
		__func__, ## args)

#define TFP_DRV_LOG(level, fmt, args...) \
	TFP_DRV_LOG_RAW(level, fmt, ## args)

/**
 * @file
 *
 * TrueFlow Portability API Header File
 */

/**
 * send message parameter definition
 */
struct tfp_send_msg_parms {
	/**
	 * [in] mailbox, specifying the Mailbox to send the command on.
	 */
	uint32_t  mailbox;
	/**
	 * [in] tlv_subtype, specifies the tlv_type.
	 */
	uint16_t  tf_type;
	/**
	 * [in] tlv_subtype, specifies the tlv_subtype.
	 */
	uint16_t  tf_subtype;
	/**
	 * [out] tf_resp_code, response code from the internal tlv
	 *       message. Only supported on tunneled messages.
	 */
	uint32_t tf_resp_code;
	/**
	 * [out] size, number specifying the request size of the data in bytes
	 */
	uint32_t req_size;
	/**
	 * [in] data, pointer to the data to be sent within the HWRM command
	 */
	uint32_t *req_data;
	/**
	 * [out] size, number specifying the response size of the data in bytes
	 */
	uint32_t resp_size;
	/**
	 * [out] data, pointer to the data to be sent within the HWRM command
	 */
	uint32_t *resp_data;
};

/**
 * calloc parameter definition
 */
struct tfp_calloc_parms {
	/**
	 * [in] nitems, number specifying number of items to allocate.
	 */
	size_t nitems;
	/**
	 * [in] size, number specifying the size of each memory item
	 *      requested. Size is in bytes.
	 */
	size_t size;
	/**
	 * [in] alignment, number indicates byte alignment required. 0
	 *      - don't care, 16 - 16 byte alignment, 4K - 4K alignment etc
	 */
	size_t alignment;
	/**
	 * [out] mem_va, pointer to the allocated memory.
	 */
	void *mem_va;
	/**
	 * [out] mem_pa, physical address of the allocated memory.
	 */
	void *mem_pa;
};

/**
 * @page Portability
 *
 * @ref tfp_send_direct
 * @ref tfp_send_msg_tunneled
 *
 * @ref tfp_calloc
 * @ref tfp_memcpy
 * @ref tfp_free
 *
 * @ref tfp_spinlock_init
 * @ref tfp_spinlock_lock
 * @ref tfp_spinlock_unlock
 *
 */

/**
 * Provides communication capability from the TrueFlow API layer to
 * the TrueFlow firmware. The portability layer internally provides
 * the transport to the firmware.
 *
 * [in] session, pointer to session handle
 * [in] parms, parameter structure
 *
 * Returns:
 *   0              - Success
 *   -1             - Global error like not supported
 *   -EINVAL        - Parameter Error
 */
int tfp_send_msg_direct(struct tf *tfp,
			struct tfp_send_msg_parms *parms);

/**
 * Provides communication capability from the TrueFlow API layer to
 * the TrueFlow firmware. The portability layer internally provides
 * the transport to the firmware.
 *
 * [in] session, pointer to session handle
 * [in] parms, parameter structure
 *
 * Returns:
 *   0              - Success
 *   -1             - Global error like not supported
 *   -EINVAL        - Parameter Error
 */
int tfp_send_msg_tunneled(struct tf *tfp,
			  struct tfp_send_msg_parms *parms);

/**
 * Sends OEM command message to Chimp
 *
 * [in] session, pointer to session handle
 * [in] max_flows, max number of flows requested
 *
 * Returns:
 *   0              - Success
 *   -1             - Global error like not supported
 *   -EINVAL        - Parameter Error
 */
int
tfp_msg_hwrm_oem_cmd(struct tf *tfp,
		     uint32_t max_flows);

/**
 * Sends OEM command message to Chimp
 *
 * [in] session, pointer to session handle
 * [in] max_flows, max number of flows requested
 *
 * Returns:
 *   0              - Success
 *   -1             - Global error like not supported
 *   -EINVAL        - Parameter Error
 */
int
tfp_msg_hwrm_oem_cmd(struct tf *tfp,
		     uint32_t max_flows);

/**
 * Allocates zero'ed memory from the heap.
 *
 * NOTE: Also performs virt2phy address conversion by default thus is
 * can be expensive to invoke.
 *
 * [in] parms, parameter structure
 *
 * Returns:
 *   0              - Success
 *   -ENOMEM        - No memory available
 *   -EINVAL        - Parameter error
 */
int tfp_calloc(struct tfp_calloc_parms *parms);
void tfp_memcpy(void *dest, void *src, size_t n);
void tfp_free(void *addr);

void tfp_spinlock_init(struct tfp_spinlock_parms *slock);
void tfp_spinlock_lock(struct tfp_spinlock_parms *slock);
void tfp_spinlock_unlock(struct tfp_spinlock_parms *slock);

/**
 * Lookup of the FID in the platform specific structure.
 *
 * [in] session
 *   Pointer to session handle
 *
 * [out] fw_fid
 *   Pointer to the fw_fid
 *
 * Returns:
 *   0       - Success
 *   -EINVAL - Parameter error
 */
int tfp_get_fid(struct tf *tfp, uint16_t *fw_fid);


/*
 * @ref tfp_cpu_to_le_16
 * @ref tfp_le_to_cpu_16
 * @ref tfp_cpu_to_le_32
 * @ref tfp_le_to_cpu_32
 * @ref tfp_cpu_to_le_64
 * @ref tfp_le_to_cpu_64
 * @ref tfp_cpu_to_be_16
 * @ref tfp_be_to_cpu_16
 * @ref tfp_cpu_to_be_32
 * @ref tfp_be_to_cpu_32
 * @ref tfp_cpu_to_be_64
 * @ref tfp_be_to_cpu_64
 */

#define tfp_cpu_to_le_16(val) rte_cpu_to_le_16(val)
#define tfp_le_to_cpu_16(val) rte_le_to_cpu_16(val)
#define tfp_cpu_to_le_32(val) rte_cpu_to_le_32(val)
#define tfp_le_to_cpu_32(val) rte_le_to_cpu_32(val)
#define tfp_cpu_to_le_64(val) rte_cpu_to_le_64(val)
#define tfp_le_to_cpu_64(val) rte_le_to_cpu_64(val)
#define tfp_cpu_to_be_16(val) rte_cpu_to_be_16(val)
#define tfp_be_to_cpu_16(val) rte_be_to_cpu_16(val)
#define tfp_cpu_to_be_32(val) rte_cpu_to_be_32(val)
#define tfp_be_to_cpu_32(val) rte_be_to_cpu_32(val)
#define tfp_cpu_to_be_64(val) rte_cpu_to_be_64(val)
#define tfp_be_to_cpu_64(val) rte_be_to_cpu_64(val)
#define tfp_bswap_16(val) rte_bswap16(val)
#define tfp_bswap_32(val) rte_bswap32(val)
#define tfp_bswap_64(val) rte_bswap64(val)

/**
 * Lookup of the FID in the platform specific structure.
 *
 * [in] session
 *   Pointer to session handle
 *
 * [out] fw_fid
 *   Pointer to the fw_fid
 *
 * Returns:
 *   0       - Success
 *   -EINVAL - Parameter error
 */
int tfp_get_fid(struct tf *tfp, uint16_t *fw_fid);

#endif /* _TFP_H_ */
