/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2015 Intel Corporation
 */

#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <sys/queue.h>

#include <rte_interrupts.h>
#include <rte_log.h>
#include <rte_debug.h>
#include <rte_pci.h>
#include <rte_vxlan.h>
#include <ethdev_driver.h>
#include <rte_malloc.h>

#include "ixgbe_logs.h"
#include "base/ixgbe_api.h"
#include "base/ixgbe_common.h"
#include "ixgbe_ethdev.h"

/* To get PBALLOC (Packet Buffer Allocation) bits from FDIRCTRL value */
#define FDIRCTRL_PBALLOC_MASK           0x03

/* For calculating memory required for FDIR filters */
#define PBALLOC_SIZE_SHIFT              15

/* Number of bits used to mask bucket hash for different pballoc sizes */
#define PERFECT_BUCKET_64KB_HASH_MASK   0x07FF  /* 11 bits */
#define PERFECT_BUCKET_128KB_HASH_MASK  0x0FFF  /* 12 bits */
#define PERFECT_BUCKET_256KB_HASH_MASK  0x1FFF  /* 13 bits */
#define SIG_BUCKET_64KB_HASH_MASK       0x1FFF  /* 13 bits */
#define SIG_BUCKET_128KB_HASH_MASK      0x3FFF  /* 14 bits */
#define SIG_BUCKET_256KB_HASH_MASK      0x7FFF  /* 15 bits */
#define IXGBE_DEFAULT_FLEXBYTES_OFFSET  12 /* default flexbytes offset in bytes */
#define IXGBE_FDIR_MAX_FLEX_LEN         2 /* len in bytes of flexbytes */
#define IXGBE_FDIRCTRL_FLEX_MASK        (0x1F << IXGBE_FDIRCTRL_FLEX_SHIFT)
#define IXGBE_FDIRCMD_CMD_INTERVAL_US   10

#define IXGBE_FDIR_FLOW_TYPES ( \
	(1ULL << RTE_ETH_FLOW_NONFRAG_IPV4_UDP) | \
	(1ULL << RTE_ETH_FLOW_NONFRAG_IPV4_TCP) | \
	(1ULL << RTE_ETH_FLOW_NONFRAG_IPV4_SCTP) | \
	(1ULL << RTE_ETH_FLOW_NONFRAG_IPV4_OTHER) | \
	(1ULL << RTE_ETH_FLOW_NONFRAG_IPV6_UDP) | \
	(1ULL << RTE_ETH_FLOW_NONFRAG_IPV6_TCP) | \
	(1ULL << RTE_ETH_FLOW_NONFRAG_IPV6_SCTP) | \
	(1ULL << RTE_ETH_FLOW_NONFRAG_IPV6_OTHER))

#define IPV6_ADDR_TO_MASK(ipaddr, ipv6m) do { \
	uint8_t ipv6_addr[16]; \
	uint8_t i; \
	memcpy(ipv6_addr, (ipaddr), sizeof(ipv6_addr));\
	(ipv6m) = 0; \
	for (i = 0; i < sizeof(ipv6_addr); i++) { \
		if (ipv6_addr[i] == UINT8_MAX) \
			(ipv6m) |= 1 << i; \
		else if (ipv6_addr[i] != 0) { \
			PMD_DRV_LOG(ERR, " invalid IPv6 address mask."); \
			return -EINVAL; \
		} \
	} \
} while (0)

#define IPV6_MASK_TO_ADDR(ipv6m, ipaddr) do { \
	uint8_t ipv6_addr[16]; \
	for (uint8_t i = 0; i < sizeof(ipv6_addr); i++) { \
		if ((ipv6m) & (1 << i)) \
			ipv6_addr[i] = UINT8_MAX; \
		else \
			ipv6_addr[i] = 0; \
	} \
	memcpy((ipaddr), ipv6_addr, sizeof(ipv6_addr));\
} while (0)

#define IXGBE_FDIRIP6M_INNER_MAC_SHIFT 4

#define FDIRENTRIES_NUM_SHIFT 10

static int fdir_erase_filter_82599(struct ixgbe_hw *hw, uint32_t fdirhash);
static int fdir_set_input_mask_82599(struct ixgbe_adapter *adapter,
		const struct ixgbe_hw_fdir_mask *mask,
		enum rte_fdir_mode mode);
static int fdir_set_input_mask_x550(struct ixgbe_adapter *adapter,
		const struct ixgbe_hw_fdir_mask *mask,
		enum rte_fdir_mode mode);
static int fdir_enable_82599(struct ixgbe_hw *hw, uint32_t fdirctrl);
static uint32_t ixgbe_atr_compute_hash_82599(union ixgbe_atr_input *atr_input,
				 uint32_t key);
static uint32_t atr_compute_sig_hash_82599(union ixgbe_atr_input *input,
		enum rte_eth_fdir_pballoc_type pballoc);
static uint32_t atr_compute_perfect_hash_82599(union ixgbe_atr_input *input,
		enum rte_eth_fdir_pballoc_type pballoc);
static int fdir_write_perfect_filter_82599(struct ixgbe_hw *hw,
			union ixgbe_atr_input *input, uint8_t queue,
			uint32_t fdircmd, uint32_t fdirhash,
			enum rte_fdir_mode mode);
static int fdir_add_signature_filter_82599(struct ixgbe_hw *hw,
		union ixgbe_atr_input *input, u8 queue, uint32_t fdircmd,
		uint32_t fdirhash);

static uint32_t
ixgbe_fdir_pballoc_to_fdirctrl(enum rte_eth_fdir_pballoc_type pballoc)
{
	switch (pballoc) {
	case RTE_ETH_FDIR_PBALLOC_64K:
		return IXGBE_FDIRCTRL_PBALLOC_64K;
	case RTE_ETH_FDIR_PBALLOC_128K:
		return IXGBE_FDIRCTRL_PBALLOC_128K;
	case RTE_ETH_FDIR_PBALLOC_256K:
		return IXGBE_FDIRCTRL_PBALLOC_256K;
	default:
		return 0;
	}
}

static uint32_t
ixgbe_fdir_pballoc_to_bytes(enum rte_eth_fdir_pballoc_type pballoc)
{
	uint32_t pballoc_val = ixgbe_fdir_pballoc_to_fdirctrl(pballoc);

	return pballoc_val == 0 ? 0 : 1U << (PBALLOC_SIZE_SHIFT + pballoc_val);
}

static uint32_t
ixgbe_fdir_max_entries(enum rte_eth_fdir_pballoc_type pballoc)
{
	uint32_t pballoc_val = ixgbe_fdir_pballoc_to_fdirctrl(pballoc);

	if (pballoc_val == 0)
		return 0;

	return 1 << (FDIRENTRIES_NUM_SHIFT + pballoc_val);
}

/**
 * This function is based on ixgbe_fdir_enable_82599() in base/ixgbe_82599.c.
 * It adds extra configuration of fdirctrl that is common for all filter types.
 *
 *  Initialize Flow Director control registers
 *  @hw: pointer to hardware structure
 *  @fdirctrl: value to write to flow director control register
 **/
static int
fdir_enable_82599(struct ixgbe_hw *hw, uint32_t fdirctrl)
{
	int i;

	PMD_INIT_FUNC_TRACE();

	/* Prime the keys for hashing */
	IXGBE_WRITE_REG(hw, IXGBE_FDIRHKEY, IXGBE_ATR_BUCKET_HASH_KEY);
	IXGBE_WRITE_REG(hw, IXGBE_FDIRSKEY, IXGBE_ATR_SIGNATURE_HASH_KEY);

	/*
	 * Continue setup of fdirctrl register bits:
	 *  Set the maximum length per hash bucket to 0xA filters
	 *  Send interrupt when 64 filters are left
	 */
	fdirctrl |= (0xA << IXGBE_FDIRCTRL_MAX_LENGTH_SHIFT) |
		    (4 << IXGBE_FDIRCTRL_FULL_THRESH_SHIFT);

	/*
	 * Poll init-done after we write the register.  Estimated times:
	 *      10G: PBALLOC = 11b, timing is 60us
	 *       1G: PBALLOC = 11b, timing is 600us
	 *     100M: PBALLOC = 11b, timing is 6ms
	 *
	 *     Multiple these timings by 4 if under full Rx load
	 *
	 * So we'll poll for IXGBE_FDIR_INIT_DONE_POLL times, sleeping for
	 * 1 msec per poll time.  If we're at line rate and drop to 100M, then
	 * this might not finish in our poll time, but we can live with that
	 * for now.
	 */
	IXGBE_WRITE_REG(hw, IXGBE_FDIRCTRL, fdirctrl);
	IXGBE_WRITE_FLUSH(hw);
	for (i = 0; i < IXGBE_FDIR_INIT_DONE_POLL; i++) {
		if (IXGBE_READ_REG(hw, IXGBE_FDIRCTRL) &
				   IXGBE_FDIRCTRL_INIT_DONE)
			break;
		msec_delay(1);
	}

	if (i >= IXGBE_FDIR_INIT_DONE_POLL) {
		PMD_INIT_LOG(ERR, "Flow Director poll time exceeded during enabling!");
		return -ETIMEDOUT;
	}
	return 0;
}

/*
 * Set appropriate bits in fdirctrl for: variable reporting levels, moving
 * flexbytes matching field, and drop queue (only for perfect matching mode).
 */
static inline int
configure_fdir_flags(const struct ixgbe_fdir_hw_config *conf,
		enum rte_fdir_mode mode, uint32_t *fdirctrl)
{
	uint32_t pballoc;

	*fdirctrl = 0;

	pballoc = ixgbe_fdir_pballoc_to_fdirctrl(conf->pballoc);
	if (pballoc == 0) {
		/* bad value */
		PMD_INIT_LOG(ERR, "Invalid fdir_conf->pballoc value");
		return -EINVAL;
	}
	*fdirctrl |= pballoc;

	/* status flags: write hash & swindex in the rx descriptor */
	switch (conf->status) {
	case RTE_FDIR_NO_REPORT_STATUS:
		/* do nothing, default mode */
		break;
	case RTE_FDIR_REPORT_STATUS:
		/* report status when the packet matches a fdir rule */
		*fdirctrl |= IXGBE_FDIRCTRL_REPORT_STATUS;
		break;
	case RTE_FDIR_REPORT_STATUS_ALWAYS:
		/* always report status */
		*fdirctrl |= IXGBE_FDIRCTRL_REPORT_STATUS_ALWAYS;
		break;
	default:
		/* bad value */
		PMD_INIT_LOG(ERR, "Invalid fdir_conf->status value");
		return -EINVAL;
	};

	*fdirctrl |= (IXGBE_DEFAULT_FLEXBYTES_OFFSET / sizeof(uint16_t)) <<
		     IXGBE_FDIRCTRL_FLEX_SHIFT;

	if (mode >= RTE_FDIR_MODE_PERFECT &&
	    mode <= RTE_FDIR_MODE_PERFECT_TUNNEL) {
		*fdirctrl |= IXGBE_FDIRCTRL_PERFECT_MATCH;
		*fdirctrl |= (IXGBE_FDIR_DROP_QUEUE << IXGBE_FDIRCTRL_DROP_Q_SHIFT);
		if (mode == RTE_FDIR_MODE_PERFECT_MAC_VLAN)
			*fdirctrl |= (IXGBE_FDIRCTRL_FILTERMODE_MACVLAN
					<< IXGBE_FDIRCTRL_FILTERMODE_SHIFT);
		else if (mode == RTE_FDIR_MODE_PERFECT_TUNNEL)
			*fdirctrl |= (IXGBE_FDIRCTRL_FILTERMODE_CLOUD
					<< IXGBE_FDIRCTRL_FILTERMODE_SHIFT);
	}

	return 0;
}

/**
 * Reverse the bits in FDIR registers that store 2 x 16 bit masks.
 *
 *  @hi_dword: Bits 31:16 mask to be bit swapped.
 *  @lo_dword: Bits 15:0  mask to be bit swapped.
 *
 *  Flow director uses several registers to store 2 x 16 bit masks with the
 *  bits reversed such as FDIRTCPM, FDIRUDPM. The LS bit of the
 *  mask affects the MS bit/byte of the target. This function reverses the
 *  bits in these masks.
 *  **/
static inline uint32_t
reverse_fdir_bitmasks(uint16_t hi_dword, uint16_t lo_dword)
{
	uint32_t mask = hi_dword << 16;

	mask |= lo_dword;
	mask = ((mask & 0x55555555) << 1) | ((mask & 0xAAAAAAAA) >> 1);
	mask = ((mask & 0x33333333) << 2) | ((mask & 0xCCCCCCCC) >> 2);
	mask = ((mask & 0x0F0F0F0F) << 4) | ((mask & 0xF0F0F0F0) >> 4);
	return ((mask & 0x00FF00FF) << 8) | ((mask & 0xFF00FF00) >> 8);
}

/*
 * This references ixgbe_fdir_set_input_mask_82599() in base/ixgbe_82599.c,
 * but makes use of the rte_fdir_masks structure to see which bits to set.
 */
static int
fdir_set_input_mask_82599(struct ixgbe_adapter *adapter,
		const struct ixgbe_hw_fdir_mask *mask,
		enum rte_fdir_mode mode)
{
	struct ixgbe_hw *hw = IXGBE_DEV_PRIVATE_TO_HW(adapter);
	/* Mask VM pool and destination IPv6 since they are not supported. */
	uint32_t fdirm = IXGBE_FDIRM_POOL | IXGBE_FDIRM_DIPv6;
	uint32_t fdirtcpm;  /* TCP source and destination port masks. */
	uint32_t fdiripv6m; /* IPv6 source and destination masks. */
	volatile uint32_t *reg;

	PMD_INIT_FUNC_TRACE();

	if (!mask->l4_proto_match)
		/* set L4P to ignore L4 protocol for IP traffic */
		fdirm |= IXGBE_FDIRM_L4P;

	if (mask->vlan_tci_mask == rte_cpu_to_be_16(0x0FFF))
		/* mask VLAN Priority */
		fdirm |= IXGBE_FDIRM_VLANP;
	else if (mask->vlan_tci_mask == rte_cpu_to_be_16(0xE000))
		/* mask VLAN ID */
		fdirm |= IXGBE_FDIRM_VLANID;
	else if (mask->vlan_tci_mask == 0)
		/* mask VLAN ID and Priority */
		fdirm |= IXGBE_FDIRM_VLANID | IXGBE_FDIRM_VLANP;
	else if (mask->vlan_tci_mask != rte_cpu_to_be_16(0xEFFF)) {
		PMD_INIT_LOG(ERR, "invalid vlan_tci_mask");
		return -EINVAL;
	}

	/* flex byte mask */
	if (mask->flex_bytes_mask == 0)
		fdirm |= IXGBE_FDIRM_FLEX;

	IXGBE_WRITE_REG(hw, IXGBE_FDIRM, fdirm);

	/* store the TCP/UDP port masks, bit reversed from port layout */
	fdirtcpm = reverse_fdir_bitmasks(
			rte_be_to_cpu_16(mask->dst_port_mask),
			rte_be_to_cpu_16(mask->src_port_mask));

	/* write all the same so that UDP, TCP and SCTP use the same mask
	 * (little-endian)
	 */
	IXGBE_WRITE_REG(hw, IXGBE_FDIRTCPM, ~fdirtcpm);
	IXGBE_WRITE_REG(hw, IXGBE_FDIRUDPM, ~fdirtcpm);
	/* only certain MAC types have SCTP masking register */
	if (hw->mac.type == ixgbe_mac_X550 ||
			hw->mac.type == ixgbe_mac_X550EM_x ||
			hw->mac.type == ixgbe_mac_X550EM_a ||
			hw->mac.type == ixgbe_mac_E610)
		IXGBE_WRITE_REG(hw, IXGBE_FDIRSCTPM, ~fdirtcpm);

	/* Store source and destination IPv4 masks (big-endian),
	 * can not use IXGBE_WRITE_REG.
	 */
	reg = IXGBE_PCI_REG_ADDR(hw, IXGBE_FDIRSIP4M);
	*reg = ~(mask->src_ipv4_mask);
	reg = IXGBE_PCI_REG_ADDR(hw, IXGBE_FDIRDIP4M);
	*reg = ~(mask->dst_ipv4_mask);

	if (mode == RTE_FDIR_MODE_SIGNATURE) {
		/*
		 * Store source and destination IPv6 masks (bit reversed)
		 */
		fdiripv6m = (mask->dst_ipv6_mask << 16) |
				mask->src_ipv6_mask;

		IXGBE_WRITE_REG(hw, IXGBE_FDIRIP6M, ~fdiripv6m);
	}

	return IXGBE_SUCCESS;
}

/*
 * This references ixgbe_fdir_set_input_mask_82599() in base/ixgbe_82599.c,
 * but makes use of the rte_fdir_masks structure to see which bits to set.
 */
static int
fdir_set_input_mask_x550(struct ixgbe_adapter *adapter,
		const struct ixgbe_hw_fdir_mask *mask,
		enum rte_fdir_mode mode)
{
	struct ixgbe_hw *hw = IXGBE_DEV_PRIVATE_TO_HW(adapter);
	/* Mask VM pool and destination IPv6 since they are not supported. */
	uint32_t fdirm = IXGBE_FDIRM_POOL | IXGBE_FDIRM_DIPv6;
	uint32_t fdiripv6m;
	uint16_t mac_mask;

	PMD_INIT_FUNC_TRACE();

	/* set the default UDP port for VxLAN */
	if (mode == RTE_FDIR_MODE_PERFECT_TUNNEL)
		IXGBE_WRITE_REG(hw, IXGBE_VXLANCTRL, RTE_VXLAN_DEFAULT_PORT);

	/* some bits must be set for mac vlan or tunnel mode */
	fdirm |= IXGBE_FDIRM_L4P | IXGBE_FDIRM_L3P;

	/* flex byte mask */
	if (mask->flex_bytes_mask == 0)
		fdirm |= IXGBE_FDIRM_FLEX;

	if (mask->vlan_tci_mask == rte_cpu_to_be_16(0x0FFF))
		/* mask VLAN Priority */
		fdirm |= IXGBE_FDIRM_VLANP;
	else if (mask->vlan_tci_mask == rte_cpu_to_be_16(0xE000))
		/* mask VLAN ID */
		fdirm |= IXGBE_FDIRM_VLANID;
	else if (mask->vlan_tci_mask == 0)
		/* mask VLAN ID and Priority */
		fdirm |= IXGBE_FDIRM_VLANID | IXGBE_FDIRM_VLANP;
	else if (mask->vlan_tci_mask != rte_cpu_to_be_16(0xEFFF)) {
		PMD_INIT_LOG(ERR, "invalid vlan_tci_mask");
		return -EINVAL;
	}

	IXGBE_WRITE_REG(hw, IXGBE_FDIRM, fdirm);

	fdiripv6m = ((u32)0xFFFFU << IXGBE_FDIRIP6M_DIPM_SHIFT);
	fdiripv6m |= IXGBE_FDIRIP6M_ALWAYS_MASK;
	if (mode == RTE_FDIR_MODE_PERFECT_MAC_VLAN)
		fdiripv6m |= IXGBE_FDIRIP6M_TUNNEL_TYPE |
				IXGBE_FDIRIP6M_TNI_VNI;

	if (mode == RTE_FDIR_MODE_PERFECT_TUNNEL) {
		fdiripv6m |= IXGBE_FDIRIP6M_INNER_MAC;
		mac_mask = mask->mac_addr_byte_mask &
			(IXGBE_FDIRIP6M_INNER_MAC >>
			IXGBE_FDIRIP6M_INNER_MAC_SHIFT);
		fdiripv6m &= ~((mac_mask << IXGBE_FDIRIP6M_INNER_MAC_SHIFT) &
				IXGBE_FDIRIP6M_INNER_MAC);

		switch (mask->tunnel_type_mask) {
		case 0:
			/* Mask tunnel type */
			fdiripv6m |= IXGBE_FDIRIP6M_TUNNEL_TYPE;
			break;
		case 1:
			break;
		default:
			PMD_INIT_LOG(ERR, "invalid tunnel_type_mask");
			return -EINVAL;
		}

		switch (rte_be_to_cpu_32(mask->tunnel_id_mask)) {
		case 0x0:
			/* Mask vxlan id */
			fdiripv6m |= IXGBE_FDIRIP6M_TNI_VNI;
			break;
		case 0x00FFFFFF:
			fdiripv6m |= IXGBE_FDIRIP6M_TNI_VNI_24;
			break;
		case 0xFFFFFFFF:
			break;
		default:
			PMD_INIT_LOG(ERR, "invalid tunnel_id_mask");
			return -EINVAL;
		}
	}

	IXGBE_WRITE_REG(hw, IXGBE_FDIRIP6M, fdiripv6m);
	IXGBE_WRITE_REG(hw, IXGBE_FDIRTCPM, 0xFFFFFFFF);
	IXGBE_WRITE_REG(hw, IXGBE_FDIRUDPM, 0xFFFFFFFF);
	IXGBE_WRITE_REG(hw, IXGBE_FDIRSCTPM, 0xFFFFFFFF);
	IXGBE_WRITE_REG(hw, IXGBE_FDIRDIP4M, 0xFFFFFFFF);
	IXGBE_WRITE_REG(hw, IXGBE_FDIRSIP4M, 0xFFFFFFFF);

	return IXGBE_SUCCESS;
}

static int
ixgbe_fdir_set_input_mask(struct ixgbe_adapter *adapter,
		const struct ixgbe_hw_fdir_mask *mask,
		enum rte_fdir_mode mode)
{
	if (mode >= RTE_FDIR_MODE_SIGNATURE &&
	    mode <= RTE_FDIR_MODE_PERFECT)
		return fdir_set_input_mask_82599(adapter, mask, mode);
	else if (mode >= RTE_FDIR_MODE_PERFECT_MAC_VLAN &&
		 mode <= RTE_FDIR_MODE_PERFECT_TUNNEL)
		return fdir_set_input_mask_x550(adapter, mask, mode);

	PMD_DRV_LOG(ERR, "Not supported fdir mode - %d!", mode);
	return -ENOTSUP;
}

static int
ixgbe_fdir_set_flexbytes_offset(struct ixgbe_adapter *adapter, uint16_t offset)
{
	struct ixgbe_hw *hw = IXGBE_DEV_PRIVATE_TO_HW(adapter);
	uint32_t fdirctrl;
	int i;

	/**
	 * 82599 adapters flow director init flow cannot be restarted,
	 * Workaround 82599 silicon errata by performing the following steps
	 * before re-writing the FDIRCTRL control register with the same value.
	 * - write 1 to bit 8 of FDIRCMD register &
	 * - write 0 to bit 8 of FDIRCMD register
	 */
	IXGBE_WRITE_REG(hw, IXGBE_FDIRCMD,
			(IXGBE_READ_REG(hw, IXGBE_FDIRCMD) |
			 IXGBE_FDIRCMD_CLEARHT));
	IXGBE_WRITE_FLUSH(hw);
	IXGBE_WRITE_REG(hw, IXGBE_FDIRCMD,
			(IXGBE_READ_REG(hw, IXGBE_FDIRCMD) &
			 ~IXGBE_FDIRCMD_CLEARHT));
	IXGBE_WRITE_FLUSH(hw);

	fdirctrl = IXGBE_READ_REG(hw, IXGBE_FDIRCTRL);

	fdirctrl &= ~IXGBE_FDIRCTRL_FLEX_MASK;
	fdirctrl |= ((offset >> 1) /* convert to word offset */
		<< IXGBE_FDIRCTRL_FLEX_SHIFT);

	IXGBE_WRITE_REG(hw, IXGBE_FDIRCTRL, fdirctrl);
	IXGBE_WRITE_FLUSH(hw);
	for (i = 0; i < IXGBE_FDIR_INIT_DONE_POLL; i++) {
		if (IXGBE_READ_REG(hw, IXGBE_FDIRCTRL) &
			IXGBE_FDIRCTRL_INIT_DONE)
			break;
		msec_delay(1);
	}

	if (i >= IXGBE_FDIR_INIT_DONE_POLL) {
		PMD_DRV_LOG(ERR, "Flow Director poll time exceeded!");
		return -ETIMEDOUT;
	}

	return 0;
}

int
ixgbe_fdir_mask_program(struct ixgbe_adapter *adapter,
		const struct ixgbe_hw_fdir_mask *mask,
		uint8_t flex_bytes_offset,
		enum rte_fdir_mode mode)
{
	int ret;

	if (mask->flex_bytes_mask != 0) {
		ret = ixgbe_fdir_set_flexbytes_offset(adapter, flex_bytes_offset);
		if (ret != 0)
			return ret;
	}

	return ixgbe_fdir_set_input_mask(adapter, mask, mode);
}

void
ixgbe_fdir_disable(struct ixgbe_hw *hw)
{
	int i;

	IXGBE_WRITE_REG(hw, IXGBE_FDIRCTRL, 0);
	IXGBE_WRITE_REG(hw, IXGBE_RXPBSIZE(0),
			hw->mac.rx_pb_size << IXGBE_RXPBSIZE_SHIFT);
	for (i = 1; i < 8; i++)
		IXGBE_WRITE_REG(hw, IXGBE_RXPBSIZE(i), 0);
	IXGBE_WRITE_FLUSH(hw);
}

int
ixgbe_fdir_configure(struct rte_eth_dev_data *dev_data,
		const struct ixgbe_fdir_hw_config *fdir_conf,
		enum rte_fdir_mode mode)
{
	struct ixgbe_adapter *adapter = IXGBE_DEV_PRIVATE_TO_ADAPTER(dev_data->dev_private);
	struct ixgbe_hw *hw = IXGBE_DEV_PRIVATE_TO_HW(adapter);
	int err;
	uint32_t fdirctrl, fdir_size, rx_buffer_size;
	int i;

	PMD_INIT_FUNC_TRACE();

	switch (dev_data->dev_conf.rxmode.mq_mode) {
	case RTE_ETH_MQ_RX_VMDQ_DCB:
	case RTE_ETH_MQ_RX_DCB:
	case RTE_ETH_MQ_RX_DCB_RSS:
		PMD_INIT_LOG(ERR, "Flow Director is not supported with DCB");
		return -ENOTSUP;
	default:
		break;
	}

	err = configure_fdir_flags(fdir_conf, mode, &fdirctrl);
	if (err)
		return err;

	/*
	 * Before enabling Flow Director, the Rx Packet Buffer size must be
	 * reduced. The new value is the default size minus flow director
	 * memory usage size.
	 */

	/* default is in KiB so shift the value to get bytes */
	rx_buffer_size = (uint32_t)hw->mac.rx_pb_size << IXGBE_RXPBSIZE_SHIFT;

	fdir_size = ixgbe_fdir_pballoc_to_bytes(fdir_conf->pballoc);
	rx_buffer_size -= fdir_size;
	IXGBE_WRITE_REG(hw, IXGBE_RXPBSIZE(0), rx_buffer_size);

	/*
	 * The defaults in the HW for RX PB 1-7 are not zero and so should be
	 * initialized to zero for non DCB mode otherwise actual total RX PB
	 * would be bigger than programmed and filter space would run into
	 * the PB 0 region.
	 */
	for (i = 1; i < 8; i++)
		IXGBE_WRITE_REG(hw, IXGBE_RXPBSIZE(i), 0);

	err = fdir_enable_82599(hw, fdirctrl);
	if (err < 0) {
		PMD_INIT_LOG(ERR, " Error on enabling FD.");
		goto error;
	}
	err = ixgbe_reinit_fdir_tables_82599(hw);
	if (err < 0) {
		PMD_INIT_LOG(ERR, "Failed to re-initialize FD table.");
		goto error;
	}
	return 0;

error:
	ixgbe_fdir_disable(hw);
	return err;
}

/*
 * The below function is taken from the FreeBSD IXGBE drivers release
 * 2.3.8. The only change is not to mask hash_result with IXGBE_ATR_HASH_MASK
 * before returning, as the signature hash can use 16bits.
 *
 * The newer driver has optimised functions for calculating bucket and
 * signature hashes. However they don't support IPv6 type packets for signature
 * filters so are not used here.
 *
 * Note that the bkt_hash field in the ixgbe_atr_input structure is also never
 * set.
 *
 * Compute the hashes for SW ATR
 *  @stream: input bitstream to compute the hash on
 *  @key: 32-bit hash key
 **/
static uint32_t
ixgbe_atr_compute_hash_82599(union ixgbe_atr_input *atr_input,
				 uint32_t key)
{
	/*
	 * The algorithm is as follows:
	 *    Hash[15:0] = Sum { S[n] x K[n+16] }, n = 0...350
	 *    where Sum {A[n]}, n = 0...n is bitwise XOR of A[0], A[1]...A[n]
	 *    and A[n] x B[n] is bitwise AND between same length strings
	 *
	 *    K[n] is 16 bits, defined as:
	 *       for n modulo 32 >= 15, K[n] = K[n % 32 : (n % 32) - 15]
	 *       for n modulo 32 < 15, K[n] =
	 *             K[(n % 32:0) | (31:31 - (14 - (n % 32)))]
	 *
	 *    S[n] is 16 bits, defined as:
	 *       for n >= 15, S[n] = S[n:n - 15]
	 *       for n < 15, S[n] = S[(n:0) | (350:350 - (14 - n))]
	 *
	 *    To simplify for programming, the algorithm is implemented
	 *    in software this way:
	 *
	 *    key[31:0], hi_hash_dword[31:0], lo_hash_dword[31:0], hash[15:0]
	 *
	 *    for (i = 0; i < 352; i+=32)
	 *        hi_hash_dword[31:0] ^= Stream[(i+31):i];
	 *
	 *    lo_hash_dword[15:0]  ^= Stream[15:0];
	 *    lo_hash_dword[15:0]  ^= hi_hash_dword[31:16];
	 *    lo_hash_dword[31:16] ^= hi_hash_dword[15:0];
	 *
	 *    hi_hash_dword[31:0]  ^= Stream[351:320];
	 *
	 *    if (key[0])
	 *        hash[15:0] ^= Stream[15:0];
	 *
	 *    for (i = 0; i < 16; i++) {
	 *        if (key[i])
	 *            hash[15:0] ^= lo_hash_dword[(i+15):i];
	 *        if (key[i + 16])
	 *            hash[15:0] ^= hi_hash_dword[(i+15):i];
	 *    }
	 *
	 */
	__be32 common_hash_dword = 0;
	u32 hi_hash_dword, lo_hash_dword, flow_vm_vlan;
	u32 hash_result = 0;
	u8 i;

	/* record the flow_vm_vlan bits as they are a key part to the hash */
	flow_vm_vlan = IXGBE_NTOHL(atr_input->dword_stream[0]);

	/* generate common hash dword */
	for (i = 1; i <= 13; i++)
		common_hash_dword ^= atr_input->dword_stream[i];

	hi_hash_dword = IXGBE_NTOHL(common_hash_dword);

	/* low dword is word swapped version of common */
	lo_hash_dword = (hi_hash_dword >> 16) | (hi_hash_dword << 16);

	/* apply flow ID/VM pool/VLAN ID bits to hash words */
	hi_hash_dword ^= flow_vm_vlan ^ (flow_vm_vlan >> 16);

	/* Process bits 0 and 16 */
	if (key & 0x0001)
		hash_result ^= lo_hash_dword;
	if (key & 0x00010000)
		hash_result ^= hi_hash_dword;

	/*
	 * apply flow ID/VM pool/VLAN ID bits to lo hash dword, we had to
	 * delay this because bit 0 of the stream should not be processed
	 * so we do not add the vlan until after bit 0 was processed
	 */
	lo_hash_dword ^= flow_vm_vlan ^ (flow_vm_vlan << 16);


	/* process the remaining 30 bits in the key 2 bits at a time */
	for (i = 15; i; i--) {
		if (key & (0x0001 << i))
			hash_result ^= lo_hash_dword >> i;
		if (key & (0x00010000 << i))
			hash_result ^= hi_hash_dword >> i;
	}

	return hash_result;
}

static uint32_t
atr_compute_perfect_hash_82599(union ixgbe_atr_input *input,
		enum rte_eth_fdir_pballoc_type pballoc)
{
	if (pballoc == RTE_ETH_FDIR_PBALLOC_256K)
		return ixgbe_atr_compute_hash_82599(input,
				IXGBE_ATR_BUCKET_HASH_KEY) &
				PERFECT_BUCKET_256KB_HASH_MASK;
	else if (pballoc == RTE_ETH_FDIR_PBALLOC_128K)
		return ixgbe_atr_compute_hash_82599(input,
				IXGBE_ATR_BUCKET_HASH_KEY) &
				PERFECT_BUCKET_128KB_HASH_MASK;
	else
		return ixgbe_atr_compute_hash_82599(input,
				IXGBE_ATR_BUCKET_HASH_KEY) &
				PERFECT_BUCKET_64KB_HASH_MASK;
}

/**
 * ixgbe_fdir_check_cmd_complete - poll to check whether FDIRCMD is complete
 * @hw: pointer to hardware structure
 */
static inline int
ixgbe_fdir_check_cmd_complete(struct ixgbe_hw *hw, uint32_t *fdircmd)
{
	int i;

	for (i = 0; i < IXGBE_FDIRCMD_CMD_POLL; i++) {
		*fdircmd = IXGBE_READ_REG(hw, IXGBE_FDIRCMD);
		if (!(*fdircmd & IXGBE_FDIRCMD_CMD_MASK))
			return 0;
		rte_delay_us(IXGBE_FDIRCMD_CMD_INTERVAL_US);
	}

	return -ETIMEDOUT;
}

/*
 * Calculate the hash value needed for signature-match filters. In the FreeBSD
 * driver, this is done by the optimised function
 * ixgbe_atr_compute_sig_hash_82599(). However that can't be used here as it
 * doesn't support calculating a hash for an IPv6 filter.
 */
static uint32_t
atr_compute_sig_hash_82599(union ixgbe_atr_input *input,
		enum rte_eth_fdir_pballoc_type pballoc)
{
	uint32_t bucket_hash, sig_hash;

	if (pballoc == RTE_ETH_FDIR_PBALLOC_256K)
		bucket_hash = ixgbe_atr_compute_hash_82599(input,
				IXGBE_ATR_BUCKET_HASH_KEY) &
				SIG_BUCKET_256KB_HASH_MASK;
	else if (pballoc == RTE_ETH_FDIR_PBALLOC_128K)
		bucket_hash = ixgbe_atr_compute_hash_82599(input,
				IXGBE_ATR_BUCKET_HASH_KEY) &
				SIG_BUCKET_128KB_HASH_MASK;
	else
		bucket_hash = ixgbe_atr_compute_hash_82599(input,
				IXGBE_ATR_BUCKET_HASH_KEY) &
				SIG_BUCKET_64KB_HASH_MASK;

	sig_hash = ixgbe_atr_compute_hash_82599(input,
			IXGBE_ATR_SIGNATURE_HASH_KEY);

	return (sig_hash << IXGBE_FDIRHASH_SIG_SW_INDEX_SHIFT) | bucket_hash;
}

/*
 * This is based on ixgbe_fdir_write_perfect_filter_82599() in
 * base/ixgbe_82599.c, with the ability to set extra flags in FDIRCMD register
 * added, and IPv6 support also added. The hash value is also pre-calculated
 * as the pballoc value is needed to do it.
 */
static int
fdir_write_perfect_filter_82599(struct ixgbe_hw *hw,
			union ixgbe_atr_input *input, uint8_t queue,
			uint32_t fdircmd, uint32_t fdirhash,
			enum rte_fdir_mode mode)
{
	uint32_t fdirport, fdirvlan;
	u32 addr_low, addr_high;
	u32 tunnel_type = 0;
	int err = 0;
	volatile uint32_t *reg;

	if (mode == RTE_FDIR_MODE_PERFECT) {
		/* record the IPv4 address (big-endian)
		 * can not use IXGBE_WRITE_REG.
		 */
		reg = IXGBE_PCI_REG_ADDR(hw, IXGBE_FDIRIPSA);
		*reg = input->formatted.src_ip[0];
		reg = IXGBE_PCI_REG_ADDR(hw, IXGBE_FDIRIPDA);
		*reg = input->formatted.dst_ip[0];

		/* record source and destination port (little-endian)*/
		fdirport = IXGBE_NTOHS(input->formatted.dst_port);
		fdirport <<= IXGBE_FDIRPORT_DESTINATION_SHIFT;
		fdirport |= IXGBE_NTOHS(input->formatted.src_port);
		IXGBE_WRITE_REG(hw, IXGBE_FDIRPORT, fdirport);
	} else if (mode >= RTE_FDIR_MODE_PERFECT_MAC_VLAN &&
		   mode <= RTE_FDIR_MODE_PERFECT_TUNNEL) {
		/* for mac vlan and tunnel modes */
		addr_low = ((u32)input->formatted.inner_mac[0] |
			    ((u32)input->formatted.inner_mac[1] << 8) |
			    ((u32)input->formatted.inner_mac[2] << 16) |
			    ((u32)input->formatted.inner_mac[3] << 24));
		addr_high = ((u32)input->formatted.inner_mac[4] |
			     ((u32)input->formatted.inner_mac[5] << 8));

		if (mode == RTE_FDIR_MODE_PERFECT_MAC_VLAN) {
			IXGBE_WRITE_REG(hw, IXGBE_FDIRSIPv6(0), addr_low);
			IXGBE_WRITE_REG(hw, IXGBE_FDIRSIPv6(1), addr_high);
			IXGBE_WRITE_REG(hw, IXGBE_FDIRSIPv6(2), 0);
		} else {
			/* tunnel mode */
			if (input->formatted.tunnel_type)
				tunnel_type = 0x80000000;
			tunnel_type |= addr_high;
			IXGBE_WRITE_REG(hw, IXGBE_FDIRSIPv6(0), addr_low);
			IXGBE_WRITE_REG(hw, IXGBE_FDIRSIPv6(1), tunnel_type);
			IXGBE_WRITE_REG(hw, IXGBE_FDIRSIPv6(2),
					input->formatted.tni_vni);
		}
		IXGBE_WRITE_REG(hw, IXGBE_FDIRIPSA, 0);
		IXGBE_WRITE_REG(hw, IXGBE_FDIRIPDA, 0);
		IXGBE_WRITE_REG(hw, IXGBE_FDIRPORT, 0);
	}

	/* record vlan (little-endian) and flex_bytes(big-endian) */
	fdirvlan = input->formatted.flex_bytes;
	fdirvlan <<= IXGBE_FDIRVLAN_FLEX_SHIFT;
	fdirvlan |= IXGBE_NTOHS(input->formatted.vlan_id);
	IXGBE_WRITE_REG(hw, IXGBE_FDIRVLAN, fdirvlan);

	/* configure FDIRHASH register */
	IXGBE_WRITE_REG(hw, IXGBE_FDIRHASH, fdirhash);

	/*
	 * flush all previous writes to make certain registers are
	 * programmed prior to issuing the command
	 */
	IXGBE_WRITE_FLUSH(hw);

	/* configure FDIRCMD register */
	fdircmd |= IXGBE_FDIRCMD_CMD_ADD_FLOW |
		  IXGBE_FDIRCMD_LAST | IXGBE_FDIRCMD_QUEUE_EN;
	fdircmd |= input->formatted.flow_type << IXGBE_FDIRCMD_FLOW_TYPE_SHIFT;
	fdircmd |= (uint32_t)queue << IXGBE_FDIRCMD_RX_QUEUE_SHIFT;
	fdircmd |= (uint32_t)input->formatted.vm_pool << IXGBE_FDIRCMD_VT_POOL_SHIFT;

	IXGBE_WRITE_REG(hw, IXGBE_FDIRCMD, fdircmd);

	PMD_DRV_LOG(DEBUG, "Rx Queue=%x hash=%x", queue, fdirhash);

	err = ixgbe_fdir_check_cmd_complete(hw, &fdircmd);
	if (err < 0)
		PMD_DRV_LOG(ERR, "Timeout writing flow director filter.");

	return err;
}

/**
 * This function is based on ixgbe_atr_add_signature_filter_82599() in
 * base/ixgbe_82599.c, but uses a pre-calculated hash value. It also supports
 * setting extra fields in the FDIRCMD register, and removes the code that was
 * verifying the flow_type field. According to the documentation, a flow type of
 * 00 (i.e. not TCP, UDP, or SCTP) is not supported, however it appears to
 * work ok...
 *
 *  Adds a signature hash filter
 *  @hw: pointer to hardware structure
 *  @input: unique input dword
 *  @queue: queue index to direct traffic to
 *  @fdircmd: any extra flags to set in fdircmd register
 *  @fdirhash: pre-calculated hash value for the filter
 **/
static int
fdir_add_signature_filter_82599(struct ixgbe_hw *hw,
		union ixgbe_atr_input *input, u8 queue, uint32_t fdircmd,
		uint32_t fdirhash)
{
	int err = 0;

	PMD_INIT_FUNC_TRACE();

	/* configure FDIRCMD register */
	fdircmd |= IXGBE_FDIRCMD_CMD_ADD_FLOW |
		  IXGBE_FDIRCMD_LAST | IXGBE_FDIRCMD_QUEUE_EN;
	fdircmd |= input->formatted.flow_type << IXGBE_FDIRCMD_FLOW_TYPE_SHIFT;
	fdircmd |= (uint32_t)queue << IXGBE_FDIRCMD_RX_QUEUE_SHIFT;

	IXGBE_WRITE_REG(hw, IXGBE_FDIRHASH, fdirhash);
	IXGBE_WRITE_REG(hw, IXGBE_FDIRCMD, fdircmd);

	PMD_DRV_LOG(DEBUG, "Rx Queue=%x hash=%x", queue, fdirhash);

	err = ixgbe_fdir_check_cmd_complete(hw, &fdircmd);
	if (err < 0)
		PMD_DRV_LOG(ERR, "Timeout writing flow director filter.");

	return err;
}

/*
 * This is based on ixgbe_fdir_erase_perfect_filter_82599() in
 * base/ixgbe_82599.c. It is modified to take in the hash as a parameter so
 * that it can be used for removing signature and perfect filters.
 */
static int
fdir_erase_filter_82599(struct ixgbe_hw *hw, uint32_t fdirhash)
{
	uint32_t fdircmd = 0;
	int err = 0;

	IXGBE_WRITE_REG(hw, IXGBE_FDIRHASH, fdirhash);

	/* flush hash to HW */
	IXGBE_WRITE_FLUSH(hw);

	/* Query if filter is present */
	IXGBE_WRITE_REG(hw, IXGBE_FDIRCMD, IXGBE_FDIRCMD_CMD_QUERY_REM_FILT);

	err = ixgbe_fdir_check_cmd_complete(hw, &fdircmd);
	if (err < 0) {
		PMD_INIT_LOG(ERR, "Timeout querying for flow director filter.");
		return err;
	}

	/* if filter exists in hardware then remove it */
	if (fdircmd & IXGBE_FDIRCMD_FILTER_VALID) {
		IXGBE_WRITE_REG(hw, IXGBE_FDIRHASH, fdirhash);
		IXGBE_WRITE_FLUSH(hw);
		IXGBE_WRITE_REG(hw, IXGBE_FDIRCMD,
				IXGBE_FDIRCMD_CMD_REMOVE_FLOW);
	}
	err = ixgbe_fdir_check_cmd_complete(hw, &fdircmd);
	if (err < 0)
		PMD_INIT_LOG(ERR, "Timeout erasing flow director filter.");
	return err;

}

void
ixgbe_fdir_mask_set_default(struct ixgbe_hw_fdir_mask *mask)
{
	/*
	 * Default mask value 0xFF means ignore all fields.
	 */
	memset(mask, 0xFF, sizeof(*mask));
	mask->vlan_tci_mask = 0;
	mask->flex_bytes_mask = 0;
	mask->dst_port_mask = 0;
	mask->src_port_mask = 0;
	mask->l4_proto_match = 0;
}

/* hardware loses its flow director setup across a stop/start cycle */
void
ixgbe_fdir_hw_invalidate(struct rte_eth_dev *dev)
{
	struct ixgbe_adapter *adapter = IXGBE_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct ixgbe_fdir_state *state = &adapter->fdir_state;

	state->config.hw_configured = false;
	state->mask_conf.mask_configured = false;
}

bool
ixgbe_fdir_mode_is_perfect(enum rte_fdir_mode mode)
{
	return mode >= RTE_FDIR_MODE_PERFECT && mode <= RTE_FDIR_MODE_PERFECT_TUNNEL;
}

static uint32_t
ixgbe_fdir_compute_hash(struct ixgbe_adapter *adapter, struct ixgbe_fdir_rule *rule)
{
	const struct ixgbe_fdir_hw_config *fdir_conf =
			IXGBE_DEV_PRIVATE_TO_FDIR_HW_CONFIG(adapter);
	uint32_t fdirhash;

	if (!ixgbe_fdir_mode_is_perfect(rule->mode))
		return atr_compute_sig_hash_82599(&rule->ixgbe_fdir, fdir_conf->pballoc);

	fdirhash = atr_compute_perfect_hash_82599(&rule->ixgbe_fdir, fdir_conf->pballoc);
	fdirhash |= rule->soft_id << IXGBE_FDIRHASH_SIG_SW_INDEX_SHIFT;

	return fdirhash;
}

int
ixgbe_fdir_filter_program(struct ixgbe_adapter *adapter,
		struct ixgbe_fdir_rule *rule)
{
	struct ixgbe_hw *hw = IXGBE_DEV_PRIVATE_TO_HW(adapter);
	uint32_t fdircmd_flags = 0;
	uint32_t fdirhash = ixgbe_fdir_compute_hash(adapter, rule);
	uint8_t queue = rule->queue;
	int err;

	if (rule->fdirflags & IXGBE_FDIRCMD_DROP) {
		fdircmd_flags = IXGBE_FDIRCMD_DROP;
		queue = IXGBE_FDIR_DROP_QUEUE;
	}

	if (ixgbe_fdir_mode_is_perfect(rule->mode))
		err = fdir_write_perfect_filter_82599(hw, &rule->ixgbe_fdir, queue,
				fdircmd_flags, fdirhash, rule->mode);
	else
		err = fdir_add_signature_filter_82599(hw, &rule->ixgbe_fdir, queue,
				fdircmd_flags, fdirhash);

	if (err < 0)
		PMD_DRV_LOG(ERR, "Fail to add FDIR filter!");

	return err;
}

int
ixgbe_fdir_filter_clear(struct ixgbe_adapter *adapter, struct ixgbe_fdir_rule *rule)
{
	struct ixgbe_hw *hw = IXGBE_DEV_PRIVATE_TO_HW(adapter);
	uint32_t fdirhash = ixgbe_fdir_compute_hash(adapter, rule);
	int err;

	err = fdir_erase_filter_82599(hw, fdirhash);
	if (err < 0)
		PMD_DRV_LOG(ERR, "Fail to delete FDIR filter!");

	return err;
}

void
ixgbe_fdir_info_get(struct rte_eth_dev *dev, struct rte_eth_fdir_info *fdir_info)
{
	struct ixgbe_adapter *adapter = dev->data->dev_private;
	const struct ixgbe_fdir_hw_config *hw_config =
			IXGBE_DEV_PRIVATE_TO_FDIR_HW_CONFIG(adapter);
	const struct ixgbe_fdir_config_state *fdir_conf = &adapter->fdir_state.config;
	const struct ixgbe_fdir_mask_state *info = &adapter->fdir_state.mask_conf;
	uint32_t max_num;
	uint8_t offset = info->flex_bytes_offset;

	fdir_info->mode = fdir_conf->mode;
	max_num = ixgbe_fdir_max_entries(hw_config->pballoc);
	if (fdir_info->mode >= RTE_FDIR_MODE_PERFECT &&
	    fdir_info->mode <= RTE_FDIR_MODE_PERFECT_TUNNEL)
		fdir_info->guarant_spc = max_num;
	else if (fdir_info->mode == RTE_FDIR_MODE_SIGNATURE)
		fdir_info->guarant_spc = max_num * 4;

	fdir_info->mask.vlan_tci_mask = info->mask.vlan_tci_mask;
	fdir_info->mask.ipv4_mask.src_ip = info->mask.src_ipv4_mask;
	fdir_info->mask.ipv4_mask.dst_ip = info->mask.dst_ipv4_mask;
	IPV6_MASK_TO_ADDR(info->mask.src_ipv6_mask,
			fdir_info->mask.ipv6_mask.src_ip);
	IPV6_MASK_TO_ADDR(info->mask.dst_ipv6_mask,
			fdir_info->mask.ipv6_mask.dst_ip);
	fdir_info->mask.src_port_mask = info->mask.src_port_mask;
	fdir_info->mask.dst_port_mask = info->mask.dst_port_mask;
	fdir_info->mask.mac_addr_byte_mask = info->mask.mac_addr_byte_mask;
	fdir_info->mask.tunnel_id_mask = info->mask.tunnel_id_mask;
	fdir_info->mask.tunnel_type_mask = info->mask.tunnel_type_mask;
	fdir_info->max_flexpayload = IXGBE_FDIR_MAX_FLEX_LEN;

	if (fdir_info->mode == RTE_FDIR_MODE_PERFECT_MAC_VLAN ||
	    fdir_info->mode == RTE_FDIR_MODE_PERFECT_TUNNEL)
		fdir_info->flow_types_mask[0] = 0ULL;
	else
		fdir_info->flow_types_mask[0] = IXGBE_FDIR_FLOW_TYPES;
	for (uint32_t i = 1; i < RTE_FLOW_MASK_ARRAY_SIZE; i++)
		fdir_info->flow_types_mask[i] = 0ULL;

	fdir_info->flex_payload_unit = sizeof(uint16_t);
	fdir_info->max_flex_payload_segment_num = 1;
	fdir_info->flex_payload_limit = IXGBE_MAX_FLX_SOURCE_OFF;
	fdir_info->flex_conf.nb_payloads = 1;
	fdir_info->flex_conf.flex_set[0].type = RTE_ETH_RAW_PAYLOAD;
	fdir_info->flex_conf.flex_set[0].src_offset[0] = offset;
	fdir_info->flex_conf.flex_set[0].src_offset[1] = offset + 1;
	fdir_info->flex_conf.nb_flexmasks = 1;
	fdir_info->flex_conf.flex_mask[0].flow_type = RTE_ETH_FLOW_UNKNOWN;
	fdir_info->flex_conf.flex_mask[0].mask[0] =
			(uint8_t)(info->mask.flex_bytes_mask & 0x00FF);
	fdir_info->flex_conf.flex_mask[0].mask[1] =
			(uint8_t)((info->mask.flex_bytes_mask & 0xFF00) >> 8);
}

void
ixgbe_fdir_stats_get(struct rte_eth_dev *dev, struct rte_eth_fdir_stats *fdir_stats)
{
	struct ixgbe_hw *hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct ixgbe_adapter *adapter = dev->data->dev_private;
	const struct ixgbe_fdir_hw_config *hw_config =
			IXGBE_DEV_PRIVATE_TO_FDIR_HW_CONFIG(adapter);
	struct ixgbe_fdir_stats *info = &adapter->fdir_state.stats;
	uint32_t reg, max_num;
	enum rte_fdir_mode fdir_mode = adapter->fdir_state.config.mode;

	if (adapter->fdir_state.config.rule_count == 0) {
		*fdir_stats = (struct rte_eth_fdir_stats){0};
		return;
	}

	/* Get the information from registers */
	reg = IXGBE_READ_REG(hw, IXGBE_FDIRFREE);
	fdir_stats->collision = (uint16_t)((reg & IXGBE_FDIRFREE_COLL_MASK) >>
				     IXGBE_FDIRFREE_COLL_SHIFT);
	fdir_stats->free = (uint16_t)((reg & IXGBE_FDIRFREE_FREE_MASK) >>
				IXGBE_FDIRFREE_FREE_SHIFT);

	reg = IXGBE_READ_REG(hw, IXGBE_FDIRLEN);
	fdir_stats->maxhash = (uint16_t)((reg & IXGBE_FDIRLEN_MAXHASH_MASK) >>
				   IXGBE_FDIRLEN_MAXHASH_SHIFT);
	fdir_stats->maxlen  = (uint8_t)((reg & IXGBE_FDIRLEN_MAXLEN_MASK) >>
				  IXGBE_FDIRLEN_MAXLEN_SHIFT);

	reg = IXGBE_READ_REG(hw, IXGBE_FDIRUSTAT);
	info->remove += (reg & IXGBE_FDIRUSTAT_REMOVE_MASK) >>
		IXGBE_FDIRUSTAT_REMOVE_SHIFT;
	info->add += (reg & IXGBE_FDIRUSTAT_ADD_MASK) >>
		IXGBE_FDIRUSTAT_ADD_SHIFT;

	reg = IXGBE_READ_REG(hw, IXGBE_FDIRFSTAT) & 0xFFFF;
	info->f_remove += (reg & IXGBE_FDIRFSTAT_FREMOVE_MASK) >>
		IXGBE_FDIRFSTAT_FREMOVE_SHIFT;
	info->f_add += (reg & IXGBE_FDIRFSTAT_FADD_MASK) >>
		IXGBE_FDIRFSTAT_FADD_SHIFT;

	/*  Copy the new information in the fdir parameter */
	fdir_stats->remove = info->remove;
	fdir_stats->add = info->add;
	fdir_stats->f_remove = info->f_remove;
	fdir_stats->f_add = info->f_add;

	max_num = ixgbe_fdir_max_entries(hw_config->pballoc);
	if (fdir_mode >= RTE_FDIR_MODE_PERFECT &&
	    fdir_mode <= RTE_FDIR_MODE_PERFECT_TUNNEL)
		fdir_stats->guarant_cnt = max_num - fdir_stats->free;
	else if (fdir_mode == RTE_FDIR_MODE_SIGNATURE)
		fdir_stats->guarant_cnt = max_num * 4 - fdir_stats->free;

}
