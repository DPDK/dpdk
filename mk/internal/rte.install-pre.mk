# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

#
# get all variables starting with "INSTALL-y-", and extract the
# installation dir and path
#
INSTALL-y := $(filter INSTALL-y-%,$(.VARIABLES))
INSTALL-n := $(filter INSTALL-n-%,$(.VARIABLES))
INSTALL- := $(filter INSTALL--%,$(.VARIABLES))
INSTALL-DIRS-y := $(patsubst INSTALL-y-%,%,$(INSTALL-y))
INSTALL-FILES-y := $(foreach i,$(INSTALL-DIRS-y),\
	$(addprefix $(RTE_OUTPUT)/$(i)/,$(notdir $(INSTALL-y-$(i)))))
INSTALL-FILES-all := $(foreach i,$(INSTALL-DIRS-y) $(INSTALL-DIRS-n) $(INSTALL-DIRS-),\
	$(addprefix $(RTE_OUTPUT)/$(i)/,$(notdir $(INSTALL-y-$(i)))))

_INSTALL_TARGETS := _preinstall _install _postinstall

#
# get all variables starting with "SYMLINK-y-", and extract the
# installation dir and path
#
SYMLINK-y := $(filter SYMLINK-y-%,$(.VARIABLES))
SYMLINK-n := $(filter SYMLINK-n-%,$(.VARIABLES))
SYMLINK- := $(filter SYMLINK--%,$(.VARIABLES))
SYMLINK-DIRS-y := $(patsubst SYMLINK-y-%,%,$(SYMLINK-y))
SYMLINK-FILES-y := $(foreach i,$(SYMLINK-DIRS-y),\
	$(addprefix $(RTE_OUTPUT)/$(i)/,$(notdir $(SYMLINK-y-$(i)))))
SYMLINK-FILES-all := $(foreach i,$(SYMLINK-DIRS-y) $(SYMLINK-DIRS-n) $(SYMLINK-DIRS-),\
	$(addprefix $(RTE_OUTPUT)/$(i)/,$(notdir $(SYMLINK-y-$(i)))))

_SYMLINK_TARGETS := _presymlink _symlink _postsymlink
