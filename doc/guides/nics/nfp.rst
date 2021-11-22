..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2015-2017 Netronome Systems, Inc. All rights reserved.
    All rights reserved.

NFP poll mode driver library
============================

Netronome's sixth generation of flow processors pack 216 programmable
cores and over 100 hardware accelerators that uniquely combine packet,
flow, security and content processing in a single device that scales
up to 400-Gb/s.

This document explains how to use DPDK with the Netronome Poll Mode
Driver (PMD) supporting Netronome's Network Flow Processor 6xxx
(NFP-6xxx) and Netronome's Flow Processor 4xxx (NFP-4xxx).

NFP is a SRIOV capable device and the PMD supports the physical
function (PF) and the virtual functions (VFs).

Dependencies
------------

Before using the Netronome's DPDK PMD some NFP configuration,
which is not related to DPDK, is required. The system requires
installation of **Netronome's BSP (Board Support Package)** along
with a specific NFP firmware application. Netronome's NSP ABI
version should be 0.20 or higher.

If you have a NFP device you should already have the code and
documentation for this configuration. Contact
**support@netronome.com** to obtain the latest available firmware.

The NFP Linux netdev kernel driver for VFs has been a part of the
vanilla kernel since kernel version 4.5, and support for the PF
since kernel version 4.11. Support for older kernels can be obtained
on Github at
**https://github.com/Netronome/nfp-drv-kmods** along with the build
instructions.

NFP PMD needs to be used along with UIO ``igb_uio`` or VFIO (``vfio-pci``)
Linux kernel driver.

Building the software
---------------------

Netronome's PMD code is provided in the **drivers/net/nfp** directory.
Although NFP PMD has Netronome´s BSP dependencies, it is possible to
compile it along with other DPDK PMDs even if no BSP was installed previously.
Of course, a DPDK app will require such a BSP installed for using the
NFP PMD, along with a specific NFP firmware application.

Once the DPDK is built all the DPDK apps and examples include support for
the NFP PMD.


Driver compilation and testing
------------------------------

Refer to the document :ref:`compiling and testing a PMD for a NIC <pmd_build_and_test>`
for details.

Using the PF
------------

NFP PMD supports using the NFP PF as another DPDK port, but it does not
have any functionality for controlling VFs. In fact, it is not possible to use
the PMD with the VFs if the PF is being used by DPDK, that is, with the NFP PF
bound to ``igb_uio`` or ``vfio-pci`` kernel drivers. Future DPDK versions will
have a PMD able to work with the PF and VFs at the same time and with the PF
implementing VF management along with other PF-only functionalities/offloads.

The PMD PF has extra work to do which will delay the DPDK app initialization
like uploading the firmware and configure the Link state properly when starting or
stopping a PF port. Since DPDK 18.05 the firmware upload happens when
a PF is initialized, which was not always true with older DPDK versions.

Depending on the Netronome product installed in the system, firmware files
should be available under ``/lib/firmware/netronome``. DPDK PMD supporting the
PF looks for a firmware file in this order:

	1) First try to find a firmware image specific for this device using the
	   NFP serial number:

		serial-00-15-4d-12-20-65-10-ff.nffw

	2) Then try the PCI name:

		pci-0000:04:00.0.nffw

	3) Finally try the card type and media:

		nic_AMDA0099-0001_2x25.nffw

Netronome's software packages install firmware files under ``/lib/firmware/netronome``
to support all the Netronome's SmartNICs and different firmware applications.
This is usually done using file names based on SmartNIC type and media and with a
directory per firmware application. Options 1 and 2 for firmware filenames allow
more than one SmartNIC, same type of SmartNIC or different ones, and to upload a
different firmware to each SmartNIC.

   .. Note::
      Currently the NFP PMD supports using the PF with Agilio Basic Firmware. See
      https://help.netronome.com/support/solutions for more information on the
      various firmwares supported by the Netronome Agilio CX smartNIC.

PF multiport support
--------------------

The NFP PMD can work with up to 8 ports on the same PF device. The number of
available ports is firmware and hardware dependent, and the driver looks for a
firmware symbol during initialization to know how many can be used.

DPDK apps work with ports, and a port is usually a PF or a VF PCI device.
However, with the NFP PF multiport there is just one PF PCI device. Supporting
this particular configuration requires the PMD to create ports in a special way,
although once they are created, DPDK apps should be able to use them as normal
PCI ports.

NFP ports belonging to same PF can be seen inside PMD initialization with a
suffix added to the PCI ID: wwww:xx:yy.z_portn. For example, a PF with PCI ID
0000:03:00.0 and four ports is seen by the PMD code as:

   .. code-block:: console

      0000:03:00.0_port0
      0000:03:00.0_port1
      0000:03:00.0_port2
      0000:03:00.0_port3

   .. Note::

      There are some limitations with multiport support: RX interrupts and
      device hot-plugging are not supported.

PF multiprocess support
-----------------------

Due to how the driver needs to access the NFP through a CPP interface, which implies
to use specific registers inside the chip, the number of secondary processes with PF
ports is limited to only one.

This limitation will be solved in future versions but having basic multiprocess support
is important for allowing development and debugging through the PF using a secondary
process which will create a CPP bridge for user space tools accessing the NFP.


System configuration
--------------------

#. **Enable SR-IOV on the NFP device:** The current NFP PMD supports the PF and
   the VFs on a NFP device. However, it is not possible to work with both at the
   same time because the VFs require the PF being bound to the NFP PF Linux
   netdev driver.  Make sure you are working with a kernel with NFP PF support or
   get the drivers from the above Github repository and follow the instructions
   for building and installing it.

   VFs need to be enabled before they can be used with the PMD.
   Before enabling the VFs it is useful to obtain information about the
   current NFP PCI device detected by the system:

   .. code-block:: console

      lspci -d19ee:

   Now, for example, configure two virtual functions on a NFP-6xxx device
   whose PCI system identity is "0000:03:00.0":

   .. code-block:: console

      echo 2 > /sys/bus/pci/devices/0000:03:00.0/sriov_numvfs

   The result of this command may be shown using lspci again:

   .. code-block:: console

      lspci -d19ee: -k

   Two new PCI devices should appear in the output of the above command. The
   -k option shows the device driver, if any, that devices are bound to.
   Depending on the modules loaded at this point the new PCI devices may be
   bound to nfp_netvf driver.
