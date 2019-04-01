# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2016 6WIND S.A.

ifeq ($(RTE_SDK),)
$(error "Please define RTE_SDK environment variable")
endif

# Default target, detect a build directory, by looking for a path with a .config
RTE_TARGET ?= $(notdir $(abspath $(dir $(firstword $(wildcard $(RTE_SDK)/*/.config)))))

include $(RTE_SDK)/mk/rte.vars.mk

DIRS-$(CONFIG_RTE_LIBRTE_BBDEV) += bbdev_app
DIRS-$(CONFIG_RTE_LIBRTE_PMD_BOND) += bond
DIRS-y += cmdline
ifeq ($(CONFIG_RTE_LIBRTE_POWER),y)
DIRS-$(CONFIG_RTE_LIBRTE_DISTRIBUTOR) += distributor
endif
DIRS-y += ethtool
DIRS-y += exception_path
DIRS-$(CONFIG_RTE_LIBRTE_EFD) += server_node_efd
DIRS-$(CONFIG_RTE_LIBRTE_CRYPTODEV) += fips_validation
DIRS-$(CONFIG_RTE_LIBRTE_FLOW_CLASSIFY) += flow_classify
DIRS-y += flow_filtering
DIRS-y += helloworld
DIRS-$(CONFIG_RTE_LIBRTE_PIPELINE) += ip_pipeline
ifeq ($(CONFIG_RTE_LIBRTE_LPM),y)
DIRS-$(CONFIG_RTE_IP_FRAG) += ip_reassembly
DIRS-$(CONFIG_RTE_IP_FRAG) += ip_fragmentation
endif
ifeq ($(CONFIG_RTE_LIBRTE_ACL)$(CONFIG_RTE_LIBRTE_HASH)$(CONFIG_RTE_LIBRTE_LPM)$(CONFIG_RTE_LIBRTE_SECURITY),yyyy)
DIRS-$(CONFIG_RTE_LIBRTE_CRYPTODEV) += ipsec-secgw
endif
DIRS-$(CONFIG_RTE_LIBRTE_HASH) += ipv4_multicast
DIRS-$(CONFIG_RTE_LIBRTE_KNI) += kni
DIRS-y += l2fwd
ifneq ($(PQOS_INSTALL_PATH),)
DIRS-y += l2fwd-cat
endif
DIRS-$(CONFIG_RTE_LIBRTE_CRYPTODEV) += l2fwd-crypto
DIRS-$(CONFIG_RTE_LIBRTE_JOBSTATS) += l2fwd-jobstats
DIRS-y += l2fwd-keepalive
DIRS-y += l2fwd-keepalive/ka-agent
ifeq ($(CONFIG_RTE_LIBRTE_HASH),y)
DIRS-$(CONFIG_RTE_LIBRTE_LPM) += l3fwd
endif
DIRS-$(CONFIG_RTE_LIBRTE_ACL) += l3fwd-acl
ifeq ($(CONFIG_RTE_LIBRTE_LPM)$(CONFIG_RTE_LIBRTE_HASH),yy)
DIRS-$(CONFIG_RTE_LIBRTE_POWER) += l3fwd-power
DIRS-y += l3fwd-vf
endif
DIRS-y += link_status_interrupt
DIRS-$(CONFIG_RTE_LIBRTE_LPM) += load_balancer
DIRS-y += multi_process
DIRS-y += netmap_compat/bridge
DIRS-$(CONFIG_RTE_LIBRTE_REORDER) += packet_ordering
ifeq ($(CONFIG_RTE_ARCH_X86_64),y)
DIRS-y += performance-thread
endif
DIRS-$(CONFIG_RTE_LIBRTE_IEEE1588) += ptpclient
DIRS-$(CONFIG_RTE_LIBRTE_METER) += qos_meter
DIRS-$(CONFIG_RTE_LIBRTE_SCHED) += qos_sched
DIRS-y += quota_watermark
DIRS-$(CONFIG_RTE_ETHDEV_RXTX_CALLBACKS) += rxtx_callbacks
DIRS-y += service_cores
DIRS-y += skeleton
ifeq ($(CONFIG_RTE_LIBRTE_HASH),y)
DIRS-$(CONFIG_RTE_LIBRTE_VHOST) += tep_termination
endif
DIRS-$(CONFIG_RTE_LIBRTE_TIMER) += timer
DIRS-$(CONFIG_RTE_LIBRTE_VHOST) += vhost vhost_scsi vdpa
ifeq ($(CONFIG_RTE_LIBRTE_CRYPTODEV),y)
DIRS-$(CONFIG_RTE_LIBRTE_VHOST) += vhost_crypto
endif
DIRS-y += vmdq
DIRS-y += vmdq_dcb
ifeq ($(CONFIG_RTE_LIBRTE_POWER), y)
ifeq ($(shell pkg-config --atleast-version=0.9.3 libvirt; echo $$?), 0)
DIRS-y += vm_power_manager
else
$(info vm_power_manager requires libvirt >= 0.9.3)
endif
endif

DIRS-y += eventdev_pipeline

include $(RTE_SDK)/mk/rte.extsubdir.mk
