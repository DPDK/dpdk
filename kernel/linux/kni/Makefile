# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

#
# module name and path
#
MODULE = rte_kni

#
# CFLAGS
#
MODULE_CFLAGS += -I$(SRCDIR) --param max-inline-insns-single=50
MODULE_CFLAGS += -I$(RTE_OUTPUT)/include -I$(SRCDIR)/ethtool/ixgbe -I$(SRCDIR)/ethtool/igb
MODULE_CFLAGS += -include $(RTE_OUTPUT)/include/rte_config.h
MODULE_CFLAGS += -Wall -Werror

-include /etc/lsb-release

ifeq ($(DISTRIB_ID),Ubuntu)
MODULE_CFLAGS += -DUBUNTU_RELEASE_CODE=$(subst .,,$(DISTRIB_RELEASE))
UBUNTU_KERNEL_CODE := $(shell echo `grep UTS_RELEASE $(RTE_KERNELDIR)/include/generated/utsrelease.h \
	 | cut -d '"' -f2 | cut -d- -f1,2 | tr .- ,`,1)
MODULE_CFLAGS += -D"UBUNTU_KERNEL_CODE=UBUNTU_KERNEL_VERSION($(UBUNTU_KERNEL_CODE))"
endif

#
# all source are stored in SRCS-y
#
SRCS-y := kni_misc.c
SRCS-y += kni_net.c
SRCS-$(CONFIG_RTE_KNI_KMOD_ETHTOOL) += kni_ethtool.c

SRCS-$(CONFIG_RTE_KNI_KMOD_ETHTOOL) += ethtool/ixgbe/ixgbe_main.c
SRCS-$(CONFIG_RTE_KNI_KMOD_ETHTOOL) += ethtool/ixgbe/ixgbe_api.c
SRCS-$(CONFIG_RTE_KNI_KMOD_ETHTOOL) += ethtool/ixgbe/ixgbe_common.c
SRCS-$(CONFIG_RTE_KNI_KMOD_ETHTOOL) += ethtool/ixgbe/ixgbe_ethtool.c
SRCS-$(CONFIG_RTE_KNI_KMOD_ETHTOOL) += ethtool/ixgbe/ixgbe_82599.c
SRCS-$(CONFIG_RTE_KNI_KMOD_ETHTOOL) += ethtool/ixgbe/ixgbe_82598.c
SRCS-$(CONFIG_RTE_KNI_KMOD_ETHTOOL) += ethtool/ixgbe/ixgbe_x540.c
SRCS-$(CONFIG_RTE_KNI_KMOD_ETHTOOL) += ethtool/ixgbe/ixgbe_phy.c
SRCS-$(CONFIG_RTE_KNI_KMOD_ETHTOOL) += ethtool/ixgbe/kcompat.c

SRCS-$(CONFIG_RTE_KNI_KMOD_ETHTOOL) += ethtool/igb/e1000_82575.c
SRCS-$(CONFIG_RTE_KNI_KMOD_ETHTOOL) += ethtool/igb/e1000_i210.c
SRCS-$(CONFIG_RTE_KNI_KMOD_ETHTOOL) += ethtool/igb/e1000_api.c
SRCS-$(CONFIG_RTE_KNI_KMOD_ETHTOOL) += ethtool/igb/e1000_mac.c
SRCS-$(CONFIG_RTE_KNI_KMOD_ETHTOOL) += ethtool/igb/e1000_manage.c
SRCS-$(CONFIG_RTE_KNI_KMOD_ETHTOOL) += ethtool/igb/e1000_mbx.c
SRCS-$(CONFIG_RTE_KNI_KMOD_ETHTOOL) += ethtool/igb/e1000_nvm.c
SRCS-$(CONFIG_RTE_KNI_KMOD_ETHTOOL) += ethtool/igb/e1000_phy.c
SRCS-$(CONFIG_RTE_KNI_KMOD_ETHTOOL) += ethtool/igb/igb_ethtool.c
SRCS-$(CONFIG_RTE_KNI_KMOD_ETHTOOL) += ethtool/igb/igb_main.c
SRCS-$(CONFIG_RTE_KNI_KMOD_ETHTOOL) += ethtool/igb/igb_param.c
SRCS-$(CONFIG_RTE_KNI_KMOD_ETHTOOL) += ethtool/igb/igb_vmdq.c

include $(RTE_SDK)/mk/rte.module.mk
