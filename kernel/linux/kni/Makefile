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
MODULE_CFLAGS += -I$(RTE_OUTPUT)/include
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

include $(RTE_SDK)/mk/rte.module.mk
