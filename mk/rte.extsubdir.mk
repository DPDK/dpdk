# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2014 6WIND S.A.

MAKEFLAGS += --no-print-directory

ALL_DEPDIRS := $(patsubst DEPDIRS-%,%,$(filter DEPDIRS-%,$(.VARIABLES)))

# output directory
O ?= $(CURDIR)
BASE_OUTPUT ?= $(abspath $(O))
CUR_SUBDIR ?= .

.PHONY: all
all: $(DIRS-y)

.PHONY: clean
clean: $(DIRS-y)

.PHONY: $(DIRS-y)
$(DIRS-y):
	@echo "== $@"
	$(Q)$(MAKE) -C $(@) \
		M=$(CURDIR)/$(@)/Makefile \
		O=$(BASE_OUTPUT)/$(CUR_SUBDIR)/$(@)/$(RTE_TARGET) \
		BASE_OUTPUT=$(BASE_OUTPUT) \
		CUR_SUBDIR=$(CUR_SUBDIR)/$(@) \
		S=$(CURDIR)/$(@) \
		$(filter-out $(DIRS-y),$(MAKECMDGOALS))

define depdirs_rule
$(DEPDIRS-$(1)):

$(1): | $(DEPDIRS-$(1))

$(if $(D),$(info $(1) depends on $(DEPDIRS-$(1))))
endef

$(foreach dir,$(ALL_DEPDIRS),\
	$(eval $(call depdirs_rule,$(dir))))
