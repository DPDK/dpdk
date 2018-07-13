/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 * Copyright(c) 2015-2018 Intel Corporation
 */
#ifndef _ICP_QAT_FW_COMP_H_
#define _ICP_QAT_FW_COMP_H_

#include "icp_qat_fw.h"

enum icp_qat_fw_comp_cmd_id {
	ICP_QAT_FW_COMP_CMD_STATIC = 0,
	/*!< Static Compress Request */

	ICP_QAT_FW_COMP_CMD_DYNAMIC = 1,
	/*!< Dynamic Compress Request */

	ICP_QAT_FW_COMP_CMD_DECOMPRESS = 2,
	/*!< Decompress Request */

	ICP_QAT_FW_COMP_CMD_DELIMITER
	/**< Delimiter type */
};

/**< Flag usage */

#define ICP_QAT_FW_COMP_STATELESS_SESSION 0
/**< @ingroup icp_qat_fw_comp
 *  Flag representing that session is stateless
 */

#define ICP_QAT_FW_COMP_STATEFUL_SESSION 1
/**< @ingroup icp_qat_fw_comp
 *  Flag representing that session is stateful
 */

#define ICP_QAT_FW_COMP_NOT_AUTO_SELECT_BEST 0
/**< @ingroup icp_qat_fw_comp
 * Flag representing that autoselectbest is NOT used
 */

#define ICP_QAT_FW_COMP_AUTO_SELECT_BEST 1
/**< @ingroup icp_qat_fw_comp
 * Flag representing that autoselectbest is used
 */

#define ICP_QAT_FW_COMP_NOT_ENH_AUTO_SELECT_BEST 0
/**< @ingroup icp_qat_fw_comp
 * Flag representing that enhanced autoselectbest is NOT used
 */

#define ICP_QAT_FW_COMP_ENH_AUTO_SELECT_BEST 1
/**< @ingroup icp_qat_fw_comp
 * Flag representing that enhanced autoselectbest is used
 */

#define ICP_QAT_FW_COMP_NOT_DISABLE_TYPE0_ENH_AUTO_SELECT_BEST 0
/**< @ingroup icp_qat_fw_comp
 * Flag representing that enhanced autoselectbest is NOT used
 */

#define ICP_QAT_FW_COMP_DISABLE_TYPE0_ENH_AUTO_SELECT_BEST 1
/**< @ingroup icp_qat_fw_comp
 * Flag representing that enhanced autoselectbest is used
 */

#define ICP_QAT_FW_COMP_DISABLE_SECURE_RAM_USED_AS_INTMD_BUF 1
/**< @ingroup icp_qat_fw_comp
 * Flag representing secure RAM from being used as
 * an intermediate buffer is DISABLED.
 */

#define ICP_QAT_FW_COMP_ENABLE_SECURE_RAM_USED_AS_INTMD_BUF 0
/**< @ingroup icp_qat_fw_comp
 * Flag representing secure RAM from being used as
 * an intermediate buffer is ENABLED.
 */

/**< Flag mask & bit position */

#define ICP_QAT_FW_COMP_SESSION_TYPE_BITPOS 2
/**< @ingroup icp_qat_fw_comp
 * Starting bit position for the session type
 */

#define ICP_QAT_FW_COMP_SESSION_TYPE_MASK 0x1
/**< @ingroup icp_qat_fw_comp
 * One bit mask used to determine the session type
 */

#define ICP_QAT_FW_COMP_AUTO_SELECT_BEST_BITPOS 3
/**< @ingroup icp_qat_fw_comp
 * Starting bit position for auto select best
 */

#define ICP_QAT_FW_COMP_AUTO_SELECT_BEST_MASK 0x1
/**< @ingroup icp_qat_fw_comp
 * One bit mask for auto select best
 */

#define ICP_QAT_FW_COMP_ENHANCED_AUTO_SELECT_BEST_BITPOS 4
/**< @ingroup icp_qat_fw_comp
 * Starting bit position for enhanced auto select best
 */

#define ICP_QAT_FW_COMP_ENHANCED_AUTO_SELECT_BEST_MASK 0x1
/**< @ingroup icp_qat_fw_comp
 * One bit mask for enhanced auto select best
 */

#define ICP_QAT_FW_COMP_RET_DISABLE_TYPE0_HEADER_DATA_BITPOS 5
/**< @ingroup icp_qat_fw_comp
 * Starting bit position for disabling type zero header write back
 * when Enhanced autoselect best is enabled. If set firmware does
 * not return type0 store block header, only copies src to dest.
 * (if best output is Type0)
 */

#define ICP_QAT_FW_COMP_RET_DISABLE_TYPE0_HEADER_DATA_MASK 0x1
/**< @ingroup icp_qat_fw_comp
 * One bit mask for auto select best
 */

#define ICP_QAT_FW_COMP_DISABLE_SECURE_RAM_AS_INTMD_BUF_BITPOS 7
/**< @ingroup icp_qat_fw_comp
 * Starting bit position for flag used to disable secure ram from
 *  being used as an intermediate buffer.
 */

#define ICP_QAT_FW_COMP_DISABLE_SECURE_RAM_AS_INTMD_BUF_MASK 0x1
/**< @ingroup icp_qat_fw_comp
 * One bit mask for disable secure ram for use as an intermediate
 * buffer.
 */

#define ICP_QAT_FW_COMP_FLAGS_BUILD(sesstype, autoselect, enhanced_asb,        \
				    ret_uncomp, secure_ram)                    \
	((((sesstype)&ICP_QAT_FW_COMP_SESSION_TYPE_MASK)                       \
	  << ICP_QAT_FW_COMP_SESSION_TYPE_BITPOS) |                            \
	 (((autoselect)&ICP_QAT_FW_COMP_AUTO_SELECT_BEST_MASK)                 \
	  << ICP_QAT_FW_COMP_AUTO_SELECT_BEST_BITPOS) |                        \
	 (((enhanced_asb)&ICP_QAT_FW_COMP_ENHANCED_AUTO_SELECT_BEST_MASK)      \
	  << ICP_QAT_FW_COMP_ENHANCED_AUTO_SELECT_BEST_BITPOS) |               \
	 (((ret_uncomp)&ICP_QAT_FW_COMP_RET_DISABLE_TYPE0_HEADER_DATA_MASK)    \
	  << ICP_QAT_FW_COMP_RET_DISABLE_TYPE0_HEADER_DATA_BITPOS) |           \
	 (((secure_ram)&ICP_QAT_FW_COMP_DISABLE_SECURE_RAM_AS_INTMD_BUF_MASK)  \
	  << ICP_QAT_FW_COMP_DISABLE_SECURE_RAM_AS_INTMD_BUF_BITPOS))

union icp_qat_fw_comp_req_hdr_cd_pars {
	/**< LWs 2-5 */
	struct {
		uint64_t content_desc_addr;
		/**< Address of the content descriptor */

		uint16_t content_desc_resrvd1;
		/**< Content descriptor reserved field */

		uint8_t content_desc_params_sz;
		/**< Size of the content descriptor parameters in quad words.
		 * These parameters describe the session setup configuration
		 * info for the slices that this request relies upon i.e.
		 * the configuration word and cipher key needed by the cipher
		 * slice if there is a request for cipher processing.
		 */

		uint8_t content_desc_hdr_resrvd2;
		/**< Content descriptor reserved field */

		uint32_t content_desc_resrvd3;
		/**< Content descriptor reserved field */
	} s;

	struct {
		uint32_t comp_slice_cfg_word[ICP_QAT_FW_NUM_LONGWORDS_2];
		/* Compression Slice Config Word */

		uint32_t content_desc_resrvd4;
		/**< Content descriptor reserved field */

	} sl;

};

struct icp_qat_fw_comp_req_params {
	/**< LW 14 */
	uint32_t comp_len;
	/**< Size of input to process in bytes Note:  Only EOP requests can be
	 * odd for decompression. IA must set LSB to zero for odd sized
	 * intermediate inputs
	 */

	/**< LW 15 */
	uint32_t out_buffer_sz;
	/**< Size of output buffer in bytes */

	/**< LW 16 */
	uint32_t initial_crc32;
	/**< CRC of previously processed bytes */

	/**< LW 17 */
	uint32_t initial_adler;
	/**< Adler of previously processed bytes */

	/**< LW 18 */
	uint32_t req_par_flags;

	/**< LW 19 */
	uint32_t rsrvd;
};

#define ICP_QAT_FW_COMP_REQ_PARAM_FLAGS_BUILD(sop, eop, bfinal, cnv, cnvnr)    \
	((((sop)&ICP_QAT_FW_COMP_SOP_MASK) << ICP_QAT_FW_COMP_SOP_BITPOS) |    \
	 (((eop)&ICP_QAT_FW_COMP_EOP_MASK) << ICP_QAT_FW_COMP_EOP_BITPOS) |    \
	 (((bfinal)&ICP_QAT_FW_COMP_BFINAL_MASK)                               \
	  << ICP_QAT_FW_COMP_BFINAL_BITPOS) |                                  \
	 ((cnv & ICP_QAT_FW_COMP_CNV_MASK) << ICP_QAT_FW_COMP_CNV_BITPOS) |    \
	 ((cnvnr & ICP_QAT_FW_COMP_CNV_RECOVERY_MASK)                          \
	  << ICP_QAT_FW_COMP_CNV_RECOVERY_BITPOS))

#define ICP_QAT_FW_COMP_NOT_SOP 0
/**< @ingroup icp_qat_fw_comp
 * Flag representing that a request is NOT Start of Packet
 */

#define ICP_QAT_FW_COMP_SOP 1
/**< @ingroup icp_qat_fw_comp
 * Flag representing that a request IS Start of Packet
 */

#define ICP_QAT_FW_COMP_NOT_EOP 0
/**< @ingroup icp_qat_fw_comp
 * Flag representing that a request is NOT Start of Packet
 */

#define ICP_QAT_FW_COMP_EOP 1
/**< @ingroup icp_qat_fw_comp
 * Flag representing that a request IS End of Packet
 */

#define ICP_QAT_FW_COMP_NOT_BFINAL 0
/**< @ingroup icp_qat_fw_comp
 * Flag representing to indicate firmware this is not the last block
 */

#define ICP_QAT_FW_COMP_BFINAL 1
/**< @ingroup icp_qat_fw_comp
 * Flag representing to indicate firmware this is the last block
 */

#define ICP_QAT_FW_COMP_NO_CNV 0
/**< @ingroup icp_qat_fw_comp
 * Flag indicating that NO cnv check is to be performed on the request
 */

#define ICP_QAT_FW_COMP_CNV 1
/**< @ingroup icp_qat_fw_comp
 * Flag indicating that a cnv check IS to be performed on the request
 */

#define ICP_QAT_FW_COMP_NO_CNV_RECOVERY 0
/**< @ingroup icp_qat_fw_comp
 * Flag indicating that NO cnv recovery is to be performed on the request
 */

#define ICP_QAT_FW_COMP_CNV_RECOVERY 1
/**< @ingroup icp_qat_fw_comp
 * Flag indicating that a cnv recovery is to be performed on the request
 */

#define ICP_QAT_FW_COMP_SOP_BITPOS 0
/**< @ingroup icp_qat_fw_comp
 * Starting bit position for SOP
 */

#define ICP_QAT_FW_COMP_SOP_MASK 0x1
/**< @ingroup icp_qat_fw_comp
 *  One bit mask used to determine SOP
 */

#define ICP_QAT_FW_COMP_EOP_BITPOS 1
/**< @ingroup icp_qat_fw_comp
 *  Starting bit position for EOP
 */

#define ICP_QAT_FW_COMP_EOP_MASK 0x1
/**< @ingroup icp_qat_fw_comp
 *  One bit mask used to determine EOP
 */

#define ICP_QAT_FW_COMP_BFINAL_MASK 0x1
/**< @ingroup icp_qat_fw_comp
 *  One bit mask for the bfinal bit
 */

#define ICP_QAT_FW_COMP_BFINAL_BITPOS 6
/**< @ingroup icp_qat_fw_comp
 *  Starting bit position for the bfinal bit
 */

#define ICP_QAT_FW_COMP_CNV_MASK 0x1
/**< @ingroup icp_qat_fw_comp
 * One bit mask for the CNV bit
 */

#define ICP_QAT_FW_COMP_CNV_BITPOS 16
/**< @ingroup icp_qat_fw_comp
 * Starting bit position for the CNV bit
 */

#define ICP_QAT_FW_COMP_CNV_RECOVERY_MASK 0x1
/**< @ingroup icp_qat_fw_comp
 * One bit mask for the CNV Recovery bit
 */

#define ICP_QAT_FW_COMP_CNV_RECOVERY_BITPOS 17
/**< @ingroup icp_qat_fw_comp
 * Starting bit position for the CNV Recovery bit
 */

struct icp_qat_fw_xlt_req_params {
	/**< LWs 20-21 */
	uint64_t inter_buff_ptr;
	/**< This field specifies the physical address of an intermediate
	 *  buffer SGL array. The array contains a pair of 64-bit
	 *  intermediate buffer pointers to SGL buffer descriptors, one pair
	 *  per CPM. Please refer to the CPM1.6 Firmware Interface HLD
	 *  specification for more details.
	 */
};


struct icp_qat_fw_comp_cd_hdr {
	/**< LW 24 */
	uint16_t ram_bank_flags;
	/**< Flags to show which ram banks to access */

	uint8_t comp_cfg_offset;
	/**< Quad word offset from the content descriptor parameters address
	 * to the parameters for the compression processing
	 */

	uint8_t next_curr_id;
	/**< This field combines the next and current id (each four bits) -
	 * the next id is the most significant nibble.
	 * Next Id:  Set to the next slice to pass the compressed data through.
	 * Set to ICP_QAT_FW_SLICE_DRAM_WR if the data is not to go through
	 * anymore slices after compression
	 * Current Id: Initialised with the compression slice type
	 */

	/**< LW 25 */
	uint32_t resrvd;
	/**< LWs 26-27 */

	uint64_t comp_state_addr;
	/**< Pointer to compression state */

	/**< LWs 28-29 */
	uint64_t ram_banks_addr;
	/**< Pointer to banks */

};


struct icp_qat_fw_xlt_cd_hdr {
	/**< LW 30 */
	uint16_t resrvd1;
	/**< Reserved field and assumed set to 0 */

	uint8_t resrvd2;
	/**< Reserved field and assumed set to 0 */

	uint8_t next_curr_id;
	/**< This field combines the next and current id (each four bits) -
	 * the next id is the most significant nibble.
	 * Next Id:  Set to the next slice to pass the translated data through.
	 * Set to ICP_QAT_FW_SLICE_DRAM_WR if the data is not to go through
	 * any more slices after compression
	 * Current Id: Initialised with the translation slice type
	 */

	/**< LW 31 */
	uint32_t resrvd3;
	/**< Reserved and should be set to zero, needed for quadword
	 * alignment
	 */
};

struct icp_qat_fw_comp_req {
	/**< LWs 0-1 */
	struct icp_qat_fw_comn_req_hdr comn_hdr;
	/**< Common request header - for Service Command Id,
	 * use service-specific Compression Command Id.
	 * Service Specific Flags - use Compression Command Flags
	 */

	/**< LWs 2-5 */
	union icp_qat_fw_comp_req_hdr_cd_pars cd_pars;
	/**< Compression service-specific content descriptor field which points
	 * either to a content descriptor parameter block or contains the
	 * compression slice config word.
	 */

	/**< LWs 6-13 */
	struct icp_qat_fw_comn_req_mid comn_mid;
	/**< Common request middle section */

	/**< LWs 14-19 */
	struct icp_qat_fw_comp_req_params comp_pars;
	/**< Compression request Parameters block */

	/**< LWs 20-21 */
	union {
		struct icp_qat_fw_xlt_req_params xlt_pars;
		/**< Translation request Parameters block */
		uint32_t resrvd1[ICP_QAT_FW_NUM_LONGWORDS_2];
		/**< Reserved if not used for translation */

	} u1;

	/**< LWs 22-23 */
	union {
		uint32_t resrvd2[ICP_QAT_FW_NUM_LONGWORDS_2];
		/**< Reserved - not used if Batch and Pack is disabled.*/

		uint64_t bnp_res_table_addr;
		/**< A generic pointer to the unbounded list of
		 * icp_qat_fw_resp_comp_pars members. This pointer is only
		 * used when the Batch and Pack is enabled.
		 */
	} u3;

	/**< LWs 24-29 */
	struct icp_qat_fw_comp_cd_hdr comp_cd_ctrl;
	/**< Compression request content descriptor control block header */

	/**< LWs 30-31 */
	union {
		struct icp_qat_fw_xlt_cd_hdr xlt_cd_ctrl;
		/**< Translation request content descriptor
		 * control block header
		 */

		uint32_t resrvd3[ICP_QAT_FW_NUM_LONGWORDS_2];
		/**< Reserved if not used for translation */
	} u2;
};

struct icp_qat_fw_resp_comp_pars {
	/**< LW 4 */
	uint32_t input_byte_counter;
	/**< Input byte counter */

	/**< LW 5 */
	uint32_t output_byte_counter;
	/**< Output byte counter */

	/**< LW 6 & 7*/
	union {
		uint64_t curr_chksum;
		struct {
			/**< LW 6 */
			uint32_t curr_crc32;
			/**< LW 7 */
			uint32_t curr_adler_32;
		};
	};
};

struct icp_qat_fw_comp_resp {
	/**< LWs 0-1 */
	struct icp_qat_fw_comn_resp_hdr comn_resp;
	/**< Common interface response format see icp_qat_fw.h */

	/**< LWs 2-3 */
	uint64_t opaque_data;
	/**< Opaque data passed from the request to the response message */

	/**< LWs 4-7 */
	struct icp_qat_fw_resp_comp_pars comp_resp_pars;
	/**< Common response params (checksums and byte counts) */
};

#endif
