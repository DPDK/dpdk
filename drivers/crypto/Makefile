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

core-libs := librte_eal librte_mbuf librte_mempool librte_ring librte_cryptodev

DIRS-$(CONFIG_RTE_LIBRTE_PMD_AESNI_GCM) += aesni_gcm
DEPDIRS-aesni_gcm = $(core-libs)
DIRS-$(CONFIG_RTE_LIBRTE_PMD_AESNI_MB) += aesni_mb
DEPDIRS-aesni_mb = $(core-libs)
DIRS-$(CONFIG_RTE_LIBRTE_PMD_ARMV8_CRYPTO) += armv8
DEPDIRS-armv8 = $(core-libs)
DIRS-$(CONFIG_RTE_LIBRTE_PMD_OPENSSL) += openssl
DEPDIRS-openssl = $(core-libs)
DIRS-$(CONFIG_RTE_LIBRTE_PMD_QAT) += qat
DEPDIRS-qat = $(core-libs)
DIRS-$(CONFIG_RTE_LIBRTE_PMD_CRYPTO_SCHEDULER) += scheduler
DEPDIRS-scheduler = $(core-libs) librte_kvargs librte_reorder
DIRS-$(CONFIG_RTE_LIBRTE_PMD_SNOW3G) += snow3g
DEPDIRS-snow3g = $(core-libs)
DIRS-$(CONFIG_RTE_LIBRTE_PMD_KASUMI) += kasumi
DEPDIRS-kasumi = $(core-libs)
DIRS-$(CONFIG_RTE_LIBRTE_PMD_ZUC) += zuc
DEPDIRS-zuc = $(core-libs)
DIRS-$(CONFIG_RTE_LIBRTE_PMD_NULL_CRYPTO) += null
DEPDIRS-null = $(core-libs)
DIRS-$(CONFIG_RTE_LIBRTE_PMD_DPAA2_SEC) += dpaa2_sec
DEPDIRS-dpaa2_sec = $(core-libs)

include $(RTE_SDK)/mk/rte.subdir.mk
