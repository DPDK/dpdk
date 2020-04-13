# SPDX-License-Identifier: BSD-3-Clause
# Copyright(C) 2019 Marvell International Ltd.
#

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_pmd_octeontx2_event.a

CFLAGS += $(WERROR_FLAGS)
CFLAGS += -I$(RTE_SDK)/drivers/common/octeontx2
CFLAGS += -I$(RTE_SDK)/drivers/crypto/octeontx2
CFLAGS += -I$(RTE_SDK)/drivers/mempool/octeontx2
CFLAGS += -I$(RTE_SDK)/drivers/event/octeontx2
CFLAGS += -I$(RTE_SDK)/drivers/net/octeontx2
CFLAGS += -O3

ifneq ($(CONFIG_RTE_ARCH_64),y)
CFLAGS += -Wno-int-to-pointer-cast
CFLAGS += -Wno-pointer-to-int-cast
ifeq ($(CONFIG_RTE_TOOLCHAIN_ICC),y)
CFLAGS += -diag-disable 2259
endif
endif

EXPORT_MAP := rte_pmd_octeontx2_event_version.map

#
# all source are stored in SRCS-y
#

SRCS-$(CONFIG_RTE_LIBRTE_PMD_OCTEONTX2_EVENTDEV) += otx2_worker_dual.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_OCTEONTX2_EVENTDEV) += otx2_worker.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_OCTEONTX2_EVENTDEV) += otx2_tim_worker.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_OCTEONTX2_EVENTDEV) += otx2_evdev.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_OCTEONTX2_EVENTDEV) += otx2_evdev_adptr.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_OCTEONTX2_EVENTDEV) += otx2_tim_evdev.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_OCTEONTX2_EVENTDEV) += otx2_evdev_selftest.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_OCTEONTX2_EVENTDEV) += otx2_evdev_irq.c

LDLIBS += -lrte_eal -lrte_bus_pci -lrte_pci -lrte_kvargs
LDLIBS += -lrte_mempool -lrte_eventdev -lrte_mbuf -lrte_ethdev
LDLIBS += -lrte_common_octeontx2 -lrte_mempool_octeontx2

include $(RTE_SDK)/mk/rte.lib.mk
