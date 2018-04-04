/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 CESNET
 */

#include <stdint.h>

#include <rte_common.h>

#include "szedata2_iobuf.h"

/*
 * IBUFs and OBUFs can generally be located at different offsets in different
 * firmwares (modes).
 * This part defines base offsets of IBUFs and OBUFs for various cards
 * and firmwares (modes).
 * Type of firmware (mode) is set through configuration option
 * CONFIG_RTE_LIBRTE_PMD_SZEDATA2_AS.
 * Possible values are:
 * 0 - for cards (modes):
 *     NFB-100G1 (100G1)
 *
 * 1 - for cards (modes):
 *     NFB-100G2Q (100G1)
 *
 * 2 - for cards (modes):
 *     NFB-40G2 (40G2)
 *     NFB-100G2C (100G2)
 *     NFB-100G2Q (40G2)
 *
 * 3 - for cards (modes):
 *     NFB-40G2 (10G8)
 *     NFB-100G2Q (10G8)
 *
 * 4 - for cards (modes):
 *     NFB-100G1 (10G10)
 *
 * 5 - for experimental firmwares and future use
 */
#if !defined(RTE_LIBRTE_PMD_SZEDATA2_AS)
#error "RTE_LIBRTE_PMD_SZEDATA2_AS has to be defined"
#elif RTE_LIBRTE_PMD_SZEDATA2_AS == 0

/*
 * Cards (modes):
 *     NFB-100G1 (100G1)
 */

const uint32_t szedata2_ibuf_base_table[] = {
	0x8000
};
const uint32_t szedata2_obuf_base_table[] = {
	0x9000
};

#elif RTE_LIBRTE_PMD_SZEDATA2_AS == 1

/*
 * Cards (modes):
 *     NFB-100G2Q (100G1)
 */

const uint32_t szedata2_ibuf_base_table[] = {
	0x8800
};
const uint32_t szedata2_obuf_base_table[] = {
	0x9800
};

#elif RTE_LIBRTE_PMD_SZEDATA2_AS == 2

/*
 * Cards (modes):
 *     NFB-40G2 (40G2)
 *     NFB-100G2C (100G2)
 *     NFB-100G2Q (40G2)
 */

const uint32_t szedata2_ibuf_base_table[] = {
	0x8000,
	0x8800
};
const uint32_t szedata2_obuf_base_table[] = {
	0x9000,
	0x9800
};

#elif RTE_LIBRTE_PMD_SZEDATA2_AS == 3

/*
 * Cards (modes):
 *     NFB-40G2 (10G8)
 *     NFB-100G2Q (10G8)
 */

const uint32_t szedata2_ibuf_base_table[] = {
	0x8000,
	0x8200,
	0x8400,
	0x8600,
	0x8800,
	0x8A00,
	0x8C00,
	0x8E00
};
const uint32_t szedata2_obuf_base_table[] = {
	0x9000,
	0x9200,
	0x9400,
	0x9600,
	0x9800,
	0x9A00,
	0x9C00,
	0x9E00
};

#elif RTE_LIBRTE_PMD_SZEDATA2_AS == 4

/*
 * Cards (modes):
 *     NFB-100G1 (10G10)
 */

const uint32_t szedata2_ibuf_base_table[] = {
	0x8000,
	0x8200,
	0x8400,
	0x8600,
	0x8800,
	0x8A00,
	0x8C00,
	0x8E00,
	0x9000,
	0x9200
};
const uint32_t szedata2_obuf_base_table[] = {
	0xA000,
	0xA200,
	0xA400,
	0xA600,
	0xA800,
	0xAA00,
	0xAC00,
	0xAE00,
	0xB000,
	0xB200
};

#elif RTE_LIBRTE_PMD_SZEDATA2_AS == 5

/*
 * Future use and experimental firmwares.
 */

const uint32_t szedata2_ibuf_base_table[] = {
	0x8000,
	0x8200,
	0x8400,
	0x8600,
	0x8800
};
const uint32_t szedata2_obuf_base_table[] = {
	0x9000,
	0x9200,
	0x9400,
	0x9600,
	0x9800
};

#else
#error "RTE_LIBRTE_PMD_SZEDATA2_AS has wrong value, see comments in config file"
#endif

const uint32_t szedata2_ibuf_count = RTE_DIM(szedata2_ibuf_base_table);
const uint32_t szedata2_obuf_count = RTE_DIM(szedata2_obuf_base_table);
