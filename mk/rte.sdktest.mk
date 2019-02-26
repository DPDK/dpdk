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
.PHONY: test test-fast test-perf test-drivers test-dump coverage

PERFLIST=ring_perf,mempool_perf,memcpy_perf,hash_perf,timer_perf,\
         reciprocal_division,reciprocal_division_perf,lpm_perf,red_all,\
         barrier,hash_multiwriter,timer_racecond,efd,hash_functions,\
         eventdev_selftest_sw,member_perf,efd_perf,lpm6_perf,red_perf,\
         distributor_perf,ring_pmd_perf,pmd_perf,ring_perf
DRIVERSLIST=link_bonding,link_bonding_mode4,link_bonding_rssconf,\
            cryptodev_sw_mrvl,cryptodev_dpaa2_sec,cryptodev_dpaa_sec,\
            cryptodev_qat,cryptodev_aesni_mb,cryptodev_openssl,\
            cryptodev_scheduler,cryptodev_aesni_gcm,cryptodev_null,\
            cryptodev_sw_snow3g,cryptodev_sw_kasumi,cryptodev_sw_zuc
DUMPLIST=dump_struct_sizes,dump_mempool,dump_malloc_stats,dump_devargs,\
         dump_log_types,dump_ring,dump_physmem,dump_memzone

SPACESTR:=
SPACESTR+=
STRIPPED_PERFLIST=$(subst $(SPACESTR),,$(PERFLIST))
STRIPPED_DRIVERSLIST=$(subst $(SPACESTR),,$(DRIVERSLIST))
STRIPPED_DUMPLIST=$(subst $(SPACESTR),,$(DUMPLIST))

coverage: BLACKLIST=-$(STRIPPED_PERFLIST)
test-fast: BLACKLIST=-$(STRIPPED_PERFLIST),$(STRIPPED_DRIVERSLIST),$(STRIPPED_DUMPLIST)
test-perf: WHITELIST=$(STRIPPED_PERFLIST)
test-drivers: WHITELIST=$(STRIPPED_DRIVERSLIST)
test-dump: WHITELIST=$(STRIPPED_DUMPLIST)

test test-fast test-perf test-drivers test-dump:
	@mkdir -p $(AUTOTEST_DIR) ; \
	cd $(AUTOTEST_DIR) ; \
	if [ -f $(RTE_OUTPUT)/app/test ]; then \
		python $(RTE_SDK)/app/test/autotest.py \
			$(RTE_OUTPUT)/app/test \
			$(RTE_TARGET) \
			$(BLACKLIST) $(WHITELIST); \
	else \
		echo "No test found, please do a 'make' first, or specify O=" ; \
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
		python $(RTE_SDK)/app/test/autotest.py \
			$(RTE_OUTPUT)/app/test \
			$(RTE_TARGET) \
			$(BLACKLIST) $(WHITELIST) ; \
		$(RTE_OUTPUT)/app/dpdk-procinfo --file-prefix=ring_perf -- -m; \
	else \
		echo "No test found, please do a 'make' first, or specify O=" ;\
	fi
