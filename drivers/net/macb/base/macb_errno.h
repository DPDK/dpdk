/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Phytium Technology Co., Ltd.
 */

#ifndef _MACB_ERRNO_H_
#define _MACB_ERRNO_H_

#include <errno.h>

#ifndef EPERM
#define EPERM		1
#endif /* EPERM */
#ifndef ENOENT
#define ENOENT		2
#endif /* ENOENT */
#ifndef EIO
#define EIO		5
#endif /* EIO */
#ifndef ENXIO
#define ENXIO		6
#endif /* ENXIO */
#ifndef ENOMEM
#define ENOMEM		12
#endif /* ENOMEM */
#ifndef EACCES
#define EACCES		13
#endif /* EACCES */
#ifndef EFAULT
#define EFAULT		14
#endif /* EFAULT */
#ifndef EBUSY
#define EBUSY		16
#endif /* EBUSY */
#ifndef EEXIST
#define EEXIST		17
#endif /* EEXIST */
#ifndef ENODEV
#define ENODEV		19
#endif /* ENODEV */
#ifndef EINVAL
#define EINVAL		22
#endif /* EINVAL */
#ifndef ENOSPC
#define ENOSPC		28
#endif /* ENOSPC */
#ifndef ENOMSG
#define ENOMSG		42
#endif /* ENOMSG */

#ifndef ENOBUFS
#define ENOBUFS		105
#endif /* ENOBUFS */

#ifndef ENOTSUP
#define ENOTSUP		252
#endif /* ENOTSUP */

#endif /* _MACB_ERRNO_H_ */
