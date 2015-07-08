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

include $(RTE_SDK)/mk/internal/rte.build-pre.mk

# VPATH contains at least SRCDIR
VPATH += $(SRCDIR)

ifeq ($(CONFIG_RTE_BUILD_COMBINE_LIBS),y)
ifeq ($(CONFIG_RTE_BUILD_SHARED_LIB),y)
LIB_ONE := lib$(RTE_LIBNAME).so
else
LIB_ONE := lib$(RTE_LIBNAME).a
endif
endif

.PHONY:sharelib
sharelib: $(LIB_ONE) FORCE

OBJS = $(wildcard $(RTE_OUTPUT)/build/lib/*.o)

ifeq ($(LINK_USING_CC),1)
# Override the definition of LD here, since we're linking with CC
LD := $(CC) $(CPU_CFLAGS)
O_TO_S = $(LD) $(call linkerprefix,$(CPU_LDFLAGS)) \
	-shared $(OBJS) -o $(RTE_OUTPUT)/lib/$(LIB_ONE)
else
O_TO_S = $(LD) $(CPU_LDFLAGS) \
	-shared $(OBJS) -o $(RTE_OUTPUT)/lib/$(LIB_ONE)
endif

O_TO_S_STR = $(subst ','\'',$(O_TO_S)) #'# fix syntax highlight
O_TO_S_DISP = $(if $(V),"$(O_TO_S_STR)","  LD $(@)")
O_TO_S_CMD = "cmd_$@ = $(O_TO_S_STR)"
O_TO_S_DO = @set -e; \
    echo $(O_TO_S_DISP); \
    $(O_TO_S)

O_TO_A =  $(AR) crus $(RTE_OUTPUT)/lib/$(LIB_ONE) $(OBJS)
O_TO_A_STR = $(subst ','\'',$(O_TO_A)) #'# fix syntax highlight
O_TO_A_DISP = $(if $(V),"$(O_TO_A_STR)","  LD $(@)")
O_TO_A_CMD = "cmd_$@ = $(O_TO_A_STR)"
O_TO_A_DO = @set -e; \
    echo $(O_TO_A_DISP); \
    $(O_TO_A)
#
# Archive objects to share library
#

ifeq ($(CONFIG_RTE_BUILD_COMBINE_LIBS),y)
ifeq ($(CONFIG_RTE_BUILD_SHARED_LIB),y)
$(LIB_ONE): FORCE
	@[ -d $(dir $@) ] || mkdir -p $(dir $@)
	$(O_TO_S_DO)
else
$(LIB_ONE): FORCE
	@[ -d $(dir $@) ] || mkdir -p $(dir $@)
	$(O_TO_A_DO)
endif
endif

#
# Clean all generated files
#
.PHONY: clean
clean: _postclean

.PHONY: doclean
doclean:
	$(Q)rm -rf $(LIB_ONE)

.PHONY: FORCE
FORCE:
