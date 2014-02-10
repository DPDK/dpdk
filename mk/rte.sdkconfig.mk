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

INSTALL_CONFIGS := $(filter-out %~,\
	$(patsubst $(RTE_SRCDIR)/config/defconfig_%,%,\
	$(wildcard $(RTE_SRCDIR)/config/defconfig_*)))
INSTALL_TARGETS := $(addsuffix _install,$(INSTALL_CONFIGS))

.PHONY: config
ifeq ($(RTE_CONFIG_TEMPLATE),)
config:
	@echo -n "No template specified. Use T=template " ; \
	echo "among the following list:" ; \
	for t in $(INSTALL_CONFIGS); do \
		echo "  $$t" ; \
	done
else
config: $(RTE_OUTPUT)/include/rte_config.h $(RTE_OUTPUT)/Makefile
	$(Q)$(MAKE) depdirs
	@echo "Configuration done"
endif

ifdef NODOTCONF
$(RTE_OUTPUT)/.config: ;
else
$(RTE_OUTPUT)/.config: $(RTE_CONFIG_TEMPLATE) FORCE
	@[ -d $(RTE_OUTPUT) ] || mkdir -p $(RTE_OUTPUT)
	$(Q)if [ "$(RTE_CONFIG_TEMPLATE)" != "" -a -f "$(RTE_CONFIG_TEMPLATE)" ]; then \
		$(CPP) -undef -P -x assembler-with-cpp \
		-fdirectives-only -ffreestanding \
		-o $(RTE_OUTPUT)/.config_tmp $(RTE_CONFIG_TEMPLATE) ; \
		if ! cmp -s $(RTE_OUTPUT)/.config_tmp $(RTE_OUTPUT)/.config; then \
			cp $(RTE_OUTPUT)/.config_tmp $(RTE_OUTPUT)/.config ; \
		fi ; \
		rm -f $(RTE_OUTPUT)/.config_tmp ; \
	else \
		echo -n "No template specified. Use T=template " ; \
		echo "among the following list:" ; \
		for t in $(INSTALL_CONFIGS); do \
			echo "  $$t" ; \
		done ; \
	fi
endif

# generate a Makefile for this build directory
# use a relative path so it will continue to work even if we move the directory
SDK_RELPATH=$(shell $(RTE_SDK)/scripts/relpath.sh $(abspath $(RTE_SRCDIR)) \
				$(abspath $(RTE_OUTPUT)))
OUTPUT_RELPATH=$(shell $(RTE_SDK)/scripts/relpath.sh $(abspath $(RTE_OUTPUT)) \
				$(abspath $(RTE_SRCDIR)))
$(RTE_OUTPUT)/Makefile:
	@[ -d $(RTE_OUTPUT) ] || mkdir -p $(RTE_OUTPUT)
	$(Q)$(RTE_SDK)/scripts/gen-build-mk.sh $(SDK_RELPATH) $(OUTPUT_RELPATH) \
		> $(RTE_OUTPUT)/Makefile

# clean installed files, and generate a new config header file
# if NODOTCONF variable is defined, don't try to rebuild .config
$(RTE_OUTPUT)/include/rte_config.h: $(RTE_OUTPUT)/.config
	$(Q)rm -rf $(RTE_OUTPUT)/include $(RTE_OUTPUT)/app \
		$(RTE_OUTPUT)/hostapp $(RTE_OUTPUT)/lib \
		$(RTE_OUTPUT)/hostlib $(RTE_OUTPUT)/kmod $(RTE_OUTPUT)/build
	@[ -d $(RTE_OUTPUT)/include ] || mkdir -p $(RTE_OUTPUT)/include
	$(Q)$(RTE_SDK)/scripts/gen-config-h.sh $(RTE_OUTPUT)/.config \
		> $(RTE_OUTPUT)/include/rte_config.h

# generate the rte_config.h
.PHONY: headerconfig
headerconfig: $(RTE_OUTPUT)/include/rte_config.h
	@true

# check that .config is present, and if yes, check that rte_config.h
# is up to date
.PHONY: checkconfig
checkconfig:
	@if [ ! -f $(RTE_OUTPUT)/.config ]; then \
		echo "No .config in build directory"; \
		exit 1; \
	fi
	$(Q)$(MAKE) -f $(RTE_SDK)/mk/rte.sdkconfig.mk \
		headerconfig NODOTCONF=1

.PHONY: FORCE
FORCE:
