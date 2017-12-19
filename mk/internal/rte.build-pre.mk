# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

_BUILD_TARGETS := _prebuild _build _postbuild

comma := ,
linkerprefix = $(subst -Wl$(comma)-L,-L,$(addprefix -Wl$(comma),$1))
