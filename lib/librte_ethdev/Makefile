# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2017 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_ethdev.a

CFLAGS += -DALLOW_EXPERIMENTAL_API
CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)
LDLIBS += -lrte_net -lrte_eal -lrte_mempool -lrte_ring
LDLIBS += -lrte_mbuf -lrte_kvargs -lrte_cmdline -lrte_meter

EXPORT_MAP := rte_ethdev_version.map

LIBABIVER := 12

SRCS-y += ethdev_private.c
SRCS-y += rte_ethdev.c
SRCS-y += rte_class_eth.c
SRCS-y += rte_flow.c
SRCS-y += rte_tm.c
SRCS-y += rte_mtr.c
SRCS-y += ethdev_profile.c

#
# Export include files
#
SYMLINK-y-include += rte_ethdev.h
SYMLINK-y-include += rte_ethdev_driver.h
SYMLINK-y-include += rte_ethdev_core.h
SYMLINK-y-include += rte_ethdev_pci.h
SYMLINK-y-include += rte_ethdev_vdev.h
SYMLINK-y-include += rte_eth_ctrl.h
SYMLINK-y-include += rte_dev_info.h
SYMLINK-y-include += rte_flow.h
SYMLINK-y-include += rte_flow_driver.h
SYMLINK-y-include += rte_tm.h
SYMLINK-y-include += rte_tm_driver.h
SYMLINK-y-include += rte_mtr.h
SYMLINK-y-include += rte_mtr_driver.h

include $(RTE_SDK)/mk/rte.lib.mk
