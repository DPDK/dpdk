# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

ifeq ($(FLEXRAN_SDK),)
$(error "Please define FLEXRAN_SDK environment variable")
endif

# library name
LIB = librte_pmd_bbdev_turbo_sw.a

# build flags
CFLAGS += -DALLOW_EXPERIMENTAL_API
CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)
LDLIBS += -lrte_eal -lrte_mbuf -lrte_mempool -lrte_ring -lrte_kvargs
LDLIBS += -lrte_bbdev
LDLIBS += -lrte_bus_vdev

# versioning export map
EXPORT_MAP := rte_pmd_bbdev_turbo_sw_version.map

# external library dependencies
CFLAGS += -I$(FLEXRAN_SDK)/lib_common
CFLAGS += -I$(FLEXRAN_SDK)/lib_turbo
CFLAGS += -I$(FLEXRAN_SDK)/lib_crc
CFLAGS += -I$(FLEXRAN_SDK)/lib_rate_matching

LDLIBS += -L$(FLEXRAN_SDK)/lib_turbo -lturbo
LDLIBS += -L$(FLEXRAN_SDK)/lib_crc -lcrc
LDLIBS += -L$(FLEXRAN_SDK)/lib_rate_matching -lrate_matching
LDLIBS += -L$(FLEXRAN_SDK)/lib_common -lcommon
LDLIBS += -lstdc++ -lirc -limf -lipps

# library version
LIBABIVER := 1

# library source files
SRCS-$(CONFIG_RTE_LIBRTE_PMD_BBDEV_TURBO_SW) += bbdev_turbo_software.c

include $(RTE_SDK)/mk/rte.lib.mk
