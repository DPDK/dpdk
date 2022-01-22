/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2022 Marvell.
 */

#include "cn9k_ethdev.h"
#include "cn9k_rx.h"

#define R(name, flags) NIX_RX_RECV_VEC(cn9k_nix_recv_pkts_vec_##name, flags)

NIX_RX_FASTPATH_MODES_80_95
#undef R
