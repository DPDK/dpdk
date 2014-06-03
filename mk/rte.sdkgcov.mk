#   BSD LICENSE
#
#   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
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
