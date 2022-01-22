/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2022 Marvell.
 */

#include "cn9k_worker.h"
#include "cnxk_eventdev.h"
#include "cnxk_worker.h"

#define R(name, flags) SSO_DEQ_CA_SEG(cn9k_sso_hws_deq_ca_seg_##name, flags)

NIX_RX_FASTPATH_MODES_112_127
#undef R
