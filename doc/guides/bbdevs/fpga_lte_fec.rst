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

Section 3 of the DPDK manual provides instructions on installing and compiling DPDK. The
default set of bbdev compile flags may be found in config/common_base, where for example
the flag to build the FPGA LTE FEC device, ``CONFIG_RTE_LIBRTE_PMD_BBDEV_FPGA_LTE_FEC``, is already
set.

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
``vfio`` and ``igb_uio``. However, in order to work the FPGA LTE FEC device firstly needs
to be bound to one of these linux drivers through DPDK.


Bind PF UIO driver(s)
~~~~~~~~~~~~~~~~~~~~~

Install the DPDK igb_uio driver, bind it with the PF PCI device ID and use
``lspci`` to confirm the PF device is under use by ``igb_uio`` DPDK UIO driver.

The igb_uio driver may be bound to the PF PCI device using one of three methods:


1. PCI functions (physical or virtual, depending on the use case) can be bound to
the UIO driver by repeating this command for every function.

.. code-block:: console

  cd <dpdk-top-level-directory>
  insmod ./build/kmod/igb_uio.ko
  echo "1172 5052" > /sys/bus/pci/drivers/igb_uio/new_id
  lspci -vd1172:


2. Another way to bind PF with DPDK UIO driver is by using the ``dpdk-devbind.py`` tool

.. code-block:: console

  cd <dpdk-top-level-directory>
  ./usertools/dpdk-devbind.py -b igb_uio 0000:06:00.0

where the PCI device ID (example: 0000:06:00.0) is obtained using lspci -vd1172:


3. A third way to bind is to use ``dpdk-setup.sh`` tool

.. code-block:: console

  cd <dpdk-top-level-directory>
  ./usertools/dpdk-setup.sh

  select 'Bind Ethernet/Crypto/Baseband device to IGB UIO module'
  or
  select 'Bind Ethernet/Crypto/Baseband device to VFIO module' depending on driver required
  enter PCI device ID
  select 'Display current Ethernet/Crypto/Baseband device settings' to confirm binding


In the same way the FPGA LTE FEC PF can be bound with vfio, but vfio driver does not
support SR-IOV configuration right out of the box, so it will need to be patched.


Enable Virtual Functions
~~~~~~~~~~~~~~~~~~~~~~~~

Now, it should be visible in the printouts that PCI PF is under igb_uio control
"``Kernel driver in use: igb_uio``"

To show the number of available VFs on the device, read ``sriov_totalvfs`` file..

.. code-block:: console

  cat /sys/bus/pci/devices/0000\:<b>\:<d>.<f>/sriov_totalvfs

  where 0000\:<b>\:<d>.<f> is the PCI device ID


To enable VFs via igb_uio, echo the number of virtual functions intended to
enable to ``max_vfs`` file..

.. code-block:: console

  echo <num-of-vfs> > /sys/bus/pci/devices/0000\:<b>\:<d>.<f>/max_vfs


Afterwards, all VFs must be bound to appropriate UIO drivers as required, same
way it was done with the physical function previously.

Enabling SR-IOV via vfio driver is pretty much the same, except that the file
name is different:

.. code-block:: console

  echo <num-of-vfs> > /sys/bus/pci/devices/0000\:<b>\:<d>.<f>/sriov_numvfs


Configure the VFs through PF
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The PCI virtual functions must be configured before working or getting assigned
to VMs/Containers. The configuration involves allocating the number of hardware
queues, priorities, load balance, bandwidth and other settings necessary for the
device to perform FEC functions.

This configuration needs to be executed at least once after reboot or PCI FLR and can
be achieved by using the function ``fpga_lte_fec_configure()``, which sets up the
parameters defined in ``fpga_lte_fec_conf`` structure:

.. code-block:: c

  struct fpga_lte_fec_conf {
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


An example configuration code calling the function ``fpga_lte_fec_configure()`` is shown
below:

.. code-block:: c

  struct fpga_lte_fec_conf conf;
  unsigned int i;

  memset(&conf, 0, sizeof(struct fpga_lte_fec_conf));
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
  ret = fpga_lte_fec_configure(info->dev_name, &conf);
  TEST_ASSERT_SUCCESS(ret,
      "Failed to configure 4G FPGA PF for bbdev %s",
      info->dev_name);


Test Application
----------------

BBDEV provides a test application, ``test-bbdev.py`` and range of test data for testing
the functionality of FPGA LTE FEC turbo encode and turbo decode, depending on the device's
capabilities. The test application is located under app->test-bbdev folder and has the
following options:

.. code-block:: console

  "-p", "--testapp-path": specifies path to the bbdev test app.
  "-e", "--eal-params"	: EAL arguments which are passed to the test app.
  "-t", "--timeout"	: Timeout in seconds (default=300).
  "-c", "--test-cases"	: Defines test cases to run. Run all if not specified.
  "-v", "--test-vector"	: Test vector path (default=dpdk_path+/app/test-bbdev/test_vectors/bbdev_null.data).
  "-n", "--num-ops"	: Number of operations to process on device (default=32).
  "-b", "--burst-size"	: Operations enqueue/dequeue burst size (default=32).
  "-l", "--num-lcores"	: Number of lcores to run (default=16).
  "-i", "--init-device" : Initialise PF device with default values.


To execute the test application tool using simple turbo decode or turbo encode data,
type one of the following:

.. code-block:: console

  ./test-bbdev.py -c validation -n 64 -b 8 -v ./turbo_dec_default.data
  ./test-bbdev.py -c validation -n 64 -b 8 -v ./turbo_enc_default.data


The test application ``test-bbdev.py``, supports the ability to configure the PF device with
a default set of values, if the "-i" or "- -init-device" option is included. The default values
are defined in test_bbdev_perf.c as:

- VF_UL_QUEUE_VALUE 4
- VF_DL_QUEUE_VALUE 4
- UL_BANDWIDTH 3
- DL_BANDWIDTH 3
- UL_LOAD_BALANCE 128
- DL_LOAD_BALANCE 128
- FLR_TIMEOUT 610


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
