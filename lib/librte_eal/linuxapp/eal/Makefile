# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2016 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

LIB = librte_eal.a

ARCH_DIR ?= $(RTE_ARCH)

EXPORT_MAP := ../../rte_eal_version.map
VPATH += $(RTE_SDK)/lib/librte_eal/common/arch/$(ARCH_DIR)

LIBABIVER := 7

VPATH += $(RTE_SDK)/lib/librte_eal/common

CFLAGS += -DALLOW_EXPERIMENTAL_API
CFLAGS += -I$(SRCDIR)/include
CFLAGS += -I$(RTE_SDK)/lib/librte_eal/common
CFLAGS += -I$(RTE_SDK)/lib/librte_eal/common/include
CFLAGS += $(WERROR_FLAGS) -O3

LDLIBS += -ldl
LDLIBS += -lpthread
LDLIBS += -lgcc_s
LDLIBS += -lrt
ifeq ($(CONFIG_RTE_EAL_NUMA_AWARE_HUGEPAGES),y)
LDLIBS += -lnuma
endif

# specific to linuxapp exec-env
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUXAPP) := eal.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUXAPP) += eal_cpuflags.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUXAPP) += eal_hugepage_info.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUXAPP) += eal_memory.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUXAPP) += eal_thread.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUXAPP) += eal_log.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUXAPP) += eal_vfio.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUXAPP) += eal_vfio_mp_sync.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUXAPP) += eal_memalloc.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUXAPP) += eal_debug.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUXAPP) += eal_lcore.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUXAPP) += eal_timer.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUXAPP) += eal_interrupts.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUXAPP) += eal_alarm.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUXAPP) += eal_dev.c

# from common dir
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUXAPP) += eal_common_lcore.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUXAPP) += eal_common_timer.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUXAPP) += eal_common_memzone.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUXAPP) += eal_common_log.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUXAPP) += eal_common_launch.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUXAPP) += eal_common_memalloc.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUXAPP) += eal_common_memory.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUXAPP) += eal_common_tailqs.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUXAPP) += eal_common_errno.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUXAPP) += eal_common_cpuflags.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUXAPP) += eal_common_hypervisor.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUXAPP) += eal_common_string_fns.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUXAPP) += eal_common_hexdump.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUXAPP) += eal_common_devargs.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUXAPP) += eal_common_bus.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUXAPP) += eal_common_dev.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUXAPP) += eal_common_options.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUXAPP) += eal_common_thread.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUXAPP) += eal_common_proc.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUXAPP) += eal_common_fbarray.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUXAPP) += rte_malloc.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUXAPP) += malloc_elem.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUXAPP) += malloc_heap.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUXAPP) += malloc_mp.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUXAPP) += rte_keepalive.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUXAPP) += rte_service.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUXAPP) += rte_reciprocal.c

# from arch dir
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUXAPP) += rte_cpuflags.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUXAPP) += rte_hypervisor.c
SRCS-$(CONFIG_RTE_ARCH_X86) += rte_spinlock.c
SRCS-y += rte_cycles.c

CFLAGS_eal_common_cpuflags.o := $(CPUFLAGS_LIST)

CFLAGS_eal.o := -D_GNU_SOURCE
CFLAGS_eal_interrupts.o := -D_GNU_SOURCE
CFLAGS_eal_vfio_mp_sync.o := -D_GNU_SOURCE
CFLAGS_eal_timer.o := -D_GNU_SOURCE
CFLAGS_eal_lcore.o := -D_GNU_SOURCE
CFLAGS_eal_memalloc.o := -D_GNU_SOURCE
CFLAGS_eal_thread.o := -D_GNU_SOURCE
CFLAGS_eal_log.o := -D_GNU_SOURCE
CFLAGS_eal_common_log.o := -D_GNU_SOURCE
CFLAGS_eal_hugepage_info.o := -D_GNU_SOURCE
CFLAGS_eal_common_whitelist.o := -D_GNU_SOURCE
CFLAGS_eal_common_options.o := -D_GNU_SOURCE
CFLAGS_eal_common_thread.o := -D_GNU_SOURCE
CFLAGS_eal_common_lcore.o := -D_GNU_SOURCE
CFLAGS_rte_cycles.o := -D_GNU_SOURCE

# workaround for a gcc bug with noreturn attribute
# http://gcc.gnu.org/bugzilla/show_bug.cgi?id=12603
ifeq ($(CONFIG_RTE_TOOLCHAIN_GCC),y)
CFLAGS_eal_thread.o += -Wno-return-type
endif

INC := rte_kni_common.h

SYMLINK-$(CONFIG_RTE_EXEC_ENV_LINUXAPP)-include/exec-env := \
	$(addprefix include/exec-env/,$(INC))

include $(RTE_SDK)/mk/rte.lib.mk
