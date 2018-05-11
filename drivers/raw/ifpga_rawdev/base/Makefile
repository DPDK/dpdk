#SPDX-License-Identifier: BSD-3-Clause
#Copyright(c) 2010-2018 Intel Corporation

ifneq ($(CONFIG_RTE_LIBRTE_EAL),)
OSDEP := osdep_rte
else
OSDEP := osdep_raw
endif

CFLAGS += -I$(RTE_SDK)/drivers/raw/ifpga_rawdev/base/$(OSDEP)

SRCS-y += ifpga_api.c
SRCS-y += ifpga_enumerate.c
SRCS-y += ifpga_feature_dev.c
SRCS-y += ifpga_fme.c
SRCS-y += ifpga_fme_iperf.c
SRCS-y += ifpga_fme_dperf.c
SRCS-y += ifpga_fme_error.c
SRCS-y += ifpga_port.c
SRCS-y += ifpga_port_error.c
SRCS-y += opae_hw_api.c
SRCS-y += opae_ifpga_hw_api.c
SRCS-y += opae_debug.c
SRCS-y += ifpga_fme_pr.c

SRCS-y += $(wildcard $(SRCDIR)/base/$(OSDEP)/*.c)
