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
#define sk_sleep(s) ((s)->sk_sleep)
#else
#define HAVE_SOCKET_WQ
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 7, 0)
#define HAVE_STATIC_SOCK_MAP_FD
#else
#define kni_sock_map_fd(s) sock_map_fd(s, 0)
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 9, 0)
#define HAVE_CHANGE_CARRIER_CB
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 14, 0)
#define ether_addr_copy(dst, src) memcpy(dst, src, ETH_ALEN)
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0)
#define HAVE_IOV_ITER_MSGHDR
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 1, 0)
#define HAVE_KIOCB_MSG_PARAM
#define HAVE_REBUILD_HEADER
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 2, 0)
#define HAVE_SK_ALLOC_KERN_PARAM
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 7, 0)
#define HAVE_TRANS_START_HELPER
#endif

/*
 * KNI uses NET_NAME_UNKNOWN macro to select correct version of alloc_netdev()
 * For old kernels just backported the commit that enables the macro
 * (685343fc3ba6) but still uses old API, it is required to undefine macro to
 * select correct version of API, this is safe since KNI doesn't use the value.
 * This fix is specific to RedHat/CentOS kernels.
 */
#if (defined(RHEL_RELEASE_CODE) && \
	(RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(6, 8)) && \
	(LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 34)))
#undef NET_NAME_UNKNOWN
#endif
