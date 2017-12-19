# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

ifeq (,$(wildcard $(RTE_OUTPUT)/.config))
  $(error "need a make config first")
else
  include $(RTE_SDK)/mk/rte.vars.mk
endif
ifeq (,$(wildcard $(RTE_OUTPUT)/Makefile))
  $(error "need a make config first")
endif

DATE := $(shell date '+%Y%m%d-%H%M')
AUTOTEST_DIR := $(RTE_OUTPUT)/autotest-$(DATE)

DIR := $(shell basename $(RTE_OUTPUT))

#
# test: launch auto-tests, very simple for now.
#
.PHONY: test test-basic test-fast test-perf coverage

PERFLIST=ring_perf,mempool_perf,memcpy_perf,hash_perf,timer_perf
coverage: BLACKLIST=-$(PERFLIST)
test-fast: BLACKLIST=-$(PERFLIST)
test-perf: WHITELIST=$(PERFLIST)

test test-basic test-fast test-perf:
	@mkdir -p $(AUTOTEST_DIR) ; \
	cd $(AUTOTEST_DIR) ; \
	if [ -f $(RTE_OUTPUT)/app/test ]; then \
		python $(RTE_SDK)/test/test/autotest.py \
			$(RTE_OUTPUT)/app/test \
			$(RTE_TARGET) \
			$(BLACKLIST) $(WHITELIST); \
	else \
		echo "No test found, please do a 'make test-build' first, or specify O=" ; \
	fi

# this is a special target to ease the pain of running coverage tests
# this runs all the autotests, cmdline_test script and dpdk-procinfo
coverage:
	@mkdir -p $(AUTOTEST_DIR) ; \
	cd $(AUTOTEST_DIR) ; \
	if [ -f $(RTE_OUTPUT)/app/test ]; then \
		python $(RTE_SDK)/test/cmdline_test/cmdline_test.py \
			$(RTE_OUTPUT)/app/cmdline_test; \
		ulimit -S -n 100 ; \
		python $(RTE_SDK)/test/test/autotest.py \
			$(RTE_OUTPUT)/app/test \
			$(RTE_TARGET) \
			$(BLACKLIST) $(WHITELIST) ; \
		$(RTE_OUTPUT)/app/dpdk-procinfo --file-prefix=ring_perf -- -m; \
	else \
		echo "No test found, please do a 'make test-build' first, or specify O=" ;\
	fi
