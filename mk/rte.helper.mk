# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Marvell International Ltd

# rte_cc_has_argument
# Usage: MACHINE_CFLAGS += $(call rte_cc_has_argument, -mno-avx512f)
# Return the argument if the argument is supported by the compiler.
#
define rte_cc_has_argument
	$(shell $(CC) -E $(1) -xc /dev/null 1>/dev/null 2>/dev/null && echo $(1))
endef
