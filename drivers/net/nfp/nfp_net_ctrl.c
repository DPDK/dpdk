/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Corigine Systems, Inc.
 * All rights reserved.
 */

#include "nfp_net_ctrl.h"

#include <ethdev_pci.h>
#include <nfp_platform.h>

#include "nfp_logs.h"
#include "nfp_net_common.h"

static void
nfp_net_tlv_caps_reset(struct nfp_net_tlv_caps *caps)
{
	memset(caps, 0, sizeof(*caps));
	caps->mbox_off = NFP_NET_CFG_MBOX_BASE;
	caps->mbox_len = NFP_NET_CFG_MBOX_VAL_MAX_SZ;
}

int
nfp_net_tlv_caps_parse(struct rte_eth_dev *dev)
{
	uint32_t hdr;
	uint8_t *end;
	uint8_t *data;
	uint32_t length;
	uint32_t offset;
	uint32_t tlv_type;
	struct nfp_net_hw *net_hw;
	struct nfp_net_tlv_caps *caps;

	net_hw = dev->data->dev_private;
	caps = &net_hw->tlv_caps;
	nfp_net_tlv_caps_reset(caps);

	data = net_hw->super.ctrl_bar + NFP_NET_CFG_TLV_BASE;
	end = net_hw->super.ctrl_bar + NFP_NET_CFG_BAR_SZ;

	hdr = rte_read32(data);
	if (hdr == 0) {
		PMD_DRV_LOG(INFO, "TLV is empty!");
		return 0;
	}

	for (; ; data += length) {
		offset = data - net_hw->super.ctrl_bar;

		if (data + NFP_NET_CFG_TLV_VALUE > end) {
			PMD_DRV_LOG(ERR, "Reached end of BAR without END TLV");
			return -EINVAL;
		}

		hdr = rte_read32(data);

		length = FIELD_GET(NFP_NET_CFG_TLV_HEADER_LENGTH, hdr);
		if ((length & (NFP_NET_CFG_TLV_LENGTH_INC - 1)) != 0) {
			PMD_DRV_LOG(ERR, "TLV size not multiple of 4B len: %u", length);
			return -EINVAL;
		}

		/* Advance past the header */
		data += NFP_NET_CFG_TLV_VALUE;
		if (data + length > end) {
			PMD_DRV_LOG(ERR, "Oversized TLV offset: %u len: %u",
					offset, length);
			return -EINVAL;
		}

		tlv_type = FIELD_GET(NFP_NET_CFG_TLV_HEADER_TYPE, hdr);

		switch (tlv_type) {
		case NFP_NET_CFG_TLV_TYPE_UNKNOWN:
			PMD_DRV_LOG(ERR, "Unknown TLV at offset: %u", offset);
			return -EINVAL;
		case NFP_NET_CFG_TLV_TYPE_RESERVED:
			break;
		case NFP_NET_CFG_TLV_TYPE_END:
			if (length == 0)
				return 0;

			PMD_DRV_LOG(ERR, "END TLV should be empty, has len: %u", length);
			return -EINVAL;
		case NFP_NET_CFG_TLV_TYPE_MBOX:
			caps->mbox_len = length;

			if (length != 0)
				caps->mbox_off = data - net_hw->super.ctrl_bar;
			else
				caps->mbox_off = 0;
			break;
		case NFP_NET_CFG_TLV_TYPE_MBOX_CMSG_TYPES:
			if (length != 0)
				caps->mbox_cmsg_types = rte_read32(data);
			break;
		default:
			if (FIELD_GET(NFP_NET_CFG_TLV_HEADER_REQUIRED, hdr) == 0)
				break;

			PMD_DRV_LOG(ERR, "Unknown TLV type: %u offset: %u len: %u",
					tlv_type, offset, length);
			return -EINVAL;
		}
	}

	PMD_DRV_LOG(ERR, "Reached end of BAR without END TLV");
	return -EINVAL;
}
