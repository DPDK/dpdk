/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef NTOSS_SYSTEM_NT_UTIL_H
#define NTOSS_SYSTEM_NT_UTIL_H

#include <stdint.h>
#include "nt4ga_link.h"

/* Total max VDPA ports */
#define MAX_VDPA_PORTS 128UL

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) RTE_DIM(arr)
#endif

/*
 * Windows size in seconds for measuring FLM load
 * and Port load.
 * The windows size must max be 3 min in order to
 * prevent overflow.
 */
#define PORT_LOAD_WINDOWS_SIZE 2ULL
#define FLM_LOAD_WINDOWS_SIZE 2ULL

#define PCIIDENT_TO_DOMAIN(pci_ident) ((uint16_t)(((unsigned int)(pci_ident) >> 16) & 0xFFFFU))
#define PCIIDENT_TO_BUSNR(pci_ident) ((uint8_t)(((unsigned int)(pci_ident) >> 8) & 0xFFU))
#define PCIIDENT_TO_DEVNR(pci_ident) ((uint8_t)(((unsigned int)(pci_ident) >> 3) & 0x1FU))
#define PCIIDENT_TO_FUNCNR(pci_ident) ((uint8_t)(((unsigned int)(pci_ident) >> 0) & 0x7U))
#define PCIIDENT_PRINT_STR "%04x:%02x:%02x.%x"
#define BDF_TO_PCIIDENT(dom, bus, dev, fnc) (((dom) << 16) | ((bus) << 8) | ((dev) << 3) | (fnc))

uint64_t nt_os_get_time_monotonic_counter(void);
void nt_os_wait_usec(int val);
void nt_os_wait_usec_poll(int val);

static inline int min(int a, int b)
{
	return (a < b) ? a : b;
}

uint64_t nt_util_align_size(uint64_t size);

struct nt_dma_s {
	uint64_t iova;
	uint64_t addr;
	uint64_t size;
};

struct port_link_speed {
	int port_id;
	int link_speed;
};

struct nt_dma_s *nt_dma_alloc(uint64_t size, uint64_t align, int numa);
struct nt_util_vfio_impl {
	int (*vfio_dma_map)(int vf_num, void *virt_addr, uint64_t *iova_addr, uint64_t size);
	int (*vfio_dma_unmap)(int vf_num, void *virt_addr, uint64_t iova_addr, uint64_t size);
};

void nt_util_vfio_init(struct nt_util_vfio_impl *impl);

int nt_link_speed_to_eth_speed_num(enum nt_link_speed_e nt_link_speed);
uint32_t nt_link_speed_capa_to_eth_speed_capa(int nt_link_speed_capa);
int nt_link_duplex_to_eth_duplex(enum nt_link_duplex_e nt_link_duplex);

int nthw_string_to_u32(const char *key_str __rte_unused, const char *value_str, void *extra_args);

#endif	/* NTOSS_SYSTEM_NT_UTIL_H */
