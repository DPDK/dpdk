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

ifdef O
ifeq ("$(origin O)", "command line")
$(error "Cannot use O= with doc target")
endif
endif

ifdef T
ifeq ("$(origin T)", "command line")
$(error "Cannot use T= with doc target")
endif
endif

.PHONY: doc
doc:
	$(Q)$(MAKE) -C $(RTE_SDK)/doc/images $@ BASEDOCDIR=.. DOCDIR=images
	$(Q)$(MAKE) -f $(RTE_SDK)/doc/rst/Makefile -C $(RTE_SDK)/doc/pdf pdfdoc BASEDOCDIR=.. DOCDIR=rst
	$(Q)$(MAKE) -f $(RTE_SDK)/mk/rte.sdkdoc.mk doxydoc

.PHONY: pdfdoc
pdfdoc:
	$(Q)$(MAKE) -C $(RTE_SDK)/doc/images $@ BASEDOCDIR=.. DOCDIR=images
	$(Q)$(MAKE) -f $(RTE_SDK)/doc/rst/Makefile -C $(RTE_SDK)/doc/pdf $@ BASEDOCDIR=.. DOCDIR=rst

.PHONY: doxydoc
doxydoc:
	$(Q)$(MAKE) -C $(RTE_SDK)/doc/images $@ BASEDOCDIR=.. DOCDIR=images
	$(Q)mkdir -p $(RTE_SDK)/doc/latex
	$(Q)cat $(RTE_SDK)/doc/gen/doxygen_pdf/Doxyfile | doxygen -
	$(Q)mv $(RTE_SDK)/doc/images/*.pdf $(RTE_SDK)/doc/latex/
	$(Q)sed -i s/darkgray/headercolour/g $(RTE_SDK)/doc/latex/doxygen.sty
	$(Q)cp $(RTE_SDK)/doc/gen/doxygen_pdf/Makefile_doxygen $(RTE_SDK)/doc/latex/Makefile
	$(Q)$(MAKE) -C $(RTE_SDK)/doc/latex
	$(Q)mv $(RTE_SDK)/doc/latex/refman.pdf $(RTE_SDK)/doc/api_gen.pdf
	$(Q)rm -rf $(RTE_SDK)/doc/latex

.PHONY: docclean
docclean:
	$(Q)$(MAKE) -C $(RTE_SDK)/doc/images $@ BASEDOCDIR=.. DOCDIR=images
	$(Q)$(MAKE) -f $(RTE_SDK)/doc/rst/Makefile -C $(RTE_SDK)/doc/pdf $@ BASEDOCDIR=.. DOCDIR=rst
	$(Q)rm -rf $(RTE_SDK)/doc/latex
