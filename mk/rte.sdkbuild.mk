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

# If DESTDIR variable is given, install binary dpdk

#
# include rte.vars.mk if config file exists
#
ifeq (,$(wildcard $(RTE_OUTPUT)/.config))
  $(error "need a make config first")
else
  include $(RTE_SDK)/mk/rte.vars.mk
endif

#
# include .depdirs and define rules to order priorities between build
# of directories.
#
-include $(RTE_OUTPUT)/.depdirs

define depdirs_rule
$(1): $(sort $(LOCAL_DEPDIRS-$(1)))
endef

$(foreach d,$(ROOTDIRS-y),$(eval $(call depdirs_rule,$(d))))

#
# build and clean targets
#

CLEANDIRS = $(addsuffix _clean,$(ROOTDIRS-y) $(ROOTDIRS-n) $(ROOTDIRS-))

.PHONY: build
build: $(ROOTDIRS-y)
	@echo "Build complete [$(RTE_TARGET)]"
ifneq ($(DESTDIR),)
	$(Q)mkdir -p $(DESTDIR)
	$(Q)tar -C $(RTE_SDK) -cf - mk scripts/*.sh | tar -C $(DESTDIR) -x \
	  --keep-newer-files --warning=no-ignore-newer -f -
	$(Q)mkdir -p $(DESTDIR)/`basename $(RTE_OUTPUT)`
	$(Q)tar -C $(RTE_OUTPUT) -chf - \
	  --exclude app --exclude hostapp --exclude build \
	  --exclude Makefile --exclude .depdirs . | \
	  tar -C $(DESTDIR)/`basename $(RTE_OUTPUT)` -x --keep-newer-files \
	  --warning=no-ignore-newer -f -
	$(Q)install -D $(RTE_OUTPUT)/app/testpmd \
	  $(DESTDIR)/`basename $(RTE_OUTPUT)`/app/testpmd
	@echo Installation in $(DESTDIR) complete
endif

.PHONY: clean
clean: $(CLEANDIRS)
	@rm -rf $(RTE_OUTPUT)/include $(RTE_OUTPUT)/app \
		$(RTE_OUTPUT)/hostapp $(RTE_OUTPUT)/lib \
		$(RTE_OUTPUT)/hostlib $(RTE_OUTPUT)/kmod
	@[ -d $(RTE_OUTPUT)/include ] || mkdir -p $(RTE_OUTPUT)/include
	@$(RTE_SDK)/scripts/gen-config-h.sh $(RTE_OUTPUT)/.config \
		> $(RTE_OUTPUT)/include/rte_config.h
	$(Q)$(MAKE) -f $(RTE_SDK)/GNUmakefile gcovclean
	@echo Clean complete

.SECONDEXPANSION:
.PHONY: $(ROOTDIRS-y)
$(ROOTDIRS-y):
	@[ -d $(BUILDDIR)/$@ ] || mkdir -p $(BUILDDIR)/$@
	@echo "== Build $@"
	$(Q)$(MAKE) S=$@ -f $(RTE_SRCDIR)/$@/Makefile -C $(BUILDDIR)/$@ all
	@if [ $@ = drivers -a $(CONFIG_RTE_BUILD_COMBINE_LIBS) = y ]; then \
		$(MAKE) -f $(RTE_SDK)/lib/Makefile sharelib; \
	fi

%_clean:
	@echo "== Clean $*"
	$(Q)if [ -f $(RTE_SRCDIR)/$*/Makefile -a -d $(BUILDDIR)/$* ]; then \
		$(MAKE) S=$* -f $(RTE_SRCDIR)/$*/Makefile -C $(BUILDDIR)/$* clean ; \
	fi

RTE_MAKE_SUBTARGET ?= all

%_sub: $(addsuffix _sub,$(FULL_DEPDIRS-$(*)))
	@echo $(addsuffix _sub,$(FULL_DEPDIRS-$(*)))
	@[ -d $(BUILDDIR)/$* ] || mkdir -p $(BUILDDIR)/$*
	@echo "== Build $*"
	$(Q)$(MAKE) S=$* -f $(RTE_SRCDIR)/$*/Makefile -C $(BUILDDIR)/$* \
		$(RTE_MAKE_SUBTARGET)

.PHONY: all
all: build

.PHONY: FORCE
FORCE:
