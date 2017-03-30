#   BSD LICENSE
#
#   Copyright(c) 2016-2017 Intel Corporation. All rights reserved.
#
#   Redistribution and use in source and binary forms, with or without
#   modification, are permitted provided that the following conditions
#   are met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in
#       the documentation and/or other materials provided with the
#       distribution.
#     * Neither the name of Intel Corporation nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
#   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
#   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
#   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
#   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
#   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
#   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
#   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
#   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
#   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
#   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
#   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

include $(RTE_SDK)/mk/rte.vars.mk

# library name
LIB = librte_pmd_sw_event.a

# build flags
CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)
# for older GCC versions, allow us to initialize an event using
# designated initializers.
ifeq ($(CONFIG_RTE_TOOLCHAIN_GCC),y)
ifeq ($(shell test $(GCC_VERSION) -le 50 && echo 1), 1)
CFLAGS += -Wno-missing-field-initializers
endif
endif

# library version
LIBABIVER := 1

# versioning export map
EXPORT_MAP := rte_pmd_evdev_sw_version.map

# library source files
SRCS-$(CONFIG_RTE_LIBRTE_PMD_SW_EVENTDEV) += sw_evdev.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_SW_EVENTDEV) += sw_evdev_worker.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_SW_EVENTDEV) += sw_evdev_scheduler.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_SW_EVENTDEV) += sw_evdev_xstats.c

# export include files
SYMLINK-y-include +=

include $(RTE_SDK)/mk/rte.lib.mk
