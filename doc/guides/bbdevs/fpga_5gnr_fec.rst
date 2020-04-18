..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2019 Intel Corporation

Intel(R) FPGA 5GNR FEC Poll Mode Driver
=======================================

The BBDEV FPGA 5GNR FEC poll mode driver (PMD) supports an FPGA implementation of a VRAN
LDPC Encode / Decode 5GNR wireless acceleration function, using Intel's PCI-e and FPGA
based Vista Creek device.

Features
--------

FPGA 5GNR FEC PMD supports the following features:

- LDPC Encode in the DL
- LDPC Decode in the UL
- 8 VFs per PF (physical device)
- Maximum of 32 UL queues per VF
- Maximum of 32 DL queues per VF
- PCIe Gen-3 x8 Interface
- MSI-X
- SR-IOV

FPGA 5GNR FEC PMD supports the following BBDEV capabilities:

* For the LDPC encode operation:
   - ``RTE_BBDEV_LDPC_CRC_24B_ATTACH`` :  set to attach CRC24B to CB(s)
   - ``RTE_BBDEV_LDPC_RATE_MATCH`` :  if set then do not do Rate Match bypass

* For the LDPC decode operation:
   - ``RTE_BBDEV_LDPC_CRC_TYPE_24B_CHECK`` :  check CRC24B from CB(s)
   - ``RTE_BBDEV_LDPC_ITERATION_STOP_ENABLE`` :  disable early termination
   - ``RTE_BBDEV_LDPC_CRC_TYPE_24B_DROP`` :  drops CRC24B bits appended while decoding
   - ``RTE_BBDEV_LDPC_HQ_COMBINE_IN_ENABLE`` :  provides an input for HARQ combining
   - ``RTE_BBDEV_LDPC_HQ_COMBINE_OUT_ENABLE`` :  provides an input for HARQ combining
   - ``RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_IN_ENABLE`` :  HARQ memory input is internal
   - ``RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_OUT_ENABLE`` :  HARQ memory output is internal
   - ``RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_LOOPBACK`` :  loopback data to/from HARQ memory
   - ``RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_FILLERS`` :  HARQ memory includes the fillers bits


Limitations
-----------

FPGA 5GNR FEC does not support the following:

- Scatter-Gather function


Installation
------------

Section 3 of the DPDK manual provides instuctions on installing and compiling DPDK. The
default set of bbdev compile flags may be found in config/common_base, where for example
the flag to build the FPGA 5GNR FEC device, ``CONFIG_RTE_LIBRTE_PMD_BBDEV_FPGA_5GNR_FEC``,
is already set. It is assumed DPDK has been compiled using for instance:

.. code-block:: console

  make install T=x86_64-native-linuxapp-gcc


DPDK requires hugepages to be configured as detailed in section 2 of the DPDK manual.
The bbdev test application has been tested with a configuration 40 x 1GB hugepages. The
hugepage configuration of a server may be examined using:

.. code-block:: console

   grep Huge* /proc/meminfo


Initialization
--------------

When the device first powers up, its PCI Physical Functions (PF) can be listed through this command:

.. code-block:: console

  sudo lspci -vd8086:0d8f

The physical and virtual functions are compatible with Linux UIO drivers:
``vfio`` and ``igb_uio``. However, in order to work the FPGA 5GNR FEC device firstly needs
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
  echo "8086 0d8f" > /sys/bus/pci/drivers/igb_uio/new_id
  lspci -vd8086:0d8f


2. Another way to bind PF with DPDK UIO driver is by using the ``dpdk-devbind.py`` tool

.. code-block:: console

  cd <dpdk-top-level-directory>
  ./usertools/dpdk-devbind.py -b igb_uio 0000:06:00.0

where the PCI device ID (example: 0000:06:00.0) is obtained using lspci -vd8086:0d8f


3. A third way to bind is to use ``dpdk-setup.sh`` tool

.. code-block:: console

  cd <dpdk-top-level-directory>
  ./usertools/dpdk-setup.sh

  select 'Bind Ethernet/Crypto/Baseband device to IGB UIO module'
  or
  select 'Bind Ethernet/Crypto/Baseband device to VFIO module' depending on driver required
  enter PCI device ID
  select 'Display current Ethernet/Crypto/Baseband device settings' to confirm binding


In the same way the FPGA 5GNR FEC PF can be bound with vfio, but vfio driver does not
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


Test Vectors
~~~~~~~~~~~~

In addition to the simple LDPC decoder and LDPC encoder tests, bbdev also provides
a range of additional tests under the test_vectors folder, which may be useful. The results
of these tests will depend on the FPGA 5GNR FEC capabilities.
