# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

#
# .mk to build subdirectories
#

include $(RTE_SDK)/mk/internal/rte.install-pre.mk
include $(RTE_SDK)/mk/internal/rte.clean-pre.mk
include $(RTE_SDK)/mk/internal/rte.build-pre.mk

ALL_DEPDIRS := $(patsubst DEPDIRS-%,%,$(filter DEPDIRS-%,$(.VARIABLES)))

CLEANDIRS = $(addsuffix _clean,$(DIRS-y) $(DIRS-n) $(DIRS-))

VPATH += $(SRCDIR)
_BUILD = $(DIRS-y)
_INSTALL = $(INSTALL-FILES-y) $(SYMLINK-FILES-y)
_CLEAN = $(CLEANDIRS)

.PHONY: all
all: install

.PHONY: install
install: build _postinstall

_postinstall: build

.PHONY: build
build: _postbuild

.SECONDEXPANSION:
.PHONY: $(DIRS-y)
$(DIRS-y):
	@[ -d $(CURDIR)/$@ ] || mkdir -p $(CURDIR)/$@
	@echo "== Build $S/$@"
	@$(MAKE) S=$S/$@ -f $(SRCDIR)/$@/Makefile -C $(CURDIR)/$@ all

.PHONY: clean
clean: _postclean

%_clean:
	@echo "== Clean $S/$*"
	@if [ -f $(SRCDIR)/$*/Makefile -a -d $(CURDIR)/$* ]; then \
		$(MAKE) S=$S/$* -f $(SRCDIR)/$*/Makefile -C $(CURDIR)/$* clean ; \
	fi
	@rm -f $(_BUILD_TARGETS) $(_INSTALL_TARGETS) $(_CLEAN_TARGETS)

define depdirs_rule
$(DEPDIRS-$(1)):

$(1): | $(DEPDIRS-$(1))

$(if $(D),$(info $(1) depends on $(DEPDIRS-$(1))))
endef

$(foreach dir,$(ALL_DEPDIRS),\
	$(eval $(call depdirs_rule,$(dir))))

include $(RTE_SDK)/mk/internal/rte.install-post.mk
include $(RTE_SDK)/mk/internal/rte.clean-post.mk
include $(RTE_SDK)/mk/internal/rte.build-post.mk

.PHONY: FORCE
FORCE:
