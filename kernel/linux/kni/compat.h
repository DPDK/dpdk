/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Minimal wrappers to allow compiling kni on older kernels.
 */

#include <linux/version.h>

#ifndef RHEL_RELEASE_VERSION
#define RHEL_RELEASE_VERSION(a, b) (((a) << 8) + (b))
#endif

/* SuSE version macro is the same as Linux kernel version */
#ifndef SLE_VERSION
#define SLE_VERSION(a, b, c) KERNEL_VERSION(a, b, c)
#endif
#ifdef CONFIG_SUSE_KERNEL
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 57))
/* SLES12SP3 is at least 4.4.57+ based */
#define SLE_VERSION_CODE SLE_VERSION(12, 3, 0)
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 12, 28))
/* SLES12 is at least 3.12.28+ based */
#define SLE_VERSION_CODE SLE_VERSION(12, 0, 0)
#elif ((LINUX_VERSION_CODE >= KERNEL_VERSION(3, 0, 61)) && \
	(LINUX_VERSION_CODE < KERNEL_VERSION(3, 1, 0)))
/* SLES11 SP3 is at least 3.0.61+ based */
#define SLE_VERSION_CODE SLE_VERSION(11, 3, 0)
#elif (LINUX_VERSION_CODE == KERNEL_VERSION(2, 6, 32))
/* SLES11 SP1 is 2.6.32 based */
#define SLE_VERSION_CODE SLE_VERSION(11, 1, 0)
#elif (LINUX_VERSION_CODE == KERNEL_VERSION(2, 6, 27))
/* SLES11 GA is 2.6.27 based */
#define SLE_VERSION_CODE SLE_VERSION(11, 0, 0)
#endif /* LINUX_VERSION_CODE == KERNEL_VERSION(x,y,z) */
#endif /* CONFIG_SUSE_KERNEL */
#ifndef SLE_VERSION_CODE
#define SLE_VERSION_CODE 0
#endif /* SLE_VERSION_CODE */


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

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 7, 0) || \
	(defined(RHEL_RELEASE_CODE) && \
	 RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(7, 4)) || \
	(SLE_VERSION_CODE && SLE_VERSION_CODE == SLE_VERSION(12, 3, 0))
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

/*
 * RHEL has two different version with different kernel version:
 * 3.10 is for AMD, Intel, IBM POWER7 and POWER8;
 * 4.14 is for ARM and IBM POWER9
 */
#if (defined(RHEL_RELEASE_CODE) && \
	(RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(7, 5)) && \
	(RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(8, 0)) && \
	(LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)))
#define ndo_change_mtu ndo_change_mtu_rh74
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 10, 0)
#define HAVE_MAX_MTU_PARAM
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
#define HAVE_SIGNAL_FUNCTIONS_OWN_HEADER
#endif

/*
 * iova to kva mapping support can be provided since 4.6.0, but required
 * kernel version increased to >= 4.10.0 because of the updates in
 * get_user_pages_remote() kernel API
 */
#if KERNEL_VERSION(4, 10, 0) <= LINUX_VERSION_CODE
#define HAVE_IOVA_TO_KVA_MAPPING_SUPPORT
#endif

#if KERNEL_VERSION(5, 6, 0) <= LINUX_VERSION_CODE || \
	(defined(RHEL_RELEASE_CODE) && \
	 RHEL_RELEASE_VERSION(8, 3) <= RHEL_RELEASE_CODE) || \
	 (defined(CONFIG_SUSE_KERNEL) && defined(HAVE_ARG_TX_QUEUE))
#define HAVE_TX_TIMEOUT_TXQUEUE
#endif

#if KERNEL_VERSION(5, 9, 0) > LINUX_VERSION_CODE
#define HAVE_TSK_IN_GUP
#endif

#if KERNEL_VERSION(5, 15, 0) <= LINUX_VERSION_CODE
#define HAVE_ETH_HW_ADDR_SET
#endif

#if KERNEL_VERSION(5, 18, 0) > LINUX_VERSION_CODE
#define HAVE_NETIF_RX_NI
#endif
