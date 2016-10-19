/*
 * Copyright (c) 2016 QLogic Corporation.
 * All rights reserved.
 * www.qlogic.com
 *
 * See LICENSE.qede_pmd for copyright and licensing details.
 */

#ifndef __ECORE_MCP_H__
#define __ECORE_MCP_H__

#include "bcm_osal.h"
#include "mcp_public.h"
#include "ecore_mcp_api.h"

/* Using hwfn number (and not pf_num) is required since in CMT mode,
 * same pf_num may be used by two different hwfn
 * TODO - this shouldn't really be in .h file, but until all fields
 * required during hw-init will be placed in their correct place in shmem
 * we need it in ecore_dev.c [for readin the nvram reflection in shmem].
 */
#define MCP_PF_ID_BY_REL(p_hwfn, rel_pfid) (ECORE_IS_BB((p_hwfn)->p_dev) ? \
					    ((rel_pfid) | \
					     ((p_hwfn)->abs_pf_id & 1) << 3) : \
					     rel_pfid)
#define MCP_PF_ID(p_hwfn) MCP_PF_ID_BY_REL(p_hwfn, (p_hwfn)->rel_pf_id)

#define MFW_PORT(_p_hwfn)	((_p_hwfn)->abs_pf_id % \
				 ((_p_hwfn)->p_dev->num_ports_in_engines * \
				  ecore_device_num_engines((_p_hwfn)->p_dev)))

struct ecore_mcp_info {
	/* Spinlock used for protecting the access to the MFW mailbox */
	osal_spinlock_t lock;
	/* Flag to indicate whether sending a MFW mailbox is forbidden */
	bool block_mb_sending;

	/* Address of the MCP public area */
	u32 public_base;
	/* Address of the driver mailbox */
	u32 drv_mb_addr;
	/* Address of the MFW mailbox */
	u32 mfw_mb_addr;
	/* Address of the port configuration (link) */
	u32 port_addr;

	/* Current driver mailbox sequence */
	u16 drv_mb_seq;
	/* Current driver pulse sequence */
	u16 drv_pulse_seq;

	struct ecore_mcp_link_params       link_input;
	struct ecore_mcp_link_state	   link_output;
	struct ecore_mcp_link_capabilities link_capabilities;

	struct ecore_mcp_function_info	   func_info;

	u8 *mfw_mb_cur;
	u8 *mfw_mb_shadow;
	u16 mfw_mb_length;
	u16 mcp_hist;
};

struct ecore_mcp_mb_params {
	u32 cmd;
	u32 param;
	union drv_union_data *p_data_src;
	union drv_union_data *p_data_dst;
	u32 mcp_resp;
	u32 mcp_param;
};

/**
 * @brief Initialize the interface with the MCP
 *
 * @param p_hwfn - HW func
 * @param p_ptt - PTT required for register access
 *
 * @return enum _ecore_status_t
 */
enum _ecore_status_t ecore_mcp_cmd_init(struct ecore_hwfn *p_hwfn,
					struct ecore_ptt *p_ptt);

/**
 * @brief Initialize the port interface with the MCP
 *
 * @param p_hwfn
 * @param p_ptt
 * Can only be called after `num_ports_in_engines' is set
 */
void ecore_mcp_cmd_port_init(struct ecore_hwfn *p_hwfn,
			     struct ecore_ptt *p_ptt);
/**
 * @brief Releases resources allocated during the init process.
 *
 * @param p_hwfn - HW func
 * @param p_ptt - PTT required for register access
 *
 * @return enum _ecore_status_t
 */

enum _ecore_status_t ecore_mcp_free(struct ecore_hwfn *p_hwfn);

/**
 * @brief This function is called from the DPC context. After
 * pointing PTT to the mfw mb, check for events sent by the MCP
 * to the driver and ack them. In case a critical event
 * detected, it will be handled here, otherwise the work will be
 * queued to a sleepable work-queue.
 *
 * @param p_hwfn - HW function
 * @param p_ptt - PTT required for register access
 * @return enum _ecore_status_t - ECORE_SUCCESS - operation
 * was successul.
 */
enum _ecore_status_t ecore_mcp_handle_events(struct ecore_hwfn *p_hwfn,
					     struct ecore_ptt *p_ptt);

/**
 * @brief When MFW doesn't get driver pulse for couple of seconds, at some
 * threshold before timeout expires, it will generate interrupt
 * through a dedicated status block (DPSB - Driver Pulse Status
 * Block), which the driver should respond immediately, by
 * providing keepalive indication after setting the PTT to the
 * driver-MFW mailbox. This function is called directly from the
 * DPC upon receiving the DPSB attention.
 *
 * @param p_hwfn - hw function
 * @param p_ptt - PTT required for register access
 * @return enum _ecore_status_t - ECORE_SUCCESS - operation
 * was successul.
 */
enum _ecore_status_t ecore_issue_pulse(struct ecore_hwfn *p_hwfn,
				       struct ecore_ptt *p_ptt);

/**
 * @brief Sends a LOAD_REQ to the MFW, and in case operation
 *        succeed, returns whether this PF is the first on the
 *        chip/engine/port or function. This function should be
 *        called when driver is ready to accept MFW events after
 *        Storms initializations are done.
 *
 * @param p_hwfn       - hw function
 * @param p_ptt        - PTT required for register access
 * @param p_load_code  - The MCP response param containing one
 *      of the following:
 *      FW_MSG_CODE_DRV_LOAD_ENGINE
 *      FW_MSG_CODE_DRV_LOAD_PORT
 *      FW_MSG_CODE_DRV_LOAD_FUNCTION
 * @return enum _ecore_status_t -
 *      ECORE_SUCCESS - Operation was successul.
 *      ECORE_BUSY - Operation failed
 */
enum _ecore_status_t ecore_mcp_load_req(struct ecore_hwfn *p_hwfn,
					struct ecore_ptt *p_ptt,
					u32 *p_load_code);

/**
 * @brief Read the MFW mailbox into Current buffer.
 *
 * @param p_hwfn
 * @param p_ptt
 */
void ecore_mcp_read_mb(struct ecore_hwfn *p_hwfn,
		       struct ecore_ptt *p_ptt);

/**
 * @brief Ack to mfw that driver finished FLR process for VFs
 *
 * @param p_hwfn
 * @param p_ptt
 * @param vfs_to_ack - bit mask of all engine VFs for which the PF acks.
 *
 * @param return enum _ecore_status_t - ECORE_SUCCESS upon success.
 */
enum _ecore_status_t ecore_mcp_ack_vf_flr(struct ecore_hwfn *p_hwfn,
					  struct ecore_ptt *p_ptt,
					  u32 *vfs_to_ack);

/**
 * @brief - calls during init to read shmem of all function-related info.
 *
 * @param p_hwfn
 *
 * @param return ECORE_SUCCESS upon success.
 */
enum _ecore_status_t ecore_mcp_fill_shmem_func_info(struct ecore_hwfn *p_hwfn,
						    struct ecore_ptt *p_ptt);

/**
 * @brief - Reset the MCP using mailbox command.
 *
 * @param p_hwfn
 * @param p_ptt
 *
 * @param return ECORE_SUCCESS upon success.
 */
enum _ecore_status_t ecore_mcp_reset(struct ecore_hwfn *p_hwfn,
				     struct ecore_ptt *p_ptt);

/**
 * @brief - Sends an NVM write command request to the MFW with
 *          payload.
 *
 * @param p_hwfn
 * @param p_ptt
 * @param cmd - Command: Either DRV_MSG_CODE_NVM_WRITE_NVRAM or
 *            DRV_MSG_CODE_NVM_PUT_FILE_DATA
 * @param param - [0:23] - Offset [24:31] - Size
 * @param o_mcp_resp - MCP response
 * @param o_mcp_param - MCP response param
 * @param i_txn_size -  Buffer size
 * @param i_buf - Pointer to the buffer
 *
 * @param return ECORE_SUCCESS upon success.
 */
enum _ecore_status_t ecore_mcp_nvm_wr_cmd(struct ecore_hwfn *p_hwfn,
					  struct ecore_ptt *p_ptt,
					  u32 cmd,
					  u32 param,
					  u32 *o_mcp_resp,
					  u32 *o_mcp_param,
					  u32 i_txn_size,
					  u32 *i_buf);

/**
 * @brief - Sends an NVM read command request to the MFW to get
 *        a buffer.
 *
 * @param p_hwfn
 * @param p_ptt
 * @param cmd - Command: DRV_MSG_CODE_NVM_GET_FILE_DATA or
 *            DRV_MSG_CODE_NVM_READ_NVRAM commands
 * @param param - [0:23] - Offset [24:31] - Size
 * @param o_mcp_resp - MCP response
 * @param o_mcp_param - MCP response param
 * @param o_txn_size -  Buffer size output
 * @param o_buf - Pointer to the buffer returned by the MFW.
 *
 * @param return ECORE_SUCCESS upon success.
 */
enum _ecore_status_t ecore_mcp_nvm_rd_cmd(struct ecore_hwfn *p_hwfn,
					  struct ecore_ptt *p_ptt,
					  u32 cmd,
					  u32 param,
					  u32 *o_mcp_resp,
					  u32 *o_mcp_param,
					  u32 *o_txn_size,
					  u32 *o_buf);

/**
 * @brief indicates whether the MFW objects [under mcp_info] are accessible
 *
 * @param p_hwfn
 *
 * @return true iff MFW is running and mcp_info is initialized
 */
bool ecore_mcp_is_init(struct ecore_hwfn *p_hwfn);

/**
 * @brief request MFW to configure MSI-X for a VF
 *
 * @param p_hwfn
 * @param p_ptt
 * @param vf_id - absolute inside engine
 * @param num_sbs - number of entries to request
 *
 * @return enum _ecore_status_t
 */
enum _ecore_status_t ecore_mcp_config_vf_msix(struct ecore_hwfn *p_hwfn,
					      struct ecore_ptt *p_ptt,
					      u8 vf_id, u8 num);

/**
 * @brief - Halt the MCP.
 *
 * @param p_hwfn
 * @param p_ptt
 *
 * @param return ECORE_SUCCESS upon success.
 */
enum _ecore_status_t ecore_mcp_halt(struct ecore_hwfn *p_hwfn,
				    struct ecore_ptt *p_ptt);

/**
 * @brief - Wake up the MCP.
 *
 * @param p_hwfn
 * @param p_ptt
 *
 * @param return ECORE_SUCCESS upon success.
 */
enum _ecore_status_t ecore_mcp_resume(struct ecore_hwfn *p_hwfn,
				      struct ecore_ptt *p_ptt);
int __ecore_configure_pf_max_bandwidth(struct ecore_hwfn *p_hwfn,
				       struct ecore_ptt *p_ptt,
				       struct ecore_mcp_link_state *p_link,
				       u8 max_bw);
int __ecore_configure_pf_min_bandwidth(struct ecore_hwfn *p_hwfn,
				       struct ecore_ptt *p_ptt,
				       struct ecore_mcp_link_state *p_link,
				       u8 min_bw);
enum _ecore_status_t ecore_mcp_mask_parities(struct ecore_hwfn *p_hwfn,
					     struct ecore_ptt *p_ptt,
					     u32 mask_parities);
/**
 * @brief - Sends crash mdump related info to the MFW.
 *
 * @param p_hwfn
 * @param p_ptt
 *
 * @param return ECORE_SUCCESS upon success.
 */
enum _ecore_status_t ecore_mcp_mdump_set_values(struct ecore_hwfn *p_hwfn,
						struct ecore_ptt *p_ptt,
						u32 epoch);

/**
 * @brief - Triggers a MFW crash dump procedure.
 *
 * @param p_hwfn
 * @param p_ptt
 *
 * @param return ECORE_SUCCESS upon success.
 */
enum _ecore_status_t ecore_mcp_mdump_trigger(struct ecore_hwfn *p_hwfn,
					     struct ecore_ptt *p_ptt);

/**
 * @brief - Clears the MFW crash dump logs.
 *
 * @param p_hwfn
 * @param p_ptt
 *
 * @param return ECORE_SUCCESS upon success.
 */
enum _ecore_status_t ecore_mcp_mdump_clear_logs(struct ecore_hwfn *p_hwfn,
						struct ecore_ptt *p_ptt);

/**
 * @brief - Gets the MFW crash dump configuration and logs info.
 *
 * @param p_hwfn
 * @param p_ptt
 *
 * @param return ECORE_SUCCESS upon success.
 */
enum _ecore_status_t ecore_mcp_mdump_get_info(struct ecore_hwfn *p_hwfn,
					      struct ecore_ptt *p_ptt);

enum _ecore_status_t ecore_mcp_get_resc_info(struct ecore_hwfn *p_hwfn,
					     struct ecore_ptt *p_ptt,
					     struct resource_info *p_resc_info,
					     u32 *p_mcp_resp, u32 *p_mcp_param);

/**
 * @brief - Initiates PF FLR
 *
 * @param p_hwfn
 * @param p_ptt
 *
 * @param return ECORE_SUCCESS upon success.
 */
enum _ecore_status_t ecore_mcp_initiate_pf_flr(struct ecore_hwfn *p_hwfn,
					       struct ecore_ptt *p_ptt);

#endif /* __ECORE_MCP_H__ */
