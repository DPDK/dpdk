..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2019 Intel Corporation

Intel(R) FPGA LTE FEC Poll Mode Driver
======================================

The BBDEV FPGA LTE FEC poll mode driver (PMD) supports an FPGA implementation of a VRAN
Turbo Encode / Decode LTE wireless acceleration function, using Intel's PCI-e and FPGA
based Vista Creek device.

Features
--------

FPGA LTE FEC PMD supports the following features:

- Turbo Encode in the DL with total throughput of 4.5 Gbits/s
- Turbo Decode in the UL with total throughput of 1.5 Gbits/s assuming 8 decoder iterations
- Support for code block level processing
- 8 VFs per PF (physical device)
- Maximum of 32 UL queues per VF
- Maximum of 32 DL queues per VF
- PCIe Gen-3 x8 Interface
- MSI-X
- SR-IOV


FPGA LTE FEC PMD supports the following BBDEV capabilities:

* For the turbo encode operation:
   - ``RTE_BBDEV_TURBO_CRC_24B_ATTACH`` :  set to attach CRC24B to CB(s)
   - ``RTE_BBDEV_TURBO_RATE_MATCH`` :  if set then do not do Rate Match bypass
   - ``RTE_BBDEV_TURBO_ENC_INTERRUPTS`` :  set for encoder dequeue interrupts


* For the turbo decode operation:
   - ``RTE_BBDEV_TURBO_CRC_TYPE_24B`` :  check CRC24B from CB(s)
   - ``RTE_BBDEV_TURBO_SUBBLOCK_DEINTERLEAVE`` :  perform subblock de-interleave
   - ``RTE_BBDEV_TURBO_DEC_INTERRUPTS`` :  set for decoder dequeue interrupts
   - ``RTE_BBDEV_TURBO_NEG_LLR_1_BIT_IN`` :  set if negative LLR encoder i/p is supported
   - ``RTE_BBDEV_TURBO_DEC_TB_CRC_24B_KEEP`` :  keep CRC24B bits appended while decoding


Limitations
-----------

FPGA LTE FEC does not support the following:

- Scatter-Gather function


Installation
--------------

Section 3 of the DPDK manual provides instructions on installing and compiling DPDK.

DPDK requires hugepages to be configured as detailed in section 2 of the DPDK manual.
The bbdev test application has been tested with a configuration 40 x 1GB hugepages. The
hugepage configuration of a server may be examined using:

.. code-block:: console

   grep Huge* /proc/meminfo


Initialization
--------------

When the device first powers up, its PCI Physical Functions (PF) can be listed through this command:

.. code-block:: console

  sudo lspci -vd1172:5052

The physical and virtual functions are compatible with Linux UIO drivers:
``vfio_pci`` and ``igb_uio``. However, in order to work the FPGA LTE FEC device firstly needs
to be bound to one of these linux drivers through DPDK.

For more details on how to bind the PF device and create VF devices, see
:ref:`linux_gsg_binding_kernel`.


Configure the VFs through PF
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The PCI virtual functions must be configured before working or getting assigned
to VMs/Containers. The configuration involves allocating the number of hardware
queues, priorities, load balance, bandwidth and other settings necessary for the
device to perform FEC functions.

This configuration needs to be executed at least once after reboot or PCI FLR and can
be achieved by using the function ``rte_fpga_lte_fec_configure()``, which sets up the
parameters defined in ``rte_fpga_lte_fec_conf`` structure:

.. code-block:: c

  struct rte_fpga_lte_fec_conf {
      bool pf_mode_en;
      uint8_t vf_ul_queues_number[FPGA_LTE_FEC_NUM_VFS];
      uint8_t vf_dl_queues_number[FPGA_LTE_FEC_NUM_VFS];
      uint8_t ul_bandwidth;
      uint8_t dl_bandwidth;
      uint8_t ul_load_balance;
      uint8_t dl_load_balance;
      uint16_t flr_time_out;
  };

- ``pf_mode_en``: identifies whether only PF is to be used, or the VFs. PF and
  VFs are mutually exclusive and cannot run simultaneously.
  Set to 1 for PF mode enabled.
  If PF mode is enabled all queues available in the device are assigned
  exclusively to PF and 0 queues given to VFs.

- ``vf_*l_queues_number``: defines the hardware queue mapping for every VF.

- ``*l_bandwidth``: in case of congestion on PCIe interface. The device
  allocates different bandwidth to UL and DL. The weight is configured by this
  setting. The unit of weight is 3 code blocks. For example, if the code block
  cbps (code block per second) ratio between UL and DL is 12:1, then the
  configuration value should be set to 36:3. The schedule algorithm is based
  on code block regardless the length of each block.

- ``*l_load_balance``: hardware queues are load-balanced in a round-robin
  fashion. Queues get filled first-in first-out until they reach a pre-defined
  watermark level, if exceeded, they won't get assigned new code blocks..
  This watermark is defined by this setting.

  If all hardware queues exceeds the watermark, no code blocks will be
  streamed in from UL/DL code block FIFO.

- ``flr_time_out``: specifies how many 16.384us to be FLR time out. The
  time_out = flr_time_out x 16.384us. For instance, if you want to set 10ms for
  the FLR time out then set this setting to 0x262=610.


An example configuration code calling the function ``rte_fpga_lte_fec_configure()`` is shown
below:

.. code-block:: c

  struct rte_fpga_lte_fec_conf conf;
  unsigned int i;

  memset(&conf, 0, sizeof(struct rte_fpga_lte_fec_conf));
  conf.pf_mode_en = 1;

  for (i = 0; i < FPGA_LTE_FEC_NUM_VFS; ++i) {
      conf.vf_ul_queues_number[i] = 4;
      conf.vf_dl_queues_number[i] = 4;
  }
  conf.ul_bandwidth = 12;
  conf.dl_bandwidth = 5;
  conf.dl_load_balance = 64;
  conf.ul_load_balance = 64;

  /* setup FPGA PF */
  ret = rte_fpga_lte_fec_configure(info->dev_name, &conf);
  TEST_ASSERT_SUCCESS(ret,
      "Failed to configure 4G FPGA PF for bbdev %s",
      info->dev_name);


Test Application
----------------

BBDEV provides a test application, ``test-bbdev.py`` and range of test data for testing
the functionality of the device, depending on the device's capabilities.

For more details on how to use the test application,
see :ref:`test_bbdev_application`.

Test Vectors
~~~~~~~~~~~~

In addition to the simple turbo decoder and turbo encoder tests, bbdev also provides
a range of additional tests under the test_vectors folder, which may be useful. The results
of these tests will depend on the FPGA LTE FEC capabilities:

* turbo decoder tests:
   - ``turbo_dec_c1_k6144_r0_e10376_crc24b_sbd_negllr_high_snr.data``
   - ``turbo_dec_c1_k6144_r0_e10376_crc24b_sbd_negllr_low_snr.data``
   - ``turbo_dec_c1_k6144_r0_e34560_negllr.data``
   - ``turbo_dec_c1_k6144_r0_e34560_sbd_negllr.data``
   - ``turbo_dec_c2_k3136_r0_e4920_sbd_negllr_crc24b.data``
   - ``turbo_dec_c2_k3136_r0_e4920_sbd_negllr.data``


* turbo encoder tests:
   - ``turbo_enc_c1_k40_r0_e1190_rm.data``
   - ``turbo_enc_c1_k40_r0_e1194_rm.data``
   - ``turbo_enc_c1_k40_r0_e1196_rm.data``
   - ``turbo_enc_c1_k40_r0_e272_rm.data``
   - ``turbo_enc_c1_k6144_r0_e18444.data``
   - ``turbo_enc_c1_k6144_r0_e32256_crc24b_rm.data``
   - ``turbo_enc_c2_k5952_r0_e17868_crc24b.data``
   - ``turbo_enc_c3_k4800_r2_e14412_crc24b.data``
   - ``turbo_enc_c4_k4800_r2_e14412_crc24b.data``


Alternate Baseband Device configuration tool
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

On top of the embedded configuration feature supported in test-bbdev using "- -init-device"
option, there is also a tool available to perform that device configuration using a companion
application.
The ``pf_bb_config`` application notably enables then to run bbdev-test from the VF
and not only limited to the PF as captured above.

See for more details: https://github.com/intel/pf-bb-config

Specifically for the BBDEV FPGA LTE FEC PMD, the command below can be used:

.. code-block:: console

  ./pf_bb_config FPGA_LTE -c fpga_lte/fpga_lte_config_vf.cfg
  ./test-bbdev.py -e="-l 4-11 -a${VF_PCI_ADDR}" -c validation -n 64 -b 32 -l 1 -v ./turbo_dec_default.data
