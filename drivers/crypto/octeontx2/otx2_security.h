/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2020 Marvell International Ltd.
 */

#ifndef __OTX2_SECURITY_H__
#define __OTX2_SECURITY_H__

#include "otx2_ethdev_sec.h"

union otx2_sec_session_ipsec {
	struct otx2_sec_session_ipsec_ip ip;
};

struct otx2_sec_session {
	union otx2_sec_session_ipsec ipsec;
	void *userdata;
	/**< Userdata registered by the application */
} __rte_cache_aligned;

#endif /* __OTX2_SECURITY_H__ */
