/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>

#include <rte_ethdev.h>
#include <rte_malloc.h>

#include "ntlog.h"
#include "nt_util.h"

static struct nt_util_vfio_impl vfio_cb;

/* uses usleep which schedules out the calling service */
void nthw_os_wait_usec(int val)
{
	rte_delay_us_sleep(val);
}

/* spins in a waiting loop calling pause asm instruction uses RDTSC - precise wait */
void nthw_os_wait_usec_poll(int val)
{
	rte_delay_us(val);
}

uint64_t nthw_os_get_time_monotonic_counter(void)
{
	return rte_get_timer_cycles();
}

/* Allocation size matching minimum alignment of specified size */
uint64_t nthw_util_align_size(uint64_t size)
{
	uint64_t alignment_size = 1ULL << rte_log2_u64(size);
	return alignment_size;
}

void nthw_util_vfio_init(struct nt_util_vfio_impl *impl)
{
	vfio_cb = *impl;
}

struct nt_dma_s *nthw_dma_alloc(uint64_t size, uint64_t align, int numa)
{
	int res;
	struct nt_dma_s *vfio_addr;

	vfio_addr = rte_malloc(NULL, sizeof(struct nt_dma_s), 0);

	if (!vfio_addr) {
		NT_LOG(ERR, GENERAL, "VFIO rte_malloc failed");
		return NULL;
	}

	void *addr = rte_malloc_socket(NULL, size, align, numa);

	if (!addr) {
		rte_free(vfio_addr);
		NT_LOG(ERR, GENERAL, "VFIO rte_malloc_socket failed");
		return NULL;
	}

	res = vfio_cb.vfio_dma_map(0, addr, &vfio_addr->iova, nthw_util_align_size(size));

	if (res != 0) {
		rte_free(addr);
		rte_free(vfio_addr);
		NT_LOG(ERR, GENERAL, "VFIO nt_dma_map failed");
		return NULL;
	}

	vfio_addr->addr = (uint64_t)addr;
	vfio_addr->size = nthw_util_align_size(size);

	NT_LOG(DBG, GENERAL,
		"VFIO DMA alloc addr=%" PRIX64 ", iova=%" PRIX64
		", size=%" PRIX64 "align=0x%" PRIX64,
		vfio_addr->addr, vfio_addr->iova, vfio_addr->size, align);

	return vfio_addr;
}

/* NOTE: please note the difference between RTE_ETH_SPEED_NUM_xxx and RTE_ETH_LINK_SPEED_xxx */
int nthw_link_speed_to_eth_speed_num(enum nt_link_speed_e nt_link_speed)
{
	int eth_speed_num = RTE_ETH_SPEED_NUM_NONE;

	switch (nt_link_speed) {
	case NT_LINK_SPEED_10M:
		eth_speed_num = RTE_ETH_SPEED_NUM_10M;
		break;

	case NT_LINK_SPEED_100M:
		eth_speed_num = RTE_ETH_SPEED_NUM_100M;
		break;

	case NT_LINK_SPEED_1G:
		eth_speed_num = RTE_ETH_SPEED_NUM_1G;
		break;

	case NT_LINK_SPEED_10G:
		eth_speed_num = RTE_ETH_SPEED_NUM_10G;
		break;

	case NT_LINK_SPEED_25G:
		eth_speed_num = RTE_ETH_SPEED_NUM_25G;
		break;

	case NT_LINK_SPEED_40G:
		eth_speed_num = RTE_ETH_SPEED_NUM_40G;
		break;

	case NT_LINK_SPEED_50G:
		eth_speed_num = RTE_ETH_SPEED_NUM_50G;
		break;

	case NT_LINK_SPEED_100G:
		eth_speed_num = RTE_ETH_SPEED_NUM_100G;
		break;

	default:
		eth_speed_num = RTE_ETH_SPEED_NUM_NONE;
		break;
	}

	return eth_speed_num;
}

uint32_t nthw_link_speed_capa_to_eth_speed_capa(int nt_link_speed_capa)
{
	uint32_t eth_speed_capa = 0;

	if (nt_link_speed_capa & NT_LINK_SPEED_10M)
		eth_speed_capa |= RTE_ETH_LINK_SPEED_10M;

	if (nt_link_speed_capa & NT_LINK_SPEED_100M)
		eth_speed_capa |= RTE_ETH_LINK_SPEED_100M;

	if (nt_link_speed_capa & NT_LINK_SPEED_1G)
		eth_speed_capa |= RTE_ETH_LINK_SPEED_1G;

	if (nt_link_speed_capa & NT_LINK_SPEED_10G)
		eth_speed_capa |= RTE_ETH_LINK_SPEED_10G;

	if (nt_link_speed_capa & NT_LINK_SPEED_25G)
		eth_speed_capa |= RTE_ETH_LINK_SPEED_25G;

	if (nt_link_speed_capa & NT_LINK_SPEED_40G)
		eth_speed_capa |= RTE_ETH_LINK_SPEED_40G;

	if (nt_link_speed_capa & NT_LINK_SPEED_50G)
		eth_speed_capa |= RTE_ETH_LINK_SPEED_50G;

	if (nt_link_speed_capa & NT_LINK_SPEED_100G)
		eth_speed_capa |= RTE_ETH_LINK_SPEED_100G;

	return eth_speed_capa;
}

int nthw_link_duplex_to_eth_duplex(enum nt_link_duplex_e nt_link_duplex)
{
	int eth_link_duplex = 0;

	switch (nt_link_duplex) {
	case NT_LINK_DUPLEX_FULL:
		eth_link_duplex = RTE_ETH_LINK_FULL_DUPLEX;
		break;

	case NT_LINK_DUPLEX_HALF:
		eth_link_duplex = RTE_ETH_LINK_HALF_DUPLEX;
		break;

	case NT_LINK_DUPLEX_UNKNOWN:	/* fall-through */
	default:
		break;
	}

	return eth_link_duplex;
}

int nthw_string_to_u32(const char *key_str __rte_unused, const char *value_str, void *extra_args)
{
	if (!value_str || !extra_args)
		return -1;

	const uint32_t value = strtol(value_str, NULL, 0);
	*(uint32_t *)extra_args = value;
	return 0;
}
