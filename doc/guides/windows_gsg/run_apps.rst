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


Load virt2phys Driver
---------------------

Access to physical addresses is provided by a kernel-mode driver, virt2phys.
It is mandatory at least for using hardware PMDs, but may also be required
for mempools.

Refer to documentation in ``dpdk-kmods`` repository for details on system
setup, driver build and installation. This driver is not signed, so signature
checking must be disabled to load it.

.. warning::

    Disabling driver signature enforcement weakens OS security.
    It is discouraged in production environments.

Compiled package consists of ``virt2phys.inf``, ``virt2phys.cat``,
and ``virt2phys.sys``. It can be installed as follows
from Elevated Command Prompt:

.. code-block:: console

    pnputil /add-driver Z:\path\to\virt2phys.inf /install

On Windows Server additional steps are required:

1. From Device Manager, Action menu, select "Add legacy hardware".
2. It will launch the "Add Hardware Wizard". Click "Next".
3. Select second option "Install the hardware that I manually select
   from a list (Advanced)".
4. On the next screen, "Kernel bypass" will be shown as a device class.
5. Select it, and click "Next".
6. The previously installed drivers will now be installed for the
   "Virtual to physical address translator" device.

When loaded successfully, the driver is shown in *Device Manager* as *Virtual
to physical address translator* device under *Kernel bypass* category.
Installed driver persists across reboots.

If DPDK is unable to communicate with the driver, a warning is printed
on initialization (debug-level logs provide more details):

.. code-block:: text

    EAL: Cannot open virt2phys driver interface



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
