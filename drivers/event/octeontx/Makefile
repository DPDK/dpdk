# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Cavium, Inc
#

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_pmd_octeontx_ssovf.a

CFLAGS += $(WERROR_FLAGS)
CFLAGS += -I$(RTE_SDK)/drivers/common/octeontx/
CFLAGS += -I$(RTE_SDK)/drivers/mempool/octeontx/
CFLAGS += -I$(RTE_SDK)/drivers/net/octeontx/

LDLIBS += -lrte_eal -lrte_eventdev -lrte_common_octeontx -lrte_pmd_octeontx
LDLIBS += -lrte_bus_pci -lrte_mempool -lrte_mbuf -lrte_kvargs
LDLIBS += -lrte_bus_vdev -lrte_ethdev

EXPORT_MAP := rte_pmd_octeontx_event_version.map

#
# all source are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_LIBRTE_PMD_OCTEONTX_SSOVF) += ssovf_worker.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_OCTEONTX_SSOVF) += ssovf_evdev.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_OCTEONTX_SSOVF) += ssovf_evdev_selftest.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_OCTEONTX_SSOVF) += ssovf_probe.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_OCTEONTX_SSOVF) += timvf_worker.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_OCTEONTX_SSOVF) += timvf_evdev.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_OCTEONTX_SSOVF) += timvf_probe.c

ifeq ($(CONFIG_RTE_TOOLCHAIN_GCC),y)
CFLAGS_ssovf_worker.o += -fno-prefetch-loop-arrays
CFLAGS_timvf_worker.o += -fno-prefetch-loop-arrays

ifeq ($(shell test $(GCC_VERSION) -ge 46 && echo 1), 1)
CFLAGS_ssovf_worker.o += -Ofast
CFLAGS_timvf_worker.o += -Ofast
else
CFLAGS_ssovf_worker.o += -O3 -ffast-math
CFLAGS_timvf_worker.o += -O3 -ffast-math
endif

else
CFLAGS_ssovf_worker.o += -Ofast
CFLAGS_timvf_worker.o += -Ofast
endif

include $(RTE_SDK)/mk/rte.lib.mk
