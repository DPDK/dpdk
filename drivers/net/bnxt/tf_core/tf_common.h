/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Broadcom
 * All rights reserved.
 */

#ifndef _TF_COMMON_H_
#define _TF_COMMON_H_

/* Helper to check the parms */
#define TF_CHECK_PARMS_SESSION(tfp, parms) do {	\
		if ((parms) == NULL || (tfp) == NULL) { \
			TFP_DRV_LOG(ERR, "Invalid Argument(s)\n"); \
			return -EINVAL; \
		} \
		if ((tfp)->session == NULL || \
		    (tfp)->session->core_data == NULL) { \
			TFP_DRV_LOG(ERR, "%s: session error\n", \
				    tf_dir_2_str((parms)->dir)); \
			return -EINVAL; \
		} \
	} while (0)

#define TF_CHECK_PARMS_SESSION_NO_DIR(tfp, parms) do {	\
		if ((parms) == NULL || (tfp) == NULL) { \
			TFP_DRV_LOG(ERR, "Invalid Argument(s)\n"); \
			return -EINVAL; \
		} \
		if ((tfp)->session == NULL || \
		    (tfp)->session->core_data == NULL) { \
			TFP_DRV_LOG(ERR, "Session error\n"); \
			return -EINVAL; \
		} \
	} while (0)

#define TF_CHECK_PARMS(tfp, parms) do {	\
		if ((parms) == NULL || (tfp) == NULL) { \
			TFP_DRV_LOG(ERR, "Invalid Argument(s)\n"); \
			return -EINVAL; \
		} \
	} while (0)

#define TF_CHECK_TFP_SESSION(tfp) do { \
		if ((tfp) == NULL) { \
			TFP_DRV_LOG(ERR, "Invalid Argument(s)\n"); \
			return -EINVAL; \
		} \
		if ((tfp)->session == NULL || \
		    (tfp)->session->core_data == NULL) { \
			TFP_DRV_LOG(ERR, "Session error\n"); \
			return -EINVAL; \
		} \
	} while (0)


#define TF_CHECK_PARMS1(parms) do {					\
		if ((parms) == NULL) {					\
			TFP_DRV_LOG(ERR, "Invalid Argument(s)\n");	\
			return -EINVAL;					\
		}							\
	} while (0)

#define TF_CHECK_PARMS2(parms1, parms2) do {				\
		if ((parms1) == NULL || (parms2) == NULL) {		\
			TFP_DRV_LOG(ERR, "Invalid Argument(s)\n");	\
			return -EINVAL;					\
		}							\
	} while (0)

#define TF_CHECK_PARMS3(parms1, parms2, parms3) do {			\
		if ((parms1) == NULL ||					\
		    (parms2) == NULL ||					\
		    (parms3) == NULL) {					\
			TFP_DRV_LOG(ERR, "Invalid Argument(s)\n");	\
			return -EINVAL;					\
		}							\
	} while (0)

#endif /* _TF_COMMON_H_ */
