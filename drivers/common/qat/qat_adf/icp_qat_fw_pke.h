/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

/**
 * @file icp_qat_fw_pke.h
 * @defgroup icp_qat_fw_pke ICP QAT FW PKE Processing Definitions
 * @ingroup icp_qat_fw
 * Revision: 0.1
 * @brief
 *      This file documents the external interfaces that the QAT FW running
 *      on the QAT Acceleration Engine provides to clients wanting to
 *      accelerate crypto asymmetric applications
 */

#ifndef _ICP_QAT_FW_PKE_H_
#define _ICP_QAT_FW_PKE_H_

/*
 * Keep all dpdk-specific changes in this section
 */

#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

/* End of DPDK-specific section
 * Don't modify below this.
 */

/*
 ****************************************************************************
 * Include local header files
 ****************************************************************************
 */
#include "icp_qat_fw.h"

/**
 *****************************************************************************
 *
 * @ingroup icp_qat_fw_pke
 *
 * @brief
 *      PKE response status field structure contained
 *      within LW1, comprising the common error codes and
 *      the response flags.
 *
 *****************************************************************************/
struct icp_qat_fw_pke_resp_status {
	u8 comn_err_code;
	/**< 8 bit common error code */

	u8 pke_resp_flags;
	/**< 8-bit PKE response flags  */
};

/**
 *****************************************************************************
 * @ingroup icp_qat_fw_pke
 *      Definition of the QAT FW PKE request header pars field.
 *
 * @description
 *      PKE request message header pars structure
 *
 *****************************************************************************/
struct icp_qat_fw_req_hdr_pke_cd_pars {
	/**< LWs 2-3 */
	u64 content_desc_addr;
	/**< Content descriptor pointer */

	/**< LW 4 */
	u32 content_desc_resrvd;
	/**< Content descriptor reserved field */

	/**< LW 5 */
	u32 func_id;
	/**< MMP functionality Id */
};

/**
 *****************************************************************************
 * @ingroup icp_qat_fw_pke
 *      Definition of the QAT FW PKE request header mid section.
 *
 * @description
 *      PKE request message header middle structure
 *
 *****************************************************************************/
struct icp_qat_fw_req_pke_mid {
	/**< LWs 6-11 */
	u64 opaque;
	/**< Opaque data passed unmodified from the request to response messages
	 * by firmware (fw)
	 */

	u64 src_data_addr;
	/**< Generic definition of the source data supplied to the QAT AE. The
	 * common flags are used to further describe the attributes of this
	 * field
	 */

	u64 dest_data_addr;
	/**< Generic definition of the destination data supplied to the QAT AE.
	 * The common flags are used to further describe the attributes of this
	 * field
	 */
};

/**
 *****************************************************************************
 * @ingroup icp_qat_fw_pke
 *      Definition of the QAT FW PKE request header.
 *
 * @description
 *      PKE request message header structure
 *
 *****************************************************************************/
struct icp_qat_fw_req_pke_hdr {
	/**< LW0 */
	u8 resrvd1;
	/**< reserved field */

	u8 resrvd2;
	/**< reserved field */

	u8 service_type;
	/**< Service type */

	u8 hdr_flags;
	/**< This represents a flags field for the Service Request.
	 * The most significant bit is the 'valid' flag and the only
	 * one used. All remaining bit positions are unused and
	 * are therefore reserved and need to be set to 0.
	 */

	/**< LW1 */
	u16 comn_req_flags;
	/**< Common Request flags must indicate flat buffer
	 * Common Request flags - PKE slice flags no longer used - slice
	 * allocated to a threadstrand.
	 */

	u8 kpt_mask;
	/** < KPT input parameters array mask, indicate which node in array is
	 *encrypted
	 */

	u8 kpt_rn_mask;
	/**< KPT random node(RN) mask - indicate which node is RN that QAT
	 * should generate itself.
	 */

	/**< LWs 2-5 */
	struct icp_qat_fw_req_hdr_pke_cd_pars cd_pars;
	/**< PKE request message header pars structure */
};

/**
 ***************************************************************************
 *
 * @ingroup icp_qat_fw_pke
 *
 * @brief
 *      PKE request message structure (64 bytes)
 *
 *****************************************************************************/
struct icp_qat_fw_pke_request {
	/**< LWs 0-5 */
	struct icp_qat_fw_req_pke_hdr pke_hdr;
	/**< Request header for PKE - CD Header/Param size  must be zero */

	/**< LWs 6-11 */
	struct icp_qat_fw_req_pke_mid pke_mid;
	/**< Request middle section for PKE */

	/**< LW 12 */
	u8 output_param_count;
	/**< Number of output large integers for request */

	u8 input_param_count;
	/**< Number of input large integers for request */

	u16 resrvd1;
	/** Reserved **/

	/**< LW 13 */
	u32 resrvd2;
	/**< Reserved */

	/**< LWs 14-15 */
	u64 next_req_adr;
	/** < PKE - next request address */
};

/**
 *****************************************************************************
 *
 * @ingroup icp_qat_fw_pke
 *
 * @brief
 *      PKE response message header structure
 *
 *****************************************************************************/
struct icp_qat_fw_resp_pke_hdr {
	/**< LW0 */
	u8 resrvd1;
	/**< Reserved */

	u8 resrvd2;
	/**< Reserved */

	u8 response_type;
	/**< Response type - copied from the request to the response message */

	u8 hdr_flags;
	/**< This represents a flags field for the Response.
	 * The most significant bit is the 'valid' flag and the only
	 * one used. All remaining bit positions are unused and
	 * are therefore reserved
	 */

	/**< LW1 */
	struct icp_qat_fw_pke_resp_status resp_status;

	u16 resrvd4;
	/**< Set to zero. */
};

/**
 *****************************************************************************
 *
 * @ingroup icp_qat_fw_pke
 *
 * @brief
 *      PKE response message structure (32 bytes)
 *
 *****************************************************************************/
struct icp_qat_fw_pke_resp {
	/**< LWs 0-1 */
	struct icp_qat_fw_resp_pke_hdr pke_resp_hdr;
	/**< Response header for PKE */

	/**< LWs 2-3 */
	u64 opaque;
	/**< Opaque data passed from the request to the response message */

	/**< LWs 4-5 */
	u64 src_data_addr;
	/**< Generic definition of the source data supplied to the QAT AE. The
	 * common flags are used to further describe the attributes of this
	 * field
	 */

	/**< LWs 6-7 */
	u64 dest_data_addr;
	/**< Generic definition of the destination data supplied to the QAT AE.
	 * The common flags are used to further describe the attributes of this
	 * field
	 */
};

/* ========================================================================= */
/* MACRO DEFINITIONS                                                         */
/* ========================================================================= */

/**< @ingroup icp_qat_fw_pke
 * Macro defining the bit position and mask of the 'valid' flag, within the
 * hdr_flags field of LW0 (service request and response) of the PKE request
 */
#define ICP_QAT_FW_PKE_HDR_VALID_FLAG_BITPOS 7
#define ICP_QAT_FW_PKE_HDR_VALID_FLAG_MASK 0x1

/**< @ingroup icp_qat_fw_pke
 * Macro defining the bit position and mask of the PKE status flag, within the
 * status field LW1 of a PKE response message
 */
#define QAT_COMN_RESP_PKE_STATUS_BITPOS 6
/**< @ingroup icp_qat_fw_pke
 * Starting bit position indicating the PKE status flag within the PKE response
 * pke_resp_flags byte.
 */

#define QAT_COMN_RESP_PKE_STATUS_MASK 0x1
/**< @ingroup icp_qat_fw_pke
 * One bit mask used to determine PKE status mask
 */

/*
 *  < @ingroup icp_qat_fw_pke
 *  *** PKE Response Status Field Definition ***
 *  The PKE response follows the CPM 1.5 message format. The status field is
 *  16 bits wide, where the status flags are contained within the most
 *  significant byte of the icp_qat_fw_pke_resp_status structure.
 *  The lower 8 bits of this word now contain the common error codes,
 *  which are defined in the common header file(*).
 */
/*  +=====+-----+----+-----+-----+-----+-----+-----+-----+---------------------+
 *  | Bit |  15 | 14 | 13  | 12  | 11  | 10  |  9  |  8  |    [7....0]         |
 *  +=====+-----+----+-----+-----+-----+-----+-----+-----+---------------------+
 *  |Flags|Rsrvd|Pke |Rsrvd|Rsrvd|Rsrvd|Rsrvd|Rsrvd|Rsrvd|Common error codes(*)|
 *  +=====+-----+----+-----+-----+-----+-----+-----+-----+---------------------+
 */

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_pke
 *
 * @description
 *     Macro for extraction of the PKE bit from the 16-bit status field
 *     particular to a PKE response. The status flags are contained within
 *     the most significant byte of the word. The lower 8 bits of this status
 *     word now contain the common error codes, which are defined in the common
 *     header file. The appropriate macro definition to extract the PKE status
 *     lag from the PKE response assumes that a single byte i.e. pke_resp_flags
 *     is passed to the macro.
 *
 * @param status
 *     Status to extract the PKE status bit
 *
 *****************************************************************************/
#define ICP_QAT_FW_PKE_RESP_PKE_STAT_GET(flags)                                \
	QAT_FIELD_GET((flags), QAT_COMN_RESP_PKE_STATUS_BITPOS,                \
		      QAT_COMN_RESP_PKE_STATUS_MASK)

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_pke
 *
 * @description
 *      Extract the valid flag from the PKE Request's header flags. Note that
 *      this invokes the common macro which may be used by either the request
 *      or the response.
 *
 * @param icp_qat_fw_req_pke_hdr    Structure passed to extract the valid bit
 *                                  from the 'hdr_flags' field.
 *
 *****************************************************************************/
#define ICP_QAT_FW_PKE_RQ_VALID_FLAG_GET(icp_qat_fw_req_pke_hdr)               \
	ICP_QAT_FW_PKE_HDR_VALID_FLAG_GET(icp_qat_fw_req_pke_hdr)

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_pke
 *
 * @description
 *      Set the valid bit in the PKE Request's header flags. Note that
 *      this invokes the common macro which may be used by either the request
 *      or the response.
 *
 * @param icp_qat_fw_req_pke_hdr    Structure passed to set the valid bit.
 * @param val    Value of the valid bit flag.
 *
 *****************************************************************************/
#define ICP_QAT_FW_PKE_RQ_VALID_FLAG_SET(icp_qat_fw_req_pke_hdr, val)          \
	ICP_QAT_FW_PKE_HDR_VALID_FLAG_SET(icp_qat_fw_req_pke_hdr, val)

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_pke
 *
 * @description
 *      Extract the valid flag from the PKE Response's header flags. Note that
 *      invokes the common macro which may be used by either the request
 *      or the response.
 *
 * @param icp_qat_fw_resp_pke_hdr    Structure to extract the valid bit
 *                                   from the 'hdr_flags' field.
 *
 *****************************************************************************/
#define ICP_QAT_FW_PKE_RESP_VALID_FLAG_GET(icp_qat_fw_resp_pke_hdr)            \
	ICP_QAT_FW_PKE_HDR_VALID_FLAG_GET(icp_qat_fw_resp_pke_hdr)

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_pke
 *
 * @description
 *      Set the valid bit in the PKE Response's header flags. Note that
 *      this invokes the common macro which may be used by either the
 *      request or the response.
 *
 * @param icp_qat_fw_resp_pke_hdr    Structure to set the valid bit
 * @param val    Value of the valid bit flag.
 *
 *****************************************************************************/
#define ICP_QAT_FW_PKE_RESP_VALID_FLAG_SET(icp_qat_fw_resp_pke_hdr, val)       \
	ICP_QAT_FW_PKE_HDR_VALID_FLAG_SET(icp_qat_fw_resp_pke_hdr, val)

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_pke
 *
 * @description
 *      Common macro to extract the valid flag from the header flags field
 *      within the header structure (request or response).
 *
 * @param hdr    Structure (request or response) to extract the
 *               valid bit from the 'hdr_flags' field.
 *
 *****************************************************************************/
#define ICP_QAT_FW_PKE_HDR_VALID_FLAG_GET(hdr)                                 \
	QAT_FIELD_GET(hdr.hdr_flags, ICP_QAT_FW_PKE_HDR_VALID_FLAG_BITPOS,     \
		      ICP_QAT_FW_PKE_HDR_VALID_FLAG_MASK)

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_pke
 *
 * @description
 *      Common macro to set the valid bit in the header flags field within
 *      the header structure (request or response).
 *
 * @param hdr    Structure (request or response) containing the header
 *               flags field, to allow the valid bit to be set.
 * @param val    Value of the valid bit flag.
 *
 *****************************************************************************/
#define ICP_QAT_FW_PKE_HDR_VALID_FLAG_SET(hdr, val)                            \
	QAT_FIELD_SET((hdr.hdr_flags), (val),                                  \
		      ICP_QAT_FW_PKE_HDR_VALID_FLAG_BITPOS,                    \
		      ICP_QAT_FW_PKE_HDR_VALID_FLAG_MASK)

#endif /* _ICP_QAT_FW_PKE_H_ */
