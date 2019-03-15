# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

.PHONY: showversion
showversion:
	@cat $(RTE_SRCDIR)/VERSION

.PHONY: showversionum
showversionum:
	@cat $(RTE_SRCDIR)/VERSION | awk -F '.' '{print $$1$$2}'

INSTALL_CONFIGS := $(sort $(filter-out %app-icc,$(filter-out %app-clang,\
	$(filter-out %app-gcc,$(filter-out %~,\
	$(patsubst $(RTE_SRCDIR)/config/defconfig_%,%,\
	$(wildcard $(RTE_SRCDIR)/config/defconfig_*)))))))
INSTALL_TARGETS := $(addsuffix _install,$(INSTALL_CONFIGS))

.PHONY: showconfigs
showconfigs:
	@$(foreach CONFIG, $(INSTALL_CONFIGS), echo $(CONFIG);)

.PHONY: notemplate
notemplate:
	@printf "No template specified. Use 'make defconfig' or "
	@echo "use T=template from the following list:"
	@$(MAKE) -rR showconfigs | sed 's,^,  ,'

.PHONY: defconfig
defconfig:
	@$(MAKE) config T=$(shell \
                uname -m | awk '{ \
                if ($$0 == "aarch64") { \
                        print "arm64-armv8a"} \
                else if ($$0 == "armv7l") { \
                        print "arm-armv7a"} \
                else if ($$0 == "ppc64") { \
                        print "ppc_64-power8"} \
                else if ($$0 == "amd64") { \
                        print "x86_64-native"} \
                else { \
                        printf "%s-native", $$0} }' \
		)-$(shell \
                uname | awk '{ \
                if ($$0 == "Linux") { \
                        print "linux"} \
                else { \
                        print "freebsd"} }' \
		)-$(shell \
		${CC} --version | grep -o 'cc\|gcc\|icc\|clang' | awk \
		'{ \
		if ($$1 == "cc") { \
			print "gcc" } \
		else { \
			print $$1 } \
		}' \
		)

.PHONY: config
ifeq ($(RTE_CONFIG_TEMPLATE),)
config: notemplate
else
config: $(RTE_OUTPUT)/include/rte_config.h $(RTE_OUTPUT)/Makefile
	@echo "Configuration done using" \
		$(patsubst defconfig_%,%,$(notdir $(RTE_CONFIG_TEMPLATE)))
endif

$(RTE_OUTPUT):
	$(Q)mkdir -p $@

ifdef NODOTCONF
$(RTE_OUTPUT)/.config: ;
else
# Generate config from template, if there are duplicates keep only the last.
# To do so the temp config is checked for duplicate keys with cut/sort/uniq
# Then for each of those identified duplicates as long as there are more than
# just one left the last match is removed.
# Part of the config includes the version information taken from "VERSION"
# in the repo. This needs to be split into the various parts using sed and awk.
# To ensure correct version comparison, we append ".99" to the version number
# so that the version of a release is higher than that of its rc's.
$(RTE_OUTPUT)/.config: $(RTE_CONFIG_TEMPLATE) FORCE | $(RTE_OUTPUT)
	$(Q)if [ "$(RTE_CONFIG_TEMPLATE)" != "" -a -f "$(RTE_CONFIG_TEMPLATE)" ]; then \
		$(CPP) -undef -P -x assembler-with-cpp \
		`cat $(RTE_SRCDIR)/VERSION | \
		sed -e 's/-rc/.-rc./' -e 's/$$/..99/' | \
		awk -F '.' '{print "-D__YEAR="int($$1), "-D__MONTH="int($$2), "-D__MINOR="int($$3), "-D__SUFFIX=\""$$4"\"", "-D__RELEASE="int($$5)}'` \
		-ffreestanding \
		-o $(RTE_OUTPUT)/.config_tmp $(RTE_CONFIG_TEMPLATE) ; \
		config=$$(cat $(RTE_OUTPUT)/.config_tmp) ; \
		echo "$$config" | awk -F '=' 'BEGIN {i=1} \
			/^#/ {pos[i++]=$$0} \
			!/^#/ {if (!s[$$1]) {pos[i]=$$0; s[$$1]=i++} \
				else {pos[s[$$1]]=$$0}} END \
			{for (j=1; j<i; j++) print pos[j]}' \
			> $(RTE_OUTPUT)/.config_tmp ; \
		if ! cmp -s $(RTE_OUTPUT)/.config_tmp $(RTE_OUTPUT)/.config; then \
			cp $(RTE_OUTPUT)/.config_tmp $(RTE_OUTPUT)/.config ; \
			cp $(RTE_OUTPUT)/.config_tmp $(RTE_OUTPUT)/.config.orig ; \
		fi ; \
		rm -f $(RTE_OUTPUT)/.config_tmp ; \
	else \
		$(MAKE) -rRf $(RTE_SDK)/mk/rte.sdkconfig.mk notemplate; \
	fi
endif

# generate a Makefile for this build directory
# use a relative path so it will continue to work even if we move the directory
SDK_RELPATH=$(shell $(RTE_SDK)/buildtools/relpath.sh $(abspath $(RTE_SRCDIR)) \
				$(abspath $(RTE_OUTPUT)))
OUTPUT_RELPATH=$(shell $(RTE_SDK)/buildtools/relpath.sh $(abspath $(RTE_OUTPUT)) \
				$(abspath $(RTE_SRCDIR)))
$(RTE_OUTPUT)/Makefile: | $(RTE_OUTPUT)
	$(Q)$(RTE_SDK)/buildtools/gen-build-mk.sh $(SDK_RELPATH) > $@

# clean installed files, and generate a new config header file
# if NODOTCONF variable is defined, don't try to rebuild .config
$(RTE_OUTPUT)/include/rte_config.h: $(RTE_OUTPUT)/.config
	$(Q)rm -rf $(RTE_OUTPUT)/include $(RTE_OUTPUT)/app \
		$(RTE_OUTPUT)/lib \
		$(RTE_OUTPUT)/hostlib $(RTE_OUTPUT)/kmod $(RTE_OUTPUT)/build
	$(Q)mkdir -p $(RTE_OUTPUT)/include
	$(Q)$(RTE_SDK)/buildtools/gen-config-h.sh $(RTE_OUTPUT)/.config \
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
