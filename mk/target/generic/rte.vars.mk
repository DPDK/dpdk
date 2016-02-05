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

#
# This .mk is the generic target rte.var.mk ; it includes .mk for
# the specified machine, architecture, toolchain (compiler) and
# executive environment.
#

#
# machine:
#
#   - can define ARCH variable (overriden by cmdline value)
#   - can define CROSS variable (overriden by cmdline value)
#   - define MACHINE_CFLAGS variable (overriden by cmdline value)
#   - define MACHINE_LDFLAGS variable (overriden by cmdline value)
#   - define MACHINE_ASFLAGS variable (overriden by cmdline value)
#   - can define CPU_CFLAGS variable (overriden by cmdline value) that
#     overrides the one defined in arch.
#   - can define CPU_LDFLAGS variable (overriden by cmdline value) that
#     overrides the one defined in arch.
#   - can define CPU_ASFLAGS variable (overriden by cmdline value) that
#     overrides the one defined in arch.
#
include $(RTE_SDK)/mk/machine/$(RTE_MACHINE)/rte.vars.mk

#
# arch:
#
#   - define ARCH variable (overriden by cmdline or by previous
#     optional define in machine .mk)
#   - define CROSS variable (overriden by cmdline or previous define
#     in machine .mk)
#   - define CPU_CFLAGS variable (overriden by cmdline or previous
#     define in machine .mk)
#   - define CPU_LDFLAGS variable (overriden by cmdline or previous
#     define in machine .mk)
#   - define CPU_ASFLAGS variable (overriden by cmdline or previous
#     define in machine .mk)
#   - may override any previously defined variable
#
include $(RTE_SDK)/mk/arch/$(RTE_ARCH)/rte.vars.mk

#
# toolchain:
#
#   - define CC, LD, AR, AS, ...
#   - define TOOLCHAIN_CFLAGS variable (overriden by cmdline value)
#   - define TOOLCHAIN_LDFLAGS variable (overriden by cmdline value)
#   - define TOOLCHAIN_ASFLAGS variable (overriden by cmdline value)
#   - may override any previously defined variable
#
include $(RTE_SDK)/mk/toolchain/$(RTE_TOOLCHAIN)/rte.vars.mk

#
# exec-env:
#
#   - define EXECENV_CFLAGS variable (overriden by cmdline)
#   - define EXECENV_LDFLAGS variable (overriden by cmdline)
#   - define EXECENV_ASFLAGS variable (overriden by cmdline)
#   - may override any previously defined variable
#
include $(RTE_SDK)/mk/exec-env/$(RTE_EXEC_ENV)/rte.vars.mk

# Don't set CFLAGS/LDFLAGS flags for kernel module, all flags are
# provided by Kbuild framework.
ifeq ($(KERNELRELEASE),)

# now that the environment is mostly set up, including the machine type we will
# be passing to the compiler, set up the specific CPU flags based on that info.
include $(RTE_SDK)/mk/rte.cpuflags.mk

# merge all CFLAGS
CFLAGS := $(CPU_CFLAGS) $(EXECENV_CFLAGS) $(TOOLCHAIN_CFLAGS) $(MACHINE_CFLAGS)
CFLAGS += $(TARGET_CFLAGS)

# merge all LDFLAGS
LDFLAGS := $(CPU_LDFLAGS) $(EXECENV_LDFLAGS) $(TOOLCHAIN_LDFLAGS) $(MACHINE_LDFLAGS)
LDFLAGS += $(TARGET_LDFLAGS)

# merge all ASFLAGS
ASFLAGS := $(CPU_ASFLAGS) $(EXECENV_ASFLAGS) $(TOOLCHAIN_ASFLAGS) $(MACHINE_ASFLAGS)
ASFLAGS += $(TARGET_ASFLAGS)

# add default include and lib paths
CFLAGS += -I$(RTE_OUTPUT)/include
LDFLAGS += -L$(RTE_OUTPUT)/lib

# always include rte_config.h: the one in $(RTE_OUTPUT)/include is
# the configuration of SDK when $(BUILDING_RTE_SDK) is true, or the
# configuration of the application if $(BUILDING_RTE_SDK) is not
# defined.
ifeq ($(BUILDING_RTE_SDK),1)
# building sdk
CFLAGS += -include $(RTE_OUTPUT)/include/rte_config.h
ifeq ($(CONFIG_RTE_INSECURE_FUNCTION_WARNING),y)
CFLAGS += -include rte_warnings.h
endif
else
# if we are building an external application, include SDK's lib and
# includes too
CFLAGS += -I$(RTE_SDK_BIN)/include
ifneq ($(wildcard $(RTE_OUTPUT)/include/rte_config.h),)
CFLAGS += -include $(RTE_OUTPUT)/include/rte_config.h
endif
CFLAGS += -include $(RTE_SDK_BIN)/include/rte_config.h
ifeq ($(CONFIG_RTE_INSECURE_FUNCTION_WARNING),y)
CFLAGS += -include rte_warnings.h
endif
LDFLAGS += -L$(RTE_SDK_BIN)/lib
endif

export CFLAGS
export LDFLAGS

endif
