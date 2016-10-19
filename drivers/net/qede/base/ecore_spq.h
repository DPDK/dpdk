/*
 * Copyright (c) 2016 QLogic Corporation.
 * All rights reserved.
 * www.qlogic.com
 *
 * See LICENSE.qede_pmd for copyright and licensing details.
 */

#ifndef __ECORE_SPQ_H__
#define __ECORE_SPQ_H__

#include "ecore_hsi_common.h"
#include "ecore_status.h"
#include "ecore_hsi_eth.h"
#include "ecore_chain.h"
#include "ecore_sp_api.h"

union ramrod_data {
	struct pf_start_ramrod_data			pf_start;
	struct pf_update_ramrod_data			pf_update;
	struct rl_update_ramrod_data			rl_update;
	struct rx_queue_start_ramrod_data		rx_queue_start;
	struct rx_queue_update_ramrod_data		rx_queue_update;
	struct rx_queue_stop_ramrod_data		rx_queue_stop;
	struct tx_queue_start_ramrod_data		tx_queue_start;
	struct tx_queue_stop_ramrod_data		tx_queue_stop;
	struct vport_start_ramrod_data			vport_start;
	struct vport_stop_ramrod_data			vport_stop;
	struct vport_update_ramrod_data			vport_update;
	struct core_rx_start_ramrod_data		core_rx_queue_start;
	struct core_rx_stop_ramrod_data			core_rx_queue_stop;
	struct core_tx_start_ramrod_data		core_tx_queue_start;
	struct core_tx_stop_ramrod_data			core_tx_queue_stop;
	struct vport_filter_update_ramrod_data		vport_filter_update;

	struct vf_start_ramrod_data			vf_start;
	struct vf_stop_ramrod_data			vf_stop;
};

#define EQ_MAX_CREDIT	0xffffffff

enum spq_priority {
	ECORE_SPQ_PRIORITY_NORMAL,
	ECORE_SPQ_PRIORITY_HIGH,
};

union ecore_spq_req_comp {
	struct ecore_spq_comp_cb cb;
	u64			 *done_addr;
};

/* SPQ_MODE_EBLOCK */
struct ecore_spq_comp_done {
	u64 done;
	u8  fw_return_code;
};

struct ecore_spq_entry {
	osal_list_entry_t		list;

	u8				flags;

	/* HSI slow path element */
	struct slow_path_element	elem;

	union ramrod_data		ramrod;

	enum spq_priority		priority;

	/* pending queue for this entry */
	osal_list_t			*queue;

	enum spq_mode			comp_mode;
	struct ecore_spq_comp_cb	comp_cb;
	struct ecore_spq_comp_done	comp_done; /* SPQ_MODE_EBLOCK */
};

struct ecore_eq {
	struct ecore_chain	chain;
	u8			eq_sb_index;	/* index within the SB */
	__le16			*p_fw_cons;	/* ptr to index value */
};

struct ecore_consq {
	struct ecore_chain	chain;
};

struct ecore_spq {
	osal_spinlock_t			lock;

	osal_list_t			unlimited_pending;
	osal_list_t			pending;
	osal_list_t			completion_pending;
	osal_list_t			free_pool;

	struct ecore_chain		chain;

	/* allocated dma-able memory for spq entries (+ramrod data) */
	dma_addr_t			p_phys;
	struct ecore_spq_entry		*p_virt;

	/* Bitmap for handling out-of-order completions */
#define SPQ_RING_SIZE		\
	(CORE_SPQE_PAGE_SIZE_BYTES / sizeof(struct slow_path_element))
/* BITS_PER_LONG */
#define SPQ_COMP_BMAP_SIZE	(SPQ_RING_SIZE / (sizeof(unsigned long) * 8))
	unsigned long			p_comp_bitmap[SPQ_COMP_BMAP_SIZE];
	u8				comp_bitmap_idx;
#define SPQ_COMP_BMAP_SET_BIT(p_spq, idx)				\
	(OSAL_SET_BIT(((idx) % SPQ_RING_SIZE), (p_spq)->p_comp_bitmap))

#define SPQ_COMP_BMAP_CLEAR_BIT(p_spq, idx)				\
	(OSAL_CLEAR_BIT(((idx) % SPQ_RING_SIZE), (p_spq)->p_comp_bitmap))

#define SPQ_COMP_BMAP_TEST_BIT(p_spq, idx)	\
	(OSAL_TEST_BIT(((idx) % SPQ_RING_SIZE), (p_spq)->p_comp_bitmap))

	/* Statistics */
	u32				unlimited_pending_count;
	u32				normal_count;
	u32				high_count;
	u32				comp_sent_count;
	u32				comp_count;

	u32				cid;
};

struct ecore_port;
struct ecore_hwfn;

/**
 * @brief ecore_spq_post - Posts a Slow hwfn request to FW, or lacking that
 *        Pends it to the future list.
 *
 * @param p_hwfn
 * @param p_req
 *
 * @return enum _ecore_status_t
 */
enum _ecore_status_t ecore_spq_post(struct ecore_hwfn	   *p_hwfn,
				    struct ecore_spq_entry *p_ent,
				    u8                     *fw_return_code);

/**
 * @brief ecore_spq_allocate - Alloocates & initializes the SPQ and EQ.
 *
 * @param p_hwfn
 *
 * @return enum _ecore_status_t
 */
enum _ecore_status_t ecore_spq_alloc(struct ecore_hwfn	*p_hwfn);

/**
 * @brief ecore_spq_setup - Reset the SPQ to its start state.
 *
 * @param p_hwfn
 */
void ecore_spq_setup(struct ecore_hwfn *p_hwfn);

/**
 * @brief ecore_spq_deallocate - Deallocates the given SPQ struct.
 *
 * @param p_hwfn
 */
void ecore_spq_free(struct ecore_hwfn *p_hwfn);

/**
 * @brief ecore_spq_get_entry - Obtain an entrry from the spq
 *        free pool list.
 *
 *
 *
 * @param p_hwfn
 * @param pp_ent
 *
 * @return enum _ecore_status_t
 */
enum _ecore_status_t
ecore_spq_get_entry(struct ecore_hwfn		*p_hwfn,
		    struct ecore_spq_entry	**pp_ent);

/**
 * @brief ecore_spq_return_entry - Return an entry to spq free
 *                                 pool list
 *
 * @param p_hwfn
 * @param p_ent
 */
void ecore_spq_return_entry(struct ecore_hwfn		*p_hwfn,
			    struct ecore_spq_entry      *p_ent);
/**
 * @brief ecore_eq_allocate - Allocates & initializes an EQ struct
 *
 * @param p_hwfn
 * @param num_elem number of elements in the eq
 *
 * @return struct ecore_eq* - a newly allocated structure; NULL upon error.
 */
struct ecore_eq *ecore_eq_alloc(struct ecore_hwfn	*p_hwfn,
				 u16			num_elem);

/**
 * @brief ecore_eq_setup - Reset the SPQ to its start state.
 *
 * @param p_hwfn
 * @param p_eq
 */
void ecore_eq_setup(struct ecore_hwfn *p_hwfn,
		    struct ecore_eq   *p_eq);

/**
 * @brief ecore_eq_deallocate - deallocates the given EQ struct.
 *
 * @param p_hwfn
 * @param p_eq
 */
void ecore_eq_free(struct ecore_hwfn *p_hwfn,
		   struct ecore_eq   *p_eq);

/**
 * @brief ecore_eq_prod_update - update the FW with default EQ producer
 *
 * @param p_hwfn
 * @param prod
 */
void ecore_eq_prod_update(struct ecore_hwfn	*p_hwfn,
			  u16			prod);

/**
 * @brief ecore_eq_completion - Completes currently pending EQ elements
 *
 * @param p_hwfn
 * @param cookie
 *
 * @return enum _ecore_status_t
 */
enum _ecore_status_t ecore_eq_completion(struct ecore_hwfn	*p_hwfn,
					 void			*cookie);

/**
 * @brief ecore_spq_completion - Completes a single event
 *
 * @param p_hwfn
 * @param echo - echo value from cookie (used for determining completion)
 * @param p_data - data from cookie (used in callback function if applicable)
 *
 * @return enum _ecore_status_t
 */
enum _ecore_status_t ecore_spq_completion(struct ecore_hwfn	*p_hwfn,
					  __le16		echo,
					  u8                    fw_return_code,
					  union event_ring_data	*p_data);

/**
 * @brief ecore_spq_get_cid - Given p_hwfn, return cid for the hwfn's SPQ
 *
 * @param p_hwfn
 *
 * @return u32 - SPQ CID
 */
u32 ecore_spq_get_cid(struct ecore_hwfn *p_hwfn);

/**
 * @brief ecore_consq_alloc - Allocates & initializes an ConsQ
 *        struct
 *
 * @param p_hwfn
 *
 * @return struct ecore_eq* - a newly allocated structure; NULL upon error.
 */
struct ecore_consq *ecore_consq_alloc(struct ecore_hwfn	*p_hwfn);

/**
 * @brief ecore_consq_setup - Reset the ConsQ to its start
 *        state.
 *
 * @param p_hwfn
 * @param p_eq
 */
void ecore_consq_setup(struct ecore_hwfn *p_hwfn,
		    struct ecore_consq   *p_consq);

/**
 * @brief ecore_consq_free - deallocates the given ConsQ struct.
 *
 * @param p_hwfn
 * @param p_eq
 */
void ecore_consq_free(struct ecore_hwfn *p_hwfn,
		   struct ecore_consq   *p_consq);

#endif /* __ECORE_SPQ_H__ */
