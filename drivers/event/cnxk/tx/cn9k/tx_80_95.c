/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2022 Marvell.
 */

#include "cn9k_worker.h"

#define T(name, sz, flags) SSO_TX(cn9k_sso_hws_tx_adptr_enq_##name, sz, flags)

NIX_TX_FASTPATH_MODES_80_95
#undef T
