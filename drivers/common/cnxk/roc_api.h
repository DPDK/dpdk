/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef _ROC_API_H_
#define _ROC_API_H_

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/* Alignment */
#define ROC_ALIGN 128

/* Bits manipulation */
#include "roc_bits.h"

/* Bitfields manipulation */
#include "roc_bitfield.h"

/* Constants */
#define PLT_ETHER_ADDR_LEN 6

/* Platform definition */
#include "roc_platform.h"

#define ROC_LMT_LINE_SZ		    128
#define ROC_NUM_LMT_LINES	    2048
#define ROC_LMT_LINES_PER_CORE_LOG2 5
#define ROC_LMT_LINE_SIZE_LOG2	    7
#define ROC_LMT_BASE_PER_CORE_LOG2                                             \
	(ROC_LMT_LINES_PER_CORE_LOG2 + ROC_LMT_LINE_SIZE_LOG2)

/* IO */
#if defined(__aarch64__)
#include "roc_io.h"
#else
#include "roc_io_generic.h"
#endif

/* PCI IDs */
#define PCI_VENDOR_ID_CAVIUM	      0x177D
#define PCI_DEVID_CNXK_RVU_PF	      0xA063
#define PCI_DEVID_CNXK_RVU_VF	      0xA064
#define PCI_DEVID_CNXK_RVU_AF	      0xA065
#define PCI_DEVID_CNXK_RVU_SSO_TIM_PF 0xA0F9
#define PCI_DEVID_CNXK_RVU_SSO_TIM_VF 0xA0FA
#define PCI_DEVID_CNXK_RVU_NPA_PF     0xA0FB
#define PCI_DEVID_CNXK_RVU_NPA_VF     0xA0FC
#define PCI_DEVID_CNXK_RVU_AF_VF      0xA0f8
#define PCI_DEVID_CNXK_DPI_VF	      0xA081
#define PCI_DEVID_CNXK_EP_VF	      0xB203
#define PCI_DEVID_CNXK_RVU_SDP_PF     0xA0f6
#define PCI_DEVID_CNXK_RVU_SDP_VF     0xA0f7
#define PCI_DEVID_CNXK_BPHY	      0xA089

#define PCI_DEVID_CN9K_CGX  0xA059
#define PCI_DEVID_CN10K_RPM 0xA060

#define PCI_DEVID_CN9K_RVU_CPT_PF  0xA0FD
#define PCI_DEVID_CN9K_RVU_CPT_VF  0xA0FE
#define PCI_DEVID_CN10K_RVU_CPT_PF 0xA0F2
#define PCI_DEVID_CN10K_RVU_CPT_VF 0xA0F3

#define PCI_SUBSYSTEM_DEVID_CN10KA  0xB900
#define PCI_SUBSYSTEM_DEVID_CN10KAS 0xB900

#define PCI_SUBSYSTEM_DEVID_CN9KA 0x0000
#define PCI_SUBSYSTEM_DEVID_CN9KB 0xb400
#define PCI_SUBSYSTEM_DEVID_CN9KC 0x0200
#define PCI_SUBSYSTEM_DEVID_CN9KD 0xB200
#define PCI_SUBSYSTEM_DEVID_CN9KE 0xB100

/* HW structure definition */
#include "hw/cpt.h"
#include "hw/nix.h"
#include "hw/npa.h"
#include "hw/npc.h"
#include "hw/rvu.h"
#include "hw/sdp.h"
#include "hw/sso.h"
#include "hw/ssow.h"
#include "hw/tim.h"

/* Model */
#include "roc_model.h"

/* Mbox */
#include "roc_mbox.h"

/* NPA */
#include "roc_npa.h"

/* NPC */
#include "roc_npc.h"

/* NIX */
#include "roc_nix.h"

/* SSO */
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
#include "roc_ie.h"
#include "roc_ie_on.h"
#include "roc_ie_ot.h"
#include "roc_se.h"

/* HASH computation */
#include "roc_hash.h"

#endif /* _ROC_API_H_ */
