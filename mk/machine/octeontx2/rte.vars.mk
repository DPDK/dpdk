# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Marvell International Ltd
#

#
# machine:
#
#   - can define ARCH variable (overridden by cmdline value)
#   - can define CROSS variable (overridden by cmdline value)
#   - define MACHINE_CFLAGS variable (overridden by cmdline value)
#   - define MACHINE_LDFLAGS variable (overridden by cmdline value)
#   - define MACHINE_ASFLAGS variable (overridden by cmdline value)
#   - can define CPU_CFLAGS variable (overridden by cmdline value) that
#     overrides the one defined in arch.
#   - can define CPU_LDFLAGS variable (overridden by cmdline value) that
#     overrides the one defined in arch.
#   - can define CPU_ASFLAGS variable (overridden by cmdline value) that
#     overrides the one defined in arch.
#   - may override any previously defined variable
#

# ARCH =
# CROSS =
# MACHINE_CFLAGS =
# MACHINE_LDFLAGS =
# MACHINE_ASFLAGS =
# CPU_CFLAGS =
# CPU_LDFLAGS =
# CPU_ASFLAGS =

include $(RTE_SDK)/mk/rte.helper.mk

MACHINE_CFLAGS += $(call rte_cc_has_argument, -march=-mcpu=armv8.2-a+crc+crypto+lse)
MACHINE_CFLAGS += $(call rte_cc_has_argument, -mcpu=octeontx2)
