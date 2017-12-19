# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

# build helper .mk

# fast way, no need to do prebuild and postbuild
ifeq ($(PREBUILD)$(POSTBUILD),)

_postbuild: $(_BUILD)
	@touch _postbuild

else # slower way

_prebuild: $(PREBUILD)
	@touch _prebuild

ifneq ($(_BUILD),)
$(_BUILD): _prebuild
else
_BUILD = _prebuild
endif

_build: $(_BUILD)
	@touch _build

ifneq ($(POSTBUILD),)
$(POSTBUILD): _build
else
POSTBUILD = _build
endif

_postbuild: $(POSTBUILD)
	@touch _postbuild
endif