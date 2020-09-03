# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2015-2020 Intel Corporation

subdirs := lib ethtool-app

.PHONY: all static shared clean $(subdirs)
all static shared clean: $(subdirs)

ethtool-app: lib
$(subdirs):
	$(MAKE) -C $@ $(MAKECMDGOALS)
