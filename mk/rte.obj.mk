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

include $(RTE_SDK)/mk/internal/rte.compile-pre.mk
include $(RTE_SDK)/mk/internal/rte.install-pre.mk
include $(RTE_SDK)/mk/internal/rte.clean-pre.mk
include $(RTE_SDK)/mk/internal/rte.build-pre.mk

# VPATH contains at least SRCDIR
VPATH += $(SRCDIR)

ifneq ($(OBJ),)
_BUILD = $(OBJ)
else
_BUILD = $(OBJS-y)
endif
_INSTALL = $(INSTALL-FILES-y) $(SYMLINK-FILES-y)
_CLEAN = doclean

.PHONY: all
all: install

.PHONY: install
install: build _postinstall

_postinstall: build

.PHONY: build
build: _postbuild

ifneq ($(OBJ),)
exe2cmd = $(strip $(call dotfile,$(patsubst %,%.cmd,$(1))))

O_TO_O = $(LD) -r -o $(OBJ) $(OBJS-y)
O_TO_O_STR = $(subst ','\'',$(O_TO_O)) #'# fix syntax highlight
O_TO_O_DISP =  $(if $(V),"$(O_TO_O_STR)","  LD $(@)")
O_TO_O_CMD = "cmd_$@ = $(O_TO_O_STR)"
O_TO_O_DO = @set -e; \
	echo $(O_TO_O_DISP); \
	$(O_TO_O) && \
	echo $(O_TO_O_CMD) > $(call exe2cmd,$(@))

-include .$(OBJ).cmd

#
# Archive objects in .a file if needed
#
$(OBJ): $(OBJS-y) FORCE
	@[ -d $(dir $@) ] || mkdir -p $(dir $@)
	$(if $(D),\
		@echo -n "$< -> $@ " ; \
		echo -n "file_missing=$(call boolean,$(file_missing)) " ; \
		echo -n "cmdline_changed=$(call boolean,$(call cmdline_changed,$(O_TO_O_STR))) " ; \
		echo -n "depfile_missing=$(call boolean,$(depfile_missing)) " ; \
		echo "depfile_newer=$(call boolean,$(depfile_newer)) ")
	$(if $(or \
		$(file_missing),\
		$(call cmdline_changed,$(O_TO_O_STR)),\
		$(depfile_missing),\
		$(depfile_newer)),\
		$(O_TO_O_DO))
endif

#
# Clean all generated files
#
.PHONY: clean
clean: _postclean

.PHONY: doclean
doclean:
	@rm -rf $(OBJ) $(OBJS-all) $(DEPS-all) $(DEPSTMP-all) \
	  $(CMDS-all) $(INSTALL-FILES-all)
	@rm -f $(_BUILD_TARGETS) $(_INSTALL_TARGETS) $(_CLEAN_TARGETS)

include $(RTE_SDK)/mk/internal/rte.compile-post.mk
include $(RTE_SDK)/mk/internal/rte.install-post.mk
include $(RTE_SDK)/mk/internal/rte.clean-post.mk
include $(RTE_SDK)/mk/internal/rte.build-post.mk

.PHONY: FORCE
FORCE:
