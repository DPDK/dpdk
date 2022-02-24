/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2022 Marvell.
 */

#include "cn10k_ethdev.h"
#include "cn10k_rx.h"

#define R(name, flags)                                                         \
	NIX_RX_RECV_VEC(cn10k_nix_recv_pkts_vec_##name, flags)                 \
	NIX_RX_RECV_VEC(cn10k_nix_recv_pkts_reas_vec_##name, flags | NIX_RX_REAS_F)

NIX_RX_FASTPATH_MODES_96_111
#undef R
