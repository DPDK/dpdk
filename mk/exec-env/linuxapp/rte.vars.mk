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
# exec-env:
#
#   - define EXECENV_CFLAGS variable (overriden by cmdline)
#   - define EXECENV_LDFLAGS variable (overriden by cmdline)
#   - define EXECENV_ASFLAGS variable (overriden by cmdline)
#   - may override any previously defined variable
#
# examples for RTE_EXEC_ENV: linuxapp, bsdapp
#
ifeq ($(CONFIG_RTE_BUILD_SHARED_LIB),y)
EXECENV_CFLAGS  = -pthread -fPIC
else
EXECENV_CFLAGS  = -pthread
endif

EXECENV_LDLIBS  =
EXECENV_ASFLAGS =

ifeq ($(CONFIG_RTE_BUILD_SHARED_LIB),y)
EXECENV_LDLIBS += -lgcc_s
endif

# force applications to link with gcc/icc instead of using ld
LINK_USING_CC := 1

# For shared libraries
EXECENV_LDFLAGS += -export-dynamic
# Add library to the group to resolve symbols
EXECENV_LDLIBS  += -ldl

export EXECENV_CFLAGS EXECENV_LDFLAGS EXECENV_ASFLAGS EXECENV_LDLIBS
