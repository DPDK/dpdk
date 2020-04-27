/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2016 - 2018 Cavium Inc.
 * All rights reserved.
 * www.cavium.com
 */

#ifndef __ECORE_DEV_API_H__
#define __ECORE_DEV_API_H__

#include "ecore_status.h"
#include "ecore_chain.h"
#include "ecore_int_api.h"

/**
 * @brief ecore_init_dp - initialize the debug level
 *
 * @param p_dev
 * @param dp_module
 * @param dp_level
 * @param dp_ctx
 */
void ecore_init_dp(struct ecore_dev *p_dev,
		   u32 dp_module,
		   u8 dp_level,
		   void *dp_ctx);

/**
 * @brief ecore_init_struct - initialize the device structure to
 *        its defaults
 *
 * @param p_dev
 */
enum _ecore_status_t ecore_init_struct(struct ecore_dev *p_dev);

/**
 * @brief ecore_resc_free -
 *
 * @param p_dev
 */
void ecore_resc_free(struct ecore_dev *p_dev);

/**
 * @brief ecore_resc_alloc -
 *
 * @param p_dev
 *
 * @return enum _ecore_status_t
 */
enum _ecore_status_t ecore_resc_alloc(struct ecore_dev *p_dev);

/**
 * @brief ecore_resc_setup -
 *
 * @param p_dev
 */
void ecore_resc_setup(struct ecore_dev *p_dev);

enum ecore_mfw_timeout_fallback {
	ECORE_TO_FALLBACK_TO_NONE,
	ECORE_TO_FALLBACK_TO_DEFAULT,
	ECORE_TO_FALLBACK_FAIL_LOAD,
};

enum ecore_override_force_load {
	ECORE_OVERRIDE_FORCE_LOAD_NONE,
	ECORE_OVERRIDE_FORCE_LOAD_ALWAYS,
	ECORE_OVERRIDE_FORCE_LOAD_NEVER,
};

struct ecore_drv_load_params {
	/* Indicates whether the driver is running over a crash kernel.
	 * As part of the load request, this will be used for providing the
	 * driver role to the MFW.
	 * In case of a crash kernel over PDA - this should be set to false.
	 */
	bool is_crash_kernel;

	/* The timeout value that the MFW should use when locking the engine for
	 * the driver load process.
	 * A value of '0' means the default value, and '255' means no timeout.
	 */
	u8 mfw_timeout_val;
#define ECORE_LOAD_REQ_LOCK_TO_DEFAULT	0
#define ECORE_LOAD_REQ_LOCK_TO_NONE	255

	/* Action to take in case the MFW doesn't support timeout values other
	 * than default and none.
	 */
	enum ecore_mfw_timeout_fallback mfw_timeout_fallback;

	/* Avoid engine reset when first PF loads on it */
	bool avoid_eng_reset;

	/* Allow overriding the default force load behavior */
	enum ecore_override_force_load override_force_load;
};

struct ecore_hw_init_params {
	/* Tunneling parameters */
	struct ecore_tunnel_info *p_tunn;

	bool b_hw_start;

	/* Interrupt mode [msix, inta, etc.] to use */
	enum ecore_int_mode int_mode;

	/* NPAR tx switching to be used for vports configured for tx-switching
	 */
	bool allow_npar_tx_switch;

	/* Binary fw data pointer in binary fw file */
	const u8 *bin_fw_data;

	/* Driver load parameters */
	struct ecore_drv_load_params *p_drv_load_params;

	/* Avoid engine affinity for RoCE/storage in case of CMT mode */
	bool avoid_eng_affin;

	/* SPQ block timeout in msec */
	u32 spq_timeout_ms;
};

/**
 * @brief ecore_hw_init -
 *
 * @param p_dev
 * @param p_params
 *
 * @return enum _ecore_status_t
 */
enum _ecore_status_t ecore_hw_init(struct ecore_dev *p_dev,
				   struct ecore_hw_init_params *p_params);

/**
 * @brief ecore_hw_timers_stop_all -
 *
 * @param p_dev
 *
 * @return void
 */
void ecore_hw_timers_stop_all(struct ecore_dev *p_dev);

/**
 * @brief ecore_hw_stop -
 *
 * @param p_dev
 *
 * @return enum _ecore_status_t
 */
enum _ecore_status_t ecore_hw_stop(struct ecore_dev *p_dev);

/**
 * @brief ecore_hw_stop_fastpath -should be called incase
 *        slowpath is still required for the device,
 *        but fastpath is not.
 *
 * @param p_dev
 *
 * @return enum _ecore_status_t
 */
enum _ecore_status_t ecore_hw_stop_fastpath(struct ecore_dev *p_dev);

#ifndef LINUX_REMOVE
/**
 * @brief ecore_prepare_hibernate -should be called when
 *        the system is going into the hibernate state
 *
 * @param p_dev
 *
 */
void ecore_prepare_hibernate(struct ecore_dev *p_dev);

enum ecore_db_rec_width {
	DB_REC_WIDTH_32B,
	DB_REC_WIDTH_64B,
};

enum ecore_db_rec_space {
	DB_REC_KERNEL,
	DB_REC_USER,
};

/**
 * @brief db_recovery_add - add doorbell information to the doorbell
 * recovery mechanism.
 *
 * @param p_dev
 * @param db_addr - doorbell address
 * @param db_data - address of where db_data is stored
 * @param db_width - doorbell is 32b pr 64b
 * @param db_space - doorbell recovery addresses are user or kernel space
 */
enum _ecore_status_t ecore_db_recovery_add(struct ecore_dev *p_dev,
					   void OSAL_IOMEM *db_addr,
					   void *db_data,
					   enum ecore_db_rec_width db_width,
					   enum ecore_db_rec_space db_space);

/**
 * @brief db_recovery_del - remove doorbell information from the doorbell
 * recovery mechanism. db_data serves as key (db_addr is not unique).
 *
 * @param cdev
 * @param db_addr - doorbell address
 * @param db_data - address where db_data is stored. Serves as key for the
 *                  entry to delete.
 */
enum _ecore_status_t ecore_db_recovery_del(struct ecore_dev *p_dev,
					   void OSAL_IOMEM *db_addr,
					   void *db_data);

static OSAL_INLINE bool ecore_is_mf_ufp(struct ecore_hwfn *p_hwfn)
{
	return !!OSAL_GET_BIT(ECORE_MF_UFP_SPECIFIC, &p_hwfn->p_dev->mf_bits);
}

#endif

/**
 * @brief ecore_hw_start_fastpath -restart fastpath traffic,
 *        only if hw_stop_fastpath was called

 * @param p_hwfn
 *
 * @return enum _ecore_status_t
 */
enum _ecore_status_t ecore_hw_start_fastpath(struct ecore_hwfn *p_hwfn);

enum ecore_hw_prepare_result {
	ECORE_HW_PREPARE_SUCCESS,

	/* FAILED results indicate probe has failed & cleaned up */
	ECORE_HW_PREPARE_FAILED_ENG2,
	ECORE_HW_PREPARE_FAILED_ME,
	ECORE_HW_PREPARE_FAILED_MEM,
	ECORE_HW_PREPARE_FAILED_DEV,
	ECORE_HW_PREPARE_FAILED_NVM,

	/* BAD results indicate probe is passed even though some wrongness
	 * has occurred; Trying to actually use [I.e., hw_init()] might have
	 * dire reprecautions.
	 */
	ECORE_HW_PREPARE_BAD_IOV,
	ECORE_HW_PREPARE_BAD_MCP,
	ECORE_HW_PREPARE_BAD_IGU,
};

struct ecore_hw_prepare_params {
	/* Personality to initialize */
	int personality;

	/* Force the driver's default resource allocation */
	bool drv_resc_alloc;

	/* Check the reg_fifo after any register access */
	bool chk_reg_fifo;

	/* Request the MFW to initiate PF FLR */
	bool initiate_pf_flr;

	/* The OS Epoch time in seconds */
	u32 epoch;

	/* Allow the MFW to collect a crash dump */
	bool allow_mdump;

	/* Allow prepare to pass even if some initializations are failing.
	 * If set, the `p_prepare_res' field would be set with the return,
	 * and might allow probe to pass even if there are certain issues.
	 */
	bool b_relaxed_probe;
	enum ecore_hw_prepare_result p_relaxed_res;

	/* Enable/disable request by ecore client for pacing */
	bool b_en_pacing;

	/* Indicates whether this PF serves a storage target */
	bool b_is_target;

	/* retry count for VF acquire on channel timeout */
	u8 acquire_retry_cnt;
};

/**
 * @brief ecore_hw_prepare -
 *
 * @param p_dev
 * @param p_params
 *
 * @return enum _ecore_status_t
 */
enum _ecore_status_t ecore_hw_prepare(struct ecore_dev *p_dev,
				      struct ecore_hw_prepare_params *p_params);

/**
 * @brief ecore_hw_remove -
 *
 * @param p_dev
 */
void ecore_hw_remove(struct ecore_dev *p_dev);

/**
 * @brief ecore_ptt_acquire - Allocate a PTT window
 *
 * Should be called at the entry point to the driver (at the beginning of an
 * exported function)
 *
 * @param p_hwfn
 *
 * @return struct ecore_ptt
 */
struct ecore_ptt *ecore_ptt_acquire(struct ecore_hwfn *p_hwfn);

/**
 * @brief ecore_ptt_release - Release PTT Window
 *
 * Should be called at the end of a flow - at the end of the function that
 * acquired the PTT.
 *
 *
 * @param p_hwfn
 * @param p_ptt
 */
void ecore_ptt_release(struct ecore_hwfn *p_hwfn,
		       struct ecore_ptt *p_ptt);

struct ecore_eth_stats_common {
	u64 no_buff_discards;
	u64 packet_too_big_discard;
	u64 ttl0_discard;
	u64 rx_ucast_bytes;
	u64 rx_mcast_bytes;
	u64 rx_bcast_bytes;
	u64 rx_ucast_pkts;
	u64 rx_mcast_pkts;
	u64 rx_bcast_pkts;
	u64 mftag_filter_discards;
	u64 mac_filter_discards;
	u64 gft_filter_drop;
	u64 tx_ucast_bytes;
	u64 tx_mcast_bytes;
	u64 tx_bcast_bytes;
	u64 tx_ucast_pkts;
	u64 tx_mcast_pkts;
	u64 tx_bcast_pkts;
	u64 tx_err_drop_pkts;
	u64 tpa_coalesced_pkts;
	u64 tpa_coalesced_events;
	u64 tpa_aborts_num;
	u64 tpa_not_coalesced_pkts;
	u64 tpa_coalesced_bytes;

	/* port */
	u64 rx_64_byte_packets;
	u64 rx_65_to_127_byte_packets;
	u64 rx_128_to_255_byte_packets;
	u64 rx_256_to_511_byte_packets;
	u64 rx_512_to_1023_byte_packets;
	u64 rx_1024_to_1518_byte_packets;
	u64 rx_crc_errors;
	u64 rx_mac_crtl_frames;
	u64 rx_pause_frames;
	u64 rx_pfc_frames;
	u64 rx_align_errors;
	u64 rx_carrier_errors;
	u64 rx_oversize_packets;
	u64 rx_jabbers;
	u64 rx_undersize_packets;
	u64 rx_fragments;
	u64 tx_64_byte_packets;
	u64 tx_65_to_127_byte_packets;
	u64 tx_128_to_255_byte_packets;
	u64 tx_256_to_511_byte_packets;
	u64 tx_512_to_1023_byte_packets;
	u64 tx_1024_to_1518_byte_packets;
	u64 tx_pause_frames;
	u64 tx_pfc_frames;
	u64 brb_truncates;
	u64 brb_discards;
	u64 rx_mac_bytes;
	u64 rx_mac_uc_packets;
	u64 rx_mac_mc_packets;
	u64 rx_mac_bc_packets;
	u64 rx_mac_frames_ok;
	u64 tx_mac_bytes;
	u64 tx_mac_uc_packets;
	u64 tx_mac_mc_packets;
	u64 tx_mac_bc_packets;
	u64 tx_mac_ctrl_frames;
	u64 link_change_count;
};

struct ecore_eth_stats_bb {
	u64 rx_1519_to_1522_byte_packets;
	u64 rx_1519_to_2047_byte_packets;
	u64 rx_2048_to_4095_byte_packets;
	u64 rx_4096_to_9216_byte_packets;
	u64 rx_9217_to_16383_byte_packets;
	u64 tx_1519_to_2047_byte_packets;
	u64 tx_2048_to_4095_byte_packets;
	u64 tx_4096_to_9216_byte_packets;
	u64 tx_9217_to_16383_byte_packets;
	u64 tx_lpi_entry_count;
	u64 tx_total_collisions;
};

struct ecore_eth_stats_ah {
	u64 rx_1519_to_max_byte_packets;
	u64 tx_1519_to_max_byte_packets;
};

struct ecore_eth_stats {
	struct ecore_eth_stats_common common;
	union {
		struct ecore_eth_stats_bb bb;
		struct ecore_eth_stats_ah ah;
	};
};

/**
 * @brief ecore_chain_alloc - Allocate and initialize a chain
 *
 * @param p_hwfn
 * @param intended_use
 * @param mode
 * @param num_elems
 * @param elem_size
 * @param p_chain
 *
 * @return enum _ecore_status_t
 */
enum _ecore_status_t
ecore_chain_alloc(struct ecore_dev *p_dev,
		  enum ecore_chain_use_mode intended_use,
		  enum ecore_chain_mode mode,
		  enum ecore_chain_cnt_type cnt_type,
		  u32 num_elems,
		  osal_size_t elem_size,
		  struct ecore_chain *p_chain,
		  struct ecore_chain_ext_pbl *ext_pbl);

/**
 * @brief ecore_chain_free - Free chain DMA memory
 *
 * @param p_hwfn
 * @param p_chain
 */
void ecore_chain_free(struct ecore_dev *p_dev,
		      struct ecore_chain *p_chain);

/**
 * @@brief ecore_fw_l2_queue - Get absolute L2 queue ID
 *
 *  @param p_hwfn
 *  @param src_id - relative to p_hwfn
 *  @param dst_id - absolute per engine
 *
 *  @return enum _ecore_status_t
 */
enum _ecore_status_t ecore_fw_l2_queue(struct ecore_hwfn *p_hwfn,
				       u16 src_id,
				       u16 *dst_id);

/**
 * @@brief ecore_fw_vport - Get absolute vport ID
 *
 *  @param p_hwfn
 *  @param src_id - relative to p_hwfn
 *  @param dst_id - absolute per engine
 *
 *  @return enum _ecore_status_t
 */
enum _ecore_status_t ecore_fw_vport(struct ecore_hwfn *p_hwfn,
				    u8 src_id,
				    u8 *dst_id);

/**
 * @@brief ecore_fw_rss_eng - Get absolute RSS engine ID
 *
 *  @param p_hwfn
 *  @param src_id - relative to p_hwfn
 *  @param dst_id - absolute per engine
 *
 *  @return enum _ecore_status_t
 */
enum _ecore_status_t ecore_fw_rss_eng(struct ecore_hwfn *p_hwfn,
				      u8 src_id,
				      u8 *dst_id);

/**
 * @brief ecore_llh_get_num_ppfid - Return the allocated number of LLH filter
 *	banks that are allocated to the PF.
 *
 * @param p_dev
 *
 * @return u8 - Number of LLH filter banks
 */
u8 ecore_llh_get_num_ppfid(struct ecore_dev *p_dev);

enum ecore_eng {
	ECORE_ENG0,
	ECORE_ENG1,
	ECORE_BOTH_ENG,
};

/**
 * @brief ecore_llh_get_l2_affinity_hint - Return the hint for the L2 affinity
 *
 * @param p_dev
 *
 * @return enum ecore_eng - L2 affintiy hint
 */
enum ecore_eng ecore_llh_get_l2_affinity_hint(struct ecore_dev *p_dev);

/**
 * @brief ecore_llh_set_ppfid_affinity - Set the engine affinity for the given
 *	LLH filter bank.
 *
 * @param p_dev
 * @param ppfid - relative within the allocated ppfids ('0' is the default one).
 * @param eng
 *
 * @return enum _ecore_status_t
 */
enum _ecore_status_t ecore_llh_set_ppfid_affinity(struct ecore_dev *p_dev,
						  u8 ppfid, enum ecore_eng eng);

/**
 * @brief ecore_llh_set_roce_affinity - Set the RoCE engine affinity
 *
 * @param p_dev
 * @param eng
 *
 * @return enum _ecore_status_t
 */
enum _ecore_status_t ecore_llh_set_roce_affinity(struct ecore_dev *p_dev,
						 enum ecore_eng eng);

/**
 * @brief ecore_llh_add_mac_filter - Add a LLH MAC filter into the given filter
 *	bank.
 *
 * @param p_dev
 * @param ppfid - relative within the allocated ppfids ('0' is the default one).
 * @param mac_addr - MAC to add
 *
 * @return enum _ecore_status_t
 */
enum _ecore_status_t ecore_llh_add_mac_filter(struct ecore_dev *p_dev, u8 ppfid,
					      u8 mac_addr[ETH_ALEN]);

/**
 * @brief ecore_llh_remove_mac_filter - Remove a LLH MAC filter from the given
 *	filter bank.
 *
 * @param p_dev
 * @param ppfid - relative within the allocated ppfids ('0' is the default one).
 * @param mac_addr - MAC to remove
 */
void ecore_llh_remove_mac_filter(struct ecore_dev *p_dev, u8 ppfid,
				 u8 mac_addr[ETH_ALEN]);

enum ecore_llh_prot_filter_type_t {
	ECORE_LLH_FILTER_ETHERTYPE,
	ECORE_LLH_FILTER_TCP_SRC_PORT,
	ECORE_LLH_FILTER_TCP_DEST_PORT,
	ECORE_LLH_FILTER_TCP_SRC_AND_DEST_PORT,
	ECORE_LLH_FILTER_UDP_SRC_PORT,
	ECORE_LLH_FILTER_UDP_DEST_PORT,
	ECORE_LLH_FILTER_UDP_SRC_AND_DEST_PORT
};

/**
 * @brief ecore_llh_add_protocol_filter - Add a LLH protocol filter into the
 *	given filter bank.
 *
 * @param p_dev
 * @param ppfid - relative within the allocated ppfids ('0' is the default one).
 * @param type - type of filters and comparing
 * @param source_port_or_eth_type - source port or ethertype to add
 * @param dest_port - destination port to add
 *
 * @return enum _ecore_status_t
 */
enum _ecore_status_t
ecore_llh_add_protocol_filter(struct ecore_dev *p_dev, u8 ppfid,
			      enum ecore_llh_prot_filter_type_t type,
			      u16 source_port_or_eth_type, u16 dest_port);

/**
 * @brief ecore_llh_remove_protocol_filter - Remove a LLH protocol filter from
 *	the given filter bank.
 *
 * @param p_dev
 * @param ppfid - relative within the allocated ppfids ('0' is the default one).
 * @param type - type of filters and comparing
 * @param source_port_or_eth_type - source port or ethertype to add
 * @param dest_port - destination port to add
 */
void ecore_llh_remove_protocol_filter(struct ecore_dev *p_dev, u8 ppfid,
				      enum ecore_llh_prot_filter_type_t type,
				      u16 source_port_or_eth_type,
				      u16 dest_port);

/**
 * @brief ecore_llh_clear_ppfid_filters - Remove all LLH filters from the given
 *	filter bank.
 *
 * @param p_dev
 * @param ppfid - relative within the allocated ppfids ('0' is the default one).
 */
void ecore_llh_clear_ppfid_filters(struct ecore_dev *p_dev, u8 ppfid);

/**
 * @brief ecore_llh_clear_all_filters - Remove all LLH filters
 *
 * @param p_dev
 */
void ecore_llh_clear_all_filters(struct ecore_dev *p_dev);

/**
 * @brief ecore_llh_set_function_as_default - set function as default per port
 *
 * @param p_hwfn
 * @param p_ptt
 */
enum _ecore_status_t
ecore_llh_set_function_as_default(struct ecore_hwfn *p_hwfn,
				  struct ecore_ptt *p_ptt);

/**
 *@brief Cleanup of previous driver remains prior to load
 *
 * @param p_hwfn
 * @param p_ptt
 * @param id - For PF, engine-relative. For VF, PF-relative.
 * @param is_vf - true iff cleanup is made for a VF.
 *
 * @return enum _ecore_status_t
 */
enum _ecore_status_t ecore_final_cleanup(struct ecore_hwfn	*p_hwfn,
					 struct ecore_ptt	*p_ptt,
					 u16			id,
					 bool			is_vf);

/**
 * @brief ecore_get_queue_coalesce - Retrieve coalesce value for a given queue.
 *
 * @param p_hwfn
 * @param p_coal - store coalesce value read from the hardware.
 * @param p_handle
 *
 * @return enum _ecore_status_t
 **/
enum _ecore_status_t
ecore_get_queue_coalesce(struct ecore_hwfn *p_hwfn, u16 *coal,
			 void *handle);

/**
 * @brief ecore_set_queue_coalesce - Configure coalesce parameters for Rx and
 *    Tx queue. The fact that we can configure coalescing to up to 511, but on
 *    varying accuracy [the bigger the value the less accurate] up to a mistake
 *    of 3usec for the highest values.
 *    While the API allows setting coalescing per-qid, all queues sharing a SB
 *    should be in same range [i.e., either 0-0x7f, 0x80-0xff or 0x100-0x1ff]
 *    otherwise configuration would break.
 *
 * @param p_hwfn
 * @param rx_coal - Rx Coalesce value in micro seconds.
 * @param tx_coal - TX Coalesce value in micro seconds.
 * @param p_handle
 *
 * @return enum _ecore_status_t
 **/
enum _ecore_status_t
ecore_set_queue_coalesce(struct ecore_hwfn *p_hwfn, u16 rx_coal,
			 u16 tx_coal, void *p_handle);

/**
 * @brief ecore_pglueb_set_pfid_enable - Enable or disable PCI BUS MASTER
 *
 * @param p_hwfn
 * @param p_ptt
 * @param b_enable - true/false
 *
 * @return enum _ecore_status_t
 */
enum _ecore_status_t ecore_pglueb_set_pfid_enable(struct ecore_hwfn *p_hwfn,
						  struct ecore_ptt *p_ptt,
						  bool b_enable);

/**
 * @brief Whether FIP discovery fallback special mode is enabled or not.
 *
 * @param cdev
 *
 * @return true if device is in FIP special mode, false otherwise.
 */
bool ecore_is_mf_fip_special(struct ecore_dev *p_dev);
#endif
