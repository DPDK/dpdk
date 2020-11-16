# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2020 Intel Corporation

subdirs := client_server_mp hotplug_mp simple_mp symmetric_mp

.PHONY: all static shared clean $(subdirs)
all static shared clean: $(subdirs)

$(subdirs):
	$(MAKE) -C $@ $(MAKECMDGOALS)
