/*-
 * BSD LICENSE
 *
 * Copyright (c) 2015-2017 Atomic Rules LLC
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in
 * the documentation and/or other materials provided with the
 * distribution.
 * * Neither the name of copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _ARK_PKTDIR_H_
#define _ARK_PKTDIR_H_

#include <stdint.h>

#define ARK_PKTDIR_BASE_ADR  0xa0000

typedef void *ark_pkt_dir_t;


/* The packet director is an internal Arkville hardware module for
 * directing packet data in non-typical flows, such as testing.
 * This module is *not* intended for end-user manipulation, hence
 * there is minimal documentation.
 */

/*
 * This is an overlay structures to a memory mapped FPGA device.  These
 * structs will never be instantiated in ram memory
 */
struct ark_pkt_dir_regs {
	uint32_t ctrl;
	uint32_t status;
	uint32_t stall_cnt;
} __attribute__ ((packed));

struct ark_pkt_dir_inst {
	volatile struct ark_pkt_dir_regs *regs;
};

ark_pkt_dir_t ark_pktdir_init(void *base);
void ark_pktdir_uninit(ark_pkt_dir_t handle);
void ark_pktdir_setup(ark_pkt_dir_t handle, uint32_t v);
uint32_t ark_pktdir_stall_cnt(ark_pkt_dir_t handle);
uint32_t ark_pktdir_status(ark_pkt_dir_t handle);

#endif
