/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2015-2019 Amazon.com, Inc. or its affiliates.
 * All rights reserved.
 */

#ifndef __ENA_PLATFORM_H__
#define __ENA_PLATFORM_H__

#define swap16_to_le(x)		(x)

#define swap32_to_le(x)		(x)

#define swap64_to_le(x)		(x)

#define swap16_from_le(x)       (x)

#define swap32_from_le(x)	(x)

#define swap64_from_le(x)	(x)

#define ena_assert_msg(cond, msg)		\
	do {					\
		if (unlikely(!(cond))) {	\
			rte_log(RTE_LOG_ERR, ena_logtype_driver, \
				"Assert failed on %s:%s:%d: ",	\
				__FILE__, __func__, __LINE__);	\
			rte_panic(msg);		\
		}				\
	} while (0)

#endif /* __ENA_PLATFORM_H__ */
