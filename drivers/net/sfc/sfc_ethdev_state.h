/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019-2021 Xilinx, Inc.
 * Copyright(c) 2019 Solarflare Communications Inc.
 *
 * This software was jointly developed between OKTET Labs (under contract
 * for Solarflare) and Solarflare Communications, Inc.
 */

#ifndef _SFC_ETHDEV_STATE_H
#define _SFC_ETHDEV_STATE_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * +---------------+
 * | UNINITIALIZED |<-----------+
 * +---------------+		|
 *	|.eth_dev_init		|.eth_dev_uninit
 *	V			|
 * +---------------+------------+
 * |  INITIALIZED  |
 * +---------------+<-----------<---------------+
 *	|.dev_configure		|		|
 *	V			|failed		|
 * +---------------+------------+		|
 * |  CONFIGURING  |				|
 * +---------------+----+			|
 *	|success	|			|
 *	|		|		+---------------+
 *	|		|		|    CLOSING    |
 *	|		|		+---------------+
 *	|		|			^
 *	V		|.dev_configure		|
 * +---------------+----+			|.dev_close
 * |  CONFIGURED   |----------------------------+
 * +---------------+<-----------+
 *	|.dev_start		|
 *	V			|
 * +---------------+		|
 * |   STARTING    |------------^
 * +---------------+ failed	|
 *	|success		|
 *	|		+---------------+
 *	|		|   STOPPING    |
 *	|		+---------------+
 *	|			^
 *	V			|.dev_stop
 * +---------------+------------+
 * |    STARTED    |
 * +---------------+
 */
enum sfc_ethdev_state {
	SFC_ETHDEV_UNINITIALIZED = 0,
	SFC_ETHDEV_INITIALIZED,
	SFC_ETHDEV_CONFIGURING,
	SFC_ETHDEV_CONFIGURED,
	SFC_ETHDEV_CLOSING,
	SFC_ETHDEV_STARTING,
	SFC_ETHDEV_STARTED,
	SFC_ETHDEV_STOPPING,

	SFC_ETHDEV_NSTATES
};

#ifdef __cplusplus
}
#endif

#endif  /* _SFC_ETHDEV_STATE_H */
