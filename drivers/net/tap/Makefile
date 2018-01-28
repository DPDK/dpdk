# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2016 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_pmd_tap.a

EXPORT_MAP := rte_pmd_tap_version.map

LIBABIVER := 1

#
# TAP_MAX_QUEUES must be a power of 2
#
ifeq ($(TAP_MAX_QUEUES),)
	TAP_MAX_QUEUES = 16
endif
CFLAGS += -O3
CFLAGS += -I$(SRCDIR)
CFLAGS += -I.
CFLAGS += $(WERROR_FLAGS)
LDLIBS += -lrte_eal -lrte_mbuf -lrte_mempool -lrte_ring
LDLIBS += -lrte_ethdev -lrte_net -lrte_kvargs -lrte_hash
LDLIBS += -lrte_bus_vdev

CFLAGS += -DTAP_MAX_QUEUES=$(TAP_MAX_QUEUES)

#
# all source are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_LIBRTE_PMD_TAP) += rte_eth_tap.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_TAP) += tap_flow.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_TAP) += tap_netlink.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_TAP) += tap_tcmsgs.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_TAP) += tap_bpf_api.c
SRCS-$(CONFIG_RTE_LIBRTE_PMD_TAP) += tap_intr.c

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
	$Q sh -- '$<' '$@' \
		HAVE_TC_BPF \
		linux/pkt_cls.h \
		enum TCA_BPF_UNSPEC \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_TC_BPF_FD \
		linux/pkt_cls.h \
		enum TCA_BPF_FD \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_TC_ACT_BPF \
		linux/tc_act/tc_bpf.h \
		enum TCA_ACT_BPF_UNSPEC \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_TC_ACT_BPF_FD \
		linux/tc_act/tc_bpf.h \
		enum TCA_ACT_BPF_FD \
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
