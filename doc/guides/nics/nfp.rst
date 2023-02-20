..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2015-2017 Netronome Systems, Inc. All rights reserved.
    Copyright(c) 2021 Corigine, Inc. All rights reserved.

NFP poll mode driver library
============================

Netronome and Corigine's sixth generation of flow processors pack 216
programmable cores and over 100 hardware accelerators that uniquely combine
packet, flow, security and content processing in a single device that scales
up to 400-Gb/s.

This document explains how to use DPDK with the Network Flow Processor (NFP)
Poll Mode Driver (PMD) supporting Netronome and Corigine's NFP-6xxx, NFP-4xxx
and NFP-38xx product lines.

NFP is a SR-IOV capable device and the PMD supports the physical
function (PF) and the virtual functions (VFs).

Dependencies
------------

Before using the NFP DPDK PMD some NFP configuration,
which is not related to DPDK, is required. The system requires
installation of the **nfp-bsp (Board Support Package)** along
with a specific NFP firmware application. The NSP ABI
version should be 0.20 or higher.

If you have a NFP device you should already have the documentation to perform
this configuration. Contact **support@netronome.com** (for Netronome products)
or **smartnic-support@corigine.com** (for Corigine products) to obtain the
latest available firmware.

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

The NFP PMD code is provided in the **drivers/net/nfp** directory. Although
NFP PMD has BSP dependencies, it is possible to compile it along with other
DPDK PMDs even if no BSP was installed previously.
Of course, a DPDK app will require such a BSP installed for using the
NFP PMD, along with a specific NFP firmware application.

Once the DPDK is built all the DPDK apps and examples include support for
the NFP PMD.


Driver compilation and testing
------------------------------

Refer to the document
:ref:`compiling and testing a PMD for a NIC <pmd_build_and_test>` for details.

Using the PF
------------

The PMD PF has extra work to do which will delay the DPDK app initialization
like uploading the firmware and configure the Link state properly when starting
or stopping a PF port. Since DPDK 18.05 the firmware upload happens when
a PF is initialized, which was not always true with older DPDK versions.

Depending on the product installed in the system, firmware files should be
available under ``/lib/firmware/netronome``. DPDK PMD supporting the PF looks
for a firmware file in this order:

	1) First try to find a firmware image specific for this device using the
	   NFP serial number:

		serial-00-15-4d-12-20-65-10-ff.nffw

	2) Then try the PCI name:

		pci-0000:04:00.0.nffw

	3) Finally try the card type and media:

		nic_AMDA0099-0001_2x25.nffw

Netronome and Corigine's software packages install firmware files under
``/lib/firmware/netronome`` to support all the Netronome and Corigine SmartNICs
and different firmware applications. This is usually done using file names
based on SmartNIC type and media and with a directory per firmware application.
Options 1 and 2 for firmware filenames allow more than one SmartNIC, same type
of SmartNIC or different ones, and to upload a different firmware to each
SmartNIC.

   .. Note::
      Currently the NFP PMD supports using the PF with Agilio Firmware with
      NFD3 and Agilio Firmware with NFDk. See
      `Netronome Support <https://help.netronome.com/support/solutions>`_.
      for more information on the various firmwares supported by the Netronome
      Agilio SmartNIC range, or
      `Corigine Support <https://www.corigine.com/productsOverviewList-30.html>`_.
      for more information about Corigine's range.

PF multiport support
--------------------

The NFP PMD can work with up to 8 ports on the same PF device. The number of
available ports is firmware and hardware dependent, and the driver looks for a
firmware symbol during initialization to know how many can be used.

DPDK apps work with ports, and a port is usually a PF or a VF PCI device.
However, with the NFP PF multiport there is just one PF PCI device. Supporting
this particular configuration requires the PMD to create ports in a special
way, although once they are created, DPDK apps should be able to use them as
normal PCI ports.

NFP ports belonging to the same PF can be seen inside PMD initialization with a
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

Due to how the driver needs to access the NFP through a CPP interface, which
implies to use specific registers inside the chip, the number of secondary
processes with PF ports is limited to only one.

This limitation will be solved in future versions, but having basic
multiprocess support is important for allowing development and debugging
through the PF using a secondary process, which will create a CPP bridge
for user space tools accessing the NFP.


System configuration
--------------------

#. **Enable SR-IOV on the NFP device:** The current NFP PMD supports the PF and
   the VFs on a NFP device. However, it is not possible to work with both at
   the same time when using the ``nfp`` Linux netdev kernel driver. If the PF
   is bound to the ``nfp`` kernel module, and VFs are created, the VFs may be
   bound to the ``vfio-pci`` kernel module. It is also possible to bind the PF
   to the ``vfio-pci`` kernel module, and create VFs afterwards. This requires
   loading the ``vfio-pci`` module with the following parameters:

   .. code-block:: console

      modprobe vfio-pci enable_sriov=1 disable_idle_d3=1

   VFs need to be enabled before they can be used with the PMD. Before enabling
   the VFs it is useful to obtain information about the current NFP PCI device
   detected by the system. This can be done on Netronome SmartNICs using:

   .. code-block:: console

      lspci -d 19ee:

   and on Corigine SmartNICs using:

   .. code-block:: console

      lspci -d 1da8:

   Now, for example, to configure two virtual functions on a NFP device
   whose PCI system identity is "0000:03:00.0":

   .. code-block:: console

      echo 2 > /sys/bus/pci/devices/0000:03:00.0/sriov_numvfs

   The result of this command may be shown using lspci again on Netronome
   SmartNICs:

   .. code-block:: console

      lspci -kd 19ee:

   and on Corigine SmartNICs:

   .. code-block:: console

      lspci -kd 1da8:

   Two new PCI devices should appear in the output of the above command. The
   -k option shows the device driver, if any, that the devices are bound to.
   Depending on the modules loaded, at this point the new PCI devices may be
   bound to the ``nfp`` kernel driver or ``vfio-pci``.


Flow offload
------------

Using the flower firmware application, some types of Netronome or Corigine
SmartNICs can offload the flows onto the cards.

The flower firmware application requires the PMD running two services:

	* PF vNIC service: handling the feedback traffic.
	* ctrl vNIC service: communicate between PMD and firmware through
	  control messages.

To achieve the offload of flow, the representor ports are exposed to OVS.
The flower firmware application supports representor port for VF and physical
port. There will always exist a representor port for each physical port,
and the number of the representor port for VF is specified by the user through
a parameter.

In the Rx direction, the flower firmware application will prepend the input
port information into metadata for each packet which can't offloaded. The PF
vNIC service will keep polling packets from the firmware, and multiplex them
to the corresponding representor port.

In the Tx direction, the representor port will prepend the output port
information into metadata for each packet, and then send it to the firmware
through the PF vNIC.

The ctrl vNIC service handles various control messages, for example, the
creation and configuration of representor port, the pattern and action of flow
rules, the statistics of flow rules, etc.

Metadata Format
---------------

The NFP packet metadata format

NFD3
~~~~

The packet metadata starts with a field type header that can contain up-to
8 4-bit datatype specifiers (32-bits in total). This is followed by up to 8
32-bit words of data for each field described in the header. And directly
following the metadata (header and data) comes the packet.

The order of type is correspond with the data, but the nums of data field are
decided by the corresponding type, if the type need N data field, it need to
be wrote N times in the heads.
::

       3                   2                   1                   0
   2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   | Type7 | Type6 | Type5 | Type4 | Type3 | Type2 | Type1 | Type0 |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                        Data for field 0                       |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                        Data for field 1                       |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                        Data for field 2                       |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                        Data for field 3                       |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                        Data for field 4                       |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                        Data for field 5                       |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                        Data for field 6                       |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                        Data for field 7                       |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                          Packet Data                          |
   |                              ...                              |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

NFDk
~~~~

The packet metadata starts with a field type header that can contain 8 bit
metadata length and 6 4-bit datatype specifiers (32-bits in total). This is
followed by up to 6 32-bit words of data for each field described in the
header. And directly following the metadata (header and data) comes the
packet.

The order of type is correspond with the data, but the nums of data field are
decided by the corresponding type, if the type need N data field, it need to
be wrote N times in the heads. It is the same with NFD3.
::

       3                   2                   1                   0
     1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   | Type5 | Type4 | Type3 | Type2 | Type1 | Type0 |metadata length|
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                        Data for field 0                       |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                        Data for field 1                       |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                        Data for field 2                       |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                        Data for field 3                       |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                        Data for field 4                       |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                        Data for field 5                       |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                        Data for field 6                       |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                        Data for field 7                       |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                          Packet Data                          |
   |                              ...                              |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

There are two classes of metadata one for ingress and one for egress. In each
class the supported NFP types are:

RX
~~

NFP_NET_META_HASH
The hash type is 4 bit which is next field type after NFP_NET_META_HASH in
the header. The hash value is 32 bit which need 1 data field.
::

   -----------------------------------------------------------------
       3                   2                   1                   0
   2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                            Hash value                         |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

NFP_NET_META_VLAN
Metadata with L2 (1W/4B)
::

   ----------------------------------------------------------------
      3                   2                   1                   0
    1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |f|            reserved   | tpid| PCP |p|   vlan outermost VID  |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                                   ^                               ^
                             NOTE: |             TCI               |
                                   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   f 0 -> not stripping
   1 -> stripping

   tpid 0 -> RTE_ETHER_TYPE_VLAN 0x8100 IEEE 802.1Q VLAN tagging
        1 -> RTE_ETHER_TYPE_QINQ 0x88a8 IEEE 802.1ad QINQ tagging
   Tpid just be stored, now we don't handle it

   The vlan[0] is the innermost VLAN
   The vlan[1] is the QinQ info

TX
~~

NFP_NET_META_VLAN
::

   -----------------------------------------------------------------
       3                   2                   1                   0
     1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |               TPID            | PCP |p|   vlan outermost VID  |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                                   ^                               ^
                             NOTE: |             TCI               |
                                   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
