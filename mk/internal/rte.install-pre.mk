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

#
# get all variables starting with "INSTALL-y-", and extract the
# installation dir and path
#
INSTALL-y := $(filter INSTALL-y-%,$(.VARIABLES))
INSTALL-n := $(filter INSTALL-n-%,$(.VARIABLES))
INSTALL- := $(filter INSTALL--%,$(.VARIABLES))
INSTALL-DIRS-y := $(patsubst INSTALL-y-%,%,$(INSTALL-y))
INSTALL-FILES-y := $(foreach i,$(INSTALL-DIRS-y),\
	$(addprefix $(RTE_OUTPUT)/$(i)/,$(notdir $(INSTALL-y-$(i)))))
INSTALL-FILES-all := $(foreach i,$(INSTALL-DIRS-y) $(INSTALL-DIRS-n) $(INSTALL-DIRS-),\
	$(addprefix $(RTE_OUTPUT)/$(i)/,$(notdir $(INSTALL-y-$(i)))))

_INSTALL_TARGETS := _preinstall _install _postinstall

#
# get all variables starting with "SYMLINK-y-", and extract the
# installation dir and path
#
SYMLINK-y := $(filter SYMLINK-y-%,$(.VARIABLES))
SYMLINK-n := $(filter SYMLINK-n-%,$(.VARIABLES))
SYMLINK- := $(filter SYMLINK--%,$(.VARIABLES))
SYMLINK-DIRS-y := $(patsubst SYMLINK-y-%,%,$(SYMLINK-y))
SYMLINK-FILES-y := $(foreach i,$(SYMLINK-DIRS-y),\
	$(addprefix $(RTE_OUTPUT)/$(i)/,$(notdir $(SYMLINK-y-$(i)))))
SYMLINK-FILES-all := $(foreach i,$(SYMLINK-DIRS-y) $(SYMLINK-DIRS-n) $(SYMLINK-DIRS-),\
	$(addprefix $(RTE_OUTPUT)/$(i)/,$(notdir $(SYMLINK-y-$(i)))))

_SYMLINK_TARGETS := _presymlink _symlink _postsymlink
