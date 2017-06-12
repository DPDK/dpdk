/*-
 *   BSD LICENSE
 *
 *   Copyright (c) 2017 CESNET
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of CESNET nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdint.h>

#include <rte_common.h>

#include "szedata2_iobuf.h"

/*
 * IBUFs and OBUFs can generally be located at different offsets in different
 * firmwares.
 * This part defines base offsets of IBUFs and OBUFs through various firmwares.
 * Currently one firmware type is supported.
 * Type of firmware is set through configuration option
 * CONFIG_RTE_LIBRTE_PMD_SZEDATA2_AS.
 * Possible values are:
 * 0 - for firmwares:
 *     NIC_100G1_LR4
 *     HANIC_100G1_LR4
 *     HANIC_100G1_SR10
 */
#if !defined(RTE_LIBRTE_PMD_SZEDATA2_AS)
#error "RTE_LIBRTE_PMD_SZEDATA2_AS has to be defined"
#elif RTE_LIBRTE_PMD_SZEDATA2_AS == 0

const uint32_t szedata2_ibuf_base_table[] = {
	0x8000
};
const uint32_t szedata2_obuf_base_table[] = {
	0x9000
};

#else
#error "RTE_LIBRTE_PMD_SZEDATA2_AS has wrong value, see comments in config file"
#endif

const uint32_t szedata2_ibuf_count = RTE_DIM(szedata2_ibuf_base_table);
const uint32_t szedata2_obuf_count = RTE_DIM(szedata2_obuf_base_table);
