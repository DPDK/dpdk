#   BSD LICENSE
#
#   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
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

INC := rte_atomic.h rte_branch_prediction.h rte_byteorder.h rte_common.h
INC += rte_cycles.h rte_debug.h rte_eal.h rte_errno.h rte_launch.h rte_lcore.h
INC += rte_log.h rte_memcpy.h rte_memory.h rte_memzone.h rte_pci.h
INC += rte_pci_dev_ids.h rte_per_lcore.h rte_prefetch.h rte_random.h
INC += rte_rwlock.h rte_spinlock.h rte_tailq.h rte_interrupts.h rte_alarm.h
INC += rte_string_fns.h rte_cpuflags.h rte_version.h rte_tailq_elem.h
INC += rte_eal_memconfig.h rte_malloc_heap.h
INC += rte_hexdump.h rte_devargs.h rte_dev.h
INC += rte_common_vect.h
INC += rte_pci_dev_feature_defs.h rte_pci_dev_features.h

ifeq ($(CONFIG_RTE_INSECURE_FUNCTION_WARNING),y)
INC += rte_warnings.h
endif

ARCH_INC := rte_atomic.h

SYMLINK-$(CONFIG_RTE_LIBRTE_EAL)-include := $(addprefix include/,$(INC))
SYMLINK-$(CONFIG_RTE_LIBRTE_EAL)-include/arch := \
	$(addprefix include/$(RTE_ARCH)/arch/,$(ARCH_INC))

# add libc if configured
DEPDIRS-$(CONFIG_RTE_LIBC) += lib/libc

include $(RTE_SDK)/mk/rte.install.mk
