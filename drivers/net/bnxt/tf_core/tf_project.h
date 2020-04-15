/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Broadcom
 * All rights reserved.
 */

#ifndef _TF_PROJECT_H_
#define _TF_PROJECT_H_

/* Wh+ support enabled */
#ifndef TF_SUPPORT_P4
#define TF_SUPPORT_P4 1
#endif

/* Shadow DB Support */
#ifndef TF_SHADOW
#define TF_SHADOW 0
#endif

/* Shared memory for session */
#ifndef TF_SHARED
#define TF_SHARED 0
#endif

#endif /* _TF_PROJECT_H_ */
