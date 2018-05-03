# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2014 6WIND S.A.

# examples application are seen as external applications which are
# not part of SDK.
BUILDING_RTE_SDK :=
export BUILDING_RTE_SDK

# Build directory is given with O=
O ?= $(RTE_SDK)/examples

# Target for which examples should be built.
T ?= *

# list all available configurations
EXAMPLES_CONFIGS := $(patsubst $(RTE_SRCDIR)/config/defconfig_%,%,\
	$(wildcard $(RTE_SRCDIR)/config/defconfig_$(T)))
EXAMPLES_TARGETS := $(addsuffix _examples,\
	$(filter-out %~,$(EXAMPLES_CONFIGS)))

.PHONY: examples
examples: $(EXAMPLES_TARGETS)

%_examples:
	@echo ================== Build examples for $*
	$(Q)if [ ! -d "${RTE_SDK}/${*}" ]; then \
		echo "Target ${*} does not exist in ${RTE_SDK}/${*}." ; \
		echo -n "Please install DPDK first (make install) or use another " ; \
		echo "target argument (T=target)." ; \
		false ; \
	else \
		$(MAKE) -C examples O=$(abspath $(O)) RTE_TARGET=$(*); \
	fi

EXAMPLES_CLEAN_TARGETS := $(addsuffix _examples_clean,\
	$(filter-out %~,$(EXAMPLES_CONFIGS)))

.PHONY: examples_clean
examples_clean: $(EXAMPLES_CLEAN_TARGETS)

%_examples_clean:
	@echo ================== Clean examples for $*
	$(Q)if [ ! -d "${RTE_SDK}/${*}" ]; then \
		echo "Target ${*} does not exist in ${RTE_SDK}/${*}." ; \
		echo -n "Please install DPDK first (make install) or use another " ; \
		echo "target argument (T=target)." ; \
		false ; \
	else \
		$(MAKE) -C examples O=$(abspath $(O)) RTE_TARGET=$(*) clean; \
	fi
