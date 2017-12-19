# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

ifdef T
ifeq ("$(origin T)", "command line")
$(error "Cannot use T= with gcov target")
endif
endif

ifeq (,$(wildcard $(RTE_OUTPUT)/.config))
  $(error "need a make config first")
else
  include $(RTE_SDK)/mk/rte.vars.mk
endif
ifeq (,$(wildcard $(RTE_OUTPUT)/Makefile))
  $(error "need a make config first")
endif

INPUTDIR  = $(RTE_OUTPUT)
OUTPUTDIR =  $(RTE_OUTPUT)/gcov

.PHONY: gcovclean
gcovclean:
	$(Q)find $(INPUTDIR)/build -name "*.gcno" -o -name "*.gcda" -exec rm {} \;
	$(Q)rm -rf $(OUTPUTDIR)

.PHONY: gcov
gcov:
	$(Q)for APP in test ; do \
		mkdir -p $(OUTPUTDIR)/$$APP ; cd $(OUTPUTDIR)/$$APP ; \
		for FIC in `strings $(RTE_OUTPUT)/app/$$APP | grep gcda | sed s,gcda,o,` ; do \
			SUBDIR=`basename $$FIC`;\
			mkdir $$SUBDIR ;\
			cd $$SUBDIR ;\
			$(GCOV) $(RTE_OUTPUT)/app/$$APP -o $$FIC > gcov.log; \
			cd - >/dev/null;\
		done ; \
		cd - >/dev/null; \
	done
