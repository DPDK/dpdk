/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020-2023 Broadcom
 * All rights reserved.
 */

#ifndef _TF_COMMON_H_
#define _TF_COMMON_H_

/* Helpers to performs parameter check */

/**
 * Checks 1 parameter against NULL.
 */
#define TF_CHECK_PARMS1(parms) do {					\
		if ((parms) == NULL) {					\
			TFP_DRV_LOG(ERR, "Invalid Argument(s)\n");	\
			return -EINVAL;					\
		}							\
	} while (0)

/**
 * Checks 2 parameters against NULL.
 */
#define TF_CHECK_PARMS2(parms1, parms2) do {				\
		if ((parms1) == NULL || (parms2) == NULL) {		\
			TFP_DRV_LOG(ERR, "Invalid Argument(s)\n");	\
			return -EINVAL;					\
		}							\
	} while (0)

/**
 * Checks 3 parameters against NULL.
 */
#define TF_CHECK_PARMS3(parms1, parms2, parms3) do {			\
		if ((parms1) == NULL ||					\
		    (parms2) == NULL ||					\
		    (parms3) == NULL) {					\
			TFP_DRV_LOG(ERR, "Invalid Argument(s)\n");	\
			return -EINVAL;					\
		}							\
	} while (0)
#endif /* _TF_COMMON_H_ */
