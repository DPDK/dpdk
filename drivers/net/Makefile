#   BSD LICENSE
#
#   Copyright(c) 2010-2015 Intel Corporation. All rights reserved.
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

# set in mk/toolchain/xxx/rte.toolchain-compat.mk
ifeq ($(CONFIG_RTE_LIBRTE_THUNDERX_NICVF_PMD),d)
        $(warning thunderx pmd is not supported by old compilers)
endif

core-libs := librte_eal librte_mbuf librte_mempool librte_ring librte_ether
core-libs += librte_net librte_kvargs

DIRS-$(CONFIG_RTE_LIBRTE_PMD_AF_PACKET) += af_packet
DEPDIRS-af_packet = $(core-libs)
DIRS-$(CONFIG_RTE_LIBRTE_ARK_PMD) += ark
DEPDIRS-ark = $(core-libs)
DIRS-$(CONFIG_RTE_LIBRTE_AVP_PMD) += avp
DEPDIRS-avp = $(core-libs)
DIRS-$(CONFIG_RTE_LIBRTE_BNX2X_PMD) += bnx2x
DEPDIRS-bnx2x = $(core-libs)
DIRS-$(CONFIG_RTE_LIBRTE_PMD_BOND) += bonding
DEPDIRS-bonding = $(core-libs) librte_cmdline
DIRS-$(CONFIG_RTE_LIBRTE_CXGBE_PMD) += cxgbe
DEPDIRS-cxgbe = $(core-libs)
DIRS-$(CONFIG_RTE_LIBRTE_DPAA2_PMD) += dpaa2
DEPDIRS-dpaa2 = $(core-libs)
DIRS-$(CONFIG_RTE_LIBRTE_E1000_PMD) += e1000
DEPDIRS-e1000 = $(core-libs)
DIRS-$(CONFIG_RTE_LIBRTE_ENA_PMD) += ena
DEPDIRS-ena = $(core-libs)
DIRS-$(CONFIG_RTE_LIBRTE_ENIC_PMD) += enic
DEPDIRS-enic = $(core-libs) librte_hash
DIRS-$(CONFIG_RTE_LIBRTE_FM10K_PMD) += fm10k
DEPDIRS-fm10k = $(core-libs) librte_hash
DIRS-$(CONFIG_RTE_LIBRTE_I40E_PMD) += i40e
DEPDIRS-i40e = $(core-libs) librte_hash
DIRS-$(CONFIG_RTE_LIBRTE_IXGBE_PMD) += ixgbe
DEPDIRS-ixgbe = $(core-libs) librte_hash
DIRS-$(CONFIG_RTE_LIBRTE_LIO_PMD) += liquidio
DEPDIRS-liquidio = $(core-libs)
DIRS-$(CONFIG_RTE_LIBRTE_MLX4_PMD) += mlx4
DEPDIRS-mlx4 = $(core-libs)
DIRS-$(CONFIG_RTE_LIBRTE_MLX5_PMD) += mlx5
DEPDIRS-mlx5 = $(core-libs)
DIRS-$(CONFIG_RTE_LIBRTE_NFP_PMD) += nfp
DEPDIRS-nfp = $(core-libs)
DIRS-$(CONFIG_RTE_LIBRTE_BNXT_PMD) += bnxt
DEPDIRS-bnxt = $(core-libs)
DIRS-$(CONFIG_RTE_LIBRTE_PMD_NULL) += null
DEPDIRS-null = $(core-libs)
DIRS-$(CONFIG_RTE_LIBRTE_PMD_PCAP) += pcap
DEPDIRS-pcap = $(core-libs)
DIRS-$(CONFIG_RTE_LIBRTE_QEDE_PMD) += qede
DEPDIRS-qede = $(core-libs)
DIRS-$(CONFIG_RTE_LIBRTE_PMD_RING) += ring
DEPDIRS-ring = $(core-libs)
DIRS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD) += sfc
DEPDIRS-sfc = $(core-libs)
DIRS-$(CONFIG_RTE_LIBRTE_PMD_SZEDATA2) += szedata2
DEPDIRS-szedata2 = $(core-libs)
DIRS-$(CONFIG_RTE_LIBRTE_PMD_TAP) += tap
DEPDIRS-tap = $(core-libs) librte_hash
DIRS-$(CONFIG_RTE_LIBRTE_THUNDERX_NICVF_PMD) += thunderx
DEPDIRS-thunderx = $(core-libs)
DIRS-$(CONFIG_RTE_LIBRTE_VIRTIO_PMD) += virtio
DEPDIRS-virtio = $(core-libs)
DIRS-$(CONFIG_RTE_LIBRTE_VMXNET3_PMD) += vmxnet3
DEPDIRS-vmxnet3 = $(core-libs)
DIRS-$(CONFIG_RTE_LIBRTE_PMD_XENVIRT) += xenvirt
DEPDIRS-xenvirt = $(core-libs) librte_cmdline

ifeq ($(CONFIG_RTE_LIBRTE_KNI),y)
DIRS-$(CONFIG_RTE_LIBRTE_PMD_KNI) += kni
endif
DEPDIRS-kni = $(core-libs) librte_kni

ifeq ($(CONFIG_RTE_LIBRTE_VHOST),y)
DIRS-$(CONFIG_RTE_LIBRTE_PMD_VHOST) += vhost
endif # $(CONFIG_RTE_LIBRTE_VHOST)
DEPDIRS-vhost = $(core-libs) librte_vhost

include $(RTE_SDK)/mk/rte.subdir.mk
