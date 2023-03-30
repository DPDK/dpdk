..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(C) 2021 Marvell.

Marvell cnxk platform guide
===========================

This document gives an overview of **Marvell OCTEON CN9K and CN10K** RVU H/W block,
packet flow and procedure to build DPDK on OCTEON cnxk platform.

More information about CN9K and CN10K SoC can be found at `Marvell Official Website
<https://www.marvell.com/embedded-processors/infrastructure-processors/>`_.

Supported OCTEON cnxk SoCs
--------------------------

- CN93xx
- CN96xx
- CN98xx
- CN106xx
- CNF105xx
- CN103XX

Resource Virtualization Unit architecture
-----------------------------------------

The :numref:`figure_cnxk_resource_virtualization` diagram depicts the
RVU architecture and a resource provisioning example.

.. _figure_cnxk_resource_virtualization:

.. figure:: img/cnxk_resource_virtualization.*

    cnxk Resource virtualization architecture and provisioning example


Resource Virtualization Unit (RVU) on Marvell's OCTEON CN9K/CN10K SoC maps HW
resources belonging to the network, crypto and other functional blocks onto
PCI-compatible physical and virtual functions.

Each functional block has multiple local functions (LFs) for
provisioning to different PCIe devices. RVU supports multiple PCIe SRIOV
physical functions (PFs) and virtual functions (VFs).

The :numref:`table_cnxk_rvu_dpdk_mapping` shows the various local
functions (LFs) provided by the RVU and its functional mapping to
DPDK subsystem.

.. _table_cnxk_rvu_dpdk_mapping:

.. table:: RVU managed functional blocks and its mapping to DPDK subsystem

   +---+-----+--------------------------------------------------------------+
   | # | LF  | DPDK subsystem mapping                                       |
   +===+=====+==============================================================+
   | 1 | NIX | rte_ethdev, rte_tm, rte_event_eth_[rt]x_adapter, rte_security|
   +---+-----+--------------------------------------------------------------+
   | 2 | NPA | rte_mempool                                                  |
   +---+-----+--------------------------------------------------------------+
   | 3 | NPC | rte_flow                                                     |
   +---+-----+--------------------------------------------------------------+
   | 4 | CPT | rte_cryptodev, rte_event_crypto_adapter                      |
   +---+-----+--------------------------------------------------------------+
   | 5 | SSO | rte_eventdev                                                 |
   +---+-----+--------------------------------------------------------------+
   | 6 | TIM | rte_event_timer_adapter                                      |
   +---+-----+--------------------------------------------------------------+
   | 7 | LBK | rte_ethdev                                                   |
   +---+-----+--------------------------------------------------------------+
   | 8 | DPI | rte_dmadev                                                   |
   +---+-----+--------------------------------------------------------------+
   | 9 | SDP | rte_ethdev                                                   |
   +---+-----+--------------------------------------------------------------+
   | 10| REE | rte_regexdev                                                 |
   +---+-----+--------------------------------------------------------------+
   | 11| BPHY| rte_rawdev                                                   |
   +---+-----+--------------------------------------------------------------+
   | 12| GPIO| rte_rawdev                                                   |
   +---+-----+--------------------------------------------------------------+
   | 13| ML  | rte_mldev                                                    |
   +---+-----+--------------------------------------------------------------+

PF0 is called the administrative / admin function (AF) and has exclusive
privileges to provision RVU functional block's LFs to each of the PF/VF.

PF/VFs communicates with AF via a shared memory region (mailbox).Upon receiving
requests from PF/VF, AF does resource provisioning and other HW configuration.

AF is always attached to host, but PF/VFs may be used by host kernel itself,
or attached to VMs or to userspace applications like DPDK, etc. So, AF has to
handle provisioning/configuration requests sent by any device from any domain.

The AF driver does not receive or process any data.
It is only a configuration driver used in control path.

The :numref:`figure_cnxk_resource_virtualization` diagram also shows a
resource provisioning example where,

1. PFx and PFx-VF0 bound to Linux netdev driver.
2. PFx-VF1 ethdev driver bound to the first DPDK application.
3. PFy ethdev driver, PFy-VF0 ethdev driver, PFz eventdev driver, PFm-VF0 cryptodev driver bound to the second DPDK application.

LBK HW Access
-------------

Loopback HW Unit (LBK) receives packets from NIX-RX and sends packets back to NIX-TX.
The loopback block has N channels and contains data buffering that is shared across
all channels. The LBK HW Unit is abstracted using ethdev subsystem, Where PF0's
VFs are exposed as ethdev device and odd-even pairs of VFs are tied together,
that is, packets sent on odd VF end up received on even VF and vice versa.
This would enable HW accelerated means of communication between two domains
where even VF bound to the first domain and odd VF bound to the second domain.

Typical application usage models are,

#. Communication between the Linux kernel and DPDK application.
#. Exception path to Linux kernel from DPDK application as SW ``KNI`` replacement.
#. Communication between two different DPDK applications.

SDP interface
-------------

System DPI Packet Interface unit(SDP) provides PCIe endpoint support for remote host
to DMA packets into and out of cnxk SoC. SDP interface comes in to live only when
cnxk SoC is connected in PCIe endpoint mode. It can be used to send/receive
packets to/from remote host machine using input/output queue pairs exposed to it.
SDP interface receives input packets from remote host from NIX-RX and sends packets
to remote host using NIX-TX. Remote host machine need to use corresponding driver
(kernel/user mode) to communicate with SDP interface on cnxk SoC. SDP supports
single PCIe SRIOV physical function(PF) and multiple virtual functions(VF's). Users
can bind PF or VF to use SDP interface and it will be enumerated as ethdev ports.

The primary use case for SDP is to enable the smart NIC use case. Typical usage models are,

#. Communication channel between remote host and cnxk SoC over PCIe.
#. Transfer packets received from network interface to remote host over PCIe and
   vice-versa.

cnxk packet flow
----------------------

The :numref:`figure_cnxk_packet_flow_hw_accelerators` diagram depicts
the packet flow on cnxk SoC in conjunction with use of various HW accelerators.

.. _figure_cnxk_packet_flow_hw_accelerators:

.. figure:: img/cnxk_packet_flow_hw_accelerators.*

    cnxk packet flow in conjunction with use of HW accelerators

HW Offload Drivers
------------------

This section lists dataplane H/W block(s) available in cnxk SoC.

#. **Ethdev Driver**
   See :doc:`../nics/cnxk` for NIX Ethdev driver information.

#. **Mempool Driver**
   See :doc:`../mempool/cnxk` for NPA mempool driver information.

#. **Baseband PHY Driver**
   See :doc:`../rawdevs/cnxk_bphy` for Baseband PHY driver information.

#. **Dmadev Driver**
   See :doc:`../dmadevs/cnxk` for DPI Dmadev driver information.

#. **Regex Device Driver**
   See :doc:`../regexdevs/cn9k` for REE Regex device driver information.

#. **ML Device Driver**
   See :doc:`../mldevs/cnxk` for Machine Learning device driver information.

Procedure to Setup Platform
---------------------------

There are three main prerequisites for setting up DPDK on cnxk
compatible board:

1. **RVU AF Linux kernel driver**

   The dependent kernel drivers can be obtained from the
   `kernel.org <https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/drivers/net/ethernet/marvell/octeontx2>`_.

   Alternatively, the Marvell SDK also provides the required kernel drivers.

   Linux kernel should be configured with the following features enabled:

.. code-block:: console

        # 64K pages enabled for better performance
        CONFIG_ARM64_64K_PAGES=y
        CONFIG_ARM64_VA_BITS_48=y
        # huge pages support enabled
        CONFIG_HUGETLBFS=y
        CONFIG_HUGETLB_PAGE=y
        # VFIO enabled with TYPE1 IOMMU at minimum
        CONFIG_VFIO_IOMMU_TYPE1=y
        CONFIG_VFIO_VIRQFD=y
        CONFIG_VFIO=y
        CONFIG_VFIO_NOIOMMU=y
        CONFIG_VFIO_PCI=y
        CONFIG_VFIO_PCI_MMAP=y
        # SMMUv3 driver
        CONFIG_ARM_SMMU_V3=y
        # ARMv8.1 LSE atomics
        CONFIG_ARM64_LSE_ATOMICS=y
        # OCTEONTX2 drivers
        CONFIG_OCTEONTX2_MBOX=y
        CONFIG_OCTEONTX2_AF=y
        # Enable if netdev PF driver required
        CONFIG_OCTEONTX2_PF=y
        # Enable if netdev VF driver required
        CONFIG_OCTEONTX2_VF=y
        CONFIG_CRYPTO_DEV_OCTEONTX2_CPT=y
        # Enable if OCTEONTX2 DMA PF driver required
        CONFIG_OCTEONTX2_DPI_PF=n

2. **ARM64 Linux Tool Chain**

   For example, the *aarch64* Linaro Toolchain, which can be obtained from
   `here <https://releases.linaro.org/components/toolchain/binaries/7.4-2019.02/aarch64-linux-gnu/>`_.

   Alternatively, the Marvell SDK also provides GNU GCC toolchain, which is
   optimized for cnxk CPU.

3. **Rootfile system**

   Any *aarch64* supporting filesystem may be used. For example,
   Ubuntu 15.10 (Wily) or 16.04 LTS (Xenial) userland which can be obtained
   from `<http://cdimage.ubuntu.com/ubuntu-base/releases/16.04/release/ubuntu-base-16.04.1-base-arm64.tar.gz>`_.

   Alternatively, the Marvell SDK provides the buildroot based root filesystem.
   The SDK includes all the above prerequisites necessary to bring up the cnxk board.

- Follow the DPDK :doc:`../linux_gsg/index` to setup the basic DPDK environment.


Debugging Options
-----------------

.. _table_cnxk_common_debug_options:

.. table:: cnxk common debug options

   +---+------------+-------------------------------------------------------+
   | # | Component  | EAL log command                                       |
   +===+============+=======================================================+
   | 1 | Common     | --log-level='pmd\.cnxk\.base,8'                       |
   +---+------------+-------------------------------------------------------+
   | 2 | Mailbox    | --log-level='pmd\.cnxk\.mbox,8'                       |
   +---+------------+-------------------------------------------------------+

Debugfs support
~~~~~~~~~~~~~~~

The **RVU AF Linux kernel driver** provides support to dump RVU blocks
context or stats using debugfs.

Enable ``debugfs`` by:

1. Compile kernel with debugfs enabled, i.e ``CONFIG_DEBUGFS=y``.
2. Boot OCTEON CN9K/CN10K with debugfs supported kernel.
3. Verify ``debugfs`` mounted by default "mount | grep -i debugfs" or mount it manually by using.

.. code-block:: console

       # mount -t debugfs none /sys/kernel/debug

Currently ``debugfs`` supports the following RVU blocks NIX, NPA, NPC, NDC,
SSO & RPM.

The file structure under ``/sys/kernel/debug`` is as follows

.. code-block:: console

        octeontx2/
        |
        cn10k/
        |-- rpm
        |   |-- rpm0
        |   |   '-- lmac0
        |   |       '-- stats
        |   |-- rpm1
        |   |   |-- lmac0
        |   |   |   '-- stats
        |   |   '-- lmac1
        |   |       '-- stats
        |   '-- rpm2
        |       '-- lmac0
        |           '-- stats
        |-- cpt
        |   |-- cpt_engines_info
        |   |-- cpt_engines_sts
        |   |-- cpt_err_info
        |   |-- cpt_lfs_info
        |   '-- cpt_pc
        |---- nix
        |   |-- cq_ctx
        |   |-- ndc_rx_cache
        |   |-- ndc_rx_hits_miss
        |   |-- ndc_tx_cache
        |   |-- ndc_tx_hits_miss
        |   |-- qsize
        |   |-- rq_ctx
        |   '-- sq_ctx
        |-- npa
        |   |-- aura_ctx
        |   |-- ndc_cache
        |   |-- ndc_hits_miss
        |   |-- pool_ctx
        |   '-- qsize
        |-- npc
        |    |-- mcam_info
        |    |-- mcam_rules
        |    '-- rx_miss_act_stats
        |-- rsrc_alloc
        '-- sso
             |-- hws
             |   '-- sso_hws_info
             '-- hwgrp
                 |-- sso_hwgrp_aq_thresh
                 |-- sso_hwgrp_iaq_walk
                 |-- sso_hwgrp_pc
                 |-- sso_hwgrp_free_list_walk
                 |-- sso_hwgrp_ient_walk
                 '-- sso_hwgrp_taq_walk

RVU block LF allocation:

.. code-block:: console

        cat /sys/kernel/debug/cn10k/rsrc_alloc

        pcifunc    NPA    NIX    SSO GROUP    SSOWS    TIM    CPT
        PF1         0       0
        PF4                 1
        PF13                          0, 1     0, 1      0

RPM example usage:

.. code-block:: console

        cat /sys/kernel/debug/cn10k/rpm/rpm0/lmac0/stats

        =======Link Status======

        Link is UP 25000 Mbps

        =======NIX RX_STATS(rpm port level)======

        rx_ucast_frames: 0
        rx_mcast_frames: 0
        rx_bcast_frames: 0
        rx_frames: 0
        rx_bytes: 0
        rx_drops: 0
        rx_errors: 0

        =======NIX TX_STATS(rpm port level)======

        tx_ucast_frames: 0
        tx_mcast_frames: 0
        tx_bcast_frames: 0
        tx_frames: 0
        tx_bytes: 0
        tx_drops: 0

        =======rpm RX_STATS======

        Octets of received packets: 0
        Octets of received packets with out error: 0
        Received packets with alignment errors: 0
        Control/PAUSE packets received: 0
        Packets received with Frame too long Errors: 0
        Packets received with a1nrange length Errors: 0
        Received packets: 0
        Packets received with FrameCheckSequenceErrors: 0
        Packets received with VLAN header: 0
        Error packets: 0
        Packets received with unicast DMAC: 0
        Packets received with multicast DMAC: 0
        Packets received with broadcast DMAC: 0
        Dropped packets: 0
        Total frames received on interface: 0
        Packets received with an octet count < 64: 0
        Packets received with an octet count == 64: 0
        Packets received with an octet count of 65–127: 0
        Packets received with an octet count of 128-255: 0
        Packets received with an octet count of 256-511: 0
        Packets received with an octet count of 512-1023: 0
        Packets received with an octet count of 1024-1518: 0
        Packets received with an octet count of > 1518: 0
        Oversized Packets: 0
        Jabber Packets: 0
        Fragmented Packets: 0
        CBFC(class based flow control) pause frames received for class 0: 0
        CBFC pause frames received for class 1: 0
        CBFC pause frames received for class 2: 0
        CBFC pause frames received for class 3: 0
        CBFC pause frames received for class 4: 0
        CBFC pause frames received for class 5: 0
        CBFC pause frames received for class 6: 0
        CBFC pause frames received for class 7: 0
        CBFC pause frames received for class 8: 0
        CBFC pause frames received for class 9: 0
        CBFC pause frames received for class 10: 0
        CBFC pause frames received for class 11: 0
        CBFC pause frames received for class 12: 0
        CBFC pause frames received for class 13: 0
        CBFC pause frames received for class 14: 0
        CBFC pause frames received for class 15: 0
        MAC control packets received: 0

        =======rpm TX_STATS======

        Total octets sent on the interface: 0
        Total octets transmitted OK: 0
        Control/Pause frames sent: 0
        Total frames transmitted OK: 0
        Total frames sent with VLAN header: 0
        Error Packets: 0
        Packets sent to unicast DMAC: 0
        Packets sent to the multicast DMAC: 0
        Packets sent to a broadcast DMAC: 0
        Packets sent with an octet count == 64: 0
        Packets sent with an octet count of 65–127: 0
        Packets sent with an octet count of 128-255: 0
        Packets sent with an octet count of 256-511: 0
        Packets sent with an octet count of 512-1023: 0
        Packets sent with an octet count of 1024-1518: 0
        Packets sent with an octet count of > 1518: 0
        CBFC(class based flow control) pause frames transmitted for class 0: 0
        CBFC pause frames transmitted for class 1: 0
        CBFC pause frames transmitted for class 2: 0
        CBFC pause frames transmitted for class 3: 0
        CBFC pause frames transmitted for class 4: 0
        CBFC pause frames transmitted for class 5: 0
        CBFC pause frames transmitted for class 6: 0
        CBFC pause frames transmitted for class 7: 0
        CBFC pause frames transmitted for class 8: 0
        CBFC pause frames transmitted for class 9: 0
        CBFC pause frames transmitted for class 10: 0
        CBFC pause frames transmitted for class 11: 0
        CBFC pause frames transmitted for class 12: 0
        CBFC pause frames transmitted for class 13: 0
        CBFC pause frames transmitted for class 14: 0
        CBFC pause frames transmitted for class 15: 0
        MAC control packets sent: 0
        Total frames sent on the interface: 0

CPT example usage:

.. code-block:: console

        cat /sys/kernel/debug/cn10k/cpt/cpt_pc

        CPT instruction requests   0
        CPT instruction latency    0
        CPT NCB read requests      0
        CPT NCB read latency       0
        CPT read requests caused by UC fills   0
        CPT active cycles pc       1395642
        CPT clock count pc         5579867595493

NIX example usage:

.. code-block:: console

        Usage: echo <nixlf> [cq number/all] > /sys/kernel/debug/cn10k/nix/cq_ctx
               cat /sys/kernel/debug/cn10k/nix/cq_ctx
        echo 0 0 > /sys/kernel/debug/cn10k/nix/cq_ctx
        cat /sys/kernel/debug/cn10k/nix/cq_ctx

        =====cq_ctx for nixlf:0 and qidx:0 is=====
        W0: base                        158ef1a00

        W1: wrptr                       0
        W1: avg_con                     0
        W1: cint_idx                    0
        W1: cq_err                      0
        W1: qint_idx                    0
        W1: bpid                        0
        W1: bp_ena                      0

        W2: update_time                 31043
        W2:avg_level                    255
        W2: head                        0
        W2:tail                         0

        W3: cq_err_int_ena              5
        W3:cq_err_int                   0
        W3: qsize                       4
        W3:caching                      1
        W3: substream                   0x000
        W3: ena                                 1
        W3: drop_ena                    1
        W3: drop                        64
        W3: bp                          0

NPA example usage:

.. code-block:: console

        Usage: echo <npalf> [pool number/all] > /sys/kernel/debug/cn10k/npa/pool_ctx
               cat /sys/kernel/debug/cn10k/npa/pool_ctx
        echo 0 0 > /sys/kernel/debug/cn10k/npa/pool_ctx
        cat /sys/kernel/debug/cn10k/npa/pool_ctx

        ======POOL : 0=======
        W0: Stack base          1375bff00
        W1: ena                 1
        W1: nat_align           1
        W1: stack_caching       1
        W1: stack_way_mask      0
        W1: buf_offset          1
        W1: buf_size            19
        W2: stack_max_pages     24315
        W2: stack_pages         24314
        W3: op_pc               267456
        W4: stack_offset        2
        W4: shift               5
        W4: avg_level           255
        W4: avg_con             0
        W4: fc_ena              0
        W4: fc_stype            0
        W4: fc_hyst_bits        0
        W4: fc_up_crossing      0
        W4: update_time         62993
        W5: fc_addr             0
        W6: ptr_start           1593adf00
        W7: ptr_end             180000000
        W8: err_int             0
        W8: err_int_ena         7
        W8: thresh_int          0
        W8: thresh_int_ena      0
        W8: thresh_up           0
        W8: thresh_qint_idx     0
        W8: err_qint_idx        0

NPC example usage:

.. code-block:: console

        cat /sys/kernel/debug/cn10k/npc/mcam_info

        NPC MCAM info:
        RX keywidth    : 224bits
        TX keywidth    : 224bits

        MCAM entries   : 2048
        Reserved       : 158
        Available      : 1890

        MCAM counters  : 512
        Reserved       : 1
        Available      : 511

SSO example usage:

.. code-block:: console

        Usage: echo [<hws>/all] > /sys/kernel/debug/cn10k/sso/hws/sso_hws_info
        echo 0 > /sys/kernel/debug/cn10k/sso/hws/sso_hws_info

        ==================================================
        SSOW HWS[0] Arbitration State      0x0
        SSOW HWS[0] Guest Machine Control  0x0
        SSOW HWS[0] SET[0] Group Mask[0] 0xffffffffffffffff
        SSOW HWS[0] SET[0] Group Mask[1] 0xffffffffffffffff
        SSOW HWS[0] SET[0] Group Mask[2] 0xffffffffffffffff
        SSOW HWS[0] SET[0] Group Mask[3] 0xffffffffffffffff
        SSOW HWS[0] SET[1] Group Mask[0] 0xffffffffffffffff
        SSOW HWS[0] SET[1] Group Mask[1] 0xffffffffffffffff
        SSOW HWS[0] SET[1] Group Mask[2] 0xffffffffffffffff
        SSOW HWS[0] SET[1] Group Mask[3] 0xffffffffffffffff
        ==================================================

Compile DPDK
------------

DPDK may be compiled either natively on OCTEON CN9K/CN10K platform or cross-compiled on
an x86 based platform.
Meson build option ``enable_iova_as_pa`` is disabled on CNXK platforms.
So only PMDs supporting this option are enabled on CNXK platform builds.

Native Compilation
~~~~~~~~~~~~~~~~~~

Refer to :doc:`../linux_gsg/build_dpdk` for generic native builds.

CN9K:

.. code-block:: console

        meson setup -Dplatform=cn9k build
        ninja -C build

CN10K:

.. code-block:: console

        meson setup -Dplatform=cn10k build
        ninja -C build

Cross Compilation
~~~~~~~~~~~~~~~~~

Refer to :doc:`../linux_gsg/cross_build_dpdk_for_arm64` for generic arm64 details.

CN9K:

.. code-block:: console

        meson setup build --cross-file config/arm/arm64_cn9k_linux_gcc
        ninja -C build

CN10K:

.. code-block:: console

        meson setup build --cross-file config/arm/arm64_cn10k_linux_gcc
        ninja -C build

.. note::

   By default, meson cross compilation uses ``aarch64-linux-gnu-gcc`` toolchain,
   if Marvell toolchain is available then it can be used by overriding the
   c, cpp, ar, strip ``binaries`` attributes to respective Marvell
   toolchain binaries in ``config/arm/arm64_cn10k_linux_gcc`` file.

Environment Variables
~~~~~~~~~~~~~~~~~~~~~

* ``BPHY_INTR_MLOCK_DISABLE``
   When defined disables memory locking in
   BPHY environment.

* ``ROC_CN10K_MBOX_TIMEOUT``, ``ROC_MBOX_TIMEOUT``
   When set, overrides MBOX timeout by value in milli seconds.

* ``ETH_SEC_IV_OVR``
   When set, overrides outbound inline SA IV. By default IV is generated
   by HW. Format of variable is string of comma separated one byte values as
   for ex: "0x0, 0x10, 0x20, ..."
