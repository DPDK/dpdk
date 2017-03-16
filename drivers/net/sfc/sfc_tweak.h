/*-
 *   BSD LICENSE
 *
 * Copyright (c) 2016-2017 Solarflare Communications Inc.
 * All rights reserved.
 *
 * This software was jointly developed between OKTET Labs (under contract
 * for Solarflare) and Solarflare Communications, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _SFC_TWEAK_H_
#define _SFC_TWEAK_H_

/*
 * The header is intended to collect defines/constants which could be
 * tweaked to improve the PMD performance characteristics depending on
 * the usecase or requirements (CPU load, packet rate, latency).
 */

/**
 * Number of Rx descriptors in the bulk submitted on Rx ring refill.
 */
#define SFC_RX_REFILL_BULK	(RTE_CACHE_LINE_SIZE / sizeof(efx_qword_t))

/**
 * Make the transmit path reap at least one time per a burst;
 * this improves cache locality because the same mbufs may be used to send
 * subsequent bursts in certain cases because of well-timed reap
 */
#define SFC_TX_XMIT_PKTS_REAP_AT_LEAST_ONCE	0

/** Default free threshold follows recommendations from DPDK documentation */
#define SFC_TX_DEFAULT_FREE_THRESH	32

#endif /* _SFC_TWEAK_H_ */
