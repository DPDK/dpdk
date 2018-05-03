# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2012-2013 6WIND S.A.

include $(RTE_SDK)/mk/internal/rte.compile-pre.mk
include $(RTE_SDK)/mk/internal/rte.install-pre.mk
include $(RTE_SDK)/mk/internal/rte.clean-pre.mk
include $(RTE_SDK)/mk/internal/rte.build-pre.mk

# VPATH contains at least SRCDIR
VPATH += $(SRCDIR)

_BUILD = $(SHARED)
_INSTALL = $(INSTALL-FILES-y) $(SYMLINK-FILES-y) $(RTE_OUTPUT)/lib/$(SHARED)
_CLEAN = doclean

# Set fPIC in CFLAGS for .so generation
CFLAGS += -fPIC

.PHONY: all
all: install

.PHONY: install
install: build _postinstall

_postinstall: build

.PHONY: build
build: _postbuild

exe2cmd = $(strip $(call dotfile,$(patsubst %,%.cmd,$(1))))

ifeq ($(LINK_USING_CC),1)
override EXTRA_LDFLAGS := $(call linkerprefix,$(EXTRA_LDFLAGS))
O_TO_SO = $(CC) $(call linkerprefix,$(LDFLAGS)) $(LDFLAGS_$(@)) $(EXTRA_LDFLAGS) \
	-shared -o $@ $(OBJS-y) $(call linkerprefix,$(LDLIBS))
else
O_TO_SO = $(LD) $(LDFLAGS) $(LDFLAGS_$(@)) $(EXTRA_LDFLAGS) \
	-shared -o $@ $(OBJS-y) $(LDLIBS)
endif

O_TO_SO_STR = $(subst ','\'',$(O_TO_SO)) #'# fix syntax highlight
O_TO_SO_DISP = $(if $(V),"$(O_TO_SO_STR)","  LD $(@)")
O_TO_SO_CMD = "cmd_$@ = $(O_TO_SO_STR)"
O_TO_SO_DO = @set -e; \
	echo $(O_TO_SO_DISP); \
	$(O_TO_SO) && \
	echo $(O_TO_SO_CMD) > $(call exe2cmd,$(@))

-include .$(SHARED).cmd

# path where libraries are retrieved
LDLIBS_PATH := $(subst -Wl$(comma)-L,,$(filter -Wl$(comma)-L%,$(LDLIBS)))
LDLIBS_PATH += $(subst -L,,$(filter -L%,$(LDLIBS)))

# list of .a files that are linked to this application
LDLIBS_NAMES := $(patsubst -l%,lib%.a,$(filter -l%,$(LDLIBS)))
LDLIBS_NAMES += $(patsubst -Wl$(comma)-l%,lib%.a,$(filter -Wl$(comma)-l%,$(LDLIBS)))

# list of found libraries files (useful for deps). If not found, the
# library is silently ignored and dep won't be checked
LDLIBS_FILES := $(sort $(wildcard $(foreach dir,$(LDLIBS_PATH),\
	$(addprefix $(dir)/,$(LDLIBS_NAMES)))))

#
# Archive objects in .so file if needed
#
$(SHARED): $(OBJS-y) $(LDLIBS_FILES) $(DEP_$(SHARED)) FORCE
	@[ -d $(dir $@) ] || mkdir -p $(dir $@)
	$(if $(D),\
		@echo -n "$< -> $@ " ; \
		echo -n "file_missing=$(call boolean,$(file_missing)) " ; \
		echo -n "cmdline_changed=$(call boolean,$(call cmdline_changed,$(O_TO_SO_STR))) " ; \
		echo -n "depfile_missing=$(call boolean,$(depfile_missing)) " ; \
		echo "depfile_newer=$(call boolean,$(depfile_newer)) ")
	$(if $(or \
		$(file_missing),\
		$(call cmdline_changed,$(O_TO_SO_STR)),\
		$(depfile_missing),\
		$(depfile_newer)),\
		$(O_TO_SO_DO))

#
# install lib in $(RTE_OUTPUT)/lib
#
$(RTE_OUTPUT)/lib/$(SHARED): $(SHARED)
	@echo "  INSTALL-SHARED $(SHARED)"
	@[ -d $(RTE_OUTPUT)/lib ] || mkdir -p $(RTE_OUTPUT)/lib
	$(Q)cp -f $(SHARED) $(RTE_OUTPUT)/lib

#
# Clean all generated files
#
.PHONY: clean
clean: _postclean

.PHONY: doclean
doclean:
	$(Q)rm -rf $(SHARED) $(OBJS-all) $(DEPS-all) $(DEPSTMP-all) \
	  $(CMDS-all) $(INSTALL-FILES-all)
	$(Q)rm -f $(_BUILD_TARGETS) $(_INSTALL_TARGETS) $(_CLEAN_TARGETS)

include $(RTE_SDK)/mk/internal/rte.compile-post.mk
include $(RTE_SDK)/mk/internal/rte.install-post.mk
include $(RTE_SDK)/mk/internal/rte.clean-post.mk
include $(RTE_SDK)/mk/internal/rte.build-post.mk

.PHONY: FORCE
FORCE:
