# SPDX-License-Identifier: BSD-3-Clause

include $(RTE_SDK)/mk/rte.vars.mk

LIB = librte_bus_vmbus.a
EXPORT_MAP := rte_bus_vmbus_version.map

CFLAGS += -I$(SRCDIR)
CFLAGS += -O3 $(WERROR_FLAGS)

ifneq ($(CONFIG_RTE_EXEC_ENV_LINUX),)
SYSTEM := linux
endif
ifneq ($(CONFIG_RTE_EXEC_ENV_FREEBSD),)
$(error "VMBUS not implemented for BSD yet")
endif

CFLAGS += -I$(RTE_SDK)/drivers/bus/vmbus/$(SYSTEM)
CFLAGS += -I$(RTE_SDK)/lib/librte_eal/common

LDLIBS += -lrte_eal -lrte_mbuf -lrte_mempool -lrte_ring
LDLIBS += -lrte_ethdev

include $(RTE_SDK)/drivers/bus/vmbus/$(SYSTEM)/Makefile
SRCS-$(CONFIG_RTE_LIBRTE_VMBUS) := $(addprefix $(SYSTEM)/,$(SRCS))
SRCS-$(CONFIG_RTE_LIBRTE_VMBUS) += vmbus_common.c
SRCS-$(CONFIG_RTE_LIBRTE_VMBUS) += vmbus_channel.c vmbus_bufring.c
SRCS-$(CONFIG_RTE_LIBRTE_VMBUS) += vmbus_common_uio.c

SYMLINK-$(CONFIG_RTE_LIBRTE_VMBUS)-include += rte_bus_vmbus.h
SYMLINK-$(CONFIG_RTE_LIBRTE_VMBUS)-include += rte_vmbus_reg.h

include $(RTE_SDK)/mk/rte.lib.mk
