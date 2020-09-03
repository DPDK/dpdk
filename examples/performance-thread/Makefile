# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2015-2020 Intel Corporation

subdirs := l3fwd-thread pthread_shim

.PHONY: all static shared clean $(subdirs)
all static shared clean: $(subdirs)

ifeq ($(filter $(shell uname -m),x86_64 arm64),)
$(error This application is only supported for x86_64 and arm64 targets)
endif

$(subdirs):
	$(MAKE) -C $@ $(MAKECMDGOALS)
