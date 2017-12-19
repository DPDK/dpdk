# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

include $(RTE_SDK)/mk/internal/rte.compile-pre.mk
include $(RTE_SDK)/mk/internal/rte.install-pre.mk
include $(RTE_SDK)/mk/internal/rte.clean-pre.mk
include $(RTE_SDK)/mk/internal/rte.build-pre.mk

# VPATH contains at least SRCDIR
VPATH += $(SRCDIR)

ifneq ($(OBJ),)
_BUILD = $(OBJ)
else
_BUILD = $(OBJS-y)
endif
_INSTALL = $(INSTALL-FILES-y) $(SYMLINK-FILES-y)
_CLEAN = doclean

.PHONY: all
all: install

.PHONY: install
install: build _postinstall

_postinstall: build

.PHONY: build
build: _postbuild

ifneq ($(OBJ),)
exe2cmd = $(strip $(call dotfile,$(patsubst %,%.cmd,$(1))))

O_TO_O = $(LD) -r -o $(OBJ) $(OBJS-y)
O_TO_O_STR = $(subst ','\'',$(O_TO_O)) #'# fix syntax highlight
O_TO_O_DISP =  $(if $(V),"$(O_TO_O_STR)","  LD $(@)")
O_TO_O_CMD = "cmd_$@ = $(O_TO_O_STR)"
O_TO_O_DO = @set -e; \
	echo $(O_TO_O_DISP); \
	$(O_TO_O) && \
	echo $(O_TO_O_CMD) > $(call exe2cmd,$(@))

-include .$(OBJ).cmd

#
# Archive objects in .a file if needed
#
$(OBJ): $(OBJS-y) FORCE
	@[ -d $(dir $@) ] || mkdir -p $(dir $@)
	$(if $(D),\
		@echo -n "$< -> $@ " ; \
		echo -n "file_missing=$(call boolean,$(file_missing)) " ; \
		echo -n "cmdline_changed=$(call boolean,$(call cmdline_changed,$(O_TO_O_STR))) " ; \
		echo -n "depfile_missing=$(call boolean,$(depfile_missing)) " ; \
		echo "depfile_newer=$(call boolean,$(depfile_newer)) ")
	$(if $(or \
		$(file_missing),\
		$(call cmdline_changed,$(O_TO_O_STR)),\
		$(depfile_missing),\
		$(depfile_newer)),\
		$(O_TO_O_DO))
endif

#
# Clean all generated files
#
.PHONY: clean
clean: _postclean

.PHONY: doclean
doclean:
	@rm -rf $(OBJ) $(OBJS-all) $(DEPS-all) $(DEPSTMP-all) \
	  $(CMDS-all) $(INSTALL-FILES-all)
	@rm -f $(_BUILD_TARGETS) $(_INSTALL_TARGETS) $(_CLEAN_TARGETS)

include $(RTE_SDK)/mk/internal/rte.compile-post.mk
include $(RTE_SDK)/mk/internal/rte.install-post.mk
include $(RTE_SDK)/mk/internal/rte.clean-post.mk
include $(RTE_SDK)/mk/internal/rte.build-post.mk

.PHONY: FORCE
FORCE:
