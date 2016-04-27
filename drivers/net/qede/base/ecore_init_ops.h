/*
 * Copyright (c) 2016 QLogic Corporation.
 * All rights reserved.
 * www.qlogic.com
 *
 * See LICENSE.qede_pmd for copyright and licensing details.
 */

#ifndef __ECORE_INIT_OPS__
#define __ECORE_INIT_OPS__

#include "ecore.h"

/**
 * @brief ecore_init_iro_array - init iro_arr.
 *
 *
 * @param p_dev
 */
void ecore_init_iro_array(struct ecore_dev *p_dev);

/**
 * @brief ecore_init_run - Run the init-sequence.
 *
 *
 * @param p_hwfn
 * @param p_ptt
 * @param phase
 * @param phase_id
 * @param modes
 * @return _ecore_status_t
 */
enum _ecore_status_t ecore_init_run(struct ecore_hwfn *p_hwfn,
				    struct ecore_ptt *p_ptt,
				    int phase, int phase_id, int modes);

/**
 * @brief ecore_init_hwfn_allocate - Allocate RT array, Store 'values' ptrs.
 *
 *
 * @param p_hwfn
 *
 * @return _ecore_status_t
 */
enum _ecore_status_t ecore_init_alloc(struct ecore_hwfn *p_hwfn);

/**
 * @brief ecore_init_hwfn_deallocate
 *
 *
 * @param p_hwfn
 */
void ecore_init_free(struct ecore_hwfn *p_hwfn);

/**
 * @brief ecore_init_clear_rt_data - Clears the runtime init array.
 *
 *
 * @param p_hwfn
 */
void ecore_init_clear_rt_data(struct ecore_hwfn *p_hwfn);

/**
 * @brief ecore_init_store_rt_reg - Store a configuration value in the RT array.
 *
 *
 * @param p_hwfn
 * @param rt_offset
 * @param val
 */
void ecore_init_store_rt_reg(struct ecore_hwfn *p_hwfn, u32 rt_offset, u32 val);

#define STORE_RT_REG(hwfn, offset, val)				\
	ecore_init_store_rt_reg(hwfn, offset, val)

#define OVERWRITE_RT_REG(hwfn, offset, val)			\
	ecore_init_store_rt_reg(hwfn, offset, val)

/**
* @brief
*
*
* @param p_hwfn
* @param rt_offset
* @param val
* @param size
*/

void ecore_init_store_rt_agg(struct ecore_hwfn *p_hwfn,
			     u32 rt_offset, u32 *val, osal_size_t size);

#define STORE_RT_REG_AGG(hwfn, offset, val)			\
	ecore_init_store_rt_agg(hwfn, offset, (u32 *)&val, sizeof(val))

/**
 * @brief
 *      Initialize GTT global windows and set admin window
 *      related params of GTT/PTT to default values.
 *
 * @param p_hwfn
 */
void ecore_gtt_init(struct ecore_hwfn *p_hwfn);
#endif /* __ECORE_INIT_OPS__ */
