# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2015 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

LIB = librte_eal.a

ARCH_DIR ?= $(RTE_ARCH)
VPATH += $(RTE_SDK)/lib/librte_eal/common
VPATH += $(RTE_SDK)/lib/librte_eal/common/arch/$(ARCH_DIR)

CFLAGS += -DALLOW_EXPERIMENTAL_API
CFLAGS += -I$(SRCDIR)/include
CFLAGS += -I$(RTE_SDK)/lib/librte_eal/common
CFLAGS += -I$(RTE_SDK)/lib/librte_eal/common/include
CFLAGS += $(WERROR_FLAGS) -O3

LDLIBS += -lexecinfo
LDLIBS += -lpthread
LDLIBS += -lgcc_s

EXPORT_MAP := ../../rte_eal_version.map

LIBABIVER := 7

# specific to bsdapp exec-env
SRCS-$(CONFIG_RTE_EXEC_ENV_BSDAPP) := eal.c
SRCS-$(CONFIG_RTE_EXEC_ENV_BSDAPP) += eal_cpuflags.c
SRCS-$(CONFIG_RTE_EXEC_ENV_BSDAPP) += eal_memory.c
SRCS-$(CONFIG_RTE_EXEC_ENV_BSDAPP) += eal_hugepage_info.c
SRCS-$(CONFIG_RTE_EXEC_ENV_BSDAPP) += eal_thread.c
SRCS-$(CONFIG_RTE_EXEC_ENV_BSDAPP) += eal_debug.c
SRCS-$(CONFIG_RTE_EXEC_ENV_BSDAPP) += eal_memalloc.c
SRCS-$(CONFIG_RTE_EXEC_ENV_BSDAPP) += eal_lcore.c
SRCS-$(CONFIG_RTE_EXEC_ENV_BSDAPP) += eal_timer.c
SRCS-$(CONFIG_RTE_EXEC_ENV_BSDAPP) += eal_interrupts.c
SRCS-$(CONFIG_RTE_EXEC_ENV_BSDAPP) += eal_alarm.c
SRCS-$(CONFIG_RTE_EXEC_ENV_BSDAPP) += eal_dev.c

# from common dir
SRCS-$(CONFIG_RTE_EXEC_ENV_BSDAPP) += eal_common_lcore.c
SRCS-$(CONFIG_RTE_EXEC_ENV_BSDAPP) += eal_common_timer.c
SRCS-$(CONFIG_RTE_EXEC_ENV_BSDAPP) += eal_common_memzone.c
SRCS-$(CONFIG_RTE_EXEC_ENV_BSDAPP) += eal_common_log.c
SRCS-$(CONFIG_RTE_EXEC_ENV_BSDAPP) += eal_common_launch.c
SRCS-$(CONFIG_RTE_EXEC_ENV_BSDAPP) += eal_common_memalloc.c
SRCS-$(CONFIG_RTE_EXEC_ENV_BSDAPP) += eal_common_memory.c
SRCS-$(CONFIG_RTE_EXEC_ENV_BSDAPP) += eal_common_tailqs.c
SRCS-$(CONFIG_RTE_EXEC_ENV_BSDAPP) += eal_common_errno.c
SRCS-$(CONFIG_RTE_EXEC_ENV_BSDAPP) += eal_common_cpuflags.c
SRCS-$(CONFIG_RTE_EXEC_ENV_BSDAPP) += eal_common_hypervisor.c
SRCS-$(CONFIG_RTE_EXEC_ENV_BSDAPP) += eal_common_string_fns.c
SRCS-$(CONFIG_RTE_EXEC_ENV_BSDAPP) += eal_common_hexdump.c
SRCS-$(CONFIG_RTE_EXEC_ENV_BSDAPP) += eal_common_devargs.c
SRCS-$(CONFIG_RTE_EXEC_ENV_BSDAPP) += eal_common_bus.c
SRCS-$(CONFIG_RTE_EXEC_ENV_BSDAPP) += eal_common_dev.c
SRCS-$(CONFIG_RTE_EXEC_ENV_BSDAPP) += eal_common_options.c
SRCS-$(CONFIG_RTE_EXEC_ENV_BSDAPP) += eal_common_thread.c
SRCS-$(CONFIG_RTE_EXEC_ENV_BSDAPP) += eal_common_proc.c
SRCS-$(CONFIG_RTE_EXEC_ENV_BSDAPP) += eal_common_fbarray.c
SRCS-$(CONFIG_RTE_EXEC_ENV_BSDAPP) += rte_malloc.c
SRCS-$(CONFIG_RTE_EXEC_ENV_BSDAPP) += malloc_elem.c
SRCS-$(CONFIG_RTE_EXEC_ENV_BSDAPP) += malloc_heap.c
SRCS-$(CONFIG_RTE_EXEC_ENV_BSDAPP) += malloc_mp.c
SRCS-$(CONFIG_RTE_EXEC_ENV_BSDAPP) += rte_keepalive.c
SRCS-$(CONFIG_RTE_EXEC_ENV_BSDAPP) += rte_service.c
SRCS-$(CONFIG_RTE_EXEC_ENV_BSDAPP) += rte_reciprocal.c

# from arch dir
SRCS-$(CONFIG_RTE_EXEC_ENV_BSDAPP) += rte_cpuflags.c
SRCS-$(CONFIG_RTE_EXEC_ENV_BSDAPP) += rte_hypervisor.c
SRCS-$(CONFIG_RTE_ARCH_X86) += rte_spinlock.c
SRCS-y += rte_cycles.c

CFLAGS_eal_common_cpuflags.o := $(CPUFLAGS_LIST)

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

INC :=  # no bsdapp specific headers

SYMLINK-$(CONFIG_RTE_EXEC_ENV_BSDAPP)-include/exec-env := \
	$(addprefix include/exec-env/,$(INC))

include $(RTE_SDK)/mk/rte.lib.mk
