# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

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

MACHINE_CFLAGS = -march=native

# On FreeBSD systems, sometimes the correct CPU type is not picked up.
# To get everything to compile, we need SSE4.2 support, so check if that is
# reported by compiler. If not, check if the CPU actually supports it, and if
# so, set the compilation target to be a corei7, minimum target with SSE4.2.
SSE42_SUPPORT=$(shell $(CC) -march=native -dM -E - </dev/null | grep SSE4_2)
ifeq ($(SSE42_SUPPORT),)
    MACHINE_CFLAGS = -march=corei7
endif
