..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2020 Dmitry Kozlyuk

Running DPDK Applications
=========================

Grant *Lock pages in memory* Privilege
--------------------------------------

Use of hugepages ("large pages" in Windows terminology) requires
``SeLockMemoryPrivilege`` for the user running an application.

1. Open *Local Security Policy* snap-in, either:

   * Control Panel / Computer Management / Local Security Policy;
   * or Win+R, type ``secpol``, press Enter.

2. Open *Local Policies / User Rights Assignment / Lock pages in memory.*

3. Add desired users or groups to the list of grantees.

4. Privilege is applied upon next logon. In particular, if privilege has been
   granted to current user, a logoff is required before it is available.

See `Large-Page Support`_ in MSDN for details.

.. _Large-Page Support: https://docs.microsoft.com/en-us/windows/win32/memory/large-page-support


Install Drivers
---------------

Certain kernel-mode drivers are required to run DPDK applications.
Refer to `Windows documentation <https://git.dpdk.org/dpdk-kmods/tree/windows>`_
in ``dpdk-kmods`` repository for common instructions on system setup,
driver build and installation.
The drivers are not signed, so signature enforcement has to be disabled.

.. warning::

    Disabling driver signature enforcement weakens OS security.
    It is discouraged in production environments.


virt2phys
~~~~~~~~~

Access to physical addresses is provided by a kernel-mode driver, virt2phys.
It is mandatory for allocating physically-contiguous memory which is required
by hardware PMDs.

When loaded successfully, the driver is shown in *Device Manager* as *Virtual
to physical address translator* device under *Kernel bypass* category.
Installed driver persists across reboots.

If DPDK is unable to communicate with the driver, a warning is printed
on initialization (debug-level logs provide more details):

.. code-block:: text

    EAL: Cannot open virt2phys driver interface


NetUIO
~~~~~~

NetUIO kernel-mode driver provides access to the device hardware resources.
It is mandatory for all hardware PMDs, except for mlx5 PMD.

Refer to `NetUIO documentation <https://git.dpdk.org/dpdk-kmods/tree/windows/netuio/README.rst>`_
in ``dpdk-kmods`` repository for instructions to build and set up the driver.
Devices supported by NetUIO are listed in ``netuio.inf``.
The list can be extended in order to try running DPDK with new devices.


Run the ``helloworld`` Example
------------------------------

Navigate to the examples in the build directory and run `dpdk-helloworld.exe`.

.. code-block:: console

    cd C:\Users\me\dpdk\build\examples
    dpdk-helloworld.exe -l 0-3
    hello from core 1
    hello from core 3
    hello from core 0
    hello from core 2
