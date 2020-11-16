..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2015 Intel Corporation.

How to get best performance with NICs on Intel platforms
========================================================

This document is a step-by-step guide for getting high performance from DPDK applications on Intel platforms.


Hardware and Memory Requirements
--------------------------------

For best performance use an Intel Xeon class server system such as Ivy Bridge, Haswell or newer.

Ensure that each memory channel has at least one memory DIMM inserted, and that the memory size for each is at least 4GB.
**Note**: this has one of the most direct effects on performance.

You can check the memory configuration using ``dmidecode`` as follows::

      dmidecode -t memory | grep Locator

      Locator: DIMM_A1
      Bank Locator: NODE 1
      Locator: DIMM_A2
      Bank Locator: NODE 1
      Locator: DIMM_B1
      Bank Locator: NODE 1
      Locator: DIMM_B2
      Bank Locator: NODE 1
      ...
      Locator: DIMM_G1
      Bank Locator: NODE 2
      Locator: DIMM_G2
      Bank Locator: NODE 2
      Locator: DIMM_H1
      Bank Locator: NODE 2
      Locator: DIMM_H2
      Bank Locator: NODE 2

The sample output above shows a total of 8 channels, from ``A`` to ``H``, where each channel has 2 DIMMs.

You can also use ``dmidecode`` to determine the memory frequency::

      dmidecode -t memory | grep Speed

      Speed: 2133 MHz
      Configured Clock Speed: 2134 MHz
      Speed: Unknown
      Configured Clock Speed: Unknown
      Speed: 2133 MHz
      Configured Clock Speed: 2134 MHz
      Speed: Unknown
      ...
      Speed: 2133 MHz
      Configured Clock Speed: 2134 MHz
      Speed: Unknown
      Configured Clock Speed: Unknown
      Speed: 2133 MHz
      Configured Clock Speed: 2134 MHz
      Speed: Unknown
      Configured Clock Speed: Unknown

The output shows a speed of 2133 MHz (DDR4) and Unknown (not existing).
This aligns with the previous output which showed that each channel has one memory bar.


Network Interface Card Requirements
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Use a `DPDK supported <https://core.dpdk.org/supported/>`_ high end NIC such as the Intel XL710 40GbE.

Make sure each NIC has been flashed the latest version of NVM/firmware.

Use PCIe Gen3 slots, such as Gen3 ``x8`` or Gen3 ``x16`` because PCIe Gen2 slots don't provide enough bandwidth
for 2 x 10GbE and above.
You can use ``lspci`` to check the speed of a PCI slot using something like the following::

      lspci -s 03:00.1 -vv | grep LnkSta

      LnkSta: Speed 8GT/s, Width x8, TrErr- Train- SlotClk+ DLActive- ...
      LnkSta2: Current De-emphasis Level: -6dB, EqualizationComplete+ ...

When inserting NICs into PCI slots always check the caption, such as CPU0 or CPU1 to indicate which socket it is connected to.

Care should be take with NUMA.
If you are using 2 or more ports from different NICs, it is best to ensure that these NICs are on the same CPU socket.
An example of how to determine this is shown further below.


BIOS Settings
~~~~~~~~~~~~~

The following are some recommendations on BIOS settings. Different platforms will have different BIOS naming
so the following is mainly for reference:

#. Establish the steady state for the system, consider reviewing BIOS settings desired for best performance characteristic e.g. optimize for performance or energy efficiency.

#. Match the BIOS settings to the needs of the application you are testing.

#. Typically, **Performance** as the CPU Power and Performance policy is a reasonable starting point.

#. Consider using Turbo Boost to increase the frequency on cores.

#. Disable all virtualization options when you test the physical function of the NIC, and turn on VT-d if you wants to use VFIO.


Linux boot command line
~~~~~~~~~~~~~~~~~~~~~~~

The following are some recommendations on GRUB boot settings:

#. Use the default grub file as a starting point.

#. Reserve 1G huge pages via grub configurations. For example to reserve 8 huge pages of 1G size::

      default_hugepagesz=1G hugepagesz=1G hugepages=8

#. Isolate CPU cores which will be used for DPDK. For example::

      isolcpus=2,3,4,5,6,7,8

#. If it wants to use VFIO, use the following additional grub parameters::

      iommu=pt intel_iommu=on


Configurations before running DPDK
----------------------------------

1. Reserve huge pages.
   See the earlier section on :ref:`linux_gsg_hugepages` for more details.

   .. code-block:: console

      # Get the hugepage size.
      awk '/Hugepagesize/ {print $2}' /proc/meminfo

      # Get the total huge page numbers.
      awk '/HugePages_Total/ {print $2} ' /proc/meminfo

      # Unmount the hugepages.
      umount `awk '/hugetlbfs/ {print $2}' /proc/mounts`

      # Create the hugepage mount folder.
      mkdir -p /mnt/huge

      # Mount to the specific folder.
      mount -t hugetlbfs nodev /mnt/huge

2. Check the CPU layout using the DPDK ``cpu_layout`` utility:

   .. code-block:: console

      cd dpdk_folder

      usertools/cpu_layout.py

   Or run ``lscpu`` to check the cores on each socket.

3. Check your NIC id and related socket id:

   .. code-block:: console

      # List all the NICs with PCI address and device IDs.
      lspci -nn | grep Eth

   For example suppose your output was as follows::

      82:00.0 Ethernet [0200]: Intel XL710 for 40GbE QSFP+ [8086:1583]
      82:00.1 Ethernet [0200]: Intel XL710 for 40GbE QSFP+ [8086:1583]
      85:00.0 Ethernet [0200]: Intel XL710 for 40GbE QSFP+ [8086:1583]
      85:00.1 Ethernet [0200]: Intel XL710 for 40GbE QSFP+ [8086:1583]

   Check the PCI device related numa node id:

   .. code-block:: console

      cat /sys/bus/pci/devices/0000\:xx\:00.x/numa_node

   Usually ``0x:00.x`` is on socket 0 and ``8x:00.x`` is on socket 1.
   **Note**: To get the best performance, ensure that the core and NICs are in the same socket.
   In the example above ``85:00.0`` is on socket 1 and should be used by cores on socket 1 for the best performance.

4. Check which kernel drivers needs to be loaded and whether there is a need to unbind the network ports from their kernel drivers.
More details about DPDK setup and Linux kernel requirements see :ref:`linux_gsg_compiling_dpdk` and :ref:`linux_gsg_linux_drivers`.
