/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
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

#include "ixgbe_logs.h"
#include "ixgbe/ixgbe_api.h"
#include "ixgbe/ixgbe_common.h"
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

/**
 * This function is based on ixgbe_fdir_enable_82599() in ixgbe/ixgbe_82599.c.
 * It adds extra configuration of fdirctrl that is common for all filter types.
 *
 *  Initialize Flow Director control registers
 *  @hw: pointer to hardware structure
 *  @fdirctrl: value to write to flow director control register
 **/
static void fdir_enable_82599(struct ixgbe_hw *hw, u32 fdirctrl)
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

	if (i >= IXGBE_FDIR_INIT_DONE_POLL)
		PMD_INIT_LOG(WARNING, "Flow Director poll time exceeded!\n");
}

/*
 * Set appropriate bits in fdirctrl for: variable reporting levels, moving
 * flexbytes matching field, and drop queue (only for perfect matching mode).
 */
static int
configure_fdir_flags(struct rte_fdir_conf *conf, uint32_t *fdirctrl)
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

	*fdirctrl |= (conf->flexbytes_offset << IXGBE_FDIRCTRL_FLEX_SHIFT);

	if (conf->mode == RTE_FDIR_MODE_PERFECT) {
		*fdirctrl |= IXGBE_FDIRCTRL_PERFECT_MATCH;
		*fdirctrl |= (conf->drop_queue << IXGBE_FDIRCTRL_DROP_Q_SHIFT);
	}

	return 0;
}

int
ixgbe_fdir_configure(struct rte_eth_dev *dev)
{
	struct ixgbe_hw *hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	int err;
	uint32_t fdirctrl, pbsize;
	int i;

	PMD_INIT_FUNC_TRACE();

	if (hw->mac.type != ixgbe_mac_82599EB && hw->mac.type !=ixgbe_mac_X540)
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
	 * intialized to zero for non DCB mode otherwise actual total RX PB
	 * would be bigger than programmed and filter space would run into
	 * the PB 0 region.
	 */
	for (i = 1; i < 8; i++)
		IXGBE_WRITE_REG(hw, IXGBE_RXPBSIZE(i), 0);

	fdir_enable_82599(hw, fdirctrl);
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
static u32
ixgbe_atr_compute_hash_82599(union ixgbe_atr_input *atr_input,
				 u32 key)
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
	 *    if(key[0])
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
	if (key & 0x0001) hash_result ^= lo_hash_dword;
	if (key & 0x00010000) hash_result ^= hi_hash_dword;

	/*
	 * apply flow ID/VM pool/VLAN ID bits to lo hash dword, we had to
	 * delay this because bit 0 of the stream should not be processed
	 * so we do not add the vlan until after bit 0 was processed
	 */
	lo_hash_dword ^= flow_vm_vlan ^ (flow_vm_vlan << 16);


	/* process the remaining 30 bits in the key 2 bits at a time */
	for (i = 15; i; i-- ) {
		if (key & (0x0001 << i)) hash_result ^= lo_hash_dword >> i;
		if (key & (0x00010000 << i)) hash_result ^= hi_hash_dword >> i;
	}

	return hash_result;
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

/**
 * This function is based on ixgbe_atr_add_signature_filter_82599() in
 * ixgbe/ixgbe_82599.c, but uses a pre-calculated hash value. It also supports
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
static void
fdir_add_signature_filter_82599(struct ixgbe_hw *hw,
		union ixgbe_atr_input *input, u8 queue, u32 fdircmd,
		u32 fdirhash)
{
	u64  fdirhashcmd;

	PMD_INIT_FUNC_TRACE();

	/* configure FDIRCMD register */
	fdircmd |= IXGBE_FDIRCMD_CMD_ADD_FLOW |
	          IXGBE_FDIRCMD_LAST | IXGBE_FDIRCMD_QUEUE_EN;
	fdircmd |= input->formatted.flow_type << IXGBE_FDIRCMD_FLOW_TYPE_SHIFT;
	fdircmd |= (u32)queue << IXGBE_FDIRCMD_RX_QUEUE_SHIFT;

	/*
	 * The lower 32-bits of fdirhashcmd is for FDIRHASH, the upper 32-bits
	 * is for FDIRCMD.  Then do a 64-bit register write from FDIRHASH.
	 */
	fdirhashcmd = (u64)fdircmd << 32;
	fdirhashcmd |= fdirhash;
	IXGBE_WRITE_REG64(hw, IXGBE_FDIRHASH, fdirhashcmd);

	PMD_INIT_LOG(DEBUG, "Tx Queue=%x hash=%x\n", queue, (u32)fdirhashcmd);
}

/*
 * Convert DPDK rte_fdir_filter struct to ixgbe_atr_input union that is used
 * by the IXGBE driver code.
 */
static int
fdir_filter_to_atr_input(struct rte_fdir_filter *fdir_filter,
		union ixgbe_atr_input *input)
{
	if ((fdir_filter->l4type == RTE_FDIR_L4TYPE_SCTP ||
			fdir_filter->l4type == RTE_FDIR_L4TYPE_NONE) &&
			(fdir_filter->port_src || fdir_filter->port_dst)) {
		PMD_INIT_LOG(ERR, "Invalid fdir_filter");
		return -EINVAL;
	}

	memset(input, 0, sizeof(*input));

	input->formatted.vlan_id = fdir_filter->vlan_id;
	input->formatted.src_port = fdir_filter->port_src;
	input->formatted.dst_port = fdir_filter->port_dst;
	input->formatted.flex_bytes = fdir_filter->flex_bytes;

	switch (fdir_filter->l4type) {
	case RTE_FDIR_L4TYPE_TCP:
		input->formatted.flow_type = IXGBE_ATR_FLOW_TYPE_TCPV4;
		break;
	case RTE_FDIR_L4TYPE_UDP:
		input->formatted.flow_type = IXGBE_ATR_FLOW_TYPE_UDPV4;
		break;
	case RTE_FDIR_L4TYPE_SCTP:
		input->formatted.flow_type = IXGBE_ATR_FLOW_TYPE_SCTPV4;
		break;
	case RTE_FDIR_L4TYPE_NONE:
		input->formatted.flow_type = IXGBE_ATR_FLOW_TYPE_IPV4;
		break;
	default:
		PMD_INIT_LOG(ERR, " Error on l4type input");
		return -EINVAL;
	}

	if (fdir_filter->iptype == RTE_FDIR_IPTYPE_IPV6) {
		input->formatted.flow_type |= IXGBE_ATR_L4TYPE_IPV6_MASK;

		input->formatted.src_ip[0] = fdir_filter->ip_src.ipv6_addr[0];
		input->formatted.src_ip[1] = fdir_filter->ip_src.ipv6_addr[1];
		input->formatted.src_ip[2] = fdir_filter->ip_src.ipv6_addr[2];
		input->formatted.src_ip[3] = fdir_filter->ip_src.ipv6_addr[3];

		input->formatted.dst_ip[0] = fdir_filter->ip_dst.ipv6_addr[0];
		input->formatted.dst_ip[1] = fdir_filter->ip_dst.ipv6_addr[1];
		input->formatted.dst_ip[2] = fdir_filter->ip_dst.ipv6_addr[2];
		input->formatted.dst_ip[3] = fdir_filter->ip_dst.ipv6_addr[3];

	} else {
		input->formatted.src_ip[0] = fdir_filter->ip_src.ipv4_addr;
		input->formatted.dst_ip[0] = fdir_filter->ip_dst.ipv4_addr;
	}

	return 0;
}

/*
 * Adds or updates a signature filter.
 *
 * dev: ethernet device to add filter to
 * fdir_filter: filter details
 * queue: queue index to direct traffic to
 * update: 0 to add a new filter, otherwise update existing.
 */
static int
fdir_add_update_signature_filter(struct rte_eth_dev *dev,
		struct rte_fdir_filter *fdir_filter, uint8_t queue, int update)
{
	struct ixgbe_hw *hw= IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint32_t fdircmd_flags = (update) ? IXGBE_FDIRCMD_FILTER_UPDATE : 0;
	uint32_t fdirhash;
	union ixgbe_atr_input input;
	int err;

	if (hw->mac.type != ixgbe_mac_82599EB && hw->mac.type !=ixgbe_mac_X540)
		return -ENOSYS;

	err = fdir_filter_to_atr_input(fdir_filter, &input);
	if (err)
		return err;

	fdirhash = atr_compute_sig_hash_82599(&input,
			dev->data->dev_conf.fdir_conf.pballoc);
	fdir_add_signature_filter_82599(hw, &input, queue, fdircmd_flags,
			fdirhash);
	return 0;
}

int
ixgbe_fdir_add_signature_filter(struct rte_eth_dev *dev,
		struct rte_fdir_filter *fdir_filter, uint8_t queue)
{
	PMD_INIT_FUNC_TRACE();
	return fdir_add_update_signature_filter(dev, fdir_filter, queue, 0);
}

int
ixgbe_fdir_update_signature_filter(struct rte_eth_dev *dev,
		struct rte_fdir_filter *fdir_filter, uint8_t queue)
{
	PMD_INIT_FUNC_TRACE();
	return fdir_add_update_signature_filter(dev, fdir_filter, queue, 1);
}

/*
 * This is based on ixgbe_fdir_erase_perfect_filter_82599() in
 * ixgbe/ixgbe_82599.c. It is modified to take in the hash as a parameter so
 * that it can be used for removing signature and perfect filters.
 */
static s32
fdir_erase_filter_82599(struct ixgbe_hw *hw,
	__rte_unused union ixgbe_atr_input *input, uint32_t fdirhash)
{
	u32 fdircmd = 0;
	u32 retry_count;
	s32 err = 0;

	IXGBE_WRITE_REG(hw, IXGBE_FDIRHASH, fdirhash);

	/* flush hash to HW */
	IXGBE_WRITE_FLUSH(hw);

	/* Query if filter is present */
	IXGBE_WRITE_REG(hw, IXGBE_FDIRCMD, IXGBE_FDIRCMD_CMD_QUERY_REM_FILT);

	for (retry_count = 10; retry_count; retry_count--) {
		/* allow 10us for query to process */
		usec_delay(10);
		/* verify query completed successfully */
		fdircmd = IXGBE_READ_REG(hw, IXGBE_FDIRCMD);
		if (!(fdircmd & IXGBE_FDIRCMD_CMD_MASK))
			break;
	}

	if (!retry_count) {
		PMD_INIT_LOG(ERR, "Timeout querying for flow director filter");
		err = -EIO;
	}

	/* if filter exists in hardware then remove it */
	if (fdircmd & IXGBE_FDIRCMD_FILTER_VALID) {
		IXGBE_WRITE_REG(hw, IXGBE_FDIRHASH, fdirhash);
		IXGBE_WRITE_FLUSH(hw);
		IXGBE_WRITE_REG(hw, IXGBE_FDIRCMD,
				IXGBE_FDIRCMD_CMD_REMOVE_FLOW);
	}

	return err;
}

int
ixgbe_fdir_remove_signature_filter(struct rte_eth_dev *dev,
		struct rte_fdir_filter *fdir_filter)
{
	struct ixgbe_hw *hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	union ixgbe_atr_input input;
	int err;

	PMD_INIT_FUNC_TRACE();

	if (hw->mac.type != ixgbe_mac_82599EB && hw->mac.type !=ixgbe_mac_X540)
		return -ENOSYS;

	err = fdir_filter_to_atr_input(fdir_filter, &input);
	if (err)
		return err;

	return fdir_erase_filter_82599(hw, &input,
			atr_compute_sig_hash_82599(&input,
			dev->data->dev_conf.fdir_conf.pballoc));
}

/**
 * Reverse the bits in FDIR registers that store 2 x 16 bit masks.
 *
 *  @hi_dword: Bits 31:16 mask to be bit swapped.
 *  @lo_dword: Bits 15:0  mask to be bit swapped.
 *
 *  Flow director uses several registers to store 2 x 16 bit masks with the
 *  bits reversed such as FDIRTCPM, FDIRUDPM and FDIRIP6M. The LS bit of the
 *  mask affects the MS bit/byte of the target. This function reverses the
 *  bits in these masks.
 *  **/
static uint32_t
reverse_fdir_bitmasks(uint16_t hi_dword, uint16_t lo_dword)
{
	u32 mask = hi_dword << 16;
	mask |= lo_dword;
	mask = ((mask & 0x55555555) << 1) | ((mask & 0xAAAAAAAA) >> 1);
	mask = ((mask & 0x33333333) << 2) | ((mask & 0xCCCCCCCC) >> 2);
	mask = ((mask & 0x0F0F0F0F) << 4) | ((mask & 0xF0F0F0F0) >> 4);
	return ((mask & 0x00FF00FF) << 8) | ((mask & 0xFF00FF00) >> 8);
}

/*
 * This macro exists in ixgbe/ixgbe_82599.c, however in that file it reverses
 * the bytes, and then reverses them again. So here it does nothing.
 */
#define IXGBE_WRITE_REG_BE32 IXGBE_WRITE_REG

/*
 * This is based on ixgbe_fdir_set_input_mask_82599() in ixgbe/ixgbe_82599.c,
 * but makes use of the rte_fdir_masks structure to see which bits to set.
 */
static int
fdir_set_input_mask_82599(struct ixgbe_hw *hw,
		struct rte_fdir_masks *input_mask)
{
	/* mask VM pool since it is currently not supported */
	u32 fdirm = IXGBE_FDIRM_POOL;
	u32 fdirtcpm;  /* TCP source and destination port masks. */
	u32 fdiripv6m; /* IPv6 source and destination masks. */

	PMD_INIT_FUNC_TRACE();

	/*
	 * Program the relevant mask registers.  If src/dst_port or src/dst_addr
	 * are zero, then assume a full mask for that field. Also assume that
	 * a VLAN of 0 is unspecified, so mask that out as well.  L4type
	 * cannot be masked out in this implementation.
	 */
	if (input_mask->only_ip_flow) {
		/* use the L4 protocol mask for raw IPv4/IPv6 traffic */
		fdirm |= IXGBE_FDIRM_L4P;
		if (input_mask->dst_port_mask || input_mask->src_port_mask) {
			PMD_INIT_LOG(ERR, " Error on src/dst port mask\n");
			return -EINVAL;
		}
	}

	if (!input_mask->comp_ipv6_dst)
		/* mask DIPV6 */
		fdirm |= IXGBE_FDIRM_DIPv6;

	if (!input_mask->vlan_id)
		/* mask VLAN ID*/
		fdirm |= IXGBE_FDIRM_VLANID;

	if (!input_mask->vlan_prio)
		/* mask VLAN priority */
		fdirm |= IXGBE_FDIRM_VLANP;

	if (!input_mask->flexbytes)
		/* Mask Flex Bytes */
		fdirm |= IXGBE_FDIRM_FLEX;

	IXGBE_WRITE_REG(hw, IXGBE_FDIRM, fdirm);

	/* store the TCP/UDP port masks, bit reversed from port layout */
	fdirtcpm = reverse_fdir_bitmasks(input_mask->dst_port_mask,
	                                 input_mask->src_port_mask);

	/* write both the same so that UDP and TCP use the same mask */
	IXGBE_WRITE_REG(hw, IXGBE_FDIRTCPM, ~fdirtcpm);
	IXGBE_WRITE_REG(hw, IXGBE_FDIRUDPM, ~fdirtcpm);

	if (!input_mask->set_ipv6_mask) {
		/* Store source and destination IPv4 masks (big-endian) */
		IXGBE_WRITE_REG_BE32(hw, IXGBE_FDIRSIP4M,
				IXGBE_NTOHL(~input_mask->src_ipv4_mask));
		IXGBE_WRITE_REG_BE32(hw, IXGBE_FDIRDIP4M,
				IXGBE_NTOHL(~input_mask->dst_ipv4_mask));
	}
	else {
		/* Store source and destination IPv6 masks (bit reversed) */
		fdiripv6m = reverse_fdir_bitmasks(input_mask->dst_ipv6_mask,
		                                  input_mask->src_ipv6_mask);

		IXGBE_WRITE_REG(hw, IXGBE_FDIRIP6M, ~fdiripv6m);
	}

	return IXGBE_SUCCESS;
}

int
ixgbe_fdir_set_masks(struct rte_eth_dev *dev, struct rte_fdir_masks *fdir_masks)
{
	struct ixgbe_hw *hw;
	int err;

	PMD_INIT_FUNC_TRACE();

	hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	if (hw->mac.type != ixgbe_mac_82599EB && hw->mac.type !=ixgbe_mac_X540)
		return -ENOSYS;

	err = ixgbe_reinit_fdir_tables_82599(hw);
	if (err) {
		PMD_INIT_LOG(ERR, "reinit of fdir tables failed");
		return -EIO;
	}

	return fdir_set_input_mask_82599(hw, fdir_masks);
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

/*
 * This is based on ixgbe_fdir_write_perfect_filter_82599() in
 * ixgbe/ixgbe_82599.c, with the ability to set extra flags in FDIRCMD register
 * added, and IPv6 support also added. The hash value is also pre-calculated
 * as the pballoc value is needed to do it.
 */
static void
fdir_write_perfect_filter_82599(struct ixgbe_hw *hw, union ixgbe_atr_input *input,
		uint16_t soft_id, uint8_t queue, uint32_t fdircmd,
		uint32_t fdirhash)
{
	u32 fdirport, fdirvlan;

	/* record the source address (big-endian) */
	if (input->formatted.flow_type & IXGBE_ATR_L4TYPE_IPV6_MASK) {
		IXGBE_WRITE_REG_BE32(hw, IXGBE_FDIRSIPv6(0), input->formatted.src_ip[0]);
		IXGBE_WRITE_REG_BE32(hw, IXGBE_FDIRSIPv6(1), input->formatted.src_ip[1]);
		IXGBE_WRITE_REG_BE32(hw, IXGBE_FDIRSIPv6(2), input->formatted.src_ip[2]);
		IXGBE_WRITE_REG_BE32(hw, IXGBE_FDIRIPSA, input->formatted.src_ip[3]);
	}
	else {
		IXGBE_WRITE_REG_BE32(hw, IXGBE_FDIRIPSA, input->formatted.src_ip[0]);
	}

	/* record the first 32 bits of the destination address (big-endian) */
	IXGBE_WRITE_REG_BE32(hw, IXGBE_FDIRIPDA, input->formatted.dst_ip[0]);

	/* record source and destination port (little-endian)*/
	fdirport = IXGBE_NTOHS(input->formatted.dst_port);
	fdirport <<= IXGBE_FDIRPORT_DESTINATION_SHIFT;
	fdirport |= IXGBE_NTOHS(input->formatted.src_port);
	IXGBE_WRITE_REG(hw, IXGBE_FDIRPORT, fdirport);

	/* record vlan (little-endian) and flex_bytes(big-endian) */
	fdirvlan = input->formatted.flex_bytes;
	fdirvlan <<= IXGBE_FDIRVLAN_FLEX_SHIFT;
	fdirvlan |= IXGBE_NTOHS(input->formatted.vlan_id);
	IXGBE_WRITE_REG(hw, IXGBE_FDIRVLAN, fdirvlan);

	/* configure FDIRHASH register */
	fdirhash |= soft_id << IXGBE_FDIRHASH_SIG_SW_INDEX_SHIFT;
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
	fdircmd |= (u32)queue << IXGBE_FDIRCMD_RX_QUEUE_SHIFT;
	fdircmd |= (u32)input->formatted.vm_pool << IXGBE_FDIRCMD_VT_POOL_SHIFT;

	IXGBE_WRITE_REG(hw, IXGBE_FDIRCMD, fdircmd);
}

/*
 * Adds or updates a perfect filter.
 *
 * dev: ethernet device to add filter to
 * fdir_filter: filter details
 * soft_id: software index for the filters
 * queue: queue index to direct traffic to
 * drop: non-zero if packets should be sent to the drop queue
 * update: 0 to add a new filter, otherwise update existing.
 */
static int
fdir_add_update_perfect_filter(struct rte_eth_dev *dev,
		struct rte_fdir_filter *fdir_filter, uint16_t soft_id,
		uint8_t queue, int drop, int update)
{
	struct ixgbe_hw *hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint32_t fdircmd_flags = (update) ? IXGBE_FDIRCMD_FILTER_UPDATE : 0;
	uint32_t fdirhash;
	union ixgbe_atr_input input;
	int err;

	if (hw->mac.type != ixgbe_mac_82599EB && hw->mac.type !=ixgbe_mac_X540)
		return -ENOSYS;

	err = fdir_filter_to_atr_input(fdir_filter, &input);
	if (err)
		return err;

	if (drop) {
		queue = dev->data->dev_conf.fdir_conf.drop_queue;
		fdircmd_flags |= IXGBE_FDIRCMD_DROP;
	}

	fdirhash = atr_compute_perfect_hash_82599(&input,
			dev->data->dev_conf.fdir_conf.pballoc);

	fdir_write_perfect_filter_82599(hw, &input, soft_id, queue,
			fdircmd_flags, fdirhash);
	return 0;
}

int
ixgbe_fdir_add_perfect_filter(struct rte_eth_dev *dev,
		struct rte_fdir_filter *fdir_filter, uint16_t soft_id,
		uint8_t queue, uint8_t drop)
{
	PMD_INIT_FUNC_TRACE();
	return fdir_add_update_perfect_filter(dev, fdir_filter, soft_id, queue,
			drop, 0);
}

int
ixgbe_fdir_update_perfect_filter(struct rte_eth_dev *dev,
		struct rte_fdir_filter *fdir_filter, uint16_t soft_id,
		uint8_t queue, uint8_t drop)
{
	PMD_INIT_FUNC_TRACE();
	return fdir_add_update_perfect_filter(dev, fdir_filter, soft_id, queue,
			drop, 1);
}

int
ixgbe_fdir_remove_perfect_filter(struct rte_eth_dev *dev,
		struct rte_fdir_filter *fdir_filter,
		uint16_t soft_id)
{
	struct ixgbe_hw *hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	union ixgbe_atr_input input;
	uint32_t fdirhash;
	int err;

	PMD_INIT_FUNC_TRACE();

	if (hw->mac.type != ixgbe_mac_82599EB && hw->mac.type !=ixgbe_mac_X540)
		return -ENOSYS;

	err = fdir_filter_to_atr_input(fdir_filter, &input);
	if (err)
		return err;

	/* configure FDIRHASH register */
	fdirhash = atr_compute_perfect_hash_82599(&input,
			dev->data->dev_conf.fdir_conf.pballoc);
	fdirhash |= soft_id << IXGBE_FDIRHASH_SIG_SW_INDEX_SHIFT;

	return fdir_erase_filter_82599(hw, &input, fdirhash);
}

void
ixgbe_fdir_info_get(struct rte_eth_dev *dev, struct rte_eth_fdir *fdir)
{
	struct ixgbe_hw *hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct ixgbe_hw_fdir_info *info =
			IXGBE_DEV_PRIVATE_TO_FDIR_INFO(dev->data->dev_private);
	uint32_t reg;

	if (hw->mac.type != ixgbe_mac_82599EB && hw->mac.type !=ixgbe_mac_X540)
		return;

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
	fdir->collision = info->collision;
	fdir->free = info->free;
	fdir->maxhash = info->maxhash;
	fdir->maxlen = info->maxlen;
	fdir->remove = info->remove;
	fdir->add = info->add;
	fdir->f_remove = info->f_remove;
	fdir->f_add = info->f_add;
}
