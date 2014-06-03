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

# install helper .mk

#
# generate rules to install files in RTE_OUTPUT.
#
# arg1: relative install dir in RTE_OUTPUT
# arg2: relative file name in a source dir (VPATH)
#
define install_rule
$(addprefix $(RTE_OUTPUT)/$(1)/,$(notdir $(2))): $(2)
	@echo "  INSTALL-FILE $(addprefix $(1)/,$(notdir $(2)))"
	@[ -d $(RTE_OUTPUT)/$(1) ] || mkdir -p $(RTE_OUTPUT)/$(1)
	$(Q)cp -rf $$(<) $(RTE_OUTPUT)/$(1)
endef

$(foreach dir,$(INSTALL-DIRS-y),\
	$(foreach file,$(INSTALL-y-$(dir)),\
		$(eval $(call install_rule,$(dir),$(file)))))


#
# generate rules to install symbolic links of files in RTE_OUTPUT.
#
# arg1: relative install dir in RTE_OUTPUT
# arg2: relative file name in a source dir (VPATH)
#
define symlink_rule
$(addprefix $(RTE_OUTPUT)/$(1)/,$(notdir $(2))): $(2)
	@echo "  SYMLINK-FILE $(addprefix $(1)/,$(notdir $(2)))"
	@[ -d $(RTE_OUTPUT)/$(1) ] || mkdir -p $(RTE_OUTPUT)/$(1)
	$(Q)ln -nsf `$(RTE_SDK)/scripts/relpath.sh $$(<) $(RTE_OUTPUT)/$(1)` \
		$(RTE_OUTPUT)/$(1)
endef

$(foreach dir,$(SYMLINK-DIRS-y),\
	$(foreach file,$(SYMLINK-y-$(dir)),\
		$(eval $(call symlink_rule,$(dir),$(file)))))


# fast way, no need to do preinstall and postinstall
ifeq ($(PREINSTALL)$(POSTINSTALL),)

_postinstall: $(_INSTALL)
	@touch _postinstall

else # slower way

_preinstall: $(PREINSTALL)
	@touch _preinstall

ifneq ($(_INSTALL),)
$(_INSTALL): _preinstall
else
_INSTALL = _preinstall
endif

_install: $(_INSTALL)
	@touch _install

ifneq ($(POSTINSTALL),)
$(POSTINSTALL): _install
else
POSTINSTALL = _install
endif

_postinstall: $(POSTINSTALL)
	@touch _postinstall
endif
