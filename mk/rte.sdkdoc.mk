#   BSD LICENSE
#
#   Copyright(c) 2010-2015 Intel Corporation. All rights reserved.
#   Copyright(c) 2013-2015 6WIND S.A.
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
$(error "Cannot use T= with doc target")
endif
endif

RTE_SPHINX_BUILD = sphinx-build
RTE_PDFLATEX_VERBOSE := --interaction=nonstopmode

ifndef V
RTE_SPHINX_VERBOSE := -q
RTE_PDFLATEX_VERBOSE := --interaction=batchmode
RTE_INKSCAPE_VERBOSE := >/dev/null 2>&1
endif
ifeq '$V' '0'
RTE_SPHINX_VERBOSE := -q
RTE_PDFLATEX_VERBOSE := --interaction=batchmode
RTE_INKSCAPE_VERBOSE := >/dev/null 2>&1
endif

RTE_PDF_DPI ?= 300

RTE_GUIDES := $(filter %/, $(wildcard $(RTE_SDK)/doc/guides/*/))

.PHONY: help
help:
	@cat $(RTE_SDK)/doc/build-sdk-quick.txt
	@$(MAKE) -rR showconfigs | sed 's,^,\t\t\t\t,'

.PHONY: all
all: api-html guides-html guides-pdf

.PHONY: clean
clean: api-html-clean guides-html-clean guides-pdf-clean

.PHONY: api-html
api-html: api-html-clean
	@echo 'doxygen for API...'
	$(Q)mkdir -p $(RTE_OUTPUT)/doc/html
	$(Q)(cat $(RTE_SDK)/doc/api/doxy-api.conf     && \
	    printf 'PROJECT_NUMBER = '                && \
	                      $(MAKE) -rR showversion && \
	    echo OUTPUT_DIRECTORY = $(RTE_OUTPUT)/doc && \
	    echo HTML_OUTPUT      = html/api          && \
	    echo GENERATE_HTML    = YES               && \
	    echo GENERATE_LATEX   = NO                && \
	    echo GENERATE_MAN     = NO                )| \
	    doxygen -
	$(Q)$(RTE_SDK)/doc/api/doxy-html-custom.sh $(RTE_OUTPUT)/doc/html/api/doxygen.css

.PHONY: api-html-clean
api-html-clean:
	$(Q)rm -f $(RTE_OUTPUT)/doc/html/api/*
	$(Q)rmdir -p --ignore-fail-on-non-empty $(RTE_OUTPUT)/doc/html/api 2>&- || true

guides-pdf-clean: guides-pdf-img-clean
guides-pdf-img-clean:
	$(Q)rm -f $(RTE_SDK)/doc/guides/*/img/*.pdf

guides-%-clean:
	$(Q)rm -rf $(RTE_OUTPUT)/doc/$*/guides
	$(Q)rmdir -p --ignore-fail-on-non-empty $(RTE_OUTPUT)/doc/$* 2>&- || true

guides-pdf: $(addprefix guides-pdf-, $(notdir $(RTE_GUIDES:/=))) ;
guides-pdf-%:
	@echo 'sphinx processing $@...'
	$(Q)$(RTE_SPHINX_BUILD) -b latex $(RTE_SPHINX_VERBOSE) \
		-c $(RTE_SDK)/doc/guides $(RTE_SDK)/doc/guides/$* \
		$(RTE_OUTPUT)/doc/pdf/guides/$*
	$(if $^,$(Q)rm -f $^)
	@echo 'pdflatex processing $@...'
	$(Q)$(MAKE) all-pdf -sC $(RTE_OUTPUT)/doc/pdf/guides/$* \
		LATEXOPTS=$(RTE_PDFLATEX_VERBOSE)
	$(Q)mv $(RTE_OUTPUT)/doc/pdf/guides/$*/doc.pdf \
		$(RTE_OUTPUT)/doc/pdf/guides/$*.pdf

guides-%:
	@echo 'sphinx processing $@...'
	$(Q)$(RTE_SPHINX_BUILD) -b $* $(RTE_SPHINX_VERBOSE) \
		-c $(RTE_SDK)/doc/guides $(RTE_SDK)/doc/guides \
		$(RTE_OUTPUT)/doc/$*/guides

# Each PDF depends on generated images *.pdf from *.svg
$(foreach guide, $(RTE_GUIDES), $(foreach img, $(wildcard $(guide)img/*.svg), \
	$(eval guides-pdf-$(notdir $(guide:/=)): $(img:svg=pdf))))
%.pdf: %.svg
	$(Q)inkscape -d $(RTE_PDF_DPI) -D -f $< -A $@ $(RTE_INKSCAPE_VERBOSE)
