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

# Configuration, compilation and installation can be done at once
# with make install T=<config>

# The build directory is T and may be prepended with O
O ?= .
RTE_OUTPUT := $O/$T

.PHONY: install
install:
	@echo ================== Installing $T
	$(Q)if [ ! -f $(RTE_OUTPUT)/.config ]; then \
		$(MAKE) config O=$(RTE_OUTPUT); \
	elif cmp -s $(RTE_OUTPUT)/.config.orig $(RTE_OUTPUT)/.config; then \
		$(MAKE) config O=$(RTE_OUTPUT); \
	else \
		if [ -f $(RTE_OUTPUT)/.config.orig ] ; then \
			tmp_build=$(RTE_OUTPUT)/.config.tmp; \
			$(MAKE) config O=$$tmp_build; \
			if ! cmp -s $(RTE_OUTPUT)/.config.orig $$tmp_build/.config ; then \
				echo "Conflict: local config and template config have both changed"; \
				exit 1; \
			fi; \
		fi; \
		echo "Using local configuration"; \
	fi
	$(Q)$(MAKE) all O=$(RTE_OUTPUT)
