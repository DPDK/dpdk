# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2016 Intel Corporation

ifeq ($(RTE_SDK),)
	$(error "Please define RTE_SDK environment variable")
endif

# Default target, can be overridden by command line or environment
RTE_TARGET ?= x86_64-native-linuxapp-gcc

include $(RTE_SDK)/mk/rte.vars.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(CONFIG_RTE_LIBRTE_SECURITY),y)
$(error "RTE_LIBRTE_SECURITY is required to build ipsec-secgw")
endif
endif

APP = ipsec-secgw

CFLAGS += -O3 -gdwarf-2
CFLAGS += $(WERROR_FLAGS)
ifeq ($(CONFIG_RTE_TOOLCHAIN_ICC),y)
CFLAGS_sa.o += -diag-disable=vec
endif

ifeq ($(DEBUG),1)
CFLAGS += -DIPSEC_DEBUG -fstack-protector-all -O0
endif

#
# all source are stored in SRCS-y
#
SRCS-y += parser.c
SRCS-y += ipsec.c
SRCS-y += esp.c
SRCS-y += sp4.c
SRCS-y += sp6.c
SRCS-y += sa.c
SRCS-y += rt.c
SRCS-y += ipsec-secgw.c

include $(RTE_SDK)/mk/rte.extapp.mk
