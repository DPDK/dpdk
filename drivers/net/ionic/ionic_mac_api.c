/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 * Copyright(c) 2018-2019 Pensando Systems, Inc. All rights reserved.
 */

#include <stdbool.h>

#include "ionic_mac_api.h"

int32_t
ionic_init_mac(struct ionic_hw *hw)
{
	int err = 0;

	IONIC_PRINT_CALL();

	/*
	 * Set the mac type
	 */
	ionic_set_mac_type(hw);

	switch (hw->mac.type) {
	case IONIC_MAC_CAPRI:
		break;
	default:
		err = -EINVAL;
		break;
	}

	return err;
}

int32_t
ionic_set_mac_type(struct ionic_hw *hw)
{
	int err = 0;

	IONIC_PRINT_CALL();

	if (hw->vendor_id != IONIC_PENSANDO_VENDOR_ID) {
		IONIC_PRINT(ERR, "Unsupported vendor id: %" PRIx32 "",
			hw->vendor_id);
		return -EINVAL;
	}

	switch (hw->device_id) {
	case IONIC_DEV_ID_ETH_PF:
	case IONIC_DEV_ID_ETH_VF:
	case IONIC_DEV_ID_ETH_MGMT:
		hw->mac.type = IONIC_MAC_CAPRI;
		break;
	default:
		err = -EINVAL;
		IONIC_PRINT(ERR, "Unsupported device id: %" PRIx32 "",
			hw->device_id);
		break;
	}

	IONIC_PRINT(INFO, "Mac: %d (%d)",
		hw->mac.type, err);

	return err;
}

