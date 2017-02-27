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

Use a `DPDK supported <http://dpdk.org/doc/nics>`_ high end NIC such as the Intel XL710 40GbE.

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

#. Before starting consider resetting all BIOS settings to their default.

#. Disable all power saving options such as: Power performance tuning, CPU P-State, CPU C3 Report and CPU C6 Report.

#. Select **Performance** as the CPU Power and Performance policy.

#. Disable Turbo Boost to ensure the performance scaling increases with the number of cores.

#. Set memory frequency to the highest available number, NOT auto.

#. Disable all virtualization options when you test the physical function of the NIC, and turn on ``VT-d`` if you wants to use VFIO.


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

1. Build the DPDK target and reserve huge pages.
   See the earlier section on :ref:`linux_gsg_hugepages` for more details.

   The following shell commands may help with building and configuration:

   .. code-block:: console

      # Build DPDK target.
      cd dpdk_folder
      make install T=x86_64-native-linuxapp-gcc -j

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

2. Check the CPU layout using using the DPDK ``cpu_layout`` utility:

   .. code-block:: console

      cd dpdk_folder

      usertools/cpu_layout.py

   Or run ``lscpu`` to check the the cores on each socket.

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

4. Bind the test ports to DPDK compatible drivers, such as igb_uio. For example bind two ports to a DPDK compatible driver and check the status:

   .. code-block:: console


      # Bind ports 82:00.0 and 85:00.0 to dpdk driver
      ./dpdk_folder/usertools/dpdk-devbind.py -b igb_uio 82:00.0 85:00.0

      # Check the port driver status
      ./dpdk_folder/usertools/dpdk-devbind.py --status

   See ``dpdk-devbind.py --help`` for more details.


More details about DPDK setup and Linux kernel requirements see :ref:`linux_gsg_compiling_dpdk`.


Example of getting best performance for an Intel NIC
----------------------------------------------------

The following is an example of running the DPDK ``l3fwd`` sample application to get high performance with an
Intel server platform and Intel XL710 NICs.
For specific 40G NIC configuration please refer to the i40e NIC guide.

The example scenario is to get best performance with two Intel XL710 40GbE ports.
See :numref:`figure_intel_perf_test_setup` for the performance test setup.

.. _figure_intel_perf_test_setup:

.. figure:: img/intel_perf_test_setup.*

   Performance Test Setup


1. Add two Intel XL710 NICs to the platform, and use one port per card to get best performance.
   The reason for using two NICs is to overcome a PCIe Gen3's limitation since it cannot provide 80G bandwidth
   for two 40G ports, but two different PCIe Gen3 x8 slot can.
   Refer to the sample NICs output above, then we can select ``82:00.0`` and ``85:00.0`` as test ports::

      82:00.0 Ethernet [0200]: Intel XL710 for 40GbE QSFP+ [8086:1583]
      85:00.0 Ethernet [0200]: Intel XL710 for 40GbE QSFP+ [8086:1583]

2. Connect the ports to the traffic generator. For high speed testing, it's best to use a hardware traffic generator.

3. Check the PCI devices numa node (socket id) and get the cores number on the exact socket id.
   In this case, ``82:00.0`` and ``85:00.0`` are both in socket 1, and the cores on socket 1 in the referenced platform
   are 18-35 and 54-71.
   Note: Don't use 2 logical cores on the same core (e.g core18 has 2 logical cores, core18 and core54), instead, use 2 logical
   cores from different cores (e.g core18 and core19).

4. Bind these two ports to igb_uio.

5. As to XL710 40G port, we need at least two queue pairs to achieve best performance, then two queues per port
   will be required, and each queue pair will need a dedicated CPU core for receiving/transmitting packets.

6. The DPDK sample application ``l3fwd`` will be used for performance testing, with using two ports for bi-directional forwarding.
   Compile the ``l3fwd sample`` with the default lpm mode.

7. The command line of running l3fwd would be something like the followings::

      ./l3fwd -l 18-21 -n 4 -w 82:00.0 -w 85:00.0 \
              -- -p 0x3 --config '(0,0,18),(0,1,19),(1,0,20),(1,1,21)'

   This means that the application uses core 18 for port 0, queue pair 0 forwarding, core 19 for port 0, queue pair 1 forwarding,
   core 20 for port 1, queue pair 0 forwarding, and core 21 for port 1, queue pair 1 forwarding.


8. Configure the traffic at a traffic generator.

   * Start creating a stream on packet generator.

   * Set the Ethernet II type to 0x0800.
