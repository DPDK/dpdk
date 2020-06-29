# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2019 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

LIB = librte_eal.a

ARCH_DIR ?= $(RTE_ARCH)
VPATH += $(RTE_SDK)/lib/librte_eal/$(ARCH_DIR)
VPATH += $(RTE_SDK)/lib/librte_eal/unix
VPATH += $(RTE_SDK)/lib/librte_eal/common

CFLAGS += -I$(SRCDIR)/include
CFLAGS += -I$(RTE_SDK)/lib/librte_eal/common
CFLAGS += -I$(RTE_SDK)/lib/librte_eal/include
CFLAGS += $(WERROR_FLAGS) -O3

LDLIBS += -lexecinfo
LDLIBS += -lpthread
LDLIBS += -lgcc_s
LDLIBS += -lrte_kvargs
LDLIBS += -lrte_telemetry

EXPORT_MAP := ../rte_eal_version.map

# specific to freebsd exec-env
SRCS-$(CONFIG_RTE_EXEC_ENV_FREEBSD) := eal.c
SRCS-$(CONFIG_RTE_EXEC_ENV_FREEBSD) += eal_cpuflags.c
SRCS-$(CONFIG_RTE_EXEC_ENV_FREEBSD) += eal_memory.c
SRCS-$(CONFIG_RTE_EXEC_ENV_FREEBSD) += eal_hugepage_info.c
SRCS-$(CONFIG_RTE_EXEC_ENV_FREEBSD) += eal_thread.c
SRCS-$(CONFIG_RTE_EXEC_ENV_FREEBSD) += eal_debug.c
SRCS-$(CONFIG_RTE_EXEC_ENV_FREEBSD) += eal_memalloc.c
SRCS-$(CONFIG_RTE_EXEC_ENV_FREEBSD) += eal_lcore.c
SRCS-$(CONFIG_RTE_EXEC_ENV_FREEBSD) += eal_timer.c
SRCS-$(CONFIG_RTE_EXEC_ENV_FREEBSD) += eal_interrupts.c
SRCS-$(CONFIG_RTE_EXEC_ENV_FREEBSD) += eal_alarm.c
SRCS-$(CONFIG_RTE_EXEC_ENV_FREEBSD) += eal_dev.c

# from common dir
SRCS-$(CONFIG_RTE_EXEC_ENV_FREEBSD) += eal_common_config.c
SRCS-$(CONFIG_RTE_EXEC_ENV_FREEBSD) += eal_common_lcore.c
SRCS-$(CONFIG_RTE_EXEC_ENV_FREEBSD) += eal_common_timer.c
SRCS-$(CONFIG_RTE_EXEC_ENV_FREEBSD) += eal_common_memzone.c
SRCS-$(CONFIG_RTE_EXEC_ENV_FREEBSD) += eal_common_log.c
SRCS-$(CONFIG_RTE_EXEC_ENV_FREEBSD) += eal_common_launch.c
SRCS-$(CONFIG_RTE_EXEC_ENV_FREEBSD) += eal_common_mcfg.c
SRCS-$(CONFIG_RTE_EXEC_ENV_FREEBSD) += eal_common_memalloc.c
SRCS-$(CONFIG_RTE_EXEC_ENV_FREEBSD) += eal_common_memory.c
SRCS-$(CONFIG_RTE_EXEC_ENV_FREEBSD) += eal_common_tailqs.c
SRCS-$(CONFIG_RTE_EXEC_ENV_FREEBSD) += eal_common_errno.c
SRCS-$(CONFIG_RTE_EXEC_ENV_FREEBSD) += eal_common_cpuflags.c
SRCS-$(CONFIG_RTE_EXEC_ENV_FREEBSD) += eal_common_hypervisor.c
SRCS-$(CONFIG_RTE_EXEC_ENV_FREEBSD) += eal_common_string_fns.c
SRCS-$(CONFIG_RTE_EXEC_ENV_FREEBSD) += eal_common_hexdump.c
SRCS-$(CONFIG_RTE_EXEC_ENV_FREEBSD) += eal_common_debug.c
SRCS-$(CONFIG_RTE_EXEC_ENV_FREEBSD) += eal_common_devargs.c
SRCS-$(CONFIG_RTE_EXEC_ENV_FREEBSD) += eal_common_class.c
SRCS-$(CONFIG_RTE_EXEC_ENV_FREEBSD) += eal_common_bus.c
SRCS-$(CONFIG_RTE_EXEC_ENV_FREEBSD) += eal_common_dev.c
SRCS-$(CONFIG_RTE_EXEC_ENV_FREEBSD) += eal_common_options.c
SRCS-$(CONFIG_RTE_EXEC_ENV_FREEBSD) += eal_common_thread.c
SRCS-$(CONFIG_RTE_EXEC_ENV_FREEBSD) += eal_common_proc.c
SRCS-$(CONFIG_RTE_EXEC_ENV_FREEBSD) += eal_common_fbarray.c
SRCS-$(CONFIG_RTE_EXEC_ENV_FREEBSD) += eal_common_uuid.c
SRCS-$(CONFIG_RTE_EXEC_ENV_FREEBSD) += eal_common_trace.c
SRCS-$(CONFIG_RTE_EXEC_ENV_FREEBSD) += eal_common_trace_ctf.c
SRCS-$(CONFIG_RTE_EXEC_ENV_FREEBSD) += eal_common_trace_points.c
SRCS-$(CONFIG_RTE_EXEC_ENV_FREEBSD) += eal_common_trace_utils.c
SRCS-$(CONFIG_RTE_EXEC_ENV_FREEBSD) += rte_malloc.c
SRCS-$(CONFIG_RTE_EXEC_ENV_FREEBSD) += hotplug_mp.c
SRCS-$(CONFIG_RTE_EXEC_ENV_FREEBSD) += malloc_elem.c
SRCS-$(CONFIG_RTE_EXEC_ENV_FREEBSD) += malloc_heap.c
SRCS-$(CONFIG_RTE_EXEC_ENV_FREEBSD) += malloc_mp.c
SRCS-$(CONFIG_RTE_EXEC_ENV_FREEBSD) += rte_keepalive.c
SRCS-$(CONFIG_RTE_EXEC_ENV_FREEBSD) += rte_service.c
SRCS-$(CONFIG_RTE_EXEC_ENV_FREEBSD) += rte_random.c
SRCS-$(CONFIG_RTE_EXEC_ENV_FREEBSD) += rte_reciprocal.c

# from unix dir
SRCS-$(CONFIG_RTE_EXEC_ENV_FREEBSD) += eal_file.c
SRCS-$(CONFIG_RTE_EXEC_ENV_FREEBSD) += eal_unix_memory.c
SRCS-$(CONFIG_RTE_EXEC_ENV_FREEBSD) += eal_unix_timer.c

# from arch dir
SRCS-$(CONFIG_RTE_EXEC_ENV_FREEBSD) += rte_cpuflags.c
SRCS-$(CONFIG_RTE_EXEC_ENV_FREEBSD) += rte_hypervisor.c
SRCS-$(CONFIG_RTE_ARCH_X86) += rte_spinlock.c
SRCS-y += rte_cycles.c

CFLAGS_eal_common_cpuflags.o := $(CPUFLAGS_LIST)

# workaround for a gcc bug with noreturn attribute
# http://gcc.gnu.org/bugzilla/show_bug.cgi?id=12603
ifeq ($(CONFIG_RTE_TOOLCHAIN_GCC),y)
CFLAGS_eal_thread.o += -Wno-return-type
CFLAGS_eal_hpet.o += -Wno-return-type
endif

INC := rte_os.h

SYMLINK-$(CONFIG_RTE_EXEC_ENV_FREEBSD)-include := $(addprefix include/,$(INC))

include $(RTE_SDK)/mk/rte.lib.mk
