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

DEFAULT_DPI ?= 300

ifeq ($(BASEDOCDIR),)
$(error "must be called from RTE root Makefile")
endif
ifeq ($(DOCDIR),)
$(error "must be called from RTE root Makefile")
endif

VPATH = $(abspath $(BASEDOCDIR)/$(DOCDIR))

pngfiles = $(patsubst %.svg,%.png,$(SVG))
pdfimgfiles = $(patsubst %.svg,%.pdf,$(SVG))
pdffiles = $(patsubst %.rst,%.pdf,$(RST))

.PHONY: all doc clean

compare = $(strip $(subst $(1),,$(2)) $(subst $(2),,$(1)))
dirname = $(patsubst %/,%,$(dir $1))

# windows only: this is needed for native programs that do not handle
# unix-like paths on win32
ifdef COMSPEC
winpath = "$(shell cygpath --windows $(abspath $(1)))"
else
winpath = $(1)
endif

all doc: $(pngfiles) $(pdffiles) $(DIRS)
	@true

pdfdoc: $(pngfiles) $(pdffiles) $(DIRS)
	@true

doxydoc: $(pdfimgfiles) $(DIRS)
	@true

.PHONY: $(DIRS)
$(DIRS):
	@[ -d $(CURDIR)/$@ ] || mkdir -p $(CURDIR)/$@
	$(Q)$(MAKE) DOCDIR=$(DOCDIR)/$@ BASEDOCDIR=$(BASEDOCDIR)/.. \
		-f $(RTE_SDK)/doc/$(DOCDIR)/$@/Makefile -C $(CURDIR)/$@ $(MAKECMDGOALS)

%.png: %.svg
	@echo "  INKSCAPE $(@)"
	$(Q)inkscape -d $(DEFAULT_DPI) -D -b ffffff -y 1.0 -e $(call winpath,$(@)) $(call winpath,$(<))

%.pdf: %.svg
	@echo "  INKSCAPE $(@)"
	$(Q)inkscape -d $(DEFAULT_DPI) -D -b ffffff -y 1.0 -A $(call winpath,$(@)) $(call winpath,$(<))

.SECONDEXPANSION:
$(foreach f,$(RST),$(eval DEP_$(f:%.rst=%.html) = $(DEP_$(f))))
%.html: %.rst $$(DEP_$$@)
	@echo "  RST2HTML $(@)"
	$(Q)mkdir -p `dirname $(@)` ; \
	python $(BASEDOCDIR)/gen/gen-common.py html $(BASEDOCDIR) > $(BASEDOCDIR)/gen/rte.rst ; \
	python $(BASEDOCDIR)/html/rst2html-highlight.py --link-stylesheet \
		--stylesheet-path=$(BASEDOCDIR)/html/rte.css \
		--strip-comments< $(<) > $(@) ; \

# there is a bug in rst2pdf (issue 311): replacement of DSTDIR is not
# what we expect: we should not have to add doc/
ifdef COMSPEC
WORKAROUND_PATH=$(BASEDOCDIR)
else
WORKAROUND_PATH=$(BASEDOCDIR)/doc
endif

.SECONDEXPANSION:
$(foreach f,$(RST),$(eval DEP_$(f:%.rst=%.pdf) = $(DEP_$(f))))
%.pdf: %.rst $$(DEP_$$@)
	@echo "  RST2PDF $(@)"
	$(Q)mkdir -p `dirname $(@)` ; \
	python $(BASEDOCDIR)/gen/gen-common.py pdf $(BASEDOCDIR) > $(BASEDOCDIR)/gen/rte.rst ; \
	rst2pdf -s $(BASEDOCDIR)/pdf/rte-stylesheet.json \
		--default-dpi=300 < $(<) > $(@)

CLEANDIRS = $(addsuffix _clean,$(DIRS))

docclean clean: $(CLEANDIRS)
	@rm -f $(htmlfiles) $(pdffiles) $(pngfiles) $(pdfimgfiles) $(BASEDOCDIR)/gen/rte.rst

%_clean:
	@if [ -f $(RTE_SDK)/doc/$(DOCDIR)/$*/Makefile -a -d $(CURDIR)/$* ]; then \
		$(MAKE) DOCDIR=$(DOCDIR)/$* BASEDOCDIR=$(BASEDOCDIR)/.. \
		-f $(RTE_SDK)/doc/$(DOCDIR)/$*/Makefile -C $(CURDIR)/$* clean ; \
	fi

.NOTPARALLEL:
