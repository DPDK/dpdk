/*
 * Copyright (c) 2016 QLogic Corporation.
 * All rights reserved.
 * www.qlogic.com
 *
 * See LICENSE.qede_pmd for copyright and licensing details.
 */

#ifndef __ECORE_INT_H__
#define __ECORE_INT_H__

#include "ecore.h"
#include "ecore_int_api.h"

#define ECORE_CAU_DEF_RX_TIMER_RES 0
#define ECORE_CAU_DEF_TX_TIMER_RES 0

#define ECORE_SB_ATT_IDX	0x0001
#define ECORE_SB_EVENT_MASK	0x0003

#define SB_ALIGNED_SIZE(p_hwfn)					\
	ALIGNED_TYPE_SIZE(struct status_block, p_hwfn)

struct ecore_igu_block {
	u8 status;
#define ECORE_IGU_STATUS_FREE	0x01
#define ECORE_IGU_STATUS_VALID	0x02
#define ECORE_IGU_STATUS_PF	0x04

	u8 vector_number;
	u8 function_id;
	u8 is_pf;
};

struct ecore_igu_map {
	struct ecore_igu_block igu_blocks[MAX_TOT_SB_PER_PATH];
};

struct ecore_igu_info {
	struct ecore_igu_map igu_map;
	u16 igu_dsb_id;
	u16 igu_base_sb;
	u16 igu_base_sb_iov;
	u16 igu_sb_cnt;
	u16 igu_sb_cnt_iov;
	u16 free_blks;
};

/* TODO Names of function may change... */
void ecore_int_igu_init_pure_rt(struct ecore_hwfn *p_hwfn,
				struct ecore_ptt *p_ptt,
				bool b_set, bool b_slowpath);

void ecore_int_igu_init_rt(struct ecore_hwfn *p_hwfn);

/**
 * @brief ecore_int_igu_read_cam - Reads the IGU CAM.
 *	This function needs to be called during hardware
 *	prepare. It reads the info from igu cam to know which
 *	status block is the default / base status block etc.
 *
 * @param p_hwfn
 * @param p_ptt
 *
 * @return enum _ecore_status_t
 */
enum _ecore_status_t ecore_int_igu_read_cam(struct ecore_hwfn *p_hwfn,
					    struct ecore_ptt *p_ptt);

typedef enum _ecore_status_t (*ecore_int_comp_cb_t) (struct ecore_hwfn *p_hwfn,
						     void *cookie);
/**
 * @brief ecore_int_register_cb - Register callback func for
 *      slowhwfn statusblock.
 *
 *	Every protocol that uses the slowhwfn status block
 *	should register a callback function that will be called
 *	once there is an update of the sp status block.
 *
 * @param p_hwfn
 * @param comp_cb - function to be called when there is an
 *                  interrupt on the sp sb
 *
 * @param cookie  - passed to the callback function
 * @param sb_idx  - OUT parameter which gives the chosen index
 *                  for this protocol.
 * @param p_fw_cons  - pointer to the actual address of the
 *                     consumer for this protocol.
 *
 * @return enum _ecore_status_t
 */
enum _ecore_status_t ecore_int_register_cb(struct ecore_hwfn *p_hwfn,
					   ecore_int_comp_cb_t comp_cb,
					   void *cookie,
					   u8 *sb_idx, __le16 **p_fw_cons);
/**
 * @brief ecore_int_unregister_cb - Unregisters callback
 *      function from sp sb.
 *      Partner of ecore_int_register_cb -> should be called
 *      when no longer required.
 *
 * @param p_hwfn
 * @param pi
 *
 * @return enum _ecore_status_t
 */
enum _ecore_status_t ecore_int_unregister_cb(struct ecore_hwfn *p_hwfn, u8 pi);

/**
 * @brief ecore_int_get_sp_sb_id - Get the slowhwfn sb id.
 *
 * @param p_hwfn
 *
 * @return u16
 */
u16 ecore_int_get_sp_sb_id(struct ecore_hwfn *p_hwfn);

/**
 * @brief Status block cleanup. Should be called for each status
 *        block that will be used -> both PF / VF
 *
 * @param p_hwfn
 * @param p_ptt
 * @param sb_id		- igu status block id
 * @param opaque	- opaque fid of the sb owner.
 * @param cleanup_set	- set(1) / clear(0)
 */
void ecore_int_igu_init_pure_rt_single(struct ecore_hwfn *p_hwfn,
				       struct ecore_ptt *p_ptt,
				       u32 sb_id, u16 opaque, bool b_set);

/**
 * @brief ecore_int_cau_conf - configure cau for a given status
 *        block
 *
 * @param p_hwfn
 * @param ptt
 * @param sb_phys
 * @param igu_sb_id
 * @param vf_number
 * @param vf_valid
 */
void ecore_int_cau_conf_sb(struct ecore_hwfn *p_hwfn,
			   struct ecore_ptt *p_ptt,
			   dma_addr_t sb_phys,
			   u16 igu_sb_id, u16 vf_number, u8 vf_valid);

/**
* @brief ecore_int_alloc
*
* @param p_hwfn
 * @param p_ptt
*
* @return enum _ecore_status_t
*/
enum _ecore_status_t ecore_int_alloc(struct ecore_hwfn	*p_hwfn,
				     struct ecore_ptt	*p_ptt);

/**
* @brief ecore_int_free
*
* @param p_hwfn
*/
void ecore_int_free(struct ecore_hwfn *p_hwfn);

/**
* @brief ecore_int_setup
*
* @param p_hwfn
* @param p_ptt
*/
void ecore_int_setup(struct ecore_hwfn *p_hwfn, struct ecore_ptt *p_ptt);

/**
 * @brief - Returns an Rx queue index appropriate for usage with given SB.
 *
 * @param p_hwfn
 * @param sb_id - absolute index of SB
 *
 * @return index of Rx queue
 */
u16 ecore_int_queue_id_from_sb_id(struct ecore_hwfn *p_hwfn, u16 sb_id);

/**
 * @brief - Enable Interrupt & Attention for hw function
 *
 * @param p_hwfn
 * @param p_ptt
 * @param int_mode
 *
* @return enum _ecore_status_t
 */
enum _ecore_status_t ecore_int_igu_enable(struct ecore_hwfn *p_hwfn,
					  struct ecore_ptt *p_ptt,
					  enum ecore_int_mode int_mode);

/**
 * @brief - Initialize CAU status block entry
 *
 * @param p_hwfn
 * @param p_sb_entry
 * @param pf_id
 * @param vf_number
 * @param vf_valid
 */
void ecore_init_cau_sb_entry(struct ecore_hwfn *p_hwfn,
			     struct cau_sb_entry *p_sb_entry, u8 pf_id,
			     u16 vf_number, u8 vf_valid);

enum _ecore_status_t ecore_int_set_timer_res(struct ecore_hwfn *p_hwfn,
					     struct ecore_ptt *p_ptt,
					     u8 timer_res, u16 sb_id, bool tx);
#ifndef ASIC_ONLY
#define ECORE_MAPPING_MEMORY_SIZE(dev) \
	((CHIP_REV_IS_SLOW(dev) && (!(dev)->b_is_emul_full)) ? \
	 136 : NUM_OF_SBS(dev))
#else
#define ECORE_MAPPING_MEMORY_SIZE(dev) NUM_OF_SBS(dev)
#endif

#endif /* __ECORE_INT_H__ */
