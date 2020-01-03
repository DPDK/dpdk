..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2014 Intel Corporation.

.. _building_from_source:

Compiling the DPDK Target from Source
=====================================

Prerequisites
-------------

The following FreeBSD packages are required to build DPDK:

* meson
* ninja
* pkgconf

These can be installed using (as root)::

  pkg install meson pkgconf

To compile the required kernel modules for memory management and working
with physical NIC devices, the kernel sources for FreeBSD also
need to be installed. If not already present on the system, these can be
installed via commands like the following, for FreeBSD 12.1 on x86_64::

  fetch http://ftp.freebsd.org/pub/FreeBSD/releases/amd64/12.1-RELEASE/src.txz
  tar -C / -xJvf src.txz

To enable the telemetry library in DPDK, the jansson library also needs to
be installed, and can be installed via::

  pkg install jansson

Individual drivers may have additional requirements. Consult the relevant
driver guide for any driver-specific requirements of interest.

Building DPDK
-------------

The following commands can be used to build and install DPDK on a system.
The final, install, step generally needs to be run as root::

  meson build
  cd build
  ninja
  ninja install

This will install the DPDK libraries and drivers to `/usr/local/lib` with a
pkg-config file `libdpdk.pc` installed to `/usr/local/lib/pkgconfig`. The
DPDK test applications, such as `dpdk-testpmd` are installed to
`/usr/local/bin`. To use these applications, it is recommended that the
`contigmem` and `nic_uio` kernel modules be loaded first, as described in
the next section.

.. note::

        It is recommended that pkg-config be used to query information
        about the compiler and linker flags needed to build applications
        against DPDK.  In some cases, the path `/usr/local/lib/pkgconfig`
        may not be in the default search paths for `.pc` files, which means
        that queries for DPDK information may fail. This can be fixed by
        setting the appropriate path in `PKG_CONFIG_PATH` environment
        variable.


.. _loading_contigmem:

Loading the DPDK contigmem Module
---------------------------------

To run a DPDK application, physically contiguous memory is required.
In the absence of non-transparent superpages, the included sources for the
contigmem kernel module provides the ability to present contiguous blocks of
memory for the DPDK to use. The contigmem module must be loaded into the
running kernel before any DPDK is run. Once DPDK is installed on the
system, the module can be found in the `/boot/modules` directory.

The amount of physically contiguous memory along with the number of physically
contiguous blocks to be reserved by the module can be set at runtime prior to
module loading using::

    kenv hw.contigmem.num_buffers=n
    kenv hw.contigmem.buffer_size=m

The kernel environment variables can also be specified during boot by placing the
following in ``/boot/loader.conf``:

.. code-block:: shell

    hw.contigmem.num_buffers=n
    hw.contigmem.buffer_size=m

The variables can be inspected using the following command::

    sysctl -a hw.contigmem

Where n is the number of blocks and m is the size in bytes of each area of
contiguous memory.  A default of two buffers of size 1073741824 bytes (1 Gigabyte)
each is set during module load if they are not specified in the environment.

The module can then be loaded using kldload::

    kldload contigmem

It is advisable to include the loading of the contigmem module during the boot
process to avoid issues with potential memory fragmentation during later system
up time.  This can be achieved by placing lines similar to the following into
``/boot/loader.conf``:

.. code-block:: shell

    hw.contigmem.num_buffers=1
    hw.contigmem.buffer_size=1073741824
    contigmem_load="YES"

.. note::

    The contigmem_load directive should be placed after any definitions of
    ``hw.contigmem.num_buffers`` and ``hw.contigmem.buffer_size`` if the default values
    are not to be used.

An error such as::

    kldload: can't load ./x86_64-native-freebsd-gcc/kmod/contigmem.ko:
             Exec format error

is generally attributed to not having enough contiguous memory
available and can be verified via dmesg or ``/var/log/messages``::

    kernel: contigmalloc failed for buffer <n>

To avoid this error, reduce the number of buffers or the buffer size.

.. _loading_nic_uio:

Loading the DPDK nic_uio Module
-------------------------------

After loading the contigmem module, the ``nic_uio`` module must also be loaded into the
running kernel prior to running any DPDK application, e.g. using::

    kldload nic_uio

.. note::

    If the ports to be used are currently bound to a existing kernel driver
    then the ``hw.nic_uio.bdfs sysctl`` value will need to be set before loading the
    module. Setting this value is described in the next section below.

Currently loaded modules can be seen by using the ``kldstat`` command and a module
can be removed from the running kernel by using ``kldunload <module_name>``.

To load the module during boot place the following into ``/boot/loader.conf``:

.. code-block:: shell

    nic_uio_load="YES"

.. note::

    ``nic_uio_load="YES"`` must appear after the contigmem_load directive, if it exists.

By default, the ``nic_uio`` module will take ownership of network ports if they are
recognized DPDK devices and are not owned by another module. However, since
the FreeBSD kernel includes support, either built-in, or via a separate driver
module, for most network card devices, it is likely that the ports to be used are
already bound to a driver other than ``nic_uio``. The following sub-section describe
how to query and modify the device ownership of the ports to be used by
DPDK applications.

.. _binding_network_ports:

Binding Network Ports to the nic_uio Module
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Device ownership can be viewed using the pciconf -l command. The example below shows
four Intel® 82599 network ports under ``if_ixgbe`` module ownership.

.. code-block:: none

    pciconf -l
    ix0@pci0:1:0:0: class=0x020000 card=0x00038086 chip=0x10fb8086 rev=0x01 hdr=0x00
    ix1@pci0:1:0:1: class=0x020000 card=0x00038086 chip=0x10fb8086 rev=0x01 hdr=0x00
    ix2@pci0:2:0:0: class=0x020000 card=0x00038086 chip=0x10fb8086 rev=0x01 hdr=0x00
    ix3@pci0:2:0:1: class=0x020000 card=0x00038086 chip=0x10fb8086 rev=0x01 hdr=0x00

The first column constitutes three components:

#. Device name: ``ixN``

#. Unit name: ``pci0``

#. Selector (Bus:Device:Function): ``1:0:0``

Where no driver is associated with a device, the device name will be ``none``.

By default, the FreeBSD kernel will include built-in drivers for the most common
devices; a kernel rebuild would normally be required to either remove the drivers
or configure them as loadable modules.

To avoid building a custom kernel, the ``nic_uio`` module can detach a network port
from its current device driver. This is achieved by setting the ``hw.nic_uio.bdfs``
kernel environment variable prior to loading ``nic_uio``, as follows::

    kenv hw.nic_uio.bdfs="b:d:f,b:d:f,..."

Where a comma separated list of selectors is set, the list must not contain any
whitespace.

For example to re-bind ``ix2@pci0:2:0:0`` and ``ix3@pci0:2:0:1`` to the ``nic_uio`` module
upon loading, use the following command::

    kenv hw.nic_uio.bdfs="2:0:0,2:0:1"

The variable can also be specified during boot by placing the following into
``/boot/loader.conf``, before the previously-described ``nic_uio_load`` line - as
shown:

.. code-block:: shell

    hw.nic_uio.bdfs="2:0:0,2:0:1"
    nic_uio_load="YES"

Binding Network Ports Back to their Original Kernel Driver
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

If the original driver for a network port has been compiled into the kernel,
it is necessary to reboot FreeBSD to restore the original device binding. Before
doing so, update or remove the ``hw.nic_uio.bdfs`` in ``/boot/loader.conf``.

If rebinding to a driver that is a loadable module, the network port binding can
be reset without rebooting. To do so, unload both the target kernel module and the
``nic_uio`` module, modify or clear the ``hw.nic_uio.bdfs`` kernel environment (kenv)
value, and reload the two drivers - first the original kernel driver, and then
the ``nic_uio driver``. Note: the latter does not need to be reloaded unless there are
ports that are still to be bound to it.

Example commands to perform these steps are shown below::

    kldunload nic_uio
    kldunload <original_driver>

    # To clear the value completely:
    kenv -u hw.nic_uio.bdfs

    # To update the list of ports to bind:
    kenv hw.nic_uio.bdfs="b:d:f,b:d:f,..."

    kldload <original_driver>

    kldload nic_uio  # optional
