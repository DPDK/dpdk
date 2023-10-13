..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2022 Intel Corporation

.. include:: <isonum.txt>

Intel\ |reg| vRAN Boost Poll Mode Driver (PMD)
==============================================

The Intel\ |reg| vRAN Boost integrated accelerator enables
cost-effective 4G and 5G next-generation virtualized Radio Access Network (vRAN)
solutions.
The Intel vRAN Boost v1.0 (VRB1 in the code) is specifically integrated on the
4th Gen Intel\ |reg| Xeon\ |reg| Scalable processor with Intel\ |reg| vRAN Boost,
also known as Sapphire Rapids Edge Enhanced (SPR-EE).

Features
--------

Intel vRAN Boost v1.0 includes a 5G Low Density Parity Check (LDPC) encoder/decoder,
rate match/dematch, Hybrid Automatic Repeat Request (HARQ) with access to DDR
memory for buffer management, a 4G Turbo encoder/decoder,
a Fast Fourier Transform (FFT) block providing DFT/iDFT processing offload
for the 5G Sounding Reference Signal (SRS), a Queue Manager (QMGR),
and a DMA subsystem.
There is no dedicated on-card memory for HARQ, the coherent memory on the CPU side is being used.

These hardware blocks provide the following features exposed by the PMD:

- LDPC Encode in the Downlink (5GNR)
- LDPC Decode in the Uplink (5GNR)
- Turbo Encode in the Downlink (4G)
- Turbo Decode in the Uplink (4G)
- FFT processing
- Single Root I/O Virtualization (SR-IOV) with 16 Virtual Functions (VFs) per Physical Function (PF)
- Maximum of 256 queues per VF
- Message Signaled Interrupts (MSIs)

The Intel vRAN Boost v1.0 PMD supports the following bbdev capabilities:

* For the LDPC encode operation:
   - ``RTE_BBDEV_LDPC_CRC_24B_ATTACH``: set to attach CRC24B to CB(s).
   - ``RTE_BBDEV_LDPC_RATE_MATCH``: if set then do not do Rate Match bypass.
   - ``RTE_BBDEV_LDPC_INTERLEAVER_BYPASS``: if set then bypass interleaver.

* For the LDPC decode operation:
   - ``RTE_BBDEV_LDPC_CRC_TYPE_24B_CHECK``: check CRC24B from CB(s).
   - ``RTE_BBDEV_LDPC_CRC_TYPE_24B_DROP``: drops CRC24B bits appended while decoding.
   - ``RTE_BBDEV_LDPC_CRC_TYPE_24A_CHECK``: check CRC24A from CB(s).
   - ``RTE_BBDEV_LDPC_CRC_TYPE_16_CHECK``: check CRC16 from CB(s).
   - ``RTE_BBDEV_LDPC_HQ_COMBINE_IN_ENABLE``: provides an input for HARQ combining.
   - ``RTE_BBDEV_LDPC_HQ_COMBINE_OUT_ENABLE``: provides an input for HARQ combining.
   - ``RTE_BBDEV_LDPC_ITERATION_STOP_ENABLE``: disable early termination.
   - ``RTE_BBDEV_LDPC_DEC_SCATTER_GATHER``: supports scatter-gather for input/output data.
   - ``RTE_BBDEV_LDPC_HARQ_6BIT_COMPRESSION``: supports compression of the HARQ input/output.
   - ``RTE_BBDEV_LDPC_LLR_COMPRESSION``: supports LLR input compression.

* For the turbo encode operation:
   - ``RTE_BBDEV_TURBO_CRC_24B_ATTACH``: set to attach CRC24B to CB(s).
   - ``RTE_BBDEV_TURBO_RATE_MATCH``: if set then do not do Rate Match bypass.
   - ``RTE_BBDEV_TURBO_ENC_INTERRUPTS``: set for encoder dequeue interrupts.
   - ``RTE_BBDEV_TURBO_RV_INDEX_BYPASS``: set to bypass RV index.
   - ``RTE_BBDEV_TURBO_ENC_SCATTER_GATHER``: supports scatter-gather for input/output data.

* For the turbo decode operation:
   - ``RTE_BBDEV_TURBO_CRC_TYPE_24B``: check CRC24B from CB(s).
   - ``RTE_BBDEV_TURBO_SUBBLOCK_DEINTERLEAVE``: perform subblock de-interleave.
   - ``RTE_BBDEV_TURBO_DEC_INTERRUPTS``: set for decoder dequeue interrupts.
   - ``RTE_BBDEV_TURBO_NEG_LLR_1_BIT_IN``: set if negative LLR input is supported.
   - ``RTE_BBDEV_TURBO_DEC_TB_CRC_24B_KEEP``: keep CRC24B bits appended while decoding.
   - ``RTE_BBDEV_TURBO_DEC_CRC_24B_DROP``: option to drop the code block CRC after decoding.
   - ``RTE_BBDEV_TURBO_EARLY_TERMINATION``: set early termination feature.
   - ``RTE_BBDEV_TURBO_DEC_SCATTER_GATHER``: supports scatter-gather for input/output data.
   - ``RTE_BBDEV_TURBO_HALF_ITERATION_EVEN``: set half iteration granularity.
   - ``RTE_BBDEV_TURBO_CONTINUE_CRC_MATCH``: set to run an extra odd iteration after CRC match.
   - ``RTE_BBDEV_TURBO_MAP_DEC``: supports flexible parallel MAP engine decoding.

* For the FFT operation:
   - ``RTE_BBDEV_FFT_WINDOWING``: flexible windowing capability.
   - ``RTE_BBDEV_FFT_CS_ADJUSTMENT``: flexible adjustment of Cyclic Shift time offset.
   - ``RTE_BBDEV_FFT_DFT_BYPASS``: set for bypass the DFT and get directly into iDFT input.
   - ``RTE_BBDEV_FFT_IDFT_BYPASS``: set for bypass the IDFT and get directly the DFT output.
   - ``RTE_BBDEV_FFT_WINDOWING_BYPASS``: set for bypass time domain windowing.


Installation
------------

Section 3 of the DPDK manual provides instructions on installing and compiling DPDK.

DPDK requires hugepages to be configured as detailed in section 2 of the DPDK manual.
The bbdev test application has been tested with a configuration 40 x 1GB hugepages.
The hugepage configuration of a server may be examined using:

.. code-block:: console

   grep Huge* /proc/meminfo


Initialization
--------------

When the device first powers up, its PCI Physical Functions (PF)
can be listed through these commands for Intel vRAN Boost v1:

.. code-block:: console

   sudo lspci -vd8086:57c0

The physical and virtual functions are compatible with Linux UIO drivers:
``vfio`` and ``igb_uio``.
However, in order to work the 5G/4G FEC device first needs to be bound
to one of these Linux drivers through DPDK.


Bind PF UIO driver(s)
~~~~~~~~~~~~~~~~~~~~~

Install the DPDK igb_uio driver, bind it with the PF PCI device ID and use
``lspci`` to confirm the PF device is under use by ``igb_uio`` DPDK UIO driver.

The igb_uio driver may be bound to the PF PCI device using one of two methods
for Intel vRAN Boost v1:

#. PCI functions (physical or virtual, depending on the use case) can be bound
to the UIO driver by repeating this command for every function.

.. code-block:: console

   cd <dpdk-top-level-directory>
   insmod build/kmod/igb_uio.ko
   echo "8086 57c0" > /sys/bus/pci/drivers/igb_uio/new_id
   lspci -vd8086:57c0

#. Another way to bind PF with DPDK UIO driver is by using the ``dpdk-devbind.py`` tool

.. code-block:: console

   cd <dpdk-top-level-directory>
   usertools/dpdk-devbind.py -b igb_uio 0000:f7:00.0

where the PCI device ID (example: 0000:f7:00.0) is obtained using ``lspci -vd8086:57c0``.

In a similar way the PF may be bound with vfio-pci as any PCIe device.


Enable Virtual Functions
~~~~~~~~~~~~~~~~~~~~~~~~

Now, it should be visible in the printouts that PCI PF is under igb_uio control
"``Kernel driver in use: igb_uio``"

To show the number of available VFs on the device, read ``sriov_totalvfs`` file.

.. code-block:: console

   cat /sys/bus/pci/devices/0000\:<b>\:<d>.<f>/sriov_totalvfs

where ``0000\:<b>\:<d>.<f>`` is the PCI device ID

To enable VFs via igb_uio, echo the number of virtual functions intended
to enable to ``max_vfs`` file.

.. code-block:: console

   echo <num-of-vfs> > /sys/bus/pci/devices/0000\:<b>\:<d>.<f>/max_vfs

Afterwards, all VFs must be bound to appropriate UIO drivers as required,
same way it was done with the physical function previously.

Enabling SR-IOV via VFIO driver is pretty much the same,
except that the file name is different:

.. code-block:: console

   echo <num-of-vfs> > /sys/bus/pci/devices/0000\:<b>\:<d>.<f>/sriov_numvfs


Configure the VFs through PF
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The PCI virtual functions must be configured before working or getting assigned
to VMs/Containers.
The configuration involves allocating the number of hardware queues, priorities,
load balance, bandwidth and other settings necessary for the device
to perform FEC functions.

This configuration needs to be executed at least once after reboot or PCI FLR
and can be achieved by using the functions ``rte_acc200_configure()``,
which sets up the parameters defined in the compatible ``acc200_conf`` structure.


Test Application
----------------

BBDEV provides a test application, ``test-bbdev.py`` and range of test data for testing
the functionality of the device, depending on the device's capabilities.

For more details on how to use the test application,
see :ref:`test_bbdev_application`.


Test Vectors
~~~~~~~~~~~~

In addition to the simple LDPC decoder and LDPC encoder tests,
bbdev also provides a range of additional tests under the test_vectors folder,
which may be useful.
The results of these tests will depend on the device capabilities which may
cause some test cases to be skipped, but no failure should be reported.


Alternate Baseband Device configuration tool
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

On top of the embedded configuration feature supported in test-bbdev using
"- -init-device" option mentioned above, there is also a tool available
to perform that device configuration using a companion application.
The ``pf_bb_config`` application notably enables then to run bbdev-test
from the VF and not only limited to the PF as captured above.

See for more details: https://github.com/intel/pf-bb-config

Specifically for the bbdev Intel vRAN Boost v1 PMD, the command below can be used
(note that ACC200 was used previously to refer to VRB1):

.. code-block:: console

   pf_bb_config ACC200 -c ./acc200/acc200_config_vf_5g.cfg
   test-bbdev.py -e="-c 0xff0 -a${VF_PCI_ADDR}" -c validation -n 64 -b 64 -l 1 -v ./ldpc_dec_default.data
