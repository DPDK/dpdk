# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Marvell International Ltd.
#

include $(RTE_SDK)/mk/rte.vars.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(MAKECMDGOALS),config)
ifeq ($(LIBMUSDK_PATH),)
$(error "Please define LIBMUSDK_PATH environment variable")
endif
endif
endif

# library name
LIB = librte_common_mvep.a

# versioning export map
EXPORT_MAP := rte_common_mvep_version.map

# external library dependencies
CFLAGS += -I$($RTE_SDK)/drivers/common/mvep
CFLAGS += -I$(LIBMUSDK_PATH)/include
CFLAGS += -DMVCONF_TYPES_PUBLIC
CFLAGS += -DMVCONF_DMA_PHYS_ADDR_T_PUBLIC
CFLAGS += $(WERROR_FLAGS)
CFLAGS += -O3
LDLIBS += -L$(LIBMUSDK_PATH)/lib
LDLIBS += -lmusdk
LDLIBS += -lrte_eal -lrte_kvargs

# library source files
SRCS-y += mvep_common.c

include $(RTE_SDK)/mk/rte.lib.mk
