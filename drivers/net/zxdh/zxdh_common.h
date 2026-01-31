/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 ZTE Corporation
 */

#ifndef ZXDH_COMMON_H
#define ZXDH_COMMON_H

#include <stdint.h>
#include <rte_ethdev.h>

#include "zxdh_ethdev.h"
#include "zxdh_logs.h"

#define ZXDH_VF_LOCK_REG               0x90
#define ZXDH_VF_LOCK_ENABLE_MASK       0x1
#define ZXDH_ACQUIRE_CHANNEL_NUM_MAX   10
#define VF_IDX(pcie_id)     ((pcie_id) & 0xff)

struct zxdh_res_para {
	uint64_t virt_addr;
	uint16_t pcie_id;
	uint16_t src_type; /* refer to BAR_DRIVER_TYPE */
};

static inline size_t
zxdh_get_value(uint32_t fld_sz, uint8_t *addr) {
	size_t result = 0;
	switch (fld_sz) {
	case 1:
		result = *((uint8_t *)addr);
		break;
	case 2:
		result = *((uint16_t *)addr);
		break;
	case 4:
		result = *((uint32_t *)addr);
		break;
	case 8:
		result = *((uint64_t *)addr);
		break;
	default:
		PMD_MSG_LOG(ERR, "unreachable field size %u", fld_sz);
		break;
	}
	return result;
}

static inline void
zxdh_set_value(uint32_t fld_sz, uint8_t *addr, size_t value) {
	switch (fld_sz) {
	case 1:
		*(uint8_t *)addr = (uint8_t)value;
		break;
	case 2:
		*(uint16_t *)addr = (uint16_t)value;
		break;
	case 4:
		*(uint32_t *)addr = (uint32_t)value;
		break;
	case 8:
		*(uint64_t *)addr = (uint64_t)value;
		break;
	default:
		PMD_MSG_LOG(ERR, "unreachable field size %u", fld_sz);
		break;
	}
}

#define __zxdh_nullp(typ) ((struct zxdh_ifc_##typ##_bits *)0)
#define __zxdh_bit_sz(typ, fld) sizeof(__zxdh_nullp(typ)->fld)
#define __zxdh_bit_off(typ, fld) ((unsigned int)(uintptr_t) \
				  (&(__zxdh_nullp(typ)->fld)))
#define __zxdh_dw_bit_off(typ, fld) (32 - __zxdh_bit_sz(typ, fld) - \
				    (__zxdh_bit_off(typ, fld) & 0x1f))
#define __zxdh_dw_off(typ, fld) (__zxdh_bit_off(typ, fld) / 32)
#define __zxdh_64_off(typ, fld) (__zxdh_bit_off(typ, fld) / 64)
#define __zxdh_dw_mask(typ, fld) (__zxdh_mask(typ, fld) << \
				  __zxdh_dw_bit_off(typ, fld))
#define __zxdh_mask(typ, fld) ((uint32_t)((1ull << __zxdh_bit_sz(typ, fld)) - 1))
#define __zxdh_16_off(typ, fld) (__zxdh_bit_off(typ, fld) / 16)
#define __zxdh_16_bit_off(typ, fld) (16 - __zxdh_bit_sz(typ, fld) - \
				    (__zxdh_bit_off(typ, fld) & 0xf))
#define __zxdh_mask16(typ, fld) ((uint16_t)((1ull << __zxdh_bit_sz(typ, fld)) - 1))
#define __zxdh_16_mask(typ, fld) (__zxdh_mask16(typ, fld) << \
				  __zxdh_16_bit_off(typ, fld))
#define ZXDH_ST_SZ_BYTES(typ) (sizeof(struct zxdh_ifc_##typ##_bits) / 8)
#define ZXDH_ST_SZ_DW(typ) (sizeof(struct zxdh_ifc_##typ##_bits) / 32)
#define ZXDH_BYTE_OFF(typ, fld) (__zxdh_bit_off(typ, fld) / 8)
#define ZXDH_ADDR_OF(typ, p, fld) ((uint8_t *)(p) + ZXDH_BYTE_OFF(typ, fld))

#define ZXDH_SET(typ, p, fld, v) do { \
	RTE_BUILD_BUG_ON(__zxdh_bit_sz(typ, fld) % 8); \
	uint32_t fld_sz = __zxdh_bit_sz(typ, fld) / 8; \
	uint8_t *addr = ZXDH_ADDR_OF(typ, p, fld); \
	zxdh_set_value(fld_sz, addr, v); \
} while (0)

#define ZXDH_GET(typ, p, fld) ({ \
	RTE_BUILD_BUG_ON(__zxdh_bit_sz(typ, fld) % 8); \
	uint32_t fld_sz = __zxdh_bit_sz(typ, fld) / 8; \
	uint8_t *addr = ZXDH_ADDR_OF(typ, p, fld); \
	zxdh_get_value(fld_sz, addr); \
})

#define ZXDH_SET_ARRAY(typ, p, fld, index, value, type) \
	do { \
		type *addr = (type *)((uint8_t *)ZXDH_ADDR_OF(typ, p, fld) + \
		(index) * sizeof(type)); \
		*addr = (type)(value); \
	} while (0)

#define ZXDH_GET_ARRAY(typ, p, fld, index, type) \
	({ \
		type *addr = (type *)((uint8_t *)ZXDH_ADDR_OF(typ, p, fld) + \
		(index) * sizeof(type)); \
		*addr; \
	})

int32_t zxdh_phyport_get(struct rte_eth_dev *dev, uint8_t *phyport);
int32_t zxdh_panelid_get(struct rte_eth_dev *dev, uint8_t *pannelid);
int32_t zxdh_hashidx_get(struct rte_eth_dev *dev, uint8_t *hash_idx);
uint32_t zxdh_read_bar_reg(struct rte_eth_dev *dev, uint32_t bar, uint32_t reg);
void zxdh_write_bar_reg(struct rte_eth_dev *dev, uint32_t bar, uint32_t reg, uint32_t val);
void zxdh_release_lock(struct zxdh_hw *hw);
int32_t zxdh_timedlock(struct zxdh_hw *hw, uint32_t us);
uint32_t zxdh_read_comm_reg(uint64_t pci_comm_cfg_baseaddr, uint32_t reg);
void zxdh_write_comm_reg(uint64_t pci_comm_cfg_baseaddr, uint32_t reg, uint32_t val);
int32_t zxdh_datach_set(struct rte_eth_dev *dev, uint16_t ph_chno);

bool zxdh_rx_offload_enabled(struct zxdh_hw *hw);
bool zxdh_tx_offload_enabled(struct zxdh_hw *hw);

#endif /* ZXDH_COMMON_H */
