#   BSD LICENSE
#
#   Copyright(c) 2016 6WIND S.A.
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
#     * Neither the name of 6WIND S.A. nor the names of its
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

ifeq ($(RTE_SDK),)
$(error "Please define RTE_SDK environment variable")
endif

# Default target, can be overriden by command line or environment
RTE_TARGET ?= x86_64-native-linuxapp-gcc

include $(RTE_SDK)/mk/rte.vars.mk

DIRS-$(CONFIG_RTE_LIBRTE_PMD_BOND) += bond
DIRS-y += cmdline
DIRS-$(CONFIG_RTE_LIBRTE_DISTRIBUTOR) += distributor
ifneq ($(ICP_ROOT),)
DIRS-y += dpdk_qat
endif
DIRS-y += ethtool
DIRS-y += exception_path
DIRS-y += helloworld
DIRS-$(CONFIG_RTE_LIBRTE_PIPELINE) += ip_pipeline
ifeq ($(CONFIG_RTE_LIBRTE_LPM),y)
DIRS-$(CONFIG_RTE_IP_FRAG) += ip_reassembly
DIRS-$(CONFIG_RTE_IP_FRAG) += ip_fragmentation
endif
ifeq ($(CONFIG_RTE_LIBRTE_ACL)$(CONFIG_RTE_LIBRTE_HASH)$(CONFIG_RTE_LIBRTE_LPM),yyy)
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
DIRS-$(CONFIG_RTE_LIBRTE_IEEE1588) += ptpclient
DIRS-$(CONFIG_RTE_LIBRTE_METER) += qos_meter
DIRS-$(CONFIG_RTE_LIBRTE_SCHED) += qos_sched
DIRS-y += quota_watermark
DIRS-$(CONFIG_RTE_ETHDEV_RXTX_CALLBACKS) += rxtx_callbacks
DIRS-y += skeleton
ifeq ($(CONFIG_RTE_LIBRTE_HASH),y)
DIRS-$(CONFIG_RTE_LIBRTE_VHOST) += tep_termination
endif
DIRS-$(CONFIG_RTE_LIBRTE_TIMER) += timer
DIRS-$(CONFIG_RTE_LIBRTE_VHOST) += vhost
DIRS-$(CONFIG_RTE_LIBRTE_XEN_DOM0) += vhost_xen
DIRS-y += vmdq
DIRS-y += vmdq_dcb
ifeq ($(CONFIG_RTE_LIBRTE_POWER), y)
ifeq ($(shell pkg-config --atleast-version=0.9.3 libvirt; echo $$?), 0)
DIRS-y += vm_power_manager
else
$(info vm_power_manager requires libvirt >= 0.9.3)
endif
endif

include $(RTE_SDK)/mk/rte.extsubdir.mk
