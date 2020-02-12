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

lib='libibverbs'
deps='pthread|nl'

pkg-config --libs --static $lib |
	tr '[:space:]' '\n' |
	sed -r "/^-l($deps)/! s,(^-l)(.*),\1:lib\2.a," |   # explicit .a
	sed -n '/^-[Ll]/p' |   # extra link options may break with make
	tac |
	awk "/^-l:$lib.a/&&c++ {next} 1" | # drop first duplicates of main lib
	tac
