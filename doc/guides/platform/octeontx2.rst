..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2019 Marvell International Ltd.

Marvell OCTEON TX2 Platform Guide
=================================

This document gives an overview of **Marvell OCTEON TX2** RVU H/W block,
packet flow and procedure to build DPDK on OCTEON TX2 platform.

More information about OCTEON TX2 SoC can be found at `Marvell Official Website
<https://www.marvell.com/embedded-processors/infrastructure-processors/>`_.

Supported OCTEON TX2 SoCs
-------------------------

- CN98xx
- CN96xx
- CN93xx

OCTEON TX2 Resource Virtualization Unit architecture
----------------------------------------------------

The :numref:`figure_octeontx2_resource_virtualization` diagram depicts the
RVU architecture and a resource provisioning example.

.. _figure_octeontx2_resource_virtualization:

.. figure:: img/octeontx2_resource_virtualization.*

    OCTEON TX2 Resource virtualization architecture and provisioning example


Resource Virtualization Unit (RVU) on Marvell's OCTEON TX2 SoC maps HW
resources belonging to the network, crypto and other functional blocks onto
PCI-compatible physical and virtual functions.

Each functional block has multiple local functions (LFs) for
provisioning to different PCIe devices. RVU supports multiple PCIe SRIOV
physical functions (PFs) and virtual functions (VFs).

The :numref:`table_octeontx2_rvu_dpdk_mapping` shows the various local
functions (LFs) provided by the RVU and its functional mapping to
DPDK subsystem.

.. _table_octeontx2_rvu_dpdk_mapping:

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
   | 8 | DPI | rte_rawdev                                                   |
   +---+-----+--------------------------------------------------------------+
   | 9 | SDP | rte_ethdev                                                   |
   +---+-----+--------------------------------------------------------------+
   | 10| REE | rte_regexdev                                                 |
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

The :numref:`figure_octeontx2_resource_virtualization` diagram also shows a
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
to DMA packets into and out of OCTEON TX2 SoC. SDP interface comes in to live only when
OCTEON TX2 SoC is connected in PCIe endpoint mode. It can be used to send/receive
packets to/from remote host machine using input/output queue pairs exposed to it.
SDP interface receives input packets from remote host from NIX-RX and sends packets
to remote host using NIX-TX. Remote host machine need to use corresponding driver
(kernel/user mode) to communicate with SDP interface on OCTEON TX2 SoC. SDP supports
single PCIe SRIOV physical function(PF) and multiple virtual functions(VF's). Users
can bind PF or VF to use SDP interface and it will be enumerated as ethdev ports.

The primary use case for SDP is to enable the smart NIC use case. Typical usage models are,

#. Communication channel between remote host and OCTEON TX2 SoC over PCIe.
#. Transfer packets received from network interface to remote host over PCIe and
   vice-versa.

OCTEON TX2 packet flow
----------------------

The :numref:`figure_octeontx2_packet_flow_hw_accelerators` diagram depicts
the packet flow on OCTEON TX2 SoC in conjunction with use of various HW accelerators.

.. _figure_octeontx2_packet_flow_hw_accelerators:

.. figure:: img/octeontx2_packet_flow_hw_accelerators.*

    OCTEON TX2 packet flow in conjunction with use of HW accelerators

HW Offload Drivers
------------------

This section lists dataplane H/W block(s) available in OCTEON TX2 SoC.

#. **Ethdev Driver**
   See :doc:`../nics/octeontx2` for NIX Ethdev driver information.

#. **Mempool Driver**
   See :doc:`../mempool/octeontx2` for NPA mempool driver information.

#. **Event Device Driver**
   See :doc:`../eventdevs/octeontx2` for SSO event device driver information.

#. **DMA Rawdev Driver**
   See :doc:`../rawdevs/octeontx2_dma` for DMA driver information.

#. **Crypto Device Driver**
   See :doc:`../cryptodevs/octeontx2` for CPT crypto device driver information.

#. **Regex Device Driver**
   See :doc:`../regexdevs/octeontx2` for REE regex device driver information.

Procedure to Setup Platform
---------------------------

There are three main prerequisites for setting up DPDK on OCTEON TX2
compatible board:

1. **OCTEON TX2 Linux kernel driver**

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
   optimized for OCTEON TX2 CPU.

3. **Rootfile system**

   Any *aarch64* supporting filesystem may be used. For example,
   Ubuntu 15.10 (Wily) or 16.04 LTS (Xenial) userland which can be obtained
   from `<http://cdimage.ubuntu.com/ubuntu-base/releases/16.04/release/ubuntu-base-16.04.1-base-arm64.tar.gz>`_.

   Alternatively, the Marvell SDK provides the buildroot based root filesystem.
   The SDK includes all the above prerequisites necessary to bring up the OCTEON TX2 board.

- Follow the DPDK :doc:`../linux_gsg/index` to setup the basic DPDK environment.


Debugging Options
-----------------

.. _table_octeontx2_common_debug_options:

.. table:: OCTEON TX2 common debug options

   +---+------------+-------------------------------------------------------+
   | # | Component  | EAL log command                                       |
   +===+============+=======================================================+
   | 1 | Common     | --log-level='pmd\.octeontx2\.base,8'                  |
   +---+------------+-------------------------------------------------------+
   | 2 | Mailbox    | --log-level='pmd\.octeontx2\.mbox,8'                  |
   +---+------------+-------------------------------------------------------+

Debugfs support
~~~~~~~~~~~~~~~

The **OCTEON TX2 Linux kernel driver** provides support to dump RVU blocks
context or stats using debugfs.

Enable ``debugfs`` by:

1. Compile kernel with debugfs enabled, i.e ``CONFIG_DEBUGFS=y``.
2. Boot OCTEON TX2 with debugfs supported kernel.
3. Verify ``debugfs`` mounted by default "mount | grep -i debugfs" or mount it manually by using.

.. code-block:: console

       # mount -t debugfs none /sys/kernel/debug

Currently ``debugfs`` supports the following RVU blocks NIX, NPA, NPC, NDC,
SSO & CGX.

The file structure under ``/sys/kernel/debug`` is as follows

.. code-block:: console

        octeontx2/
        |-- cgx
        |   |-- cgx0
        |   |   '-- lmac0
        |   |       '-- stats
        |   |-- cgx1
        |   |   |-- lmac0
        |   |   |   '-- stats
        |   |   '-- lmac1
        |   |       '-- stats
        |   '-- cgx2
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
        |   |-- sq_ctx
        |   '-- tx_stall_hwissue
        |-- npa
        |   |-- aura_ctx
        |   |-- ndc_cache
        |   |-- ndc_hits_miss
        |   |-- pool_ctx
        |   '-- qsize
        |-- npc
        |    |-- mcam_info
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

        cat /sys/kernel/debug/octeontx2/rsrc_alloc

        pcifunc    NPA    NIX    SSO GROUP    SSOWS    TIM    CPT
        PF1         0       0
        PF4                 1
        PF13                          0, 1     0, 1      0

CGX example usage:

.. code-block:: console

        cat /sys/kernel/debug/octeontx2/cgx/cgx2/lmac0/stats

        =======Link Status======
        Link is UP 40000 Mbps
        =======RX_STATS======
        Received packets: 0
        Octets of received packets: 0
        Received PAUSE packets: 0
        Received PAUSE and control packets: 0
        Filtered DMAC0 (NIX-bound) packets: 0
        Filtered DMAC0 (NIX-bound) octets: 0
        Packets dropped due to RX FIFO full: 0
        Octets dropped due to RX FIFO full: 0
        Error packets: 0
        Filtered DMAC1 (NCSI-bound) packets: 0
        Filtered DMAC1 (NCSI-bound) octets: 0
        NCSI-bound packets dropped: 0
        NCSI-bound octets dropped: 0
        =======TX_STATS======
        Packets dropped due to excessive collisions: 0
        Packets dropped due to excessive deferral: 0
        Multiple collisions before successful transmission: 0
        Single collisions before successful transmission: 0
        Total octets sent on the interface: 0
        Total frames sent on the interface: 0
        Packets sent with an octet count < 64: 0
        Packets sent with an octet count == 64: 0
        Packets sent with an octet count of 65127: 0
        Packets sent with an octet count of 128-255: 0
        Packets sent with an octet count of 256-511: 0
        Packets sent with an octet count of 512-1023: 0
        Packets sent with an octet count of 1024-1518: 0
        Packets sent with an octet count of > 1518: 0
        Packets sent to a broadcast DMAC: 0
        Packets sent to the multicast DMAC: 0
        Transmit underflow and were truncated: 0
        Control/PAUSE packets sent: 0

CPT example usage:

.. code-block:: console

        cat /sys/kernel/debug/octeontx2/cpt/cpt_pc

        CPT instruction requests   0
        CPT instruction latency    0
        CPT NCB read requests      0
        CPT NCB read latency       0
        CPT read requests caused by UC fills   0
        CPT active cycles pc       1395642
        CPT clock count pc         5579867595493

NIX example usage:

.. code-block:: console

        Usage: echo <nixlf> [cq number/all] > /sys/kernel/debug/octeontx2/nix/cq_ctx
               cat /sys/kernel/debug/octeontx2/nix/cq_ctx
        echo 0 0 > /sys/kernel/debug/octeontx2/nix/cq_ctx
        cat /sys/kernel/debug/octeontx2/nix/cq_ctx

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

        Usage: echo <npalf> [pool number/all] > /sys/kernel/debug/octeontx2/npa/pool_ctx
               cat /sys/kernel/debug/octeontx2/npa/pool_ctx
        echo 0 0 > /sys/kernel/debug/octeontx2/npa/pool_ctx
        cat /sys/kernel/debug/octeontx2/npa/pool_ctx

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

        cat /sys/kernel/debug/octeontx2/npc/mcam_info

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

        Usage: echo [<hws>/all] > /sys/kernel/debug/octeontx2/sso/hws/sso_hws_info
        echo 0 > /sys/kernel/debug/octeontx2/sso/hws/sso_hws_info

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

DPDK may be compiled either natively on OCTEON TX2 platform or cross-compiled on
an x86 based platform.

Native Compilation
~~~~~~~~~~~~~~~~~~

.. code-block:: console

        meson build
        ninja -C build

Cross Compilation
~~~~~~~~~~~~~~~~~

Refer to :doc:`../linux_gsg/cross_build_dpdk_for_arm64` for generic arm64 details.

.. code-block:: console

        meson build --cross-file config/arm/arm64_octeontx2_linux_gcc
        ninja -C build

.. note::

   By default, meson cross compilation uses ``aarch64-linux-gnu-gcc`` toolchain,
   if Marvell toolchain is available then it can be used by overriding the
   c, cpp, ar, strip ``binaries`` attributes to respective Marvell
   toolchain binaries in ``config/arm/arm64_octeontx2_linux_gcc`` file.
