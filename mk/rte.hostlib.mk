# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

# tell rte.compile-pre.mk to use HOSTCC instead of CC
USE_HOST := 1
include $(RTE_SDK)/mk/internal/rte.compile-pre.mk
include $(RTE_SDK)/mk/internal/rte.install-pre.mk
include $(RTE_SDK)/mk/internal/rte.clean-pre.mk
include $(RTE_SDK)/mk/internal/rte.build-pre.mk

# VPATH contains at least SRCDIR
VPATH += $(SRCDIR)

_BUILD = $(HOSTLIB)
_INSTALL = $(INSTALL-FILES-y) $(SYMLINK-FILES-y) $(RTE_OUTPUT)/hostlib/$(HOSTLIB)
_CLEAN = doclean

.PHONY: all
all: install

.PHONY: install
install: build _postinstall

_postinstall: build

.PHONY: build
build: _postbuild

exe2cmd = $(strip $(call dotfile,$(patsubst %,%.cmd,$(1))))

O_TO_A = $(AR) crus $(HOSTLIB) $(OBJS-y)
O_TO_A_STR = $(subst ','\'',$(O_TO_A)) #'# fix syntax highlight
O_TO_A_DISP = $(if $(V),"$(O_TO_A_STR)","  HOSTAR $(@)")
O_TO_A_CMD = "cmd_$@ = $(O_TO_A_STR)"
O_TO_A_DO = @set -e; \
	echo $(O_TO_A_DISP); \
	$(O_TO_A) && \
	echo $(O_TO_A_CMD) > $(call exe2cmd,$(@))

-include .$(HOSTLIB).cmd

#
# Archive objects in .a file if needed
#
$(HOSTLIB): $(OBJS-y) FORCE
	@[ -d $(dir $@) ] || mkdir -p $(dir $@)
	$(if $(D),\
		@echo -n "$@ -> $< " ; \
		echo -n "file_missing=$(call boolean,$(file_missing)) " ; \
		echo -n "cmdline_changed=$(call boolean,$(call cmdline_changed,$(O_TO_A_STR))) " ; \
		echo -n "depfile_missing=$(call boolean,$(depfile_missing)) " ; \
		echo "depfile_newer=$(call boolean,$(depfile_newer)) ")
	$(if $(or \
		$(file_missing),\
		$(call cmdline_changed,$(O_TO_A_STR)),\
		$(depfile_missing),\
		$(depfile_newer)),\
		$(O_TO_A_DO))

#
# install lib in $(RTE_OUTPUT)/hostlib
#
$(RTE_OUTPUT)/hostlib/$(HOSTLIB): $(HOSTLIB)
	@echo "  INSTALL-HOSTLIB $(HOSTLIB)"
	@[ -d $(RTE_OUTPUT)/hostlib ] || mkdir -p $(RTE_OUTPUT)/hostlib
	$(Q)cp -f $(HOSTLIB) $(RTE_OUTPUT)/hostlib

#
# Clean all generated files
#
.PHONY: clean
clean: _postclean

.PHONY: doclean
doclean:
	$(Q)rm -rf $(HOSTLIB) $(OBJS-all) $(DEPS-all) $(DEPSTMP-all) \
	  $(CMDS-all) $(INSTALL-FILES-all)
	$(Q)rm -f $(_BUILD_TARGETS) $(_INSTALL_TARGETS) $(_CLEAN_TARGETS)

include $(RTE_SDK)/mk/internal/rte.compile-post.mk
include $(RTE_SDK)/mk/internal/rte.install-post.mk
include $(RTE_SDK)/mk/internal/rte.clean-post.mk
include $(RTE_SDK)/mk/internal/rte.build-post.mk

.PHONY: FORCE
FORCE:
