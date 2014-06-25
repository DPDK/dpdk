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

include $(RTE_SDK)/mk/rte.vars.mk

DIRS-$(CONFIG_RTE_LIBC) += libc
DIRS-$(CONFIG_RTE_LIBRTE_EAL) += librte_eal
DIRS-$(CONFIG_RTE_LIBRTE_MALLOC) += librte_malloc
DIRS-$(CONFIG_RTE_LIBRTE_RING) += librte_ring
DIRS-$(CONFIG_RTE_LIBRTE_MEMPOOL) += librte_mempool
DIRS-$(CONFIG_RTE_LIBRTE_MBUF) += librte_mbuf
DIRS-$(CONFIG_RTE_LIBRTE_TIMER) += librte_timer
DIRS-$(CONFIG_RTE_LIBRTE_CFGFILE) += librte_cfgfile
DIRS-$(CONFIG_RTE_LIBRTE_CMDLINE) += librte_cmdline
DIRS-$(CONFIG_RTE_LIBRTE_ETHER) += librte_ether
DIRS-$(CONFIG_RTE_LIBRTE_E1000_PMD) += librte_pmd_e1000
DIRS-$(CONFIG_RTE_LIBRTE_IXGBE_PMD) += librte_pmd_ixgbe
DIRS-$(CONFIG_RTE_LIBRTE_I40E_PMD) += librte_pmd_i40e
DIRS-$(CONFIG_RTE_LIBRTE_PMD_BOND) += librte_pmd_bond
DIRS-$(CONFIG_RTE_LIBRTE_PMD_RING) += librte_pmd_ring
DIRS-$(CONFIG_RTE_LIBRTE_PMD_PCAP) += librte_pmd_pcap
DIRS-$(CONFIG_RTE_LIBRTE_VIRTIO_PMD) += librte_pmd_virtio
DIRS-$(CONFIG_RTE_LIBRTE_VMXNET3_PMD) += librte_pmd_vmxnet3
DIRS-$(CONFIG_RTE_LIBRTE_PMD_XENVIRT) += librte_pmd_xenvirt
DIRS-$(CONFIG_RTE_LIBRTE_HASH) += librte_hash
DIRS-$(CONFIG_RTE_LIBRTE_LPM) += librte_lpm
DIRS-$(CONFIG_RTE_LIBRTE_ACL) += librte_acl
DIRS-$(CONFIG_RTE_LIBRTE_NET) += librte_net
DIRS-$(CONFIG_RTE_LIBRTE_IP_FRAG) += librte_ip_frag
DIRS-$(CONFIG_RTE_LIBRTE_POWER) += librte_power
DIRS-$(CONFIG_RTE_LIBRTE_METER) += librte_meter
DIRS-$(CONFIG_RTE_LIBRTE_SCHED) += librte_sched
DIRS-$(CONFIG_RTE_LIBRTE_KVARGS) += librte_kvargs
DIRS-$(CONFIG_RTE_LIBRTE_DISTRIBUTOR) += librte_distributor
DIRS-$(CONFIG_RTE_LIBRTE_PORT) += librte_port
DIRS-$(CONFIG_RTE_LIBRTE_TABLE) += librte_table
DIRS-$(CONFIG_RTE_LIBRTE_PIPELINE) += librte_pipeline

ifeq ($(CONFIG_RTE_EXEC_ENV_LINUXAPP),y)
DIRS-$(CONFIG_RTE_LIBRTE_KNI) += librte_kni
DIRS-$(CONFIG_RTE_LIBRTE_IVSHMEM) += librte_ivshmem
endif

include $(RTE_SDK)/mk/rte.sharelib.mk
include $(RTE_SDK)/mk/rte.subdir.mk
