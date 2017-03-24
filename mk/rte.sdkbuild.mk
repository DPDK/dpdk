#   BSD LICENSE
#
#   Copyright(c) 2010-2015 Intel Corporation. All rights reserved.
#   All rights reserved.
#
#   Redistribution and use in source and binary forms, with or without
#   modification, are permitted provided that the following conditions
#   are met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in
#       the documentation and/or other materials provided with the
#       distribution.
#     * Neither the name of Intel Corporation nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
#   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
#   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
#   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
#   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
#   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
#   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
#   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
#   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
#   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
#   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
#   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#
# include rte.vars.mk if config file exists
#
ifeq (,$(wildcard $(RTE_OUTPUT)/.config))
  $(error "need a make config first")
else
  include $(RTE_SDK)/mk/rte.vars.mk
endif

buildtools: | lib
drivers: | lib buildtools
app: | lib buildtools drivers
test: | lib buildtools drivers

#
# build and clean targets
#

CLEANDIRS = $(addsuffix _clean,$(ROOTDIRS-y) $(ROOTDIRS-n) $(ROOTDIRS-))

.PHONY: build
build: $(ROOTDIRS-y)
	@echo "Build complete [$(RTE_TARGET)]"

.PHONY: clean
clean: $(CLEANDIRS)
	@rm -rf $(RTE_OUTPUT)/include $(RTE_OUTPUT)/app \
		$(RTE_OUTPUT)/lib \
		$(RTE_OUTPUT)/hostlib $(RTE_OUTPUT)/kmod
	@[ -d $(RTE_OUTPUT)/include ] || mkdir -p $(RTE_OUTPUT)/include
	@$(RTE_SDK)/buildtools/gen-config-h.sh $(RTE_OUTPUT)/.config \
		> $(RTE_OUTPUT)/include/rte_config.h
	$(Q)$(MAKE) -f $(RTE_SDK)/GNUmakefile gcovclean
	@echo Clean complete

.PHONY: test-build
test-build: test

.SECONDEXPANSION:
.PHONY: $(ROOTDIRS-y) $(ROOTDIRS-)
$(ROOTDIRS-y) $(ROOTDIRS-):
	@[ -d $(BUILDDIR)/$@ ] || mkdir -p $(BUILDDIR)/$@
	@echo "== Build $@"
	$(Q)$(MAKE) S=$@ -f $(RTE_SRCDIR)/$@/Makefile -C $(BUILDDIR)/$@ all
	@if [ $@ = drivers ]; then \
		$(MAKE) -f $(RTE_SDK)/mk/rte.combinedlib.mk; \
	fi

%_clean:
	@echo "== Clean $*"
	$(Q)if [ -f $(RTE_SRCDIR)/$*/Makefile -a -d $(BUILDDIR)/$* ]; then \
		$(MAKE) S=$* -f $(RTE_SRCDIR)/$*/Makefile -C $(BUILDDIR)/$* clean ; \
	fi

RTE_MAKE_SUBTARGET ?= all

%_sub: $(addsuffix _sub,$(*))
	@echo $(addsuffix _sub,$(*))
	@[ -d $(BUILDDIR)/$* ] || mkdir -p $(BUILDDIR)/$*
	@echo "== Build $*"
	$(Q)$(MAKE) S=$* -f $(RTE_SRCDIR)/$*/Makefile -C $(BUILDDIR)/$* \
		$(RTE_MAKE_SUBTARGET)

.PHONY: all
all: build

.PHONY: FORCE
FORCE:
