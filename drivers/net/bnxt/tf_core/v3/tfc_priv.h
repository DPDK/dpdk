/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Broadcom
 * All rights reserved.
 */

#ifndef _TFC_PRIV_H_
#define _TFC_PRIV_H_

#include <stdint.h>
#include "tfc.h"

/**
 * @page Common Utility functions
 *
 * @ref tfc_get_fid
 *
 * @ref tfc_get_pfid
 *
 * @ref tfc_bp_is_pf
 *
 * @ref tfc_bp_vf_max
 */
/**
 * Get the FID for this DPDK port/function.
 *
 * @param[in] tfcp
 *   Pointer to TFC handle
 *
 * @param[out] fw_fid
 *   The function ID
 *
 * @return
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_get_fid(struct tfc *tfcp, uint16_t *fw_fid);

/**
 * Get the PFID for this DPDK port/function.
 *
 * @param[in] tfcp
 *   Pointer to TFC handle
 *
 * @param[out] pfid
 *   The Physical Function ID for this port/function
 *
 * @return
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_get_pfid(struct tfc *tfcp, uint16_t *pfid);

/**
 * Is this DPDK port/function a PF?
 *
 * @param[in] tfcp
 *   Pointer to TFC handle
 *
 * @param[out] is_pf
 *   If true, the DPDK port is a PF (as opposed to a VF)
 *
 * @return
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_bp_is_pf(struct tfc *tfcp, bool *is_pf);

/**
 * Get the maximum VF for the PF
 *
 * @param[in] tfcp
 *   Pointer to TFC handle
 *
 * @param[out] max_vf
 *   The maximum VF for the PF (only valid on a PF)
 *
 * @return
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_bp_vf_max(struct tfc *tfcp, uint16_t *max_vf);
#endif  /* _TFC_PRIV_H_ */
