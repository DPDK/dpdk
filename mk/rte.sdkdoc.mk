# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2015 Intel Corporation.
# Copyright(c) 2013-2015 6WIND S.A.

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

API_EXAMPLES := $(RTE_OUTPUT)/doc/html/examples.dox

.PHONY: help
help:
	@cat $(RTE_SDK)/doc/build-sdk-quick.txt
	@$(MAKE) -rR showconfigs | sed 's,^,\t\t\t\t,'

.PHONY: all
all: api-html guides-html guides-pdf

.PHONY: clean
clean: api-html-clean guides-html-clean guides-pdf-clean guides-man-clean

.PHONY: api-html
api-html: $(API_EXAMPLES)
	@echo 'doxygen for API...'
	$(Q)mkdir -p $(RTE_OUTPUT)/doc/html
	$(Q)(cat $(RTE_SDK)/doc/api/doxy-api.conf     && \
	    printf 'PROJECT_NUMBER = '                && \
	                     $(MAKE) -rRs showversion && \
	    echo INPUT           += $(API_EXAMPLES)   && \
	    echo OUTPUT_DIRECTORY = $(RTE_OUTPUT)/doc && \
	    echo HTML_OUTPUT      = html/api          && \
	    echo GENERATE_HTML    = YES               && \
	    echo GENERATE_LATEX   = NO                && \
	    echo GENERATE_MAN     = NO                )| \
	    doxygen -
	$(Q)$(RTE_SDK)/doc/api/doxy-html-custom.sh $(RTE_OUTPUT)/doc/html/api/doxygen.css

.PHONY: api-html-clean
api-html-clean:
	$(Q)rm -f $(API_EXAMPLES)
	$(Q)rm -f $(RTE_OUTPUT)/doc/html/api/*
	$(Q)rmdir -p --ignore-fail-on-non-empty $(RTE_OUTPUT)/doc/html/api 2>&- || true

$(API_EXAMPLES): api-html-clean
	$(Q)mkdir -p $(@D)
	@printf '/**\n' > $(API_EXAMPLES)
	@printf '@page examples DPDK Example Programs\n\n' >> $(API_EXAMPLES)
	@find examples -type f -name '*.c' -printf '@example %p\n' | LC_ALL=C sort >> $(API_EXAMPLES)
	@printf '*/\n' >> $(API_EXAMPLES)

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

guides-html-prepare:
	$(Q)install -D -m0644 $(RTE_SDK)/doc/guides/custom.css \
		$(RTE_OUTPUT)/doc/html/guides/_static/css/custom.css

guides-%-prepare: ;

guides-%: guides-%-prepare
	@echo 'sphinx processing $@...'
	$(Q)$(RTE_SPHINX_BUILD) -b $* $(RTE_SPHINX_VERBOSE) \
		-c $(RTE_SDK)/doc/guides $(RTE_SDK)/doc/guides \
		$(RTE_OUTPUT)/doc/$*/guides

# Each PDF depends on generated images *.pdf from *.svg
$(foreach guide, $(RTE_GUIDES), $(foreach img, $(wildcard $(guide)img/*.svg), \
	$(eval guides-pdf-$(notdir $(guide:/=)): $(img:svg=pdf))))
%.pdf: %.svg
	$(Q)inkscape -d $(RTE_PDF_DPI) -D -f $< -A $@ $(RTE_INKSCAPE_VERBOSE)
