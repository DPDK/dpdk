/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef _ROC_API_H_
#define _ROC_API_H_

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/* Bits manipulation */
#include "roc_bits.h"

/* Bit fields manipulation */
#include "roc_bitfield.h"

/* ROC Constants */
#include "roc_constants.h"

/* Platform definition */
#include "roc_platform.h"

/* IO */
#if defined(__aarch64__)
#include "roc_io.h"
#else
#include "roc_io_generic.h"
#endif

/* HW structure definition */
#include "hw/cpt.h"
#include "hw/dpi.h"
#include "hw/ml.h"
#include "hw/nix.h"
#include "hw/npa.h"
#include "hw/npc.h"
#include "hw/ree.h"
#include "hw/rvu.h"
#include "hw/sdp.h"
#include "hw/sso.h"
#include "hw/ssow.h"
#include "hw/tim.h"

/* Model */
#include "roc_model.h"

/* HW Errata */
#include "roc_errata.h"

/* HW Features */
#include "roc_features.h"

/* Mbox */
#include "roc_mbox.h"

/* NPA */
#include "roc_npa_dp.h"
#include "roc_npa.h"

/* NPC */
#include "roc_npc.h"

/* NIX */
#include "roc_nix.h"

/* SSO */
#include "roc_sso_dp.h"
#include "roc_sso.h"

/* TIM */
#include "roc_tim.h"

/* Utils */
#include "roc_utils.h"

/* Idev */
#include "roc_idev.h"

/* Baseband phy cgx */
#include "roc_bphy_cgx.h"

/* Baseband phy */
#include "roc_bphy.h"

/* CPT */
#include "roc_cpt.h"

/* CPT microcode */
#include "roc_ae.h"
#include "roc_ae_fpm_tables.h"
#include "roc_cpt_sg.h"
#include "roc_ie.h"
#include "roc_ie_on.h"
#include "roc_ie_ot.h"
#include "roc_se.h"

/* DPI */
#include "roc_dpi.h"

/* REE */
#include "roc_ree.h"

/* AES */
#include "roc_aes.h"

/* HASH computation */
#include "roc_hash.h"

/* NIX Inline dev */
#include "roc_nix_inl_dp.h"
#include "roc_nix_inl.h"

/* ML */
#include "roc_ml.h"

#endif /* _ROC_API_H_ */
