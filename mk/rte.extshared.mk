# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2012-2013 6WIND S.A.

MAKEFLAGS += --no-print-directory

# we must create the output dir first and recall the same Makefile
# from this directory
ifeq ($(NOT_FIRST_CALL),)

NOT_FIRST_CALL = 1
export NOT_FIRST_CALL

all:
	$(Q)mkdir -p $(RTE_OUTPUT)
	$(Q)$(MAKE) -C $(RTE_OUTPUT) -f $(RTE_EXTMK) \
		S=$(RTE_SRCDIR) O=$(RTE_OUTPUT) SRCDIR=$(RTE_SRCDIR)
	@echo $(RTE_OUTPUT)/lib must be added to /etc/ld.so.conf or \
		LD_LIBRARY_PATH variable to allow binary to link with dynamic library

%::
	$(Q)mkdir -p $(RTE_OUTPUT)
	$(Q)$(MAKE) -C $(RTE_OUTPUT) -f $(RTE_EXTMK) $@ \
		S=$(RTE_SRCDIR) O=$(RTE_OUTPUT) SRCDIR=$(RTE_SRCDIR)
else
include $(RTE_SDK)/mk/rte.shared.mk
endif
