/*
 * Copyright (c) 2016 QLogic Corporation.
 * All rights reserved.
 * www.qlogic.com
 *
 * See LICENSE.qede_pmd for copyright and licensing details.
 */

#ifndef __ECORE_DEV_API_H__
#define __ECORE_DEV_API_H__

#include "ecore_status.h"
#include "ecore_chain.h"
#include "ecore_int_api.h"

struct ecore_tunn_start_params;

/**
 * @brief ecore_init_dp - initialize the debug level
 *
 * @param p_dev
 * @param dp_module
 * @param dp_level
 * @param dp_ctx
 */
void ecore_init_dp(struct ecore_dev *p_dev,
		   u32 dp_module, u8 dp_level, void *dp_ctx);

/**
 * @brief ecore_init_struct - initialize the device structure to
 *        its defaults
 *
 * @param p_dev
 */
void ecore_init_struct(struct ecore_dev *p_dev);

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

/**
 * @brief ecore_hw_init -
 *
 * @param p_dev
 * @param p_tunn - tunneling parameters
 * @param b_hw_start
 * @param int_mode - interrupt mode [msix, inta, etc.] to use.
 * @param allow_npar_tx_switch - npar tx switching to be used
 *	  for vports configured for tx-switching.
 * @param bin_fw_data - binary fw data pointer in binary fw file.
 *			Pass NULL if not using binary fw file.
 *
 * @return enum _ecore_status_t
 */
enum _ecore_status_t ecore_hw_init(struct ecore_dev *p_dev,
				   struct ecore_tunn_start_params *p_tunn,
				   bool b_hw_start,
				   enum ecore_int_mode int_mode,
				   bool allow_npar_tx_switch,
				   const u8 *bin_fw_data);

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
 *        slowpath is still required for the device, but
 *        fastpath is not.
 *
 * @param p_dev
 *
 */
void ecore_hw_stop_fastpath(struct ecore_dev *p_dev);

/**
 * @brief ecore_prepare_hibernate -should be called when
 *        the system is going into the hibernate state
 *
 * @param p_dev
 *
 */
void ecore_prepare_hibernate(struct ecore_dev *p_dev);

/**
 * @brief ecore_hw_start_fastpath -restart fastpath traffic,
 *        only if hw_stop_fastpath was called

 * @param p_dev
 *
 */
void ecore_hw_start_fastpath(struct ecore_hwfn *p_hwfn);

/**
 * @brief ecore_hw_reset -
 *
 * @param p_dev
 *
 * @return enum _ecore_status_t
 */
enum _ecore_status_t ecore_hw_reset(struct ecore_dev *p_dev);

/**
 * @brief ecore_hw_prepare -
 *
 * @param p_dev
 * @param personality - personality to initialize
 *
 * @return enum _ecore_status_t
 */
enum _ecore_status_t ecore_hw_prepare(struct ecore_dev *p_dev, int personality);

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
void ecore_ptt_release(struct ecore_hwfn *p_hwfn, struct ecore_ptt *p_ptt);

#ifndef __EXTRACT__LINUX__
struct ecore_eth_stats {
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
	u64 rx_1519_to_1522_byte_packets;
	u64 rx_1519_to_2047_byte_packets;
	u64 rx_2048_to_4095_byte_packets;
	u64 rx_4096_to_9216_byte_packets;
	u64 rx_9217_to_16383_byte_packets;
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
	u64 tx_1519_to_2047_byte_packets;
	u64 tx_2048_to_4095_byte_packets;
	u64 tx_4096_to_9216_byte_packets;
	u64 tx_9217_to_16383_byte_packets;
	u64 tx_pause_frames;
	u64 tx_pfc_frames;
	u64 tx_lpi_entry_count;
	u64 tx_total_collisions;
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
};
#endif

enum ecore_dmae_address_type_t {
	ECORE_DMAE_ADDRESS_HOST_VIRT,
	ECORE_DMAE_ADDRESS_HOST_PHYS,
	ECORE_DMAE_ADDRESS_GRC
};

/* value of flags If ECORE_DMAE_FLAG_RW_REPL_SRC flag is set and the
 * source is a block of length DMAE_MAX_RW_SIZE and the
 * destination is larger, the source block will be duplicated as
 * many times as required to fill the destination block. This is
 * used mostly to write a zeroed buffer to destination address
 * using DMA
 */
#define ECORE_DMAE_FLAG_RW_REPL_SRC	0x00000001
#define ECORE_DMAE_FLAG_VF_SRC		0x00000002
#define ECORE_DMAE_FLAG_VF_DST		0x00000004
#define ECORE_DMAE_FLAG_COMPLETION_DST	0x00000008

struct ecore_dmae_params {
	u32 flags;		/* consists of ECORE_DMAE_FLAG_* values */
	u8 src_vfid;
	u8 dst_vfid;
};

/**
* @brief ecore_dmae_host2grc - copy data from source addr to
* dmae registers using the given ptt
*
* @param p_hwfn
* @param p_ptt
* @param source_addr
* @param grc_addr (dmae_data_offset)
* @param size_in_dwords
* @param flags (one of the flags defined above)
*/
enum _ecore_status_t
ecore_dmae_host2grc(struct ecore_hwfn *p_hwfn,
		    struct ecore_ptt *p_ptt,
		    u64 source_addr,
		    u32 grc_addr, u32 size_in_dwords, u32 flags);

/**
* @brief ecore_dmae_grc2host - Read data from dmae data offset
* to source address using the given ptt
*
* @param p_ptt
* @param grc_addr (dmae_data_offset)
* @param dest_addr
* @param size_in_dwords
* @param flags - one of the flags defined above
*/
enum _ecore_status_t
ecore_dmae_grc2host(struct ecore_hwfn *p_hwfn,
		    struct ecore_ptt *p_ptt,
		    u32 grc_addr,
		    dma_addr_t dest_addr, u32 size_in_dwords, u32 flags);

/**
* @brief ecore_dmae_host2host - copy data from to source address
* to a destination address (for SRIOV) using the given ptt
*
* @param p_hwfn
* @param p_ptt
* @param source_addr
* @param dest_addr
* @param size_in_dwords
* @param params
*/
enum _ecore_status_t
ecore_dmae_host2host(struct ecore_hwfn *p_hwfn,
		     struct ecore_ptt *p_ptt,
		     dma_addr_t source_addr,
		     dma_addr_t dest_addr,
		     u32 size_in_dwords, struct ecore_dmae_params *p_params);

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
		  osal_size_t elem_size, struct ecore_chain *p_chain);

/**
 * @brief ecore_chain_free - Free chain DMA memory
 *
 * @param p_hwfn
 * @param p_chain
 */
void ecore_chain_free(struct ecore_dev *p_dev, struct ecore_chain *p_chain);

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
				       u16 src_id, u16 *dst_id);

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
				    u8 src_id, u8 *dst_id);

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
				      u8 src_id, u8 *dst_id);

/**
 * @brief ecore_llh_add_mac_filter - configures a MAC filter in llh
 *
 * @param p_hwfn
 * @param p_ptt
 * @param p_filter - MAC to add
 */
enum _ecore_status_t ecore_llh_add_mac_filter(struct ecore_hwfn *p_hwfn,
					      struct ecore_ptt *p_ptt,
					      u8 *p_filter);

/**
 * @brief ecore_llh_remove_mac_filter - removes a MAC filtre from llh
 *
 * @param p_hwfn
 * @param p_ptt
 * @param p_filter - MAC to remove
 */
void ecore_llh_remove_mac_filter(struct ecore_hwfn *p_hwfn,
				 struct ecore_ptt *p_ptt, u8 *p_filter);

/**
 * @brief ecore_llh_add_ethertype_filter - configures a ethertype filter in llh
 *
 * @param p_hwfn
 * @param p_ptt
 * @param filter - ethertype to add
 */
enum _ecore_status_t ecore_llh_add_ethertype_filter(struct ecore_hwfn *p_hwfn,
						    struct ecore_ptt *p_ptt,
						    u16 filter);

/**
 * @brief ecore_llh_remove_ethertype_filter - removes a ethertype llh filter
 *
 * @param p_hwfn
 * @param p_ptt
 * @param filter - ethertype to remove
 */
void ecore_llh_remove_ethertype_filter(struct ecore_hwfn *p_hwfn,
				       struct ecore_ptt *p_ptt, u16 filter);

/**
 * @brief ecore_llh_clear_all_filters - removes all MAC filters from llh
 *
 * @param p_hwfn
 * @param p_ptt
 */
void ecore_llh_clear_all_filters(struct ecore_hwfn *p_hwfn,
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
enum _ecore_status_t ecore_final_cleanup(struct ecore_hwfn *p_hwfn,
					 struct ecore_ptt *p_ptt,
					 u16 id, bool is_vf);

/**
 * @brief ecore_test_registers - Perform register tests
 *
 * @param p_hwfn
 * @param p_ptt
 *
 *  @return enum _ecore_status_t
 */
enum _ecore_status_t ecore_test_registers(struct ecore_hwfn *p_hwfn,
					  struct ecore_ptt *p_ptt);

/**
 * @brief ecore_set_rxq_coalesce - Configure coalesce parameters for an Rx queue
 *
 * @param p_hwfn
 * @param p_ptt
 * @param coalesce - Coalesce value in micro seconds.
 * @param qid - Queue index.
 *
 * @return enum _ecore_status_t
 */
enum _ecore_status_t ecore_set_rxq_coalesce(struct ecore_hwfn *p_hwfn,
					    struct ecore_ptt *p_ptt,
					    u8 coalesce, u8 qid);

/**
 * @brief ecore_set_txq_coalesce - Configure coalesce parameters for a Tx queue
 *
 * @param p_hwfn
 * @param p_ptt
 * @param coalesce - Coalesce value in micro seconds.
 * @param qid - Queue index.
 *
 * @return enum _ecore_status_t
 */
enum _ecore_status_t ecore_set_txq_coalesce(struct ecore_hwfn *p_hwfn,
					    struct ecore_ptt *p_ptt,
					    u8 coalesce, u8 qid);

#endif
