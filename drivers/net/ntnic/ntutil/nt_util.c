/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <assert.h>

#include <rte_ethdev.h>
#include <rte_malloc.h>

#include "ntlog.h"
#include "nt_util.h"

static struct nt_util_vfio_impl vfio_cb;

/* uses usleep which schedules out the calling thread */
void nt_os_wait_usec(int val)
{
	rte_delay_us_sleep(val);
}

uint64_t nt_os_get_time_monotonic_counter(void)
{
	return rte_get_timer_cycles();
}

/* Allocation size matching minimum alignment of specified size */
uint64_t nt_util_align_size(uint64_t size)
{
	return 1 << rte_log2_u64(size);
}

void nt_util_vfio_init(struct nt_util_vfio_impl *impl)
{
	vfio_cb = *impl;
}

struct nt_dma_s *nt_dma_alloc(uint64_t size, uint64_t align, int numa)
{
	int res;
	struct nt_dma_s *vfio_addr;

	vfio_addr = rte_malloc(NULL, sizeof(struct nt_dma_s), 0);

	if (!vfio_addr) {
		NT_LOG(ERR, GENERAL, "VFIO rte_malloc failed\n");
		return NULL;
	}

	void *addr = rte_malloc_socket(NULL, size, align, numa);

	if (!addr) {
		rte_free(vfio_addr);
		NT_LOG(ERR, GENERAL, "VFIO rte_malloc_socket failed\n");
		return NULL;
	}

	res = vfio_cb.vfio_dma_map(0, addr, &vfio_addr->iova, nt_util_align_size(size));

	if (res != 0) {
		rte_free(addr);
		rte_free(vfio_addr);
		NT_LOG(ERR, GENERAL, "VFIO nt_dma_map failed\n");
		return NULL;
	}

	vfio_addr->addr = (uint64_t)addr;
	vfio_addr->size = nt_util_align_size(size);

	NT_LOG(DBG, GENERAL,
		"VFIO DMA alloc addr=%" PRIX64 ", iova=%" PRIX64
		", size=%" PRIX64 "align=0x%" PRIX64 "\n",
		vfio_addr->addr, vfio_addr->iova, vfio_addr->size, align);

	return vfio_addr;
}

void nt_dma_free(struct nt_dma_s *vfio_addr)
{
	NT_LOG(DBG, GENERAL, "VFIO DMA free addr=%" PRIX64 ", iova=%" PRIX64 ", size=%" PRIX64 "\n",
		vfio_addr->addr, vfio_addr->iova, vfio_addr->size);

	int res = vfio_cb.vfio_dma_unmap(0, (void *)vfio_addr->addr, vfio_addr->iova,
			vfio_addr->size);

	if (res != 0) {
		NT_LOG(WRN, GENERAL,
			"VFIO DMA free FAILED addr=%" PRIX64 ", iova=%" PRIX64 ", size=%" PRIX64 "\n",
			vfio_addr->addr, vfio_addr->iova, vfio_addr->size);
	}

	rte_free((void *)(vfio_addr->addr));
	rte_free(vfio_addr);
}

/* NOTE: please note the difference between RTE_ETH_SPEED_NUM_xxx and RTE_ETH_LINK_SPEED_xxx */
int nt_link_speed_to_eth_speed_num(enum nt_link_speed_e nt_link_speed)
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

uint32_t nt_link_speed_capa_to_eth_speed_capa(int nt_link_speed_capa)
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

/* Converts link speed provided in Mbps to NT specific definitions.*/
nt_link_speed_t convert_link_speed(int link_speed_mbps)
{
	switch (link_speed_mbps) {
	case 10:
		return NT_LINK_SPEED_10M;

	case 100:
		return NT_LINK_SPEED_100M;

	case 1000:
		return NT_LINK_SPEED_1G;

	case 10000:
		return NT_LINK_SPEED_10G;

	case 40000:
		return NT_LINK_SPEED_40G;

	case 100000:
		return NT_LINK_SPEED_100G;

	case 50000:
		return NT_LINK_SPEED_50G;

	case 25000:
		return NT_LINK_SPEED_25G;

	default:
		return NT_LINK_SPEED_UNKNOWN;
	}
}

int nt_link_duplex_to_eth_duplex(enum nt_link_duplex_e nt_link_duplex)
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
