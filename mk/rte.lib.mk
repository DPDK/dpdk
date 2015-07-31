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
include $(RTE_SDK)/mk/internal/rte.depdirs-pre.mk

# VPATH contains at least SRCDIR
VPATH += $(SRCDIR)

ifeq ($(CONFIG_RTE_BUILD_SHARED_LIB),y)
LIB := $(patsubst %.a,%.so.$(LIBABIVER),$(LIB))
ifeq ($(CONFIG_RTE_NEXT_ABI),y)
LIB := $(LIB).1
endif
CPU_LDFLAGS += --version-script=$(SRCDIR)/$(EXPORT_MAP)
endif


_BUILD = $(LIB)
_INSTALL = $(INSTALL-FILES-y) $(SYMLINK-FILES-y) $(RTE_OUTPUT)/lib/$(LIB)
_CLEAN = doclean

.PHONY: all
all: install

.PHONY: install
install: build _postinstall

_postinstall: build

.PHONY: build
build: _postbuild

exe2cmd = $(strip $(call dotfile,$(patsubst %,%.cmd,$(1))))

ifeq ($(LINK_USING_CC),1)
# Override the definition of LD here, since we're linking with CC
LD := $(CC) $(CPU_CFLAGS)
_CPU_LDFLAGS := $(call linkerprefix,$(CPU_LDFLAGS))
else
_CPU_LDFLAGS := $(CPU_LDFLAGS)
endif

O_TO_A = $(AR) crDs $(LIB) $(OBJS-y)
O_TO_A_STR = $(subst ','\'',$(O_TO_A)) #'# fix syntax highlight
O_TO_A_DISP = $(if $(V),"$(O_TO_A_STR)","  AR $(@)")
O_TO_A_CMD = "cmd_$@ = $(O_TO_A_STR)"
O_TO_A_DO = @set -e; \
	echo $(O_TO_A_DISP); \
	$(O_TO_A) && \
	echo $(O_TO_A_CMD) > $(call exe2cmd,$(@))

O_TO_S = $(LD) $(_CPU_LDFLAGS) $(EXTRA_LDFLAGS) $(LDLIBS) -shared $(OBJS-y) \
	 -Wl,-soname,$(LIB) -o $(LIB)
O_TO_S_STR = $(subst ','\'',$(O_TO_S)) #'# fix syntax highlight
O_TO_S_DISP = $(if $(V),"$(O_TO_S_STR)","  LD $(@)")
O_TO_S_DO = @set -e; \
	echo $(O_TO_S_DISP); \
	$(O_TO_S) && \
	echo $(O_TO_S_CMD) > $(call exe2cmd,$(@))

ifeq ($(CONFIG_RTE_BUILD_SHARED_LIB),n)
O_TO_C = $(AR) crus $(LIB_ONE) $(OBJS-y)
O_TO_C_STR = $(subst ','\'',$(O_TO_C)) #'# fix syntax highlight
O_TO_C_DISP = $(if $(V),"$(O_TO_C_STR)","  AR_C $(@)")
O_TO_C_DO = @set -e; \
	$(lib_dir) \
	$(copy_obj)
else
O_TO_C = $(LD) -shared $(OBJS-y) -o $(LIB_ONE)
O_TO_C_STR = $(subst ','\'',$(O_TO_C)) #'# fix syntax highlight
O_TO_C_DISP = $(if $(V),"$(O_TO_C_STR)","  LD_C $(@)")
O_TO_C_DO = @set -e; \
	$(lib_dir) \
	$(copy_obj)
endif

copy_obj = cp -f $(OBJS-y) $(RTE_OUTPUT)/build/lib;
lib_dir = [ -d $(RTE_OUTPUT)/lib ] || mkdir -p $(RTE_OUTPUT)/lib;
-include .$(LIB).cmd

#
# Archive objects in .a file if needed
#
ifeq ($(CONFIG_RTE_BUILD_SHARED_LIB),y)
$(LIB): $(OBJS-y) $(DEP_$(LIB)) FORCE
ifeq ($(LIBABIVER),)
	@echo "Must Specify a $(LIB) ABI version"
	@false
endif
	@[ -d $(dir $@) ] || mkdir -p $(dir $@)
	$(if $(D),\
		@echo -n "$< -> $@ " ; \
		echo -n "file_missing=$(call boolean,$(file_missing)) " ; \
		echo -n "cmdline_changed=$(call boolean,$(call cmdline_changed,$(O_TO_S_STR))) " ; \
		echo -n "depfile_missing=$(call boolean,$(depfile_missing)) " ; \
		echo "depfile_newer=$(call boolean,$(depfile_newer)) ")
	$(if $(or \
		$(file_missing),\
		$(call cmdline_changed,$(O_TO_S_STR)),\
		$(depfile_missing),\
		$(depfile_newer)),\
		$(O_TO_S_DO))

ifeq ($(CONFIG_RTE_BUILD_COMBINE_LIBS),y)
	$(if $(or \
        $(file_missing),\
        $(call cmdline_changed,$(O_TO_C_STR)),\
        $(depfile_missing),\
        $(depfile_newer)),\
        $(O_TO_C_DO))
endif
else
$(LIB): $(OBJS-y) $(DEP_$(LIB)) FORCE
	@[ -d $(dir $@) ] || mkdir -p $(dir $@)
	$(if $(D),\
	    @echo -n "$< -> $@ " ; \
	    echo -n "file_missing=$(call boolean,$(file_missing)) " ; \
	    echo -n "cmdline_changed=$(call boolean,$(call cmdline_changed,$(O_TO_A_STR))) " ; \
	    echo -n "depfile_missing=$(call boolean,$(depfile_missing)) " ; \
	    echo "depfile_newer=$(call boolean,$(depfile_newer)) ")
	$(if $(or \
	    $(file_missing),\
	    $(call cmdline_changed,$(O_TO_A_STR)),\
	    $(depfile_missing),\
	    $(depfile_newer)),\
	    $(O_TO_A_DO))
ifeq ($(CONFIG_RTE_BUILD_COMBINE_LIBS),y)
	$(if $(or \
        $(file_missing),\
        $(call cmdline_changed,$(O_TO_C_STR)),\
        $(depfile_missing),\
        $(depfile_newer)),\
        $(O_TO_C_DO))
endif
endif

#
# install lib in $(RTE_OUTPUT)/lib
#
$(RTE_OUTPUT)/lib/$(LIB): $(LIB)
	@echo "  INSTALL-LIB $(LIB)"
	@[ -d $(RTE_OUTPUT)/lib ] || mkdir -p $(RTE_OUTPUT)/lib
	$(Q)cp -f $(LIB) $(RTE_OUTPUT)/lib
ifeq ($(CONFIG_RTE_BUILD_SHARED_LIB),y)
ifeq ($(CONFIG_RTE_NEXT_ABI),y)
	$(Q)ln -s -f $< $(basename $(basename $@))
else
	$(Q)ln -s -f $< $(basename $@)
endif
endif

#
# Clean all generated files
#
.PHONY: clean
clean: _postclean

.PHONY: doclean
doclean:
	$(Q)rm -rf $(LIB) $(OBJS-all) $(DEPS-all) $(DEPSTMP-all) \
	  $(CMDS-all) $(INSTALL-FILES-all)
	$(Q)rm -f $(_BUILD_TARGETS) $(_INSTALL_TARGETS) $(_CLEAN_TARGETS)

include $(RTE_SDK)/mk/internal/rte.compile-post.mk
include $(RTE_SDK)/mk/internal/rte.install-post.mk
include $(RTE_SDK)/mk/internal/rte.clean-post.mk
include $(RTE_SDK)/mk/internal/rte.build-post.mk
include $(RTE_SDK)/mk/internal/rte.depdirs-post.mk

.PHONY: FORCE
FORCE:
