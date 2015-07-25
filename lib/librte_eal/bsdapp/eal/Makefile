#   BSD LICENSE
#
#   Copyright(c) 2010-2015 Intel Corporation. All rights reserved.
#   All rights reserved.
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

LIB = librte_eal.a

VPATH += $(RTE_SDK)/lib/librte_eal/common

CFLAGS += -I$(SRCDIR)/include
CFLAGS += -I$(RTE_SDK)/lib/librte_eal/common
CFLAGS += -I$(RTE_SDK)/lib/librte_eal/common/include
CFLAGS += -I$(RTE_SDK)/lib/librte_ring
CFLAGS += -I$(RTE_SDK)/lib/librte_mempool
CFLAGS += $(WERROR_FLAGS) -O3

EXPORT_MAP := rte_eal_version.map

LIBABIVER := 1

# specific to linuxapp exec-env
SRCS-$(CONFIG_RTE_LIBRTE_EAL_BSDAPP) := eal.c
SRCS-$(CONFIG_RTE_LIBRTE_EAL_BSDAPP) += eal_memory.c
SRCS-$(CONFIG_RTE_LIBRTE_EAL_BSDAPP) += eal_hugepage_info.c
SRCS-$(CONFIG_RTE_LIBRTE_EAL_BSDAPP) += eal_thread.c
SRCS-$(CONFIG_RTE_LIBRTE_EAL_BSDAPP) += eal_log.c
SRCS-$(CONFIG_RTE_LIBRTE_EAL_BSDAPP) += eal_pci.c
SRCS-$(CONFIG_RTE_LIBRTE_EAL_BSDAPP) += eal_debug.c
SRCS-$(CONFIG_RTE_LIBRTE_EAL_BSDAPP) += eal_lcore.c
SRCS-$(CONFIG_RTE_LIBRTE_EAL_BSDAPP) += eal_timer.c
SRCS-$(CONFIG_RTE_LIBRTE_EAL_BSDAPP) += eal_interrupts.c
SRCS-$(CONFIG_RTE_LIBRTE_EAL_BSDAPP) += eal_alarm.c

# from common dir
SRCS-$(CONFIG_RTE_LIBRTE_EAL_BSDAPP) += eal_common_lcore.c
SRCS-$(CONFIG_RTE_LIBRTE_EAL_BSDAPP) += eal_common_timer.c
SRCS-$(CONFIG_RTE_LIBRTE_EAL_BSDAPP) += eal_common_memzone.c
SRCS-$(CONFIG_RTE_LIBRTE_EAL_BSDAPP) += eal_common_log.c
SRCS-$(CONFIG_RTE_LIBRTE_EAL_BSDAPP) += eal_common_launch.c
SRCS-$(CONFIG_RTE_LIBRTE_EAL_BSDAPP) += eal_common_pci.c
SRCS-$(CONFIG_RTE_LIBRTE_EAL_BSDAPP) += eal_common_pci_uio.c
SRCS-$(CONFIG_RTE_LIBRTE_EAL_BSDAPP) += eal_common_memory.c
SRCS-$(CONFIG_RTE_LIBRTE_EAL_BSDAPP) += eal_common_tailqs.c
SRCS-$(CONFIG_RTE_LIBRTE_EAL_BSDAPP) += eal_common_errno.c
SRCS-$(CONFIG_RTE_LIBRTE_EAL_BSDAPP) += eal_common_cpuflags.c
SRCS-$(CONFIG_RTE_LIBRTE_EAL_BSDAPP) += eal_common_string_fns.c
SRCS-$(CONFIG_RTE_LIBRTE_EAL_BSDAPP) += eal_common_hexdump.c
SRCS-$(CONFIG_RTE_LIBRTE_EAL_BSDAPP) += eal_common_devargs.c
SRCS-$(CONFIG_RTE_LIBRTE_EAL_BSDAPP) += eal_common_dev.c
SRCS-$(CONFIG_RTE_LIBRTE_EAL_BSDAPP) += eal_common_options.c
SRCS-$(CONFIG_RTE_LIBRTE_EAL_BSDAPP) += eal_common_thread.c
SRCS-$(CONFIG_RTE_LIBRTE_EAL_BSDAPP) += rte_malloc.c
SRCS-$(CONFIG_RTE_LIBRTE_EAL_BSDAPP) += malloc_elem.c
SRCS-$(CONFIG_RTE_LIBRTE_EAL_BSDAPP) += malloc_heap.c

CFLAGS_eal.o := -D_GNU_SOURCE
#CFLAGS_eal_thread.o := -D_GNU_SOURCE
CFLAGS_eal_log.o := -D_GNU_SOURCE
CFLAGS_eal_common_log.o := -D_GNU_SOURCE

# workaround for a gcc bug with noreturn attribute
# http://gcc.gnu.org/bugzilla/show_bug.cgi?id=12603
ifeq ($(CONFIG_RTE_TOOLCHAIN_GCC),y)
CFLAGS_eal_thread.o += -Wno-return-type
CFLAGS_eal_hpet.o += -Wno-return-type
endif

INC := rte_interrupts.h

SYMLINK-$(CONFIG_RTE_LIBRTE_EAL_BSDAPP)-include/exec-env := \
	$(addprefix include/exec-env/,$(INC))

DEPDIRS-$(CONFIG_RTE_LIBRTE_EAL_BSDAPP) += lib/librte_eal/common

include $(RTE_SDK)/mk/rte.lib.mk
