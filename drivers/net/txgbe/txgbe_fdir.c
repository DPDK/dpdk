/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2020 Beijing WangXun Technology Co., Ltd.
 * Copyright(c) 2010-2017 Intel Corporation
 */

#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/queue.h>
#include <rte_malloc.h>

#include "txgbe_logs.h"
#include "base/txgbe.h"
#include "txgbe_ethdev.h"

#define TXGBE_DEFAULT_FLEXBYTES_OFFSET  12 /*default flexbytes offset in bytes*/
#define TXGBE_MAX_FLX_SOURCE_OFF        62
#define TXGBE_FDIRCMD_CMD_INTERVAL_US   10

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

/**
 *  Initialize Flow Director control registers
 *  @hw: pointer to hardware structure
 *  @fdirctrl: value to write to flow director control register
 **/
static int
txgbe_fdir_enable(struct txgbe_hw *hw, uint32_t fdirctrl)
{
	int i;

	PMD_INIT_FUNC_TRACE();

	/* Prime the keys for hashing */
	wr32(hw, TXGBE_FDIRBKTHKEY, TXGBE_ATR_BUCKET_HASH_KEY);
	wr32(hw, TXGBE_FDIRSIGHKEY, TXGBE_ATR_SIGNATURE_HASH_KEY);

	/*
	 * Continue setup of fdirctrl register bits:
	 *  Set the maximum length per hash bucket to 0xA filters
	 *  Send interrupt when 64 filters are left
	 */
	fdirctrl |= TXGBE_FDIRCTL_MAXLEN(0xA) |
		    TXGBE_FDIRCTL_FULLTHR(4);

	/*
	 * Poll init-done after we write the register.  Estimated times:
	 *      10G: PBALLOC = 11b, timing is 60us
	 *       1G: PBALLOC = 11b, timing is 600us
	 *     100M: PBALLOC = 11b, timing is 6ms
	 *
	 *     Multiple these timings by 4 if under full Rx load
	 *
	 * So we'll poll for TXGBE_FDIR_INIT_DONE_POLL times, sleeping for
	 * 1 msec per poll time.  If we're at line rate and drop to 100M, then
	 * this might not finish in our poll time, but we can live with that
	 * for now.
	 */
	wr32(hw, TXGBE_FDIRCTL, fdirctrl);
	txgbe_flush(hw);
	for (i = 0; i < TXGBE_FDIR_INIT_DONE_POLL; i++) {
		if (rd32(hw, TXGBE_FDIRCTL) & TXGBE_FDIRCTL_INITDONE)
			break;
		msec_delay(1);
	}

	if (i >= TXGBE_FDIR_INIT_DONE_POLL) {
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
configure_fdir_flags(const struct rte_eth_fdir_conf *conf,
		     uint32_t *fdirctrl, uint32_t *flex)
{
	*fdirctrl = 0;
	*flex = 0;

	switch (conf->pballoc) {
	case RTE_ETH_FDIR_PBALLOC_64K:
		/* 8k - 1 signature filters */
		*fdirctrl |= TXGBE_FDIRCTL_BUF_64K;
		break;
	case RTE_ETH_FDIR_PBALLOC_128K:
		/* 16k - 1 signature filters */
		*fdirctrl |= TXGBE_FDIRCTL_BUF_128K;
		break;
	case RTE_ETH_FDIR_PBALLOC_256K:
		/* 32k - 1 signature filters */
		*fdirctrl |= TXGBE_FDIRCTL_BUF_256K;
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
		*fdirctrl |= TXGBE_FDIRCTL_REPORT_MATCH;
		break;
	case RTE_FDIR_REPORT_STATUS_ALWAYS:
		/* always report status */
		*fdirctrl |= TXGBE_FDIRCTL_REPORT_ALWAYS;
		break;
	default:
		/* bad value */
		PMD_INIT_LOG(ERR, "Invalid fdir_conf->status value");
		return -EINVAL;
	};

	*flex |= TXGBE_FDIRFLEXCFG_BASE_MAC;
	*flex |= TXGBE_FDIRFLEXCFG_OFST(TXGBE_DEFAULT_FLEXBYTES_OFFSET / 2);

	switch (conf->mode) {
	case RTE_FDIR_MODE_SIGNATURE:
		break;
	case RTE_FDIR_MODE_PERFECT:
		*fdirctrl |= TXGBE_FDIRCTL_PERFECT;
		*fdirctrl |= TXGBE_FDIRCTL_DROPQP(conf->drop_queue);
		break;
	default:
		/* bad value */
		PMD_INIT_LOG(ERR, "Invalid fdir_conf->mode value");
		return -EINVAL;
	}

	return 0;
}

int
txgbe_fdir_set_input_mask(struct rte_eth_dev *dev)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	struct txgbe_hw_fdir_info *info = TXGBE_DEV_FDIR(dev);
	enum rte_fdir_mode mode = dev->data->dev_conf.fdir_conf.mode;
	/*
	 * mask VM pool and DIPv6 since there are currently not supported
	 * mask FLEX byte, it will be set in flex_conf
	 */
	uint32_t fdirm = TXGBE_FDIRMSK_POOL;
	uint32_t fdirtcpm;  /* TCP source and destination port masks. */
	uint32_t fdiripv6m; /* IPv6 source and destination masks. */

	PMD_INIT_FUNC_TRACE();

	if (mode != RTE_FDIR_MODE_SIGNATURE &&
	    mode != RTE_FDIR_MODE_PERFECT) {
		PMD_DRV_LOG(ERR, "Not supported fdir mode - %d!", mode);
		return -ENOTSUP;
	}

	/*
	 * Program the relevant mask registers.  If src/dst_port or src/dst_addr
	 * are zero, then assume a full mask for that field. Also assume that
	 * a VLAN of 0 is unspecified, so mask that out as well.  L4type
	 * cannot be masked out in this implementation.
	 */
	if (info->mask.dst_port_mask == 0 && info->mask.src_port_mask == 0) {
		/* use the L4 protocol mask for raw IPv4/IPv6 traffic */
		fdirm |= TXGBE_FDIRMSK_L4P;
	}

	/* TBD: don't support encapsulation yet */
	wr32(hw, TXGBE_FDIRMSK, fdirm);

	/* store the TCP/UDP port masks */
	fdirtcpm = rte_be_to_cpu_16(info->mask.dst_port_mask) << 16;
	fdirtcpm |= rte_be_to_cpu_16(info->mask.src_port_mask);

	/* write all the same so that UDP, TCP and SCTP use the same mask
	 * (little-endian)
	 */
	wr32(hw, TXGBE_FDIRTCPMSK, ~fdirtcpm);
	wr32(hw, TXGBE_FDIRUDPMSK, ~fdirtcpm);
	wr32(hw, TXGBE_FDIRSCTPMSK, ~fdirtcpm);

	/* Store source and destination IPv4 masks (big-endian) */
	wr32(hw, TXGBE_FDIRSIP4MSK, ~info->mask.src_ipv4_mask);
	wr32(hw, TXGBE_FDIRDIP4MSK, ~info->mask.dst_ipv4_mask);

	if (mode == RTE_FDIR_MODE_SIGNATURE) {
		/*
		 * Store source and destination IPv6 masks (bit reversed)
		 */
		fdiripv6m = TXGBE_FDIRIP6MSK_DST(info->mask.dst_ipv6_mask) |
			    TXGBE_FDIRIP6MSK_SRC(info->mask.src_ipv6_mask);

		wr32(hw, TXGBE_FDIRIP6MSK, ~fdiripv6m);
	}

	return 0;
}

static int
txgbe_fdir_store_input_mask(struct rte_eth_dev *dev)
{
	struct rte_eth_fdir_masks *input_mask =
				&dev->data->dev_conf.fdir_conf.mask;
	enum rte_fdir_mode mode = dev->data->dev_conf.fdir_conf.mode;
	struct txgbe_hw_fdir_info *info = TXGBE_DEV_FDIR(dev);
	uint16_t dst_ipv6m = 0;
	uint16_t src_ipv6m = 0;

	if (mode != RTE_FDIR_MODE_SIGNATURE &&
	    mode != RTE_FDIR_MODE_PERFECT) {
		PMD_DRV_LOG(ERR, "Not supported fdir mode - %d!", mode);
		return -ENOTSUP;
	}

	memset(&info->mask, 0, sizeof(struct txgbe_hw_fdir_mask));
	info->mask.vlan_tci_mask = input_mask->vlan_tci_mask;
	info->mask.src_port_mask = input_mask->src_port_mask;
	info->mask.dst_port_mask = input_mask->dst_port_mask;
	info->mask.src_ipv4_mask = input_mask->ipv4_mask.src_ip;
	info->mask.dst_ipv4_mask = input_mask->ipv4_mask.dst_ip;
	IPV6_ADDR_TO_MASK(input_mask->ipv6_mask.src_ip, src_ipv6m);
	IPV6_ADDR_TO_MASK(input_mask->ipv6_mask.dst_ip, dst_ipv6m);
	info->mask.src_ipv6_mask = src_ipv6m;
	info->mask.dst_ipv6_mask = dst_ipv6m;

	return 0;
}

int
txgbe_fdir_set_flexbytes_offset(struct rte_eth_dev *dev,
				uint16_t offset)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	int i;

	for (i = 0; i < 64; i++) {
		uint32_t flexreg, flex;
		flexreg = rd32(hw, TXGBE_FDIRFLEXCFG(i / 4));
		flex = TXGBE_FDIRFLEXCFG_BASE_MAC;
		flex |= TXGBE_FDIRFLEXCFG_OFST(offset / 2);
		flexreg &= ~(TXGBE_FDIRFLEXCFG_ALL(~0UL, i % 4));
		flexreg |= TXGBE_FDIRFLEXCFG_ALL(flex, i % 4);
		wr32(hw, TXGBE_FDIRFLEXCFG(i / 4), flexreg);
	}

	txgbe_flush(hw);
	for (i = 0; i < TXGBE_FDIR_INIT_DONE_POLL; i++) {
		if (rd32(hw, TXGBE_FDIRCTL) &
			TXGBE_FDIRCTL_INITDONE)
			break;
		msec_delay(1);
	}
	return 0;
}

/*
 * txgbe_check_fdir_flex_conf -check if the flex payload and mask configuration
 * arguments are valid
 */
static int
txgbe_set_fdir_flex_conf(struct rte_eth_dev *dev, uint32_t flex)
{
	const struct rte_eth_fdir_flex_conf *conf =
				&dev->data->dev_conf.fdir_conf.flex_conf;
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	struct txgbe_hw_fdir_info *info = TXGBE_DEV_FDIR(dev);
	const struct rte_eth_flex_payload_cfg *flex_cfg;
	const struct rte_eth_fdir_flex_mask *flex_mask;
	uint16_t flexbytes = 0;
	uint16_t i;

	if (conf == NULL) {
		PMD_DRV_LOG(ERR, "NULL pointer.");
		return -EINVAL;
	}

	flex |= TXGBE_FDIRFLEXCFG_DIA;

	for (i = 0; i < conf->nb_payloads; i++) {
		flex_cfg = &conf->flex_set[i];
		if (flex_cfg->type != RTE_ETH_RAW_PAYLOAD) {
			PMD_DRV_LOG(ERR, "unsupported payload type.");
			return -EINVAL;
		}
		if (((flex_cfg->src_offset[0] & 0x1) == 0) &&
		    (flex_cfg->src_offset[1] == flex_cfg->src_offset[0] + 1) &&
		     flex_cfg->src_offset[0] <= TXGBE_MAX_FLX_SOURCE_OFF) {
			flex &= ~TXGBE_FDIRFLEXCFG_OFST_MASK;
			flex |=
			    TXGBE_FDIRFLEXCFG_OFST(flex_cfg->src_offset[0] / 2);
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
		flexbytes = (uint16_t)(((flex_mask->mask[1] << 8) & 0xFF00) |
					((flex_mask->mask[0]) & 0xFF));
		if (flexbytes == UINT16_MAX) {
			flex &= ~TXGBE_FDIRFLEXCFG_DIA;
		} else if (flexbytes != 0) {
		     /* TXGBE_FDIRFLEXCFG_DIA is set by default when set mask */
			PMD_DRV_LOG(ERR, " invalid flexbytes mask arguments.");
			return -EINVAL;
		}
	}

	info->mask.flex_bytes_mask = flexbytes ? UINT16_MAX : 0;
	info->flex_bytes_offset = (uint8_t)(TXGBD_FDIRFLEXCFG_OFST(flex) * 2);

	for (i = 0; i < 64; i++) {
		uint32_t flexreg;
		flexreg = rd32(hw, TXGBE_FDIRFLEXCFG(i / 4));
		flexreg &= ~(TXGBE_FDIRFLEXCFG_ALL(~0UL, i % 4));
		flexreg |= TXGBE_FDIRFLEXCFG_ALL(flex, i % 4);
		wr32(hw, TXGBE_FDIRFLEXCFG(i / 4), flexreg);
	}
	return 0;
}

int
txgbe_fdir_configure(struct rte_eth_dev *dev)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	int err;
	uint32_t fdirctrl, flex, pbsize;
	int i;
	enum rte_fdir_mode mode = dev->data->dev_conf.fdir_conf.mode;

	PMD_INIT_FUNC_TRACE();

	/* supports mac-vlan and tunnel mode */
	if (mode != RTE_FDIR_MODE_SIGNATURE &&
	    mode != RTE_FDIR_MODE_PERFECT)
		return -ENOSYS;

	err = configure_fdir_flags(&dev->data->dev_conf.fdir_conf,
				   &fdirctrl, &flex);
	if (err)
		return err;

	/*
	 * Before enabling Flow Director, the Rx Packet Buffer size
	 * must be reduced.  The new value is the current size minus
	 * flow director memory usage size.
	 */
	pbsize = rd32(hw, TXGBE_PBRXSIZE(0));
	pbsize -= TXGBD_FDIRCTL_BUF_BYTE(fdirctrl);
	wr32(hw, TXGBE_PBRXSIZE(0), pbsize);

	/*
	 * The defaults in the HW for RX PB 1-7 are not zero and so should be
	 * initialized to zero for non DCB mode otherwise actual total RX PB
	 * would be bigger than programmed and filter space would run into
	 * the PB 0 region.
	 */
	for (i = 1; i < 8; i++)
		wr32(hw, TXGBE_PBRXSIZE(i), 0);

	err = txgbe_fdir_store_input_mask(dev);
	if (err < 0) {
		PMD_INIT_LOG(ERR, " Error on setting FD mask");
		return err;
	}

	err = txgbe_fdir_set_input_mask(dev);
	if (err < 0) {
		PMD_INIT_LOG(ERR, " Error on setting FD mask");
		return err;
	}

	err = txgbe_set_fdir_flex_conf(dev, flex);
	if (err < 0) {
		PMD_INIT_LOG(ERR, " Error on setting FD flexible arguments.");
		return err;
	}

	err = txgbe_fdir_enable(hw, fdirctrl);
	if (err < 0) {
		PMD_INIT_LOG(ERR, " Error on enabling FD.");
		return err;
	}
	return 0;
}

/*
 * Note that the bkt_hash field in the txgbe_atr_input structure is also never
 * set.
 *
 * Compute the hashes for SW ATR
 *  @stream: input bitstream to compute the hash on
 *  @key: 32-bit hash key
 **/
static uint32_t
txgbe_atr_compute_hash(struct txgbe_atr_input *atr_input,
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
	__be32 *dword_stream = (__be32 *)atr_input;
	__be32 common_hash_dword = 0;
	u32 hi_hash_dword, lo_hash_dword, flow_pool_ptid;
	u32 hash_result = 0;
	u8 i;

	/* record the flow_vm_vlan bits as they are a key part to the hash */
	flow_pool_ptid = be_to_cpu32(dword_stream[0]);

	/* generate common hash dword */
	for (i = 1; i <= 10; i++)
		common_hash_dword ^= dword_stream[i];

	hi_hash_dword = be_to_cpu32(common_hash_dword);

	/* low dword is word swapped version of common */
	lo_hash_dword = (hi_hash_dword >> 16) | (hi_hash_dword << 16);

	/* apply (Flow ID/VM Pool/Packet Type) bits to hash words */
	hi_hash_dword ^= flow_pool_ptid ^ (flow_pool_ptid >> 16);

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
	lo_hash_dword ^= flow_pool_ptid ^ (flow_pool_ptid << 16);

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
atr_compute_perfect_hash(struct txgbe_atr_input *input,
		enum rte_eth_fdir_pballoc_type pballoc)
{
	uint32_t bucket_hash;

	bucket_hash = txgbe_atr_compute_hash(input,
				TXGBE_ATR_BUCKET_HASH_KEY);
	if (pballoc == RTE_ETH_FDIR_PBALLOC_256K)
		bucket_hash &= PERFECT_BUCKET_256KB_HASH_MASK;
	else if (pballoc == RTE_ETH_FDIR_PBALLOC_128K)
		bucket_hash &= PERFECT_BUCKET_128KB_HASH_MASK;
	else
		bucket_hash &= PERFECT_BUCKET_64KB_HASH_MASK;

	return TXGBE_FDIRPIHASH_BKT(bucket_hash);
}

/**
 * txgbe_fdir_check_cmd_complete - poll to check whether FDIRPICMD is complete
 * @hw: pointer to hardware structure
 */
static inline int
txgbe_fdir_check_cmd_complete(struct txgbe_hw *hw, uint32_t *fdircmd)
{
	int i;

	for (i = 0; i < TXGBE_FDIRCMD_CMD_POLL; i++) {
		*fdircmd = rd32(hw, TXGBE_FDIRPICMD);
		if (!(*fdircmd & TXGBE_FDIRPICMD_OP_MASK))
			return 0;
		rte_delay_us(TXGBE_FDIRCMD_CMD_INTERVAL_US);
	}

	return -ETIMEDOUT;
}

/*
 * Calculate the hash value needed for signature-match filters. In the FreeBSD
 * driver, this is done by the optimised function
 * txgbe_atr_compute_sig_hash_raptor(). However that can't be used here as it
 * doesn't support calculating a hash for an IPv6 filter.
 */
static uint32_t
atr_compute_signature_hash(struct txgbe_atr_input *input,
		enum rte_eth_fdir_pballoc_type pballoc)
{
	uint32_t bucket_hash, sig_hash;

	bucket_hash = txgbe_atr_compute_hash(input,
				TXGBE_ATR_BUCKET_HASH_KEY);
	if (pballoc == RTE_ETH_FDIR_PBALLOC_256K)
		bucket_hash &= SIG_BUCKET_256KB_HASH_MASK;
	else if (pballoc == RTE_ETH_FDIR_PBALLOC_128K)
		bucket_hash &= SIG_BUCKET_128KB_HASH_MASK;
	else
		bucket_hash &= SIG_BUCKET_64KB_HASH_MASK;

	sig_hash = txgbe_atr_compute_hash(input,
				TXGBE_ATR_SIGNATURE_HASH_KEY);

	return TXGBE_FDIRPIHASH_SIG(sig_hash) |
	       TXGBE_FDIRPIHASH_BKT(bucket_hash);
}

/**
 * With the ability to set extra flags in FDIRPICMD register
 * added, and IPv6 support also added. The hash value is also pre-calculated
 * as the pballoc value is needed to do it.
 */
static int
fdir_write_perfect_filter(struct txgbe_hw *hw,
			struct txgbe_atr_input *input, uint8_t queue,
			uint32_t fdircmd, uint32_t fdirhash,
			enum rte_fdir_mode mode)
{
	uint32_t fdirport, fdirflex;
	int err = 0;

	UNREFERENCED_PARAMETER(mode);

	/* record the IPv4 address (little-endian)
	 * can not use wr32.
	 */
	wr32(hw, TXGBE_FDIRPISIP4, be_to_le32(input->src_ip[0]));
	wr32(hw, TXGBE_FDIRPIDIP4, be_to_le32(input->dst_ip[0]));

	/* record source and destination port (little-endian)*/
	fdirport = TXGBE_FDIRPIPORT_DST(be_to_le16(input->dst_port));
	fdirport |= TXGBE_FDIRPIPORT_SRC(be_to_le16(input->src_port));
	wr32(hw, TXGBE_FDIRPIPORT, fdirport);

	/* record pkt_type (little-endian) and flex_bytes(big-endian) */
	fdirflex = TXGBE_FDIRPIFLEX_FLEX(be_to_npu16(input->flex_bytes));
	fdirflex |= TXGBE_FDIRPIFLEX_PTYPE(be_to_le16(input->pkt_type));
	wr32(hw, TXGBE_FDIRPIFLEX, fdirflex);

	/* configure FDIRHASH register */
	fdirhash |= TXGBE_FDIRPIHASH_VLD;
	wr32(hw, TXGBE_FDIRPIHASH, fdirhash);

	/*
	 * flush all previous writes to make certain registers are
	 * programmed prior to issuing the command
	 */
	txgbe_flush(hw);

	/* configure FDIRPICMD register */
	fdircmd |= TXGBE_FDIRPICMD_OP_ADD |
		   TXGBE_FDIRPICMD_UPD |
		   TXGBE_FDIRPICMD_LAST |
		   TXGBE_FDIRPICMD_QPENA;
	fdircmd |= TXGBE_FDIRPICMD_FT(input->flow_type);
	fdircmd |= TXGBE_FDIRPICMD_QP(queue);
	fdircmd |= TXGBE_FDIRPICMD_POOL(input->vm_pool);

	wr32(hw, TXGBE_FDIRPICMD, fdircmd);

	PMD_DRV_LOG(DEBUG, "Rx Queue=%x hash=%x", queue, fdirhash);

	err = txgbe_fdir_check_cmd_complete(hw, &fdircmd);
	if (err < 0)
		PMD_DRV_LOG(ERR, "Timeout writing flow director filter.");

	return err;
}

/**
 * This function supports setting extra fields in the FDIRPICMD register, and
 * removes the code that was verifying the flow_type field. According to the
 * documentation, a flow type of 00 (i.e. not TCP, UDP, or SCTP) is not
 * supported, however it appears to work ok...
 *  Adds a signature hash filter
 *  @hw: pointer to hardware structure
 *  @input: unique input dword
 *  @queue: queue index to direct traffic to
 *  @fdircmd: any extra flags to set in fdircmd register
 *  @fdirhash: pre-calculated hash value for the filter
 **/
static int
fdir_add_signature_filter(struct txgbe_hw *hw,
		struct txgbe_atr_input *input, uint8_t queue, uint32_t fdircmd,
		uint32_t fdirhash)
{
	int err = 0;

	PMD_INIT_FUNC_TRACE();

	/* configure FDIRPICMD register */
	fdircmd |= TXGBE_FDIRPICMD_OP_ADD |
		   TXGBE_FDIRPICMD_UPD |
		   TXGBE_FDIRPICMD_LAST |
		   TXGBE_FDIRPICMD_QPENA;
	fdircmd |= TXGBE_FDIRPICMD_FT(input->flow_type);
	fdircmd |= TXGBE_FDIRPICMD_QP(queue);

	fdirhash |= TXGBE_FDIRPIHASH_VLD;
	wr32(hw, TXGBE_FDIRPIHASH, fdirhash);
	wr32(hw, TXGBE_FDIRPICMD, fdircmd);

	PMD_DRV_LOG(DEBUG, "Rx Queue=%x hash=%x", queue, fdirhash);

	err = txgbe_fdir_check_cmd_complete(hw, &fdircmd);
	if (err < 0)
		PMD_DRV_LOG(ERR, "Timeout writing flow director filter.");

	return err;
}

/*
 * This is modified to take in the hash as a parameter so that
 * it can be used for removing signature and perfect filters.
 */
static int
fdir_erase_filter_raptor(struct txgbe_hw *hw, uint32_t fdirhash)
{
	uint32_t fdircmd = 0;
	int err = 0;

	wr32(hw, TXGBE_FDIRPIHASH, fdirhash);

	/* flush hash to HW */
	txgbe_flush(hw);

	/* Query if filter is present */
	wr32(hw, TXGBE_FDIRPICMD, TXGBE_FDIRPICMD_OP_QRY);

	err = txgbe_fdir_check_cmd_complete(hw, &fdircmd);
	if (err < 0) {
		PMD_INIT_LOG(ERR, "Timeout querying for flow director filter.");
		return err;
	}

	/* if filter exists in hardware then remove it */
	if (fdircmd & TXGBE_FDIRPICMD_VLD) {
		wr32(hw, TXGBE_FDIRPIHASH, fdirhash);
		txgbe_flush(hw);
		wr32(hw, TXGBE_FDIRPICMD, TXGBE_FDIRPICMD_OP_REM);
	}

	err = txgbe_fdir_check_cmd_complete(hw, &fdircmd);
	if (err < 0)
		PMD_INIT_LOG(ERR, "Timeout erasing flow director filter.");

	return err;
}

static inline struct txgbe_fdir_filter *
txgbe_fdir_filter_lookup(struct txgbe_hw_fdir_info *fdir_info,
			 struct txgbe_atr_input *input)
{
	int ret;

	ret = rte_hash_lookup(fdir_info->hash_handle, (const void *)input);
	if (ret < 0)
		return NULL;

	return fdir_info->hash_map[ret];
}

static inline int
txgbe_insert_fdir_filter(struct txgbe_hw_fdir_info *fdir_info,
			 struct txgbe_fdir_filter *fdir_filter)
{
	int ret;

	ret = rte_hash_add_key(fdir_info->hash_handle, &fdir_filter->input);
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
txgbe_remove_fdir_filter(struct txgbe_hw_fdir_info *fdir_info,
			 struct txgbe_atr_input *input)
{
	int ret;
	struct txgbe_fdir_filter *fdir_filter;

	ret = rte_hash_del_key(fdir_info->hash_handle, input);
	if (ret < 0)
		return ret;

	fdir_filter = fdir_info->hash_map[ret];
	fdir_info->hash_map[ret] = NULL;

	TAILQ_REMOVE(&fdir_info->fdir_list, fdir_filter, entries);
	rte_free(fdir_filter);

	return 0;
}

int
txgbe_fdir_filter_program(struct rte_eth_dev *dev,
			  struct txgbe_fdir_rule *rule,
			  bool del,
			  bool update)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	uint32_t fdirhash;
	uint8_t queue;
	bool is_perfect = FALSE;
	int err;
	struct txgbe_hw_fdir_info *info = TXGBE_DEV_FDIR(dev);
	enum rte_fdir_mode fdir_mode = dev->data->dev_conf.fdir_conf.mode;
	struct txgbe_fdir_filter *node;

	if (fdir_mode == RTE_FDIR_MODE_NONE ||
	    fdir_mode != rule->mode)
		return -ENOTSUP;

	if (fdir_mode >= RTE_FDIR_MODE_PERFECT)
		is_perfect = TRUE;

	if (is_perfect) {
		if (rule->input.flow_type & TXGBE_ATR_L3TYPE_IPV6) {
			PMD_DRV_LOG(ERR, "IPv6 is not supported in"
				    " perfect mode!");
			return -ENOTSUP;
		}
		fdirhash = atr_compute_perfect_hash(&rule->input,
				dev->data->dev_conf.fdir_conf.pballoc);
		fdirhash |= TXGBE_FDIRPIHASH_IDX(rule->soft_id);
	} else {
		fdirhash = atr_compute_signature_hash(&rule->input,
				dev->data->dev_conf.fdir_conf.pballoc);
	}

	if (del) {
		err = txgbe_remove_fdir_filter(info, &rule->input);
		if (err < 0) {
			PMD_DRV_LOG(ERR,
				"No such fdir filter to delete %d!", err);
			return err;
		}

		err = fdir_erase_filter_raptor(hw, fdirhash);
		if (err < 0)
			PMD_DRV_LOG(ERR, "Fail to delete FDIR filter!");
		else
			PMD_DRV_LOG(DEBUG, "Success to delete FDIR filter!");
		return err;
	}

	/* add or update an fdir filter*/
	if (rule->fdirflags & TXGBE_FDIRPICMD_DROP) {
		if (!is_perfect) {
			PMD_DRV_LOG(ERR, "Drop option is not supported in"
				    " signature mode.");
			return -EINVAL;
		}
		queue = dev->data->dev_conf.fdir_conf.drop_queue;
	} else if (rule->queue < TXGBE_MAX_RX_QUEUE_NUM) {
		queue = rule->queue;
	} else {
		return -EINVAL;
	}

	node = txgbe_fdir_filter_lookup(info, &rule->input);
	if (node) {
		if (!update) {
			PMD_DRV_LOG(ERR, "Conflict with existing fdir filter!");
			return -EINVAL;
		}
		node->fdirflags = rule->fdirflags;
		node->fdirhash = fdirhash;
		node->queue = queue;
	} else {
		node = rte_zmalloc("txgbe_fdir",
				   sizeof(struct txgbe_fdir_filter), 0);
		if (!node)
			return -ENOMEM;
		rte_memcpy(&node->input, &rule->input,
			   sizeof(struct txgbe_atr_input));
		node->fdirflags = rule->fdirflags;
		node->fdirhash = fdirhash;
		node->queue = queue;

		err = txgbe_insert_fdir_filter(info, node);
		if (err < 0) {
			rte_free(node);
			return err;
		}
	}

	if (is_perfect)
		err = fdir_write_perfect_filter(hw, &node->input,
						node->queue, node->fdirflags,
						node->fdirhash, fdir_mode);
	else
		err = fdir_add_signature_filter(hw, &node->input,
						node->queue, node->fdirflags,
						node->fdirhash);
	if (err < 0) {
		PMD_DRV_LOG(ERR, "Fail to add FDIR filter!");
		txgbe_remove_fdir_filter(info, &rule->input);
	} else {
		PMD_DRV_LOG(DEBUG, "Success to add FDIR filter");
	}

	return err;
}

static int
txgbe_fdir_flush(struct rte_eth_dev *dev)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	struct txgbe_hw_fdir_info *info = TXGBE_DEV_FDIR(dev);
	int ret;

	ret = txgbe_reinit_fdir_tables(hw);
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

/* restore flow director filter */
void
txgbe_fdir_filter_restore(struct rte_eth_dev *dev)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	struct txgbe_hw_fdir_info *fdir_info = TXGBE_DEV_FDIR(dev);
	struct txgbe_fdir_filter *node;
	bool is_perfect = FALSE;
	enum rte_fdir_mode fdir_mode = dev->data->dev_conf.fdir_conf.mode;

	if (fdir_mode >= RTE_FDIR_MODE_PERFECT &&
	    fdir_mode <= RTE_FDIR_MODE_PERFECT_TUNNEL)
		is_perfect = TRUE;

	if (is_perfect) {
		TAILQ_FOREACH(node, &fdir_info->fdir_list, entries) {
			(void)fdir_write_perfect_filter(hw,
							      &node->input,
							      node->queue,
							      node->fdirflags,
							      node->fdirhash,
							      fdir_mode);
		}
	} else {
		TAILQ_FOREACH(node, &fdir_info->fdir_list, entries) {
			(void)fdir_add_signature_filter(hw,
							      &node->input,
							      node->queue,
							      node->fdirflags,
							      node->fdirhash);
		}
	}
}

/* remove all the flow director filters */
int
txgbe_clear_all_fdir_filter(struct rte_eth_dev *dev)
{
	struct txgbe_hw_fdir_info *fdir_info = TXGBE_DEV_FDIR(dev);
	struct txgbe_fdir_filter *fdir_filter;
	struct txgbe_fdir_filter *filter_flag;
	int ret = 0;

	/* flush flow director */
	rte_hash_reset(fdir_info->hash_handle);
	memset(fdir_info->hash_map, 0,
	       sizeof(struct txgbe_fdir_filter *) * TXGBE_MAX_FDIR_FILTER_NUM);
	filter_flag = TAILQ_FIRST(&fdir_info->fdir_list);
	while ((fdir_filter = TAILQ_FIRST(&fdir_info->fdir_list))) {
		TAILQ_REMOVE(&fdir_info->fdir_list,
			     fdir_filter,
			     entries);
		rte_free(fdir_filter);
	}

	if (filter_flag != NULL)
		ret = txgbe_fdir_flush(dev);

	return ret;
}
