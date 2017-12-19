# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

# clean helper .mk

# fast way, no need to do preclean and postclean
ifeq ($(PRECLEAN)$(POSTCLEAN),)

_postclean: $(_CLEAN)
	@touch _postclean

else # slower way

_preclean: $(PRECLEAN)
	@touch _preclean

ifneq ($(_CLEAN),)
$(_CLEAN): _preclean
else
_CLEAN = _preclean
endif

_clean: $(_CLEAN)
	@touch _clean

ifneq ($(POSTCLEAN),)
$(POSTCLEAN): _clean
else
POSTCLEAN = _clean
endif

_postclean: $(POSTCLEAN)
	@touch _postclean
endif
