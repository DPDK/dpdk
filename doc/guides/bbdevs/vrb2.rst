.. SPDX-License-Identifier: BSD-3-Clause
   Copyright(c) 2023 Intel Corporation

.. include:: <isonum.txt>

Intel\ |reg| vRAN Boost v2 Poll Mode Driver (PMD)
=================================================

The Intel\ |reg| vRAN Boost integrated accelerator enables
cost-effective 4G and 5G next-generation virtualized Radio Access Network (vRAN)
solutions.
The Intel vRAN Boost v2.0 (VRB2 in the code) is specifically integrated on the
Intel\ |reg| Xeon\ |reg| Granite Rapids-D Process (GNR-D).

Features
--------

Intel vRAN Boost v2.0 includes a 5G Low Density Parity Check (LDPC) encoder/decoder,
rate match/dematch, Hybrid Automatic Repeat Request (HARQ) with access to DDR
memory for buffer management, a 4G Turbo encoder/decoder,
a Fast Fourier Transform (FFT) block providing DFT/iDFT processing offload
for the 5G Sounding Reference Signal (SRS), a MLD-TS accelerator, a Queue Manager (QMGR),
and a DMA subsystem.
There is no dedicated on-card memory for HARQ, the coherent memory on the CPU side is being used.

These hardware blocks provide the following features exposed by the PMD:

- LDPC Encode in the Downlink (5GNR)
- LDPC Decode in the Uplink (5GNR)
- Turbo Encode in the Downlink (4G)
- Turbo Decode in the Uplink (4G)
- FFT processing
- MLD-TS processing
- Single Root I/O Virtualization (SR-IOV) with 16 Virtual Functions (VFs) per Physical Function (PF)
- Maximum of 2048 queues per VF
- Message Signaled Interrupts (MSIs)

The Intel vRAN Boost v2.0 PMD supports the following bbdev capabilities:

* For the LDPC encode operation:
   - ``RTE_BBDEV_LDPC_CRC_24B_ATTACH``: set to attach CRC24B to CB(s).
   - ``RTE_BBDEV_LDPC_RATE_MATCH``: if set then do not do Rate Match bypass.
   - ``RTE_BBDEV_LDPC_INTERLEAVER_BYPASS``: if set then bypass interleaver.
   - ``RTE_BBDEV_LDPC_ENC_SCATTER_GATHER``: supports scatter-gather for input/output data.
   - ``RTE_BBDEV_LDPC_ENC_CONCATENATION``: concatenate code blocks with bit granularity.

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
   - ``RTE_BBDEV_LDPC_HARQ_4BIT_COMPRESSION``: supports compression of the HARQ input/output.
   - ``RTE_BBDEV_LDPC_SOFT_OUT_ENABLE``: set the APP LLR soft output.
   - ``RTE_BBDEV_LDPC_SOFT_OUT_RM_BYPASS``: set the APP LLR soft output after rate-matching.
   - ``RTE_BBDEV_LDPC_SOFT_OUT_DEINTERLEAVER_BYPASS``: disables the de-interleaver.

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
   - ``RTE_BBDEV_TURBO_SOFT_OUTPUT``: set the APP LLR soft output.
   - ``RTE_BBDEV_TURBO_EQUALIZER``: set the turbo equalizer feature.
   - ``RTE_BBDEV_TURBO_SOFT_OUT_SATURATE``: set the soft output saturation.
   - ``RTE_BBDEV_TURBO_CONTINUE_CRC_MATCH``: set to run an extra odd iteration after CRC match.
   - ``RTE_BBDEV_TURBO_NEG_LLR_1_BIT_SOFT_OUT``: set if negative APP LLR output supported.
   - ``RTE_BBDEV_TURBO_MAP_DEC``: supports flexible parallel MAP engine decoding.

* For the FFT operation:
   - ``RTE_BBDEV_FFT_WINDOWING``: flexible windowing capability.
   - ``RTE_BBDEV_FFT_CS_ADJUSTMENT``: flexible adjustment of Cyclic Shift time offset.
   - ``RTE_BBDEV_FFT_DFT_BYPASS``: set for bypass the DFT and get directly into iDFT input.
   - ``RTE_BBDEV_FFT_IDFT_BYPASS``: set for bypass the IDFT and get directly the DFT output.
   - ``RTE_BBDEV_FFT_WINDOWING_BYPASS``: set for bypass time domain windowing.

* For the MLD-TS operation:
   - ``RTE_BBDEV_MLDTS_REP``: set to repeat and reuse channel across operations.

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
can be listed through these commands for Intel vRAN Boost v2:

.. code-block:: console

   sudo lspci -vd8086:57c2

The physical and virtual functions are compatible with Linux UIO drivers:
``vfio`` (preferred) and ``igb_uio`` (legacy).
However, in order to work the 5G/4G FEC device first needs to be bound
to one of these Linux drivers through DPDK.


Configure the VFs through PF
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The PCI virtual functions must be configured before working or getting assigned
to VMs/Containers.
The configuration involves allocating the number of hardware queues, priorities,
load balance, bandwidth and other settings necessary for the device
to perform FEC functions.

This configuration needs to be executed at least once after reboot or PCI FLR
and can be achieved by using the functions ``rte_acc_configure()``,
which sets up the parameters defined in the compatible ``rte_acc_conf`` structure.


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

Specifically for the bbdev Intel vRAN Boost v2 PMD, the command below can be used
(note that ACC200 was used previously to refer to VRB2):

.. code-block:: console

   pf_bb_config VRB2 -c ./vrb2/vrb2_config_vf_5g.cfg
   test-bbdev.py -e="-c 0xff0 -a${VF_PCI_ADDR}" -c validation -n 64 -b 64 -l 1 -v ./ldpc_dec_default.data
