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

LDLIBS += -ldl
LDLIBS += -lpthread
LDLIBS += -lgcc_s
LDLIBS += -lrt
LDLIBS += -lrte_kvargs
LDLIBS += -lrte_telemetry
ifeq ($(CONFIG_RTE_EAL_NUMA_AWARE_HUGEPAGES),y)
LDLIBS += -lnuma
endif

EXPORT_MAP := ../rte_eal_version.map

# specific to linux exec-env
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUX) := eal.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUX) += eal_cpuflags.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUX) += eal_hugepage_info.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUX) += eal_memory.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUX) += eal_thread.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUX) += eal_log.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUX) += eal_vfio.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUX) += eal_vfio_mp_sync.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUX) += eal_memalloc.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUX) += eal_debug.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUX) += eal_lcore.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUX) += eal_timer.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUX) += eal_interrupts.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUX) += eal_alarm.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUX) += eal_dev.c

# from common dir
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUX) += eal_common_config.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUX) += eal_common_lcore.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUX) += eal_common_timer.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUX) += eal_common_memzone.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUX) += eal_common_log.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUX) += eal_common_launch.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUX) += eal_common_dynmem.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUX) += eal_common_mcfg.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUX) += eal_common_memalloc.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUX) += eal_common_memory.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUX) += eal_common_tailqs.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUX) += eal_common_errno.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUX) += eal_common_cpuflags.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUX) += eal_common_hypervisor.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUX) += eal_common_string_fns.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUX) += eal_common_hexdump.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUX) += eal_common_debug.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUX) += eal_common_devargs.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUX) += eal_common_class.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUX) += eal_common_bus.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUX) += eal_common_dev.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUX) += eal_common_options.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUX) += eal_common_thread.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUX) += eal_common_proc.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUX) += eal_common_fbarray.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUX) += eal_common_uuid.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUX) += eal_common_trace.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUX) += eal_common_trace_ctf.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUX) += eal_common_trace_points.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUX) += eal_common_trace_utils.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUX) += rte_malloc.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUX) += hotplug_mp.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUX) += malloc_elem.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUX) += malloc_heap.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUX) += malloc_mp.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUX) += rte_keepalive.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUX) += rte_service.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUX) += rte_random.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUX) += rte_reciprocal.c

# from unix dir
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUX) += eal_file.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUX) += eal_unix_memory.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUX) += eal_unix_timer.c

# from arch dir
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUX) += rte_cpuflags.c
SRCS-$(CONFIG_RTE_EXEC_ENV_LINUX) += rte_hypervisor.c
SRCS-$(CONFIG_RTE_ARCH_X86) += rte_spinlock.c
SRCS-y += rte_cycles.c

CFLAGS_eal_common_cpuflags.o := $(CPUFLAGS_LIST)

# workaround for a gcc bug with noreturn attribute
# http://gcc.gnu.org/bugzilla/show_bug.cgi?id=12603
ifeq ($(CONFIG_RTE_TOOLCHAIN_GCC),y)
CFLAGS_eal_thread.o += -Wno-return-type
endif

INC := rte_kni_common.h
INC += rte_os.h

SYMLINK-$(CONFIG_RTE_EXEC_ENV_LINUX)-include := $(addprefix include/,$(INC))

include $(RTE_SDK)/mk/rte.lib.mk
