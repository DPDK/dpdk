# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2015 Intel Corporation

#
# include rte.vars.mk if config file exists
#
ifeq (,$(wildcard $(RTE_OUTPUT)/.config))
  $(error "need a make config first")
else
  include $(RTE_SDK)/mk/rte.vars.mk
endif

# allow exec-env specific targets
-include $(RTE_SDK)/mk/exec-env/$(RTE_EXEC_ENV)/rte.custom.mk

buildtools: | lib
kernel: | lib
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
	@echo "==== NOTE ===="
	@echo "It is recommended to build DPDK using 'meson' and 'ninja'"
	@echo "See https://doc.dpdk.org/guides/linux_gsg/build_dpdk.html for instructions"
	@echo "Building DPDK with 'make' will be deprecated in a future release"
	@echo "=============="

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
