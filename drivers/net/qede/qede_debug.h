/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2020 Marvell Semiconductor Inc.
 * All rights reserved.
 * www.marvell.com
 */

#ifndef _QED_DEBUG_H
#define _QED_DEBUG_H

/* Forward Declaration */
struct ecore_dev;
enum ecore_dbg_features;

int qed_dbg_grc(struct ecore_dev *edev, void *buffer, u32 *num_dumped_bytes);
int qed_dbg_grc_size(struct ecore_dev *edev);
int qed_dbg_idle_chk(struct ecore_dev *edev, void *buffer,
		     u32 *num_dumped_bytes);
int qed_dbg_idle_chk_size(struct ecore_dev *edev);
int qed_dbg_reg_fifo(struct ecore_dev *edev, void *buffer,
		     u32 *num_dumped_bytes);
int qed_dbg_reg_fifo_size(struct ecore_dev *edev);
int qed_dbg_igu_fifo(struct ecore_dev *edev, void *buffer,
		     u32 *num_dumped_bytes);
int qed_dbg_igu_fifo_size(struct ecore_dev *edev);
int qed_dbg_protection_override(struct ecore_dev *edev, void *buffer,
				u32 *num_dumped_bytes);
int qed_dbg_protection_override_size(struct ecore_dev *edev);
int qed_dbg_fw_asserts(struct ecore_dev *edev, void *buffer,
		       u32 *num_dumped_bytes);
int qed_dbg_fw_asserts_size(struct ecore_dev *edev);
int qed_dbg_ilt(struct ecore_dev *edev, void *buffer, u32 *num_dumped_bytes);
int qed_dbg_ilt_size(struct ecore_dev *edev);
int qed_dbg_mcp_trace(struct ecore_dev *edev, void *buffer,
		      u32 *num_dumped_bytes);
int qed_dbg_mcp_trace_size(struct ecore_dev *edev);
int qed_dbg_all_data(struct ecore_dev *edev, void *buffer);
int qed_dbg_all_data_size(struct ecore_dev *edev);
u8 qed_get_debug_engine(struct ecore_dev *edev);
void qed_set_debug_engine(struct ecore_dev *edev, int engine_number);
int qed_dbg_feature(struct ecore_dev *edev, void *buffer,
		    enum ecore_dbg_features feature, u32 *num_dumped_bytes);
int
qed_dbg_feature_size(struct ecore_dev *edev, enum ecore_dbg_features feature);

void qed_dbg_pf_init(struct ecore_dev *edev);
void qed_dbg_pf_exit(struct ecore_dev *edev);

/***************************** Public Functions *******************************/

/**
 * @brief qed_dbg_set_bin_ptr - Sets a pointer to the binary data with debug
 *	arrays.
 *
 * @param p_hwfn -	    HW device data
 * @param bin_ptr - a pointer to the binary data with debug arrays.
 */
enum dbg_status qed_dbg_set_bin_ptr(struct ecore_hwfn *p_hwfn,
				    const u8 * const bin_ptr);

/**
 * @brief qed_dbg_set_app_ver - Sets the version of the calling app.
 *
 * The application should call this function with the TOOLS_VERSION
 * it compiles with. Must be called before all other debug functions.
 *
 * @return error if one of the following holds:
 *      - the specified app version is not supported
 * Otherwise, returns ok.
 */
enum dbg_status qed_dbg_set_app_ver(u32 ver);

/**
 * @brief qed_read_regs - Reads registers into a buffer (using GRC).
 *
 * @param p_hwfn - HW device data
 * @param p_ptt - Ptt window used for writing the registers.
 * @param buf - Destination buffer.
 * @param addr - Source GRC address in dwords.
 * @param len - Number of registers to read.
 */
void qed_read_regs(struct ecore_hwfn *p_hwfn,
		   struct ecore_ptt *p_ptt, u32 *buf, u32 addr, u32 len);

/**
 * @brief qed_read_fw_info - Reads FW info from the chip.
 *
 * The FW info contains FW-related information, such as the FW version,
 * FW image (main/L2B/kuku), FW timestamp, etc.
 * The FW info is read from the internal RAM of the first Storm that is not in
 * reset.
 *
 * @param p_hwfn -	    HW device data
 * @param p_ptt -	    Ptt window used for writing the registers.
 * @param fw_info -	Out: a pointer to write the FW info into.
 *
 * @return true if the FW info was read successfully from one of the Storms,
 * or false if all Storms are in reset.
 */
bool qed_read_fw_info(struct ecore_hwfn *p_hwfn,
		      struct ecore_ptt *p_ptt, struct fw_info *fw_info);
/**
 * @brief qed_dbg_grc_config - Sets the value of a GRC parameter.
 *
 * @param p_hwfn -	HW device data
 * @param grc_param -	GRC parameter
 * @param val -		Value to set.
 *
 * @return error if one of the following holds:
 *	- the version wasn't set
 *	- grc_param is invalid
 *	- val is outside the allowed boundaries
 */
enum dbg_status qed_dbg_grc_config(struct ecore_hwfn *p_hwfn,
				   enum dbg_grc_params grc_param, u32 val);

/**
 * @brief qed_dbg_grc_set_params_default - Reverts all GRC parameters to their
 *	default value.
 *
 * @param p_hwfn		- HW device data
 */
void qed_dbg_grc_set_params_default(struct ecore_hwfn *p_hwfn);
/**
 * @brief qed_dbg_grc_get_dump_buf_size - Returns the required buffer size for
 *	GRC Dump.
 *
 * @param p_hwfn - HW device data
 * @param p_ptt - Ptt window used for writing the registers.
 * @param buf_size - OUT: required buffer size (in dwords) for the GRC Dump
 *	data.
 *
 * @return error if one of the following holds:
 *	- the version wasn't set
 * Otherwise, returns ok.
 */
enum dbg_status qed_dbg_grc_get_dump_buf_size(struct ecore_hwfn *p_hwfn,
					      struct ecore_ptt *p_ptt,
					      u32 *buf_size);

/**
 * @brief qed_dbg_grc_dump - Dumps GRC data into the specified buffer.
 *
 * @param p_hwfn - HW device data
 * @param p_ptt - Ptt window used for writing the registers.
 * @param dump_buf - Pointer to write the collected GRC data into.
 * @param buf_size_in_dwords - Size of the specified buffer in dwords.
 * @param num_dumped_dwords - OUT: number of dumped dwords.
 *
 * @return error if one of the following holds:
 *	- the version wasn't set
 *	- the specified dump buffer is too small
 * Otherwise, returns ok.
 */
enum dbg_status qed_dbg_grc_dump(struct ecore_hwfn *p_hwfn,
				 struct ecore_ptt *p_ptt,
				 u32 *dump_buf,
				 u32 buf_size_in_dwords,
				 u32 *num_dumped_dwords);
/**
 * @brief qed_dbg_idle_chk_get_dump_buf_size - Returns the required buffer size
 *	for idle check results.
 *
 * @param p_hwfn - HW device data
 * @param p_ptt - Ptt window used for writing the registers.
 * @param buf_size - OUT: required buffer size (in dwords) for the idle check
 *	data.
 *
 * @return error if one of the following holds:
 *	- the version wasn't set
 * Otherwise, returns ok.
 */
enum dbg_status qed_dbg_idle_chk_get_dump_buf_size(struct ecore_hwfn *p_hwfn,
						   struct ecore_ptt *p_ptt,
						   u32 *buf_size);

/**
 * @brief qed_dbg_idle_chk_dump - Performs idle check and writes the results
 *	into the specified buffer.
 *
 * @param p_hwfn - HW device data
 * @param p_ptt - Ptt window used for writing the registers.
 * @param dump_buf - Pointer to write the idle check data into.
 * @param buf_size_in_dwords - Size of the specified buffer in dwords.
 * @param num_dumped_dwords - OUT: number of dumped dwords.
 *
 * @return error if one of the following holds:
 *	- the version wasn't set
 *	- the specified buffer is too small
 * Otherwise, returns ok.
 */
enum dbg_status qed_dbg_idle_chk_dump(struct ecore_hwfn *p_hwfn,
				      struct ecore_ptt *p_ptt,
				      u32 *dump_buf,
				      u32 buf_size_in_dwords,
				      u32 *num_dumped_dwords);

/**
 * @brief qed_dbg_mcp_trace_get_dump_buf_size - Returns the required buffer size
 *	for mcp trace results.
 *
 * @param p_hwfn - HW device data
 * @param p_ptt - Ptt window used for writing the registers.
 * @param buf_size - OUT: required buffer size (in dwords) for mcp trace data.
 *
 * @return error if one of the following holds:
 *	- the version wasn't set
 *	- the trace data in MCP scratchpad contain an invalid signature
 *	- the bundle ID in NVRAM is invalid
 *	- the trace meta data cannot be found (in NVRAM or image file)
 * Otherwise, returns ok.
 */
enum dbg_status qed_dbg_mcp_trace_get_dump_buf_size(struct ecore_hwfn *p_hwfn,
						    struct ecore_ptt *p_ptt,
						    u32 *buf_size);

/**
 * @brief qed_dbg_mcp_trace_dump - Performs mcp trace and writes the results
 *	into the specified buffer.
 *
 * @param p_hwfn - HW device data
 * @param p_ptt - Ptt window used for writing the registers.
 * @param dump_buf - Pointer to write the mcp trace data into.
 * @param buf_size_in_dwords - Size of the specified buffer in dwords.
 * @param num_dumped_dwords - OUT: number of dumped dwords.
 *
 * @return error if one of the following holds:
 *	- the version wasn't set
 *	- the specified buffer is too small
 *	- the trace data in MCP scratchpad contain an invalid signature
 *	- the bundle ID in NVRAM is invalid
 *	- the trace meta data cannot be found (in NVRAM or image file)
 *	- the trace meta data cannot be read (from NVRAM or image file)
 * Otherwise, returns ok.
 */
enum dbg_status qed_dbg_mcp_trace_dump(struct ecore_hwfn *p_hwfn,
				       struct ecore_ptt *p_ptt,
				       u32 *dump_buf,
				       u32 buf_size_in_dwords,
				       u32 *num_dumped_dwords);

/**
 * @brief qed_dbg_reg_fifo_get_dump_buf_size - Returns the required buffer size
 *	for grc trace fifo results.
 *
 * @param p_hwfn - HW device data
 * @param p_ptt - Ptt window used for writing the registers.
 * @param buf_size - OUT: required buffer size (in dwords) for reg fifo data.
 *
 * @return error if one of the following holds:
 *	- the version wasn't set
 * Otherwise, returns ok.
 */
enum dbg_status qed_dbg_reg_fifo_get_dump_buf_size(struct ecore_hwfn *p_hwfn,
						   struct ecore_ptt *p_ptt,
						   u32 *buf_size);

/**
 * @brief qed_dbg_reg_fifo_dump - Reads the reg fifo and writes the results into
 *	the specified buffer.
 *
 * @param p_hwfn - HW device data
 * @param p_ptt - Ptt window used for writing the registers.
 * @param dump_buf - Pointer to write the reg fifo data into.
 * @param buf_size_in_dwords - Size of the specified buffer in dwords.
 * @param num_dumped_dwords - OUT: number of dumped dwords.
 *
 * @return error if one of the following holds:
 *	- the version wasn't set
 *	- the specified buffer is too small
 *	- DMAE transaction failed
 * Otherwise, returns ok.
 */
enum dbg_status qed_dbg_reg_fifo_dump(struct ecore_hwfn *p_hwfn,
				      struct ecore_ptt *p_ptt,
				      u32 *dump_buf,
				      u32 buf_size_in_dwords,
				      u32 *num_dumped_dwords);

/**
 * @brief qed_dbg_igu_fifo_get_dump_buf_size - Returns the required buffer size
 *	for the IGU fifo results.
 *
 * @param p_hwfn - HW device data
 * @param p_ptt - Ptt window used for writing the registers.
 * @param buf_size - OUT: required buffer size (in dwords) for the IGU fifo
 *	data.
 *
 * @return error if one of the following holds:
 *	- the version wasn't set
 * Otherwise, returns ok.
 */
enum dbg_status qed_dbg_igu_fifo_get_dump_buf_size(struct ecore_hwfn *p_hwfn,
						   struct ecore_ptt *p_ptt,
						   u32 *buf_size);

/**
 * @brief qed_dbg_igu_fifo_dump - Reads the IGU fifo and writes the results into
 *	the specified buffer.
 *
 * @param p_hwfn - HW device data
 * @param p_ptt - Ptt window used for writing the registers.
 * @param dump_buf - Pointer to write the IGU fifo data into.
 * @param buf_size_in_dwords - Size of the specified buffer in dwords.
 * @param num_dumped_dwords - OUT: number of dumped dwords.
 *
 * @return error if one of the following holds:
 *	- the version wasn't set
 *	- the specified buffer is too small
 *	- DMAE transaction failed
 * Otherwise, returns ok.
 */
enum dbg_status qed_dbg_igu_fifo_dump(struct ecore_hwfn *p_hwfn,
				      struct ecore_ptt *p_ptt,
				      u32 *dump_buf,
				      u32 buf_size_in_dwords,
				      u32 *num_dumped_dwords);

/**
 * @brief qed_dbg_protection_override_get_dump_buf_size - Returns the required
 *	buffer size for protection override window results.
 *
 * @param p_hwfn - HW device data
 * @param p_ptt - Ptt window used for writing the registers.
 * @param buf_size - OUT: required buffer size (in dwords) for protection
 *	override data.
 *
 * @return error if one of the following holds:
 *	- the version wasn't set
 * Otherwise, returns ok.
 */
enum dbg_status
qed_dbg_protection_override_get_dump_buf_size(struct ecore_hwfn *p_hwfn,
					      struct ecore_ptt *p_ptt,
					      u32 *buf_size);
/**
 * @brief qed_dbg_protection_override_dump - Reads protection override window
 *	entries and writes the results into the specified buffer.
 *
 * @param p_hwfn - HW device data
 * @param p_ptt - Ptt window used for writing the registers.
 * @param dump_buf - Pointer to write the protection override data into.
 * @param buf_size_in_dwords - Size of the specified buffer in dwords.
 * @param num_dumped_dwords - OUT: number of dumped dwords.
 *
 * @return error if one of the following holds:
 *	- the version wasn't set
 *	- the specified buffer is too small
 *	- DMAE transaction failed
 * Otherwise, returns ok.
 */
enum dbg_status qed_dbg_protection_override_dump(struct ecore_hwfn *p_hwfn,
						 struct ecore_ptt *p_ptt,
						 u32 *dump_buf,
						 u32 buf_size_in_dwords,
						 u32 *num_dumped_dwords);
/**
 * @brief qed_dbg_fw_asserts_get_dump_buf_size - Returns the required buffer
 *	size for FW Asserts results.
 *
 * @param p_hwfn - HW device data
 * @param p_ptt - Ptt window used for writing the registers.
 * @param buf_size - OUT: required buffer size (in dwords) for FW Asserts data.
 *
 * @return error if one of the following holds:
 *	- the version wasn't set
 * Otherwise, returns ok.
 */
enum dbg_status qed_dbg_fw_asserts_get_dump_buf_size(struct ecore_hwfn *p_hwfn,
						     struct ecore_ptt *p_ptt,
						     u32 *buf_size);
/**
 * @brief qed_dbg_fw_asserts_dump - Reads the FW Asserts and writes the results
 *	into the specified buffer.
 *
 * @param p_hwfn - HW device data
 * @param p_ptt - Ptt window used for writing the registers.
 * @param dump_buf - Pointer to write the FW Asserts data into.
 * @param buf_size_in_dwords - Size of the specified buffer in dwords.
 * @param num_dumped_dwords - OUT: number of dumped dwords.
 *
 * @return error if one of the following holds:
 *	- the version wasn't set
 *	- the specified buffer is too small
 * Otherwise, returns ok.
 */
enum dbg_status qed_dbg_fw_asserts_dump(struct ecore_hwfn *p_hwfn,
					struct ecore_ptt *p_ptt,
					u32 *dump_buf,
					u32 buf_size_in_dwords,
					u32 *num_dumped_dwords);

/**
 * @brief qed_dbg_read_attn - Reads the attention registers of the specified
 * block and type, and writes the results into the specified buffer.
 *
 * @param p_hwfn -	 HW device data
 * @param p_ptt -	 Ptt window used for writing the registers.
 * @param block -	 Block ID.
 * @param attn_type -	 Attention type.
 * @param clear_status - Indicates if the attention status should be cleared.
 * @param results -	 OUT: Pointer to write the read results into
 *
 * @return error if one of the following holds:
 *	- the version wasn't set
 * Otherwise, returns ok.
 */
enum dbg_status qed_dbg_read_attn(struct ecore_hwfn *p_hwfn,
				  struct ecore_ptt *p_ptt,
				  enum block_id block,
				  enum dbg_attn_type attn_type,
				  bool clear_status,
				  struct dbg_attn_block_result *results);

/**
 * @brief qed_dbg_print_attn - Prints attention registers values in the
 *	specified results struct.
 *
 * @param p_hwfn
 * @param results - Pointer to the attention read results
 *
 * @return error if one of the following holds:
 *	- the version wasn't set
 * Otherwise, returns ok.
 */
enum dbg_status qed_dbg_print_attn(struct ecore_hwfn *p_hwfn,
				   struct dbg_attn_block_result *results);

/******************************* Data Types **********************************/

struct mcp_trace_format {
	u32 data;
#define MCP_TRACE_FORMAT_MODULE_MASK	0x0000ffff
#define MCP_TRACE_FORMAT_MODULE_OFFSET	0
#define MCP_TRACE_FORMAT_LEVEL_MASK	0x00030000
#define MCP_TRACE_FORMAT_LEVEL_OFFSET	16
#define MCP_TRACE_FORMAT_P1_SIZE_MASK	0x000c0000
#define MCP_TRACE_FORMAT_P1_SIZE_OFFSET 18
#define MCP_TRACE_FORMAT_P2_SIZE_MASK	0x00300000
#define MCP_TRACE_FORMAT_P2_SIZE_OFFSET 20
#define MCP_TRACE_FORMAT_P3_SIZE_MASK	0x00c00000
#define MCP_TRACE_FORMAT_P3_SIZE_OFFSET 22
#define MCP_TRACE_FORMAT_LEN_MASK	0xff000000
#define MCP_TRACE_FORMAT_LEN_OFFSET	24

	char *format_str;
};

/* MCP Trace Meta data structure */
struct mcp_trace_meta {
	u32 modules_num;
	char **modules;
	u32 formats_num;
	struct mcp_trace_format *formats;
	bool is_allocated;
};

/* Debug Tools user data */
struct dbg_tools_user_data {
	struct mcp_trace_meta mcp_trace_meta;
	const u32 *mcp_trace_user_meta_buf;
};

/******************************** Constants **********************************/

#define MAX_NAME_LEN	16

/***************************** Public Functions *******************************/

/**
 * @brief qed_dbg_user_set_bin_ptr - Sets a pointer to the binary data with
 *	debug arrays.
 *
 * @param p_hwfn - HW device data
 * @param bin_ptr - a pointer to the binary data with debug arrays.
 */
enum dbg_status qed_dbg_user_set_bin_ptr(struct ecore_hwfn *p_hwfn,
					 const u8 * const bin_ptr);

/**
 * @brief qed_dbg_alloc_user_data - Allocates user debug data.
 *
 * @param p_hwfn -		 HW device data
 * @param user_data_ptr - OUT: a pointer to the allocated memory.
 */
enum dbg_status qed_dbg_alloc_user_data(struct ecore_hwfn *p_hwfn,
					void **user_data_ptr);

/**
 * @brief qed_dbg_get_status_str - Returns a string for the specified status.
 *
 * @param status - a debug status code.
 *
 * @return a string for the specified status
 */
const char *qed_dbg_get_status_str(enum dbg_status status);

/**
 * @brief qed_get_idle_chk_results_buf_size - Returns the required buffer size
 *	for idle check results (in bytes).
 *
 * @param p_hwfn - HW device data
 * @param dump_buf - idle check dump buffer.
 * @param num_dumped_dwords - number of dwords that were dumped.
 * @param results_buf_size - OUT: required buffer size (in bytes) for the parsed
 *	results.
 *
 * @return error if the parsing fails, ok otherwise.
 */
enum dbg_status qed_get_idle_chk_results_buf_size(struct ecore_hwfn *p_hwfn,
						  u32 *dump_buf,
						  u32  num_dumped_dwords,
						  u32 *results_buf_size);
/**
 * @brief qed_print_idle_chk_results - Prints idle check results
 *
 * @param p_hwfn - HW device data
 * @param dump_buf - idle check dump buffer.
 * @param num_dumped_dwords - number of dwords that were dumped.
 * @param results_buf - buffer for printing the idle check results.
 * @param num_errors - OUT: number of errors found in idle check.
 * @param num_warnings - OUT: number of warnings found in idle check.
 *
 * @return error if the parsing fails, ok otherwise.
 */
enum dbg_status qed_print_idle_chk_results(struct ecore_hwfn *p_hwfn,
					   u32 *dump_buf,
					   u32 num_dumped_dwords,
					   char *results_buf,
					   u32 *num_errors,
					   u32 *num_warnings);

/**
 * @brief qed_dbg_mcp_trace_set_meta_data - Sets the MCP Trace meta data.
 *
 * Needed in case the MCP Trace dump doesn't contain the meta data (e.g. due to
 * no NVRAM access).
 *
 * @param data - pointer to MCP Trace meta data
 * @param size - size of MCP Trace meta data in dwords
 */
void qed_dbg_mcp_trace_set_meta_data(struct ecore_hwfn *p_hwfn,
				     const u32 *meta_buf);

/**
 * @brief qed_get_mcp_trace_results_buf_size - Returns the required buffer size
 *	for MCP Trace results (in bytes).
 *
 * @param p_hwfn - HW device data
 * @param dump_buf - MCP Trace dump buffer.
 * @param num_dumped_dwords - number of dwords that were dumped.
 * @param results_buf_size - OUT: required buffer size (in bytes) for the parsed
 *	results.
 *
 * @return error if the parsing fails, ok otherwise.
 */
enum dbg_status qed_get_mcp_trace_results_buf_size(struct ecore_hwfn *p_hwfn,
						   u32 *dump_buf,
						   u32 num_dumped_dwords,
						   u32 *results_buf_size);

/**
 * @brief qed_print_mcp_trace_results - Prints MCP Trace results
 *
 * @param p_hwfn - HW device data
 * @param dump_buf - mcp trace dump buffer, starting from the header.
 * @param num_dumped_dwords - number of dwords that were dumped.
 * @param results_buf - buffer for printing the mcp trace results.
 *
 * @return error if the parsing fails, ok otherwise.
 */
enum dbg_status qed_print_mcp_trace_results(struct ecore_hwfn *p_hwfn,
					    u32 *dump_buf,
					    u32 num_dumped_dwords,
					    char *results_buf);

/**
 * @brief qed_print_mcp_trace_results_cont - Prints MCP Trace results, and
 * keeps the MCP trace meta data allocated, to support continuous MCP Trace
 * parsing. After the continuous parsing ends, mcp_trace_free_meta_data should
 * be called to free the meta data.
 *
 * @param p_hwfn -	      HW device data
 * @param dump_buf -	      mcp trace dump buffer, starting from the header.
 * @param results_buf -	      buffer for printing the mcp trace results.
 *
 * @return error if the parsing fails, ok otherwise.
 */
enum dbg_status qed_print_mcp_trace_results_cont(struct ecore_hwfn *p_hwfn,
						 u32 *dump_buf,
						 char *results_buf);

/**
 * @brief print_mcp_trace_line - Prints MCP Trace results for a single line
 *
 * @param p_hwfn -	      HW device data
 * @param dump_buf -	      mcp trace dump buffer, starting from the header.
 * @param num_dumped_bytes -  number of bytes that were dumped.
 * @param results_buf -	      buffer for printing the mcp trace results.
 *
 * @return error if the parsing fails, ok otherwise.
 */
enum dbg_status qed_print_mcp_trace_line(struct ecore_hwfn *p_hwfn,
					 u8 *dump_buf,
					 u32 num_dumped_bytes,
					 char *results_buf);

/**
 * @brief mcp_trace_free_meta_data - Frees the MCP Trace meta data.
 * Should be called after continuous MCP Trace parsing.
 *
 * @param p_hwfn - HW device data
 */
void qed_mcp_trace_free_meta_data(struct ecore_hwfn *p_hwfn);

/**
 * @brief qed_get_reg_fifo_results_buf_size - Returns the required buffer size
 *	for reg_fifo results (in bytes).
 *
 * @param p_hwfn - HW device data
 * @param dump_buf - reg fifo dump buffer.
 * @param num_dumped_dwords - number of dwords that were dumped.
 * @param results_buf_size - OUT: required buffer size (in bytes) for the parsed
 *	results.
 *
 * @return error if the parsing fails, ok otherwise.
 */
enum dbg_status qed_get_reg_fifo_results_buf_size(struct ecore_hwfn *p_hwfn,
						  u32 *dump_buf,
						  u32 num_dumped_dwords,
						  u32 *results_buf_size);

/**
 * @brief qed_print_reg_fifo_results - Prints reg fifo results
 *
 * @param p_hwfn - HW device data
 * @param dump_buf - reg fifo dump buffer, starting from the header.
 * @param num_dumped_dwords - number of dwords that were dumped.
 * @param results_buf - buffer for printing the reg fifo results.
 *
 * @return error if the parsing fails, ok otherwise.
 */
enum dbg_status qed_print_reg_fifo_results(struct ecore_hwfn *p_hwfn,
					   u32 *dump_buf,
					   u32 num_dumped_dwords,
					   char *results_buf);

/**
 * @brief qed_get_igu_fifo_results_buf_size - Returns the required buffer size
 *	for igu_fifo results (in bytes).
 *
 * @param p_hwfn - HW device data
 * @param dump_buf - IGU fifo dump buffer.
 * @param num_dumped_dwords - number of dwords that were dumped.
 * @param results_buf_size - OUT: required buffer size (in bytes) for the parsed
 *	results.
 *
 * @return error if the parsing fails, ok otherwise.
 */
enum dbg_status qed_get_igu_fifo_results_buf_size(struct ecore_hwfn *p_hwfn,
						  u32 *dump_buf,
						  u32 num_dumped_dwords,
						  u32 *results_buf_size);

/**
 * @brief qed_print_igu_fifo_results - Prints IGU fifo results
 *
 * @param p_hwfn - HW device data
 * @param dump_buf - IGU fifo dump buffer, starting from the header.
 * @param num_dumped_dwords - number of dwords that were dumped.
 * @param results_buf - buffer for printing the IGU fifo results.
 *
 * @return error if the parsing fails, ok otherwise.
 */
enum dbg_status qed_print_igu_fifo_results(struct ecore_hwfn *p_hwfn,
					   u32 *dump_buf,
					   u32 num_dumped_dwords,
					   char *results_buf);

/**
 * @brief qed_get_protection_override_results_buf_size - Returns the required
 *	buffer size for protection override results (in bytes).
 *
 * @param p_hwfn - HW device data
 * @param dump_buf - protection override dump buffer.
 * @param num_dumped_dwords - number of dwords that were dumped.
 * @param results_buf_size - OUT: required buffer size (in bytes) for the parsed
 *	results.
 *
 * @return error if the parsing fails, ok otherwise.
 */
enum dbg_status
qed_get_protection_override_results_buf_size(struct ecore_hwfn *p_hwfn,
					     u32 *dump_buf,
					     u32 num_dumped_dwords,
					     u32 *results_buf_size);

/**
 * @brief qed_print_protection_override_results - Prints protection override
 *	results.
 *
 * @param p_hwfn - HW device data
 * @param dump_buf - protection override dump buffer, starting from the header.
 * @param num_dumped_dwords - number of dwords that were dumped.
 * @param results_buf - buffer for printing the reg fifo results.
 *
 * @return error if the parsing fails, ok otherwise.
 */
enum dbg_status qed_print_protection_override_results(struct ecore_hwfn *p_hwfn,
						      u32 *dump_buf,
						      u32 num_dumped_dwords,
						      char *results_buf);

/**
 * @brief qed_get_fw_asserts_results_buf_size - Returns the required buffer size
 *	for FW Asserts results (in bytes).
 *
 * @param p_hwfn - HW device data
 * @param dump_buf - FW Asserts dump buffer.
 * @param num_dumped_dwords - number of dwords that were dumped.
 * @param results_buf_size - OUT: required buffer size (in bytes) for the parsed
 *	results.
 *
 * @return error if the parsing fails, ok otherwise.
 */
enum dbg_status qed_get_fw_asserts_results_buf_size(struct ecore_hwfn *p_hwfn,
						    u32 *dump_buf,
						    u32 num_dumped_dwords,
						    u32 *results_buf_size);

/**
 * @brief qed_print_fw_asserts_results - Prints FW Asserts results
 *
 * @param p_hwfn - HW device data
 * @param dump_buf - FW Asserts dump buffer, starting from the header.
 * @param num_dumped_dwords - number of dwords that were dumped.
 * @param results_buf - buffer for printing the FW Asserts results.
 *
 * @return error if the parsing fails, ok otherwise.
 */
enum dbg_status qed_print_fw_asserts_results(struct ecore_hwfn *p_hwfn,
					     u32 *dump_buf,
					     u32 num_dumped_dwords,
					     char *results_buf);

/**
 * @brief qed_dbg_parse_attn - Parses and prints attention registers values in
 * the specified results struct.
 *
 * @param p_hwfn -  HW device data
 * @param results - Pointer to the attention read results
 *
 * @return error if one of the following holds:
 *	- the version wasn't set
 * Otherwise, returns ok.
 */
enum dbg_status qed_dbg_parse_attn(struct ecore_hwfn *p_hwfn,
				   struct dbg_attn_block_result *results);

#endif
