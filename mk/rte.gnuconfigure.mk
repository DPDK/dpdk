# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

include $(RTE_SDK)/mk/internal/rte.build-pre.mk
include $(RTE_SDK)/mk/internal/rte.install-pre.mk
include $(RTE_SDK)/mk/internal/rte.clean-pre.mk

# VPATH contains at least SRCDIR
VPATH += $(SRCDIR)
_BUILD = configure
_INSTALL = $(INSTALL-FILES-y) $(SYMLINK-FILES-y)
_CLEAN = doclean

.PHONY: all
all: install

.PHONY: install
install: build _postinstall

_postinstall: build

.PHONY: build
build: _postbuild

configure:
	$(Q)cd $(CONFIGURE_PATH) ; \
	./configure --prefix $(CONFIGURE_PREFIX) $(CONFIGURE_ARGS) ; \
	make ; \
	make install

.PHONY: clean
clean: _postclean

.PHONY: doclean
doclean:
	$(Q)cd $(CONFIGURE_PATH) ; make clean
	$(Q)rm -f $(_INSTALL_TARGETS) $(_CLEAN_TARGETS)

include $(RTE_SDK)/mk/internal/rte.build-post.mk
include $(RTE_SDK)/mk/internal/rte.install-post.mk
include $(RTE_SDK)/mk/internal/rte.clean-post.mk

.PHONY: FORCE
FORCE:
