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

##### if sourced from kernel Kbuild system
ifneq ($(KERNELRELEASE),)
override EXTRA_CFLAGS = $(MODULE_CFLAGS) $(EXTRA_KERNEL_CFLAGS)
obj-m          += $(MODULE).o
ifneq ($(MODULE),$(notdir $(SRCS-y:%.c=%)))
$(MODULE)-objs += $(notdir $(SRCS-y:%.c=%.o))
endif

##### if launched from rte build system
else

include $(RTE_SDK)/mk/internal/rte.install-pre.mk
include $(RTE_SDK)/mk/internal/rte.clean-pre.mk
include $(RTE_SDK)/mk/internal/rte.build-pre.mk
include $(RTE_SDK)/mk/internal/rte.depdirs-pre.mk

# DPDK uses a more up-to-date gcc, so clear the override here.
unexport CC
override CFLAGS = $(MODULE_CFLAGS)

# VPATH contains at least SRCDIR
VPATH += $(SRCDIR)

_BUILD = $(MODULE).ko
_INSTALL = $(INSTALL-FILES-y) $(SYMLINK-FILES-y) \
	$(RTE_OUTPUT)/kmod/$(MODULE).ko
_CLEAN = doclean

SRCS_LINKS = $(addsuffix _link,$(SRCS-y))

compare = $(strip $(subst $(1),,$(2)) $(subst $(2),,$(1)))

.PHONY: all
all: install

.PHONY: install
install: build _postinstall

_postinstall: build

.PHONY: build
build: _postbuild

# Link all sources in build directory
%_link: FORCE
	$(if $(call compare,$(notdir $*),$*),\
	$(Q)if [ ! -f $(notdir $(*)) ]; then ln -nfs $(SRCDIR)/$(*) . ; fi,\
	$(Q)if [ ! -f $(notdir $(*)) ]; then ln -nfs $(SRCDIR)/$(*) . ; fi)

# build module
$(MODULE).ko: $(SRCS_LINKS)
	$(Q)if [ ! -f $(notdir Makefile) ]; then ln -nfs $(SRCDIR)/Makefile . ; fi
	$(Q)if [ ! -f $(notdir BSDmakefile) ]; then ln -nfs $(SRCDIR)/BSDmakefile . ; fi
	$(Q)MAKEFLAGS= $(BSDMAKE)

# install module in $(RTE_OUTPUT)/kmod
$(RTE_OUTPUT)/kmod/$(MODULE).ko: $(MODULE).ko
	$(Q)echo INSTALL-MODULE $(MODULE).ko
	$(Q)[ -d $(RTE_OUTPUT)/kmod ] || mkdir -p $(RTE_OUTPUT)/kmod
	$(Q)cp -f $(MODULE).ko $(RTE_OUTPUT)/kmod

# install module
modules_install:
	$(Q)MAKEFLAGS= $(BSDMAKE) install

.PHONY: clean
clean: _postclean

# do a make clean and remove links
.PHONY: doclean
doclean:
	$(Q)if [ ! -f $(notdir Makefile) ]; then ln -nfs $(SRCDIR)/Makefile . ; fi
	$(Q)$(MAKE) -C $(RTE_KERNELDIR) M=$(CURDIR) O=$(RTE_KERNELDIR) clean
	$(Q)$(foreach FILE,$(SRCS-y) $(SRCS-n) $(SRCS-),\
		if [ -h $(notdir $(FILE)) ]; then rm -f $(notdir $(FILE)) ; fi ;)
	$(Q)if [ -h $(notdir Makefile) ]; then rm -f $(notdir Makefile) ; fi
	$(Q)rm -f $(_BUILD_TARGETS) $(_INSTALL_TARGETS) $(_CLEAN_TARGETS) \
		$(INSTALL-FILES-all)

include $(RTE_SDK)/mk/internal/rte.install-post.mk
include $(RTE_SDK)/mk/internal/rte.clean-post.mk
include $(RTE_SDK)/mk/internal/rte.build-post.mk
include $(RTE_SDK)/mk/internal/rte.depdirs-post.mk

.PHONY: FORCE
FORCE:

endif
