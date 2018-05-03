# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2017 Intel Corporation.
# Copyright(c) 2014-2015 6WIND S.A.

include $(RTE_SDK)/mk/internal/rte.compile-pre.mk
include $(RTE_SDK)/mk/internal/rte.install-pre.mk
include $(RTE_SDK)/mk/internal/rte.clean-pre.mk
include $(RTE_SDK)/mk/internal/rte.build-pre.mk

# VPATH contains at least SRCDIR
VPATH += $(SRCDIR)

_BUILD = $(APP)
_INSTALL = $(INSTALL-FILES-y) $(SYMLINK-FILES-y)
_INSTALL += $(RTE_OUTPUT)/app/$(APP) $(RTE_OUTPUT)/app/$(APP).map
POSTINSTALL += target-appinstall
_CLEAN = doclean
POSTCLEAN += target-appclean

ifeq ($(NO_LDSCRIPT),)
LDSCRIPT = $(RTE_LDSCRIPT)
endif

# Link only the libraries used in the application
LDFLAGS += --as-needed

# default path for libs
_LDLIBS-y += -L$(RTE_SDK_BIN)/lib

#
# Order is important: from higher level to lower level
#
_LDLIBS-$(CONFIG_RTE_LIBRTE_FLOW_CLASSIFY)  += -lrte_flow_classify
_LDLIBS-$(CONFIG_RTE_LIBRTE_PIPELINE)       += -lrte_pipeline
_LDLIBS-$(CONFIG_RTE_LIBRTE_TABLE)          += -lrte_table
_LDLIBS-$(CONFIG_RTE_LIBRTE_PORT)           += -lrte_port

_LDLIBS-$(CONFIG_RTE_LIBRTE_PDUMP)          += -lrte_pdump
_LDLIBS-$(CONFIG_RTE_LIBRTE_DISTRIBUTOR)    += -lrte_distributor
_LDLIBS-$(CONFIG_RTE_LIBRTE_IP_FRAG)        += -lrte_ip_frag
_LDLIBS-$(CONFIG_RTE_LIBRTE_GRO)            += -lrte_gro
_LDLIBS-$(CONFIG_RTE_LIBRTE_GSO)            += -lrte_gso
_LDLIBS-$(CONFIG_RTE_LIBRTE_METER)          += -lrte_meter
_LDLIBS-$(CONFIG_RTE_LIBRTE_LPM)            += -lrte_lpm
# librte_acl needs --whole-archive because of weak functions
_LDLIBS-$(CONFIG_RTE_LIBRTE_ACL)            += --whole-archive
_LDLIBS-$(CONFIG_RTE_LIBRTE_ACL)            += -lrte_acl
_LDLIBS-$(CONFIG_RTE_LIBRTE_ACL)            += --no-whole-archive
_LDLIBS-$(CONFIG_RTE_LIBRTE_JOBSTATS)       += -lrte_jobstats
_LDLIBS-$(CONFIG_RTE_LIBRTE_METRICS)        += -lrte_metrics
_LDLIBS-$(CONFIG_RTE_LIBRTE_BITRATE)        += -lrte_bitratestats
_LDLIBS-$(CONFIG_RTE_LIBRTE_LATENCY_STATS)  += -lrte_latencystats
_LDLIBS-$(CONFIG_RTE_LIBRTE_POWER)          += -lrte_power

_LDLIBS-$(CONFIG_RTE_LIBRTE_EFD)            += -lrte_efd
_LDLIBS-$(CONFIG_RTE_LIBRTE_BPF)            += -lrte_bpf
ifeq ($(CONFIG_RTE_LIBRTE_BPF_ELF),y)
_LDLIBS-$(CONFIG_RTE_LIBRTE_BPF)            += -lelf
endif

_LDLIBS-y += --whole-archive

_LDLIBS-$(CONFIG_RTE_LIBRTE_CFGFILE)        += -lrte_cfgfile
_LDLIBS-$(CONFIG_RTE_LIBRTE_HASH)           += -lrte_hash
_LDLIBS-$(CONFIG_RTE_LIBRTE_MEMBER)         += -lrte_member
_LDLIBS-$(CONFIG_RTE_LIBRTE_VHOST)          += -lrte_vhost
_LDLIBS-$(CONFIG_RTE_LIBRTE_KVARGS)         += -lrte_kvargs
_LDLIBS-$(CONFIG_RTE_LIBRTE_MBUF)           += -lrte_mbuf
_LDLIBS-$(CONFIG_RTE_LIBRTE_NET)            += -lrte_net
_LDLIBS-$(CONFIG_RTE_LIBRTE_ETHER)          += -lrte_ethdev
_LDLIBS-$(CONFIG_RTE_LIBRTE_BBDEV)          += -lrte_bbdev
_LDLIBS-$(CONFIG_RTE_LIBRTE_CRYPTODEV)      += -lrte_cryptodev
_LDLIBS-$(CONFIG_RTE_LIBRTE_SECURITY)       += -lrte_security
_LDLIBS-$(CONFIG_RTE_LIBRTE_COMPRESSDEV)    += -lrte_compressdev
_LDLIBS-$(CONFIG_RTE_LIBRTE_EVENTDEV)       += -lrte_eventdev
_LDLIBS-$(CONFIG_RTE_LIBRTE_RAWDEV)         += -lrte_rawdev
_LDLIBS-$(CONFIG_RTE_LIBRTE_TIMER)          += -lrte_timer
_LDLIBS-$(CONFIG_RTE_LIBRTE_MEMPOOL)        += -lrte_mempool
_LDLIBS-$(CONFIG_RTE_DRIVER_MEMPOOL_RING)   += -lrte_mempool_ring
_LDLIBS-$(CONFIG_RTE_LIBRTE_RING)           += -lrte_ring
_LDLIBS-$(CONFIG_RTE_LIBRTE_PCI)            += -lrte_pci
_LDLIBS-$(CONFIG_RTE_LIBRTE_EAL)            += -lrte_eal
_LDLIBS-$(CONFIG_RTE_LIBRTE_CMDLINE)        += -lrte_cmdline
_LDLIBS-$(CONFIG_RTE_LIBRTE_REORDER)        += -lrte_reorder
_LDLIBS-$(CONFIG_RTE_LIBRTE_SCHED)          += -lrte_sched

ifeq ($(CONFIG_RTE_EXEC_ENV_LINUXAPP),y)
_LDLIBS-$(CONFIG_RTE_LIBRTE_KNI)            += -lrte_kni
endif

ifeq ($(CONFIG_RTE_LIBRTE_PMD_OCTEONTX_SSOVF)$(CONFIG_RTE_LIBRTE_OCTEONTX_MEMPOOL),yy)
_LDLIBS-y += -lrte_common_octeontx
endif

_LDLIBS-$(CONFIG_RTE_LIBRTE_PCI_BUS)        += -lrte_bus_pci
_LDLIBS-$(CONFIG_RTE_LIBRTE_VDEV_BUS)       += -lrte_bus_vdev
_LDLIBS-$(CONFIG_RTE_LIBRTE_DPAA_BUS)       += -lrte_bus_dpaa
ifeq ($(CONFIG_RTE_EAL_VFIO),y)
_LDLIBS-$(CONFIG_RTE_LIBRTE_FSLMC_BUS)      += -lrte_bus_fslmc
endif

ifeq ($(CONFIG_RTE_BUILD_SHARED_LIB),n)
# plugins (link only if static libraries)

_LDLIBS-$(CONFIG_RTE_DRIVER_MEMPOOL_BUCKET) += -lrte_mempool_bucket
_LDLIBS-$(CONFIG_RTE_DRIVER_MEMPOOL_STACK)  += -lrte_mempool_stack
ifeq ($(CONFIG_RTE_LIBRTE_DPAA_BUS),y)
_LDLIBS-$(CONFIG_RTE_LIBRTE_DPAA_MEMPOOL)   += -lrte_mempool_dpaa
endif
ifeq ($(CONFIG_RTE_EAL_VFIO)$(CONFIG_RTE_LIBRTE_FSLMC_BUS),yy)
_LDLIBS-$(CONFIG_RTE_LIBRTE_DPAA2_MEMPOOL)  += -lrte_mempool_dpaa2
endif

_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_AF_PACKET)  += -lrte_pmd_af_packet
_LDLIBS-$(CONFIG_RTE_LIBRTE_ARK_PMD)        += -lrte_pmd_ark
_LDLIBS-$(CONFIG_RTE_LIBRTE_AVF_PMD)        += -lrte_pmd_avf
_LDLIBS-$(CONFIG_RTE_LIBRTE_AVP_PMD)        += -lrte_pmd_avp
_LDLIBS-$(CONFIG_RTE_LIBRTE_AXGBE_PMD)      += -lrte_pmd_axgbe
_LDLIBS-$(CONFIG_RTE_LIBRTE_BNX2X_PMD)      += -lrte_pmd_bnx2x -lz
_LDLIBS-$(CONFIG_RTE_LIBRTE_BNXT_PMD)       += -lrte_pmd_bnxt
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_BOND)       += -lrte_pmd_bond
_LDLIBS-$(CONFIG_RTE_LIBRTE_CXGBE_PMD)      += -lrte_pmd_cxgbe
ifeq ($(CONFIG_RTE_LIBRTE_DPAA_BUS),y)
_LDLIBS-$(CONFIG_RTE_LIBRTE_DPAA_PMD)       += -lrte_pmd_dpaa
endif
ifeq ($(CONFIG_RTE_EAL_VFIO)$(CONFIG_RTE_LIBRTE_FSLMC_BUS),yy)
_LDLIBS-$(CONFIG_RTE_LIBRTE_DPAA2_PMD)      += -lrte_pmd_dpaa2
endif
_LDLIBS-$(CONFIG_RTE_LIBRTE_E1000_PMD)      += -lrte_pmd_e1000
_LDLIBS-$(CONFIG_RTE_LIBRTE_ENA_PMD)        += -lrte_pmd_ena
_LDLIBS-$(CONFIG_RTE_LIBRTE_ENIC_PMD)       += -lrte_pmd_enic
_LDLIBS-$(CONFIG_RTE_LIBRTE_FM10K_PMD)      += -lrte_pmd_fm10k
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_FAILSAFE)   += -lrte_pmd_failsafe
_LDLIBS-$(CONFIG_RTE_LIBRTE_I40E_PMD)       += -lrte_pmd_i40e
_LDLIBS-$(CONFIG_RTE_LIBRTE_IXGBE_PMD)      += -lrte_pmd_ixgbe
ifeq ($(CONFIG_RTE_LIBRTE_KNI),y)
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_KNI)        += -lrte_pmd_kni
endif
_LDLIBS-$(CONFIG_RTE_LIBRTE_LIO_PMD)        += -lrte_pmd_lio
ifeq ($(CONFIG_RTE_LIBRTE_MLX4_DLOPEN_DEPS),y)
_LDLIBS-$(CONFIG_RTE_LIBRTE_MLX4_PMD)       += -lrte_pmd_mlx4 -ldl
else
_LDLIBS-$(CONFIG_RTE_LIBRTE_MLX4_PMD)       += -lrte_pmd_mlx4 -libverbs -lmlx4
endif
ifeq ($(CONFIG_RTE_LIBRTE_MLX5_DLOPEN_DEPS),y)
_LDLIBS-$(CONFIG_RTE_LIBRTE_MLX5_PMD)       += -lrte_pmd_mlx5 -ldl
else
_LDLIBS-$(CONFIG_RTE_LIBRTE_MLX5_PMD)       += -lrte_pmd_mlx5 -libverbs -lmlx5
endif
_LDLIBS-$(CONFIG_RTE_LIBRTE_MVPP2_PMD)      += -lrte_pmd_mvpp2 -L$(LIBMUSDK_PATH)/lib -lmusdk
_LDLIBS-$(CONFIG_RTE_LIBRTE_NFP_PMD)        += -lrte_pmd_nfp
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_NULL)       += -lrte_pmd_null
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_PCAP)       += -lrte_pmd_pcap -lpcap
_LDLIBS-$(CONFIG_RTE_LIBRTE_QEDE_PMD)       += -lrte_pmd_qede
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_RING)       += -lrte_pmd_ring
ifeq ($(CONFIG_RTE_LIBRTE_SCHED),y)
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_SOFTNIC)      += -lrte_pmd_softnic
endif
_LDLIBS-$(CONFIG_RTE_LIBRTE_SFC_EFX_PMD)    += -lrte_pmd_sfc_efx
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_SZEDATA2)   += -lrte_pmd_szedata2 -lsze2
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_TAP)        += -lrte_pmd_tap
_LDLIBS-$(CONFIG_RTE_LIBRTE_THUNDERX_NICVF_PMD) += -lrte_pmd_thunderx_nicvf
_LDLIBS-$(CONFIG_RTE_LIBRTE_VDEV_NETVSC_PMD) += -lrte_pmd_vdev_netvsc
_LDLIBS-$(CONFIG_RTE_LIBRTE_VIRTIO_PMD)     += -lrte_pmd_virtio
ifeq ($(CONFIG_RTE_LIBRTE_VHOST),y)
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_VHOST)      += -lrte_pmd_vhost
ifeq ($(CONFIG_RTE_EAL_VFIO),y)
_LDLIBS-$(CONFIG_RTE_LIBRTE_IFCVF_VDPA_PMD) += -lrte_ifcvf_vdpa
endif # $(CONFIG_RTE_EAL_VFIO)
endif # $(CONFIG_RTE_LIBRTE_VHOST)
_LDLIBS-$(CONFIG_RTE_LIBRTE_VMXNET3_PMD)    += -lrte_pmd_vmxnet3_uio

ifeq ($(CONFIG_RTE_LIBRTE_BBDEV),y)
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_BBDEV_NULL)     += -lrte_pmd_bbdev_null

# TURBO SOFTWARE PMD is dependent on the FLEXRAN library
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_BBDEV_TURBO_SW) += -lrte_pmd_bbdev_turbo_sw
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_BBDEV_TURBO_SW) += -L$(FLEXRAN_SDK)/lib_crc -lcrc
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_BBDEV_TURBO_SW) += -L$(FLEXRAN_SDK)/lib_turbo -lturbo
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_BBDEV_TURBO_SW) += -L$(FLEXRAN_SDK)/lib_rate_matching -lrate_matching
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_BBDEV_TURBO_SW) += -L$(FLEXRAN_SDK)/lib_common -lcommon
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_BBDEV_TURBO_SW) += -lirc -limf -lstdc++ -lipps
endif # CONFIG_RTE_LIBRTE_BBDEV

ifeq ($(CONFIG_RTE_LIBRTE_CRYPTODEV),y)
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_AESNI_MB)    += -lrte_pmd_aesni_mb
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_AESNI_MB)    += -lIPSec_MB
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_AESNI_GCM)   += -lrte_pmd_aesni_gcm
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_AESNI_GCM)   += -lIPSec_MB
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_CCP)         += -lrte_pmd_ccp -lcrypto
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_OPENSSL)     += -lrte_pmd_openssl -lcrypto
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_NULL_CRYPTO) += -lrte_pmd_null_crypto
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_QAT)         += -lrte_pmd_qat -lcrypto
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_SNOW3G)      += -lrte_pmd_snow3g
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_SNOW3G)      += -L$(LIBSSO_SNOW3G_PATH)/build -lsso_snow3g
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_KASUMI)      += -lrte_pmd_kasumi
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_KASUMI)      += -L$(LIBSSO_KASUMI_PATH)/build -lsso_kasumi
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_ZUC)         += -lrte_pmd_zuc
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_ZUC)         += -L$(LIBSSO_ZUC_PATH)/build -lsso_zuc
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_ARMV8_CRYPTO)    += -lrte_pmd_armv8
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_ARMV8_CRYPTO)    += -L$(ARMV8_CRYPTO_LIB_PATH) -larmv8_crypto
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_MVSAM_CRYPTO) += -L$(LIBMUSDK_PATH)/lib -lrte_pmd_mvsam_crypto -lmusdk
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_CRYPTO_SCHEDULER) += -lrte_pmd_crypto_scheduler
ifeq ($(CONFIG_RTE_EAL_VFIO)$(CONFIG_RTE_LIBRTE_FSLMC_BUS),yy)
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_DPAA2_SEC)   += -lrte_pmd_dpaa2_sec
endif # CONFIG_RTE_LIBRTE_FSLMC_BUS
ifeq ($(CONFIG_RTE_LIBRTE_DPAA_BUS),y)
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_DPAA_SEC)   += -lrte_pmd_dpaa_sec
endif # CONFIG_RTE_LIBRTE_DPAA_BUS
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_VIRTIO_CRYPTO) += -lrte_pmd_virtio_crypto
endif # CONFIG_RTE_LIBRTE_CRYPTODEV

ifeq ($(CONFIG_RTE_LIBRTE_COMPRESSDEV),y)
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_ISAL) += -lrte_pmd_isal_comp
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_ISAL) += -lisal
endif # CONFIG_RTE_LIBRTE_COMPRESSDEV

ifeq ($(CONFIG_RTE_LIBRTE_EVENTDEV),y)
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_SKELETON_EVENTDEV) += -lrte_pmd_skeleton_event
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_SW_EVENTDEV) += -lrte_pmd_sw_event
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_OCTEONTX_SSOVF) += -lrte_pmd_octeontx_ssovf
ifeq ($(CONFIG_RTE_LIBRTE_DPAA_BUS),y)
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_DPAA_EVENTDEV) += -lrte_pmd_dpaa_event
endif # CONFIG_RTE_LIBRTE_DPAA_BUS
ifeq ($(CONFIG_RTE_EAL_VFIO)$(CONFIG_RTE_LIBRTE_FSLMC_BUS),yy)
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_DPAA2_EVENTDEV) += -lrte_pmd_dpaa2_event
endif # CONFIG_RTE_LIBRTE_FSLMC_BUS

_LDLIBS-$(CONFIG_RTE_LIBRTE_OCTEONTX_MEMPOOL) += -lrte_mempool_octeontx
_LDLIBS-$(CONFIG_RTE_LIBRTE_OCTEONTX_PMD) += -lrte_pmd_octeontx
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_OPDL_EVENTDEV) += -lrte_pmd_opdl_event
endif # CONFIG_RTE_LIBRTE_EVENTDEV

ifeq ($(CONFIG_RTE_LIBRTE_RAWDEV),y)
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_SKELETON_RAWDEV) += -lrte_pmd_skeleton_rawdev
ifeq ($(CONFIG_RTE_EAL_VFIO)$(CONFIG_RTE_LIBRTE_FSLMC_BUS),yy)
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_DPAA2_CMDIF_RAWDEV) += -lrte_pmd_dpaa2_cmdif
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_DPAA2_QDMA_RAWDEV) += -lrte_pmd_dpaa2_qdma
endif # CONFIG_RTE_LIBRTE_FSLMC_BUS
_LDLIBS-$(CONFIG_RTE_LIBRTE_IFPGA_BUS)      += -lrte_bus_ifpga
ifeq ($(CONFIG_RTE_LIBRTE_IFPGA_BUS),y)
_LDLIBS-$(CONFIG_RTE_LIBRTE_PMD_IFPGA_RAWDEV)   += -lrte_pmd_ifpga_rawdev
endif # CONFIG_RTE_LIBRTE_IFPGA_BUS
endif # CONFIG_RTE_LIBRTE_RAWDEV

endif # !CONFIG_RTE_BUILD_SHARED_LIBS

_LDLIBS-y += --no-whole-archive

ifeq ($(CONFIG_RTE_BUILD_SHARED_LIB),n)
# The static libraries do not know their dependencies.
# So linking with static library requires explicit dependencies.
_LDLIBS-$(CONFIG_RTE_LIBRTE_EAL)            += -lrt
ifeq ($(CONFIG_RTE_EXEC_ENV_LINUXAPP)$(CONFIG_RTE_EAL_NUMA_AWARE_HUGEPAGES),yy)
_LDLIBS-$(CONFIG_RTE_LIBRTE_EAL)            += -lnuma
endif
ifeq ($(CONFIG_RTE_EXEC_ENV_LINUXAPP)$(CONFIG_RTE_USE_LIBBSD),yy)
_LDLIBS-$(CONFIG_RTE_LIBRTE_EAL)            += -lbsd
endif
_LDLIBS-$(CONFIG_RTE_LIBRTE_SCHED)          += -lm
_LDLIBS-$(CONFIG_RTE_LIBRTE_SCHED)          += -lrt
_LDLIBS-$(CONFIG_RTE_LIBRTE_MEMBER)         += -lm
_LDLIBS-$(CONFIG_RTE_LIBRTE_METER)          += -lm
ifeq ($(CONFIG_RTE_LIBRTE_VHOST_NUMA),y)
_LDLIBS-$(CONFIG_RTE_LIBRTE_VHOST)          += -lnuma
endif
_LDLIBS-$(CONFIG_RTE_PORT_PCAP)             += -lpcap
endif # !CONFIG_RTE_BUILD_SHARED_LIBS

_LDLIBS-y += $(EXECENV_LDLIBS)

LDLIBS += $(_LDLIBS-y) $(CPU_LDLIBS) $(EXTRA_LDLIBS)

# all the words except the first one
allbutfirst = $(wordlist 2,$(words $(1)),$(1))

# Eliminate duplicates without sorting, only keep the last occurrence
filter-libs = \
	$(if $(1),$(strip\
		$(if \
			$(and \
				$(filter $(firstword $(1)),$(call allbutfirst,$(1))),\
				$(filter -l%,$(firstword $(1)))),\
			,\
			$(firstword $(1))) \
		$(call filter-libs,$(call allbutfirst,$(1)))))

LDLIBS := $(call filter-libs,$(LDLIBS))

ifeq ($(RTE_DEVEL_BUILD)$(CONFIG_RTE_BUILD_SHARED_LIB),yy)
LDFLAGS += -rpath=$(RTE_SDK_BIN)/lib
endif

MAPFLAGS = -Map=$@.map --cref

.PHONY: all
all: install

.PHONY: install
install: build _postinstall

_postinstall: build

.PHONY: build
build: _postbuild

exe2cmd = $(strip $(call dotfile,$(patsubst %,%.cmd,$(1))))

ifeq ($(LINK_USING_CC),1)
O_TO_EXE = $(CC) -o $@ $(CFLAGS) $(EXTRA_CFLAGS) $(OBJS-y) $(call linkerprefix, \
	$(LDLIBS) $(LDFLAGS) $(LDFLAGS_$(@)) $(EXTRA_LDFLAGS) \
	$(MAPFLAGS))
else
O_TO_EXE = $(LD) -o $@ $(OBJS-y) \
	$(LDLIBS) $(LDFLAGS) $(LDFLAGS_$(@)) $(EXTRA_LDFLAGS) \
	$(MAPFLAGS)
endif
O_TO_EXE_STR = $(subst ','\'',$(O_TO_EXE)) #'# fix syntax highlight
O_TO_EXE_DISP = $(if $(V),"$(O_TO_EXE_STR)","  LD $(@)")
O_TO_EXE_CMD = "cmd_$@ = $(O_TO_EXE_STR)"
O_TO_EXE_DO = @set -e; \
	echo $(O_TO_EXE_DISP); \
	$(O_TO_EXE) && \
	echo $(O_TO_EXE_CMD) > $(call exe2cmd,$(@))

-include .$(APP).cmd

# path where libraries are retrieved
LDLIBS_PATH := $(subst -Wl$(comma)-L,,$(filter -Wl$(comma)-L%,$(LDLIBS)))
LDLIBS_PATH += $(subst -L,,$(filter -L%,$(LDLIBS)))

# list of .a files that are linked to this application
LDLIBS_NAMES := $(patsubst -l%,lib%.a,$(filter -l%,$(LDLIBS)))
LDLIBS_NAMES += $(patsubst -Wl$(comma)-l%,lib%.a,$(filter -Wl$(comma)-l%,$(LDLIBS)))

# list of found libraries files (useful for deps). If not found, the
# library is silently ignored and dep won't be checked
LDLIBS_FILES := $(sort $(wildcard $(foreach dir,$(LDLIBS_PATH),\
	$(addprefix $(dir)/,$(LDLIBS_NAMES)))))

#
# Compile executable file if needed
#
$(APP): $(OBJS-y) $(LDLIBS_FILES) $(DEP_$(APP)) $(LDSCRIPT) FORCE
	@[ -d $(dir $@) ] || mkdir -p $(dir $@)
	$(if $(D),\
		@echo -n "$< -> $@ " ; \
		echo -n "file_missing=$(call boolean,$(file_missing)) " ; \
		echo -n "cmdline_changed=$(call boolean,$(call cmdline_changed,$(O_TO_EXE_STR))) " ; \
		echo -n "depfile_missing=$(call boolean,$(depfile_missing)) " ; \
		echo "depfile_newer=$(call boolean,$(depfile_newer)) ")
	$(if $(or \
		$(file_missing),\
		$(call cmdline_changed,$(O_TO_EXE_STR)),\
		$(depfile_missing),\
		$(depfile_newer)),\
		$(O_TO_EXE_DO))

#
# install app in $(RTE_OUTPUT)/app
#
$(RTE_OUTPUT)/app/$(APP): $(APP)
	@echo "  INSTALL-APP $(APP)"
	@[ -d $(RTE_OUTPUT)/app ] || mkdir -p $(RTE_OUTPUT)/app
	$(Q)cp -f $(APP) $(RTE_OUTPUT)/app

#
# install app map file in $(RTE_OUTPUT)/app
#
$(RTE_OUTPUT)/app/$(APP).map: $(APP)
	@echo "  INSTALL-MAP $(APP).map"
	@[ -d $(RTE_OUTPUT)/app ] || mkdir -p $(RTE_OUTPUT)/app
	$(Q)cp -f $(APP).map $(RTE_OUTPUT)/app

#
# Clean all generated files
#
.PHONY: clean
clean: _postclean
	$(Q)rm -f $(_BUILD_TARGETS) $(_INSTALL_TARGETS) $(_CLEAN_TARGETS)

.PHONY: doclean
doclean:
	$(Q)rm -rf $(APP) $(OBJS-all) $(DEPS-all) $(DEPSTMP-all) \
	  $(CMDS-all) $(INSTALL-FILES-all) .$(APP).cmd $(APP).map


include $(RTE_SDK)/mk/internal/rte.compile-post.mk
include $(RTE_SDK)/mk/internal/rte.install-post.mk
include $(RTE_SDK)/mk/internal/rte.clean-post.mk
include $(RTE_SDK)/mk/internal/rte.build-post.mk

ifneq ($(wildcard $(RTE_SDK)/mk/target/$(RTE_TARGET)/rte.app.mk),)
include $(RTE_SDK)/mk/target/$(RTE_TARGET)/rte.app.mk
else
include $(RTE_SDK)/mk/target/generic/rte.app.mk
endif

.PHONY: FORCE
FORCE:
