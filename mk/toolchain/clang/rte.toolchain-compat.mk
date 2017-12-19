# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

#
# CPUID-related options
#
# This was added to support compiler versions which might not support all the
# flags we need
#

# find out CLANG version

CLANG_VERSION := $(shell $(CC) -v 2>&1 | \
	sed -n "s/.*version \([0-9]*\.[0-9]*\).*/\1/p")

CLANG_MAJOR_VERSION := $(shell echo $(CLANG_VERSION) | cut -f1 -d.)

CLANG_MINOR_VERSION := $(shell echo $(CLANG_VERSION) | cut -f2 -d.)

ifeq ($(shell test $(CLANG_MAJOR_VERSION)$(CLANG_MINOR_VERSION) -lt 35 && echo 1), 1)
	CC_SUPPORTS_Z := false
endif
