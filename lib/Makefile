#   BSD LICENSE
#
#   Copyright(c) 2010-2017 Intel Corporation. All rights reserved.
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

include $(RTE_SDK)/mk/rte.vars.mk

DIRS-y += librte_compat
DIRS-$(CONFIG_RTE_LIBRTE_EAL) += librte_eal
DIRS-$(CONFIG_RTE_LIBRTE_RING) += librte_ring
DEPDIRS-librte_ring := librte_eal
DIRS-$(CONFIG_RTE_LIBRTE_MEMPOOL) += librte_mempool
DEPDIRS-librte_mempool := librte_eal librte_ring
DIRS-$(CONFIG_RTE_LIBRTE_MBUF) += librte_mbuf
DEPDIRS-librte_mbuf := librte_eal librte_mempool
DIRS-$(CONFIG_RTE_LIBRTE_TIMER) += librte_timer
DEPDIRS-librte_timer := librte_eal
DIRS-$(CONFIG_RTE_LIBRTE_CFGFILE) += librte_cfgfile
DEPDIRS-librte_cfgfile := librte_eal
DIRS-$(CONFIG_RTE_LIBRTE_CMDLINE) += librte_cmdline
DEPDIRS-librte_cmdline := librte_eal
DIRS-$(CONFIG_RTE_LIBRTE_ETHER) += librte_ether
DEPDIRS-librte_ether := librte_net librte_eal librte_mempool librte_ring
DEPDIRS-librte_ether += librte_mbuf
DIRS-$(CONFIG_RTE_LIBRTE_CRYPTODEV) += librte_cryptodev
DEPDIRS-librte_cryptodev := librte_eal librte_mempool librte_ring librte_mbuf
DEPDIRS-librte_cryptodev += librte_kvargs
DIRS-$(CONFIG_RTE_LIBRTE_EVENTDEV) += librte_eventdev
DEPDIRS-librte_eventdev := librte_eal librte_ring
DIRS-$(CONFIG_RTE_LIBRTE_VHOST) += librte_vhost
DEPDIRS-librte_vhost := librte_eal librte_mempool librte_mbuf librte_ether
DIRS-$(CONFIG_RTE_LIBRTE_HASH) += librte_hash
DEPDIRS-librte_hash := librte_eal librte_ring
DIRS-$(CONFIG_RTE_LIBRTE_EFD) += librte_efd
DEPDIRS-librte_efd := librte_eal librte_ring librte_hash
DIRS-$(CONFIG_RTE_LIBRTE_LPM) += librte_lpm
DEPDIRS-librte_lpm := librte_eal
DIRS-$(CONFIG_RTE_LIBRTE_ACL) += librte_acl
DEPDIRS-librte_acl := librte_eal
DIRS-$(CONFIG_RTE_LIBRTE_NET) += librte_net
DEPDIRS-librte_net := librte_mbuf librte_eal
DIRS-$(CONFIG_RTE_LIBRTE_IP_FRAG) += librte_ip_frag
DEPDIRS-librte_ip_frag := librte_eal librte_mempool librte_mbuf librte_ether
DEPDIRS-librte_ip_frag += librte_hash
DIRS-$(CONFIG_RTE_LIBRTE_GRO) += librte_gro
DEPDIRS-librte_gro := librte_eal librte_mbuf librte_ether librte_net
DIRS-$(CONFIG_RTE_LIBRTE_JOBSTATS) += librte_jobstats
DEPDIRS-librte_jobstats := librte_eal
DIRS-$(CONFIG_RTE_LIBRTE_METRICS) += librte_metrics
DEPDIRS-librte_metrics := librte_eal
DIRS-$(CONFIG_RTE_LIBRTE_BITRATE) += librte_bitratestats
DEPDIRS-librte_bitratestats := librte_eal librte_metrics librte_ether
DIRS-$(CONFIG_RTE_LIBRTE_LATENCY_STATS) += librte_latencystats
DEPDIRS-librte_latencystats := librte_eal librte_metrics librte_ether librte_mbuf
DIRS-$(CONFIG_RTE_LIBRTE_POWER) += librte_power
DEPDIRS-librte_power := librte_eal
DIRS-$(CONFIG_RTE_LIBRTE_METER) += librte_meter
DEPDIRS-librte_meter := librte_eal
DIRS-$(CONFIG_RTE_LIBRTE_SCHED) += librte_sched
DEPDIRS-librte_sched := librte_eal librte_mempool librte_mbuf librte_net
DEPDIRS-librte_sched += librte_timer
DIRS-$(CONFIG_RTE_LIBRTE_KVARGS) += librte_kvargs
DEPDIRS-librte_kvargs := librte_eal
DIRS-$(CONFIG_RTE_LIBRTE_DISTRIBUTOR) += librte_distributor
DEPDIRS-librte_distributor := librte_eal librte_mbuf librte_ether
DIRS-$(CONFIG_RTE_LIBRTE_PORT) += librte_port
DEPDIRS-librte_port := librte_eal librte_mempool librte_mbuf librte_ether
DEPDIRS-librte_port += librte_ip_frag librte_sched
ifeq ($(CONFIG_RTE_LIBRTE_KNI),y)
DEPDIRS-librte_port += librte_kni
endif
DIRS-$(CONFIG_RTE_LIBRTE_TABLE) += librte_table
DEPDIRS-librte_table := librte_eal librte_mempool librte_mbuf
DEPDIRS-librte_table += librte_port librte_lpm librte_hash
ifeq ($(CONFIG_RTE_LIBRTE_ACL),y)
DEPDIRS-librte_table += librte_acl
endif
DIRS-$(CONFIG_RTE_LIBRTE_PIPELINE) += librte_pipeline
DEPDIRS-librte_pipeline := librte_eal librte_mempool librte_mbuf
DEPDIRS-librte_pipeline += librte_table librte_port
DIRS-$(CONFIG_RTE_LIBRTE_REORDER) += librte_reorder
DEPDIRS-librte_reorder := librte_eal librte_mempool librte_mbuf
DIRS-$(CONFIG_RTE_LIBRTE_PDUMP) += librte_pdump
DEPDIRS-librte_pdump := librte_eal librte_mempool librte_mbuf librte_ether

ifeq ($(CONFIG_RTE_EXEC_ENV_LINUXAPP),y)
DIRS-$(CONFIG_RTE_LIBRTE_KNI) += librte_kni
endif
DEPDIRS-librte_kni:= librte_eal librte_mempool librte_mbuf librte_ether

include $(RTE_SDK)/mk/rte.subdir.mk
