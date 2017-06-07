/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2015 Intel Corporation. All rights reserved.
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
 *     * Neither the name of Intel Corporation nor the names of its
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

#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/queue.h>

#include <rte_interrupts.h>
#include <rte_log.h>
#include <rte_debug.h>
#include <rte_pci.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
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
#define IXGBE_MAX_FLX_SOURCE_OFF        62
#define IXGBE_FDIRCTRL_FLEX_MASK        (0x1F << IXGBE_FDIRCTRL_FLEX_SHIFT)
#define IXGBE_FDIRCMD_CMD_INTERVAL_US   10

#define IXGBE_FDIR_FLOW_TYPES ( \
	(1 << RTE_ETH_FLOW_NONFRAG_IPV4_UDP) | \
	(1 << RTE_ETH_FLOW_NONFRAG_IPV4_TCP) | \
	(1 << RTE_ETH_FLOW_NONFRAG_IPV4_SCTP) | \
	(1 << RTE_ETH_FLOW_NONFRAG_IPV4_OTHER) | \
	(1 << RTE_ETH_FLOW_NONFRAG_IPV6_UDP) | \
	(1 << RTE_ETH_FLOW_NONFRAG_IPV6_TCP) | \
	(1 << RTE_ETH_FLOW_NONFRAG_IPV6_SCTP) | \
	(1 << RTE_ETH_FLOW_NONFRAG_IPV6_OTHER))

#define IPV6_ADDR_TO_MASK(ipaddr, ipv6m) do { \
	uint8_t ipv6_addr[16]; \
	uint8_t i; \
	rte_memcpy(ipv6_addr, (ipaddr), sizeof(ipv6_addr));\
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
	uint8_t i; \
	for (i = 0; i < sizeof(ipv6_addr); i++) { \
		if ((ipv6m) & (1 << i)) \
			ipv6_addr[i] = UINT8_MAX; \
		else \
			ipv6_addr[i] = 0; \
	} \
	rte_memcpy((ipaddr), ipv6_addr, sizeof(ipv6_addr));\
} while (0)

#define DEFAULT_VXLAN_PORT 4789
#define IXGBE_FDIRIP6M_INNER_MAC_SHIFT 4

static int fdir_erase_filter_82599(struct ixgbe_hw *hw, uint32_t fdirhash);
static int fdir_set_input_mask(struct rte_eth_dev *dev,
			       const struct rte_eth_fdir_masks *input_mask);
static int fdir_set_input_mask_82599(struct rte_eth_dev *dev);
static int fdir_set_input_mask_x550(struct rte_eth_dev *dev);
static int ixgbe_set_fdir_flex_conf(struct rte_eth_dev *dev,
		const struct rte_eth_fdir_flex_conf *conf, uint32_t *fdirctrl);
static int fdir_enable_82599(struct ixgbe_hw *hw, uint32_t fdirctrl);
static int ixgbe_fdir_filter_to_atr_input(
		const struct rte_eth_fdir_filter *fdir_filter,
		union ixgbe_atr_input *input,
		enum rte_fdir_mode mode);
static uint32_t ixgbe_atr_compute_hash_82599(union ixgbe_atr_input *atr_input,
				 uint32_t key);
static uint32_t atr_compute_sig_hash_82599(union ixgbe_atr_input *input,
		enum rte_fdir_pballoc_type pballoc);
static uint32_t atr_compute_perfect_hash_82599(union ixgbe_atr_input *input,
		enum rte_fdir_pballoc_type pballoc);
static int fdir_write_perfect_filter_82599(struct ixgbe_hw *hw,
			union ixgbe_atr_input *input, uint8_t queue,
			uint32_t fdircmd, uint32_t fdirhash,
			enum rte_fdir_mode mode);
static int fdir_add_signature_filter_82599(struct ixgbe_hw *hw,
		union ixgbe_atr_input *input, u8 queue, uint32_t fdircmd,
		uint32_t fdirhash);
static int ixgbe_add_del_fdir_filter(struct rte_eth_dev *dev,
			      const struct rte_eth_fdir_filter *fdir_filter,
			      bool del,
			      bool update);
static int ixgbe_fdir_flush(struct rte_eth_dev *dev);
static void ixgbe_fdir_info_get(struct rte_eth_dev *dev,
			struct rte_eth_fdir_info *fdir_info);
static void ixgbe_fdir_stats_get(struct rte_eth_dev *dev,
			struct rte_eth_fdir_stats *fdir_stats);

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
configure_fdir_flags(const struct rte_fdir_conf *conf, uint32_t *fdirctrl)
{
	*fdirctrl = 0;

	switch (conf->pballoc) {
	case RTE_FDIR_PBALLOC_64K:
		/* 8k - 1 signature filters */
		*fdirctrl |= IXGBE_FDIRCTRL_PBALLOC_64K;
		break;
	case RTE_FDIR_PBALLOC_128K:
		/* 16k - 1 signature filters */
		*fdirctrl |= IXGBE_FDIRCTRL_PBALLOC_128K;
		break;
	case RTE_FDIR_PBALLOC_256K:
		/* 32k - 1 signature filters */
		*fdirctrl |= IXGBE_FDIRCTRL_PBALLOC_256K;
		break;
	default:
		/* bad value */
		PMD_INIT_LOG(ERR, "Invalid fdir_conf->pballoc value");
		return -EINVAL;
	};

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

	if (conf->mode >= RTE_FDIR_MODE_PERFECT &&
	    conf->mode <= RTE_FDIR_MODE_PERFECT_TUNNEL) {
		*fdirctrl |= IXGBE_FDIRCTRL_PERFECT_MATCH;
		*fdirctrl |= (conf->drop_queue << IXGBE_FDIRCTRL_DROP_Q_SHIFT);
		if (conf->mode == RTE_FDIR_MODE_PERFECT_MAC_VLAN)
			*fdirctrl |= (IXGBE_FDIRCTRL_FILTERMODE_MACVLAN
					<< IXGBE_FDIRCTRL_FILTERMODE_SHIFT);
		else if (conf->mode == RTE_FDIR_MODE_PERFECT_TUNNEL)
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
fdir_set_input_mask_82599(struct rte_eth_dev *dev)
{
	struct ixgbe_hw *hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct ixgbe_hw_fdir_info *info =
			IXGBE_DEV_PRIVATE_TO_FDIR_INFO(dev->data->dev_private);
	/*
	 * mask VM pool and DIPv6 since there are currently not supported
	 * mask FLEX byte, it will be set in flex_conf
	 */
	uint32_t fdirm = IXGBE_FDIRM_POOL | IXGBE_FDIRM_DIPv6;
	uint32_t fdirtcpm;  /* TCP source and destination port masks. */
	uint32_t fdiripv6m; /* IPv6 source and destination masks. */
	volatile uint32_t *reg;

	PMD_INIT_FUNC_TRACE();

	/*
	 * Program the relevant mask registers.  If src/dst_port or src/dst_addr
	 * are zero, then assume a full mask for that field. Also assume that
	 * a VLAN of 0 is unspecified, so mask that out as well.  L4type
	 * cannot be masked out in this implementation.
	 */
	if (info->mask.dst_port_mask == 0 && info->mask.src_port_mask == 0)
		/* use the L4 protocol mask for raw IPv4/IPv6 traffic */
		fdirm |= IXGBE_FDIRM_L4P;

	if (info->mask.vlan_tci_mask == rte_cpu_to_be_16(0x0FFF))
		/* mask VLAN Priority */
		fdirm |= IXGBE_FDIRM_VLANP;
	else if (info->mask.vlan_tci_mask == rte_cpu_to_be_16(0xE000))
		/* mask VLAN ID */
		fdirm |= IXGBE_FDIRM_VLANID;
	else if (info->mask.vlan_tci_mask == 0)
		/* mask VLAN ID and Priority */
		fdirm |= IXGBE_FDIRM_VLANID | IXGBE_FDIRM_VLANP;
	else if (info->mask.vlan_tci_mask != rte_cpu_to_be_16(0xEFFF)) {
		PMD_INIT_LOG(ERR, "invalid vlan_tci_mask");
		return -EINVAL;
	}

	/* flex byte mask */
	if (info->mask.flex_bytes_mask == 0)
		fdirm |= IXGBE_FDIRM_FLEX;

	IXGBE_WRITE_REG(hw, IXGBE_FDIRM, fdirm);

	/* store the TCP/UDP port masks, bit reversed from port layout */
	fdirtcpm = reverse_fdir_bitmasks(
			rte_be_to_cpu_16(info->mask.dst_port_mask),
			rte_be_to_cpu_16(info->mask.src_port_mask));

	/* write all the same so that UDP, TCP and SCTP use the same mask
	 * (little-endian)
	 */
	IXGBE_WRITE_REG(hw, IXGBE_FDIRTCPM, ~fdirtcpm);
	IXGBE_WRITE_REG(hw, IXGBE_FDIRUDPM, ~fdirtcpm);
	IXGBE_WRITE_REG(hw, IXGBE_FDIRSCTPM, ~fdirtcpm);

	/* Store source and destination IPv4 masks (big-endian),
	 * can not use IXGBE_WRITE_REG.
	 */
	reg = IXGBE_PCI_REG_ADDR(hw, IXGBE_FDIRSIP4M);
	*reg = ~(info->mask.src_ipv4_mask);
	reg = IXGBE_PCI_REG_ADDR(hw, IXGBE_FDIRDIP4M);
	*reg = ~(info->mask.dst_ipv4_mask);

	if (dev->data->dev_conf.fdir_conf.mode == RTE_FDIR_MODE_SIGNATURE) {
		/*
		 * Store source and destination IPv6 masks (bit reversed)
		 */
		fdiripv6m = (info->mask.dst_ipv6_mask << 16) |
			    info->mask.src_ipv6_mask;

		IXGBE_WRITE_REG(hw, IXGBE_FDIRIP6M, ~fdiripv6m);
	}

	return IXGBE_SUCCESS;
}

/*
 * This references ixgbe_fdir_set_input_mask_82599() in base/ixgbe_82599.c,
 * but makes use of the rte_fdir_masks structure to see which bits to set.
 */
static int
fdir_set_input_mask_x550(struct rte_eth_dev *dev)
{
	struct ixgbe_hw *hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct ixgbe_hw_fdir_info *info =
			IXGBE_DEV_PRIVATE_TO_FDIR_INFO(dev->data->dev_private);
	/* mask VM pool and DIPv6 since there are currently not supported
	 * mask FLEX byte, it will be set in flex_conf
	 */
	uint32_t fdirm = IXGBE_FDIRM_POOL | IXGBE_FDIRM_DIPv6 |
			 IXGBE_FDIRM_FLEX;
	uint32_t fdiripv6m;
	enum rte_fdir_mode mode = dev->data->dev_conf.fdir_conf.mode;
	uint16_t mac_mask;

	PMD_INIT_FUNC_TRACE();

	/* set the default UDP port for VxLAN */
	if (mode == RTE_FDIR_MODE_PERFECT_TUNNEL)
		IXGBE_WRITE_REG(hw, IXGBE_VXLANCTRL, DEFAULT_VXLAN_PORT);

	/* some bits must be set for mac vlan or tunnel mode */
	fdirm |= IXGBE_FDIRM_L4P | IXGBE_FDIRM_L3P;

	if (info->mask.vlan_tci_mask == rte_cpu_to_be_16(0x0FFF))
		/* mask VLAN Priority */
		fdirm |= IXGBE_FDIRM_VLANP;
	else if (info->mask.vlan_tci_mask == rte_cpu_to_be_16(0xE000))
		/* mask VLAN ID */
		fdirm |= IXGBE_FDIRM_VLANID;
	else if (info->mask.vlan_tci_mask == 0)
		/* mask VLAN ID and Priority */
		fdirm |= IXGBE_FDIRM_VLANID | IXGBE_FDIRM_VLANP;
	else if (info->mask.vlan_tci_mask != rte_cpu_to_be_16(0xEFFF)) {
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
		mac_mask = info->mask.mac_addr_byte_mask;
		fdiripv6m |= (mac_mask << IXGBE_FDIRIP6M_INNER_MAC_SHIFT)
				& IXGBE_FDIRIP6M_INNER_MAC;

		switch (info->mask.tunnel_type_mask) {
		case 0:
			/* Mask turnnel type */
			fdiripv6m |= IXGBE_FDIRIP6M_TUNNEL_TYPE;
			break;
		case 1:
			break;
		default:
			PMD_INIT_LOG(ERR, "invalid tunnel_type_mask");
			return -EINVAL;
		}

		switch (rte_be_to_cpu_32(info->mask.tunnel_id_mask)) {
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
ixgbe_fdir_store_input_mask_82599(struct rte_eth_dev *dev,
				  const struct rte_eth_fdir_masks *input_mask)
{
	struct ixgbe_hw_fdir_info *info =
		IXGBE_DEV_PRIVATE_TO_FDIR_INFO(dev->data->dev_private);
	uint16_t dst_ipv6m = 0;
	uint16_t src_ipv6m = 0;

	memset(&info->mask, 0, sizeof(struct ixgbe_hw_fdir_mask));
	info->mask.vlan_tci_mask = input_mask->vlan_tci_mask;
	info->mask.src_port_mask = input_mask->src_port_mask;
	info->mask.dst_port_mask = input_mask->dst_port_mask;
	info->mask.src_ipv4_mask = input_mask->ipv4_mask.src_ip;
	info->mask.dst_ipv4_mask = input_mask->ipv4_mask.dst_ip;
	IPV6_ADDR_TO_MASK(input_mask->ipv6_mask.src_ip, src_ipv6m);
	IPV6_ADDR_TO_MASK(input_mask->ipv6_mask.dst_ip, dst_ipv6m);
	info->mask.src_ipv6_mask = src_ipv6m;
	info->mask.dst_ipv6_mask = dst_ipv6m;

	return IXGBE_SUCCESS;
}

static int
ixgbe_fdir_store_input_mask_x550(struct rte_eth_dev *dev,
				 const struct rte_eth_fdir_masks *input_mask)
{
	struct ixgbe_hw_fdir_info *info =
		IXGBE_DEV_PRIVATE_TO_FDIR_INFO(dev->data->dev_private);

	memset(&info->mask, 0, sizeof(struct ixgbe_hw_fdir_mask));
	info->mask.vlan_tci_mask = input_mask->vlan_tci_mask;
	info->mask.mac_addr_byte_mask = input_mask->mac_addr_byte_mask;
	info->mask.tunnel_type_mask = input_mask->tunnel_type_mask;
	info->mask.tunnel_id_mask = input_mask->tunnel_id_mask;

	return IXGBE_SUCCESS;
}

static int
ixgbe_fdir_store_input_mask(struct rte_eth_dev *dev,
			    const struct rte_eth_fdir_masks *input_mask)
{
	enum rte_fdir_mode mode = dev->data->dev_conf.fdir_conf.mode;

	if (mode >= RTE_FDIR_MODE_SIGNATURE &&
	    mode <= RTE_FDIR_MODE_PERFECT)
		return ixgbe_fdir_store_input_mask_82599(dev, input_mask);
	else if (mode >= RTE_FDIR_MODE_PERFECT_MAC_VLAN &&
		 mode <= RTE_FDIR_MODE_PERFECT_TUNNEL)
		return ixgbe_fdir_store_input_mask_x550(dev, input_mask);

	PMD_DRV_LOG(ERR, "Not supported fdir mode - %d!", mode);
	return -ENOTSUP;
}

int
ixgbe_fdir_set_input_mask(struct rte_eth_dev *dev)
{
	enum rte_fdir_mode mode = dev->data->dev_conf.fdir_conf.mode;

	if (mode >= RTE_FDIR_MODE_SIGNATURE &&
	    mode <= RTE_FDIR_MODE_PERFECT)
		return fdir_set_input_mask_82599(dev);
	else if (mode >= RTE_FDIR_MODE_PERFECT_MAC_VLAN &&
		 mode <= RTE_FDIR_MODE_PERFECT_TUNNEL)
		return fdir_set_input_mask_x550(dev);

	PMD_DRV_LOG(ERR, "Not supported fdir mode - %d!", mode);
	return -ENOTSUP;
}

int
ixgbe_fdir_set_flexbytes_offset(struct rte_eth_dev *dev,
				uint16_t offset)
{
	struct ixgbe_hw *hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint32_t fdirctrl;
	int i;

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
	return 0;
}

static int
fdir_set_input_mask(struct rte_eth_dev *dev,
		    const struct rte_eth_fdir_masks *input_mask)
{
	int ret;

	ret = ixgbe_fdir_store_input_mask(dev, input_mask);
	if (ret)
		return ret;

	return ixgbe_fdir_set_input_mask(dev);
}

/*
 * ixgbe_check_fdir_flex_conf -check if the flex payload and mask configuration
 * arguments are valid
 */
static int
ixgbe_set_fdir_flex_conf(struct rte_eth_dev *dev,
		const struct rte_eth_fdir_flex_conf *conf, uint32_t *fdirctrl)
{
	struct ixgbe_hw *hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct ixgbe_hw_fdir_info *info =
			IXGBE_DEV_PRIVATE_TO_FDIR_INFO(dev->data->dev_private);
	const struct rte_eth_flex_payload_cfg *flex_cfg;
	const struct rte_eth_fdir_flex_mask *flex_mask;
	uint32_t fdirm;
	uint16_t flexbytes = 0;
	uint16_t i;

	fdirm = IXGBE_READ_REG(hw, IXGBE_FDIRM);

	if (conf == NULL) {
		PMD_DRV_LOG(ERR, "NULL pointer.");
		return -EINVAL;
	}

	for (i = 0; i < conf->nb_payloads; i++) {
		flex_cfg = &conf->flex_set[i];
		if (flex_cfg->type != RTE_ETH_RAW_PAYLOAD) {
			PMD_DRV_LOG(ERR, "unsupported payload type.");
			return -EINVAL;
		}
		if (((flex_cfg->src_offset[0] & 0x1) == 0) &&
		    (flex_cfg->src_offset[1] == flex_cfg->src_offset[0] + 1) &&
		    (flex_cfg->src_offset[0] <= IXGBE_MAX_FLX_SOURCE_OFF)) {
			*fdirctrl &= ~IXGBE_FDIRCTRL_FLEX_MASK;
			*fdirctrl |=
				(flex_cfg->src_offset[0] / sizeof(uint16_t)) <<
					IXGBE_FDIRCTRL_FLEX_SHIFT;
		} else {
			PMD_DRV_LOG(ERR, "invalid flexbytes arguments.");
			return -EINVAL;
		}
	}

	for (i = 0; i < conf->nb_flexmasks; i++) {
		flex_mask = &conf->flex_mask[i];
		if (flex_mask->flow_type != RTE_ETH_FLOW_UNKNOWN) {
			PMD_DRV_LOG(ERR, "flexmask should be set globally.");
			return -EINVAL;
		}
		flexbytes = (uint16_t)(((flex_mask->mask[0] << 8) & 0xFF00) |
					((flex_mask->mask[1]) & 0xFF));
		if (flexbytes == UINT16_MAX)
			fdirm &= ~IXGBE_FDIRM_FLEX;
		else if (flexbytes != 0) {
			/* IXGBE_FDIRM_FLEX is set by default when set mask */
			PMD_DRV_LOG(ERR, " invalid flexbytes mask arguments.");
			return -EINVAL;
		}
	}
	IXGBE_WRITE_REG(hw, IXGBE_FDIRM, fdirm);
	info->mask.flex_bytes_mask = flexbytes ? UINT16_MAX : 0;
	info->flex_bytes_offset = (uint8_t)((*fdirctrl &
					    IXGBE_FDIRCTRL_FLEX_MASK) >>
					    IXGBE_FDIRCTRL_FLEX_SHIFT);
	return 0;
}

int
ixgbe_fdir_configure(struct rte_eth_dev *dev)
{
	struct ixgbe_hw *hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	int err;
	uint32_t fdirctrl, pbsize;
	int i;
	enum rte_fdir_mode mode = dev->data->dev_conf.fdir_conf.mode;

	PMD_INIT_FUNC_TRACE();

	if (hw->mac.type != ixgbe_mac_82599EB &&
		hw->mac.type != ixgbe_mac_X540 &&
		hw->mac.type != ixgbe_mac_X550 &&
		hw->mac.type != ixgbe_mac_X550EM_x &&
		hw->mac.type != ixgbe_mac_X550EM_a)
		return -ENOSYS;

	/* x550 supports mac-vlan and tunnel mode but other NICs not */
	if (hw->mac.type != ixgbe_mac_X550 &&
	    hw->mac.type != ixgbe_mac_X550EM_x &&
	    hw->mac.type != ixgbe_mac_X550EM_a &&
	    mode != RTE_FDIR_MODE_SIGNATURE &&
	    mode != RTE_FDIR_MODE_PERFECT)
		return -ENOSYS;

	err = configure_fdir_flags(&dev->data->dev_conf.fdir_conf, &fdirctrl);
	if (err)
		return err;

	/*
	 * Before enabling Flow Director, the Rx Packet Buffer size
	 * must be reduced.  The new value is the current size minus
	 * flow director memory usage size.
	 */
	pbsize = (1 << (PBALLOC_SIZE_SHIFT + (fdirctrl & FDIRCTRL_PBALLOC_MASK)));
	IXGBE_WRITE_REG(hw, IXGBE_RXPBSIZE(0),
	    (IXGBE_READ_REG(hw, IXGBE_RXPBSIZE(0)) - pbsize));

	/*
	 * The defaults in the HW for RX PB 1-7 are not zero and so should be
	 * initialized to zero for non DCB mode otherwise actual total RX PB
	 * would be bigger than programmed and filter space would run into
	 * the PB 0 region.
	 */
	for (i = 1; i < 8; i++)
		IXGBE_WRITE_REG(hw, IXGBE_RXPBSIZE(i), 0);

	err = fdir_set_input_mask(dev, &dev->data->dev_conf.fdir_conf.mask);
	if (err < 0) {
		PMD_INIT_LOG(ERR, " Error on setting FD mask");
		return err;
	}
	err = ixgbe_set_fdir_flex_conf(dev,
		&dev->data->dev_conf.fdir_conf.flex_conf, &fdirctrl);
	if (err < 0) {
		PMD_INIT_LOG(ERR, " Error on setting FD flexible arguments.");
		return err;
	}

	err = fdir_enable_82599(hw, fdirctrl);
	if (err < 0) {
		PMD_INIT_LOG(ERR, " Error on enabling FD.");
		return err;
	}
	return 0;
}

/*
 * Convert DPDK rte_eth_fdir_filter struct to ixgbe_atr_input union that is used
 * by the IXGBE driver code.
 */
static int
ixgbe_fdir_filter_to_atr_input(const struct rte_eth_fdir_filter *fdir_filter,
		union ixgbe_atr_input *input, enum rte_fdir_mode mode)
{
	input->formatted.vlan_id = fdir_filter->input.flow_ext.vlan_tci;
	input->formatted.flex_bytes = (uint16_t)(
		(fdir_filter->input.flow_ext.flexbytes[1] << 8 & 0xFF00) |
		(fdir_filter->input.flow_ext.flexbytes[0] & 0xFF));

	switch (fdir_filter->input.flow_type) {
	case RTE_ETH_FLOW_NONFRAG_IPV4_UDP:
		input->formatted.flow_type = IXGBE_ATR_FLOW_TYPE_UDPV4;
		break;
	case RTE_ETH_FLOW_NONFRAG_IPV4_TCP:
		input->formatted.flow_type = IXGBE_ATR_FLOW_TYPE_TCPV4;
		break;
	case RTE_ETH_FLOW_NONFRAG_IPV4_SCTP:
		input->formatted.flow_type = IXGBE_ATR_FLOW_TYPE_SCTPV4;
		break;
	case RTE_ETH_FLOW_NONFRAG_IPV4_OTHER:
		input->formatted.flow_type = IXGBE_ATR_FLOW_TYPE_IPV4;
		break;
	case RTE_ETH_FLOW_NONFRAG_IPV6_UDP:
		input->formatted.flow_type = IXGBE_ATR_FLOW_TYPE_UDPV6;
		break;
	case RTE_ETH_FLOW_NONFRAG_IPV6_TCP:
		input->formatted.flow_type = IXGBE_ATR_FLOW_TYPE_TCPV6;
		break;
	case RTE_ETH_FLOW_NONFRAG_IPV6_SCTP:
		input->formatted.flow_type = IXGBE_ATR_FLOW_TYPE_SCTPV6;
		break;
	case RTE_ETH_FLOW_NONFRAG_IPV6_OTHER:
		input->formatted.flow_type = IXGBE_ATR_FLOW_TYPE_IPV6;
		break;
	default:
		break;
	}

	switch (fdir_filter->input.flow_type) {
	case RTE_ETH_FLOW_NONFRAG_IPV4_UDP:
	case RTE_ETH_FLOW_NONFRAG_IPV4_TCP:
		input->formatted.src_port =
			fdir_filter->input.flow.udp4_flow.src_port;
		input->formatted.dst_port =
			fdir_filter->input.flow.udp4_flow.dst_port;
		/* fall-through */
	/*for SCTP flow type, port and verify_tag are meaningless in ixgbe.*/
	case RTE_ETH_FLOW_NONFRAG_IPV4_SCTP:
	case RTE_ETH_FLOW_NONFRAG_IPV4_OTHER:
		input->formatted.src_ip[0] =
			fdir_filter->input.flow.ip4_flow.src_ip;
		input->formatted.dst_ip[0] =
			fdir_filter->input.flow.ip4_flow.dst_ip;
		break;

	case RTE_ETH_FLOW_NONFRAG_IPV6_UDP:
	case RTE_ETH_FLOW_NONFRAG_IPV6_TCP:
		input->formatted.src_port =
			fdir_filter->input.flow.udp6_flow.src_port;
		input->formatted.dst_port =
			fdir_filter->input.flow.udp6_flow.dst_port;
		/* fall-through */
	/*for SCTP flow type, port and verify_tag are meaningless in ixgbe.*/
	case RTE_ETH_FLOW_NONFRAG_IPV6_SCTP:
	case RTE_ETH_FLOW_NONFRAG_IPV6_OTHER:
		rte_memcpy(input->formatted.src_ip,
			   fdir_filter->input.flow.ipv6_flow.src_ip,
			   sizeof(input->formatted.src_ip));
		rte_memcpy(input->formatted.dst_ip,
			   fdir_filter->input.flow.ipv6_flow.dst_ip,
			   sizeof(input->formatted.dst_ip));
		break;
	default:
		break;
	}

	if (mode == RTE_FDIR_MODE_PERFECT_MAC_VLAN) {
		rte_memcpy(
			input->formatted.inner_mac,
			fdir_filter->input.flow.mac_vlan_flow.mac_addr.addr_bytes,
			sizeof(input->formatted.inner_mac));
	} else if (mode == RTE_FDIR_MODE_PERFECT_TUNNEL) {
		rte_memcpy(
			input->formatted.inner_mac,
			fdir_filter->input.flow.tunnel_flow.mac_addr.addr_bytes,
			sizeof(input->formatted.inner_mac));
		input->formatted.tunnel_type =
			fdir_filter->input.flow.tunnel_flow.tunnel_type;
		input->formatted.tni_vni =
			fdir_filter->input.flow.tunnel_flow.tunnel_id;
	}

	return 0;
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
		enum rte_fdir_pballoc_type pballoc)
{
	if (pballoc == RTE_FDIR_PBALLOC_256K)
		return ixgbe_atr_compute_hash_82599(input,
				IXGBE_ATR_BUCKET_HASH_KEY) &
				PERFECT_BUCKET_256KB_HASH_MASK;
	else if (pballoc == RTE_FDIR_PBALLOC_128K)
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
		enum rte_fdir_pballoc_type pballoc)
{
	uint32_t bucket_hash, sig_hash;

	if (pballoc == RTE_FDIR_PBALLOC_256K)
		bucket_hash = ixgbe_atr_compute_hash_82599(input,
				IXGBE_ATR_BUCKET_HASH_KEY) &
				SIG_BUCKET_256KB_HASH_MASK;
	else if (pballoc == RTE_FDIR_PBALLOC_128K)
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
			if (input->formatted.tunnel_type !=
				RTE_FDIR_TUNNEL_TYPE_NVGRE)
				tunnel_type = 0x80000000;
			tunnel_type |= addr_high;
			IXGBE_WRITE_REG(hw, IXGBE_FDIRSIPv6(0), addr_low);
			IXGBE_WRITE_REG(hw, IXGBE_FDIRSIPv6(1), tunnel_type);
			IXGBE_WRITE_REG(hw, IXGBE_FDIRSIPv6(2),
					input->formatted.tni_vni);
		}
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

static inline struct ixgbe_fdir_filter *
ixgbe_fdir_filter_lookup(struct ixgbe_hw_fdir_info *fdir_info,
			 union ixgbe_atr_input *key)
{
	int ret;

	ret = rte_hash_lookup(fdir_info->hash_handle, (const void *)key);
	if (ret < 0)
		return NULL;

	return fdir_info->hash_map[ret];
}

static inline int
ixgbe_insert_fdir_filter(struct ixgbe_hw_fdir_info *fdir_info,
			 struct ixgbe_fdir_filter *fdir_filter)
{
	int ret;

	ret = rte_hash_add_key(fdir_info->hash_handle,
			       &fdir_filter->ixgbe_fdir);

	if (ret < 0) {
		PMD_DRV_LOG(ERR,
			    "Failed to insert fdir filter to hash table %d!",
			    ret);
		return ret;
	}

	fdir_info->hash_map[ret] = fdir_filter;

	TAILQ_INSERT_TAIL(&fdir_info->fdir_list, fdir_filter, entries);

	return 0;
}

static inline int
ixgbe_remove_fdir_filter(struct ixgbe_hw_fdir_info *fdir_info,
			 union ixgbe_atr_input *key)
{
	int ret;
	struct ixgbe_fdir_filter *fdir_filter;

	ret = rte_hash_del_key(fdir_info->hash_handle, key);

	if (ret < 0) {
		PMD_DRV_LOG(ERR, "No such fdir filter to delete %d!", ret);
		return ret;
	}

	fdir_filter = fdir_info->hash_map[ret];
	fdir_info->hash_map[ret] = NULL;

	TAILQ_REMOVE(&fdir_info->fdir_list, fdir_filter, entries);
	rte_free(fdir_filter);

	return 0;
}

static int
ixgbe_interpret_fdir_filter(struct rte_eth_dev *dev,
			    const struct rte_eth_fdir_filter *fdir_filter,
			    struct ixgbe_fdir_rule *rule)
{
	enum rte_fdir_mode fdir_mode = dev->data->dev_conf.fdir_conf.mode;
	int err;

	memset(rule, 0, sizeof(struct ixgbe_fdir_rule));

	err = ixgbe_fdir_filter_to_atr_input(fdir_filter,
					     &rule->ixgbe_fdir,
					     fdir_mode);
	if (err)
		return err;

	rule->mode = fdir_mode;
	if (fdir_filter->action.behavior == RTE_ETH_FDIR_REJECT)
		rule->fdirflags = IXGBE_FDIRCMD_DROP;
	rule->queue = fdir_filter->action.rx_queue;
	rule->soft_id = fdir_filter->soft_id;

	return 0;
}

int
ixgbe_fdir_filter_program(struct rte_eth_dev *dev,
			  struct ixgbe_fdir_rule *rule,
			  bool del,
			  bool update)
{
	struct ixgbe_hw *hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint32_t fdircmd_flags;
	uint32_t fdirhash;
	uint8_t queue;
	bool is_perfect = FALSE;
	int err;
	struct ixgbe_hw_fdir_info *info =
		IXGBE_DEV_PRIVATE_TO_FDIR_INFO(dev->data->dev_private);
	enum rte_fdir_mode fdir_mode = dev->data->dev_conf.fdir_conf.mode;
	struct ixgbe_fdir_filter *node;
	bool add_node = FALSE;

	if (fdir_mode == RTE_FDIR_MODE_NONE ||
	    fdir_mode != rule->mode)
		return -ENOTSUP;

	/*
	 * Sanity check for x550.
	 * When adding a new filter with flow type set to IPv4,
	 * the flow director mask should be configed before,
	 * and the L4 protocol and ports are masked.
	 */
	if ((!del) &&
	    (hw->mac.type == ixgbe_mac_X550 ||
	     hw->mac.type == ixgbe_mac_X550EM_x ||
	     hw->mac.type == ixgbe_mac_X550EM_a) &&
	    (rule->ixgbe_fdir.formatted.flow_type ==
	     IXGBE_ATR_FLOW_TYPE_IPV4) &&
	    (info->mask.src_port_mask != 0 ||
	     info->mask.dst_port_mask != 0)) {
		PMD_DRV_LOG(ERR, "By this device,"
			    " IPv4 is not supported without"
			    " L4 protocol and ports masked!");
		return -ENOTSUP;
	}

	if (fdir_mode >= RTE_FDIR_MODE_PERFECT &&
	    fdir_mode <= RTE_FDIR_MODE_PERFECT_TUNNEL)
		is_perfect = TRUE;

	if (is_perfect) {
		if (rule->ixgbe_fdir.formatted.flow_type &
		    IXGBE_ATR_L4TYPE_IPV6_MASK) {
			PMD_DRV_LOG(ERR, "IPv6 is not supported in"
				    " perfect mode!");
			return -ENOTSUP;
		}
		fdirhash = atr_compute_perfect_hash_82599(&rule->ixgbe_fdir,
							  dev->data->dev_conf.fdir_conf.pballoc);
		fdirhash |= rule->soft_id <<
			IXGBE_FDIRHASH_SIG_SW_INDEX_SHIFT;
	} else
		fdirhash = atr_compute_sig_hash_82599(&rule->ixgbe_fdir,
						      dev->data->dev_conf.fdir_conf.pballoc);

	if (del) {
		err = ixgbe_remove_fdir_filter(info, &rule->ixgbe_fdir);
		if (err < 0)
			return err;

		err = fdir_erase_filter_82599(hw, fdirhash);
		if (err < 0)
			PMD_DRV_LOG(ERR, "Fail to delete FDIR filter!");
		else
			PMD_DRV_LOG(DEBUG, "Success to delete FDIR filter!");
		return err;
	}
	/* add or update an fdir filter*/
	fdircmd_flags = (update) ? IXGBE_FDIRCMD_FILTER_UPDATE : 0;
	if (rule->fdirflags & IXGBE_FDIRCMD_DROP) {
		if (is_perfect) {
			queue = dev->data->dev_conf.fdir_conf.drop_queue;
			fdircmd_flags |= IXGBE_FDIRCMD_DROP;
		} else {
			PMD_DRV_LOG(ERR, "Drop option is not supported in"
				    " signature mode.");
			return -EINVAL;
		}
	} else if (rule->queue < IXGBE_MAX_RX_QUEUE_NUM)
		queue = (uint8_t)rule->queue;
	else
		return -EINVAL;

	node = ixgbe_fdir_filter_lookup(info, &rule->ixgbe_fdir);
	if (node) {
		if (update) {
			node->fdirflags = fdircmd_flags;
			node->fdirhash = fdirhash;
			node->queue = queue;
		} else {
			PMD_DRV_LOG(ERR, "Conflict with existing fdir filter!");
			return -EINVAL;
		}
	} else {
		add_node = TRUE;
		node = rte_zmalloc("ixgbe_fdir",
				   sizeof(struct ixgbe_fdir_filter),
				   0);
		if (!node)
			return -ENOMEM;
		(void)rte_memcpy(&node->ixgbe_fdir,
				 &rule->ixgbe_fdir,
				 sizeof(union ixgbe_atr_input));
		node->fdirflags = fdircmd_flags;
		node->fdirhash = fdirhash;
		node->queue = queue;

		err = ixgbe_insert_fdir_filter(info, node);
		if (err < 0) {
			rte_free(node);
			return err;
		}
	}

	if (is_perfect) {
		err = fdir_write_perfect_filter_82599(hw, &rule->ixgbe_fdir,
						      queue, fdircmd_flags,
						      fdirhash, fdir_mode);
	} else {
		err = fdir_add_signature_filter_82599(hw, &rule->ixgbe_fdir,
						      queue, fdircmd_flags,
						      fdirhash);
	}
	if (err < 0) {
		PMD_DRV_LOG(ERR, "Fail to add FDIR filter!");

		if (add_node)
			(void)ixgbe_remove_fdir_filter(info, &rule->ixgbe_fdir);
	} else {
		PMD_DRV_LOG(DEBUG, "Success to add FDIR filter");
	}

	return err;
}

/* ixgbe_add_del_fdir_filter - add or remove a flow diretor filter.
 * @dev: pointer to the structure rte_eth_dev
 * @fdir_filter: fdir filter entry
 * @del: 1 - delete, 0 - add
 * @update: 1 - update
 */
static int
ixgbe_add_del_fdir_filter(struct rte_eth_dev *dev,
			  const struct rte_eth_fdir_filter *fdir_filter,
			  bool del,
			  bool update)
{
	struct ixgbe_fdir_rule rule;
	int err;

	err = ixgbe_interpret_fdir_filter(dev, fdir_filter, &rule);

	if (err)
		return err;

	return ixgbe_fdir_filter_program(dev, &rule, del, update);
}

static int
ixgbe_fdir_flush(struct rte_eth_dev *dev)
{
	struct ixgbe_hw *hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct ixgbe_hw_fdir_info *info =
			IXGBE_DEV_PRIVATE_TO_FDIR_INFO(dev->data->dev_private);
	int ret;

	ret = ixgbe_reinit_fdir_tables_82599(hw);
	if (ret < 0) {
		PMD_INIT_LOG(ERR, "Failed to re-initialize FD table.");
		return ret;
	}

	info->f_add = 0;
	info->f_remove = 0;
	info->add = 0;
	info->remove = 0;

	return ret;
}

#define FDIRENTRIES_NUM_SHIFT 10
static void
ixgbe_fdir_info_get(struct rte_eth_dev *dev, struct rte_eth_fdir_info *fdir_info)
{
	struct ixgbe_hw *hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct ixgbe_hw_fdir_info *info =
			IXGBE_DEV_PRIVATE_TO_FDIR_INFO(dev->data->dev_private);
	uint32_t fdirctrl, max_num;
	uint8_t offset;

	fdirctrl = IXGBE_READ_REG(hw, IXGBE_FDIRCTRL);
	offset = ((fdirctrl & IXGBE_FDIRCTRL_FLEX_MASK) >>
			IXGBE_FDIRCTRL_FLEX_SHIFT) * sizeof(uint16_t);

	fdir_info->mode = dev->data->dev_conf.fdir_conf.mode;
	max_num = (1 << (FDIRENTRIES_NUM_SHIFT +
			(fdirctrl & FDIRCTRL_PBALLOC_MASK)));
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
		fdir_info->flow_types_mask[0] = 0;
	else
		fdir_info->flow_types_mask[0] = IXGBE_FDIR_FLOW_TYPES;

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

static void
ixgbe_fdir_stats_get(struct rte_eth_dev *dev, struct rte_eth_fdir_stats *fdir_stats)
{
	struct ixgbe_hw *hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct ixgbe_hw_fdir_info *info =
		IXGBE_DEV_PRIVATE_TO_FDIR_INFO(dev->data->dev_private);
	uint32_t reg, max_num;
	enum rte_fdir_mode fdir_mode = dev->data->dev_conf.fdir_conf.mode;

	/* Get the information from registers */
	reg = IXGBE_READ_REG(hw, IXGBE_FDIRFREE);
	info->collision = (uint16_t)((reg & IXGBE_FDIRFREE_COLL_MASK) >>
				     IXGBE_FDIRFREE_COLL_SHIFT);
	info->free = (uint16_t)((reg & IXGBE_FDIRFREE_FREE_MASK) >>
				IXGBE_FDIRFREE_FREE_SHIFT);

	reg = IXGBE_READ_REG(hw, IXGBE_FDIRLEN);
	info->maxhash = (uint16_t)((reg & IXGBE_FDIRLEN_MAXHASH_MASK) >>
				   IXGBE_FDIRLEN_MAXHASH_SHIFT);
	info->maxlen  = (uint8_t)((reg & IXGBE_FDIRLEN_MAXLEN_MASK) >>
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
	fdir_stats->collision = info->collision;
	fdir_stats->free = info->free;
	fdir_stats->maxhash = info->maxhash;
	fdir_stats->maxlen = info->maxlen;
	fdir_stats->remove = info->remove;
	fdir_stats->add = info->add;
	fdir_stats->f_remove = info->f_remove;
	fdir_stats->f_add = info->f_add;

	reg = IXGBE_READ_REG(hw, IXGBE_FDIRCTRL);
	max_num = (1 << (FDIRENTRIES_NUM_SHIFT +
			 (reg & FDIRCTRL_PBALLOC_MASK)));
	if (fdir_mode >= RTE_FDIR_MODE_PERFECT &&
	    fdir_mode <= RTE_FDIR_MODE_PERFECT_TUNNEL)
		fdir_stats->guarant_cnt = max_num - fdir_stats->free;
	else if (fdir_mode == RTE_FDIR_MODE_SIGNATURE)
		fdir_stats->guarant_cnt = max_num * 4 - fdir_stats->free;

}

/*
 * ixgbe_fdir_ctrl_func - deal with all operations on flow director.
 * @dev: pointer to the structure rte_eth_dev
 * @filter_op:operation will be taken
 * @arg: a pointer to specific structure corresponding to the filter_op
 */
int
ixgbe_fdir_ctrl_func(struct rte_eth_dev *dev,
			enum rte_filter_op filter_op, void *arg)
{
	struct ixgbe_hw *hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	int ret = 0;

	if (hw->mac.type != ixgbe_mac_82599EB &&
		hw->mac.type != ixgbe_mac_X540 &&
		hw->mac.type != ixgbe_mac_X550 &&
		hw->mac.type != ixgbe_mac_X550EM_x &&
		hw->mac.type != ixgbe_mac_X550EM_a)
		return -ENOTSUP;

	if (filter_op == RTE_ETH_FILTER_NOP)
		return 0;

	if (arg == NULL && filter_op != RTE_ETH_FILTER_FLUSH)
		return -EINVAL;

	switch (filter_op) {
	case RTE_ETH_FILTER_ADD:
		ret = ixgbe_add_del_fdir_filter(dev,
			(struct rte_eth_fdir_filter *)arg, FALSE, FALSE);
		break;
	case RTE_ETH_FILTER_UPDATE:
		ret = ixgbe_add_del_fdir_filter(dev,
			(struct rte_eth_fdir_filter *)arg, FALSE, TRUE);
		break;
	case RTE_ETH_FILTER_DELETE:
		ret = ixgbe_add_del_fdir_filter(dev,
			(struct rte_eth_fdir_filter *)arg, TRUE, FALSE);
		break;
	case RTE_ETH_FILTER_FLUSH:
		ret = ixgbe_fdir_flush(dev);
		break;
	case RTE_ETH_FILTER_INFO:
		ixgbe_fdir_info_get(dev, (struct rte_eth_fdir_info *)arg);
		break;
	case RTE_ETH_FILTER_STATS:
		ixgbe_fdir_stats_get(dev, (struct rte_eth_fdir_stats *)arg);
		break;
	default:
		PMD_DRV_LOG(ERR, "unknown operation %u", filter_op);
		ret = -EINVAL;
		break;
	}
	return ret;
}

/* restore flow director filter */
void
ixgbe_fdir_filter_restore(struct rte_eth_dev *dev)
{
	struct ixgbe_hw *hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct ixgbe_hw_fdir_info *fdir_info =
		IXGBE_DEV_PRIVATE_TO_FDIR_INFO(dev->data->dev_private);
	struct ixgbe_fdir_filter *node;
	bool is_perfect = FALSE;
	enum rte_fdir_mode fdir_mode = dev->data->dev_conf.fdir_conf.mode;

	if (fdir_mode >= RTE_FDIR_MODE_PERFECT &&
	    fdir_mode <= RTE_FDIR_MODE_PERFECT_TUNNEL)
		is_perfect = TRUE;

	if (is_perfect) {
		TAILQ_FOREACH(node, &fdir_info->fdir_list, entries) {
			(void)fdir_write_perfect_filter_82599(hw,
							      &node->ixgbe_fdir,
							      node->queue,
							      node->fdirflags,
							      node->fdirhash,
							      fdir_mode);
		}
	} else {
		TAILQ_FOREACH(node, &fdir_info->fdir_list, entries) {
			(void)fdir_add_signature_filter_82599(hw,
							      &node->ixgbe_fdir,
							      node->queue,
							      node->fdirflags,
							      node->fdirhash);
		}
	}
}

/* remove all the flow director filters */
int
ixgbe_clear_all_fdir_filter(struct rte_eth_dev *dev)
{
	struct ixgbe_hw_fdir_info *fdir_info =
		IXGBE_DEV_PRIVATE_TO_FDIR_INFO(dev->data->dev_private);
	struct ixgbe_fdir_filter *fdir_filter;
	struct ixgbe_fdir_filter *filter_flag;
	int ret = 0;

	/* flush flow director */
	rte_hash_reset(fdir_info->hash_handle);
	memset(fdir_info->hash_map, 0,
	       sizeof(struct ixgbe_fdir_filter *) * IXGBE_MAX_FDIR_FILTER_NUM);
	filter_flag = TAILQ_FIRST(&fdir_info->fdir_list);
	while ((fdir_filter = TAILQ_FIRST(&fdir_info->fdir_list))) {
		TAILQ_REMOVE(&fdir_info->fdir_list,
			     fdir_filter,
			     entries);
		rte_free(fdir_filter);
	}

	if (filter_flag != NULL)
		ret = ixgbe_fdir_flush(dev);

	return ret;
}
