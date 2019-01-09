#! /bin/sh
# SPDX-License-Identifier: BSD-3-Clause
#
# Print link options -l for static link of ibverbs.
#
# Static flavour of ibverbs and the providers libs are explicitly picked,
# thanks to the syntax -l:libfoo.a
# Other libs (pthread and nl) are unchanged, i.e. linked dynamically by default.
#
# PKG_CONFIG_PATH may be required to be set if libibverbs.pc is not installed.

pkg-config --libs-only-l --static libibverbs |
	tr '[:space:]' '\n' |
	sed -r '/^-l(pthread|nl)/! s,(^-l)(.*),\1:lib\2.a,'
