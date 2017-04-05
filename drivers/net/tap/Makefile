#   BSD LICENSE
#
#   Copyright(c) 2016 Intel Corporation. All rights reserved.
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

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_pmd_tap.a

EXPORT_MAP := rte_pmd_tap_version.map

LIBABIVER := 1

CFLAGS += -O3
CFLAGS += -I$(SRCDIR)
CFLAGS += -I.
CFLAGS += $(WERROR_FLAGS)

#
# all source are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_LIBRTE_PMD_TAP) += rte_eth_tap.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_TAP) += tap_flow.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_TAP) += tap_netlink.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_TAP) += tap_tcmsgs.c

include $(RTE_SDK)/mk/rte.lib.mk

# Generate and clean-up tap_autoconf.h.

export CC CFLAGS CPPFLAGS EXTRA_CFLAGS EXTRA_CPPFLAGS
export AUTO_CONFIG_CFLAGS = -Wno-error

ifndef V
AUTOCONF_OUTPUT := >/dev/null
endif

tap_autoconf.h.new: FORCE

tap_autoconf.h.new: $(RTE_SDK)/buildtools/auto-config-h.sh
	$Q $(RM) -f -- '$@'
	$Q sh -- '$<' '$@' \
		HAVE_TC_FLOWER \
		linux/pkt_cls.h \
		enum TCA_FLOWER_UNSPEC \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_TC_VLAN_ID \
		linux/pkt_cls.h \
		enum TCA_FLOWER_KEY_VLAN_PRIO \
		$(AUTOCONF_OUTPUT)

# Create tap_autoconf.h or update it in case it differs from the new one.

tap_autoconf.h: tap_autoconf.h.new
	$Q [ -f '$@' ] && \
		cmp '$<' '$@' $(AUTOCONF_OUTPUT) || \
		mv '$<' '$@'

$(SRCS-$(CONFIG_RTE_LIBRTE_PMD_TAP):.c=.o): tap_autoconf.h

clean_tap: FORCE
	$Q rm -f -- tap_autoconf.h tap_autoconf.h.new

clean: clean_tap
