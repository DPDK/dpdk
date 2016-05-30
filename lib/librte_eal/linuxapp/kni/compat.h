/*
 * Minimal wrappers to allow compiling kni on older kernels.
 */

#ifndef RHEL_RELEASE_VERSION
#define RHEL_RELEASE_VERSION(a, b) (((a) << 8) + (b))
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 39) && \
	(!(defined(RHEL_RELEASE_CODE) && \
	   RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(6, 4)))

#define kstrtoul strict_strtoul

#endif /* < 2.6.39 */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 33)
#define HAVE_SIMPLIFIED_PERNET_OPERATIONS
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 35)
#define sk_sleep(s) (s)->sk_sleep
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 9, 0)
#define HAVE_CHANGE_CARRIER_CB
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0)
#define HAVE_IOV_ITER_MSGHDR
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 1, 0)
#define HAVE_KIOCB_MSG_PARAM
#define HAVE_REBUILD_HEADER
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 7, 0)
#define HAVE_TRANS_START_HELPER
#endif
